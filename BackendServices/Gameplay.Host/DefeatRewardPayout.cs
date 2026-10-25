using Grpc.Core;
using Npgsql;
using NpgsqlTypes;
using System.Text.Json;
using System.Text;
using System.Security.Cryptography;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;
using ShatteredFrontier.Rewards;

internal sealed record DefeatPayout(string OperationId, bool Granted, IReadOnlyList<LootItemLine> Items, long Revision);

internal static class DefeatGearInstances
{
    // The ledger's instance invariant also applies to multi-item defeat grants.
    public static string[] Add(ContainerRecord container, IReadOnlyList<LootItemLine> items, string operationId)
    {
        var ids = new List<string>();
        var ordinals = new Dictionary<string, long>(StringComparer.Ordinal);
        foreach (var item in items.Where(item => GearDefinitions.IsGear(item.DefinitionId)))
            for (long n = 0; n < item.Quantity; ++n)
            {
                var ordinal = ordinals.GetValueOrDefault(item.DefinitionId);
                ordinals[item.DefinitionId] = ordinal + 1;
                var id = "itm_" + Convert.ToHexStringLower(SHA256.HashData(
                    Encoding.UTF8.GetBytes($"{operationId}|{item.DefinitionId}|{ordinal}")))[..32];
                if (!container.Instances.TryAdd(id, new ItemInstance(id, item.DefinitionId, 100)))
                    throw new InvalidOperationException("Duplicate defeat gear instance.");
                ids.Add(id);
            }
        return [.. ids];
    }
}

// The intent precedes the ledger Grant. A full pack leaves it Pending; a crash after Grant
// replays the ledger receipt and completes the same intent, as quest payout does.
internal sealed class DefeatRewardPayout(PostgresInventoryLedgerStore ledgerStore,
    IReadOnlyDictionary<string, LootTable> tables, IReadOnlyDictionary<string, CreatureLootRule> creatures, string connectionString)
{
    private sealed record Intent(string EventId, string RecipientId, string KillerId, string ContainerId,
        string OperationId, bool KillerProtected, bool BoarProtected, string TableId, int TableVersion,
        long Quantity, LootItemLine[] Items, string State);

    public DefeatPayout Grant(DefeatEvent defeat, string creatureId, string tableId)
    {
        if (!creatures.TryGetValue(creatureId, out var creature) || creature.TableId != tableId ||
            !tables.TryGetValue(tableId, out var table) || table.SingleDrop)
            throw new GameplayRejectedException("Creature requires a different loot route.", StatusCode.InvalidArgument);
        if (string.IsNullOrWhiteSpace(defeat.Id) || string.IsNullOrWhiteSpace(defeat.RecipientCharacterId) ||
            string.IsNullOrWhiteSpace(defeat.KillerCharacterId) || string.IsNullOrWhiteSpace(defeat.ContainerId) ||
            defeat.ExpectedRevision < 0)
            throw new GameplayRejectedException("Invalid defeat reward request.", StatusCode.InvalidArgument);
        var operationId = $"defeat:{defeat.Id.Length}:{defeat.Id}:{defeat.RecipientCharacterId}";
        var eligible = BoarEligibility.CanGrant(defeat);
        var items = eligible ? CreatureLootCatalog.RollPersonal(defeat.Id + ":" + defeat.RecipientCharacterId, table).ToArray() : [];
        var quantity = items.Sum(item => item.Quantity);
        using (var connection = new NpgsqlConnection(connectionString))
        {
            connection.Open();
            DefeatLootScope.Reserve(connection, defeat.Id, "Personal");
            using (var insert = new NpgsqlCommand("""
                INSERT INTO inventory.defeat_reward_intents
                  (defeat_event_id,recipient_id,killer_id,container_id,operation_id,killer_protected,boar_protected,
                   table_id,table_version,quantity,item_lines,state)
                VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12) ON CONFLICT DO NOTHING
                """, connection))
            {
                insert.Parameters.AddWithValue(defeat.Id); insert.Parameters.AddWithValue(defeat.RecipientCharacterId);
                insert.Parameters.AddWithValue(defeat.KillerCharacterId); insert.Parameters.AddWithValue(defeat.ContainerId);
                insert.Parameters.AddWithValue(operationId); insert.Parameters.AddWithValue(defeat.KillerInProtectedTown);
                insert.Parameters.AddWithValue(defeat.BoarInProtectedTown); insert.Parameters.AddWithValue(table.Id);
                insert.Parameters.AddWithValue(table.Version); insert.Parameters.AddWithValue(quantity);
                insert.Parameters.AddWithValue(NpgsqlDbType.Jsonb, JsonSerializer.Serialize(items));
                insert.Parameters.AddWithValue(eligible && items.Length > 0 ? "Pending" : "Ineligible"); insert.ExecuteNonQuery();
            }
            var saved = Read(connection, defeat.Id, defeat.RecipientCharacterId);
            if (saved.KillerId != defeat.KillerCharacterId || saved.ContainerId != defeat.ContainerId ||
                saved.KillerProtected != defeat.KillerInProtectedTown || saved.BoarProtected != defeat.BoarInProtectedTown ||
                saved.TableId != tableId || saved.TableVersion != table.Version ||
                !(saved.Items.SequenceEqual(items) || (saved.Items.Length == 0 && saved.Quantity == quantity &&
                    tableId == "loot/greenlands_silverbrook_boar")))
                throw new GameplayRejectedException("Conflicting defeat event ID reuse.", StatusCode.AlreadyExists);
            return Resolve(saved);
        }
    }

    public void RetryPending()
    {
        using var connection = new NpgsqlConnection(connectionString); connection.Open();
        using var cmd = new NpgsqlCommand("""
            SELECT defeat_event_id,recipient_id,killer_id,container_id,operation_id,killer_protected,boar_protected,
                   table_id,table_version,quantity,item_lines,state
            FROM inventory.defeat_reward_intents WHERE state='Pending' ORDER BY defeat_event_id
            """, connection);
        var pending = new List<Intent>();
        using (var rows = cmd.ExecuteReader()) while (rows.Read()) pending.Add(FromRow(rows));
        foreach (var intent in pending)
        {
            try { Resolve(intent); }
            catch (GameplayRejectedException ex) when (ex.Code == StatusCode.FailedPrecondition) { /* room may open later */ }
        }
    }

    private DefeatPayout Resolve(Intent intent)
    {
        if (intent.State == "Ineligible") return new(intent.OperationId, false, [], 0);
        if (intent.Items.Length == 0)
        {
            if (intent.Quantity == 0) return new(intent.OperationId, false, [], 0);
            // Pending boar intents written before item_lines was introduced.
            if (intent.TableId != "loot/greenlands_silverbrook_boar") throw new InvalidOperationException("Missing reward item snapshot.");
            intent = intent with { Items = [new LootItemLine("item/boar_hide", intent.Quantity)] };
        }
        var legacyBoar = intent.Items.Length == 1 && intent.Items[0].DefinitionId == "item/boar_hide" &&
            intent.TableId == "loot/greenlands_silverbrook_boar";
        var fingerprint = legacyBoar ? $"defeat-reward-v1|{intent.ContainerId}|{intent.RecipientId}|{intent.Quantity}" :
            $"defeat-reward-v2|{intent.ContainerId}|{intent.RecipientId}|{JsonSerializer.Serialize(intent.Items)}";
        var receipt = ledgerStore.Execute(state =>
        {
            if (state.Operations.TryGetValue(intent.OperationId, out var prior))
            {
                if (prior.Fingerprint != fingerprint) throw new GameplayRejectedException("Defeat reward receipt conflicts with intent.", StatusCode.AlreadyExists);
                return prior.Receipt;
            }
            if (!state.Containers.TryGetValue(intent.ContainerId, out var container) ||
                container.Spec.OwnerKind != OwnerKind.Character || container.Spec.OwnerId != intent.RecipientId ||
                container.Spec.Kind != ContainerKind.Backpack)
                throw new GameplayRejectedException("Reward backpack missing or owner changed.", StatusCode.FailedPrecondition);
            long total;
            try { total = checked(container.Quantities.Values.Aggregate(0L, (sum, value) => checked(sum + value)) + intent.Items.Aggregate(0L, (sum, item) => checked(sum + item.Quantity))); }
            catch (OverflowException) { throw new GameplayRejectedException("Reward quantity overflow.", StatusCode.FailedPrecondition); }
            if (total > container.Spec.Capacity)
                throw new GameplayRejectedException("Reward backpack full; intent remains pending.", StatusCode.FailedPrecondition);
            foreach (var item in intent.Items)
                container.Quantities[item.DefinitionId] = checked(container.Quantities.GetValueOrDefault(item.DefinitionId) + item.Quantity);
            var instanceIds = DefeatGearInstances.Add(container, intent.Items, intent.OperationId);
            var before = container.Revision++;
            var committed = new LedgerReceipt(intent.OperationId, LedgerAction.Grant,
                [new RevisionChange(intent.ContainerId, before, container.Revision)],
                intent.Items.Select(item => new QuantityChange(intent.ContainerId, item.DefinitionId, item.Quantity)).ToArray(), instanceIds);
            state.Operations.Add(intent.OperationId, new StoredOperation(fingerprint, committed));
            return committed;
        });
        if (receipt.Action != LedgerAction.Grant || receipt.Changes.Count != intent.Items.Length ||
            intent.Items.Any(item => !receipt.Changes.Any(change => change.ContainerId == intent.ContainerId &&
                change.DefinitionId == item.DefinitionId && change.Delta == item.Quantity)))
            throw new InvalidOperationException("Defeat reward ledger grant differs from intent.");
        using var connection = new NpgsqlConnection(connectionString); connection.Open();
        using var done = new NpgsqlCommand("UPDATE inventory.defeat_reward_intents SET state='Complete' WHERE defeat_event_id=$1 AND recipient_id=$2", connection);
        done.Parameters.AddWithValue(intent.EventId); done.Parameters.AddWithValue(intent.RecipientId); done.ExecuteNonQuery();
        return new(intent.OperationId, true, intent.Items, receipt.Revisions.Single().After);
    }

    private static Intent Read(NpgsqlConnection connection, string eventId, string recipientId)
    {
        using var cmd = new NpgsqlCommand("""
            SELECT defeat_event_id,recipient_id,killer_id,container_id,operation_id,killer_protected,boar_protected,
                   table_id,table_version,quantity,item_lines,state
            FROM inventory.defeat_reward_intents WHERE defeat_event_id=$1 AND recipient_id=$2
            """, connection);
        cmd.Parameters.AddWithValue(eventId); cmd.Parameters.AddWithValue(recipientId);
        using var row = cmd.ExecuteReader();
        if (!row.Read()) throw new InvalidOperationException("Defeat reward intent missing.");
        return FromRow(row);
    }

    private static Intent FromRow(NpgsqlDataReader row) => new(row.GetString(0), row.GetString(1), row.GetString(2),
        row.GetString(3), row.GetString(4), row.GetBoolean(5), row.GetBoolean(6), row.GetString(7),
        row.GetInt32(8), row.GetInt64(9), JsonSerializer.Deserialize<LootItemLine[]>(row.GetString(10)) ?? [], row.GetString(11));
}

internal sealed class DefeatRewardRecovery(DefeatRewardPayout payout, SharedDefeatDropPayout shared,
    ILogger<DefeatRewardRecovery> log) : BackgroundService
{
    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        while (!stoppingToken.IsCancellationRequested)
        {
            try { payout.RetryPending(); shared.RetryPending(); }
            catch (Exception ex) { log.LogWarning(ex, "Defeat reward recovery will retry"); }
            await Task.Delay(TimeSpan.FromSeconds(5), stoppingToken);
        }
    }
}

internal sealed record SharedDropResult(string EventId, IReadOnlyList<LootItemLine> Items, string? ClaimantId,
    string? OperationId, long Revision, string RaidGroupId);

internal static class DefeatLootScope
{
    public static void Reserve(NpgsqlConnection connection, string eventId, string scope)
    {
        using (var insert = new NpgsqlCommand("""
            INSERT INTO inventory.defeat_loot_events (defeat_event_id,scope)
            VALUES ($1,$2) ON CONFLICT DO NOTHING
            """, connection))
        { insert.Parameters.AddWithValue(eventId); insert.Parameters.AddWithValue(scope); insert.ExecuteNonQuery(); }
        using var read = new NpgsqlCommand("SELECT scope FROM inventory.defeat_loot_events WHERE defeat_event_id=$1", connection);
        read.Parameters.AddWithValue(eventId);
        if (read.ExecuteScalar() is not string saved || saved != scope)
            throw new GameplayRejectedException("Defeat already assigned a different loot scope.", StatusCode.AlreadyExists);
    }
}

// The same trusted defeat route creates a single immutable group roll; any snapshotted
// eligible character can claim it once. ClaimPending survives host and ledger restarts.
internal sealed class SharedDefeatDropPayout(PostgresInventoryLedgerStore ledgerStore,
    IReadOnlyDictionary<string, LootTable> tables,
    IReadOnlyDictionary<string, CreatureLootRule> creatures, string connectionString)
{
    private sealed record Intent(string EventId, string CreatureId, string TableId, int TableVersion,
        string KillerId, bool KillerProtected, bool CreatureProtected, string[] EligibleIds, LootItemLine[] Items, string RaidGroupId, string? ClaimantId,
        string? ContainerId, string? OperationId, string State);

    public SharedDropResult Create(string eventId, string creatureId, string tableId, string killerId,
        IReadOnlyList<string> eligibleIds, bool killerProtected, bool creatureProtected, string raidGroupId = "")
    {
        if (string.IsNullOrWhiteSpace(eventId) || string.IsNullOrWhiteSpace(killerId) ||
            !creatures.TryGetValue(creatureId, out var creature) || !creature.UniqueOrBoss ||
            creature.TableId != tableId || !tables.TryGetValue(tableId, out var table))
            throw new GameplayRejectedException("Invalid shared defeat.", StatusCode.InvalidArgument);
        var eligible = eligibleIds.Distinct(StringComparer.Ordinal).Order(StringComparer.Ordinal).ToArray();
        if (eligible.Length == 0 || eligible.Any(string.IsNullOrWhiteSpace) ||
            !eligible.Contains(killerId, StringComparer.Ordinal))
            throw new GameplayRejectedException("Invalid eligible group snapshot.", StatusCode.InvalidArgument);
        if (eligible.Length > 25 || (raidGroupId.Length > 0 &&
            !System.Text.RegularExpressions.Regex.IsMatch(raidGroupId, "^rgp_[a-z0-9]+$")))
            throw new GameplayRejectedException("Invalid raid group snapshot.", StatusCode.InvalidArgument);
        LootItemLine[] items = killerProtected || creatureProtected ? [] : CreatureLootCatalog.RollShared(eventId, table).ToArray();
        if (!killerProtected && !creatureProtected && items.Length == 0)
            throw new GameplayRejectedException("Shared table rolled no item.", StatusCode.FailedPrecondition);
        using var connection = new NpgsqlConnection(connectionString); connection.Open();
        DefeatLootScope.Reserve(connection, eventId, "Shared");
        using (var insert = new NpgsqlCommand("""
            INSERT INTO inventory.shared_defeat_drops
              (defeat_event_id,creature_id,table_id,table_version,killer_id,killer_protected,creature_protected,
               eligible_ids,item_lines,state,raid_group_id)
            VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11) ON CONFLICT DO NOTHING
            """, connection))
        {
            insert.Parameters.AddWithValue(eventId); insert.Parameters.AddWithValue(creatureId);
            insert.Parameters.AddWithValue(tableId); insert.Parameters.AddWithValue(table.Version);
            insert.Parameters.AddWithValue(killerId); insert.Parameters.AddWithValue(killerProtected);
            insert.Parameters.AddWithValue(creatureProtected); insert.Parameters.AddWithValue(eligible);
            insert.Parameters.AddWithValue(NpgsqlDbType.Jsonb, JsonSerializer.Serialize(items));
            insert.Parameters.AddWithValue(killerProtected || creatureProtected ? "Ineligible" : "Open");
            insert.Parameters.AddWithValue(raidGroupId);
            insert.ExecuteNonQuery();
        }
        var saved = Read(connection, eventId);
        if (saved.CreatureId != creatureId || saved.TableId != tableId || saved.TableVersion != table.Version ||
            saved.KillerId != killerId || saved.KillerProtected != killerProtected ||
            saved.CreatureProtected != creatureProtected || saved.RaidGroupId != raidGroupId ||
            !saved.EligibleIds.SequenceEqual(eligible) ||
            !saved.Items.SequenceEqual(items))
            throw new GameplayRejectedException("Conflicting shared defeat event reuse.", StatusCode.AlreadyExists);
        return new(eventId, saved.Items, saved.ClaimantId, saved.OperationId, 0, saved.RaidGroupId);
    }

    public SharedDropResult Claim(string eventId, string characterId, string containerId)
    {
        if (string.IsNullOrWhiteSpace(eventId) || string.IsNullOrWhiteSpace(characterId) ||
            string.IsNullOrWhiteSpace(containerId))
            throw new GameplayRejectedException("Invalid shared drop claim.", StatusCode.InvalidArgument);
        var owned = ledgerStore.Execute(state => state.Containers.TryGetValue(containerId, out var container) &&
            container.Spec.OwnerKind == OwnerKind.Character && container.Spec.OwnerId == characterId &&
            container.Spec.Kind == ContainerKind.Backpack);
        if (!owned) throw new GameplayRejectedException("Claim backpack is not owned by character.", StatusCode.PermissionDenied);
        Intent saved;
        using (var connection = new NpgsqlConnection(connectionString))
        {
            connection.Open();
            using var transaction = connection.BeginTransaction();
            saved = Read(connection, eventId, transaction, true);
            if (saved.State == "Ineligible")
                throw new GameplayRejectedException("Protected defeat has no shared drop.", StatusCode.FailedPrecondition);
            if (!saved.EligibleIds.Contains(characterId, StringComparer.Ordinal))
                throw new GameplayRejectedException("Character was not eligible at defeat.", StatusCode.PermissionDenied);
            if (saved.ClaimantId is not null && (saved.ClaimantId != characterId || saved.ContainerId != containerId))
                throw new GameplayRejectedException("Shared drop already claimed.", StatusCode.AlreadyExists);
            if (saved.ClaimantId is null)
            {
                var operationId = $"shared-defeat:{eventId.Length}:{eventId}";
                using var claim = new NpgsqlCommand("""
                    UPDATE inventory.shared_defeat_drops SET claimant_id=$2,container_id=$3,operation_id=$4,state='ClaimPending'
                    WHERE defeat_event_id=$1
                    """, connection, transaction);
                claim.Parameters.AddWithValue(eventId); claim.Parameters.AddWithValue(characterId);
                claim.Parameters.AddWithValue(containerId); claim.Parameters.AddWithValue(operationId);
                claim.ExecuteNonQuery();
                saved = saved with { ClaimantId = characterId, ContainerId = containerId,
                    OperationId = operationId, State = "ClaimPending" };
            }
            transaction.Commit();
        }
        return Resolve(saved);
    }

    public void RetryPending()
    {
        using var connection = new NpgsqlConnection(connectionString); connection.Open();
        using var command = new NpgsqlCommand("SELECT defeat_event_id FROM inventory.shared_defeat_drops WHERE state='ClaimPending' ORDER BY defeat_event_id", connection);
        var ids = new List<string>();
        using (var rows = command.ExecuteReader()) while (rows.Read()) ids.Add(rows.GetString(0));
        foreach (var id in ids)
        {
            try { using var readerConnection = new NpgsqlConnection(connectionString); readerConnection.Open(); Resolve(Read(readerConnection, id)); }
            catch (GameplayRejectedException ex) when (ex.Code == StatusCode.FailedPrecondition) { /* pack may open later */ }
        }
    }

    private SharedDropResult Resolve(Intent intent)
    {
        if (intent.ClaimantId is null || intent.ContainerId is null || intent.OperationId is null)
            throw new InvalidOperationException("Open shared drop cannot be resolved.");
        var fingerprint = $"shared-defeat-v1|{intent.ContainerId}|{intent.ClaimantId}|{JsonSerializer.Serialize(intent.Items)}";
        var receipt = ledgerStore.Execute(state =>
        {
            if (state.Operations.TryGetValue(intent.OperationId, out var prior))
            {
                if (prior.Fingerprint != fingerprint) throw new GameplayRejectedException("Shared drop receipt conflicts with claim.", StatusCode.AlreadyExists);
                return prior.Receipt;
            }
            if (!state.Containers.TryGetValue(intent.ContainerId, out var container) ||
                container.Spec.OwnerKind != OwnerKind.Character || container.Spec.OwnerId != intent.ClaimantId ||
                container.Spec.Kind != ContainerKind.Backpack)
                throw new GameplayRejectedException("Claim backpack missing or owner changed.", StatusCode.FailedPrecondition);
            long total;
            try { total = checked(container.Quantities.Values.Aggregate(0L, (sum, value) => checked(sum + value)) +
                intent.Items.Aggregate(0L, (sum, line) => checked(sum + line.Quantity))); }
            catch (OverflowException) { throw new GameplayRejectedException("Shared drop quantity overflow.", StatusCode.FailedPrecondition); }
            if (total > container.Spec.Capacity)
                throw new GameplayRejectedException("Claim backpack full; drop remains pending.", StatusCode.FailedPrecondition);
            foreach (var line in intent.Items)
                container.Quantities[line.DefinitionId] = checked(container.Quantities.GetValueOrDefault(line.DefinitionId) + line.Quantity);
            var instanceIds = DefeatGearInstances.Add(container, intent.Items, intent.OperationId);
            var before = container.Revision++;
            var committed = new LedgerReceipt(intent.OperationId, LedgerAction.Grant,
                [new RevisionChange(intent.ContainerId, before, container.Revision)],
                intent.Items.Select(line => new QuantityChange(intent.ContainerId, line.DefinitionId, line.Quantity)).ToArray(), instanceIds);
            state.Operations.Add(intent.OperationId, new StoredOperation(fingerprint, committed));
            return committed;
        });
        if (receipt.Action != LedgerAction.Grant || receipt.Changes.Count != intent.Items.Length ||
            intent.Items.Any(line => !receipt.Changes.Any(change => change.ContainerId == intent.ContainerId &&
                change.DefinitionId == line.DefinitionId && change.Delta == line.Quantity)))
            throw new InvalidOperationException("Shared drop ledger receipt differs from claim.");
        using var connection = new NpgsqlConnection(connectionString); connection.Open();
        using var done = new NpgsqlCommand("UPDATE inventory.shared_defeat_drops SET state='Complete' WHERE defeat_event_id=$1", connection);
        done.Parameters.AddWithValue(intent.EventId); done.ExecuteNonQuery();
        return new(intent.EventId, intent.Items, intent.ClaimantId, intent.OperationId, receipt.Revisions.Single().After, intent.RaidGroupId);
    }

    private static Intent Read(NpgsqlConnection connection, string eventId, NpgsqlTransaction? transaction = null, bool lockRow = false)
    {
        using var command = new NpgsqlCommand("""
            SELECT defeat_event_id,creature_id,table_id,table_version,killer_id,killer_protected,creature_protected,
                   eligible_ids,item_lines,claimant_id,container_id,operation_id,state,raid_group_id
            FROM inventory.shared_defeat_drops WHERE defeat_event_id=$1
            """ + (lockRow ? " FOR UPDATE" : ""), connection, transaction);
        command.Parameters.AddWithValue(eventId);
        using var row = command.ExecuteReader();
        if (!row.Read()) throw new GameplayRejectedException("Shared drop not found.", StatusCode.NotFound);
        var items = JsonSerializer.Deserialize<LootItemLine[]>(row.GetString(8))
            ?? throw new InvalidOperationException("Shared drop item snapshot missing.");
        return new(row.GetString(0), row.GetString(1), row.GetString(2), row.GetInt32(3), row.GetString(4),
            row.GetBoolean(5), row.GetBoolean(6), row.GetFieldValue<string[]>(7), items, row.GetString(13),
            row.IsDBNull(9) ? null : row.GetString(9), row.IsDBNull(10) ? null : row.GetString(10),
            row.IsDBNull(11) ? null : row.GetString(11), row.GetString(12));
    }
}
