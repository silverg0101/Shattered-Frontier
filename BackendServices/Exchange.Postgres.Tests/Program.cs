using ShatteredFrontier.Exchange.Postgres;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;

static void Check(bool condition, string message)
{
    if (!condition) throw new Exception(message);
    Console.WriteLine("PASS " + message);
}
var connection = Environment.GetEnvironmentVariable("EXCHANGE_CONNECTION") ?? throw new Exception("Missing fixture connection.");
var inventoryConnection = Environment.GetEnvironmentVariable("INVENTORY_CONNECTION") ?? throw new Exception("Missing inventory fixture connection.");
var exchange = new PostgresExchangeStore(connection);
var ledgerStore = new PostgresInventoryLedgerStore(inventoryConnection);
var ledger = new InventoryLedger(ledgerStore);
static EscrowDelivery Move(string from, long fromRev, string to, long toRev) => new(from, fromRev, to, toRev, "item/ore", 1);
switch (args[0])
{
    case "seed":
        exchange.Credit("credit-seller", "chr_seller", 20, "starter_kit");
        exchange.Credit("credit-a", "chr_a", 100, "starter_kit");
        exchange.Credit("credit-b", "chr_b", 100, "starter_kit");
        foreach (var id in new[] { "ctn_esc_trade", "ctn_esc_market", "ctn_esc_late", "ctn_esc_tomb", "ctn_esc_abort", "ctn_esc_cancel", "ctn_esc_expire", "ctn_bag_a", "ctn_bag_b", "ctn_bag_seller" })
            ledger.CreateContainer(new(id, OwnerKind.Character, "chr_seller", id.StartsWith("ctn_esc_") ? ContainerKind.Escrow : ContainerKind.Backpack, 20));
        ledger.Grant("fund-trade", "ctn_esc_trade", 0, "item/ore", 1, "fixture", "trade");
        ledger.Grant("fund-market", "ctn_esc_market", 0, "item/ore", 1, "fixture", "market");
        ledger.Grant("fund-late", "ctn_esc_late", 0, "item/ore", 1, "fixture", "late");
        ledger.Grant("fund-tomb", "ctn_esc_tomb", 0, "item/ore", 1, "fixture", "tomb");
        ledger.Grant("fund-abort", "ctn_esc_abort", 0, "item/ore", 1, "fixture", "abort");
        ledger.Grant("fund-cancel", "ctn_esc_cancel", 0, "item/ore", 1, "fixture", "cancel");
        ledger.Grant("fund-expire", "ctn_esc_expire", 0, "item/ore", 1, "fixture", "expire");
        exchange.CreateListing("list", "lst_one", "chr_seller", "ctn_bag_seller", "ctn_esc_market", "item/ore", 1, 30, 2,DateTimeOffset.UtcNow);
        Check(exchange.Wallet("chr_seller").Balance == 18 && exchange.Listing("lst_one")?.State == "active", "wallet and listing acknowledged");
        break;
    case "tombstone":
        exchange.BeginTrade("tomb","trd_tomb","chr_a","chr_seller",4,[Move("ctn_esc_tomb",1,"ctn_bag_seller",1)]);
        ledgerStore.Execute(s=>
        {
            s.Operations.Add("tomb/settle",new StoredOperation("exchange/compensated",new LedgerReceipt("tomb/settle",LedgerAction.Exchange,[],[])));
            return true;
        });
        Console.WriteLine("PASS tombstone committed before immediate stop");
        break;
    case "tombstone-recover":
        Check(exchange.Settlement("tomb")?.State=="pending" && exchange.Wallet("chr_a").Reserved==4,"tombstone crash left Exchange pending");
        Check(exchange.Reconcile("tomb",ledgerStore).State=="compensated" && exchange.Wallet("chr_a").Reserved==0,"tombstone recovery releases once");
        exchange.BeginTrade("abort","trd_abort","chr_a","chr_seller",3,[Move("ctn_esc_abort",1,"ctn_bag_seller",1)]);
        Check(exchange.CancelTrade("abort-cancel","trd_abort","chr_a",[Move("ctn_esc_abort",1,"ctn_bag_a",1)],ledgerStore).Accepted &&
            exchange.Wallet("chr_a").Reserved==0 && exchange.Settlement("abort")?.State=="cancelled","pending trade cancel releases hold and returns item");
        Check(exchange.CancelTrade("abort-cancel","trd_abort","chr_a",[Move("ctn_esc_abort",1,"ctn_bag_a",1)],ledgerStore).Accepted,"pending trade cancel replays");
        break;
    case "restart":
        Check(exchange.Wallet("chr_seller").Balance == 18 && exchange.Listing("lst_one")?.State == "active", "wallet and listing survive immediate stop");
        Check(exchange.Credit("credit-a", "chr_a", 100, "starter_kit").Amount == 100 && exchange.Wallet("chr_a").Balance == 100, "credit retry returns original receipt");
        var trade = exchange.BeginTrade("trade", "trd_one", "chr_a", "chr_seller", 10, [Move("ctn_esc_trade", 1, "ctn_bag_seller", 0)]);
        Check(trade.State == "pending" && exchange.Wallet("chr_a").Reserved == 10, "trade reserves spend");
        ledger.DeliverEscrowBatch("trade/settle", trade.Deliveries);
        Console.WriteLine("PASS crash point after ledger delivery before exchange commit");
        break;
    case "recover":
        Check(exchange.Settlement("trade")?.State == "pending" && ledger.Read("ctn_bag_seller").Quantities["item/ore"] == 1, "interrupted trade has durable debit only");
        var settled = exchange.Reconcile("trade", ledgerStore);
        Check(settled.State == "committed" && exchange.Reconcile("trade", ledgerStore).Receipt == settled.Receipt, "trade commits both sides exactly once");
        Check(exchange.Wallet("chr_a").Balance == 90 && exchange.Wallet("chr_seller").Balance == 28 && exchange.Wallet("chr_a").Reserved == 0, "trade gold conserved");
        var late = Move("ctn_esc_late", 1, "ctn_bag_a", 0);
        exchange.BeginTrade("late", "trd_late", "chr_a", "chr_seller", 5, [late]);
        try { exchange.Reconcile("late", new FailingLedgerStore(true)); throw new Exception("Ledger outage accepted."); }
        catch (ExchangeUnavailableException) { Console.WriteLine("PASS ledger outage remains retryable and pending"); }
        try { exchange.Reconcile("late", new FailingLedgerStore(false)); throw new Exception("Ledger rejection accepted."); }
        catch (ExchangeRejectedException) { Console.WriteLine("PASS ledger rejection is non-retryable"); }
        Check(exchange.Settlement("late")?.State == "pending", "ledger errors leave reservation pending");
        Check(exchange.Reconcile("late", ledgerStore).State == "compensated", "missing delivery compensated");
        try { ledger.DeliverEscrowBatch("late/settle", [late]); throw new Exception("Late delivery accepted."); }
        catch (LedgerRejectedException) { Console.WriteLine("PASS retry after compensation rejected by ledger fingerprint"); }
        Check(exchange.Wallet("chr_a").Balance == 90 && ledger.Read("ctn_esc_late").Quantities["item/ore"] == 1, "compensation left both sides unchanged");
        Check(exchange.CancelTrade("cancel-late","trd_late","chr_a",[Move("ctn_esc_late",1,"ctn_bag_a",0)],ledgerStore).Accepted &&
            exchange.CancelTrade("cancel-late","trd_late","chr_a",[Move("ctn_esc_late",1,"ctn_bag_a",0)],ledgerStore).Accepted,
            "trade cancellation returns escrow and replays");
        Check(ledger.Read("ctn_bag_a").Quantities["item/ore"]==1 && exchange.Wallet("chr_a").Reserved==0,"cancelled trade has one return and no hold");
        ledger.Grant("fund-net", "ctn_esc_late", 2, "item/ore", 1, "fixture", "net");
        var net=exchange.BeginTrade("net","trd_net","chr_a","chr_b",12,7,[Move("ctn_esc_late",3,"ctn_bag_b",0)]);
        Check(net.PayerId=="chr_a" && net.PayeeId=="chr_b" && net.Amount==5,"bidirectional gold offers net to one transfer");
        ledger.DeliverEscrowBatch("net/settle",[Move("ctn_esc_late",3,"ctn_bag_b",0)]);
        Check(exchange.Reconcile("net",ledgerStore).State=="committed","netted trade settles");
        break;
    case "concurrent":
        ledger.CreateContainer(new("ctn_esc_listing_race",OwnerKind.Character,"chr_seller",ContainerKind.Escrow,20));
        ledger.Grant("fund-listing-race","ctn_bag_seller",ledger.Read("ctn_bag_seller").Revision,"item/ore",1,"fixture","listing-race");
        using (var beginBarrier = new Barrier(2))
        {
            var beginTasks = new[] { "listing-race-a", "listing-race-b" }.Select(op => Task.Run(() =>
            {
                beginBarrier.SignalAndWait(TimeSpan.FromSeconds(10));
                try { return (Operation:op, Receipt:exchange.BeginListing(op,"lst_race","chr_seller","ctn_bag_seller","ctn_esc_listing_race","item/ore",1,5,2,DateTimeOffset.UtcNow), Error:""); }
                catch (ExchangeRejectedException ex) { return (Operation:op, Receipt:(ShatteredFrontier.Exchange.ExchangeReceipt?)null, Error:ex.Message); }
            })).ToArray();
            Task.WaitAll(beginTasks);
            Check(beginTasks.Count(t => t.Result.Receipt?.Code=="pending")==1 &&
                beginTasks.Count(t => t.Result.Error=="Listing intent exists.")==1,"same listing intent has one winner");
            var winnerOp=beginTasks.Single(t => t.Result.Receipt?.Code=="pending").Result.Operation;
            ledger.Transfer(winnerOp+"/escrow","ctn_bag_seller",ledger.Read("ctn_bag_seller").Revision,
                "ctn_esc_listing_race",0,"item/ore",1);
            Check(exchange.ReconcileListing(winnerOp,ledgerStore).Code=="ok","winning listing reconciles");
            var loserOp=beginTasks.Single(t => t.Result.Receipt is null).Result.Operation;
            try { exchange.ReconcileListing(loserOp,ledgerStore); throw new Exception("Rejected listing reconciled."); }
            catch (ExchangeRejectedException ex) when (ex.Message=="Listing intent missing.") { }
            Check(exchange.Listing("lst_race")?.State=="active" && exchange.Wallet("chr_seller").Reserved==0 &&
                exchange.Entries("chr_seller").Count(e=>e.Code=="market_listing_fee" && e.OperationId==winnerOp)==1,
                "listing race leaves no stranded fee");
        }
        foreach (var suffix in new[]{"ab","ba"})
        {
            ledger.CreateContainer(new("ctn_esc_swap_"+suffix,OwnerKind.Character,"chr_seller",ContainerKind.Escrow,20));
            ledger.CreateContainer(new("ctn_bag_swap_"+suffix,OwnerKind.Character,"chr_seller",ContainerKind.Backpack,20));
            ledger.Grant("fund-swap-"+suffix,"ctn_esc_swap_"+suffix,0,"item/ore",1,"fixture","swap");
        }
        var swapA=Move("ctn_esc_swap_ab",1,"ctn_bag_swap_ab",0);
        var swapB=Move("ctn_esc_swap_ba",1,"ctn_bag_swap_ba",0);
        exchange.BeginTrade("swap-ab","trd_swap_ab","chr_a","chr_b",3,[swapA]);
        exchange.BeginTrade("swap-ba","trd_swap_ba","chr_b","chr_a",4,[swapB]);
        ledger.DeliverEscrowBatch("swap-ab/settle",[swapA]);
        ledger.DeliverEscrowBatch("swap-ba/settle",[swapB]);
        using (var settleBarrier = new Barrier(2))
        {
            var settlements = new[]{"swap-ab","swap-ba"}.Select(op=>Task.Run(() =>
            {
                settleBarrier.SignalAndWait(TimeSpan.FromSeconds(10));
                return exchange.Reconcile(op,ledgerStore).State;
            })).ToArray();
            Task.WaitAll(settlements);
            Check(settlements.All(t=>t.Result=="committed") && exchange.Wallet("chr_a").Reserved==0 &&
                exchange.Wallet("chr_b").Reserved==0,"swapped payer settlements commit without deadlock");
        }
        var ready = new CountdownEvent(2);
        var start = new ManualResetEventSlim(false);
        var tasks = new[] { ("buy-a", "chr_a", "ctn_bag_a"), ("buy-b", "chr_b", "ctn_bag_b") }.Select(x => Task.Run(() =>
        {
            var store = new PostgresExchangeStore(connection);
            ready.Signal(); start.Wait();
            try { return store.BeginPurchase(x.Item1, "lst_one", x.Item2, 0, 30, 3, [Move("ctn_esc_market", 1, x.Item3, 0)]).State == "pending"; }
            catch (ExchangeRejectedException) { return false; }
        })).ToArray();
        Check(ready.Wait(TimeSpan.FromSeconds(10)), "buyers ready on separate connections");
        start.Set(); Task.WaitAll(tasks);
        Check(tasks.Count(x => x.Result) == 1 && exchange.Listing("lst_one")?.State == "pending", "one concurrent buyer reserves listing");
        var winner = tasks[0].Result ? "buy-a" : "buy-b";
        var pending = exchange.Settlement(winner)!;
        var destination=pending.Deliveries[0].ToId;
        ledger.Grant("advance-revision",destination,ledger.Read(destination).Revision,"item/ore",1,"fixture","advance");
        var fresh=Move("ctn_esc_market",ledger.Read("ctn_esc_market").Revision,pending.Deliveries[0].ToId,ledger.Read(pending.Deliveries[0].ToId).Revision);
        ledger.DeliverEscrowBatch(winner + "/settle", [fresh]);
        Check(exchange.Reconcile(winner, ledgerStore).State == "committed" && exchange.Listing("lst_one")?.State == "sold", "re-read revision receipt reconciles sale once");
        Check(exchange.Reconcile(winner, ledgerStore).Receipt == exchange.Settlement(winner)!.Receipt, "purchase retry returns same receipt");
        var total = exchange.Wallet("chr_a").Balance + exchange.Wallet("chr_b").Balance + exchange.Wallet("chr_seller").Balance;
        Check(total == 213 && exchange.Wallet("chr_seller").Balance == 53, "listing and transaction fees conserved as seven copper sinks");
        Check(exchange.Entries("chr_seller").Where(x=>x.FlowClass=="sink").Sum(x=>-x.Delta)==7,"entries account for all fee sinks");
        exchange.CreateListing("list-cancel","lst_cancel","chr_seller","ctn_bag_seller","ctn_esc_cancel","item/ore",1,5,0,DateTimeOffset.UtcNow);
        Check(exchange.CancelListing("cancel","lst_cancel","chr_seller",ledgerStore).Accepted && exchange.CancelListing("cancel","lst_cancel","chr_seller",ledgerStore).Accepted && exchange.Listing("lst_cancel")?.State=="cancelled","listing cancel returns and replays");
        exchange.CreateListing("list-expire","lst_expire","chr_seller","ctn_bag_seller","ctn_esc_expire","item/ore",1,5,0,DateTimeOffset.UtcNow);
        Check(exchange.ExpireListings(DateTimeOffset.UtcNow.AddDays(3),ledgerStore).Count==2 && exchange.Listing("lst_expire")?.State=="expired" &&
            exchange.Listing("lst_race")?.State=="expired","expired listings return escrow once");
        Check(exchange.ExpireListings(DateTimeOffset.UtcNow.AddDays(3),ledgerStore).Count==0,"expiry sweep replay does not refund twice");
        try { exchange.CreateListing("overflow-list","lst_overflow","chr_seller","ctn_bag_seller","ctn_esc_cancel","item/ore",long.MaxValue,2,0,DateTimeOffset.UtcNow); throw new Exception("Overflow accepted."); }
        catch(ExchangeRejectedException) { Console.WriteLine("PASS listing price overflow rejected"); }
        try { exchange.BeginTrade("bad-item","trd_bad",null,null,1,[Move("ctn_esc_cancel",1,"ctn_bag_a",0)]); throw new Exception("Item-only money accepted."); }
        catch(ExchangeRejectedException) { Console.WriteLine("PASS item-only amount requires zero"); }
        exchange.Credit("rich","chr_rich",long.MaxValue,"gm_grant");
        try { exchange.BeginTrade("overflow-payee","trd_overflow","chr_a","chr_rich",1,[Move("ctn_esc_cancel",2,"ctn_bag_b",2)]); throw new Exception("Payee overflow accepted."); }
        catch(ExchangeRejectedException) { Console.WriteLine("PASS payee overflow refused before delivery"); }
        try { exchange.CancelListing("cancel-sold","lst_one","chr_seller",ledgerStore); throw new Exception("Sold listing cancelled."); }
        catch(ExchangeRejectedException) { Console.WriteLine("PASS cancellation after sale rejected"); }
        break;
    case "outage":
        try { exchange.Wallet("chr_a"); throw new Exception("Outage accepted."); }
        catch (ExchangeUnavailableException) { Console.WriteLine("PASS exchange outage fails closed"); }
        break;
    case "newer-schema":
        try { exchange.Wallet("chr_a"); throw new Exception("Newer schema accepted."); }
        catch (ExchangeSchemaCompatibilityException) { Console.WriteLine("PASS newer schema refused"); }
        break;
    default: throw new Exception("Unknown phase.");
}

sealed class FailingLedgerStore(bool unavailable) : IInventoryLedgerStore
{
    public T Execute<T>(Func<LedgerState, T> transaction)
    {
        if (unavailable) throw new LedgerStoreUnavailableException("fixture", new TimeoutException());
        throw new LedgerRejectedException("fixture");
    }
}
