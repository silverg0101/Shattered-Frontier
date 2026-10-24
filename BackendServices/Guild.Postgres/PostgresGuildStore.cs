using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using Npgsql;

namespace ShatteredFrontier.Guild.Postgres;

public sealed record GuildMember(string CharacterId, int Rank, DateTime JoinedAtUtc);
public sealed record GuildRoster(string GuildId, string RealmId, string Name, string Tag, long Version, GuildMember[] Members);
public sealed record GuildMembership(string GuildId, string[] Members);
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
    private static void Event(NpgsqlConnection db, NpgsqlTransaction tx, string guildId, string realm, string name, object payload, string privacy = "p1_pseudonymous")
    {
        var envelope = JsonSerializer.Serialize(new { event_id = "evt_" + Guid.NewGuid().ToString("N"), event_name = name,
            schema_version = 1, occurred_at = DateTime.UtcNow, producer = new { service = "guild-service" },
            realm_id = realm, privacy_class = privacy, payload });
        using var insert = Cmd(db, tx, "INSERT INTO guild.outbox(event_id,topic,partition_key,envelope) VALUES ($1,'domain.guild',$2,$3::jsonb)",
            JsonDocument.Parse(envelope).RootElement.GetProperty("event_id").GetString()!, guildId, envelope);
        insert.ExecuteNonQuery();
    }
    private static string RealmOf(NpgsqlConnection db, NpgsqlTransaction tx, string guildId)
    {
        using var cmd = Cmd(db, tx, "SELECT realm_id FROM guild.guilds WHERE guild_id=$1 AND state='active'", guildId);
        return cmd.ExecuteScalar() as string ?? throw new GuildRejectedException("Guild not active.", "not_found");
    }
    private static bool HasPermission(NpgsqlConnection db, NpgsqlTransaction tx, string guildId, string actor, string permission)
    {
        using var cmd = Cmd(db, tx, "SELECT m.rank=0 OR $3=ANY(r.permissions) FROM guild.members m JOIN guild.ranks r ON (m.guild_id=r.guild_id AND m.rank=r.rank) WHERE m.guild_id=$1 AND m.character_id=$2", guildId, actor, permission);
        return cmd.ExecuteScalar() is true;
    }
    private static void Bump(NpgsqlConnection db, NpgsqlTransaction tx, string guildId)
    { using var cmd = Cmd(db, tx, "UPDATE guild.guilds SET version=version+1 WHERE guild_id=$1", guildId); cmd.ExecuteNonQuery(); }
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
            using (var ranks = Cmd(db, tx, "INSERT INTO guild.ranks(guild_id,rank,title,permissions) SELECT $1,n,'',ARRAY[]::text[] FROM generate_series(0,9) n", guildId)) ranks.ExecuteNonQuery();
            using (var founder = Cmd(db, tx, "INSERT INTO guild.members(character_id,guild_id,rank) VALUES ($1,$2,0)", actor, guildId)) founder.ExecuteNonQuery();
            Event(db, tx, guildId, realm, "guild.profile.created", new { founder_character_id = actor });
            Event(db, tx, guildId, realm, "guild.member.joined", new { character_id = actor, rank = 0 });
            return ReadRoster(db, tx, guildId);
        });
    }
    public bool Invite(string key, string guildId, string actor, string recipient)
    {
        Guild(guildId); Character(actor); Character(recipient);
        Require(actor != recipient, "Cannot invite self.", "invalid");
        return Mutate(connectionString, key, "invite", string.Join('|', guildId, actor, recipient), (db, tx) =>
        {
            RealmOf(db, tx, guildId);
            Require(HasPermission(db, tx, guildId, actor, "invite"), "Invite permission required.", "permission");
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
            Bump(db, tx, guildId);
            Event(db, tx, guildId, realm, "guild.member.joined", new { character_id = actor, rank = 1 });
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
    public GuildMembership? Membership(string actor)
    {
        Character(actor);
        using var db = new NpgsqlConnection(connectionString); db.Open();
        using var cmd = Cmd(db, null, "SELECT m.guild_id FROM guild.members m JOIN guild.guilds g ON g.guild_id=m.guild_id WHERE m.character_id=$1 AND g.state='active'", actor);
        var id = cmd.ExecuteScalar() as string;
        if (id is null) return null;
        using var members = Cmd(db, null, "SELECT character_id FROM guild.members WHERE guild_id=$1", id);
        using var reader = members.ExecuteReader();
        var list = new List<string>(); while (reader.Read()) list.Add(reader.GetString(0));
        return new(id, [.. list]);
    }
    public GuildRoster SetRank(string key, string guildId, string actor, string target, int rank)
    {
        Guild(guildId); Character(actor); Character(target);
        Require(rank is >= 1 and <= 9, "Invalid target rank.", "invalid");
        return Mutate(connectionString, key, "rank", string.Join('|', guildId, actor, target, rank), (db, tx) =>
        {
            var realm = RealmOf(db, tx, guildId);
            var actorRank = Rank(db, tx, guildId, actor);
            var oldRank = Rank(db, tx, guildId, target);
            Require(actorRank is not null && oldRank is not null && oldRank > 0 && actorRank < oldRank && actorRank < rank && rank != oldRank &&
                ((rank < oldRank && HasPermission(db, tx, guildId, actor, "promote_lower")) ||
                 (rank > oldRank && actorRank == 0 && HasPermission(db, tx, guildId, actor, "edit_ranks"))),
                "Rank change permission required.", "permission");
            using (var cmd = Cmd(db, tx, "UPDATE guild.members SET rank=$3,rank_since=now() WHERE guild_id=$1 AND character_id=$2", guildId, target, rank)) cmd.ExecuteNonQuery();
            Bump(db, tx, guildId);
            return ReadRoster(db, tx, guildId);
        });
    }
    public GuildRoster SetPermissions(string key, string guildId, string actor, int rank, string[] permissions)
    {
        Guild(guildId); Character(actor);
        Require(rank is >= 1 and <= 9 && permissions.All(p => p is "invite" or "kick_lower" or "promote_lower" or "edit_ranks" or "bank_withdraw_items" or "bank_withdraw_gold_daily_copper" or "chat_officer" or "settlement_build" or "declare_war"), "Invalid permissions.", "invalid");
        var ordered = permissions.Distinct(StringComparer.Ordinal).Order(StringComparer.Ordinal).ToArray();
        return Mutate(connectionString, key, "permissions", string.Join('|', guildId, actor, rank, string.Join(',', ordered)), (db, tx) =>
        {
            RealmOf(db, tx, guildId);
            Require(Rank(db, tx, guildId, actor) == 0 && HasPermission(db, tx, guildId, actor, "edit_ranks"), "Leader rank edit required.", "permission");
            using (var cmd = Cmd(db, tx, "UPDATE guild.ranks SET permissions=$3 WHERE guild_id=$1 AND rank=$2", guildId, rank, ordered)) cmd.ExecuteNonQuery();
            Bump(db, tx, guildId);
            return ReadRoster(db, tx, guildId);
        });
    }
    private static GuildRoster? Remove(NpgsqlConnection db, NpgsqlTransaction tx, string guildId, string target, string cause)
    {
        var realm = RealmOf(db, tx, guildId);
        var before = ReadRoster(db, tx, guildId);
        var leader = Rank(db, tx, guildId, target) == 0;
        using (var cmd = Cmd(db, tx, "DELETE FROM guild.members WHERE guild_id=$1 AND character_id=$2", guildId, target)) cmd.ExecuteNonQuery();
        Event(db, tx, guildId, realm, "guild.member.left", new { character_id = target, left_cause = cause });
        if (before.Members.Length == 1)
        {
            using var cmd = Cmd(db, tx, "UPDATE guild.guilds SET state='disbanded',disbanded_at=now(),assets_release_at=now()+interval '30 days',version=version+1 WHERE guild_id=$1", guildId); cmd.ExecuteNonQuery();
            Event(db, tx, guildId, realm, "guild.profile.disbanded", new { disband_cause = "last_member_left", member_count_before = 1 }, "p0_public");
            return null;
        }
        if (leader)
        {
            using var next = Cmd(db, tx, "SELECT character_id FROM guild.members WHERE guild_id=$1 ORDER BY rank,rank_since,joined_at,character_id LIMIT 1", guildId);
            var successor = (string)next.ExecuteScalar()!;
            using (var cmd = Cmd(db, tx, "UPDATE guild.members SET rank=0,rank_since=now() WHERE character_id=$1", successor)) cmd.ExecuteNonQuery();
            Event(db, tx, guildId, realm, "guild.leader.changed", new { from_character_id = target, to_character_id = successor, change_cause = "leader_left" });
        }
        Bump(db, tx, guildId);
        return ReadRoster(db, tx, guildId);
    }
    public GuildRoster? Leave(string key, string guildId, string actor)
    {
        Guild(guildId); Character(actor);
        return Mutate(connectionString, key, "leave", string.Join('|', guildId, actor), (db, tx) =>
        {
            Require(Rank(db, tx, guildId, actor) is not null, "Guild membership required.", "permission");
            return Remove(db, tx, guildId, actor, "left");
        });
    }
    public GuildRoster Kick(string key, string guildId, string actor, string target)
    {
        Guild(guildId); Character(actor); Character(target);
        return Mutate(connectionString, key, "kick", string.Join('|', guildId, actor, target), (db, tx) =>
        {
            var actorRank = Rank(db, tx, guildId, actor); var targetRank = Rank(db, tx, guildId, target);
            Require(actorRank is not null && targetRank is not null && actorRank < targetRank && HasPermission(db, tx, guildId, actor, "kick_lower"), "Kick permission required.", "permission");
            return Remove(db, tx, guildId, target, "kicked")!;
        });
    }
    public bool Disband(string key, string guildId, string actor)
    {
        Guild(guildId); Character(actor);
        return Mutate(connectionString, key, "disband", string.Join('|', guildId, actor), (db, tx) =>
        {
            var realm = RealmOf(db, tx, guildId);
            Require(Rank(db, tx, guildId, actor) == 0, "Leader required.", "permission");
            var count = ReadRoster(db, tx, guildId).Members.Length;
            using (var members = Cmd(db, tx, "DELETE FROM guild.members WHERE guild_id=$1", guildId)) members.ExecuteNonQuery();
            using (var invitations = Cmd(db, tx, "DELETE FROM guild.invitations WHERE guild_id=$1", guildId)) invitations.ExecuteNonQuery();
            using (var guild = Cmd(db, tx, "UPDATE guild.guilds SET state='disbanded',disbanded_at=now(),assets_release_at=now()+interval '30 days',version=version+1 WHERE guild_id=$1", guildId)) guild.ExecuteNonQuery();
            Event(db, tx, guildId, realm, "guild.profile.disbanded", new { disband_cause = "leader_disbanded", member_count_before = count }, "p0_public");
            return true;
        });
    }
}
