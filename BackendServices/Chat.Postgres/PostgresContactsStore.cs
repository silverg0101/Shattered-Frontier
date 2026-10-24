using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using Npgsql;

namespace ShatteredFrontier.Chat.Postgres;

public sealed record ContactRecord(string OtherCharacterId, string Kind, DateTime CreatedAtUtc);
public sealed class ContactRejectedException(string message) : ArgumentException(message);

public sealed class PostgresContactsStore(string connectionString)
{
    private static readonly Regex CharacterPattern = new("^chr_[a-z0-9_]{1,76}$", RegexOptions.Compiled | RegexOptions.CultureInvariant);
    private static readonly Regex KeyPattern = new("^[a-zA-Z0-9:_./-]{1,128}$", RegexOptions.Compiled | RegexOptions.CultureInvariant);

    private static void Character(string value)
    {
        if (!CharacterPattern.IsMatch(value)) throw new ContactRejectedException("Invalid character identity.");
    }
    private static void Kind(string value)
    {
        if (value is not ("friend" or "block")) throw new ContactRejectedException("Invalid contact kind.");
    }
    private static NpgsqlCommand Command(NpgsqlConnection db, NpgsqlTransaction? tx, string sql, params object[] values)
    {
        var command = new NpgsqlCommand(sql, db, tx);
        foreach (var value in values) command.Parameters.AddWithValue(value);
        return command;
    }
    private static void LockCharacters(NpgsqlConnection db, NpgsqlTransaction tx, IEnumerable<string> characters)
    {
        foreach (var character in characters.Distinct(StringComparer.Ordinal).Order(StringComparer.Ordinal))
            using (var mutex = Command(db, tx, "SELECT pg_advisory_xact_lock(hashtext($1), 7442)", character)) mutex.ExecuteNonQuery();
    }

    public bool Mutate(string key, string actor, string other, string kind, bool add)
    {
        if (!KeyPattern.IsMatch(key)) throw new ContactRejectedException("Invalid idempotency key.");
        Character(actor); Character(other); Kind(kind);
        if (actor == other) throw new ContactRejectedException("A character cannot contact itself.");
        var input = string.Join('|', add ? "add" : "remove", actor, other, kind);
        var hash = Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(input)));
        using var db = new NpgsqlConnection(connectionString); db.Open();
        using var tx = db.BeginTransaction();
        using (var mutex = Command(db, tx, "SELECT pg_advisory_xact_lock(hashtext($1), 7441)", key)) mutex.ExecuteNonQuery();
        LockCharacters(db, tx, [actor, other]);
        using (var erased = Command(db, tx, "SELECT EXISTS (SELECT 1 FROM chat.erased_characters WHERE character_id IN ($1,$2))", actor, other))
            if ((bool)erased.ExecuteScalar()!) throw new ContactRejectedException("Erased character cannot have contacts.");
        using (var prior = Command(db, tx, "SELECT request_hash,applied FROM chat.idempotency_keys WHERE idempotency_key=$1", key))
        using (var reader = prior.ExecuteReader())
        {
            if (reader.Read())
            {
                if (reader.GetString(0) != hash) throw new ContactRejectedException("Idempotency key reused with different request.");
                var applied = reader.GetBoolean(1);
                reader.Close(); tx.Commit(); return applied;
            }
        }
        using var change = Command(db, tx, add
            ? "INSERT INTO chat.contacts(character_id,other_character_id,kind) VALUES ($1,$2,$3) ON CONFLICT DO NOTHING"
            : "DELETE FROM chat.contacts WHERE character_id=$1 AND other_character_id=$2 AND kind=$3", actor, other, kind);
        var result = change.ExecuteNonQuery() == 1;
        using var receipt = Command(db, tx, "INSERT INTO chat.idempotency_keys(idempotency_key,request_hash,applied) VALUES ($1,$2,$3)", key, hash, result);
        receipt.ExecuteNonQuery(); tx.Commit(); return result;
    }

    public IReadOnlyList<ContactRecord> List(string actor)
    {
        Character(actor);
        using var db = new NpgsqlConnection(connectionString); db.Open();
        using var command = Command(db, null, "SELECT other_character_id,kind,created_at FROM chat.contacts WHERE character_id=$1 ORDER BY kind,other_character_id", actor);
        using var reader = command.ExecuteReader();
        var contacts = new List<ContactRecord>();
        while (reader.Read()) contacts.Add(new(reader.GetString(0), reader.GetString(1), reader.GetDateTime(2)));
        return contacts;
    }

    public bool IsBlocked(string actor, string other)
    {
        Character(actor); Character(other);
        using var db = new NpgsqlConnection(connectionString); db.Open();
        using var command = Command(db, null, "SELECT EXISTS (SELECT 1 FROM chat.contacts WHERE kind='block' AND ((character_id=$1 AND other_character_id=$2) OR (character_id=$2 AND other_character_id=$1)))", actor, other);
        return (bool)command.ExecuteScalar()!;
    }

    public bool CanDeliverWhisper(string actor, string other)
    {
        Character(actor); Character(other);
        if (actor == other) throw new ContactRejectedException("A character cannot whisper itself.");
        using var db = new NpgsqlConnection(connectionString); db.Open();
        using var command = Command(db, null, "SELECT NOT EXISTS (SELECT 1 FROM chat.erased_characters WHERE character_id IN ($1,$2)) AND NOT EXISTS (SELECT 1 FROM chat.contacts WHERE kind='block' AND ((character_id=$1 AND other_character_id=$2) OR (character_id=$2 AND other_character_id=$1)))", actor, other);
        return (bool)command.ExecuteScalar()!;
    }

    public int EraseContacts(IReadOnlyList<string> characters)
    {
        if (characters.Count == 0) throw new ContactRejectedException("At least one character is required.");
        foreach (var character in characters) Character(character);
        var ids = characters.Distinct(StringComparer.Ordinal).Order(StringComparer.Ordinal).ToArray();
        using var db = new NpgsqlConnection(connectionString); db.Open();
        using var tx = db.BeginTransaction();
        LockCharacters(db, tx, ids);
        foreach (var id in ids)
            using (var mark = Command(db, tx, "INSERT INTO chat.erased_characters(character_id) VALUES ($1) ON CONFLICT DO NOTHING", id)) mark.ExecuteNonQuery();
        using var delete = Command(db, tx, "DELETE FROM chat.contacts WHERE character_id=ANY($1) OR other_character_id=ANY($1)", (object)ids);
        var count = delete.ExecuteNonQuery();
        tx.Commit();
        return count;
    }
}
