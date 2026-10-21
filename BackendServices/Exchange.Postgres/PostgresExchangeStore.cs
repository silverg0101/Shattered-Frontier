using System.Text.Json;
using Npgsql;
using ShatteredFrontier.Exchange;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;

namespace ShatteredFrontier.Exchange.Postgres;

public sealed class ExchangeUnavailableException(string message, Exception inner) : Exception(message, inner);
public sealed class ExchangeSchemaCompatibilityException(string message) : Exception(message);
public sealed class ExchangeRejectedException(string message) : InvalidOperationException(message);
public sealed record DurableListing(string Id, string SellerId, string EscrowId, string ItemId, long Quantity, long Price, long Fee, string State, long Revision);
public sealed record DurableSettlement(string OperationId, string Kind, string SubjectId, string? PayerId, string? PayeeId,
    long Amount, long Fee, IReadOnlyList<EscrowDelivery> Deliveries, string State, ExchangeReceipt Receipt);

public sealed class PostgresExchangeStore(string connectionString)
{
    private static NpgsqlCommand Cmd(NpgsqlConnection c, NpgsqlTransaction t, string sql, params object[] args)
    {
        var cmd = new NpgsqlCommand(sql, c, t);
        foreach (var arg in args) cmd.Parameters.AddWithValue(arg);
        return cmd;
    }
    private T Run<T>(Func<NpgsqlConnection,NpgsqlTransaction,T> action)
    {
        try
        {
            using var c = new NpgsqlConnection(connectionString); c.Open();
            using var t = c.BeginTransaction();
            // ponytail: This global advisory lock serializes all exchange writers; partition by subject after the coordinator has durable worker fencing.
            using (var advisory = Cmd(c,t,"SELECT pg_advisory_xact_lock(519761,3)")) advisory.ExecuteNonQuery();
            try
            {
                using var check = Cmd(c,t,"SELECT version FROM exchange.schema_version WHERE singleton=true FOR SHARE");
                if (check.ExecuteScalar() is not int version || version != 1) throw new ExchangeSchemaCompatibilityException("Unsupported exchange schema version.");
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
    private static WalletView ReadWallet(NpgsqlConnection c,NpgsqlTransaction t,string owner)
    {
        using var cmd=Cmd(c,t,"SELECT balance,reserved,revision FROM exchange.wallets WHERE owner_id=$1",owner);
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
        var old=ReadWallet(c,t,owner);
        long balance,reserved,revision;
        try { balance=checked(old.Balance+delta); reserved=checked(old.Reserved+reserveDelta); revision=checked(old.Sequence+1); }
        catch (OverflowException) { throw new ExchangeRejectedException("Wallet overflow."); }
        if (balance<0 || reserved<0 || reserved>balance) throw new ExchangeRejectedException("Insufficient spendable gold.");
        using (var update=Cmd(c,t,"UPDATE exchange.wallets SET balance=$1,reserved=$2,revision=$3 WHERE owner_id=$4 AND revision=$5",balance,reserved,revision,owner,old.Sequence))
            if(update.ExecuteNonQuery()!=1) throw new ExchangeRejectedException("Stale wallet revision.");
        using var entry=Cmd(c,t,"INSERT INTO exchange.wallet_entries VALUES ($1,$2,$3,$4,$5,$6)",owner,revision,op,delta,flow,code); entry.ExecuteNonQuery();
    }
    public ExchangeReceipt Credit(string op,string owner,long amount,string source) => Run((c,t)=>
    {
        RequireId(op); var fingerprint=JsonSerializer.Serialize(new { command="credit",owner,amount,source });
        if(Replay(c,t,op,fingerprint) is { } prior) return prior;
        if(!owner.StartsWith("chr_",StringComparison.Ordinal) || amount<=0 || source is not ("quest_reward" or "gm_grant" or "starter_kit")) throw new ExchangeRejectedException("Invalid trusted credit.");
        Post(c,t,op,owner,amount,"faucet",source);
        var receipt=new ExchangeReceipt(op,true,"ok",owner,amount); Save(c,t,op,fingerprint,receipt); return receipt;
    });
    private static DurableListing? ReadListing(NpgsqlConnection c,NpgsqlTransaction t,string id)
    {
        using var cmd=Cmd(c,t,"SELECT id,seller_id,escrow_id,item_id,quantity,price,fee,state,revision FROM exchange.listings WHERE id=$1",id);
        using var r=cmd.ExecuteReader(); return r.Read()?new(r.GetString(0),r.GetString(1),r.GetString(2),r.GetString(3),r.GetInt64(4),r.GetInt64(5),r.GetInt64(6),r.GetString(7),r.GetInt64(8)):null;
    }
    public DurableListing? Listing(string id)=>Run((c,t)=>ReadListing(c,t,id));
    public ExchangeReceipt CreateListing(string op,string id,string seller,string escrow,string item,long quantity,long price,long listingFee) => Run((c,t)=>
    {
        RequireId(op); var fingerprint=JsonSerializer.Serialize(new { command="list",id,seller,escrow,item,quantity,price,listingFee });
        if(Replay(c,t,op,fingerprint) is { } prior) return prior;
        if(!id.StartsWith("lst_",StringComparison.Ordinal) || !seller.StartsWith("chr_",StringComparison.Ordinal) || quantity<=0 || price<=0 || listingFee<0 || listingFee>checked(quantity*price)) throw new ExchangeRejectedException("Invalid listing.");
        Post(c,t,op,seller,-listingFee,"sink","market_listing_fee");
        using(var insert=Cmd(c,t,"INSERT INTO exchange.listings(id,seller_id,escrow_id,item_id,quantity,price,fee,state) VALUES ($1,$2,$3,$4,$5,$6,$7,'active')",id,seller,escrow,item,quantity,price,listingFee)) insert.ExecuteNonQuery();
        var receipt=new ExchangeReceipt(op,true,"ok",id,0,listingFee); Save(c,t,op,fingerprint,receipt); return receipt;
    });
    private static DurableSettlement? ReadSettlement(NpgsqlConnection c,NpgsqlTransaction t,string op)
    {
        using var cmd=Cmd(c,t,"SELECT operation_id,kind,subject_id,payer_id,payee_id,amount,fee,deliveries::text,state,receipt::text FROM exchange.settlements WHERE operation_id=$1",op);
        using var r=cmd.ExecuteReader(); if(!r.Read()) return null;
        return new(r.GetString(0),r.GetString(1),r.GetString(2),r.IsDBNull(3)?null:r.GetString(3),r.IsDBNull(4)?null:r.GetString(4),r.GetInt64(5),r.GetInt64(6),
            JsonSerializer.Deserialize<EscrowDelivery[]>(r.GetString(7))!,r.GetString(8),JsonSerializer.Deserialize<ExchangeReceipt>(r.GetString(9))!);
    }
    public DurableSettlement? Settlement(string op)=>Run((c,t)=>ReadSettlement(c,t,op));
    private static DurableSettlement Begin(NpgsqlConnection c,NpgsqlTransaction t,string op,string fingerprint,string kind,string subject,string? payer,string? payee,long amount,long fee,IReadOnlyList<EscrowDelivery> deliveries)
    {
        if(Replay(c,t,op,fingerprint) is not null)
            return ReadSettlement(c,t,op) ?? throw new ExchangeRejectedException("Missing settlement receipt.");
        if(deliveries.Count==0 || amount<0 || fee<0 || fee>amount || (payer is null)!=(payee is null)) throw new ExchangeRejectedException("Invalid settlement.");
        if(payer is not null) Post(c,t,op,payer,0,"reserve","settlement_hold",amount);
        var receipt=new ExchangeReceipt(op,true,"pending",subject,amount,fee);
        using(var insert=Cmd(c,t,"INSERT INTO exchange.settlements VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9::jsonb,'pending',$10::jsonb)",op,fingerprint,kind,subject,(object?)payer??DBNull.Value,(object?)payee??DBNull.Value,amount,fee,JsonSerializer.Serialize(deliveries),JsonSerializer.Serialize(receipt))) insert.ExecuteNonQuery();
        Save(c,t,op,fingerprint,receipt);
        return ReadSettlement(c,t,op)!;
    }
    public DurableSettlement BeginTrade(string op,string tradeId,string? payer,string? payee,long amount,IReadOnlyList<EscrowDelivery> deliveries)=>Run((c,t)=>
    {
        RequireId(op); var fingerprint=JsonSerializer.Serialize(new { command="trade",tradeId,payer,payee,amount,deliveries });
        if(!tradeId.StartsWith("trd_",StringComparison.Ordinal) || payer==payee && payer is not null) throw new ExchangeRejectedException("Invalid trade.");
        return Begin(c,t,op,fingerprint,"trade",tradeId,payer,payee,amount,0,deliveries);
    });
    public DurableSettlement BeginPurchase(string op,string listingId,string buyer,long expectedRevision,long expectedPrice,long transactionFee,IReadOnlyList<EscrowDelivery> deliveries)=>Run((c,t)=>
    {
        RequireId(op); var fingerprint=JsonSerializer.Serialize(new { command="buy",listingId,buyer,expectedRevision,expectedPrice,transactionFee,deliveries });
        if(Replay(c,t,op,fingerprint) is not null) return ReadSettlement(c,t,op)!;
        var listing=ReadListing(c,t,listingId) ?? throw new ExchangeRejectedException("Listing missing.");
        if(listing.State!="active" || listing.Revision!=expectedRevision || listing.Price!=expectedPrice || listing.SellerId==buyer ||
            deliveries.Count!=1 || deliveries[0].FromId!=listing.EscrowId || deliveries[0].DefinitionId!=listing.ItemId || deliveries[0].Quantity!=listing.Quantity)
            throw new ExchangeRejectedException("Listing unavailable or delivery mismatch.");
        long total; try { total=checked(listing.Price*listing.Quantity); } catch(OverflowException) { throw new ExchangeRejectedException("Price overflow."); }
        var result=Begin(c,t,op,fingerprint,"listing",listingId,buyer,listing.SellerId,total,transactionFee,deliveries);
        using var update=Cmd(c,t,"UPDATE exchange.listings SET state='pending',revision=revision+1,settlement_id=$1 WHERE id=$2 AND revision=$3 AND state='active'",op,listingId,expectedRevision);
        if(update.ExecuteNonQuery()!=1) throw new ExchangeRejectedException("Stale listing revision.");
        return result;
    });
    public DurableSettlement Reconcile(string op,IInventoryLedgerStore ledgerStore)=>Run((c,t)=>
    {
        var pending=ReadSettlement(c,t,op) ?? throw new ExchangeRejectedException("Settlement missing.");
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
            var revisions=pending.Deliveries.SelectMany(x=>new[]{new KeyValuePair<string,long>(x.FromId,x.FromRevision),new KeyValuePair<string,long>(x.ToId,x.ToRevision)})
                .DistinctBy(x=>x.Key).ToDictionary(x=>x.Key,x=>x.Value,StringComparer.Ordinal);
            if(delivery.OperationId!=op+"/settle" || delivery.Action!=LedgerAction.Exchange ||
                delivery.Changes.Count!=expected.Length || !delivery.Changes.SequenceEqual(expected) ||
                delivery.Revisions.Count!=revisions.Count || delivery.Revisions.Any(x=>!revisions.TryGetValue(x.ContainerId,out var before) || x.Before!=before || x.After!=before+1))
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
