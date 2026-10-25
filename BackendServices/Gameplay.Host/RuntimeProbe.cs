using Grpc.Core;
using Grpc.Net.Client;
using Ledger = ShatteredFrontier.InventoryLedger.V1;
using World = ShatteredFrontier.World.V1;

internal static class RuntimeProbe
{
    public static int Run()
    {
        try
        {
            using var ledger = GrpcChannel.ForAddress("http://127.0.0.1:50071");
            using var gameplay = GrpcChannel.ForAddress("http://127.0.0.1:50072");
            var inventory = new Ledger.InventoryLedger.InventoryLedgerClient(ledger);
            var world = new World.WorldSlots.WorldSlotsClient(gameplay);
            void Expect(Action<Metadata> call, string token, StatusCode expected)
            {
                try { call(new Metadata { { "authorization", "Bearer " + token } }); }
                catch (RpcException error) when (error.StatusCode == expected) { return; }
                throw new InvalidOperationException();
            }
            void ReadLedger(Metadata headers) => inventory.GetContainer(new Ledger.GetContainerRequest { ContainerId = "ctn_runtime_probe_absent" }, headers, DateTime.UtcNow.AddSeconds(3));
            // Invalid command is rejected before changing the session-only NPC slots.
            void ReadWorld(Metadata headers) => world.Advance(new World.AdvanceSlotsRequest { Command = "runtime-probe" }, headers, DateTime.UtcNow.AddSeconds(3));
            Expect(ReadLedger, "deliberately-invalid", StatusCode.Unauthenticated);
            Expect(ReadWorld, "deliberately-invalid", StatusCode.Unauthenticated);
            Expect(ReadLedger, Environment.GetEnvironmentVariable("SF_INVENTORY_LEDGER_TOKEN") ?? "", StatusCode.NotFound);
            Expect(ReadWorld, Environment.GetEnvironmentVariable("SF_GAMEPLAY_TOKEN") ?? "", StatusCode.InvalidArgument);
            Console.WriteLine("PLAYABLE_HOSTS_VERIFIED ledger_read=not_found world=invalid_argument bad_bearers=denied");
            return 0;
        }
        catch (Exception) { Console.Error.WriteLine("PLAYABLE_HOSTS_REFUSED"); return 2; }
    }
}
