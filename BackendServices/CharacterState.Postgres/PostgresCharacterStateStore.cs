using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using Npgsql;
using ShatteredFrontier.Progression;
using ShatteredFrontier.QuestContent;

namespace ShatteredFrontier.CharacterState.Postgres;

public sealed class CharacterStoreUnavailableException(string message, Exception inner) : Exception(message, inner);
public sealed class CharacterSchemaCompatibilityException(string message) : Exception(message);
public sealed class CharacterStoreRejectedException(string message) : Exception(message);
public sealed record VersionedResult<T>(long Revision, T Result);

// Migration identity installs Schema.sql. Runtime uses DML and marker SELECT only.
public sealed class PostgresCharacterStateStore(string connectionString)
{
    public VersionedResult<ProgressionSnapshot>? ReadProgression(string characterId) => Transaction<VersionedResult<ProgressionSnapshot>?>((c, t) =>
    {
        using var cmd = Command(c, t, "SELECT revision,state::text FROM character_state.progression WHERE character_id=$1", characterId);
        using var r = cmd.ExecuteReader();
        return r.Read() ? new(r.GetInt64(0), JsonSerializer.Deserialize<ProgressionSnapshot>(r.GetString(1)) ?? throw new CharacterStoreRejectedException("Invalid stored progression state.")) : null;
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

    private T Transaction<T>(Func<NpgsqlConnection, NpgsqlTransaction, T> action)
    {
        try
        {
            using var c = new NpgsqlConnection(connectionString); c.Open();
            using var t = c.BeginTransaction();
            using (var l = Command(c, t, "SELECT pg_advisory_xact_lock(519761, 2)")) l.ExecuteNonQuery();
            try
            {
                using var marker = Command(c, t, "SELECT version,migration_id,compatible_build_min,compatible_build_max FROM character_state.schema_version WHERE singleton=true FOR SHARE");
                using var row = marker.ExecuteReader();
                if (!row.Read() || row.GetInt32(0) != 1 || row.GetString(1) != "character-state-001" || row.GetInt32(2) > 1 || row.GetInt32(3) < 1)
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

    public long AcceptQuest(QuestDefinition quest, string characterId, DateTimeOffset acceptedAt)
    {
        if (string.IsNullOrWhiteSpace(characterId)) throw new ArgumentException("Character ID required.");
        return Transaction((c, t) =>
        {
            var model = new QuestProgression(quest); model.Accept(characterId, acceptedAt);
            using var cmd = Command(c, t, "INSERT INTO character_state.quests VALUES ($1,$2,$3,0,$4::jsonb) ON CONFLICT DO NOTHING", characterId, quest.Id, quest.Revision, JsonSerializer.Serialize(model.Snapshot(characterId)));
            cmd.ExecuteNonQuery();
            using var read = Command(c, t, "SELECT content_revision,revision FROM character_state.quests WHERE character_id=$1 AND quest_id=$2", characterId, quest.Id);
            using var r = read.ExecuteReader(); r.Read();
            if (r.GetInt32(0) != quest.Revision) throw new CharacterStoreRejectedException("Quest content revision changed.");
            return r.GetInt64(1);
        });
    }

    public VersionedResult<ProgressResult> ApplyQuest(QuestDefinition quest, ObjectiveEvent evidence, long expectedRevision)
    {
        if (string.IsNullOrWhiteSpace(evidence.OperationId) || string.IsNullOrWhiteSpace(evidence.CharacterId) || evidence.QuestId != quest.Id)
            throw new ArgumentException("Quest evidence identity invalid.");
        return Transaction((c, t) =>
        {
            string hash = Fingerprint(evidence);
            var replay = Replay<ProgressResult>(c, t, "quest", evidence.CharacterId, evidence.OperationId, hash);
            if (replay is not null) return replay;
            long revision; QuestProgressSnapshot snapshot;
            using (var cmd = Command(c, t, "SELECT content_revision,revision,state::text FROM character_state.quests WHERE character_id=$1 AND quest_id=$2", evidence.CharacterId, quest.Id))
            using (var r = cmd.ExecuteReader())
            {
                if (!r.Read()) throw new CharacterStoreRejectedException("Quest not accepted.");
                if (r.GetInt32(0) != quest.Revision) throw new CharacterStoreRejectedException("Quest content revision changed.");
                revision = r.GetInt64(1);
                snapshot = JsonSerializer.Deserialize<QuestProgressSnapshot>(r.GetString(2)) ?? throw new CharacterStoreRejectedException("Invalid stored quest state.");
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
                    using var receipt = Command(c, t, "INSERT INTO character_state.reward_receipts VALUES ($1,$2,$3,$4,$5)", grant.OperationId, grant.CharacterId, grant.QuestId, grant.StepId, grant.BundleId);
                    receipt.ExecuteNonQuery();
                }
                revision++;
                Receipt(c, t, "quest", evidence.CharacterId, evidence.OperationId, hash, new VersionedResult<ProgressResult>(revision, result));
            }
            return new VersionedResult<ProgressResult>(revision, result);
        });
    }
}
