using System.Text.Json;
using Npgsql;
using ShatteredFrontier.Exchange;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;

namespace ShatteredFrontier.Exchange.Postgres;

public sealed class ExchangeUnavailableException(string message, Exception inner) : Exception(message, inner);
public sealed class ExchangeSchemaCompatibilityException(string message) : Exception(message);
public sealed class ExchangeRejectedException(string message) : InvalidOperationException(message);
public sealed record DurableListing(string Id, string SellerId, string SellerContainerId, string EscrowId, string ItemId, long Quantity, long Price, long Fee, DateTimeOffset ExpiresAt, string State, long Revision);
public sealed record DurableSettlement(string OperationId, string Kind, string SubjectId, string? PayerId, string? PayeeId,
    long Amount, long Fee, IReadOnlyList<EscrowDelivery> Deliveries, string State, ExchangeReceipt Receipt);
public sealed record ListingIntent(string Command, string Id, string Seller, string SellerContainer, string Escrow,
    string Item, long Quantity, long Price, long Fee, DateTimeOffset Now, long DurationHours, DateTimeOffset CreatedAt);

public sealed class PostgresExchangeStore(string connectionString)
{
    private static NpgsqlCommand Cmd(NpgsqlConnection c, NpgsqlTransaction t, string sql, params object[] args)
    {
        var cmd = new NpgsqlCommand(sql, c, t);
        foreach (var arg in args) cmd.Parameters.AddWithValue(arg);
        return cmd;
    }
    private T Run<T>(Func<NpgsqlConnection,NpgsqlTransaction,T> action, string? op = null)
    {
        try
        {
            using var c = new NpgsqlConnection(connectionString); c.Open();
            using var t = c.BeginTransaction();
            if (op is not null)
                using (var advisory = Cmd(c,t,"SELECT pg_advisory_xact_lock(519761,hashtext($1))",op)) advisory.ExecuteNonQuery();
            try
            {
                using var check = Cmd(c,t,"SELECT version FROM exchange.schema_version WHERE singleton=true");
                if (check.ExecuteScalar() is not int version || version != 3) throw new ExchangeSchemaCompatibilityException("Unsupported exchange schema version.");
            }
            catch (PostgresException ex) when (ex.SqlState is "42P01" or "3F000" or "42501")
            { throw new ExchangeSchemaCompatibilityException("Exchange schema marker missing or inaccessible."); }
            var value = action(c,t); t.Commit(); return value;
        }
        catch (PostgresException ex) when (ex.SqlState.StartsWith("22",StringComparison.Ordinal) || ex.SqlState.StartsWith("23",StringComparison.Ordinal))
        { throw new ExchangeRejectedException($"Exchange operation rejected ({ex.SqlState})."); }
        catch (Exception ex) when (ex is NpgsqlException or TimeoutException or System.IO.IOException)
        { throw new ExchangeUnavailableException("Exchange unavailable; retry the same operation ID.",ex); }
    }
    private T Mutate<T>(string op, Func<NpgsqlConnection,NpgsqlTransaction,T> action) => Run(action, op);
    private static void RequireId(string id)
    { if (string.IsNullOrWhiteSpace(id) || id.Length > 128) throw new ExchangeRejectedException("Invalid operation ID."); }
    private static ExchangeReceipt? Replay(NpgsqlConnection c,NpgsqlTransaction t,string op,string fingerprint)
    {
        using var cmd=Cmd(c,t,"SELECT fingerprint,receipt::text FROM exchange.operations WHERE operation_id=$1",op);
        using var r=cmd.ExecuteReader(); if (!r.Read()) return null;
        if (r.GetString(0)!=fingerprint) throw new ExchangeRejectedException("Operation ID conflict.");
        return JsonSerializer.Deserialize<ExchangeReceipt>(r.GetString(1)) ?? throw new ExchangeRejectedException("Invalid stored receipt.");
    }
    private static void Save(NpgsqlConnection c,NpgsqlTransaction t,string op,string fingerprint,ExchangeReceipt receipt)
    { using var cmd=Cmd(c,t,"INSERT INTO exchange.operations VALUES ($1,$2,$3::jsonb)",op,fingerprint,JsonSerializer.Serialize(receipt)); cmd.ExecuteNonQuery(); }
    private static WalletView ReadWallet(NpgsqlConnection c,NpgsqlTransaction t,string owner,bool forUpdate=false)
    {
        using var cmd=Cmd(c,t,"SELECT balance,reserved,revision FROM exchange.wallets WHERE owner_id=$1"+(forUpdate?" FOR UPDATE":""),owner);
        using var r=cmd.ExecuteReader(); return r.Read() ? new(r.GetInt64(0),r.GetInt64(1),r.GetInt64(0)-r.GetInt64(1),r.GetInt64(2)) : new(0,0,0,0);
    }
    public WalletView Wallet(string owner) => Run((c,t)=>ReadWallet(c,t,owner));
    public IReadOnlyList<CurrencyEntry> Entries(string owner) => Run((c,t)=>
    {
        var result=new List<CurrencyEntry>();
        using var cmd=Cmd(c,t,"SELECT operation_id,sequence,delta,flow_class,code FROM exchange.wallet_entries WHERE owner_id=$1 ORDER BY sequence",owner);
        using var r=cmd.ExecuteReader(); while(r.Read()) result.Add(new(r.GetString(0),owner,"currency/gold",r.GetInt64(1),r.GetInt64(2),r.GetString(3),r.GetString(4)));
        return (IReadOnlyList<CurrencyEntry>)result;
    });
    private static void Post(NpgsqlConnection c,NpgsqlTransaction t,string op,string owner,long delta,string flow,string code,long reserveDelta=0)
    {
        using (var ensure=Cmd(c,t,"INSERT INTO exchange.wallets(owner_id) VALUES ($1) ON CONFLICT DO NOTHING",owner)) ensure.ExecuteNonQuery();
        var old=ReadWallet(c,t,owner,true);
        long balance,reserved,revision;
        try { balance=checked(old.Balance+delta); reserved=checked(old.Reserved+reserveDelta); revision=checked(old.Sequence+1); }
        catch (OverflowException) { throw new ExchangeRejectedException("Wallet overflow."); }
        if (balance<0 || reserved<0 || reserved>balance) throw new ExchangeRejectedException("Insufficient spendable gold.");
        using (var update=Cmd(c,t,"UPDATE exchange.wallets SET balance=$1,reserved=$2,revision=$3 WHERE owner_id=$4 AND revision=$5",balance,reserved,revision,owner,old.Sequence))
            if(update.ExecuteNonQuery()!=1) throw new ExchangeRejectedException("Stale wallet revision.");
        using var entry=Cmd(c,t,"INSERT INTO exchange.wallet_entries VALUES ($1,$2,$3,$4,$5,$6)",owner,revision,op,delta,flow,code); entry.ExecuteNonQuery();
    }
    public ExchangeReceipt Credit(string op,string owner,long amount,string source) => Mutate(op,(c,t)=>
    {
        RequireId(op); var fingerprint=JsonSerializer.Serialize(new { command="credit",owner,amount,source });
        if(Replay(c,t,op,fingerprint) is { } prior) return prior;
        if(!owner.StartsWith("chr_",StringComparison.Ordinal) || amount<=0 || source is not ("quest_reward" or "gm_grant" or "starter_kit")) throw new ExchangeRejectedException("Invalid trusted credit.");
        Post(c,t,op,owner,amount,"faucet",source);
        var receipt=new ExchangeReceipt(op,true,"ok",owner,amount); Save(c,t,op,fingerprint,receipt); return receipt;
    });
    private static DurableListing? ReadListing(NpgsqlConnection c,NpgsqlTransaction t,string id,bool forUpdate=false)
    {
        using var cmd=Cmd(c,t,"SELECT id,seller_id,seller_container_id,escrow_id,item_id,quantity,price,fee,expires_at,state,revision FROM exchange.listings WHERE id=$1"+(forUpdate?" FOR UPDATE":""),id);
        using var r=cmd.ExecuteReader(); return r.Read()?new(r.GetString(0),r.GetString(1),r.GetString(2),r.GetString(3),r.GetString(4),r.GetInt64(5),r.GetInt64(6),r.GetInt64(7),r.GetFieldValue<DateTimeOffset>(8),r.GetString(9),r.GetInt64(10)):null;
    }
    public DurableListing? Listing(string id)=>Run((c,t)=>ReadListing(c,t,id));
    private static void ReturnItems(string op,IReadOnlyList<EscrowDelivery> moves,IInventoryLedgerStore store)
    {
        if(moves.Count==0) return;
        var key=op+"/return";
        try
        {
            var existing=store.Execute(s=>s.Operations.TryGetValue(key,out var prior)?prior:null);
            LedgerReceipt receipt;
            if(existing is null)
            {
                var ledger=new ShatteredFrontier.InventoryLedger.InventoryLedger(store);
                var current=moves.Select(x=>x with { FromRevision=ledger.Read(x.FromId).Revision, ToRevision=ledger.Read(x.ToId).Revision }).ToArray();
                receipt=ledger.DeliverEscrowBatch(key,current);
            }
            else receipt=existing.Receipt;
            var changes=moves.SelectMany(x=>new[]{new QuantityChange(x.FromId,x.DefinitionId,-x.Quantity),new QuantityChange(x.ToId,x.DefinitionId,x.Quantity)}).ToArray();
            if(receipt.OperationId!=key || receipt.Action!=LedgerAction.Exchange || !receipt.Changes.SequenceEqual(changes))
                throw new ExchangeRejectedException("Inventory return receipt mismatch.");
        }
        catch(LedgerStoreUnavailableException ex) { throw new ExchangeUnavailableException("Inventory store unavailable; retry return.",ex); }
        catch(LedgerRejectedException ex) { throw new ExchangeRejectedException($"Inventory return rejected: {ex.Message}"); }
    }
    private ExchangeReceipt EndListing(string op,string id,string? seller,DateTimeOffset? now,IInventoryLedgerStore store) => Mutate(op,(c,t)=>
    {
        RequireId(op); var fingerprint=JsonSerializer.Serialize(new { command=now is null?"cancel-listing":"expire-listing",id,seller,now });
        if(Replay(c,t,op,fingerprint) is { } prior) return prior;
        var listing=ReadListing(c,t,id,true) ?? throw new ExchangeRejectedException("Listing missing.");
        if(listing.State!="active" || (seller is not null && seller!=listing.SellerId) || (now is not null && now<listing.ExpiresAt))
            throw new ExchangeRejectedException("Listing not cancellable.");
        ReturnItems(op,[new(listing.EscrowId,0,listing.SellerContainerId,0,listing.ItemId,listing.Quantity)],store);
        var state=now is null?"cancelled":"expired";
        using(var update=Cmd(c,t,"UPDATE exchange.listings SET state=$1,revision=revision+1 WHERE id=$2 AND state='active'",state,id))
            if(update.ExecuteNonQuery()!=1) throw new ExchangeRejectedException("Listing changed.");
        var receipt=new ExchangeReceipt(op,true,"ok",id); Save(c,t,op,fingerprint,receipt); return receipt;
    });
    public ExchangeReceipt CancelListing(string op,string id,string seller,IInventoryLedgerStore store)=>EndListing(op,id,seller,null,store);
    public IReadOnlyList<ExchangeReceipt> ExpireListings(DateTimeOffset now,IInventoryLedgerStore store)
    {
        var ids=Run((c,t)=>
        {
            var result=new List<string>();
            using var cmd=Cmd(c,t,"SELECT id FROM exchange.listings WHERE state='active' AND expires_at<=$1 ORDER BY id",now);
            using var r=cmd.ExecuteReader(); while(r.Read()) result.Add(r.GetString(0)); return result;
        });
        return ids.Select(id=>EndListing("expire/"+id,id,null,now,store)).ToArray();
    }
    public ExchangeReceipt CreateListing(string op,string id,string seller,string sellerContainer,string escrow,string item,long quantity,long price,long listingFee,DateTimeOffset now,ExchangeSettings? settings=null) => Mutate(op,(c,t)=>
    {
        RequireId(op); var duration=(long)(settings??new ExchangeSettings()).Get("tune.economy.market_listing_durations_hours");
        var fingerprint=JsonSerializer.Serialize(new { command="list",id,seller,sellerContainer,escrow,item,quantity,price,listingFee,now,duration });
        if(Replay(c,t,op,fingerprint) is { } prior) return prior;
        long total; try { total=checked(quantity*price); } catch(OverflowException) { throw new ExchangeRejectedException("Price overflow."); }
        if(!id.StartsWith("lst_",StringComparison.Ordinal) || !seller.StartsWith("chr_",StringComparison.Ordinal) || string.IsNullOrWhiteSpace(sellerContainer) || sellerContainer==escrow || quantity<=0 || price<=0 || listingFee<0 || listingFee>total) throw new ExchangeRejectedException("Invalid listing.");
        DateTimeOffset expires; try { expires=now.AddHours(duration); } catch(ArgumentOutOfRangeException) { throw new ExchangeRejectedException("Invalid listing time."); }
        Post(c,t,op,seller,-listingFee,"sink","market_listing_fee");
        using(var insert=Cmd(c,t,"INSERT INTO exchange.listings(id,seller_id,seller_container_id,escrow_id,item_id,quantity,price,fee,expires_at,state) VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,'active')",id,seller,sellerContainer,escrow,item,quantity,price,listingFee,expires)) insert.ExecuteNonQuery();
        var receipt=new ExchangeReceipt(op,true,"ok",id,0,listingFee); Save(c,t,op,fingerprint,receipt); return receipt;
    });
    public ExchangeReceipt BeginListing(string op,string id,string seller,string sellerContainer,string escrow,string item,long quantity,long price,long fee,DateTimeOffset now,ExchangeSettings? settings=null) => Mutate(op,(c,t)=>
    {
        RequireId(op);
        var duration=(long)(settings??new ExchangeSettings()).Get("tune.economy.market_listing_durations_hours");
        // CreatedAt is assigned once. A replay compares the immutable request fields below.
        using(var find=Cmd(c,t,"SELECT fingerprint,receipt::text FROM exchange.operations WHERE operation_id=$1",op))
        using(var reader=find.ExecuteReader()) if(reader.Read())
        {
            var prior=JsonSerializer.Deserialize<ListingIntent>(reader.GetString(0));
            if(prior is null || prior.Command!="listing-intent" || prior.Id!=id || prior.Seller!=seller || prior.SellerContainer!=sellerContainer ||
                prior.Escrow!=escrow || prior.Item!=item || prior.Quantity!=quantity || prior.Price!=price || prior.Fee!=fee || prior.DurationHours!=duration)
                throw new ExchangeRejectedException("Operation ID conflict.");
            return JsonSerializer.Deserialize<ExchangeReceipt>(reader.GetString(1))!;
        }
        long total; try { total=checked(quantity*price); } catch(OverflowException) { throw new ExchangeRejectedException("Price overflow."); }
        if(!id.StartsWith("lst_",StringComparison.Ordinal) || !seller.StartsWith("chr_",StringComparison.Ordinal) || string.IsNullOrWhiteSpace(sellerContainer) ||
            sellerContainer==escrow || quantity<=0 || price<=0 || fee<0 || fee>total) throw new ExchangeRejectedException("Invalid listing.");
        try { _=now.AddHours(duration); } catch(ArgumentOutOfRangeException) { throw new ExchangeRejectedException("Invalid listing time."); }
        if(ReadListing(c,t,id) is not null) throw new ExchangeRejectedException("Listing exists.");
        using(var occupied=Cmd(c,t,"SELECT 1 FROM exchange.operations WHERE receipt->>'SubjectId'=$1 AND receipt->>'Code'='pending' AND fingerprint::jsonb->>'Command'='listing-intent' LIMIT 1",id))
            if(occupied.ExecuteScalar() is not null) throw new ExchangeRejectedException("Listing intent exists.");
        if(fee>0) Post(c,t,op,seller,0,"reserve","market_listing_hold",fee);
        var intent=new ListingIntent("listing-intent",id,seller,sellerContainer,escrow,item,quantity,price,fee,now,duration,DateTimeOffset.UtcNow);
        var receipt=new ExchangeReceipt(op,true,"pending",id,0,fee);
        Save(c,t,op,JsonSerializer.Serialize(intent),receipt);
        return receipt;
    });
    public IReadOnlyList<string> PendingListings(DateTimeOffset cutoff)=>Run((c,t)=>
    {
        var ids=new List<string>();
        using var cmd=Cmd(c,t,"SELECT operation_id FROM exchange.operations WHERE receipt->>'Code'='pending' AND fingerprint::jsonb->>'Command'='listing-intent' AND (fingerprint::jsonb->>'CreatedAt')::timestamptz<=$1 ORDER BY operation_id LIMIT 100",cutoff);
        using var reader=cmd.ExecuteReader(); while(reader.Read()) ids.Add(reader.GetString(0)); return (IReadOnlyList<string>)ids;
    });
    public ExchangeReceipt ReconcileListing(string op,IInventoryLedgerStore ledgerStore)=>Mutate(op,(c,t)=>
    {
        using var find=Cmd(c,t,"SELECT fingerprint,receipt::text FROM exchange.operations WHERE operation_id=$1",op);
        string fingerprint; ExchangeReceipt prior;
        using(var reader=find.ExecuteReader())
        {
            if(!reader.Read()) throw new ExchangeRejectedException("Listing intent missing.");
            fingerprint=reader.GetString(0); prior=JsonSerializer.Deserialize<ExchangeReceipt>(reader.GetString(1))!;
        }
        var intent=JsonSerializer.Deserialize<ListingIntent>(fingerprint);
        if(intent?.Command!="listing-intent") throw new ExchangeRejectedException("Listing intent mismatch.");
        if(prior.Code!="pending") return prior;
        StoredOperation operation;
        try
        {
            operation=ledgerStore.Execute(s=>
            {
                var key=op+"/escrow";
                if(s.Operations.TryGetValue(key,out var existing)) return existing;
                var marker=new StoredOperation("exchange/listing-compensated",new LedgerReceipt(key,LedgerAction.Transfer,[],[]));
                s.Operations.Add(key,marker); return marker;
            });
        }
        catch(LedgerStoreUnavailableException ex) { throw new ExchangeUnavailableException("Inventory store unavailable; retry listing reconciliation.",ex); }
        var receipt=operation.Receipt;
        var compensated=operation.Fingerprint=="exchange/listing-compensated" && receipt.OperationId==op+"/escrow" &&
            receipt.Action==LedgerAction.Transfer && receipt.Revisions.Count==0 && receipt.Changes.Count==0;
        if(!compensated && (receipt.OperationId!=op+"/escrow" || receipt.Action!=LedgerAction.Transfer ||
            !receipt.Changes.SequenceEqual(new[]{new QuantityChange(intent.SellerContainer,intent.Item,-intent.Quantity),new QuantityChange(intent.Escrow,intent.Item,intent.Quantity)})))
            throw new ExchangeRejectedException("Inventory listing escrow receipt mismatch.");
        if(intent.Fee>0) Post(c,t,op,intent.Seller,compensated?0:-intent.Fee,compensated?"reserve":"sink",
            compensated?"market_listing_release":"market_listing_fee",-intent.Fee);
        if(!compensated)
        {
            using var insert=Cmd(c,t,"INSERT INTO exchange.listings(id,seller_id,seller_container_id,escrow_id,item_id,quantity,price,fee,expires_at,state) VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,'active')",
                intent.Id,intent.Seller,intent.SellerContainer,intent.Escrow,intent.Item,intent.Quantity,intent.Price,intent.Fee,intent.Now.AddHours(intent.DurationHours));
            insert.ExecuteNonQuery();
        }
        var result=new ExchangeReceipt(op,!compensated,compensated?"compensated":"ok",intent.Id,0,compensated?0:intent.Fee);
        using var update=Cmd(c,t,"UPDATE exchange.operations SET receipt=$1::jsonb WHERE operation_id=$2",JsonSerializer.Serialize(result),op);
        update.ExecuteNonQuery(); return result;
    });
    private static DurableSettlement? ReadSettlement(NpgsqlConnection c,NpgsqlTransaction t,string op,bool forUpdate=false)
    {
        using var cmd=Cmd(c,t,"SELECT operation_id,kind,subject_id,payer_id,payee_id,amount,fee,deliveries::text,state,receipt::text FROM exchange.settlements WHERE operation_id=$1"+(forUpdate?" FOR UPDATE":""),op);
        using var r=cmd.ExecuteReader(); if(!r.Read()) return null;
        return new(r.GetString(0),r.GetString(1),r.GetString(2),r.IsDBNull(3)?null:r.GetString(3),r.IsDBNull(4)?null:r.GetString(4),r.GetInt64(5),r.GetInt64(6),
            JsonSerializer.Deserialize<EscrowDelivery[]>(r.GetString(7))!,r.GetString(8),JsonSerializer.Deserialize<ExchangeReceipt>(r.GetString(9))!);
    }
    public DurableSettlement? Settlement(string op)=>Run((c,t)=>ReadSettlement(c,t,op));
    private static DurableSettlement Begin(NpgsqlConnection c,NpgsqlTransaction t,string op,string fingerprint,string kind,string subject,string? payer,string? payee,long amount,long fee,IReadOnlyList<EscrowDelivery> deliveries)
    {
        if(Replay(c,t,op,fingerprint) is not null)
            return ReadSettlement(c,t,op) ?? throw new ExchangeRejectedException("Missing settlement receipt.");
        if(deliveries.Count==0 || amount<0 || fee<0 || fee>amount || (payer is null)!=(payee is null) || (payer is null && (amount!=0 || fee!=0))) throw new ExchangeRejectedException("Invalid settlement.");
        if(payee is not null && ReadWallet(c,t,payee).Balance>long.MaxValue-amount) throw new ExchangeRejectedException("Payee wallet overflow.");
        if(payer is not null) Post(c,t,op,payer,0,"reserve","settlement_hold",amount);
        var receipt=new ExchangeReceipt(op,true,"pending",subject,amount,fee);
        using(var insert=Cmd(c,t,"INSERT INTO exchange.settlements(operation_id,fingerprint,kind,subject_id,payer_id,payee_id,amount,fee,deliveries,state,receipt) VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9::jsonb,'pending',$10::jsonb)",op,fingerprint,kind,subject,(object?)payer??DBNull.Value,(object?)payee??DBNull.Value,amount,fee,JsonSerializer.Serialize(deliveries),JsonSerializer.Serialize(receipt))) insert.ExecuteNonQuery();
        Save(c,t,op,fingerprint,receipt);
        return ReadSettlement(c,t,op)!;
    }
    public DurableSettlement BeginTrade(string op,string tradeId,string? payer,string? payee,long amount,IReadOnlyList<EscrowDelivery> deliveries)=>Mutate(op,(c,t)=>
    {
        RequireId(op); var fingerprint=JsonSerializer.Serialize(new { command="trade",tradeId,payer,payee,amount,deliveries });
        if(!tradeId.StartsWith("trd_",StringComparison.Ordinal) || payer==payee && payer is not null) throw new ExchangeRejectedException("Invalid trade.");
        return Begin(c,t,op,fingerprint,"trade",tradeId,payer,payee,amount,0,deliveries);
    });
    // The coordinator supplies both offers; their difference is the only wallet transfer for the trade.
    public DurableSettlement BeginTrade(string op,string tradeId,string partyA,string partyB,long goldA,long goldB,IReadOnlyList<EscrowDelivery> deliveries)
    {
        if(goldA<0 || goldB<0 || partyA==partyB) throw new ExchangeRejectedException("Invalid trade offers.");
        return goldA>=goldB
            ? BeginTrade(op,tradeId,goldA==goldB?null:partyA,goldA==goldB?null:partyB,goldA-goldB,deliveries)
            : BeginTrade(op,tradeId,partyB,partyA,goldB-goldA,deliveries);
    }
    public ExchangeReceipt CancelTrade(string op,string tradeId,string actor,IReadOnlyList<EscrowDelivery> returns,IInventoryLedgerStore store)=>Mutate(op,(c,t)=>
    {
        RequireId(op); var fingerprint=JsonSerializer.Serialize(new { command="cancel-trade",tradeId,actor,returns });
        if(Replay(c,t,op,fingerprint) is { } prior) return prior;
        using var find=Cmd(c,t,"SELECT operation_id FROM exchange.settlements WHERE kind='trade' AND subject_id=$1 AND state IN ('pending','compensated')",tradeId);
        var settleOp=find.ExecuteScalar() as string ?? throw new ExchangeRejectedException("Trade not cancellable.");
        var pending=ReadSettlement(c,t,settleOp,true)!;
        if(pending.State=="pending")
        {
            var key=settleOp+"/settle";
            StoredOperation marker;
            try { marker=store.Execute(s=>
            {
                if(s.Operations.TryGetValue(key,out var old)) return old;
                var value=new StoredOperation("exchange/compensated",new LedgerReceipt(key,LedgerAction.Exchange,[],[]));
                s.Operations.Add(key,value); return value;
            }); }
            catch(LedgerStoreUnavailableException ex) { throw new ExchangeUnavailableException("Inventory store unavailable; retry cancel.",ex); }
            if(marker.Fingerprint!="exchange/compensated" || marker.Receipt.OperationId!=key) throw new ExchangeRejectedException("Trade already delivered.");
        }
        ReturnItems(op,returns,store);
        if(pending.State=="pending" && pending.PayerId is { } payer) Post(c,t,op,payer,0,"reserve","settlement_release",-pending.Amount);
        using(var update=Cmd(c,t,"UPDATE exchange.settlements SET state='cancelled' WHERE operation_id=$1 AND state IN ('pending','compensated')",settleOp))
            if(update.ExecuteNonQuery()!=1) throw new ExchangeRejectedException("Trade changed.");
        var receipt=new ExchangeReceipt(op,true,"ok",tradeId); Save(c,t,op,fingerprint,receipt); return receipt;
    });
    public DurableSettlement BeginPurchase(string op,string listingId,string buyer,long expectedRevision,long expectedPrice,long transactionFee,IReadOnlyList<EscrowDelivery> deliveries)=>Mutate(op,(c,t)=>
    {
        RequireId(op); var fingerprint=JsonSerializer.Serialize(new { command="buy",listingId,buyer,expectedRevision,expectedPrice,transactionFee,deliveries });
        if(Replay(c,t,op,fingerprint) is not null) return ReadSettlement(c,t,op)!;
        var listing=ReadListing(c,t,listingId,true) ?? throw new ExchangeRejectedException("Listing missing.");
        if(listing.State!="active" || listing.Revision!=expectedRevision || listing.Price!=expectedPrice || listing.SellerId==buyer || DateTimeOffset.UtcNow>=listing.ExpiresAt ||
            deliveries.Count!=1 || deliveries[0].FromId!=listing.EscrowId || deliveries[0].DefinitionId!=listing.ItemId || deliveries[0].Quantity!=listing.Quantity)
            throw new ExchangeRejectedException("Listing unavailable or delivery mismatch.");
        long total; try { total=checked(listing.Price*listing.Quantity); } catch(OverflowException) { throw new ExchangeRejectedException("Price overflow."); }
        var result=Begin(c,t,op,fingerprint,"listing",listingId,buyer,listing.SellerId,total,transactionFee,deliveries);
        using var update=Cmd(c,t,"UPDATE exchange.listings SET state='pending',revision=revision+1,settlement_id=$1 WHERE id=$2 AND revision=$3 AND state='active'",op,listingId,expectedRevision);
        if(update.ExecuteNonQuery()!=1) throw new ExchangeRejectedException("Stale listing revision.");
        return result;
    });
    public DurableSettlement Reconcile(string op,IInventoryLedgerStore ledgerStore)=>Mutate(op,(c,t)=>
    {
        var pending=ReadSettlement(c,t,op,true) ?? throw new ExchangeRejectedException("Settlement missing.");
        if(pending.State!="pending") return pending;
        StoredOperation operation;
        try
        {
            operation=ledgerStore.Execute(s=>
            {
                var key=op+"/settle";
                if(s.Operations.TryGetValue(key,out var existing)) return existing;
                var marker=new StoredOperation("exchange/compensated",new LedgerReceipt(key,LedgerAction.Exchange,[],[]));
                s.Operations.Add(key,marker);
                return marker;
            });
        }
        catch(LedgerStoreUnavailableException ex) { throw new ExchangeUnavailableException("Inventory store unavailable; retry reconciliation.",ex); }
        catch(LedgerRejectedException ex) { throw new ExchangeRejectedException($"Inventory reconciliation rejected: {ex.Message}"); }
        var delivery=operation.Receipt;
        var compensated=operation.Fingerprint=="exchange/compensated" && delivery.OperationId==op+"/settle" &&
            delivery.Action==LedgerAction.Exchange && delivery.Revisions.Count==0 && delivery.Changes.Count==0;
        if(!compensated)
        {
            var expected=pending.Deliveries.SelectMany(x=>new[]{new QuantityChange(x.FromId,x.DefinitionId,-x.Quantity),new QuantityChange(x.ToId,x.DefinitionId,x.Quantity)}).ToArray();
            if(delivery.OperationId!=op+"/settle" || delivery.Action!=LedgerAction.Exchange ||
                delivery.Changes.Count!=expected.Length || !delivery.Changes.SequenceEqual(expected))
                throw new ExchangeRejectedException("Inventory settlement receipt mismatch.");
        }
        var committed=!compensated;
        if(pending.PayerId is { } payer)
        {
            if(committed)
            {
                Post(c,t,op,payer,-pending.Amount,"transfer",pending.Kind=="trade"?"trade_send":"market_purchase",-pending.Amount);
                Post(c,t,op,pending.PayeeId!,pending.Amount,"transfer",pending.Kind=="trade"?"trade_receive":"market_sale");
                if(pending.Fee>0) Post(c,t,op,pending.PayeeId!,-pending.Fee,"sink","market_transaction_fee");
            }
            else Post(c,t,op,payer,0,"reserve","settlement_release",-pending.Amount);
        }
        var state=committed?"committed":"compensated";
        var receipt=new ExchangeReceipt(op,committed,committed?"ok":"compensated",pending.SubjectId,committed?pending.Amount:0,committed?pending.Fee:0);
        using(var update=Cmd(c,t,"UPDATE exchange.settlements SET state=$1,receipt=$2::jsonb WHERE operation_id=$3 AND state='pending'",state,JsonSerializer.Serialize(receipt),op))
            if(update.ExecuteNonQuery()!=1) throw new ExchangeRejectedException("Settlement changed.");
        using(var update=Cmd(c,t,"UPDATE exchange.operations SET receipt=$1::jsonb WHERE operation_id=$2",JsonSerializer.Serialize(receipt),op)) update.ExecuteNonQuery();
        if(pending.Kind=="listing")
        {
            using var update=Cmd(c,t,"UPDATE exchange.listings SET state=$1,revision=revision+1,settlement_id=NULL WHERE id=$2 AND settlement_id=$3 AND state='pending'",committed?"sold":"active",pending.SubjectId,op);
            if(update.ExecuteNonQuery()!=1) throw new ExchangeRejectedException("Listing reservation changed.");
        }
        return ReadSettlement(c,t,op)!;
    });
}
