using System.Text.Json;
using System.Security.Cryptography;
using Npgsql;
using ShatteredFrontier.InventoryLedger;

namespace ShatteredFrontier.InventoryLedger.Postgres;

public sealed class LedgerStoreUnavailableException(string message, Exception inner) : Exception(message, inner);
public sealed class LedgerSchemaCompatibilityException(string message) : Exception(message);

// Schema.sql is installed by the migration identity. Runtime needs DML only.
public sealed class PostgresInventoryLedgerStore(string connectionString) : IInventoryLedgerStore
{
    // Once the host-owned harvest migration is installed, the former client-first gathering
    // Grant path must fail before writing an item. Gameplay.Host grants saved harvest intents directly.
    public bool HasGatheringMigration()
    {
        try
        {
            using var connection = new NpgsqlConnection(connectionString); connection.Open();
            using var cmd = new NpgsqlCommand("SELECT to_regclass('inventory.gathering_schema_version') IS NOT NULL", connection);
            return cmd.ExecuteScalar() is true;
        }
        catch (Exception ex) when (ex is NpgsqlException or TimeoutException or System.IO.IOException)
        { throw new LedgerStoreUnavailableException("Inventory store unavailable; retry the operation ID.", ex); }
    }
    private sealed record GrantContext(string OperationId, FollowUpIntent? Intent);
    private static readonly AsyncLocal<GrantContext?> CurrentGrant = new();
    public sealed record FollowUpIntent(string OperationId, string CharacterId, int Version, byte[] Payload, string Digest);
    public sealed record OutboxDelivery(string OperationId, string CharacterId, byte[] Payload, string Digest, int Attempts);
    public (int Pending, int Parked) OutboxCounts()
    {
        using var connection = new NpgsqlConnection(connectionString); connection.Open();
        using var db = connection.BeginTransaction(); CheckSchema(connection, db);
        using var cmd = Command(connection, db, "SELECT count(*) FILTER (WHERE parked_at IS NULL), count(*) FILTER (WHERE parked_at IS NOT NULL) FROM inventory.receipt_outbox WHERE delivered_at IS NULL");
        using var row = cmd.ExecuteReader(); row.Read();
        return (checked((int)row.GetInt64(0)), checked((int)row.GetInt64(1)));
    }
    // Requeue keeps the immutable payload and character order. Only a parked row can be repaired;
    // an in-flight lease or delivered tombstone is never reset by this operator action.
    public void RequeueParkedOutbox(string operationId)
    {
        if (string.IsNullOrWhiteSpace(operationId)) throw new ArgumentException("Operation ID is required.", nameof(operationId));
        using var connection = new NpgsqlConnection(connectionString); connection.Open();
        using var db = connection.BeginTransaction(); CheckSchema(connection, db);
        using var cmd = Command(connection, db, """
            UPDATE inventory.receipt_outbox SET parked_at=NULL,attempts=0,next_attempt_at=now(),
              last_error_code=NULL
            WHERE source_operation_id=$1 AND destination='progression' AND parked_at IS NOT NULL
              AND delivered_at IS NULL AND lease_owner IS NULL AND lease_until IS NULL
            """);
        cmd.Parameters.AddWithValue(operationId);
        if (cmd.ExecuteNonQuery() != 1)
            throw new InvalidOperationException("No parked, unleased outbox row matched the operation ID.");
        db.Commit();
    }
    public IReadOnlyList<OutboxDelivery> ClaimOutbox(string owner, int batchSize, int maxAttempts)
    {
        using var connection = new NpgsqlConnection(connectionString); connection.Open();
        using var db = connection.BeginTransaction(); CheckSchema(connection, db);
        using (var exhausted = Command(connection, db, "UPDATE inventory.receipt_outbox SET parked_at=now(),lease_owner=NULL,lease_until=NULL,last_error_code='ATTEMPTS_EXHAUSTED' WHERE delivered_at IS NULL AND parked_at IS NULL AND attempts >= $1 AND (lease_until IS NULL OR lease_until < now())"))
        { exhausted.Parameters.AddWithValue(maxAttempts); exhausted.ExecuteNonQuery(); }
        using var cmd = Command(connection, db, """
            WITH due AS (
              SELECT o.source_operation_id,o.destination FROM inventory.receipt_outbox o
              WHERE o.delivered_at IS NULL AND o.parked_at IS NULL AND o.attempts < $3 AND o.next_attempt_at <= now()
                AND (o.lease_until IS NULL OR o.lease_until < now())
                AND NOT EXISTS (SELECT 1 FROM inventory.receipt_outbox earlier
                  WHERE earlier.character_id=o.character_id AND earlier.delivered_at IS NULL
                    AND earlier.character_order < o.character_order)
              ORDER BY o.created_at,o.source_operation_id LIMIT $1 FOR UPDATE OF o SKIP LOCKED
            )
            UPDATE inventory.receipt_outbox o SET lease_owner=$2,lease_until=now()+interval '30 seconds',attempts=o.attempts+1
            FROM due WHERE o.source_operation_id=due.source_operation_id AND o.destination=due.destination
            RETURNING o.source_operation_id,o.character_id,o.payload,o.payload_digest,o.attempts
            """);
        cmd.Parameters.AddWithValue(batchSize); cmd.Parameters.AddWithValue(owner); cmd.Parameters.AddWithValue(maxAttempts);
        var deliveries = new List<OutboxDelivery>();
        using (var rows = cmd.ExecuteReader()) while (rows.Read())
            deliveries.Add(new(rows.GetString(0), rows.GetString(1), rows.GetFieldValue<byte[]>(2), rows.GetString(3), rows.GetInt32(4)));
        db.Commit(); return deliveries;
    }
    public void CompleteOutbox(string operationId, string owner)
    {
        using var connection = new NpgsqlConnection(connectionString); connection.Open();
        using var db = connection.BeginTransaction(); CheckSchema(connection, db);
        using var cmd = Command(connection, db, "UPDATE inventory.receipt_outbox SET delivered_at=now(),lease_owner=NULL,lease_until=NULL,last_error_code=NULL WHERE source_operation_id=$1 AND destination='progression' AND lease_owner=$2 AND lease_until > now() AND delivered_at IS NULL");
        cmd.Parameters.AddWithValue(operationId); cmd.Parameters.AddWithValue(owner);
        if (cmd.ExecuteNonQuery() != 1) throw new InvalidOperationException("Outbox lease expired before completion.");
        db.Commit();
    }
    public void FailOutbox(string operationId, string owner, string errorCode, bool permanent, int maxAttempts)
    {
        using var connection = new NpgsqlConnection(connectionString); connection.Open();
        using var db = connection.BeginTransaction(); CheckSchema(connection, db);
        using var cmd = Command(connection, db, """
            UPDATE inventory.receipt_outbox SET lease_owner=NULL,lease_until=NULL,last_error_code=$3,
              parked_at=CASE WHEN $4 OR attempts >= $5 THEN now() ELSE NULL END,
              next_attempt_at=now()+make_interval(secs => least(300, power(2, least(attempts,8))::integer))
                + (random()*interval '1 second')
            WHERE source_operation_id=$1 AND destination='progression' AND lease_owner=$2 AND delivered_at IS NULL
            """);
        cmd.Parameters.AddWithValue(operationId); cmd.Parameters.AddWithValue(owner);
        cmd.Parameters.AddWithValue(errorCode); cmd.Parameters.AddWithValue(permanent); cmd.Parameters.AddWithValue(maxAttempts);
        cmd.ExecuteNonQuery(); db.Commit();
    }
    public T WithGrantIntent<T>(string operationId, FollowUpIntent? intent, Func<T> grant)
    {
        if (CurrentGrant.Value is not null) throw new InvalidOperationException("Nested grant intent.");
        CurrentGrant.Value = new(operationId, intent);
        try { return grant(); }
        finally { CurrentGrant.Value = null; }
    }
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
            Save(connection, db, state, CurrentGrant.Value);
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
            using var cmd = Command(connection, db, "SELECT version,migration_checksum FROM inventory.schema_version WHERE singleton = true FOR SHARE");
            using var row = cmd.ExecuteReader();
            if (!row.Read() || row.GetInt32(0) != 2 || row.GetString(1) != "66ae3e7c362f9062575bc97da4fa5e50d48548e10928b2f74b13f1f12c7bd8fb")
                throw new LedgerSchemaCompatibilityException("Unsupported inventory schema marker.");
            row.Close();
            using var table = Command(connection, db, "SELECT to_regclass('inventory.receipt_outbox') IS NOT NULL");
            if (table.ExecuteScalar() is not true)
                throw new LedgerSchemaCompatibilityException("Inventory receipt outbox is missing.");
        }
        catch (PostgresException ex) when (ex.SqlState is "42P01" or "3F000" or "42501" or "42703")
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

    private static void Save(NpgsqlConnection connection, NpgsqlTransaction db, LedgerState after, GrantContext? grant)
    {
        var intent = grant?.Intent;
        if (grant is not null && intent is null && after.Operations.TryGetValue(grant.OperationId, out _))
        {
            using var present = Command(connection, db, "SELECT 1 FROM inventory.receipt_outbox WHERE source_operation_id=$1 AND destination='progression'");
            present.Parameters.AddWithValue(grant.OperationId);
            if (present.ExecuteScalar() is not null) throw new LedgerRejectedException("Conflicting follow-up intent.");
        }
        if (intent is not null)
        {
            if (!after.Operations.TryGetValue(intent.OperationId, out var operation) || operation.Receipt.Action != LedgerAction.Grant ||
                intent.Version != 1 || intent.Payload.Length is < 1 or > 65536 ||
                !CryptographicOperations.FixedTimeEquals(System.Text.Encoding.ASCII.GetBytes(Convert.ToHexStringLower(SHA256.HashData(intent.Payload))),
                    System.Text.Encoding.ASCII.GetBytes(intent.Digest)))
                throw new LedgerRejectedException("Invalid follow-up intent.");
            using var existing = Command(connection, db, "SELECT character_id,payload_version,payload,payload_digest FROM inventory.receipt_outbox WHERE source_operation_id=$1 AND destination='progression'");
            existing.Parameters.AddWithValue(intent.OperationId);
            using var row = existing.ExecuteReader();
            if (row.Read())
            {
                if (row.GetString(0) != intent.CharacterId || row.GetInt32(1) != intent.Version ||
                    !row.GetFieldValue<byte[]>(2).AsSpan().SequenceEqual(intent.Payload) || row.GetString(3) != intent.Digest)
                    throw new LedgerRejectedException("Conflicting follow-up intent.");
            }
            else if (!after.Operations.Added.Any(x => x.Key == intent.OperationId))
                throw new LedgerRejectedException("Conflicting follow-up intent.");
            row.Close();
        }
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
        if (intent is not null && after.Operations.Added.Any(x => x.Key == intent.OperationId))
        {
            using (var characterLock = Command(connection, db, "SELECT pg_advisory_xact_lock(519763, hashtext($1))"))
            { characterLock.Parameters.AddWithValue(intent.CharacterId); characterLock.ExecuteNonQuery(); }
            using var cmd = Command(connection, db, "INSERT INTO inventory.receipt_outbox(source_operation_id,destination,character_id,character_order,payload_version,payload,payload_digest) SELECT $1,'progression',$2,coalesce(max(character_order),0)+1,$3,$4,$5 FROM inventory.receipt_outbox WHERE character_id=$2");
            cmd.Parameters.AddWithValue(intent.OperationId); cmd.Parameters.AddWithValue(intent.CharacterId);
            cmd.Parameters.AddWithValue(intent.Version); cmd.Parameters.AddWithValue(intent.Payload); cmd.Parameters.AddWithValue(intent.Digest);
            cmd.ExecuteNonQuery();
        }
    }
}
