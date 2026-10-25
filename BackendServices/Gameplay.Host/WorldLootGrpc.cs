using Grpc.Core;
using System.Text.Json;
using Npgsql;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;
using ShatteredFrontier.Rewards;
using Pb = ShatteredFrontier.Rewards.V1;

internal sealed class WorldLootGateway(PostgresInventoryLedgerStore store,
    IReadOnlyDictionary<string, LootTable> tables, IReadOnlyDictionary<string, CreatureLootRule> creatures,
    string connectionString)
{
    private readonly WorldLoot loot = new(store);
    public Pb.LootSourcesReply Create(Pb.CreateCorpseLootRequest request)
    {
        var eligible = request.EligibleCharacterIds.Distinct(StringComparer.Ordinal).Order(StringComparer.Ordinal).ToArray();
        var originalAdmission = new { request.DefeatEventId, request.CreatureId, request.TableId, request.KillerCharacterId,
            Eligible = eligible, request.Protected, request.Shared, request.RaidGroupId, request.ZoneId, request.XCm, request.YCm, request.ZCm,
            request.MeshPath, request.ScaleX, request.ScaleY, request.ScaleZ, request.Pitch, request.Yaw, request.Roll };
        var bounds = request.MeshBounds is { } b ? new WorldLootMeshBounds(b.OriginXCm, b.OriginYCm, b.OriginZCm, b.ExtentXCm, b.ExtentYCm, b.ExtentZCm) : null;
        // Keep the exact legacy admission fingerprint when the additive descriptor is absent.
        var admission = bounds is null ? JsonSerializer.Serialize(originalAdmission) : JsonSerializer.Serialize(new { Source = originalAdmission, MeshBounds = bounds });
        if (loot.ReplayDefeat(request.DefeatEventId, admission) is { } saved)
        { var replay = new Pb.LootSourcesReply(); replay.Sources.AddRange(saved.Select(v => ToProto(v))); return replay; }
        if (request.Protected || string.IsNullOrWhiteSpace(request.DefeatEventId) ||
            !creatures.TryGetValue(request.CreatureId, out var creature) || creature.TableId != request.TableId ||
            creature.UniqueOrBoss != request.Shared || !tables.TryGetValue(request.TableId, out var table))
            throw new GameplayRejectedException("Invalid corpse loot source.", StatusCode.InvalidArgument);
        if (eligible.Length == 0 || eligible.Length > 25 || eligible.Any(string.IsNullOrWhiteSpace) ||
            !eligible.Contains(request.KillerCharacterId, StringComparer.Ordinal))
            throw new GameplayRejectedException("Invalid corpse eligibility.", StatusCode.InvalidArgument);
        // Refuse historical direct-grant events written before route fencing existed.
        using (var c = new NpgsqlConnection(connectionString))
        {
            c.Open();
            using var old = new NpgsqlCommand("SELECT EXISTS(SELECT 1 FROM inventory.defeat_loot_events WHERE defeat_event_id=$1)", c);
            old.Parameters.AddWithValue(request.DefeatEventId);
            if (old.ExecuteScalar() is true) throw new GameplayRejectedException("Defeat already has a direct payout.", StatusCode.AlreadyExists);
        }
        var seeds = new List<WorldLootSeed>();
        foreach (var recipient in request.Shared ? new[] { "" } : eligible)
        {
            var id = WorldLoot.SourceId(request.DefeatEventId, recipient);
            var lines = request.Shared ? CreatureLootCatalog.RollShared(request.DefeatEventId, table) :
                CreatureLootCatalog.RollPersonal(request.DefeatEventId + ":" + recipient, table);
            var instances = new ContainerRecord(new(id, OwnerKind.System, WorldLoot.WorldOwner, ContainerKind.Escrow, 10000));
            DefeatGearInstances.Add(instances, lines, "corpse:" + id);
            var source = new WorldLootSource(id, request.ZoneId, request.XCm, request.YCm, request.ZCm,
                request.CreatureId, request.MeshPath, false, request.Shared, request.Shared ? eligible : [recipient],
                request.ScaleX, request.ScaleY, request.ScaleZ, request.Pitch, request.Yaw, request.Roll, request.DefeatEventId, bounds);
            seeds.Add(new(source, lines.Select(x => new ItemQuantity(x.DefinitionId, x.Quantity)).ToArray(), instances.Instances.Values.ToArray()));
        }
        // One durable transaction freezes every recipient's roll and eligibility.
        // Replay after a compatible content update returns these saved rolls.
        var reply = new Pb.LootSourcesReply();
        reply.Sources.AddRange(loot.CreateDefeat(request.DefeatEventId, admission, seeds).Select(v => ToProto(v)));
        return reply;
    }
    public Pb.LootSourcesReply List(string zone)
    {
        var reply = new Pb.LootSourcesReply(); reply.Sources.AddRange(loot.List(zone).Select(v => ToProto(v))); return reply;
    }
    public Pb.LootSourceView Open(Pb.OpenLootSourceRequest r) => ToProto(loot.Open(r.SourceId, r.CharacterId));
    public Pb.LootSourceView Take(Pb.TakeLootRequest r)
    {
        var receipt = loot.Take(r.OperationId, r.SourceId, r.CharacterId, r.BackpackId, r.SourceRevision, r.BackpackRevision,
            r.DefinitionId, r.Quantity, r.InstanceId);
        var view = ToProto(loot.Open(r.SourceId, r.CharacterId));
        view.LedgerOperationId = receipt.OperationId;
        view.BackpackRevision = receipt.Revisions.Single(x => x.ContainerId == r.BackpackId).After;
        return view;
    }
    public Pb.LootSourceView Drop(Pb.DropLootRequest r)
    {
        var source = new WorldLootSource(WorldLoot.SourceId(r.OperationId, r.CharacterId), r.ZoneId, r.XCm, r.YCm, r.ZCm,
            "", r.MeshPath, true, false, []);
        var view = ToProto(loot.Drop(r.OperationId, source, r.CharacterId, r.BackpackId, r.BackpackRevision,
            r.DefinitionId, r.Quantity, r.InstanceId));
        view.LedgerOperationId = r.OperationId; view.BackpackRevision = r.BackpackRevision + 1;
        return view;
    }
    private static Pb.LootSourceView ToProto(WorldLootView view)
    {
        var source = view.Source;
        var reply = new Pb.LootSourceView { Source = new Pb.LootSource { Id = source.Id, ZoneId = source.ZoneId,
            XCm = source.X, YCm = source.Y, ZCm = source.Z, CreatureId = source.CreatureId, MeshPath = source.MeshPath,
            Ground = source.Ground, Shared = source.Shared, ScaleX = source.ScaleX, ScaleY = source.ScaleY, ScaleZ = source.ScaleZ,
            Pitch = source.Pitch, Yaw = source.Yaw, Roll = source.Roll, BodyId = source.BodyId }, Revision = view.Container.Revision };
        if (source.MeshBounds is { } b) reply.Source.MeshBounds = new Pb.LootMeshBounds
            { OriginXCm = b.OriginXCm, OriginYCm = b.OriginYCm, OriginZCm = b.OriginZCm, ExtentXCm = b.ExtentXCm, ExtentYCm = b.ExtentYCm, ExtentZCm = b.ExtentZCm };
        reply.Source.EligibleCharacterIds.AddRange(source.EligibleCharacters);
        reply.Items.AddRange(view.Container.Quantities.OrderBy(x => x.Key, StringComparer.Ordinal)
            .Select(x => new Pb.SharedDropItem { DefinitionId = x.Key, Quantity = x.Value }));
        reply.Instances.AddRange((view.Container.Instances ?? []).Select(x => new Pb.LootInstance
            { InstanceId = x.Id, DefinitionId = x.DefinitionId, DurabilityPercent = x.DurabilityPercent }));
        return reply;
    }
}
