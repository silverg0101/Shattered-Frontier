using Npgsql;
using System.Security.Cryptography;
using System.Text.Json;

namespace IdentityGateway;

public sealed class PostgresIdentityStore(string connectionString, PostgresAccountDataKeys dataKeys) : IIdentityStore
{
    private NpgsqlConnection Open()
    {
        var connection = new NpgsqlConnection(connectionString);
        connection.Open();
        using var gate = new NpgsqlCommand("SELECT version FROM identity.schema_version WHERE singleton=true", connection);
        if (gate.ExecuteScalar() is not int version || version != 1)
        {
            connection.Dispose();
            throw new InvalidOperationException("Identity schema version 1 is required.");
        }
        return connection;
    }

    public bool CreateAccount(Account account)
    {
        using var c = Open();
        using var tx = c.BeginTransaction();
        using var cmd = new NpgsqlCommand("INSERT INTO identity.accounts(account_id,account_name_key,password_hash) VALUES ($1,$2,$3) ON CONFLICT (account_name_key) DO NOTHING", c, tx);
        cmd.Parameters.AddWithValue(account.Id); cmd.Parameters.AddWithValue(account.NameKey); cmd.Parameters.AddWithValue(account.PasswordHash);
        if (cmd.ExecuteNonQuery() != 1) return false;
        dataKeys.Create(account.Id, c, tx);
        tx.Commit();
        return true;
    }

    public Account? FindAccount(string nameKey)
    {
        using var c = Open();
        using var cmd = new NpgsqlCommand("SELECT account_id,account_name_key,password_hash FROM identity.accounts WHERE account_name_key=$1", c);
        cmd.Parameters.AddWithValue(nameKey);
        using var r = cmd.ExecuteReader();
        return r.Read() ? new(r.GetString(0), r.GetString(1), r.GetString(2)) : null;
    }

    public void SaveSession(byte[] tokenHash, SessionRecord session)
    {
        using var c = Open();
        using var cmd = new NpgsqlCommand("INSERT INTO identity.sessions(token_hash,session_id,account_id,expires_at) VALUES ($1,$2,$3,$4)", c);
        cmd.Parameters.AddWithValue(tokenHash); cmd.Parameters.AddWithValue(session.SessionId);
        cmd.Parameters.AddWithValue(session.AccountId); cmd.Parameters.AddWithValue(session.ExpiresAt);
        cmd.ExecuteNonQuery();
    }

    public SessionRecord? FindSession(byte[] tokenHash, DateTimeOffset now)
    {
        using var c = Open();
        using var cmd = new NpgsqlCommand("SELECT session_id,account_id,expires_at FROM identity.sessions WHERE token_hash=$1 AND ended_at IS NULL AND expires_at>$2", c);
        cmd.Parameters.AddWithValue(tokenHash); cmd.Parameters.AddWithValue(now);
        using var r = cmd.ExecuteReader();
        return r.Read() ? new(r.GetString(0), r.GetString(1), r.GetFieldValue<DateTimeOffset>(2)) : null;
    }

    public bool EndSession(byte[] tokenHash)
    {
        using var c = Open();
        using var cmd = new NpgsqlCommand("UPDATE identity.sessions SET ended_at=now() WHERE token_hash=$1 AND ended_at IS NULL", c);
        cmd.Parameters.AddWithValue(tokenHash);
        return cmd.ExecuteNonQuery() == 1;
    }

    public IReadOnlyList<CharacterRecord> ListCharacters(string accountId, string? realmId)
    {
        using var c = Open();
        using var cmd = new NpgsqlCommand("SELECT character_id,account_id,realm_id,name,name_key,appearance,state,archetype_id FROM identity.characters WHERE account_id=$1 AND state='active' AND ($2::text IS NULL OR realm_id=$2) ORDER BY created_at,character_id", c);
        cmd.Parameters.AddWithValue(accountId); cmd.Parameters.AddWithValue((object?)realmId ?? DBNull.Value);
        using var r = cmd.ExecuteReader();
        var result = new List<CharacterRecord>();
        while (r.Read()) result.Add(ReadCharacter(r));
        return result;
    }

    public CharacterRecord? FindCharacter(string characterId)
    {
        using var c = Open();
        using var cmd = new NpgsqlCommand("SELECT character_id,account_id,realm_id,name,name_key,appearance,state,archetype_id FROM identity.characters WHERE character_id=$1", c);
        cmd.Parameters.AddWithValue(characterId);
        using var r = cmd.ExecuteReader();
        return r.Read() ? ReadCharacter(r) : null;
    }

    public CreateCharacterResult CreateCharacter(CharacterRecord character, string idempotencyKey, byte[] requestHash,
        int realmLimit, int accountLimit)
    {
        using var c = Open();
        using var tx = c.BeginTransaction();
        using (var cmd = new NpgsqlCommand("SELECT pg_advisory_xact_lock(hashtext($1), 476217)", c, tx))
        {
            cmd.Parameters.AddWithValue(character.AccountId);
            cmd.ExecuteNonQuery();
        }
        using (var replay = new NpgsqlCommand("SELECT request_hash,response::text FROM identity.character_create_requests WHERE account_id=$1 AND idempotency_key=$2", c, tx))
        {
            replay.Parameters.AddWithValue(character.AccountId); replay.Parameters.AddWithValue(idempotencyKey);
            using var row = replay.ExecuteReader();
            if (row.Read())
            {
                if (!CryptographicOperations.FixedTimeEquals(row.GetFieldValue<byte[]>(0), requestHash))
                    return new(CreateCharacterOutcome.KeyConflict);
                var original = JsonSerializer.Deserialize<CharacterRecord>(row.GetString(1))
                    ?? throw new InvalidOperationException("Invalid character creation response.");
                return new(CreateCharacterOutcome.Replayed, original);
            }
        }
        using (var nameLock = new NpgsqlCommand("SELECT pg_advisory_xact_lock(hashtext($1), hashtext($2))", c, tx))
        {
            nameLock.Parameters.AddWithValue(character.RealmId); nameLock.Parameters.AddWithValue(character.NameKey);
            nameLock.ExecuteNonQuery();
        }
        using (var held = new NpgsqlCommand("SELECT EXISTS(SELECT 1 FROM identity.characters WHERE realm_id=$1 AND name_key=$2 AND (state='active' OR name_hold_until>now()))", c, tx))
        {
            held.Parameters.AddWithValue(character.RealmId); held.Parameters.AddWithValue(character.NameKey);
            if (held.ExecuteScalar() is true) return new(CreateCharacterOutcome.NameTaken);
        }
        using (var count = new NpgsqlCommand("SELECT count(*) FILTER (WHERE realm_id=$2),count(*) FROM identity.characters WHERE account_id=$1 AND state='active'", c, tx))
        {
            count.Parameters.AddWithValue(character.AccountId); count.Parameters.AddWithValue(character.RealmId);
            using var r = count.ExecuteReader(); r.Read();
            if (r.GetInt64(0) >= realmLimit || r.GetInt64(1) >= accountLimit) return new(CreateCharacterOutcome.SlotsFull);
        }
        using var insert = new NpgsqlCommand("INSERT INTO identity.characters(character_id,account_id,realm_id,name,name_key,appearance,state,archetype_id) VALUES ($1,$2,$3,$4,$5,$6,'active',$7) ON CONFLICT DO NOTHING", c, tx);
        insert.Parameters.AddWithValue(character.CharacterId); insert.Parameters.AddWithValue(character.AccountId);
        insert.Parameters.AddWithValue(character.RealmId); insert.Parameters.AddWithValue(character.Name);
        insert.Parameters.AddWithValue(character.NameKey); insert.Parameters.AddWithValue(character.Appearance);
        insert.Parameters.AddWithValue(character.Archetype);
        var inserted = insert.ExecuteNonQuery() == 1;
        if (!inserted) return new(CreateCharacterOutcome.NameTaken);
        using (var save = new NpgsqlCommand("INSERT INTO identity.character_create_requests(account_id,idempotency_key,request_hash,response) VALUES ($1,$2,$3,$4::jsonb)", c, tx))
        {
            save.Parameters.AddWithValue(character.AccountId); save.Parameters.AddWithValue(idempotencyKey);
            save.Parameters.AddWithValue(requestHash); save.Parameters.AddWithValue(JsonSerializer.Serialize(character));
            save.ExecuteNonQuery();
        }
        tx.Commit();
        return new(CreateCharacterOutcome.Created, character);
    }

    public bool DeleteCharacter(string characterId, string accountId, DateTimeOffset now, DateTimeOffset nameHoldUntil)
    {
        using var c = Open();
        using var cmd = new NpgsqlCommand("UPDATE identity.characters SET state='deleted',deleted_at=$3,name_hold_until=$4 WHERE character_id=$1 AND account_id=$2 AND state='active'", c);
        cmd.Parameters.AddWithValue(characterId); cmd.Parameters.AddWithValue(accountId); cmd.Parameters.AddWithValue(now); cmd.Parameters.AddWithValue(nameHoldUntil);
        return cmd.ExecuteNonQuery() == 1;
    }

    public bool ConsumeTicket(byte[] nonceHash, DateTimeOffset expiresAt)
    {
        using var c = Open();
        using var cmd = new NpgsqlCommand("INSERT INTO identity.ticket_uses(nonce_hash,expires_at) VALUES ($1,$2) ON CONFLICT DO NOTHING", c);
        cmd.Parameters.AddWithValue(nonceHash); cmd.Parameters.AddWithValue(expiresAt);
        return cmd.ExecuteNonQuery() == 1;
    }

    private static CharacterRecord ReadCharacter(NpgsqlDataReader r) => new(r.GetString(0), r.GetString(1),
        r.GetString(2), r.GetString(3), r.GetString(4), r.GetFieldValue<byte[]>(5), r.GetString(6), r.GetString(7));
}
