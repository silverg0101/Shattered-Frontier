using System.Text.Json;
using Npgsql;
using ShatteredFrontier.InventoryLedger;

namespace ShatteredFrontier.InventoryLedger.Postgres;

public sealed class LedgerStoreUnavailableException(string message, Exception inner) : Exception(message, inner);
public sealed class LedgerSchemaCompatibilityException(string message) : Exception(message);

// Schema.sql is installed by the migration identity. Runtime needs DML only.
public sealed class PostgresInventoryLedgerStore(string connectionString) : IInventoryLedgerStore
{
    public IReadOnlyList<ContainerSpec> FindContainers(OwnerKind ownerKind, string ownerId, ContainerKind kind)
    {
        try
        {
            using var connection = new NpgsqlConnection(connectionString);
            connection.Open();
            using var db = connection.BeginTransaction();
            CheckSchema(connection, db);
            using var cmd = Command(connection, db, "SELECT id,capacity FROM inventory.containers WHERE owner_kind=$1 AND owner_id=$2 AND kind=$3 ORDER BY id");
            cmd.Parameters.AddWithValue((int)ownerKind);
            cmd.Parameters.AddWithValue(ownerId);
            cmd.Parameters.AddWithValue((int)kind);
            var result = new List<ContainerSpec>();
            using var rows = cmd.ExecuteReader();
            while (rows.Read()) result.Add(new ContainerSpec(rows.GetString(0), ownerKind, ownerId, kind, rows.GetInt32(1)));
            return result;
        }
        catch (Exception ex) when (ex is NpgsqlException or TimeoutException or System.IO.IOException)
        { throw new LedgerStoreUnavailableException("Inventory store unavailable; retry the operation ID.", ex); }
    }

    public T Execute<T>(Func<LedgerState, T> transaction)
    {
        try
        {
            using var connection = new NpgsqlConnection(connectionString);
            connection.Open();
            using var db = connection.BeginTransaction();
            CheckSchema(connection, db);
            var state = Load(connection, db);
            var result = transaction(state);
            Save(connection, db, state);
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

    private static void CheckSchema(NpgsqlConnection connection, NpgsqlTransaction db)
    {
        try
        {
            using var cmd = Command(connection, db, "SELECT version FROM inventory.schema_version WHERE singleton = true FOR SHARE");
            var version = cmd.ExecuteScalar();
            if (version is not int value || value != 1)
                throw new LedgerSchemaCompatibilityException($"Unsupported inventory schema version: {version ?? "missing"}.");
        }
        catch (PostgresException ex) when (ex.SqlState is "42P01" or "3F000" or "42501")
        {
            throw new LedgerSchemaCompatibilityException("Inventory schema marker is missing or inaccessible.");
        }
    }

    private static LedgerState Load(NpgsqlConnection connection, NpgsqlTransaction db)
    {
        ContainerRecord? Container(string id)
        {
            using (var advisory = Command(connection, db, "SELECT pg_advisory_xact_lock(519762, hashtext($1))"))
            { advisory.Parameters.AddWithValue(id); advisory.ExecuteNonQuery(); }
            using var cmd = Command(connection, db, "SELECT id,owner_kind,owner_id,kind,capacity,revision FROM inventory.containers WHERE id=$1 FOR UPDATE");
            cmd.Parameters.AddWithValue(id);
            ContainerRecord record;
            using (var reader = cmd.ExecuteReader())
            {
                if (!reader.Read()) return null;
                var spec = new ContainerSpec(reader.GetString(0), (OwnerKind)reader.GetInt32(1), reader.GetString(2), (ContainerKind)reader.GetInt32(3), reader.GetInt32(4));
                record = new ContainerRecord(spec) { Revision = reader.GetInt64(5) };
            }
            using var entries = Command(connection, db, "SELECT definition_id,quantity FROM inventory.entries WHERE container_id=$1");
            entries.Parameters.AddWithValue(id);
            using var rows = entries.ExecuteReader();
            while (rows.Read()) record.Quantities.Add(rows.GetString(0), rows.GetInt64(1));
            return record;
        }
        IEnumerable<string> AllIds()
        {
            var ids = new List<string>();
            using var cmd = Command(connection, db, "SELECT id FROM inventory.containers ORDER BY id");
            using var rows = cmd.ExecuteReader();
            while (rows.Read()) ids.Add(rows.GetString(0));
            return ids;
        }
        StoredOperation? Operation(string key)
        {
            using (var advisory = Command(connection, db, "SELECT pg_advisory_xact_lock(519761, hashtext($1))"))
            { advisory.Parameters.AddWithValue(key); advisory.ExecuteNonQuery(); }
            using var cmd = Command(connection, db, "SELECT fingerprint,receipt::text FROM inventory.operation_receipts WHERE operation_id=$1");
            cmd.Parameters.AddWithValue(key);
            using var reader = cmd.ExecuteReader();
            if (!reader.Read()) return null;
            var receipt = JsonSerializer.Deserialize<LedgerReceipt>(reader.GetString(1)) ?? throw new InvalidOperationException("Invalid stored receipt.");
            return new StoredOperation(reader.GetString(0), receipt);
        }
        return new LedgerState(Container, AllIds, Operation);
    }

    private static void Save(NpgsqlConnection connection, NpgsqlTransaction db, LedgerState after)
    {
        foreach (var (id, record) in after.Containers.Loaded)
        {
            if (!after.Containers.TryGetOriginal(id, out var old))
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
        foreach (var (id, operation) in after.Operations.Added)
            {
                using var receipt = Command(connection, db, "INSERT INTO inventory.operation_receipts(operation_id,fingerprint,receipt) VALUES ($1,$2,$3::jsonb)");
                receipt.Parameters.AddWithValue(id); receipt.Parameters.AddWithValue(operation.Fingerprint);
                receipt.Parameters.AddWithValue(JsonSerializer.Serialize(operation.Receipt)); receipt.ExecuteNonQuery();
            }
    }
}
