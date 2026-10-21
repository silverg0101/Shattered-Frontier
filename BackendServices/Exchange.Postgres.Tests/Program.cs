using ShatteredFrontier.Exchange.Postgres;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;

static void Check(bool condition, string message)
{
    if (!condition) throw new Exception(message);
    Console.WriteLine("PASS " + message);
}
var connection = Environment.GetEnvironmentVariable("EXCHANGE_CONNECTION") ?? throw new Exception("Missing fixture connection.");
var exchange = new PostgresExchangeStore(connection);
var ledgerStore = new PostgresInventoryLedgerStore(connection);
var ledger = new InventoryLedger(ledgerStore);
static EscrowDelivery Move(string from, long fromRev, string to, long toRev) => new(from, fromRev, to, toRev, "item/ore", 1);
switch (args[0])
{
    case "seed":
        exchange.Credit("credit-seller", "chr_seller", 20, "starter_kit");
        exchange.Credit("credit-a", "chr_a", 100, "starter_kit");
        exchange.Credit("credit-b", "chr_b", 100, "starter_kit");
        foreach (var id in new[] { "ctn_esc_trade", "ctn_esc_market", "ctn_esc_late", "ctn_bag_a", "ctn_bag_b", "ctn_bag_seller" })
            ledger.CreateContainer(new(id, OwnerKind.Character, "chr_seller", id.StartsWith("ctn_esc_") ? ContainerKind.Escrow : ContainerKind.Backpack, 20));
        ledger.Grant("fund-trade", "ctn_esc_trade", 0, "item/ore", 1, "fixture", "trade");
        ledger.Grant("fund-market", "ctn_esc_market", 0, "item/ore", 1, "fixture", "market");
        ledger.Grant("fund-late", "ctn_esc_late", 0, "item/ore", 1, "fixture", "late");
        exchange.CreateListing("list", "lst_one", "chr_seller", "ctn_esc_market", "item/ore", 1, 30, 2);
        Check(exchange.Wallet("chr_seller").Balance == 18 && exchange.Listing("lst_one")?.State == "active", "wallet and listing acknowledged");
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
        break;
    case "concurrent":
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
        ledger.DeliverEscrowBatch(winner + "/settle", pending.Deliveries);
        Check(exchange.Reconcile(winner, ledgerStore).State == "committed" && exchange.Listing("lst_one")?.State == "sold", "sale delivered once");
        Check(exchange.Reconcile(winner, ledgerStore).Receipt == exchange.Settlement(winner)!.Receipt, "purchase retry returns same receipt");
        var total = exchange.Wallet("chr_a").Balance + exchange.Wallet("chr_b").Balance + exchange.Wallet("chr_seller").Balance;
        Check(total == 215 && exchange.Wallet("chr_seller").Balance == 55, "listing and transaction fees conserved as five copper sinks");
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
