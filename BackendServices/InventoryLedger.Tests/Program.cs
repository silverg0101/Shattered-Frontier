using ShatteredFrontier.InventoryLedger;

var store = new InMemoryInventoryLedgerStore();
var ledger = new InventoryLedger(store);
var owner = "chr_test";
ledger.CreateContainer(new("ctn_a", OwnerKind.Character, owner, ContainerKind.Backpack, 20));
ledger.CreateContainer(new("ctn_b", OwnerKind.Character, owner, ContainerKind.Bag, 20));
var first = ledger.Grant("op_gather", "ctn_a", 0, "item/ore", 8, "gathering_output", "gather_1");
Check(ReferenceEquals(first, ledger.Grant("op_gather", "ctn_a", 0, "item/ore", 8, "gathering_output", "gather_1")), "retry receipt");
Check(ledger.Read("ctn_a").Quantities["item/ore"] == 8, "no double grant");
Reject(() => ledger.Grant("op_gather", "ctn_a", 0, "item/ore", 9, "gathering_output", "gather_1"), "conflicting operation");
var moved = ledger.Transfer("op_move", "ctn_a", 1, "ctn_b", 0, "item/ore", 3);
Check(moved.Revisions.Count == 2 && moved.Revisions[0] == new RevisionChange("ctn_a", 1, 2), "transfer revisions");
Check(ledger.Read("ctn_a").Quantities["item/ore"] + ledger.Read("ctn_b").Quantities["item/ore"] == 8, "transfer conservation");
var made = ledger.Convert("op_craft", "ctn_a", 2, "recipe/iron_bar",
    [new("item/ore", 5)], [new("item/iron_bar", 2)]);
Check(made.Revisions[0] == new RevisionChange("ctn_a", 2, 3), "convert revisions");
Check(made.Changes.Count == 2 && made.Changes[0].Delta == -5 && made.Changes[1].Delta == 2, "recipe input output accounting");
Check(ledger.Read("ctn_a").Quantities["item/iron_bar"] == 2 && !ledger.Read("ctn_a").Quantities.ContainsKey("item/ore"), "convert committed quantities");
Check(ReferenceEquals(made, ledger.Convert("op_craft", "ctn_a", 2, "recipe/iron_bar", [new("item/ore", 5)], [new("item/iron_bar", 2)])), "craft retry");
Reject(() => ledger.Convert("op_bad", "ctn_b", 1, "recipe/bad", [new("item/ore", 4)], [new("item/bar", 1)]), "insufficient input atomic");
Check(ledger.Read("ctn_b").Revision == 1 && ledger.Read("ctn_b").Quantities["item/ore"] == 3, "failed conversion no mutation");
Reject(() => ledger.Grant("op_full", "ctn_a", 3, "item/ore", 19, "gathering_output", "gather_2"), "capacity rejection");
Check(ledger.Read("ctn_a").Revision == 3 && ledger.Read("ctn_a").Quantities["item/iron_bar"] == 2, "capacity no mutation");
ledger.Consume("op_use", "ctn_a", 3, "item/iron_bar", 1, "crafting_consumed");
Reject(() => ledger.Consume("op_stale", "ctn_a", 3, "item/iron_bar", 1, "crafting_consumed"), "stale revision loser");
Check(ledger.Read("ctn_a").Quantities["item/iron_bar"] == 1, "stale no mutation");

ledger.CreateContainer(new("ctn_parallel", OwnerKind.Character, owner, ContainerKind.Backpack, 20));
var winners = 0;
Parallel.For(0, 64, i =>
{
    try
    {
        ledger.Grant($"op_parallel_{i}", "ctn_parallel", 0, "item/ore", 1, "gathering_output", $"gather_{i}");
        Interlocked.Increment(ref winners);
    }
    catch (LedgerRejectedException) { }
});
Check(winners == 1 && ledger.Read("ctn_parallel").Revision == 1 && ledger.Read("ctn_parallel").Quantities["item/ore"] == 1, "parallel consistent winner");

var second = new InventoryLedger(store);
Check(ReferenceEquals(first, second.Grant("op_gather", "ctn_a", 0, "item/ore", 8, "gathering_output", "gather_1")), "receipt survives service instance");
Console.WriteLine("InventoryLedger checks passed");

static void Check(bool condition, string name)
{
    if (!condition) throw new Exception($"Failed: {name}");
}
static void Reject(Action action, string name)
{
    try { action(); }
    catch (LedgerRejectedException) { return; }
    throw new Exception($"Failed to reject: {name}");
}
