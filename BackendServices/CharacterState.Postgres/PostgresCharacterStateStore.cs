using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using Npgsql;
using CharacterAppearance;
using ShatteredFrontier.Progression;
using ShatteredFrontier.QuestContent;

namespace ShatteredFrontier.CharacterState.Postgres;

public sealed class CharacterStoreUnavailableException(string message, Exception inner) : Exception(message, inner);
public sealed class CharacterSchemaCompatibilityException(string message) : Exception(message);
public sealed class CharacterStoreRejectedException(string message) : Exception(message);
public sealed record VersionedResult<T>(long Revision, T Result);
public sealed record CharacterPositionSnapshot(string ZoneId, double X, double Y, double Z, float Yaw);
public sealed record CharacterDeathSnapshot(bool IsDead, string DeathEventId, long OccurredAtUnixMs, long RespawnAtUnixMs,
    CharacterPositionSnapshot DeathPosition, CharacterPositionSnapshot RespawnPosition);
public sealed record RecipeLearnIntent(string OperationId, string CharacterId, string RecipeId, string Source,
    string SourceId, string ContainerId, long ExpectedContainerRevision, long CostCopper, string Fingerprint, string State);

// Migration identity installs Schema.sql. Runtime uses DML and marker SELECT only.
public sealed class PostgresCharacterStateStore(string connectionString)
{
    public CharacterDeathSnapshot? ReadDeath(string characterId) => Transaction((c, t) => ReadDeath(c, t, characterId));

    public (int Level, int MinimumLevel, int WearPercent) ReadDeathPenalty(string eventId, string characterId) => Transaction((c, t) =>
    {
        using var cmd = Command(c, t, "SELECT level_at_death,wear_min_level,wear_percent FROM character_state.death_events WHERE death_event_id=$1 AND character_id=$2", eventId, characterId);
        using var row = cmd.ExecuteReader();
        return row.Read() ? (row.GetInt32(0), row.GetInt32(1), row.GetInt32(2)) :
            throw new CharacterStoreRejectedException("Unknown death event.");
    });
    public int ReadDeathLevel(string eventId, string characterId) => ReadDeathPenalty(eventId, characterId).Level;

    private static CharacterDeathSnapshot? ReadDeath(NpgsqlConnection c, NpgsqlTransaction t, string characterId)
    {
        using var cmd = Command(c, t, "SELECT is_dead,death_event_id,occurred_at_unix_ms,respawn_at_unix_ms,death_zone_id,death_x,death_y,death_z,respawn_zone_id,respawn_x,respawn_y,respawn_z FROM character_state.deaths WHERE character_id=$1", characterId);
        using var r = cmd.ExecuteReader();
        return r.Read() ? new(r.GetBoolean(0), r.GetString(1), r.GetInt64(2), r.GetInt64(3),
            new(r.GetString(4), r.GetDouble(5), r.GetDouble(6), r.GetDouble(7), 0),
            new(r.GetString(8), r.GetDouble(9), r.GetDouble(10), r.GetDouble(11), 0)) : null;
    }

    public CharacterDeathSnapshot ReportDeath(string eventId, string characterId, long occurredAtUnixMs,
        CharacterPositionSnapshot death, CharacterPositionSnapshot respawn, int delayMs,
        int wearMinLevel = 10, int wearPercent = 5) => Transaction((c, t) =>
    {
        if (wearMinLevel is < 1 or > 1000 || wearPercent is < 0 or > 100)
            throw new CharacterStoreRejectedException("Invalid death wear tuning.");
        LockCharacter(c, t, characterId);
        var fingerprint = Fingerprint(new { characterId, occurredAtUnixMs, death, respawn, delayMs });
        bool replay = false;
        using (var existing = Command(c, t, "SELECT character_id,fingerprint FROM character_state.death_events WHERE death_event_id=$1", eventId))
        using (var r = existing.ExecuteReader())
            if (r.Read())
            {
                if (r.GetString(0) != characterId || r.GetString(1) != fingerprint)
                    throw new CharacterStoreRejectedException("Death event ID reused with different payload.");
                replay = true;
            }
        if (replay) return ReadDeath(c, t, characterId) ?? throw new CharacterStoreRejectedException("Missing death state.");
        var prior = ReadDeath(c, t, characterId);
        if (prior?.IsDead == true) throw new CharacterStoreRejectedException("Character is already dead.");
        long due = checked(occurredAtUnixMs + delayMs);
        int level = 1;
        using (var progression = Command(c, t, "SELECT state::text FROM character_state.progression WHERE character_id=$1", characterId))
        {
            if (progression.ExecuteScalar() is string json)
            {
                var snapshot = JsonSerializer.Deserialize<ProgressionSnapshot>(json) ?? throw new CharacterStoreRejectedException("Invalid stored progression state.");
                level = new CharacterProgression(characterId, TimeProvider.System, () => .5, snapshot: snapshot).Level;
            }
        }
        using (var cmd = Command(c, t, "INSERT INTO character_state.death_events(death_event_id,character_id,fingerprint,level_at_death,wear_min_level,wear_percent) VALUES ($1,$2,$3,$4,$5,$6)",
            eventId, characterId, fingerprint, level, wearMinLevel, wearPercent)) cmd.ExecuteNonQuery();
        using (var cmd = Command(c, t, "INSERT INTO character_state.deaths(character_id,death_event_id,is_dead,occurred_at_unix_ms,respawn_at_unix_ms,death_zone_id,death_x,death_y,death_z,respawn_zone_id,respawn_x,respawn_y,respawn_z) VALUES ($1,$2,true,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12) ON CONFLICT (character_id) DO UPDATE SET death_event_id=EXCLUDED.death_event_id,is_dead=true,occurred_at_unix_ms=EXCLUDED.occurred_at_unix_ms,respawn_at_unix_ms=EXCLUDED.respawn_at_unix_ms,death_zone_id=EXCLUDED.death_zone_id,death_x=EXCLUDED.death_x,death_y=EXCLUDED.death_y,death_z=EXCLUDED.death_z,respawn_zone_id=EXCLUDED.respawn_zone_id,respawn_x=EXCLUDED.respawn_x,respawn_y=EXCLUDED.respawn_y,respawn_z=EXCLUDED.respawn_z,updated_at=now()",
            characterId, eventId, occurredAtUnixMs, due, death.ZoneId, death.X, death.Y, death.Z, respawn.ZoneId, respawn.X, respawn.Y, respawn.Z)) cmd.ExecuteNonQuery();
        return ReadDeath(c, t, characterId)!;
    });

    public CharacterDeathSnapshot ClearDeath(string characterId, string eventId, bool resurrectAtDeathPosition = false) => Transaction((c, t) =>
    {
        LockCharacter(c, t, characterId);
        var prior = ReadDeath(c, t, characterId);
        if (prior is null || prior.DeathEventId != eventId) throw new CharacterStoreRejectedException("Death event does not match.");
        if (!prior.IsDead) return prior;
        using var cmd = Command(c, t, "UPDATE character_state.deaths SET is_dead=false,updated_at=now() WHERE character_id=$1 AND death_event_id=$2", characterId, eventId);
        cmd.ExecuteNonQuery();
        var point = resurrectAtDeathPosition ? prior.DeathPosition : prior.RespawnPosition;
        using var save = Command(c, t, "INSERT INTO character_state.positions(character_id,revision,zone_id,x,y,z,yaw) VALUES ($1,1,$2,$3,$4,$5,0) ON CONFLICT (character_id) DO UPDATE SET revision=character_state.positions.revision+1,zone_id=EXCLUDED.zone_id,x=EXCLUDED.x,y=EXCLUDED.y,z=EXCLUDED.z,yaw=0,updated_at=now()",
            characterId, point.ZoneId, point.X, point.Y, point.Z);
        save.ExecuteNonQuery();
        return prior with { IsDead = false };
    });
    public VersionedResult<byte[]>? ReadAppearance(string characterId) => Transaction<VersionedResult<byte[]>?>((c, t) =>
    {
        using var cmd = Command(c, t, "SELECT revision,packet FROM character_state.appearance WHERE character_id=$1", characterId);
        using var r = cmd.ExecuteReader();
        return r.Read() ? new(r.GetInt64(0), r.GetFieldValue<byte[]>(1)) : null;
    });

    public long SaveAppearance(string characterId, long expectedRevision, byte[] packet, AppearanceValidator validator,
        IReadOnlySet<ushort> knownCosmetics, IReadOnlySet<ushort> ownedCosmetics)
    {
        ArgumentNullException.ThrowIfNull(packet);
        ArgumentNullException.ThrowIfNull(validator);
        var validation = validator.ValidatePacked(packet, knownCosmetics, ownedCosmetics);
        if (!validation.Valid) throw new CharacterStoreRejectedException(validation.Code!);
        byte[] stored = (byte[])packet.Clone();
        return Transaction((c, t) =>
        {
            LockCharacter(c, t, characterId);
            using var cmd = Command(c, t, "INSERT INTO character_state.appearance(character_id,revision,packet) VALUES ($1,1,$3) ON CONFLICT (character_id) DO UPDATE SET revision=character_state.appearance.revision+1,packet=EXCLUDED.packet,updated_at=now() WHERE character_state.appearance.revision=$2 RETURNING revision",
                characterId, expectedRevision, stored);
            var result = cmd.ExecuteScalar();
            if (result is null || expectedRevision != 0 && (long)result == 1)
                throw new CharacterStoreRejectedException("Stale appearance revision.");
            return (long)result;
        });
    }

    public VersionedResult<CharacterPositionSnapshot>? ReadPosition(string characterId) => Transaction<VersionedResult<CharacterPositionSnapshot>?>((c, t) =>
    {
        using var cmd = Command(c, t, "SELECT revision,zone_id,x,y,z,yaw FROM character_state.positions WHERE character_id=$1", characterId);
        using var r = cmd.ExecuteReader();
        return r.Read() ? new(r.GetInt64(0), new(r.GetString(1), r.GetDouble(2), r.GetDouble(3), r.GetDouble(4), r.GetFloat(5))) : null;
    });

    public long SavePosition(string characterId, long expectedRevision, CharacterPositionSnapshot position) => Transaction((c, t) =>
    {
        LockCharacter(c, t, characterId);
        using var cmd = Command(c, t, "INSERT INTO character_state.positions(character_id,revision,zone_id,x,y,z,yaw) VALUES ($1,1,$3,$4,$5,$6,$7) ON CONFLICT (character_id) DO UPDATE SET revision=character_state.positions.revision+1,zone_id=EXCLUDED.zone_id,x=EXCLUDED.x,y=EXCLUDED.y,z=EXCLUDED.z,yaw=EXCLUDED.yaw,updated_at=now() WHERE character_state.positions.revision=$2 RETURNING revision",
            characterId, expectedRevision, position.ZoneId, position.X, position.Y, position.Z, position.Yaw);
        var result = cmd.ExecuteScalar();
        if (result is null) throw new CharacterStoreRejectedException("Stale position revision.");
        if (expectedRevision != 0 && (long)result == 1) throw new CharacterStoreRejectedException("Stale position revision.");
        return (long)result;
    });

    // The zone server supplies a validated landing; the store owns the wall clock and serializes concurrent requests.
    // Updating the existing position row and cooldown together makes reconnect and server restart fail closed.
    public long CommitUnstuck(string operationId, string characterId, CharacterPositionSnapshot position, int cooldownSeconds,
        TimeProvider? clock = null) => Transaction((c, t) =>
    {
        if (string.IsNullOrWhiteSpace(operationId) || string.IsNullOrWhiteSpace(characterId) || cooldownSeconds is < 60 or > 3600)
            throw new CharacterStoreRejectedException("Invalid Unstuck settings or character.");
        LockCharacter(c, t, characterId);
        var now = (clock ?? TimeProvider.System).GetUtcNow().ToUnixTimeMilliseconds();
        var hash = Fingerprint(new { characterId, position, cooldownSeconds });
        using (var read = Command(c, t, "SELECT unstuck_at_unix_ms,unstuck_operation_id,unstuck_fingerprint,revision FROM character_state.positions WHERE character_id=$1", characterId))
        using (var row = read.ExecuteReader())
        {
            if (row.Read())
            {
                if (row.GetString(1) == operationId)
                {
                    if (row.GetString(2) != hash) throw new CharacterStoreRejectedException("Unstuck operation ID reused with different payload.");
                    return row.GetInt64(3);
                }
                if (now - row.GetInt64(0) < cooldownSeconds * 1000L)
                throw new CharacterStoreRejectedException("Unstuck cooldown active.");
            }
        }
        using var save = Command(c, t, "INSERT INTO character_state.positions(character_id,revision,zone_id,x,y,z,yaw,unstuck_at_unix_ms,unstuck_operation_id,unstuck_fingerprint) VALUES ($1,1,$2,$3,$4,$5,$6,$7,$8,$9) ON CONFLICT (character_id) DO UPDATE SET revision=character_state.positions.revision+1,zone_id=EXCLUDED.zone_id,x=EXCLUDED.x,y=EXCLUDED.y,z=EXCLUDED.z,yaw=EXCLUDED.yaw,unstuck_at_unix_ms=EXCLUDED.unstuck_at_unix_ms,unstuck_operation_id=EXCLUDED.unstuck_operation_id,unstuck_fingerprint=EXCLUDED.unstuck_fingerprint,updated_at=now() RETURNING revision",
            characterId, position.ZoneId, position.X, position.Y, position.Z, position.Yaw, now, operationId, hash);
        return (long)(save.ExecuteScalar() ?? throw new CharacterStoreRejectedException("Unstuck position not saved."));
    });

    public VersionedResult<ProgressionSnapshot>? ReadProgression(string characterId) => Transaction<VersionedResult<ProgressionSnapshot>?>((c, t) =>
    {
        using var cmd = Command(c, t, "SELECT revision,state::text FROM character_state.progression WHERE character_id=$1", characterId);
        using var r = cmd.ExecuteReader();
        return r.Read() ? new(r.GetInt64(0), JsonSerializer.Deserialize<ProgressionSnapshot>(r.GetString(1)) ?? throw new CharacterStoreRejectedException("Invalid stored progression state.")) : null;
    });

    // JSON additive migration: old rows have no CombatIdentity property. Locking the character
    // makes first restore and a concurrent progression receipt converge on one durable identity.
    public VersionedResult<ProgressionSnapshot> EnsureCombatIdentity(string characterId,
        CombatAttributeTuning tuning, ProgressionSettings? progressionSettings = null) =>
        Transaction<VersionedResult<ProgressionSnapshot>>((c, t) =>
    {
        if (string.IsNullOrWhiteSpace(characterId)) throw new CharacterStoreRejectedException("Character ID required.");
        tuning.Validate();
        LockCharacter(c, t, characterId);
        long revision = 0;
        ProgressionSnapshot? snapshot = null;
        using (var read = Command(c, t, "SELECT revision,state::text FROM character_state.progression WHERE character_id=$1", characterId))
        using (var row = read.ExecuteReader())
            if (row.Read())
            {
                revision = row.GetInt64(0);
                snapshot = JsonSerializer.Deserialize<ProgressionSnapshot>(row.GetString(1)) ??
                    throw new CharacterStoreRejectedException("Invalid stored progression state.");
            }
        if (snapshot?.CombatIdentity is not null)
        {
            tuning.Snapshot(snapshot.CombatIdentity, 1); // Validate stored identity before publication.
            return new(revision, snapshot);
        }
        snapshot = snapshot is null
            ? new CharacterProgression(characterId, TimeProvider.System, () => .5,
                settings: progressionSettings).Snapshot()
                with { CombatIdentity = tuning.StarterIdentity }
            : snapshot with { CombatIdentity = tuning.StarterIdentity };
        if (revision == 0)
        {
            using var insert = Command(c, t, "INSERT INTO character_state.progression(character_id,revision,state) VALUES ($1,1,$2::jsonb)",
                characterId, JsonSerializer.Serialize(snapshot));
            insert.ExecuteNonQuery();
        }
        else
        {
            using var update = Command(c, t, "UPDATE character_state.progression SET revision=revision+1,state=$2::jsonb WHERE character_id=$1",
                characterId, JsonSerializer.Serialize(snapshot));
            update.ExecuteNonQuery();
        }
        return new(revision + 1, snapshot);
    });

    public VersionedResult<ProgressionSnapshot>? StartProgressionSession(string characterId, TimeProvider clock, ProgressionSettings settings) => Transaction<VersionedResult<ProgressionSnapshot>?>((c, t) =>
    {
        LockCharacter(c, t, characterId);
        using var read = Command(c, t, "SELECT revision,state::text FROM character_state.progression WHERE character_id=$1", characterId);
        using var row = read.ExecuteReader();
        if (!row.Read()) return null;
        long revision = row.GetInt64(0);
        var snapshot = JsonSerializer.Deserialize<ProgressionSnapshot>(row.GetString(1)) ?? throw new CharacterStoreRejectedException("Invalid stored progression state.");
        row.Close();
        var model = new CharacterProgression(characterId, clock, () => .5, settings, snapshot: snapshot);
        if (!model.BeginSession()) return new(revision, snapshot);
        snapshot = model.Snapshot();
        using var update = Command(c, t, "UPDATE character_state.progression SET revision=revision+1,state=$2::jsonb WHERE character_id=$1 RETURNING revision", characterId, JsonSerializer.Serialize(snapshot));
        return new((long)(update.ExecuteScalar() ?? throw new CharacterStoreRejectedException("Progression update lost.")), snapshot);
    });

    public VersionedResult<ProgressionSnapshot> EndProgressionSession(string characterId, string operationId, long expectedRevision,
        bool inRestArea, TimeProvider clock, ProgressionSettings settings) => Transaction((c, t) =>
    {
        if (string.IsNullOrWhiteSpace(characterId) || string.IsNullOrWhiteSpace(operationId)) throw new ArgumentException("Session identity required.");
        LockCharacter(c, t, characterId);
        var hash = Fingerprint(new { characterId, inRestArea });
        var replay = Replay<ProgressionSnapshot>(c, t, "progression_session_end", characterId, operationId, hash);
        if (replay is not null) return replay;
        long revision = 0; ProgressionSnapshot? snapshot = null;
        using (var read = Command(c, t, "SELECT revision,state::text FROM character_state.progression WHERE character_id=$1", characterId))
        using (var row = read.ExecuteReader()) if (row.Read()) { revision = row.GetInt64(0); snapshot = JsonSerializer.Deserialize<ProgressionSnapshot>(row.GetString(1)); }
        if (revision != expectedRevision) throw new CharacterStoreRejectedException("Stale progression revision.");
        var model = new CharacterProgression(characterId, clock, () => .5, settings, snapshot: snapshot);
        if (model.LastSessionEndAt is not null) throw new CharacterStoreRejectedException("Session already ended.");
        model.EndSession(inRestArea);
        snapshot = model.Snapshot();
        if (revision == 0)
        {
            using var insert = Command(c, t, "INSERT INTO character_state.progression VALUES ($1,1,$2::jsonb)", characterId, JsonSerializer.Serialize(snapshot));
            insert.ExecuteNonQuery();
        }
        else
        {
            using var update = Command(c, t, "UPDATE character_state.progression SET revision=revision+1,state=$2::jsonb WHERE character_id=$1", characterId, JsonSerializer.Serialize(snapshot));
            update.ExecuteNonQuery();
        }
        var result = new VersionedResult<ProgressionSnapshot>(revision + 1, snapshot);
        Receipt(c, t, "progression_session_end", characterId, operationId, hash, result);
        return result;
    });

    public RecipeLearnIntent BeginRecipeLearn(string operationId, string characterId, string recipeId, string source,
        string sourceId, string containerId, long expectedContainerRevision, long costCopper, string skillId, int minimumSkill, int maxKnown)
        => Transaction((c, t) =>
    {
        LockCharacter(c, t, characterId);
        var fingerprint = Fingerprint(new { characterId, recipeId, source, sourceId, containerId, expectedContainerRevision, costCopper });
        using (var existing = Command(c, t, "SELECT operation_id,character_id,recipe_id,source,source_id,container_id,expected_container_revision,cost_copper,fingerprint,state FROM character_state.recipe_intents WHERE operation_id=$1", operationId))
        using (var r = existing.ExecuteReader())
            if (r.Read())
            {
                var prior = ReadRecipeIntent(r);
                if (prior.Fingerprint != fingerprint) throw new CharacterStoreRejectedException("Operation ID reused with different payload.");
                return prior;
            }
        ProgressionSnapshot? snapshot = null;
        using (var cmd = Command(c, t, "SELECT state::text FROM character_state.progression WHERE character_id=$1", characterId))
        using (var r = cmd.ExecuteReader()) if (r.Read()) snapshot = JsonSerializer.Deserialize<ProgressionSnapshot>(r.GetString(0));
        if ((snapshot?.KnownRecipes ?? []).Any(k => k.RecipeId == recipeId)) throw new CharacterStoreRejectedException("already_known");
        if ((snapshot?.KnownRecipes?.Length ?? 0) >= maxKnown) throw new CharacterStoreRejectedException("known_recipe_limit");
        if ((snapshot?.Skills.GetValueOrDefault(skillId) ?? 0) < minimumSkill) throw new CharacterStoreRejectedException("skill_too_low");
        using (var pending = Command(c, t, "SELECT 1 FROM character_state.recipe_intents WHERE character_id=$1 AND recipe_id=$2 AND state='Pending'", characterId, recipeId))
            if (pending.ExecuteScalar() is not null) throw new CharacterStoreRejectedException("recipe_learning_pending");
        using var insert = Command(c, t, "INSERT INTO character_state.recipe_intents(operation_id,character_id,recipe_id,source,source_id,container_id,expected_container_revision,cost_copper,fingerprint) VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9)",
            operationId, characterId, recipeId, source, sourceId, containerId, expectedContainerRevision, costCopper, fingerprint);
        insert.ExecuteNonQuery();
        return new RecipeLearnIntent(operationId, characterId, recipeId, source, sourceId, containerId, expectedContainerRevision, costCopper, fingerprint, "Pending");
    });

    private static RecipeLearnIntent ReadRecipeIntent(NpgsqlDataReader r) => new(r.GetString(0), r.GetString(1), r.GetString(2),
        r.GetString(3), r.GetString(4), r.GetString(5), r.GetInt64(6), r.GetInt64(7), r.GetString(8), r.GetString(9));

    public IReadOnlyList<RecipeLearnIntent> PendingRecipeLearns() => Transaction((c, t) =>
    {
        var result = new List<RecipeLearnIntent>();
        using var cmd = Command(c, t, "SELECT operation_id,character_id,recipe_id,source,source_id,container_id,expected_container_revision,cost_copper,fingerprint,state FROM character_state.recipe_intents WHERE state='Pending' ORDER BY created_at LIMIT 100");
        using var r = cmd.ExecuteReader(); while (r.Read()) result.Add(ReadRecipeIntent(r));
        return (IReadOnlyList<RecipeLearnIntent>)result;
    });

    public RecipeLearnIntent? RecipeIntent(string operationId) => Transaction<RecipeLearnIntent?>((c, t) =>
    {
        using var cmd = Command(c, t, "SELECT operation_id,character_id,recipe_id,source,source_id,container_id,expected_container_revision,cost_copper,fingerprint,state FROM character_state.recipe_intents WHERE operation_id=$1", operationId);
        using var r = cmd.ExecuteReader(); return r.Read() ? ReadRecipeIntent(r) : null;
    });

    public long CompleteRecipeLearn(RecipeLearnIntent intent, DateTimeOffset learnedAt) => Transaction((c, t) =>
    {
        LockCharacter(c, t, intent.CharacterId);
        using (var check = Command(c, t, "SELECT fingerprint,state FROM character_state.recipe_intents WHERE operation_id=$1 FOR UPDATE", intent.OperationId))
        using (var r = check.ExecuteReader())
        {
            if (!r.Read() || r.GetString(0) != intent.Fingerprint) throw new CharacterStoreRejectedException("Recipe intent changed.");
        }
        long revision = 0; ProgressionSnapshot? snapshot = null;
        using (var read = Command(c, t, "SELECT revision,state::text FROM character_state.progression WHERE character_id=$1", intent.CharacterId))
        using (var r = read.ExecuteReader()) if (r.Read()) { revision = r.GetInt64(0); snapshot = JsonSerializer.Deserialize<ProgressionSnapshot>(r.GetString(1)); }
        if ((snapshot?.KnownRecipes ?? []).Any(k => k.RecipeId == intent.RecipeId))
        {
            using var done = Command(c, t, "UPDATE character_state.recipe_intents SET state='Complete' WHERE operation_id=$1", intent.OperationId);
            done.ExecuteNonQuery(); return revision;
        }
        snapshot ??= new CharacterProgression(intent.CharacterId, TimeProvider.System, () => .5).Snapshot();
        var known = (snapshot.KnownRecipes ?? []).Append(new KnownRecipe(intent.RecipeId, intent.Source, learnedAt)).ToArray();
        var state = JsonSerializer.Serialize(snapshot with { KnownRecipes = known });
        if (revision == 0)
        {
            using var insert = Command(c, t, "INSERT INTO character_state.progression VALUES ($1,1,$2::jsonb)", intent.CharacterId, state);
            insert.ExecuteNonQuery(); revision = 1;
        }
        else
        {
            using var update = Command(c, t, "UPDATE character_state.progression SET revision=revision+1,state=$2::jsonb WHERE character_id=$1 RETURNING revision", intent.CharacterId, state);
            revision = (long)(update.ExecuteScalar() ?? throw new CharacterStoreRejectedException("Progression update lost."));
        }
        using var complete = Command(c, t, "UPDATE character_state.recipe_intents SET state='Complete' WHERE operation_id=$1", intent.OperationId);
        complete.ExecuteNonQuery(); return revision;
    });

    public void CancelRecipeLearn(RecipeLearnIntent intent) => Transaction((c, t) =>
    {
        LockCharacter(c, t, intent.CharacterId);
        using var cmd = Command(c, t, "DELETE FROM character_state.recipe_intents WHERE operation_id=$1 AND fingerprint=$2 AND state='Pending'",
            intent.OperationId, intent.Fingerprint);
        cmd.ExecuteNonQuery(); return 0;
    });

    public VersionedResult<QuestProgressSnapshot>? ReadQuest(QuestDefinition quest, string characterId) => Transaction<VersionedResult<QuestProgressSnapshot>?>((c, t) =>
    {
        using var cmd = Command(c, t, "SELECT content_revision,revision,state::text FROM character_state.quests WHERE character_id=$1 AND quest_id=$2", characterId, quest.Id);
        using var r = cmd.ExecuteReader();
        if (!r.Read()) return null;
        if (r.GetInt32(0) != quest.Revision) throw new CharacterStoreRejectedException("Quest content revision changed.");
        return new(r.GetInt64(1), JsonSerializer.Deserialize<QuestProgressSnapshot>(r.GetString(2)) ?? throw new CharacterStoreRejectedException("Invalid stored quest state."));
    });

    private static NpgsqlCommand Command(NpgsqlConnection c, NpgsqlTransaction t, string sql, params object[] values)
    {
        var command = new NpgsqlCommand(sql, c, t);
        foreach (var value in values) command.Parameters.AddWithValue(value);
        return command;
    }

    private static void LockCharacter(NpgsqlConnection c, NpgsqlTransaction t, string characterId)
    {
        using var command = Command(c, t, "SELECT pg_advisory_xact_lock(hashtext('sf.character:' || $1))", characterId);
        command.ExecuteNonQuery();
    }

    private T Transaction<T>(Func<NpgsqlConnection, NpgsqlTransaction, T> action)
    {
        try
        {
            using var c = new NpgsqlConnection(connectionString); c.Open();
            using var t = c.BeginTransaction();
            try
            {
                using var marker = Command(c, t, "SELECT version,migration_id,compatible_build_min,compatible_build_max FROM character_state.schema_version WHERE singleton=true FOR SHARE");
                using var row = marker.ExecuteReader();
                if (!row.Read() || row.GetInt32(0) != 10 || row.GetString(1) != "character-state-010" || row.GetInt32(2) > 10 || row.GetInt32(3) < 10)
                    throw new CharacterSchemaCompatibilityException("Unsupported character state schema version.");
            }
            catch (PostgresException ex) when (ex.SqlState is "42P01" or "3F000" or "42501")
            { throw new CharacterSchemaCompatibilityException("Character state schema marker missing or inaccessible."); }
            var result = action(c, t); t.Commit(); return result;
        }
        catch (PostgresException ex) when (ex.SqlState.StartsWith("22", StringComparison.Ordinal) || ex.SqlState.StartsWith("23", StringComparison.Ordinal))
        { throw new CharacterStoreRejectedException($"Character store rejected operation ({ex.SqlState})."); }
        catch (Exception ex) when (ex is NpgsqlException or TimeoutException or IOException)
        { throw new CharacterStoreUnavailableException("Character store unavailable; retry the same operation ID.", ex); }
    }

    private static string Fingerprint<T>(T value) => Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(JsonSerializer.Serialize(value))));
    private static VersionedResult<T>? Replay<T>(NpgsqlConnection c, NpgsqlTransaction t, string domain, string character, string operation, string fingerprint)
    {
        using var cmd = Command(c, t, "SELECT fingerprint,result::text FROM character_state.source_receipts WHERE domain=$1 AND character_id=$2 AND operation_id=$3", domain, character, operation);
        using var r = cmd.ExecuteReader();
        if (!r.Read()) return null;
        if (r.GetString(0) != fingerprint) throw new CharacterStoreRejectedException("Operation ID reused with different payload.");
        return JsonSerializer.Deserialize<VersionedResult<T>>(r.GetString(1)) ?? throw new CharacterStoreRejectedException("Invalid stored receipt.");
    }
    private static void Receipt<T>(NpgsqlConnection c, NpgsqlTransaction t, string domain, string character, string operation, string fingerprint, VersionedResult<T> result)
    {
        using var cmd = Command(c, t, "INSERT INTO character_state.source_receipts VALUES ($1,$2,$3,$4,$5::jsonb)", domain, character, operation, fingerprint, JsonSerializer.Serialize(result));
        cmd.ExecuteNonQuery();
    }

    public VersionedResult<ProgressionResult> ApplyProgression(ActionReceipt receipt, long expectedRevision, TimeProvider clock, Func<double> roll, ProgressionSettings? settings = null)
    {
        if (string.IsNullOrWhiteSpace(receipt.CharacterId) || string.IsNullOrWhiteSpace(receipt.ReceiptId)) throw new ArgumentException("Receipt identity required.");
        return Transaction((c, t) =>
        {
            LockCharacter(c, t, receipt.CharacterId);
            string hash = Fingerprint(receipt);
            var replay = Replay<ProgressionResult>(c, t, "progression", receipt.CharacterId, receipt.ReceiptId, hash);
            if (replay is not null) return replay;
            long revision = 0; ProgressionSnapshot? snapshot = null;
            using (var cmd = Command(c, t, "SELECT revision,state::text FROM character_state.progression WHERE character_id=$1", receipt.CharacterId))
            using (var r = cmd.ExecuteReader()) if (r.Read()) { revision = r.GetInt64(0); snapshot = JsonSerializer.Deserialize<ProgressionSnapshot>(r.GetString(1)); }
            if (expectedRevision != revision) throw new CharacterStoreRejectedException("Stale progression revision.");
            var model = new CharacterProgression(receipt.CharacterId, clock, roll, settings, snapshot?.Skills, snapshot?.LifetimeXp ?? 0, snapshot);
            var result = model.Apply(receipt);
            if (result.Applied)
            {
                var state = JsonSerializer.Serialize(model.Snapshot());
                if (snapshot is null)
                {
                    using var cmd = Command(c, t, "INSERT INTO character_state.progression VALUES ($1,1,$2::jsonb)", receipt.CharacterId, state); cmd.ExecuteNonQuery();
                }
                else
                {
                    using var cmd = Command(c, t, "UPDATE character_state.progression SET revision=$1,state=$2::jsonb WHERE character_id=$3 AND revision=$4", revision + 1, state, receipt.CharacterId, revision);
                    if (cmd.ExecuteNonQuery() != 1) throw new CharacterStoreRejectedException("Stale progression revision.");
                }
                revision++;
                Receipt(c, t, "progression", receipt.CharacterId, receipt.ReceiptId, hash, new VersionedResult<ProgressionResult>(revision, result));
            }
            return new VersionedResult<ProgressionResult>(revision, result);
        });
    }

    public VersionedResult<ProgressResult> AcceptQuest(QuestDefinition quest, string characterId, DateTimeOffset acceptedAt, string operationId, IReadOnlyList<QuestDefinition> prerequisites)
    {
        if (string.IsNullOrWhiteSpace(characterId) || string.IsNullOrWhiteSpace(operationId)) throw new ArgumentException("Character and operation ID required.");
        return Transaction((c, t) =>
        {
            LockCharacter(c, t, characterId);
            string hash = Fingerprint(("accept", quest.Id, characterId));
            var replay = Replay<ProgressResult>(c, t, "quest", characterId, operationId, hash);
            if (replay is not null) return replay;
            long revision = -1; QuestProgressSnapshot? existing = null;
            using (var cmd = Command(c, t, "SELECT content_revision,revision,state::text FROM character_state.quests WHERE character_id=$1 AND quest_id=$2", characterId, quest.Id))
            using (var r = cmd.ExecuteReader())
                if (r.Read())
                {
                    revision = r.GetInt64(1);
                    existing = JsonSerializer.Deserialize<QuestProgressSnapshot>(r.GetString(2)) ?? throw new CharacterStoreRejectedException("Invalid stored quest state.");
                }
            var first = quest.Steps[0].Id;
            if (existing is { Abandoned: false })
                return new VersionedResult<ProgressResult>(revision, new ProgressResult(false, existing.StepIndex >= quest.Steps.Count ? "completed" : "duplicate", null, existing.StepIndex >= quest.Steps.Count, []));
            foreach (var p in prerequisites)
            {
                using var cmd = Command(c, t, "SELECT state::text FROM character_state.quests WHERE character_id=$1 AND quest_id=$2", characterId, p.Id);
                var s = cmd.ExecuteScalar() as string;
                var snap = s is null ? null : JsonSerializer.Deserialize<QuestProgressSnapshot>(s);
                if (snap is null || snap.Abandoned || snap.StepIndex < p.Steps.Count)
                    return new VersionedResult<ProgressResult>(revision < 0 ? 0 : revision, new ProgressResult(false, "prerequisite_missing", null, false, []));
            }
            var model = new QuestProgression(quest); model.Accept(characterId, acceptedAt, (existing?.Instance ?? 0) + 1);
            var state = JsonSerializer.Serialize(model.Snapshot(characterId));
            if (existing is null)
            {
                using var cmd = Command(c, t, "INSERT INTO character_state.quests VALUES ($1,$2,$3,0,$4::jsonb)", characterId, quest.Id, quest.Revision, state); cmd.ExecuteNonQuery();
                revision = 0;
            }
            else
            {
                using var cmd = Command(c, t, "UPDATE character_state.quests SET content_revision=$1,revision=$2,state=$3::jsonb WHERE character_id=$4 AND quest_id=$5 AND revision=$6", quest.Revision, revision + 1, state, characterId, quest.Id, revision);
                if (cmd.ExecuteNonQuery() != 1) throw new CharacterStoreRejectedException("Stale quest revision.");
                revision++;
            }
            var result = new VersionedResult<ProgressResult>(revision, new ProgressResult(true, "accepted", first, false, []));
            Receipt(c, t, "quest", characterId, operationId, hash, result);
            return result;
        });
    }

    public VersionedResult<ProgressResult> AbandonQuest(QuestDefinition quest, string characterId, string operationId)
    {
        if (string.IsNullOrWhiteSpace(characterId) || string.IsNullOrWhiteSpace(operationId)) throw new ArgumentException("Character and operation ID required.");
        return Transaction((c, t) =>
        {
            LockCharacter(c, t, characterId);
            string hash = Fingerprint(("abandon", quest.Id, characterId));
            var replay = Replay<ProgressResult>(c, t, "quest", characterId, operationId, hash);
            if (replay is not null) return replay;
            long revision = 0; QuestProgressSnapshot? snapshot = null;
            using (var cmd = Command(c, t, "SELECT revision,state::text FROM character_state.quests WHERE character_id=$1 AND quest_id=$2", characterId, quest.Id))
            using (var r = cmd.ExecuteReader())
                if (r.Read()) { revision = r.GetInt64(0); snapshot = JsonSerializer.Deserialize<QuestProgressSnapshot>(r.GetString(1)); }
            if (snapshot is null || snapshot.Abandoned || snapshot.StepIndex >= quest.Steps.Count)
                return new VersionedResult<ProgressResult>(revision, new ProgressResult(false, "not_active", null, snapshot is { Abandoned: false }, []));
            using var update = Command(c, t, "UPDATE character_state.quests SET revision=$1,state=$2::jsonb WHERE character_id=$3 AND quest_id=$4 AND revision=$5", revision + 1, JsonSerializer.Serialize(snapshot with { Abandoned = true }), characterId, quest.Id, revision);
            if (update.ExecuteNonQuery() != 1) throw new CharacterStoreRejectedException("Stale quest revision.");
            var result = new VersionedResult<ProgressResult>(revision + 1, new ProgressResult(true, "abandoned", null, false, []));
            Receipt(c, t, "quest", characterId, operationId, hash, result);
            return result;
        });
    }

    public IReadOnlyList<(string QuestId, int ContentRevision, long Revision, QuestProgressSnapshot State)> ReadQuests(string characterId) => Transaction((c, t) =>
    {
        var rows = new List<(string, int, long, QuestProgressSnapshot)>();
        using var cmd = Command(c, t, "SELECT quest_id,content_revision,revision,state::text FROM character_state.quests WHERE character_id=$1 ORDER BY quest_id", characterId);
        using var r = cmd.ExecuteReader();
        while (r.Read()) rows.Add((r.GetString(0), r.GetInt32(1), r.GetInt64(2), JsonSerializer.Deserialize<QuestProgressSnapshot>(r.GetString(3)) ?? throw new CharacterStoreRejectedException("Invalid stored quest state.")));
        return (IReadOnlyList<(string QuestId, int ContentRevision, long Revision, QuestProgressSnapshot State)>)rows;
    });

    public IReadOnlySet<string> PendingRewardQuests(string characterId) => Transaction((c, t) =>
    {
        using var cmd = Command(c, t, "SELECT DISTINCT quest_id FROM character_state.reward_receipts WHERE character_id=$1 AND state='Pending'", characterId);
        using var r = cmd.ExecuteReader();
        var ids = new HashSet<string>(StringComparer.Ordinal);
        while (r.Read()) ids.Add(r.GetString(0));
        return (IReadOnlySet<string>)ids;
    });

    public VersionedResult<ProgressResult> ApplyQuest(QuestDefinition quest, ObjectiveEvent evidence, long expectedRevision, string rewardContainerId = "")
    {
        if (string.IsNullOrWhiteSpace(evidence.OperationId) || string.IsNullOrWhiteSpace(evidence.CharacterId) || evidence.QuestId != quest.Id)
            throw new ArgumentException("Quest evidence identity invalid.");
        return Transaction((c, t) =>
        {
            LockCharacter(c, t, evidence.CharacterId);
            string hash = Fingerprint(evidence);
            var replay = Replay<ProgressResult>(c, t, "quest", evidence.CharacterId, evidence.OperationId, hash);
            if (replay is not null) return replay;
            long revision; QuestProgressSnapshot snapshot;
            using (var cmd = Command(c, t, "SELECT content_revision,revision,state::text FROM character_state.quests WHERE character_id=$1 AND quest_id=$2", evidence.CharacterId, quest.Id))
            using (var r = cmd.ExecuteReader())
            {
                if (!r.Read()) return new VersionedResult<ProgressResult>(0, new ProgressResult(false, "quest not accepted", null, false, []));
                revision = r.GetInt64(1);
                snapshot = JsonSerializer.Deserialize<QuestProgressSnapshot>(r.GetString(2)) ?? throw new CharacterStoreRejectedException("Invalid stored quest state.");
                if (snapshot.Abandoned) return new VersionedResult<ProgressResult>(revision, new ProgressResult(false, "quest not accepted", null, false, []));
                if (r.GetInt32(0) != quest.Revision) throw new CharacterStoreRejectedException("Quest content revision changed.");
            }
            if (revision != expectedRevision) throw new CharacterStoreRejectedException("Stale quest revision.");
            var model = new QuestProgression(quest); model.Restore(evidence.CharacterId, snapshot);
            var result = model.Apply(evidence);
            if (result.Accepted)
            {
                using var update = Command(c, t, "UPDATE character_state.quests SET revision=$1,state=$2::jsonb WHERE character_id=$3 AND quest_id=$4 AND revision=$5", revision + 1, JsonSerializer.Serialize(model.Snapshot(evidence.CharacterId)), evidence.CharacterId, quest.Id, revision);
                if (update.ExecuteNonQuery() != 1) throw new CharacterStoreRejectedException("Stale quest revision.");
                foreach (var grant in result.Grants)
                {
                    var bundle = quest.Steps.Single(s => s.Id == grant.StepId).Rewards.Single(r => r.BundleId == grant.BundleId);
                    if (bundle.Items.Any(i => string.IsNullOrWhiteSpace(i.DefinitionId) || i.Quantity <= 0))
                        throw new CharacterStoreRejectedException("Invalid reward item snapshot.");
                    using var receipt = Command(c, t, "INSERT INTO character_state.reward_receipts(operation_id,character_id,quest_id,step_id,bundle_id,container_id,content_revision,item_lines) VALUES ($1,$2,$3,$4,$5,$6,$7,$8::jsonb)", grant.OperationId, grant.CharacterId, grant.QuestId, grant.StepId, grant.BundleId, rewardContainerId, quest.Revision, JsonSerializer.Serialize(bundle.Items));
                    receipt.ExecuteNonQuery();
                }
                revision++;
                Receipt(c, t, "quest", evidence.CharacterId, evidence.OperationId, hash, new VersionedResult<ProgressResult>(revision, result));
            }
            return new VersionedResult<ProgressResult>(revision, result);
        });
    }

    public IReadOnlyList<RewardIntent> PendingRewards(DateTimeOffset olderThan) => Transaction((c,t) =>
    {
        var result = new List<RewardIntent>();
        using var cmd = Command(c,t,"SELECT operation_id,character_id,quest_id,step_id,bundle_id,container_id,content_revision,item_lines::text FROM character_state.reward_receipts WHERE state='Pending' AND container_id<>'' AND created_at <= $1 ORDER BY created_at LIMIT 100",olderThan);
        using var r = cmd.ExecuteReader();
        while (r.Read()) result.Add(ReadReward(r));
        return (IReadOnlyList<RewardIntent>)result;
    });

    public RewardIntent? Reward(string operationId) => Transaction((c,t) =>
    {
        using var cmd = Command(c,t,"SELECT operation_id,character_id,quest_id,step_id,bundle_id,container_id,content_revision,item_lines::text FROM character_state.reward_receipts WHERE operation_id=$1",operationId);
        using var r = cmd.ExecuteReader();
        return r.Read() ? ReadReward(r) : null;
    });

    private static RewardIntent ReadReward(NpgsqlDataReader r) => new(r.GetString(0), r.GetString(1), r.GetString(2), r.GetString(3), r.GetString(4), r.GetString(5), r.GetInt32(6),
        JsonSerializer.Deserialize<QuestRewardItem[]>(r.GetString(7)) ?? throw new CharacterStoreRejectedException("Invalid stored reward snapshot."));

    public void CompleteReward(string operationId) => Transaction((c,t) =>
    {
        using (var lookup = Command(c,t,"SELECT character_id FROM character_state.reward_receipts WHERE operation_id=$1",operationId))
        {
            var characterId = lookup.ExecuteScalar() as string ?? throw new CharacterStoreRejectedException("Reward intent missing.");
            LockCharacter(c,t,characterId);
        }
        using var cmd = Command(c,t,"UPDATE character_state.reward_receipts SET state='Complete' WHERE operation_id=$1",operationId);
        if (cmd.ExecuteNonQuery()!=1) throw new CharacterStoreRejectedException("Reward intent missing.");
        return true;
    });
}

public sealed record RewardIntent(string OperationId,string CharacterId,string QuestId,string StepId,string BundleId,string ContainerId,int ContentRevision,IReadOnlyList<QuestRewardItem> Items);
