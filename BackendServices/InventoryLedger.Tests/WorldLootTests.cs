using ShatteredFrontier.InventoryLedger;

internal static class WorldLootTests
{
    public static void Run()
    {
        var store = new InMemoryInventoryLedgerStore();
        var ledger = new InventoryLedger(store);
        var loot = new WorldLoot(store);
        ledger.CreateContainer(new("ctn_a", OwnerKind.Character, "chr_a", ContainerKind.Backpack, 3));
        ledger.CreateContainer(new("ctn_b", OwnerKind.Character, "chr_b", ContainerKind.Backpack, 3));
        var source = new WorldLootSource(WorldLoot.SourceId("kill1", "chr_a"), "zone/test", 1, 2, 3, "npc/wolf", "", false, false, ["chr_a"]);
        var copy = new ItemInstance("itm_exact", "item/blade", 47);
        loot.Create(source, [new("item/blade", 1), new("item/pelt", 3)], [copy]);
        Reject(() => loot.Open(source.Id, "chr_b"));
        Reject(() => loot.Take("foreign", source.Id, "chr_a", "ctn_b", 1, 0, "item/pelt", 1, ""));
        var receipt = loot.Take("take1", source.Id, "chr_a", "ctn_a", 1, 0, "item/blade", 1, copy.Id);
        var retry = loot.Take("take1", source.Id, "chr_a", "ctn_a", 1, 0, "item/blade", 1, copy.Id);
        Check(receipt == retry && ledger.Read("ctn_a").Instances!.Single() == copy, "exact instance and receipt replay");
        Reject(() => loot.Take("full", source.Id, "chr_a", "ctn_a", 2, 1, "item/pelt", 3, ""));
        Check(loot.Open(source.Id, "chr_a").Container.Quantities["item/pelt"] == 3 && ledger.Read("ctn_a").Revision == 1, "capacity rollback");
        var ground = new WorldLootSource(WorldLoot.SourceId("drop1", "chr_a"), "zone/test", 1, 2, 3, "", "/Game/TestMesh", true, false, []);
        loot.Drop("drop1", ground, "chr_a", "ctn_a", 1, "item/blade", 1, copy.Id);
        loot.Drop("drop1", ground, "chr_a", "ctn_a", 1, "item/blade", 1, copy.Id);
        Check(ledger.Read("ctn_a").Quantities.Count == 0, "drop replay no duplicate debit");
        // A new service object over the same durable abstraction recovers its source,
        // position, exact condition and receipt without an in-process source registry.
        var recovered = new WorldLoot(store);
        Check(recovered.List("zone/test").Single(v => v.Source.Ground).Container.Instances!.Single() == copy, "source recovery");
        var winners = 0;
        Parallel.ForEach(new[] { ("chr_a", "ctn_a", 2L), ("chr_b", "ctn_b", 0L) }, actor =>
        {
            try { recovered.Take("pick_" + actor.Item1, ground.Id, actor.Item1, actor.Item2, 1, actor.Item3, "item/blade", 1, copy.Id); Interlocked.Increment(ref winners); }
            catch (LedgerRejectedException) { }
        });
        Check(winners == 1 && ledger.Read("ctn_a").Instances!.Count + ledger.Read("ctn_b").Instances!.Count == 1, "concurrent pickup one winner");
        var shared = source with { Id = WorldLoot.SourceId("shared", ""), Shared = true, EligibleCharacters = ["chr_a", "chr_b"] };
        loot.Create(shared, [new("item/pelt", 2)], []);
        loot.Take("shared1", shared.Id, "chr_a", "ctn_a", 1, ledger.Read("ctn_a").Revision, "item/pelt", 1, "");
        Reject(() => loot.Take("shared2", shared.Id, "chr_b", "ctn_b", 2, ledger.Read("ctn_b").Revision, "item/pelt", 1, ""));
        WorldLoot.ReserveDefeatRoute(store, "kill-fence", "corpse");
        Reject(() => WorldLoot.ReserveDefeatRoute(store, "kill-fence", "direct"));
        var seedA = source with { Id = WorldLoot.SourceId("batch", "chr_a") };
        var seedB = source with { Id = WorldLoot.SourceId("batch", "chr_b"), EligibleCharacters = ["chr_b"] };
        var batch = loot.CreateDefeat("batch", "immutable-eligible-a-b", [new(seedA,[new("item/pelt",1)],[]),new(seedB,[new("item/pelt",2)],[])]);
        var replayBatch = loot.CreateDefeat("batch", "immutable-eligible-a-b", []);
        Check(batch.Count == 2 && replayBatch.Count == 2, "all recipient rolls recover without rerolling new content");
        Reject(() => loot.CreateDefeat("batch", "changed-eligibility", []));
        var bounds = new WorldLootMeshBounds(10, -5, 2, 50, 40, 30);
        var described = source with { Id = WorldLoot.SourceId("metadata", "chr_a"), ScaleX = 2, ScaleY = .5, ScaleZ = 1.5, Yaw = 37, MeshBounds = bounds };
        loot.Create(described, [new("item/pelt", 1)], []);
        var recoveredBody = new WorldLoot(store).Open(described.Id, "chr_a").Source;
        Check(recoveredBody.MeshBounds == bounds && recoveredBody.MeshPath == described.MeshPath && recoveredBody.ScaleX == 2 &&
            recoveredBody.ScaleY == .5 && recoveredBody.ScaleZ == 1.5 && recoveredBody.Yaw == 37 &&
            recoveredBody.EligibleCharacters.SequenceEqual(described.EligibleCharacters), "immutable local bounds and transform recover without live mesh or content lookup");
        Reject(() => loot.Create(described with { MeshBounds = bounds with { ExtentXCm = 49 } }, [new("item/pelt", 1)], []));
        Reject(() => loot.Create(described with { Id = WorldLoot.SourceId("bad-bounds", "chr_a"), MeshBounds = bounds with { ExtentXCm = 0 } }, [], []));
        Check(!System.Text.Json.JsonSerializer.Serialize(source).Contains("MeshBounds", StringComparison.Ordinal), "absent additive bounds preserve historical source fingerprints");
        ledger.CreateContainer(new("ctn_geometry", OwnerKind.Character, "chr_geometry", ContainerKind.Backpack, 10));
        ledger.Grant("geometry-seed", "ctn_geometry", 0, "item/pelt", 2, "fixture", "geometry");
        var groundGeometry = ground with { Id = WorldLoot.SourceId("geometry-drop", "chr_geometry"), X = 1000, Y = 2000, Z = 300,
            ScaleX = 2, ScaleY = .5, ScaleZ = 1.5, Pitch = 15, Yaw = 37, Roll = 5, MeshBounds = new(100,-20,5,120,70,10) };
        var invalidGeometry = new[] {
            groundGeometry with { MeshBounds = groundGeometry.MeshBounds! with { ExtentXCm = double.NaN } },
            groundGeometry with { ScaleX = double.MaxValue },
            groundGeometry with { X = WorldLoot.MaximumWorldCoordinateCm },
            groundGeometry with { MeshBounds = new(0,0,0,0,0,0) }
        };
        for (var index = 0; index < invalidGeometry.Length; ++index)
        {
            Reject(() => loot.Drop("invalid-geometry-"+index, invalidGeometry[index], "chr_geometry", "ctn_geometry", 1, "item/pelt", 1, ""));
            Check(ledger.Read("ctn_geometry").Revision == 1 && ledger.Read("ctn_geometry").Quantities["item/pelt"] == 2, "invalid/overflowing geometry rejects before debit");
        }
        loot.Drop("geometry-drop", groundGeometry, "chr_geometry", "ctn_geometry", 1, "item/pelt", 1, "");
        var recoveredGround = new WorldLoot(store).Open(groundGeometry.Id, "chr_geometry").Source;
        Check(recoveredGround.MeshBounds == groundGeometry.MeshBounds && recoveredGround.ScaleX == 2 && recoveredGround.ScaleY == .5 &&
            recoveredGround.ScaleZ == 1.5 && recoveredGround.Pitch == 15 && recoveredGround.Yaw == 37 && recoveredGround.Roll == 5,
            "oversized offcenter nonuniform ground geometry persists without clipping");
        Console.WriteLine("WORLD_LOOT_PASS eligibility ownership capacity-rollback instance-condition replay recovery concurrent-pickup shared-claim route-fence mesh-bounds-recovery");
    }
    private static void Check(bool ok, string message) { if (!ok) throw new Exception(message); }
    private static void Reject(Action action) { try { action(); } catch (LedgerRejectedException) { return; } throw new Exception("Expected ledger rejection"); }
}
