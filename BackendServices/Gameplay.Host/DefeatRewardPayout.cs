using Grpc.Core;
using Npgsql;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;
using ShatteredFrontier.Rewards;

internal sealed record DefeatPayout(string OperationId, bool Granted, long Quantity, long Revision);

// The intent precedes the ledger Grant. A full pack leaves it Pending; a crash after Grant
// replays the ledger receipt and completes the same intent, as quest payout does.
internal sealed class DefeatRewardPayout(PostgresInventoryLedgerStore ledgerStore, LootTable table, string connectionString)
{
    private sealed record Intent(string EventId, string RecipientId, string KillerId, string ContainerId,
        string OperationId, bool KillerProtected, bool BoarProtected, string TableId, int TableVersion,
        long Quantity, string State);

    public DefeatPayout Grant(DefeatEvent defeat, string tableId)
    {
        if (string.IsNullOrWhiteSpace(defeat.Id) || string.IsNullOrWhiteSpace(defeat.RecipientCharacterId) ||
            string.IsNullOrWhiteSpace(defeat.KillerCharacterId) || string.IsNullOrWhiteSpace(defeat.ContainerId) ||
            defeat.ExpectedRevision < 0 || tableId != table.Id)
            throw new GameplayRejectedException("Invalid defeat reward request.", StatusCode.InvalidArgument);
        var operationId = $"defeat:{defeat.Id.Length}:{defeat.Id}:{defeat.RecipientCharacterId}";
        var eligible = BoarEligibility.CanGrant(defeat);
        var quantity = eligible ? BoarRewardService.Roll(defeat.Id + ":" + defeat.RecipientCharacterId, table) : 0;
        using (var connection = new NpgsqlConnection(connectionString))
        {
            connection.Open();
            using (var insert = new NpgsqlCommand("""
                INSERT INTO inventory.defeat_reward_intents
                  (defeat_event_id,recipient_id,killer_id,container_id,operation_id,killer_protected,boar_protected,
                   table_id,table_version,quantity,state)
                VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11) ON CONFLICT DO NOTHING
                """, connection))
            {
                insert.Parameters.AddWithValue(defeat.Id); insert.Parameters.AddWithValue(defeat.RecipientCharacterId);
                insert.Parameters.AddWithValue(defeat.KillerCharacterId); insert.Parameters.AddWithValue(defeat.ContainerId);
                insert.Parameters.AddWithValue(operationId); insert.Parameters.AddWithValue(defeat.KillerInProtectedTown);
                insert.Parameters.AddWithValue(defeat.BoarInProtectedTown); insert.Parameters.AddWithValue(table.Id);
                insert.Parameters.AddWithValue(table.Version); insert.Parameters.AddWithValue((long)quantity);
                insert.Parameters.AddWithValue(eligible ? "Pending" : "Ineligible"); insert.ExecuteNonQuery();
            }
            var saved = Read(connection, defeat.Id, defeat.RecipientCharacterId);
            if (saved.KillerId != defeat.KillerCharacterId || saved.ContainerId != defeat.ContainerId ||
                saved.KillerProtected != defeat.KillerInProtectedTown || saved.BoarProtected != defeat.BoarInProtectedTown ||
                saved.TableId != tableId)
                throw new GameplayRejectedException("Conflicting defeat event ID reuse.", StatusCode.AlreadyExists);
            return Resolve(saved);
        }
    }

    public void RetryPending()
    {
        using var connection = new NpgsqlConnection(connectionString); connection.Open();
        using var cmd = new NpgsqlCommand("""
            SELECT defeat_event_id,recipient_id,killer_id,container_id,operation_id,killer_protected,boar_protected,
                   table_id,table_version,quantity,state
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
        if (intent.State == "Ineligible") return new(intent.OperationId, false, 0, 0);
        var fingerprint = $"defeat-reward-v1|{intent.ContainerId}|{intent.RecipientId}|{intent.Quantity}";
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
            try { total = checked(container.Quantities.Values.Aggregate(0L, (sum, value) => checked(sum + value)) + intent.Quantity); }
            catch (OverflowException) { throw new GameplayRejectedException("Reward quantity overflow.", StatusCode.FailedPrecondition); }
            if (total > container.Spec.Capacity)
                throw new GameplayRejectedException("Reward backpack full; intent remains pending.", StatusCode.FailedPrecondition);
            container.Quantities["item/boar_hide"] = checked(container.Quantities.GetValueOrDefault("item/boar_hide") + intent.Quantity);
            var before = container.Revision++;
            var committed = new LedgerReceipt(intent.OperationId, LedgerAction.Grant,
                [new RevisionChange(intent.ContainerId, before, container.Revision)],
                [new QuantityChange(intent.ContainerId, "item/boar_hide", intent.Quantity)]);
            state.Operations.Add(intent.OperationId, new StoredOperation(fingerprint, committed));
            return committed;
        });
        if (receipt.Action != LedgerAction.Grant || receipt.Changes.Count != 1 ||
            receipt.Changes[0].ContainerId != intent.ContainerId || receipt.Changes[0].DefinitionId != "item/boar_hide" ||
            receipt.Changes[0].Delta != intent.Quantity)
            throw new InvalidOperationException("Defeat reward ledger grant differs from intent.");
        using var connection = new NpgsqlConnection(connectionString); connection.Open();
        using var done = new NpgsqlCommand("UPDATE inventory.defeat_reward_intents SET state='Complete' WHERE defeat_event_id=$1 AND recipient_id=$2", connection);
        done.Parameters.AddWithValue(intent.EventId); done.Parameters.AddWithValue(intent.RecipientId); done.ExecuteNonQuery();
        return new(intent.OperationId, true, intent.Quantity, receipt.Revisions.Single().After);
    }

    private static Intent Read(NpgsqlConnection connection, string eventId, string recipientId)
    {
        using var cmd = new NpgsqlCommand("""
            SELECT defeat_event_id,recipient_id,killer_id,container_id,operation_id,killer_protected,boar_protected,
                   table_id,table_version,quantity,state
            FROM inventory.defeat_reward_intents WHERE defeat_event_id=$1 AND recipient_id=$2
            """, connection);
        cmd.Parameters.AddWithValue(eventId); cmd.Parameters.AddWithValue(recipientId);
        using var row = cmd.ExecuteReader();
        if (!row.Read()) throw new InvalidOperationException("Defeat reward intent missing.");
        return FromRow(row);
    }

    private static Intent FromRow(NpgsqlDataReader row) => new(row.GetString(0), row.GetString(1), row.GetString(2),
        row.GetString(3), row.GetString(4), row.GetBoolean(5), row.GetBoolean(6), row.GetString(7),
        row.GetInt32(8), row.GetInt64(9), row.GetString(10));
}

internal sealed class DefeatRewardRecovery(DefeatRewardPayout payout, ILogger<DefeatRewardRecovery> log) : BackgroundService
{
    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        while (!stoppingToken.IsCancellationRequested)
        {
            try { payout.RetryPending(); }
            catch (Exception ex) { log.LogWarning(ex, "Defeat reward recovery will retry"); }
            await Task.Delay(TimeSpan.FromSeconds(5), stoppingToken);
        }
    }
}
