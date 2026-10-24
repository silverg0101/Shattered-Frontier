using System.Text.Json;
using System.Text;
using System.Security.Cryptography;
using System.Data;
using System.Text.RegularExpressions;
using Npgsql;

namespace ShatteredFrontier.Group.Postgres;

public sealed record GroupControls
{
    // Release-declared party limits. Changes take effect at the next host start.
    public int PartyMaxMembers { get; init; } = 5;
    public void Validate()
    {
        if (PartyMaxMembers is < 2 or > 5)
            throw new InvalidOperationException("Group controls outside shipped bounds.");
    }
}

public sealed record PartySnapshot(string PartyId, string Leader, string Assistant, string[] Members, int Version, string State);
public sealed class GroupRejectedException(string message, string code) : InvalidOperationException(message)
{
    public string Code { get; } = code;
}

public sealed class PostgresGroupStore(string connectionString, GroupControls controls)
{
    private static readonly Regex IdPattern = new("^[a-z][a-z0-9_]{2,79}$", RegexOptions.Compiled | RegexOptions.CultureInvariant);
    private static void Id(string value, string prefix)
    {
        if (!IdPattern.IsMatch(value) || !value.StartsWith(prefix, StringComparison.Ordinal))
            throw new GroupRejectedException($"Invalid {prefix} identity.", "invalid");
    }
    private static void Key(string value)
    {
        if (value.Length is < 1 or > 128 || !Regex.IsMatch(value, "^[a-zA-Z0-9:_./-]+$", RegexOptions.CultureInvariant))
            throw new GroupRejectedException("Invalid idempotency key.", "invalid");
    }
    private static void Realm(string value)
    {
        if (!Regex.IsMatch(value, "^[a-z0-9][a-z0-9_-]{1,31}$", RegexOptions.CultureInvariant))
            throw new GroupRejectedException("Invalid realm identity.", "invalid");
    }
    private static NpgsqlCommand Command(NpgsqlConnection connection, NpgsqlTransaction? transaction, string sql, params object[] values)
    {
        var cmd = new NpgsqlCommand(sql, connection, transaction);
        for (int i = 0; i < values.Length; i++) cmd.Parameters.AddWithValue(values[i]);
        return cmd;
    }
    private static void Require(bool condition, string message, string code = "precondition")
    {
        if (!condition) throw new GroupRejectedException(message, code);
    }
    private static PartySnapshot ReadParty(NpgsqlConnection db, NpgsqlTransaction? tx, string partyId)
    {
        using var p = Command(db, tx, "SELECT leader_character_id,state,version FROM \"group\".parties WHERE party_id=$1", partyId);
        using var row = p.ExecuteReader();
        Require(row.Read(), "Party not found.", "not_found");
        var leader = row.GetString(0); var state = row.GetString(1); var version = row.GetInt32(2);
        row.Close();
        using var m = Command(db, tx, "SELECT character_id,is_assistant FROM \"group\".party_members WHERE party_id=$1 AND left_at IS NULL ORDER BY joined_at,character_id", partyId);
        using var members = m.ExecuteReader();
        var ids = new List<string>(); var assistant = "";
        while (members.Read()) { ids.Add(members.GetString(0)); if (members.GetBoolean(1)) assistant = members.GetString(0); }
        return new PartySnapshot(partyId, leader, assistant, [.. ids], version, state);
    }
    private PartySnapshot Mutate(string key, string rpc, string input, Func<NpgsqlConnection, NpgsqlTransaction, string> apply)
    {
        Key(key);
        var hash = Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(input)));
        using var db = new NpgsqlConnection(connectionString); db.Open();
        using var tx = db.BeginTransaction();
        // One transaction lock covers both party capacity and one-active-party checks.
        using (var l = Command(db, tx, "SELECT pg_advisory_xact_lock(208940, 1)")) l.ExecuteNonQuery();
        PartySnapshot? prior = null;
        using (var replay = Command(db, tx, "SELECT rpc,request_hash,response FROM \"group\".idempotency_keys WHERE idempotency_key=$1", key))
        using (var reader = replay.ExecuteReader())
        {
            if (reader.Read())
            {
                Require(reader.GetString(0) == rpc && reader.GetString(1) == hash, "Idempotency key reused with different request.", "invalid");
                prior = JsonSerializer.Deserialize<PartySnapshot>((byte[])reader[2])!;
            }
        }
        if (prior is not null) { tx.Commit(); return prior; }
        var id = apply(db, tx);
        var snapshot = ReadParty(db, tx, id);
        using (var receipt = Command(db, tx, "INSERT INTO \"group\".idempotency_keys(idempotency_key,rpc,request_hash,response) VALUES ($1,$2,$3,$4)",
            key, rpc, hash, JsonSerializer.SerializeToUtf8Bytes(snapshot))) receipt.ExecuteNonQuery();
        tx.Commit(); return snapshot;
    }
    public PartySnapshot Create(string key, string leader, string account, string realm, string region)
    {
        Id(leader, "chr_"); Id(account, "acct_"); Realm(realm);
        Require(region.Length is > 0 and <= 32 && Regex.IsMatch(region, "^[a-z0-9_]+$"), "Invalid dc_region.", "invalid");
        return Mutate(key, "create", string.Join("|", leader, account, realm, region), (db, tx) =>
        {
            Require(FindForCharacter(db, tx, leader) is null, "Character already in a party.");
            var partyId = "pty_" + Guid.NewGuid().ToString("N");
            using (var p = Command(db, tx, "INSERT INTO \"group\".parties(party_id,dc_region,leader_character_id) VALUES ($1,$2,$3)", partyId, region, leader)) p.ExecuteNonQuery();
            using (var m = Command(db, tx, "INSERT INTO \"group\".party_members(party_id,character_id,account_id,realm_id) VALUES ($1,$2,$3,$4)", partyId, leader, account, realm)) m.ExecuteNonQuery();
            return partyId;
        });
    }
    public PartySnapshot Join(string key, string partyId, string actor, string member, string account, string realm)
    {
        Id(partyId, "pty_"); Id(actor, "chr_"); Id(member, "chr_"); Id(account, "acct_"); Realm(realm);
        return Mutate(key, "join", string.Join("|", partyId, actor, member, account, realm), (db, tx) =>
        {
            var party = ReadParty(db, tx, partyId);
            Require(party.State == "active" && party.Leader == actor, "Only an active party leader can add a member.", "permission");
            Require(party.Members.Length < controls.PartyMaxMembers && actor != member, "Party full or invalid member.");
            Require(FindForCharacter(db, tx, member) is null, "Character already in a party.");
            using var realmCheck = Command(db, tx, "SELECT realm_id FROM \"group\".party_members WHERE party_id=$1 AND left_at IS NULL LIMIT 1", partyId);
            Require((string?)realmCheck.ExecuteScalar() == realm, "Premade parties must share a realm.");
            using (var m = Command(db, tx, "INSERT INTO \"group\".party_members(party_id,character_id,account_id,realm_id) VALUES ($1,$2,$3,$4)", partyId, member, account, realm)) m.ExecuteNonQuery();
            Bump(db, tx, partyId); return partyId;
        });
    }
    public PartySnapshot Leave(string key, string partyId, string actor, string member, bool kicked)
    {
        Id(partyId, "pty_"); Id(actor, "chr_"); Id(member, "chr_");
        return Mutate(key, kicked ? "kick" : "leave", string.Join("|", partyId, actor, member), (db, tx) =>
        {
            var party = ReadParty(db, tx, partyId);
            Require(party.State == "active" && party.Members.Contains(member), "Member not in active party.");
            Require(kicked ? party.Leader == actor && actor != member : actor == member, "Actor cannot remove member.", "permission");
            using (var m = Command(db, tx, "UPDATE \"group\".party_members SET left_at=now(),left_reason=$1,is_assistant=false WHERE party_id=$2 AND character_id=$3 AND left_at IS NULL", kicked ? "kicked" : "left", partyId, member)) m.ExecuteNonQuery();
            if (party.Members.Length == 1)
            {
                using var d = Command(db, tx, "UPDATE \"group\".parties SET state='disbanded',disbanded_at=now(),version=version+1 WHERE party_id=$1", partyId);
                d.ExecuteNonQuery();
            }
            else
            {
                if (party.Leader == member)
                {
                    var next = party.Assistant.Length > 0 && party.Assistant != member ? party.Assistant : party.Members.First(id => id != member);
                    using var leader = Command(db, tx, "UPDATE \"group\".parties SET leader_character_id=$1 WHERE party_id=$2", next, partyId);
                    leader.ExecuteNonQuery();
                    using var assistant = Command(db, tx, "UPDATE \"group\".party_members SET is_assistant=false WHERE party_id=$1 AND left_at IS NULL", partyId);
                    assistant.ExecuteNonQuery();
                }
                Bump(db, tx, partyId);
            }
            return partyId;
        });
    }
    public PartySnapshot SetRole(string key, string rpc, string partyId, string actor, string member)
    {
        Id(partyId, "pty_"); Id(actor, "chr_"); Id(member, "chr_");
        Require(rpc is "promote" or "assistant", "Invalid role action.", "invalid");
        return Mutate(key, rpc, string.Join("|", partyId, actor, member), (db, tx) =>
        {
            var party = ReadParty(db, tx, partyId);
            Require(party.State == "active" && party.Leader == actor && actor != member && party.Members.Contains(member), "Only leader can assign a member role.", "permission");
            if (rpc == "promote")
            {
                using var cmd = Command(db, tx, "UPDATE \"group\".parties SET leader_character_id=$1 WHERE party_id=$2", member, partyId);
                cmd.ExecuteNonQuery();
                using var clear = Command(db, tx, "UPDATE \"group\".party_members SET is_assistant=false WHERE party_id=$1 AND character_id=$2 AND left_at IS NULL", partyId, member);
                clear.ExecuteNonQuery();
            }
            else
            {
                using var clear = Command(db, tx, "UPDATE \"group\".party_members SET is_assistant=(character_id=$2) WHERE party_id=$1 AND left_at IS NULL", partyId, member);
                clear.ExecuteNonQuery();
            }
            Bump(db, tx, partyId); return partyId;
        });
    }
    private static void Bump(NpgsqlConnection db, NpgsqlTransaction tx, string partyId)
    {
        using var cmd = Command(db, tx, "UPDATE \"group\".parties SET version=version+1 WHERE party_id=$1", partyId);
        cmd.ExecuteNonQuery();
    }
    private static string? FindForCharacter(NpgsqlConnection db, NpgsqlTransaction? tx, string character)
    {
        using var cmd = Command(db, tx, "SELECT party_id FROM \"group\".party_members WHERE character_id=$1 AND left_at IS NULL", character);
        return cmd.ExecuteScalar() as string;
    }
    public PartySnapshot? GetForCharacter(string character)
    {
        Id(character, "chr_");
        using var db = new NpgsqlConnection(connectionString); db.Open();
        using var tx = db.BeginTransaction(IsolationLevel.RepeatableRead);
        var partyId = FindForCharacter(db, tx, character);
        var result = partyId is null ? null : ReadParty(db, tx, partyId);
        tx.Commit(); return result;
    }
}
