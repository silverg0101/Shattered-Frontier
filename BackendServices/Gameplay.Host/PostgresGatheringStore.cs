using System.Security.Cryptography;
using System.Text.Json;
using Google.Protobuf;
using Grpc.Core;
using Npgsql;
using ShatteredFrontier.GatheringNodes;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;
using ShatteredFrontier.WorldContent;
using Pb = ShatteredFrontier.Gathering.V1;
using ProgressPb = ShatteredFrontier.Progression.V1;

// One locked realm row serializes node transitions across hosts. A source receipt and its immutable
// grant intent commit with the changed node state. The ledger and progression outboxes replay later.
internal sealed class PostgresGatheringStore(string connectionString, ZoneContent zone, GatheringHostTuning tuning,
    NodeSettings lifecycle, PostgresInventoryLedgerStore ledgerStore)
{
    private const string Realm = "zone/greenlands_silverbrook";
    private GatheringNodeService Model() => new(zone, tuning.Definitions(), TimeProvider.System, "silverbrook-gathering", lifecycle);
    private static NpgsqlCommand Cmd(NpgsqlConnection c, NpgsqlTransaction t, string sql) => new(sql, c, t);
    private static void Check(NpgsqlConnection c, NpgsqlTransaction t)
    {
        using var cmd = Cmd(c, t, "SELECT version,migration_id FROM inventory.gathering_schema_version WHERE singleton=true");
        using var row = cmd.ExecuteReader();
        if (!row.Read() || row.GetInt32(0) != 1 || row.GetString(1) != "gathering-nodes-001")
            throw new LedgerSchemaCompatibilityException("Unsupported gathering schema marker.");
    }
    private GatheringNodeService Load(NpgsqlConnection c, NpgsqlTransaction t)
    {
        var model = Model();
        using (var insert = Cmd(c, t, "INSERT INTO inventory.gathering_node_state(realm_id,state) VALUES ($1,$2::jsonb) ON CONFLICT DO NOTHING"))
        { insert.Parameters.AddWithValue(Realm); insert.Parameters.AddWithValue(JsonSerializer.Serialize(model.ExportState())); insert.ExecuteNonQuery(); }
        using var read = Cmd(c, t, "SELECT state::text FROM inventory.gathering_node_state WHERE realm_id=$1 FOR UPDATE");
        read.Parameters.AddWithValue(Realm);
        var saved = (string?)read.ExecuteScalar() ?? throw new InvalidOperationException("Gathering state missing.");
        model.ImportState(JsonSerializer.Deserialize<DurableNodeState[]>(saved) ?? throw new InvalidOperationException("Invalid gathering state."));
        return model;
    }
    private static void Save(NpgsqlConnection c, NpgsqlTransaction t, GatheringNodeService model)
    {
        using var cmd = Cmd(c, t, "UPDATE inventory.gathering_node_state SET state=$1::jsonb WHERE realm_id=$2");
        cmd.Parameters.AddWithValue(JsonSerializer.Serialize(model.ExportState())); cmd.Parameters.AddWithValue(Realm);
        if (cmd.ExecuteNonQuery() != 1) throw new InvalidOperationException("Gathering state disappeared.");
    }
    internal static Pb.NodeReply Wire(NodeSnapshot n) => new()
    {
        NodeId = n.Id, DefinitionId = n.DefinitionId, Generation = n.Generation,
        Charges = n.Charges, MaxCharges = n.MaxCharges,
        RegrowAtUnixMs = n.RegrowAt?.ToUnixTimeMilliseconds() ?? 0,
        GrowthStage = n.GrowthStage ?? "", NextTransitionUnixMs = n.NextTransitionAt?.ToUnixTimeMilliseconds() ?? 0
    };
    public Pb.NodeReply Get(string id)
    {
        using var c = new NpgsqlConnection(connectionString); c.Open();
        using var t = c.BeginTransaction(); Check(c, t);
        var model = Load(c, t);
        var node = model.Snapshot().FirstOrDefault(n => n.Id == id);
        if (node is null) throw new GameplayRejectedException("Unknown gathering node.", StatusCode.NotFound);
        Save(c, t, model); t.Commit(); return Wire(node);
    }
    public Pb.HarvestReply Harvest(Pb.HarvestRequest request)
    {
        if (string.IsNullOrWhiteSpace(request.OperationId) || string.IsNullOrWhiteSpace(request.CharacterId) ||
            string.IsNullOrWhiteSpace(request.NodeId) || request.Generation < 1 || string.IsNullOrWhiteSpace(request.ContainerId) ||
            request.FollowUpVersion != 1 || request.FollowUpPayload.Length is < 1 or > 65536 ||
            request.FollowUpDigest != Convert.ToHexStringLower(SHA256.HashData(request.FollowUpPayload.Span)))
            throw new GameplayRejectedException("Durable harvest identity and follow-up intent required.", StatusCode.InvalidArgument);
        ProgressPb.ApplyReceiptRequest follow;
        try { follow = ProgressPb.ApplyReceiptRequest.Parser.ParseFrom(request.FollowUpPayload); }
        catch (InvalidProtocolBufferException) { throw new GameplayRejectedException("Invalid harvest follow-up payload.", StatusCode.InvalidArgument); }
        if (follow.SourceReceiptId != request.OperationId || follow.CharacterId != request.CharacterId ||
            follow.Kind != "gather" || follow.Payload is null ||
            !follow.Payload.Success || !double.IsFinite(follow.Payload.ActionSeconds) || follow.Payload.ActionSeconds <= 0 ||
            follow.Payload.Difficulty is < 0 or > 1100 ||
            (follow.Quest is not null && (follow.Quest.NodeId.Length == 0 || follow.Quest.OccurredAtUnixMs <= 0)))
            throw new GameplayRejectedException("Harvest follow-up identity differs.", StatusCode.InvalidArgument);
        var backpack = new ShatteredFrontier.InventoryLedger.InventoryLedger(ledgerStore).Read(request.ContainerId);
        if (backpack.Spec.OwnerKind != OwnerKind.Character || backpack.Spec.Kind != ContainerKind.Backpack ||
            backpack.Spec.OwnerId != request.CharacterId)
            throw new GameplayRejectedException("Harvest backpack owner differs.", StatusCode.FailedPrecondition);
        var bytes = request.FollowUpPayload.ToByteArray();
        using var c = new NpgsqlConnection(connectionString); c.Open();
        using var t = c.BeginTransaction(); Check(c, t);
        var model = Load(c, t);
        using (var read = Cmd(c, t, "SELECT character_id,node_id,generation,container_id,payload_version,payload,payload_digest,result,delivered_at FROM inventory.gathering_harvest_receipts WHERE operation_id=$1"))
        {
            read.Parameters.AddWithValue(request.OperationId);
            using var row = read.ExecuteReader();
            if (row.Read())
            {
                if (row.GetString(0) != request.CharacterId || row.GetString(1) != request.NodeId ||
                    row.GetInt32(2) != request.Generation || row.GetString(3) != request.ContainerId ||
                    row.GetInt32(4) != request.FollowUpVersion || !row.GetFieldValue<byte[]>(5).AsSpan().SequenceEqual(bytes) ||
                    row.GetString(6) != request.FollowUpDigest)
                    throw new GameplayRejectedException("Operation ID reused with different harvest.", StatusCode.AlreadyExists);
                var prior = Pb.HarvestReply.Parser.ParseFrom(row.GetFieldValue<byte[]>(7));
                var delivered = !row.IsDBNull(8); row.Close(); t.Commit();
                if (prior.Status == "granted" && !delivered) return Resolve(request.OperationId, prior);
                return prior;
            }
        }
        var result = model.Harvest(new(request.OperationId, request.CharacterId, request.NodeId, request.Generation));
        if (result.Status == HarvestStatus.Granted &&
            new ShatteredFrontier.InventoryLedger.InventoryLedger(ledgerStore).Receipt(request.OperationId) is not null)
            throw new GameplayRejectedException("Harvest operation ID already belongs to a ledger grant.", StatusCode.AlreadyExists);
        var node = model.Snapshot().FirstOrDefault(n => n.Id == request.NodeId);
        if (result.Status == HarvestStatus.Granted && follow.Payload.SkillId !=
            (node!.Kind == ResourceKind.Wood ? "skill/logging" : "skill/mining"))
            throw new GameplayRejectedException("Harvest follow-up skill differs from node.", StatusCode.InvalidArgument);
        var reply = new Pb.HarvestReply
        {
            Status = result.Status switch { HarvestStatus.Granted => "granted", HarvestStatus.Depleted => "depleted",
                HarvestStatus.StaleGeneration => "stale_generation", _ => "unknown_node" },
            ResourceId = result.ResourceId ?? "", Quantity = result.Quantity,
            Node = node is null ? null : Wire(node)
        };
        if (result.Status == HarvestStatus.Granted && follow.Quest is not null &&
            (follow.Quest.Kind != "gather" || follow.Quest.NodeId != node!.DefinitionId ||
                follow.Quest.TargetId != result.ResourceId || follow.Quest.Quantity != result.Quantity))
            throw new GameplayRejectedException("Harvest follow-up evidence differs from node yield.", StatusCode.InvalidArgument);
        Save(c, t, model);
        using (var insert = Cmd(c, t, """
            INSERT INTO inventory.gathering_harvest_receipts
              (operation_id,character_id,node_id,generation,container_id,payload_version,payload,payload_digest,result,resource_id,quantity)
            VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11)
            """))
        {
            insert.Parameters.AddWithValue(request.OperationId); insert.Parameters.AddWithValue(request.CharacterId);
            insert.Parameters.AddWithValue(request.NodeId); insert.Parameters.AddWithValue(request.Generation);
            insert.Parameters.AddWithValue(request.ContainerId); insert.Parameters.AddWithValue(request.FollowUpVersion);
            insert.Parameters.AddWithValue(bytes); insert.Parameters.AddWithValue(request.FollowUpDigest);
            insert.Parameters.AddWithValue(reply.ToByteArray());
            insert.Parameters.AddWithValue((object?)result.ResourceId ?? DBNull.Value); insert.Parameters.AddWithValue(result.Quantity);
            insert.ExecuteNonQuery();
        }
        t.Commit();
        if (Environment.GetEnvironmentVariable("GAMEPLAY_FIXTURE_MODE") == "gathering" &&
            Environment.GetEnvironmentVariable("SF_GAMEPLAY_FAULT_AFTER_GATHER_INTENT") == request.OperationId)
            Environment.FailFast("Injected fault after durable harvest intent, before ledger grant.");
        return result.Status == HarvestStatus.Granted ? Resolve(request.OperationId, reply) : reply;
    }
    private Pb.HarvestReply Resolve(string operationId, Pb.HarvestReply reply)
    {
        try { Deliver(operationId); return reply; }
        catch (Exception ex) when (ex is LedgerRejectedException or LedgerStoreUnavailableException or NpgsqlException)
        { var pending = reply.Clone(); pending.Status = "pending"; return pending; }
    }
    public void RetryPending()
    {
        using var c = new NpgsqlConnection(connectionString); c.Open();
        using var t = c.BeginTransaction(); Check(c, t);
        using var cmd = Cmd(c, t, "SELECT operation_id FROM inventory.gathering_harvest_receipts WHERE resource_id IS NOT NULL AND delivered_at IS NULL ORDER BY created_at,operation_id LIMIT 32");
        var ids = new List<string>(); using (var rows = cmd.ExecuteReader()) while (rows.Read()) ids.Add(rows.GetString(0));
        t.Commit();
        foreach (var id in ids)
        {
            try { Deliver(id); }
            catch (Exception ex) when (ex is LedgerRejectedException or LedgerStoreUnavailableException or NpgsqlException)
            { /* durable intent remains pending; retry after capacity or store recovery */ }
        }
    }
    private void Deliver(string operationId)
    {
        string character, container, resource, digest; byte[] payload; int quantity, version;
        using (var c = new NpgsqlConnection(connectionString))
        {
            c.Open(); using var t = c.BeginTransaction(); Check(c, t);
            using var cmd = Cmd(c, t, "SELECT character_id,container_id,resource_id,quantity,payload_version,payload,payload_digest,delivered_at FROM inventory.gathering_harvest_receipts WHERE operation_id=$1");
            cmd.Parameters.AddWithValue(operationId);
            using var row = cmd.ExecuteReader();
            if (!row.Read() || row.IsDBNull(2) || !row.IsDBNull(7)) return;
            character = row.GetString(0); container = row.GetString(1); resource = row.GetString(2);
            quantity = row.GetInt32(3); version = row.GetInt32(4); payload = row.GetFieldValue<byte[]>(5); digest = row.GetString(6);
        }
        var ledger = new ShatteredFrontier.InventoryLedger.InventoryLedger(ledgerStore);
        var existing = ledger.Receipt(operationId);
        if (existing is null)
        {
            var view = ledger.Read(container);
            if (view.Spec.OwnerKind != OwnerKind.Character || view.Spec.OwnerId != character || view.Spec.Kind != ContainerKind.Backpack)
                throw new GameplayRejectedException("Harvest backpack owner differs.", StatusCode.FailedPrecondition);
            existing = ledgerStore.WithGrantIntent(operationId,
                new(operationId, character, version, payload, digest),
                () => ledger.Grant(operationId, container, view.Revision, resource, quantity, "gathering", operationId));
        }
        if (existing.Action != LedgerAction.Grant || existing.Changes.Count != 1 ||
            existing.Changes[0].ContainerId != container || existing.Changes[0].DefinitionId != resource ||
            existing.Changes[0].Delta != quantity)
            throw new GameplayRejectedException("Harvest ledger receipt conflicts with intent.", StatusCode.AlreadyExists);
        using (var c = new NpgsqlConnection(connectionString))
        {
            c.Open();
            using var cmd = new NpgsqlCommand("SELECT character_id,payload_version,payload,payload_digest FROM inventory.receipt_outbox WHERE source_operation_id=$1 AND destination='progression'", c);
            cmd.Parameters.AddWithValue(operationId);
            using var row = cmd.ExecuteReader();
            if (!row.Read() || row.GetString(0) != character || row.GetInt32(1) != version ||
                !row.GetFieldValue<byte[]>(2).AsSpan().SequenceEqual(payload) || row.GetString(3) != digest)
                throw new GameplayRejectedException("Harvest ledger follow-up intent conflicts.", StatusCode.AlreadyExists);
        }
        if (Environment.GetEnvironmentVariable("GAMEPLAY_FIXTURE_MODE") == "gathering" &&
            Environment.GetEnvironmentVariable("SF_GAMEPLAY_FAULT_AFTER_GATHER_GRANT") == operationId)
            Environment.FailFast("Injected fault after ledger grant, before marking gathering intent delivered.");
        using var doneConnection = new NpgsqlConnection(connectionString); doneConnection.Open();
        using var done = new NpgsqlCommand("UPDATE inventory.gathering_harvest_receipts SET delivered_at=now() WHERE operation_id=$1 AND delivered_at IS NULL", doneConnection);
        done.Parameters.AddWithValue(operationId); done.ExecuteNonQuery();
    }
}

internal sealed class GatheringGrantRecovery(PostgresGatheringStore store, ILogger<GatheringGrantRecovery> log) : BackgroundService
{
    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        using var timer = new PeriodicTimer(TimeSpan.FromSeconds(2));
        do
        {
            try { store.RetryPending(); }
            catch (Exception ex) when (ex is not OperationCanceledException) { log.LogWarning(ex, "Gathering grant recovery will retry"); }
        } while (await timer.WaitForNextTickAsync(stoppingToken));
    }
}
