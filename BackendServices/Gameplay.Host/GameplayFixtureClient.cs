using Grpc.Core;
using Grpc.Net.Client;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;
using Pb = ShatteredFrontier.Housing.V1;

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
            default: throw new ArgumentException("Unknown fixture mode.");
        }
    }
}
