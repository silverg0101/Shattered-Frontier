using Grpc.Core;
using Grpc.Net.Client;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;
using Pb = ShatteredFrontier.Housing.V1;
using ProgressPb = ShatteredFrontier.Progression.V1;

// Exercised only by the owned Test-GameplayHosts fixture against its disposable cluster.
internal static class GameplayFixtureClient
{
    public static void Run(string mode)
    {
        static string Required(string key) => Environment.GetEnvironmentVariable(key) ?? throw new Exception($"{key} required");
        using var channel = GrpcChannel.ForAddress($"http://127.0.0.1:{Required("GAMEPLAY_HOST_PORT")}");
        var client = new Pb.Housing.HousingClient(channel);
        var headers = new Metadata { { "authorization", "Bearer " + Required("SF_GAMEPLAY_TOKEN") } };
        var ledger = new InventoryLedger(new PostgresInventoryLedgerStore(Required("GAMEPLAY_LEDGER_CONNECTION")));
        long Revision() => ledger.Read("ctn_housing").Revision;
        long Quantity() => ledger.Read("ctn_housing").Quantities.GetValueOrDefault("item/wall_material_bundle");
        Pb.PlacementReply Place(string op, string structure, long structureRevision, long? containerRevision = null)
        {
            var request = new Pb.PlaceRequest { OperationId=op, ClaimId="clm_test", ActorId="chr_housing",
                ExpectedPermissionRevision=0, ExpectedStructureRevision=structureRevision, StructureId=structure,
                ContentId="piece/wall", MaterialId="item/wall_material_bundle", MaterialQuantity=1,
                ContainerId="ctn_housing", ExpectedContainerRevision=containerRevision ?? Revision(), CellX=checked((int)structureRevision+10) };
            return client.Place(request, headers, deadline: DateTime.UtcNow.AddSeconds(10));
        }
        Pb.PlacementReply Remove(string op, string structure, long structureRevision, long containerRevision) =>
            client.Remove(new Pb.RemoveRequest { OperationId=op, ClaimId="clm_test", ActorId="chr_housing",
                StructureId=structure, ExpectedPermissionRevision=0, ExpectedStructureRevision=structureRevision,
                ExpectedContainerRevision=containerRevision, RemovalKind="undo" },
                headers, deadline: DateTime.UtcNow.AddSeconds(10));
        static void Check(bool ok, string message) { if (!ok) throw new Exception(message); }
        switch (mode)
        {
            case "fixture-fault-seed":
                var placed = Place("place/fault", "str_fault", 2);
                Check(placed.State == "Committed" && placed.StructureRevision == 3, "Fault placement failed.");
                try { Remove("remove/fault", "str_fault", 3, Revision()); throw new Exception("Host fault did not fire."); }
                catch (RpcException) { }
                Console.WriteLine("PASS injected host fault after refund");
                break;
            case "fixture-fault-replay":
                var replay = Remove("remove/fault", "str_fault", 3, 4);
                Check(replay.State == "Removed" && replay.StructureRevision == 4 && Quantity() == 1,
                    "Post-crash refund recovery duplicated or lost a result.");
                Console.WriteLine("PASS post-crash refund replay");
                break;
            case "fixture-stale-concurrent":
                var stalePiece = Place("place/stale", "str_stale", 4);
                var actual = Revision();
                var stale = Remove("remove/stale", "str_stale", stalePiece.StructureRevision, actual-1);
                Check(stale.State == "Compensated" && Quantity() == 0, "Stale refund was not compensated.");
                var corrected = Remove("remove/stale-retry", "str_stale", stalePiece.StructureRevision, actual);
                Check(corrected.State == "Removed" && Quantity() == 1, "New operation could not retry removal.");
                var concurrentPiece = Place("place/concurrent-seed", "str_concurrent_seed", corrected.StructureRevision);
                var before = Revision();
                var gate = new ManualResetEventSlim();
                var placeTask = Task.Run(() => { gate.Wait(); try { return Place("place/concurrent-race", "str_concurrent_race", concurrentPiece.StructureRevision, before).State; } catch (RpcException ex) when (ex.StatusCode == StatusCode.FailedPrecondition) { return "Compensated"; } });
                var removeTask = Task.Run(() => { gate.Wait(); try { return Remove("remove/concurrent-race", "str_concurrent_seed", concurrentPiece.StructureRevision, before).State; } catch (RpcException ex) when (ex.StatusCode == StatusCode.FailedPrecondition) { return "Compensated"; } });
                gate.Set(); Task.WaitAll(placeTask, removeTask);
                var placeWon = placeTask.Result == "Committed";
                var removeWon = removeTask.Result == "Removed";
                Check(placeWon != removeWon && Quantity() == (removeWon ? 1 : 0), "Concurrent place/remove broke conservation.");
                Console.WriteLine("PASS stale revision retry and concurrent place/remove");
                break;
            case "fixture-place-abandon":
                ledger.CreateContainer(new ContainerSpec("ctn_abandon", OwnerKind.Character, "chr_abandon", ContainerKind.Backpack, 100));
                ledger.Grant("seed/abandon", "ctn_abandon", 0, "item/wall_material_bundle", 1, "fixture", "seed/abandon");
                client.ClaimPlot(new Pb.ClaimPlotRequest { OperationId = "claim/abandon", ClaimId = "clm_abandon",
                    OwnerId = "chr_abandon", CenterX = 700, CenterY = 700, SideMetres = 40, PrepaidDays = 1 },
                    headers, deadline: DateTime.UtcNow.AddSeconds(10));
                try
                {
                    client.Place(new Pb.PlaceRequest { OperationId = "place/abandon", ClaimId = "clm_abandon",
                        ActorId = "chr_abandon", StructureId = "str_abandon", ContentId = "piece/wall",
                        MaterialId = "item/wall_material_bundle", MaterialQuantity = 1,
                        ContainerId = "ctn_abandon", ExpectedContainerRevision = 1 },
                        headers, deadline: DateTime.UtcNow.AddSeconds(10));
                    throw new Exception("Placement fault did not fire.");
                }
                catch (RpcException) { }
                Console.WriteLine("PASS abandoned placement reserved");
                break;
            case "fixture-quest-abandon":
                ledger.CreateContainer(new ContainerSpec("ctn_quest_abandon", OwnerKind.Character, "chr_quest_abandon", ContainerKind.Backpack, 100));
                var progress = new ProgressPb.Progression.ProgressionClient(channel);
                var steps = new (string Step, string Kind, string Target, string Node)[] {
                    ("harvest_oak","gather","item/oak_log","gathering_node/oak_tree"),
                    ("mine_iron","gather","item/iron_ore",""),
                    ("refine_materials","craft","item/rough_timber",""),
                    ("refine_materials","craft","item/iron_ingot",""),
                    ("place_wall","build","structure_tag/wall",""),
                    ("defeat_boar","defeat","npc/boar","") };
                for (var i = 0; i < steps.Length; i++)
                {
                    var step = steps[i];
                    var request = new ProgressPb.ApplyReceiptRequest { SourceReceiptId = $"quest-abandon/{i}",
                        Kind = "quest", CharacterId = "chr_quest_abandon",
                        ExpectedQuestRevision = i,
                        Quest = new ProgressPb.QuestEvidence { QuestId = "quest/frontier_founder",
                            StepId = step.Step, Kind = step.Kind, TargetId = step.Target, NodeId = step.Node,
                            Quantity = 1, OccurredAtUnixMs = 1792540800000 + i * 1000 } };
                    try
                    {
                        var result = progress.ApplyReceipt(request, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                        if (i == steps.Length - 1) throw new Exception("Quest fault did not fire.");
                        Check(result.QuestAccepted, "Quest setup step rejected.");
                    }
                    catch (RpcException) when (i == steps.Length - 1) { }
                }
                Console.WriteLine("PASS abandoned reward grant committed");
                break;
            default: throw new ArgumentException("Unknown fixture mode.");
        }
    }
}

