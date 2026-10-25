using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;

internal static class WorldLootDurableChecks
{
    private static readonly string GroundId = WorldLoot.SourceId("durable-drop", "chr_loot_a");
    private static readonly WorldLootSource Ground = new(GroundId, "zone/loot_fixture", 10, 20, 30, "", "/Game/WorldMesh", true, false, []);
    public static void Seed(string connection)
    {
        var store = new PostgresInventoryLedgerStore(connection); var ledger = new InventoryLedger(store); var loot = new WorldLoot(store);
        ledger.CreateContainer(new("ctn_loot_backpack_a", OwnerKind.Character, "chr_loot_a", ContainerKind.Backpack, 3));
        ledger.CreateContainer(new("ctn_loot_backpack_b", OwnerKind.Character, "chr_loot_b", ContainerKind.Backpack, 1));
        var source = new WorldLootSource(WorldLoot.SourceId("durable-kill", "chr_loot_a"), "zone/loot_fixture", 1, 2, 3,
            "npc/wolf", "/Game/Body", false, false, ["chr_loot_a"], BodyId: "opaque_durable_defeat");
        loot.Create(source, [new("item/hide_cap", 1), new("item/pelt", 3)], [new("itm_loot_exact", "item/hide_cap", 37)]);
        loot.Take("durable-take", source.Id, "chr_loot_a", "ctn_loot_backpack_a", 1, 0, "item/hide_cap", 1, "itm_loot_exact");
        loot.Drop("durable-drop", Ground, "chr_loot_a", "ctn_loot_backpack_a", 1, "item/hide_cap", 1, "itm_loot_exact");
        Console.WriteLine("WORLD_LOOT_DURABLE_SEEDED");
    }
    public static void Recover(string connection)
    {
        var store = new PostgresInventoryLedgerStore(connection); var ledger = new InventoryLedger(store); var loot = new WorldLoot(store);
        var view = loot.List("zone/loot_fixture").Single(v => v.Source.Ground);
        Check(view.Source.Id == Ground.Id && view.Source.ZoneId == Ground.ZoneId && view.Source.X == 10 && view.Source.Y == 20 && view.Source.Z == 30 &&
            view.Source.MeshPath == Ground.MeshPath && view.Container.Instances!.Single() == new ItemInstance("itm_loot_exact", "item/hide_cap", 37), "source metadata and exact instance condition survive database restart");
        loot.Drop("durable-drop", Ground, "chr_loot_a", "ctn_loot_backpack_a", 1, "item/hide_cap", 1, "itm_loot_exact");
        Check(ledger.Read("ctn_loot_backpack_a").Quantities.Count == 0, "drop receipt replays across database restart");
        var winners = 0;
        Parallel.ForEach(new[] { ("chr_loot_a", "ctn_loot_backpack_a", 2L), ("chr_loot_b", "ctn_loot_backpack_b", 0L) }, actor =>
        {
            try { new WorldLoot(new PostgresInventoryLedgerStore(connection)).Take("durable-pick:" + actor.Item1, GroundId, actor.Item1,
                actor.Item2, 1, actor.Item3, "item/hide_cap", 1, "itm_loot_exact"); Interlocked.Increment(ref winners); }
            catch (LedgerRejectedException) { }
        });
        Check(winners == 1 && ledger.Read("ctn_loot_backpack_a").Instances!.Count + ledger.Read("ctn_loot_backpack_b").Instances!.Count == 1,
            "independent concurrent database transactions have one pickup winner and conserve exact copy");
        var corpse = WorldLoot.SourceId("durable-kill", "chr_loot_a");
        Check(loot.Open(corpse, "chr_loot_a").Source.BodyId == "opaque_durable_defeat", "opaque body identity survives restart");
        Reject(() => loot.Open(corpse, "chr_loot_b"));
        Reject(() => loot.Take("durable-full", corpse, "chr_loot_a", "ctn_loot_backpack_b", 2,
            ledger.Read("ctn_loot_backpack_b").Revision, "item/pelt", 3, ""));
        var bag = ledger.Read("ctn_loot_backpack_a");
        ledger.Grant("durable-fill", bag.Spec.Id, bag.Revision, "item/ore", 3 - bag.Quantities.Values.Sum(), "gathering_output", "fixture-fill");
        Reject(() => loot.Take("durable-capacity", corpse, "chr_loot_a", bag.Spec.Id, 2,
            ledger.Read(bag.Spec.Id).Revision, "item/pelt", 1, ""));
        Check(loot.Open(corpse, "chr_loot_a").Container.Quantities["item/pelt"] == 3, "denial commits no source debit");
        Console.WriteLine("WORLD_LOOT_DURABLE_PASS");
    }
    private static void Check(bool ok, string name) { if (!ok) throw new Exception(name); Console.WriteLine("PASS " + name); }
    private static void Reject(Action action) { try { action(); } catch (LedgerRejectedException) { return; } throw new Exception("Expected denial"); }
}
