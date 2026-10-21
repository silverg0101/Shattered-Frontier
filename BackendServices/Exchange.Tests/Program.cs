using ShatteredFrontier.Exchange;
using ShatteredFrontier.InventoryLedger;
using InventoryService = ShatteredFrontier.InventoryLedger.InventoryLedger;

static void Check(bool okay, string message) { if (!okay) throw new Exception(message); }
static (ExchangeService Exchange, InventoryService Items) Fixture()
{
    var items = new InventoryService(new InMemoryInventoryLedgerStore());
    foreach (var actor in new[] { "chr_a", "chr_b", "chr_c" })
    {
        items.CreateContainer(new("ctn_" + actor + "_bag", OwnerKind.Character, actor, ContainerKind.Backpack, 100));
        items.CreateContainer(new("ctn_" + actor + "_esc", OwnerKind.Character, actor, ContainerKind.Escrow, 100));
    }
    items.Grant("seed_a", "ctn_chr_a_bag", 0, "item/ore", 5, "starter_kit", "fixture");
    items.Grant("seed_b", "ctn_chr_b_bag", 0, "item/log", 3, "starter_kit", "fixture");
    var exchange = new ExchangeService(items);
    foreach (var actor in new[] { "chr_a", "chr_b", "chr_c" }) Check(exchange.Credit("fund_" + actor, actor, "currency/gold", 100000, "gm_grant").Accepted, "fund");
    return (exchange, items);
}
static OfferedItem Item(string from, string to, string kind, long qty) => new("ctn_" + from + "_bag", "ctn_" + from + "_esc", "ctn_" + to + "_bag", kind, qty);
static long Qty(InventoryService items, string actor, string kind) => items.Read("ctn_" + actor + "_bag").Quantities.GetValueOrDefault(kind);
var eligible = new MarketEligibility(true, false, false, false, "material", 10, 10000, true, true);

{
    var (x, items) = Fixture();
    var a = new TradeOffer("chr_a", Item("chr_a", "chr_b", "item/ore", 2), 1000);
    var b = new TradeOffer("chr_b", Item("chr_b", "chr_a", "item/log", 1), 500);
    Check(x.OpenTrade("open", "trd_one", a, b).Accepted, "open");
    Check(x.LockTrade("lock_a", "trd_one", "chr_a").Accepted, "lock a");
    try { x.AcceptTrade("lock_a", "trd_one", "chr_a"); throw new Exception("cross-command operation ID accepted"); }
    catch (InvalidOperationException) { }
    Check(x.LockTrade("lock_b", "trd_one", "chr_b").Accepted, "lock b");
    Check(x.AcceptTrade("accept_a", "trd_one", "chr_a").Accepted, "accept a");
    Check(Qty(items, "chr_a", "item/log") == 0 && Qty(items, "chr_b", "item/ore") == 0, "no one-sided delivery");
    var receipt = x.AcceptTrade("accept_b", "trd_one", "chr_b");
    Check(receipt.Accepted && ReferenceEquals(receipt, x.AcceptTrade("accept_b", "trd_one", "chr_b")), "settle replay");
    Check(Qty(items, "chr_a", "item/log") == 1 && Qty(items, "chr_b", "item/ore") == 2, "both items delivered");
    Check(x.Wallet("chr_a").Balance == 99500 && x.Wallet("chr_b").Balance == 100500, "both gold legs");
    Check(x.Wallet("chr_a").Reserved == 0 && x.Wallet("chr_b").Reserved == 0, "reserves clear");
}
{
    var (x, items) = Fixture();
    var a = new TradeOffer("chr_a", Item("chr_a", "chr_b", "item/ore", 2), 1000);
    var b = new TradeOffer("chr_b", Item("chr_b", "chr_a", "item/log", 1), 500);
    x.OpenTrade("open2", "trd_two", a, b); x.LockTrade("la2", "trd_two", "chr_a"); x.LockTrade("lb2", "trd_two", "chr_b");
    x.AcceptTrade("aa2", "trd_two", "chr_a");
    // Fill the destination so settlement fails atomically.
    items.Grant("block", "ctn_chr_a_bag", items.Read("ctn_chr_a_bag").Revision, "item/stone", 97, "starter_kit", "fixture");
    var failed = x.AcceptTrade("ab2", "trd_two", "chr_b");
    Check(!failed.Accepted && x.TradeState("trd_two") == "locked", "failed settlement stays locked");
    Check(Qty(items, "chr_b", "item/ore") == 0 && Qty(items, "chr_a", "item/log") == 0, "atomic failure");
    Check(ReferenceEquals(failed, x.AcceptTrade("ab2", "trd_two", "chr_b")), "stable failure replay");
    // Remove blocker, then cancel: both escrow sides return.
    items.Consume("unblock", "ctn_chr_a_bag", items.Read("ctn_chr_a_bag").Revision, "item/stone", 97, "item_destroyed");
    Check(x.CancelTrade("cancel2", "trd_two", "chr_a").Accepted, "cancel");
    Check(Qty(items, "chr_a", "item/ore") == 5 && Qty(items, "chr_b", "item/log") == 3, "trade escrow returned");
    Check(x.Wallet("chr_a").Reserved == 0 && x.Wallet("chr_b").Reserved == 0, "cancel releases reserves");
}
{
    var (x, items) = Fixture(); var now = DateTimeOffset.Parse("2026-10-21T12:00:00Z");
    Check(!x.List("reject_bound", "lst_bound", "chr_a", "ctn_chr_a_bag", "ctn_chr_a_esc", "item/ore", 1, 1000,
        MarketScope.GlobalExchange, now, eligible with { Bound = true }).Accepted, "bound item rejected");
    Check(Qty(items, "chr_a", "item/ore") == 5, "rejected listing keeps item");
    var listed = x.List("list", "lst_one", "chr_a", "ctn_chr_a_bag", "ctn_chr_a_esc", "item/ore", 2, 10000,
        MarketScope.GlobalExchange, now, eligible);
    Check(listed.Accepted && listed.Fee == 200, "listing fee");
    var first = Task.Run(() => x.Buy("buy_b", "lst_one", "chr_b", "ctn_chr_b_bag", 10000, now));
    var second = Task.Run(() => x.Buy("buy_c", "lst_one", "chr_c", "ctn_chr_c_bag", 10000, now));
    Task.WaitAll(first, second);
    Check(first.Result.Accepted != second.Result.Accepted, "one buyer wins");
    var winner = first.Result.Accepted ? "chr_b" : "chr_c"; var loser = winner == "chr_b" ? "chr_c" : "chr_b";
    var lost = first.Result.Accepted ? second.Result : first.Result;
    Check(lost.Code == "listing_unavailable" && ReferenceEquals(lost, x.Buy(lost.OperationId, "lst_one", loser,
        "ctn_" + loser + "_bag", 10000, now)), "stable loser receipt");
    Check(Qty(items, winner, "item/ore") == 2 && Qty(items, loser, "item/ore") == 0, "single item delivery");
    Check(x.Wallet(loser).Balance == 100000 && x.Wallet(winner).Balance == 80000, "loser not charged");
    Check(x.Wallet("chr_a").Balance == 100000 - 200 + 19000, "seller net");
    var sale = x.CurrencyLedger.Where(e => e.OperationId == (first.Result.Accepted ? "buy_b" : "buy_c")).ToArray();
    Check(-sale.Single(e => e.Code == "market_purchase").Delta == sale.Single(e => e.Code == "market_sale").Delta &&
        -sale.Single(e => e.Code == "market_purchase").Delta ==
        (sale.Single(e => e.Code == "market_sale").Delta + sale.Single(e => e.Code == "market_transaction_fee").Delta) -
        sale.Single(e => e.Code == "market_transaction_fee").Delta, "buyer equals seller net plus fee");
    Check(sale.Single(e => e.Code == "market_transaction_fee").FlowClass == "sink", "registered fee sink");
}
{
    var (x, items) = Fixture(); var now = DateTimeOffset.Parse("2026-10-21T12:00:00Z");
    x.List("list2", "lst_two", "chr_a", "ctn_chr_a_bag", "ctn_chr_a_esc", "item/ore", 2, 1000,
        MarketScope.GlobalExchange, now, eligible);
    Check(x.CancelListing("cancel_list", "lst_two", "chr_a").Accepted && Qty(items, "chr_a", "item/ore") == 5, "cancel returns escrow");
    x.List("list3", "lst_three", "chr_a", "ctn_chr_a_bag", "ctn_chr_a_esc", "item/ore", 2, 1000,
        MarketScope.GlobalExchange, now, eligible);
    Check(x.Expire("expire", "lst_three", now.AddHours(48)).Accepted && Qty(items, "chr_a", "item/ore") == 5, "expiry returns escrow");
    Check(x.CurrencyLedger.Any(e => e.Code == "market_expiry_fee" && e.Delta < 0), "expiry sink");
}
Console.WriteLine("Exchange tests passed: atomic trade, retry, concurrent buyers, fees, cancel, expiry.");
