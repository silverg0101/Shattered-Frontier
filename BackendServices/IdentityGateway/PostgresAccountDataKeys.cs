using System.Security.Cryptography;
using Npgsql;
using ShatteredFrontier.AccountData;

namespace IdentityGateway;

public sealed record DataKeyResult(ushort Version, byte[] Key);

public sealed class PostgresAccountDataKeys(string connectionString, IKekProvider kek, long rotationLimit = 1L << 32)
{
    private NpgsqlConnection Open()
    {
        var connection = new NpgsqlConnection(connectionString);
        connection.Open();
        return connection;
    }

    public void Create(string accountId, NpgsqlConnection connection, NpgsqlTransaction tx)
    {
        var dek = RandomNumberGenerator.GetBytes(32);
        try
        {
            using var cmd = new NpgsqlCommand("INSERT INTO auth.account_data_keys(account_id,key_version,kek_id,wrapped_dek) VALUES ($1,1,$2,$3)", connection, tx);
            cmd.Parameters.AddWithValue(accountId); cmd.Parameters.AddWithValue(kek.KeyId);
            cmd.Parameters.AddWithValue(WrapDek(dek, kek.KeyId, accountId));
            cmd.ExecuteNonQuery();
            using var pointer = new NpgsqlCommand("UPDATE identity.accounts SET data_key_id=$2 WHERE account_id=$1", connection, tx);
            pointer.Parameters.AddWithValue(accountId); pointer.Parameters.AddWithValue(accountId + ":1");
            pointer.ExecuteNonQuery();
        }
        finally { CryptographicOperations.ZeroMemory(dek); }
    }

    public DataKeyResult GetForRead(string accountId, ushort version, string service, string purpose)
    {
        using var c = Open();
        using var tx = c.BeginTransaction();
        using var cmd = new NpgsqlCommand("SELECT k.kek_id,k.wrapped_dek FROM auth.account_data_keys k JOIN identity.accounts a ON a.account_id=k.account_id WHERE k.account_id=$1 AND k.key_version=$2 AND k.retired_at IS NULL AND a.data_key_id IS NOT NULL AND NOT EXISTS(SELECT 1 FROM auth.erasure_tombstones t WHERE t.account_id=$1) AND NOT EXISTS(SELECT 1 FROM auth.account_data_key_erasure e WHERE e.account_id=$1)", c, tx);
        cmd.Parameters.AddWithValue(accountId); cmd.Parameters.AddWithValue((int)version);
        using var reader = cmd.ExecuteReader();
        if (!reader.Read()) throw new CryptographicException("Data key unavailable.");
        var kekId = reader.GetString(0); var wrapped = reader.GetFieldValue<byte[]>(1);
        reader.Close();
        var key = UnwrapDek(wrapped, kekId, accountId);
        Audit(c, tx, accountId, service, purpose);
        tx.Commit();
        return new(version, key);
    }

    public DataKeyResult GetCurrent(string accountId, string service, string purpose)
    {
        using var c = Open();
        using var cmd = new NpgsqlCommand("SELECT k.key_version FROM identity.accounts a JOIN auth.account_data_keys k ON k.data_key_id=a.data_key_id WHERE a.account_id=$1 AND k.retired_at IS NULL", c);
        cmd.Parameters.AddWithValue(accountId);
        if (cmd.ExecuteScalar() is not int version) throw new CryptographicException("Data key unavailable.");
        return GetForRead(accountId, (ushort)version, service, purpose);
    }

    public DataKeyResult GetForWrite(string accountId, string service, string purpose)
    {
        if (rotationLimit < 1) throw new InvalidOperationException("Rotation limit must be positive.");
        using var c = Open();
        using var tx = c.BeginTransaction();
        using var cmd = new NpgsqlCommand("SELECT k.key_version,k.kek_id,k.wrapped_dek,k.messages_written FROM identity.accounts a JOIN auth.account_data_keys k ON k.data_key_id=a.data_key_id WHERE a.account_id=$1 AND k.retired_at IS NULL AND NOT EXISTS(SELECT 1 FROM auth.erasure_tombstones t WHERE t.account_id=$1) AND NOT EXISTS(SELECT 1 FROM auth.account_data_key_erasure e WHERE e.account_id=$1) FOR UPDATE OF a,k", c, tx);
        cmd.Parameters.AddWithValue(accountId);
        using var reader = cmd.ExecuteReader();
        if (!reader.Read()) throw new CryptographicException("Data key unavailable.");
        var version = (ushort)reader.GetInt32(0); var kekId = reader.GetString(1);
        var wrapped = reader.GetFieldValue<byte[]>(2); var writes = reader.GetInt64(3);
        reader.Close();
        byte[] key;
        if (DataKeyRotation.Due(writes, rotationLimit))
        {
            if (version == ushort.MaxValue) throw new CryptographicException("Key version exhausted.");
            version++;
            key = RandomNumberGenerator.GetBytes(32);
            using var insert = new NpgsqlCommand("INSERT INTO auth.account_data_keys(account_id,key_version,kek_id,wrapped_dek,messages_written) VALUES ($1,$2,$3,$4,1)", c, tx);
            insert.Parameters.AddWithValue(accountId); insert.Parameters.AddWithValue((int)version);
            insert.Parameters.AddWithValue(kek.KeyId);
            insert.Parameters.AddWithValue(WrapDek(key, kek.KeyId, accountId));
            insert.ExecuteNonQuery();
            using var pointer = new NpgsqlCommand("UPDATE identity.accounts SET data_key_id=$2 WHERE account_id=$1", c, tx);
            pointer.Parameters.AddWithValue(accountId); pointer.Parameters.AddWithValue(accountId + ":" + version);
            pointer.ExecuteNonQuery();
        }
        else
        {
            key = UnwrapDek(wrapped, kekId, accountId);
            using var increment = new NpgsqlCommand("UPDATE auth.account_data_keys SET messages_written=messages_written+1 WHERE account_id=$1 AND key_version=$2", c, tx);
            increment.Parameters.AddWithValue(accountId); increment.Parameters.AddWithValue((int)version);
            increment.ExecuteNonQuery();
        }
        Audit(c, tx, accountId, service, purpose);
        tx.Commit();
        return new(version, key);
    }

    public int Rewrap(string oldKekId)
    {
        if (oldKekId == kek.KeyId) throw new InvalidOperationException("Current KEK is already selected.");
        using var c = Open();
        var total = 0;
        while (true)
        {
            using var tx = c.BeginTransaction();
            using var scan = new NpgsqlCommand("SELECT account_id,key_version,wrapped_dek FROM auth.account_data_keys WHERE kek_id=$1 ORDER BY account_id,key_version LIMIT 500 FOR UPDATE", c, tx);
            scan.Parameters.AddWithValue(oldKekId);
            var rows = new List<(string Account, int Version, byte[] Wrapped)>();
            using (var r = scan.ExecuteReader()) while (r.Read()) rows.Add((r.GetString(0), r.GetInt32(1), r.GetFieldValue<byte[]>(2)));
            if (rows.Count == 0) return total;
            foreach (var row in rows)
            {
                var dek = UnwrapDek(row.Wrapped, oldKekId, row.Account);
                try
                {
                    using var update = new NpgsqlCommand("UPDATE auth.account_data_keys SET kek_id=$3,wrapped_dek=$4 WHERE account_id=$1 AND key_version=$2", c, tx);
                    update.Parameters.AddWithValue(row.Account); update.Parameters.AddWithValue(row.Version);
                    update.Parameters.AddWithValue(kek.KeyId);
                    update.Parameters.AddWithValue(WrapDek(dek, kek.KeyId, row.Account));
                    update.ExecuteNonQuery();
                }
                finally { CryptographicOperations.ZeroMemory(dek); }
            }
            tx.Commit();
            total += rows.Count;
        }
    }

    public bool MarkCompromised(string accountId)
    {
        using var c = Open();
        using var cmd = new NpgsqlCommand("UPDATE auth.account_data_keys k SET messages_written=GREATEST(k.messages_written,$2) FROM identity.accounts a WHERE a.account_id=$1 AND k.data_key_id=a.data_key_id AND k.retired_at IS NULL AND NOT EXISTS(SELECT 1 FROM auth.account_data_key_erasure e WHERE e.account_id=$1)", c);
        cmd.Parameters.AddWithValue(accountId); cmd.Parameters.AddWithValue(rotationLimit);
        return cmd.ExecuteNonQuery() == 1;
    }

    public void ScheduleErasure(string accountId, bool auditHold, DateTimeOffset now)
    {
        using var c = Open();
        using var tx = c.BeginTransaction();
        using var cmd = new NpgsqlCommand("INSERT INTO auth.account_data_key_erasure(account_id,erase_after,audit_hold) VALUES ($1,$2,$3) ON CONFLICT(account_id) DO UPDATE SET audit_hold=EXCLUDED.audit_hold,erase_after=CASE WHEN auth.account_data_key_erasure.audit_hold AND NOT EXCLUDED.audit_hold THEN EXCLUDED.erase_after ELSE auth.account_data_key_erasure.erase_after END", c, tx);
        cmd.Parameters.AddWithValue(accountId); cmd.Parameters.AddWithValue(now.AddDays(7)); cmd.Parameters.AddWithValue(auditHold);
        cmd.ExecuteNonQuery();
        if (!auditHold)
        {
            using var retire = new NpgsqlCommand("UPDATE auth.account_data_keys SET retired_at=COALESCE(retired_at,$2) WHERE account_id=$1", c, tx);
            retire.Parameters.AddWithValue(accountId); retire.Parameters.AddWithValue(now); retire.ExecuteNonQuery();
        }
        tx.Commit();
    }

    public int FinalizeErasure(DateTimeOffset now)
    {
        using var c = Open();
        using var tx = c.BeginTransaction();
        var accounts = new List<string>();
        using (var cmd = new NpgsqlCommand("SELECT account_id FROM auth.account_data_key_erasure WHERE erase_after<=$1 AND NOT audit_hold FOR UPDATE", c, tx))
        {
            cmd.Parameters.AddWithValue(now);
            using var r = cmd.ExecuteReader(); while (r.Read()) accounts.Add(r.GetString(0));
        }
        foreach (var account in accounts)
        {
            using var pointer = new NpgsqlCommand("UPDATE identity.accounts SET data_key_id=NULL WHERE account_id=$1", c, tx);
            pointer.Parameters.AddWithValue(account); pointer.ExecuteNonQuery();
            using var erase = new NpgsqlCommand("DELETE FROM auth.account_data_keys WHERE account_id=$1", c, tx);
            erase.Parameters.AddWithValue(account); erase.ExecuteNonQuery();
            using var tombstone = new NpgsqlCommand("INSERT INTO auth.erasure_tombstones(account_id,erased_at) VALUES ($1,$2) ON CONFLICT(account_id) DO NOTHING", c, tx);
            tombstone.Parameters.AddWithValue(account); tombstone.Parameters.AddWithValue(now); tombstone.ExecuteNonQuery();
            using var done = new NpgsqlCommand("DELETE FROM auth.account_data_key_erasure WHERE account_id=$1", c, tx);
            done.Parameters.AddWithValue(account); done.ExecuteNonQuery();
        }
        tx.Commit();
        return accounts.Count;
    }

    public bool IsErased(string accountId)
    {
        using var c = Open();
        using var cmd = new NpgsqlCommand("SELECT EXISTS(SELECT 1 FROM auth.account_data_key_erasure WHERE account_id=$1 UNION ALL SELECT 1 FROM auth.erasure_tombstones WHERE account_id=$1)", c);
        cmd.Parameters.AddWithValue(accountId);
        return (bool)cmd.ExecuteScalar()!;
    }

    // Feed this from the independent, retained tombstone export before a restored database serves traffic.
    public int ReplayTombstones(IEnumerable<string> accountIds, DateTimeOffset erasedAt)
    {
        using var c = Open(); using var tx = c.BeginTransaction();
        var count = 0;
        foreach (var account in accountIds.Distinct(StringComparer.Ordinal))
        {
            if (!account.StartsWith("acct_", StringComparison.Ordinal) || account.Length > 96) throw new ArgumentException("Invalid tombstone ID.");
            using var pointer = new NpgsqlCommand("UPDATE identity.accounts SET data_key_id=NULL WHERE account_id=$1", c, tx);
            pointer.Parameters.AddWithValue(account); pointer.ExecuteNonQuery();
            using var erase = new NpgsqlCommand("DELETE FROM auth.account_data_keys WHERE account_id=$1", c, tx);
            erase.Parameters.AddWithValue(account); erase.ExecuteNonQuery();
            using var tombstone = new NpgsqlCommand("INSERT INTO auth.erasure_tombstones(account_id,erased_at) VALUES ($1,$2) ON CONFLICT(account_id) DO NOTHING", c, tx);
            tombstone.Parameters.AddWithValue(account); tombstone.Parameters.AddWithValue(erasedAt); tombstone.ExecuteNonQuery();
            count++;
        }
        tx.Commit(); return count;
    }

    private static void Audit(NpgsqlConnection c, NpgsqlTransaction tx, string accountId, string service, string purpose)
    {
        using var cmd = new NpgsqlCommand("INSERT INTO auth.data_key_unwraps(account_id,service,purpose) VALUES ($1,$2,$3)", c, tx);
        cmd.Parameters.AddWithValue(accountId); cmd.Parameters.AddWithValue(service); cmd.Parameters.AddWithValue(purpose);
        cmd.ExecuteNonQuery();
    }

    private byte[] WrapDek(byte[] dek, string kekId, string accountId)
    {
        var key = kek.GetKey(kekId);
        try { return AccountDataCrypto.Wrap(dek, key, accountId); }
        finally { CryptographicOperations.ZeroMemory(key); }
    }

    private byte[] UnwrapDek(byte[] wrapped, string kekId, string accountId)
    {
        var key = kek.GetKey(kekId);
        try { return AccountDataCrypto.Unwrap(wrapped, key, accountId); }
        finally { CryptographicOperations.ZeroMemory(key); }
    }
}
