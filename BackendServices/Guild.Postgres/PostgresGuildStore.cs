using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using Npgsql;

namespace ShatteredFrontier.Guild.Postgres;

public sealed record GuildMember(string CharacterId, int Rank, DateTime JoinedAtUtc);
public sealed record GuildRoster(string GuildId, string RealmId, string Name, string Tag, long Version, GuildMember[] Members);
public sealed class GuildRejectedException(string message, string code) : InvalidOperationException(message)
{
    public string Code { get; } = code;
}
public sealed record GuildControls(int MemberMax = 500)
{
    // tune.backend.guild_member_max: members, operator-owned; effective on the next acceptance.
    // Bounds follow the single-leader minimum and the specified maximum of 500.
    public static GuildControls Shipped { get; } = new();
    public void Validate()
    {
        if (MemberMax is < 1 or > 500) throw new ArgumentOutOfRangeException(nameof(MemberMax));
    }
}

public sealed class PostgresGuildStore(string connectionString, GuildControls controls)
{
    private readonly int memberMax = ValidatedMax(controls);
    private static int ValidatedMax(GuildControls controls) { controls.Validate(); return controls.MemberMax; }
    private static readonly Regex CharacterPattern = new("^chr_[a-z0-9_]{1,76}$", RegexOptions.Compiled | RegexOptions.CultureInvariant);
    private static readonly Regex GuildPattern = new("^gld_[a-z0-9]+$", RegexOptions.Compiled | RegexOptions.CultureInvariant);
    private static readonly Regex RealmPattern = new("^[a-z0-9][a-z0-9_-]{1,31}$", RegexOptions.Compiled | RegexOptions.CultureInvariant);
    private static readonly Regex NamePattern = new("^[A-Za-z0-9]+(?: [A-Za-z0-9]+)*$", RegexOptions.Compiled | RegexOptions.CultureInvariant);
    private static readonly Regex TagPattern = new("^[A-Za-z]{2,5}$", RegexOptions.Compiled | RegexOptions.CultureInvariant);
    private static readonly Regex KeyPattern = new("^[A-Za-z0-9:_./-]{1,128}$", RegexOptions.Compiled | RegexOptions.CultureInvariant);

    private static void Require(bool allowed, string message, string code = "precondition")
    {
        if (!allowed) throw new GuildRejectedException(message, code);
    }
    private static void Character(string value) => Require(CharacterPattern.IsMatch(value), "Invalid character identity.", "invalid");
    private static void Guild(string value) => Require(GuildPattern.IsMatch(value), "Invalid guild identity.", "invalid");
    private static void Realm(string value) => Require(RealmPattern.IsMatch(value), "Invalid realm identity.", "invalid");
    private static NpgsqlCommand Cmd(NpgsqlConnection db, NpgsqlTransaction? tx, string sql, params object[] values)
    {
        var cmd = new NpgsqlCommand(sql, db, tx);
        foreach (var value in values) cmd.Parameters.AddWithValue(value);
        return cmd;
    }
    private static T Mutate<T>(string connection, string key, string operation, string input, Func<NpgsqlConnection,NpgsqlTransaction,T> work)
    {
        Require(KeyPattern.IsMatch(key), "Invalid idempotency key.", "invalid");
        var hash = Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(input)));
        using var db = new NpgsqlConnection(connection); db.Open();
        using var tx = db.BeginTransaction();
        // Serializes membership, capacity, and name/tag uniqueness checks across this schema.
        using (var mutex = Cmd(db, tx, "SELECT pg_advisory_xact_lock(208940, 3)")) mutex.ExecuteNonQuery();
        using (var prior = Cmd(db, tx, "SELECT operation,request_hash,response FROM guild.idempotency_keys WHERE idempotency_key=$1", key))
        using (var reader = prior.ExecuteReader())
        {
            if (reader.Read())
            {
                Require(reader.GetString(0) == operation && reader.GetString(1) == hash,
                    "Idempotency key reused with different request.", "invalid");
                var replay = JsonSerializer.Deserialize<T>((byte[])reader[2])!;
                reader.Close(); tx.Commit(); return replay;
            }
        }
        T result;
        try { result = work(db, tx); }
        catch (PostgresException ex) when (ex.SqlState == "23505")
        { throw new GuildRejectedException("Guild name, tag, or membership is already used.", "conflict"); }
        using (var receipt = Cmd(db, tx, "INSERT INTO guild.idempotency_keys(idempotency_key,operation,request_hash,response) VALUES ($1,$2,$3,$4)",
            key, operation, hash, JsonSerializer.SerializeToUtf8Bytes(result))) receipt.ExecuteNonQuery();
        tx.Commit(); return result;
    }
    private static GuildRoster ReadRoster(NpgsqlConnection db, NpgsqlTransaction? tx, string guildId)
    {
        using var guild = Cmd(db, tx, "SELECT realm_id,name,tag,version,state FROM guild.guilds WHERE guild_id=$1", guildId);
        using var row = guild.ExecuteReader();
        Require(row.Read(), "Guild not found.", "not_found");
        var realm = row.GetString(0); var name = row.GetString(1); var tag = row.GetString(2);
        var version = row.GetInt64(3); var state = row.GetString(4);
        row.Close();
        Require(state == "active", "Guild not active.", "not_found");
        using var members = Cmd(db, tx, "SELECT character_id,rank,joined_at FROM guild.members WHERE guild_id=$1 ORDER BY rank,joined_at,character_id", guildId);
        using var reader = members.ExecuteReader();
        var list = new List<GuildMember>();
        while (reader.Read()) list.Add(new(reader.GetString(0), reader.GetInt16(1), reader.GetDateTime(2)));
        return new(guildId, realm, name, tag, version, [.. list]);
    }
    private static int? Rank(NpgsqlConnection db, NpgsqlTransaction? tx, string guildId, string actor)
    {
        using var cmd = Cmd(db, tx, "SELECT rank FROM guild.members WHERE guild_id=$1 AND character_id=$2", guildId, actor);
        return cmd.ExecuteScalar() is short value ? value : null;
    }
    public GuildRoster Create(string key, string actor, string realm, string name, string tag)
    {
        Character(actor); Realm(realm);
        Require(name.Length is >= 3 and <= 24 && NamePattern.IsMatch(name), "Invalid guild name.", "invalid");
        Require(TagPattern.IsMatch(tag), "Invalid guild tag.", "invalid");
        var normalizedTag = tag.ToUpperInvariant();
        return Mutate(connectionString, key, "create", string.Join('|', actor, realm, name, normalizedTag), (db, tx) =>
        {
            using (var membership = Cmd(db, tx, "SELECT EXISTS(SELECT 1 FROM guild.members WHERE character_id=$1)", actor))
                Require(!(bool)membership.ExecuteScalar()!, "Character already belongs to a guild.", "conflict");
            var guildId = "gld_" + Guid.NewGuid().ToString("N");
            using (var create = Cmd(db, tx, "INSERT INTO guild.guilds(guild_id,realm_id,name,name_key,tag,state) VALUES ($1,$2,$3,$4,$5,'active')",
                guildId, realm, name, GuildNameSkeleton.For(name), normalizedTag)) create.ExecuteNonQuery();
            using (var ranks = Cmd(db, tx, "INSERT INTO guild.ranks(guild_id,rank,title,permissions) VALUES ($1,0,'',ARRAY['invite']::text[]),($1,1,'',ARRAY[]::text[])", guildId)) ranks.ExecuteNonQuery();
            using (var founder = Cmd(db, tx, "INSERT INTO guild.members(character_id,guild_id,rank) VALUES ($1,$2,0)", actor, guildId)) founder.ExecuteNonQuery();
            return ReadRoster(db, tx, guildId);
        });
    }
    public bool Invite(string key, string guildId, string actor, string recipient)
    {
        Guild(guildId); Character(actor); Character(recipient);
        Require(actor != recipient, "Cannot invite self.", "invalid");
        return Mutate(connectionString, key, "invite", string.Join('|', guildId, actor, recipient), (db, tx) =>
        {
            Require(Rank(db, tx, guildId, actor) == 0, "Leader invitation required.", "permission");
            using (var existing = Cmd(db, tx, "SELECT EXISTS(SELECT 1 FROM guild.members WHERE character_id=$1)", recipient))
                Require(!(bool)existing.ExecuteScalar()!, "Recipient already belongs to a guild.", "conflict");
            using var insert = Cmd(db, tx, "INSERT INTO guild.invitations(guild_id,recipient_character_id,invited_by) VALUES ($1,$2,$3) ON CONFLICT DO NOTHING", guildId, recipient, actor);
            return insert.ExecuteNonQuery() == 1;
        });
    }
    public GuildRoster Accept(string key, string guildId, string actor, string realm)
    {
        Guild(guildId); Character(actor); Realm(realm);
        return Mutate(connectionString, key, "accept", string.Join('|', guildId, actor, realm), (db, tx) =>
        {
            var roster = ReadRoster(db, tx, guildId);
            Require(roster.RealmId == realm, "Guild and character realms differ.", "precondition");
            Require(roster.Members.Length < memberMax, "Guild roster is full.", "capacity");
            using (var existing = Cmd(db, tx, "SELECT EXISTS(SELECT 1 FROM guild.members WHERE character_id=$1)", actor))
                Require(!(bool)existing.ExecuteScalar()!, "Character already belongs to a guild.", "conflict");
            using (var invitation = Cmd(db, tx, "DELETE FROM guild.invitations WHERE guild_id=$1 AND recipient_character_id=$2", guildId, actor))
                Require(invitation.ExecuteNonQuery() == 1, "Invitation required.", "permission");
            using (var member = Cmd(db, tx, "INSERT INTO guild.members(character_id,guild_id,rank) VALUES ($1,$2,1)", actor, guildId)) member.ExecuteNonQuery();
            using (var bump = Cmd(db, tx, "UPDATE guild.guilds SET version=version+1 WHERE guild_id=$1", guildId)) bump.ExecuteNonQuery();
            using (var other = Cmd(db, tx, "DELETE FROM guild.invitations WHERE recipient_character_id=$1", actor)) other.ExecuteNonQuery();
            return ReadRoster(db, tx, guildId);
        });
    }
    public GuildRoster Roster(string guildId, string actor)
    {
        Guild(guildId); Character(actor);
        using var db = new NpgsqlConnection(connectionString); db.Open();
        Require(Rank(db, null, guildId, actor) is not null, "Guild membership required.", "permission");
        return ReadRoster(db, null, guildId);
    }
}
