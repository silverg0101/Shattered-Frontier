using Grpc.Core;
using Grpc.Net.Client;
using ShatteredFrontier.Housing.Postgres;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;
using Pb = ShatteredFrontier.Housing.V1;

// Owned wire fixture. The check phase runs after an immediate host and PostgreSQL restart.
internal static class HousingGeometryFixture
{
    public static void Run(string mode)
    {
        static string Required(string name) => Environment.GetEnvironmentVariable(name) ?? throw new Exception($"{name} required");
        static void Check(bool condition, string message) { if (!condition) throw new Exception(message); }
        using var channel = GrpcChannel.ForAddress($"http://127.0.0.1:{Required("GAMEPLAY_HOST_PORT")}");
        var client = new Pb.Housing.HousingClient(channel);
        var headers = new Metadata { { "authorization", "Bearer " + Required("SF_GAMEPLAY_TOKEN") } };
        var ledger = new InventoryLedger(new PostgresInventoryLedgerStore(Required("GAMEPLAY_LEDGER_CONNECTION")));
        var store = new PostgresHousingStore(Required("GAMEPLAY_LEDGER_CONNECTION"));
        var list = new Pb.ListStructuresRequest { ClaimId = "clm_test" };
        Pb.ListStructuresReply Listed() => client.ListStructures(list, headers, deadline: DateTime.UtcNow.AddSeconds(10));
        long Quantity() => ledger.Read("ctn_geometry").Quantities.GetValueOrDefault("item/wall_material_bundle");
        if (mode == "fixture-geometry-seed")
        {
            ledger.CreateContainer(new ContainerSpec("ctn_geometry", OwnerKind.Character, "chr_housing", ContainerKind.Backpack, 100));
            ledger.Grant("geometry/material", "ctn_geometry", 0, "item/wall_material_bundle", 1, "fixture", "geometry/material");
            var prior = Listed().StructureRevision;
            var request = new Pb.PlaceRequest { OperationId = "place/geometry", ClaimId = "clm_test", ActorId = "chr_housing",
                ExpectedPermissionRevision = store.Claim("clm_test")!.PermissionRevision, ExpectedStructureRevision = prior,
                StructureId = "str_geometry", CellX = 30, ContentId = "piece/wall",
                MaterialId = "item/wall_material_bundle", MaterialQuantity = 1,
                ContainerId = "ctn_geometry", ExpectedContainerRevision = 1,
                ShapeKey = "bld.cell_half.v1", OrientationIndex = 7 };
            foreach (var (shape, orientation) in new[] { ("", 7), ("bad/key", 7), ("UPPER", 7), ("bld.cell_half.v1", 24), ("bld.cell_half.v1", -1) })
            {
                var invalid = request.Clone(); invalid.ShapeKey = shape; invalid.OrientationIndex = orientation;
                try { client.Place(invalid, headers, deadline: DateTime.UtcNow.AddSeconds(10)); throw new Exception("Invalid geometry accepted."); }
                catch (RpcException ex) when (ex.StatusCode == StatusCode.InvalidArgument) { }
                Check(Quantity() == 1 && Listed().StructureRevision == prior, "Invalid geometry charged or changed structures.");
            }
            var placed = client.Place(request, headers, deadline: DateTime.UtcNow.AddSeconds(10));
            Check(placed.State == "Committed" && placed.ShapeKey == request.ShapeKey && placed.OrientationIndex == 7 && Quantity() == 0,
                "Geometry placement receipt or cost differs.");
            var structure = Listed().Structures.Single(s => s.StructureId == "str_geometry");
            Check(structure.ShapeKey == request.ShapeKey && structure.OrientationIndex == 7, "Geometry list differs.");
            Console.WriteLine("PASS geometry wire rejection, no debit, receipt and list");
        }
        else if (mode == "fixture-geometry-check")
        {
            var structure = Listed().Structures.Single(s => s.StructureId == "str_geometry");
            Check(structure.ShapeKey == "bld.cell_half.v1" && structure.OrientationIndex == 7, "Geometry lost after restart.");
            var receipt = store.Placement("place/geometry")!;
            Check(receipt.ShapeKey == structure.ShapeKey && receipt.OrientationIndex == structure.OrientationIndex && Quantity() == 0,
                "Durable geometry receipt or inventory differs after restart.");
            var old = client.ListStructures(new Pb.ListStructuresRequest { ClaimId = "clm_journey" }, headers, deadline: DateTime.UtcNow.AddSeconds(10))
                .Structures.Single(s => s.StructureId == "str_journey_wall");
            Check(old.ShapeKey == PostgresHousingStore.DefaultShapeKey && old.OrientationIndex == 0,
                "Older client placement did not read as the default shape.");
            Console.WriteLine("PASS geometry survived host and PostgreSQL restart; legacy default read");
        }
        else throw new ArgumentException("Unknown geometry fixture mode.");
    }
}
