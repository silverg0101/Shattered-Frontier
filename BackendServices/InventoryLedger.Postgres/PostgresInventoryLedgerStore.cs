using System.Text.Json;
using Npgsql;
using ShatteredFrontier.InventoryLedger;

namespace ShatteredFrontier.InventoryLedger.Postgres;

public sealed class LedgerStoreUnavailableException(string message, Exception inner) : Exception(message, inner);

// Schema.sql is installed by the migration identity. Runtime needs DML only.
public sealed class PostgresInventoryLedgerStore(string connectionString) : IInventoryLedgerStore
{
    public T Execute<T>(Func<LedgerState, T> transaction)
    {
        try
        {
            using var connection = new NpgsqlConnection(connectionString);
            connection.Open();
            using var db = connection.BeginTransaction();
            // A transaction-scoped lock serializes the existing whole-state callback across processes.
            // It also makes operation-ID replay win before any stale revision check.
            Command(connection, db, "SELECT pg_advisory_xact_lock(519761, 1)").ExecuteNonQuery();
            var state = Load(connection, db);
            var before = state.Copy();
            var result = transaction(state);
            Save(connection, db, before, state);
            db.Commit();
            return result;
        }
        catch (LedgerRejectedException) { throw; }
        catch (PostgresException ex) when (ex.SqlState.StartsWith("22", StringComparison.Ordinal) || ex.SqlState.StartsWith("23", StringComparison.Ordinal))
        {
            throw new LedgerRejectedException($"Inventory store rejected the operation ({ex.SqlState}).");
        }
        catch (Exception ex) when (ex is NpgsqlException or TimeoutException or System.IO.IOException)
        {
            throw new LedgerStoreUnavailableException("Inventory store unavailable; retry the operation ID.", ex);
        }
    }

    private static NpgsqlCommand Command(NpgsqlConnection connection, NpgsqlTransaction db, string sql) => new(sql, connection, db);

    private static LedgerState Load(NpgsqlConnection connection, NpgsqlTransaction db)
    {
        var state = new LedgerState();
        using (var cmd = Command(connection, db, "SELECT id,owner_kind,owner_id,kind,capacity,revision FROM inventory.containers"))
        using (var reader = cmd.ExecuteReader())
            while (reader.Read())
            {
                var spec = new ContainerSpec(reader.GetString(0), (OwnerKind)reader.GetInt32(1), reader.GetString(2), (ContainerKind)reader.GetInt32(3), reader.GetInt32(4));
                state.Containers.Add(spec.Id, new ContainerRecord(spec) { Revision = reader.GetInt64(5) });
            }
        using (var cmd = Command(connection, db, "SELECT container_id,definition_id,quantity FROM inventory.entries"))
        using (var reader = cmd.ExecuteReader())
            while (reader.Read()) state.Containers[reader.GetString(0)].Quantities.Add(reader.GetString(1), reader.GetInt64(2));
        // ponytail: Load is O(total operations) per call; upgrade to receipt lookup by operation ID plus retention.
        using (var cmd = Command(connection, db, "SELECT operation_id,fingerprint,receipt::text FROM inventory.operation_receipts"))
        using (var reader = cmd.ExecuteReader())
            while (reader.Read())
            {
                var receipt = JsonSerializer.Deserialize<LedgerReceipt>(reader.GetString(2)) ?? throw new InvalidOperationException("Invalid stored receipt.");
                state.Operations.Add(reader.GetString(0), new StoredOperation(reader.GetString(1), receipt));
            }
        return state;
    }

    private static void Save(NpgsqlConnection connection, NpgsqlTransaction db, LedgerState before, LedgerState after)
    {
        foreach (var (id, record) in after.Containers)
        {
            if (!before.Containers.TryGetValue(id, out var old))
            {
                using var insert = Command(connection, db, "INSERT INTO inventory.containers(id,owner_kind,owner_id,kind,capacity,revision) VALUES ($1,$2,$3,$4,$5,$6)");
                insert.Parameters.AddWithValue(id); insert.Parameters.AddWithValue((int)record.Spec.OwnerKind);
                insert.Parameters.AddWithValue(record.Spec.OwnerId); insert.Parameters.AddWithValue((int)record.Spec.Kind);
                insert.Parameters.AddWithValue(record.Spec.Capacity); insert.Parameters.AddWithValue(record.Revision);
                insert.ExecuteNonQuery();
            }
            else if (record.Revision != old.Revision)
            {
                using var update = Command(connection, db, "UPDATE inventory.containers SET revision=$1 WHERE id=$2 AND revision=$3");
                update.Parameters.AddWithValue(record.Revision); update.Parameters.AddWithValue(id); update.Parameters.AddWithValue(old.Revision);
                if (update.ExecuteNonQuery() != 1) throw new LedgerRejectedException("Stale container revision.");
            }
            if (old is null || record.Revision != old.Revision)
            {
                using var clear = Command(connection, db, "DELETE FROM inventory.entries WHERE container_id=$1");
                clear.Parameters.AddWithValue(id); clear.ExecuteNonQuery();
                foreach (var (definitionId, quantity) in record.Quantities)
                {
                    using var entry = Command(connection, db, "INSERT INTO inventory.entries(container_id,definition_id,quantity) VALUES ($1,$2,$3)");
                    entry.Parameters.AddWithValue(id); entry.Parameters.AddWithValue(definitionId); entry.Parameters.AddWithValue(quantity); entry.ExecuteNonQuery();
                }
            }
        }
        foreach (var (id, operation) in after.Operations)
            if (!before.Operations.ContainsKey(id))
            {
                using var receipt = Command(connection, db, "INSERT INTO inventory.operation_receipts(operation_id,fingerprint,receipt) VALUES ($1,$2,$3::jsonb)");
                receipt.Parameters.AddWithValue(id); receipt.Parameters.AddWithValue(operation.Fingerprint);
                receipt.Parameters.AddWithValue(JsonSerializer.Serialize(operation.Receipt)); receipt.ExecuteNonQuery();
            }
    }
}
