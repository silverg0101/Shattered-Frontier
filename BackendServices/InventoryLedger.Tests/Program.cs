using ShatteredFrontier.InventoryLedger;

if (args.Contains("--world-loot", StringComparer.Ordinal)) { WorldLootTests.Run(); return; }

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
// INT-04 backpack retry rule: a Stale rejection stores no receipt, and the expected revision is fingerprinted, so the
// Unreal backpack retries once under the derived id "<op>:r1" with the refreshed revision; the original id still replays nothing.
Reject(() => ledger.Grant("op_retry", "ctn_a", 3, "item/ore", 1, "gathering_output", "gather_r"), "retry stale first attempt");
var retried = ledger.Grant("op_retry:r1", "ctn_a", 4, "item/ore", 1, "gathering_output", "gather_r");
Check(retried.Revisions[0] == new RevisionChange("ctn_a", 4, 5), "derived retry id commits at the refreshed revision");
Check(ReferenceEquals(retried, ledger.Grant("op_retry:r1", "ctn_a", 4, "item/ore", 1, "gathering_output", "gather_r")), "derived retry id replays");
Reject(() => ledger.Grant("op_retry", "ctn_a", 3, "item/ore", 1, "gathering_output", "gather_r"), "stale original id left no receipt");
Reject(() => ledger.Grant("op_retry:r1", "ctn_a", 5, "item/ore", 1, "gathering_output", "gather_r"), "revision is fingerprinted");
Check(ledger.Read("ctn_a").Quantities["item/ore"] == 1, "exactly one retried grant");

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

// Alpha Starter Kit: once per character, atomic, idempotent across repeated ensures and concurrent first spawns.
var kit = new AlphaStarterKitSettings();
kit.Validate();
Reject(() => { try { (kit with { Items = [new("item/ore", 101)] }).Validate(); } catch (InvalidOperationException) { throw new LedgerRejectedException("bounds"); } }, "kit bounds");
var bag = ledger.CreateContainer(new("ctn_kit", OwnerKind.Character, "chr_kit", ContainerKind.Backpack, 160));
var kitted = kit.GrantOnce(ledger, bag);
Check(kitted.Revision == 1 && kit.Items.All(i => kitted.Quantities[i.DefinitionId] == i.Quantity), "kit granted on first spawn");
ledger.Consume("op_kit_use", "ctn_kit", 1, "item/wall_material_bundle", 1, "construction_material_consumed");
var again = kit.GrantOnce(ledger, ledger.CreateContainer(new("ctn_kit", OwnerKind.Character, "chr_kit", ContainerKind.Backpack, 160)));
Check(again.Revision == 2 && again.Quantities["item/wall_material_bundle"] == 29, "kit not granted twice");
Check(kit.GrantOnce(ledger, ledger.Read("ctn_b")).Revision == ledger.Read("ctn_b").Revision, "kit skips non-backpack containers");
ledger.CreateContainer(new("ctn_kit_race", OwnerKind.Character, "chr_kit_race", ContainerKind.Backpack, 160));
Parallel.For(0, 16, _ => kit.GrantOnce(ledger, ledger.Read("ctn_kit_race")));
Check(ledger.Read("ctn_kit_race").Revision == 1 && ledger.Read("ctn_kit_race").Quantities["item/rough_timber"] == 12, "concurrent first spawns grant one kit");
ledger.CreateContainer(new("ctn_gear_bag", OwnerKind.Character, owner, ContainerKind.Backpack, 8));
ledger.CreateContainer(new("ctn_gear_equipment", OwnerKind.Character, owner, ContainerKind.Equipment, 8));
var gearGrant = ledger.Grant("gear-grant", "ctn_gear_bag", 0, "item/hide_jerkin", 2, "loot", "defeat-1");
Check(gearGrant.InstanceIds?.Count == 2 && gearGrant.InstanceIds.Distinct().Count() == 2, "grant creates two gear identities");
Check(gearGrant.InstanceIds!.SequenceEqual(ledger.Grant("gear-grant", "ctn_gear_bag", 0, "item/hide_jerkin", 2, "loot", "defeat-1").InstanceIds!), "grant replay retains gear identities");
var firstGear = gearGrant.InstanceIds![0]; var secondGear = gearGrant.InstanceIds[1];
var gearMove = ledger.Transfer("gear-equip", "ctn_gear_bag", 1, "ctn_gear_equipment", 0, "item/hide_jerkin", 1, secondGear);
Check(gearMove.InstanceIds!.SequenceEqual([secondGear]) && ledger.Read("ctn_gear_equipment").Instances!.Single().Id == secondGear,
    "equip selects the requested copy");
Check(ledger.Read("ctn_gear_bag").Instances!.Single().Id == firstGear, "other copy remains in the bag");
Reject(() => ledger.Transfer("gear-equip", "ctn_gear_bag", 1, "ctn_gear_equipment", 0, "item/hide_jerkin", 1, firstGear), "selected-copy replay mismatch");
ledger.CreateContainer(new("ctn_gear_outputs", OwnerKind.Character, owner, ContainerKind.Backpack, 8));
var bundle = ledger.GrantBundle("gear-kit", "ctn_gear_outputs", 0,
    [new("item/hide_cap", 1), new("item/ore", 1)], "starter_kit", "gear-kit");
Check(bundle.InstanceIds?.Count == 1 && ledger.Read("ctn_gear_outputs").Instances!.Single().DefinitionId == "item/hide_cap",
    "starter bundle creates an instance only for gear");
var crafted = ledger.Convert("gear-craft", "ctn_gear_outputs", 1, "recipe/jerkin",
    [new("item/ore", 1)], [new("item/hide_jerkin/fine", 1)]);
Check(crafted.InstanceIds?.Count == 1 && ledger.Read("ctn_gear_outputs").Instances!.Any(x => x.DefinitionId == "item/hide_jerkin/fine"),
    "graded craft output creates a gear instance");
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
