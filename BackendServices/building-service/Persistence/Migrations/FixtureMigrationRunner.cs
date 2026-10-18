using System.Net;
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Text.RegularExpressions;
using Npgsql;

namespace ShatteredFrontier.Building.Migrations;

// A disposable fixture runner, deliberately not a service startup migration path.
public sealed class FixtureMigrationRunner
{
    const long SessionLock = 73406001;
    const long TransactionLock = 73406002;
    const string Profile = "cmig-v1";
    static readonly JsonSerializerOptions Json = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        PropertyNameCaseInsensitive = false,
        UnmappedMemberHandling = JsonUnmappedMemberHandling.Disallow
    };
    readonly NpgsqlConnection connection;
    readonly Manifest manifest;
    readonly Dictionary<string, string> scripts;
    readonly string? hookDirectory;
    readonly string? failAt;

    FixtureMigrationRunner(NpgsqlConnection connection, Manifest manifest,
        Dictionary<string, string> scripts, string? hookDirectory, string? failAt)
    {
        this.connection = connection;
        this.manifest = manifest;
        this.scripts = scripts;
        this.hookDirectory = hookDirectory;
        this.failAt = failAt;
    }

    public static async Task<int> RunAsync(string[] args)
    {
        try
        {
            var options = Parse(args);
            var (manifest, scripts) = await LoadAsync(options["--manifest"]);
            var id = Guid.Parse(options["--fixture-id"]);
            var secret = Environment.GetEnvironmentVariable(options["--connection-env"])
                ?? throw new InvalidOperationException("connection_env_missing");
            var builder = new NpgsqlConnectionStringBuilder(secret);
            Require(IPAddress.TryParse(builder.Host, out var address) && IPAddress.IsLoopback(address), "loopback_literal_required");
            Require(builder.Database == "h2mig_" + id.ToString("N"), "database_identity_mismatch");
            Require(builder.Username == "mig_building", "migration_login_required");
            Require(string.IsNullOrEmpty(builder.Options), "connection_options_forbidden");
            builder.Pooling = false;
            builder.Multiplexing = false;
            builder.NoResetOnClose = false;
            builder.IncludeErrorDetail = false;
            if (string.IsNullOrWhiteSpace(builder.ApplicationName)) builder.ApplicationName = "h2mig-" + id.ToString("N");
            builder.Timeout = 10;
            builder.CommandTimeout = 65;
            await using var connection = new NpgsqlConnection(builder.ConnectionString);
            await connection.OpenAsync();
            var runner = new FixtureMigrationRunner(connection, manifest, scripts,
                options.GetValueOrDefault("--hook-dir"), options.GetValueOrDefault("--fail-at"));
            await runner.GuardAsync(id, options["--system-identifier"]);
            await runner.ExecuteAsync("SET search_path = pg_catalog; SET lock_timeout = '2s'; SET statement_timeout = '60s'; SET ROLE own_building;");
            Require(await runner.ScalarAsync<string>("SHOW lock_timeout") == "2s" &&
                await runner.ScalarAsync<string>("SHOW statement_timeout") == "1min", "timeouts_not_effective");
            if (!await runner.ScalarAsync<bool>($"SELECT pg_catalog.pg_try_advisory_lock({SessionLock})"))
            {
                Emit("busy");
                return 10;
            }
            Emit("session_locked", new { backendPid = connection.ProcessID, lockTimeout = "2s", statementTimeout = "60s" });
            await runner.HookAsync("session_locked");
            if (options["command"] == "check")
            {
                Require(await runner.HistoryExistsAsync(), "schema_not_ready");
                var applied = await runner.ValidatePrefixAsync();
                Require(applied == manifest.Migrations.Length, "schema_not_ready");
                await runner.VerifyCatalogAsync(applied);
                Emit("ready", new { schemaVersion = applied, profile = Profile });
                return 0;
            }
            await runner.BootstrapAsync();
            var prefix = await runner.ValidatePrefixAsync();
            await runner.VerifyCatalogAsync(prefix);
            foreach (var migration in manifest.Migrations.Skip(prefix))
                await runner.ApplyAsync(migration);
            await runner.VerifyCatalogAsync(manifest.Migrations.Length);
            Emit(prefix == manifest.Migrations.Length ? "already_applied" : "applied",
                new { schemaVersion = manifest.Migrations.Length, profile = Profile });
            return 0;
        }
        catch (Exception exception)
        {
            // Never write connection strings, SQL text, server error detail or exception messages.
            Emit("refused_or_failed", new { error = SafeError(exception), sqlstate = (exception as PostgresException)?.SqlState });
            return 20;
        }
    }

    static Dictionary<string, string> Parse(string[] args)
    {
        Require(args.Length >= 1 && args[0] is "migrate" or "check", "command_required");
        var result = new Dictionary<string, string> { ["command"] = args[0] };
        var allowed = new[] { "--connection-env", "--manifest", "--fixture-id", "--hook-dir", "--fail-at", "--system-identifier" };
        for (var i = 1; i < args.Length; i += 2)
        {
            Require(i + 1 < args.Length && allowed.Contains(args[i]) && !result.ContainsKey(args[i]), "invalid_arguments");
            result.Add(args[i], args[i + 1]);
        }
        foreach (var required in new[] { "--connection-env", "--manifest", "--fixture-id", "--system-identifier" })
            Require(result.ContainsKey(required), "missing_argument");
        Require(Regex.IsMatch(result["--connection-env"], "^[A-Za-z_][A-Za-z0-9_]*$"), "invalid_environment_name");
        Require(Regex.IsMatch(result["--system-identifier"], "^[0-9]{1,20}$"), "invalid_system_identifier");
        return result;
    }

    static async Task<(Manifest, Dictionary<string, string>)> LoadAsync(string path)
    {
        var file = Path.GetFullPath(path);
        var manifest = JsonSerializer.Deserialize<Manifest>(await File.ReadAllBytesAsync(file), Json)
            ?? throw new InvalidOperationException("invalid_manifest");
        Require(manifest.SchemaVersion == 1 && manifest.Profile == Profile && manifest.SessionLock == SessionLock &&
            manifest.TransactionLock == TransactionLock && manifest.Migrations.Length == 3, "unsupported_manifest");
        var scripts = new Dictionary<string, string>();
        for (var i = 0; i < manifest.Migrations.Length; i++)
        {
            var migration = manifest.Migrations[i];
            var expectedId = (i + 1).ToString("D4");
            Require(migration.Id == expectedId && migration.From == i && migration.To == i + 1 &&
                migration.Mode == (i == 2 ? "concurrent-index" : "transaction") &&
                Regex.IsMatch(migration.File, "^" + expectedId + "_[a-z_]+\\.sql$") &&
                Regex.IsMatch(migration.Sha256, "^[a-f0-9]{64}$"), "invalid_migration_order_or_profile");
            if (i == 2) Require(migration.Index is { Table: "fixture_items", Name: "fixture_items_label_idx", Columns: ["label"] }, "unsupported_index_specification");
            else Require(migration.Index is null, "unexpected_index_specification");
            var bytes = await File.ReadAllBytesAsync(Path.Combine(Path.GetDirectoryName(file)!, migration.File));
            Require(!bytes.Contains((byte)'\r'), "migration_requires_lf");
            Require(Convert.ToHexStringLower(SHA256.HashData(bytes)) == migration.Sha256, "file_checksum_drift");
            scripts.Add(migration.Id, System.Text.Encoding.UTF8.GetString(bytes));
        }
        return (manifest, scripts);
    }

    async Task GuardAsync(Guid id, string expectedSystemIdentifier)
    {
        Require(await ScalarAsync<string>("SELECT current_database()") == "h2mig_" + id.ToString("N"), "connected_database_mismatch");
        Require(await ScalarAsync<string>("SELECT session_user::text") == "mig_building", "connected_login_mismatch");
        Require(!await ScalarAsync<bool>("SELECT rolsuper OR rolcreatedb OR rolcreaterole OR rolreplication OR rolbypassrls FROM pg_catalog.pg_roles WHERE rolname = session_user"), "elevated_migration_login");
        Require(!await ScalarAsync<bool>("SELECT pg_catalog.has_schema_privilege(session_user,'public','CREATE')"), "public_create_forbidden");
        await using var cmd = new NpgsqlCommand("SELECT id, system_identifier FROM fixture_control.identity", connection);
        await using var reader = await cmd.ExecuteReaderAsync();
        Require(await reader.ReadAsync() && reader.GetGuid(0) == id && !reader.IsDBNull(1), "fixture_marker_mismatch");
        Require(expectedSystemIdentifier == reader.GetString(1), "server_marker_mismatch");
        Require(!await reader.ReadAsync(), "fixture_marker_not_unique");
        await reader.CloseAsync();
        Require(await ScalarAsync<string>("SELECT fixture_control.server_identity()") == expectedSystemIdentifier, "actual_server_identity_mismatch");
    }

    async Task<bool> HistoryExistsAsync()
    {
        var history = await ScalarAsync<bool>("SELECT pg_catalog.to_regclass('building.schema_migrations') IS NOT NULL");
        var attempts = await ScalarAsync<bool>("SELECT pg_catalog.to_regclass('building.schema_migration_attempts') IS NOT NULL");
        Require(history == attempts, "partial_history_bootstrap");
        return history;
    }

    async Task BootstrapAsync()
    {
        if (await HistoryExistsAsync()) return;
        await using var tx = await connection.BeginTransactionAsync();
        await ExecuteAsync($"SELECT pg_catalog.pg_advisory_xact_lock({TransactionLock})", tx);
        await ExecuteAsync("""
            CREATE TABLE building.schema_migrations (
                id text PRIMARY KEY, sha256 text NOT NULL, mode text NOT NULL,
                profile text NOT NULL, result_version integer NOT NULL UNIQUE,
                applied_at timestamptz NOT NULL DEFAULT clock_timestamp()
            );
            CREATE TABLE building.schema_migration_attempts (
                event_seq bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
                attempt_id uuid NOT NULL, migration_id text NOT NULL, sha256 text NOT NULL,
                profile text NOT NULL, mode text NOT NULL, state text NOT NULL,
                backend_pid integer NOT NULL, sqlstate text, detail text,
                observed jsonb, created_at timestamptz NOT NULL DEFAULT clock_timestamp()
            );
            CREATE FUNCTION building.reject_migration_metadata_change() RETURNS trigger
            LANGUAGE plpgsql SET search_path = pg_catalog AS $fn$
            BEGIN RAISE EXCEPTION 'immutable migration metadata' USING ERRCODE = '55000'; END;
            $fn$;
            CREATE TRIGGER immutable_migrations BEFORE UPDATE OR DELETE OR TRUNCATE
                ON building.schema_migrations FOR EACH STATEMENT
                EXECUTE FUNCTION building.reject_migration_metadata_change();
            CREATE TRIGGER immutable_attempts BEFORE UPDATE OR DELETE OR TRUNCATE
                ON building.schema_migration_attempts FOR EACH STATEMENT
                EXECUTE FUNCTION building.reject_migration_metadata_change();
            REVOKE ALL ON building.schema_migrations, building.schema_migration_attempts FROM svc_building;
            REVOKE ALL ON SEQUENCE building.schema_migration_attempts_event_seq_seq FROM svc_building, ro_building;
            GRANT SELECT ON building.schema_migrations, building.schema_migration_attempts TO ro_building;
            """, tx);
        await tx.CommitAsync();
    }

    async Task<int> ValidatePrefixAsync()
    {
        await ValidateAttemptIdentityAsync();
        var count = 0;
        await using var cmd = new NpgsqlCommand("SELECT id,sha256,mode,profile,result_version FROM building.schema_migrations ORDER BY id", connection);
        await using var reader = await cmd.ExecuteReaderAsync();
        while (await reader.ReadAsync())
        {
            Require(count < manifest.Migrations.Length, "unknown_migration_history");
            var expected = manifest.Migrations[count];
            Require(reader.GetString(0) == expected.Id && reader.GetString(1) == expected.Sha256 &&
                reader.GetString(2) == expected.Mode && reader.GetString(3) == Profile && reader.GetInt32(4) == expected.To,
                "migration_history_drift_or_gap");
            count++;
        }
        return count;
    }

    async Task ValidateAttemptIdentityAsync()
    {
        await using var cmd = new NpgsqlCommand("SELECT DISTINCT migration_id,sha256,mode,profile FROM building.schema_migration_attempts", connection);
        await using var reader = await cmd.ExecuteReaderAsync();
        while (await reader.ReadAsync())
        {
            var migration = manifest.Migrations.SingleOrDefault(value => value.Id == reader.GetString(0));
            Require(migration is not null && migration.Sha256 == reader.GetString(1) && migration.Mode == reader.GetString(2) &&
                reader.GetString(3) == Profile, "attempt_identity_drift");
        }
    }

    async Task ApplyAsync(Migration migration)
    {
        var attempt = Guid.NewGuid();
        if (migration.Mode == "concurrent-index" && await IndexStateAsync() != "absent")
            Require(await ScalarAsync<bool>("SELECT EXISTS(SELECT 1 FROM building.schema_migration_attempts WHERE migration_id='0003')"), "untracked_index_refused");
        try
        {
            await AttemptAsync(attempt, migration, "pending");
            await AttemptAsync(attempt, migration, "running");
            if (migration.Mode == "transaction")
            {
                await using var tx = await connection.BeginTransactionAsync();
                await ExecuteAsync($"SELECT pg_catalog.pg_advisory_xact_lock({TransactionLock})", tx);
                await ExecuteAsync(scripts[migration.Id], tx);
                await RecordCompleteAsync(migration, tx);
                await AttemptAsync(attempt, migration, "complete", tx: tx);
                await HookAsync("transaction_before_commit");
                await tx.CommitAsync();
                await HookAsync("transaction_after_commit");
            }
            else await ApplyIndexAsync(migration, attempt);
            Emit("migration_complete", new { migration.Id, attempt });
        }
        catch (Exception exception)
        {
            if (connection.State == System.Data.ConnectionState.Open)
            {
                try { await AttemptAsync(attempt, migration, "failed", sqlstate: (exception as PostgresException)?.SqlState, detail: SafeError(exception)); }
                catch { /* Lost connection leaves durable running; never reconnect. */ }
            }
            throw;
        }
    }

    async Task ApplyIndexAsync(Migration migration, Guid attempt)
    {
        await HookAsync("index_running");
        var state = await IndexStateAsync();
        if (state == "invalid")
        {
            await AttemptAsync(attempt, migration, "cleanup_pending", observed: state);
            await HookAsync("index_cleanup_pending");
            await ExecuteAsync("DROP INDEX CONCURRENTLY building.fixture_items_label_idx");
            Require(await IndexStateAsync() == "absent", "index_cleanup_not_absent");
            await HookAsync("index_dropped");
            await AttemptAsync(attempt, migration, "pending", observed: "absent");
            state = "absent";
        }
        if (state == "absent")
        {
            await AttemptAsync(attempt, migration, "running", observed: "absent");
            await ExecuteAsync(scripts[migration.Id]);
        }
        Require(await IndexStateAsync() == "valid", "index_not_valid");
        await HookAsync("index_created");
        await using var tx = await connection.BeginTransactionAsync();
        await ExecuteAsync($"SELECT pg_catalog.pg_advisory_xact_lock({TransactionLock})", tx);
        await RecordCompleteAsync(migration, tx);
        await AttemptAsync(attempt, migration, "complete", observed: "valid", tx: tx);
        await tx.CommitAsync();
    }

    async Task RecordCompleteAsync(Migration migration, NpgsqlTransaction tx)
    {
        await using var cmd = new NpgsqlCommand("INSERT INTO building.schema_migrations(id,sha256,mode,profile,result_version) VALUES($1,$2,$3,$4,$5)", connection, tx);
        cmd.Parameters.AddWithValue(migration.Id);
        cmd.Parameters.AddWithValue(migration.Sha256);
        cmd.Parameters.AddWithValue(migration.Mode);
        cmd.Parameters.AddWithValue(Profile);
        cmd.Parameters.AddWithValue(migration.To);
        await cmd.ExecuteNonQueryAsync();
    }

    async Task AttemptAsync(Guid attempt, Migration migration, string state, string? sqlstate = null,
        string? detail = null, string? observed = null, NpgsqlTransaction? tx = null)
    {
        await using var cmd = new NpgsqlCommand("""
            INSERT INTO building.schema_migration_attempts
                (attempt_id,migration_id,sha256,profile,mode,state,backend_pid,sqlstate,detail,observed)
            VALUES($1,$2,$3,$4,$5,$6,pg_backend_pid(),$7,$8,$9::jsonb)
            """, connection, tx);
        cmd.Parameters.AddWithValue(attempt);
        cmd.Parameters.AddWithValue(migration.Id);
        cmd.Parameters.AddWithValue(migration.Sha256);
        cmd.Parameters.AddWithValue(Profile);
        cmd.Parameters.AddWithValue(migration.Mode);
        cmd.Parameters.AddWithValue(state);
        cmd.Parameters.Add(new NpgsqlParameter { NpgsqlDbType = NpgsqlTypes.NpgsqlDbType.Text, Value = (object?)sqlstate ?? DBNull.Value });
        cmd.Parameters.Add(new NpgsqlParameter { NpgsqlDbType = NpgsqlTypes.NpgsqlDbType.Text, Value = (object?)detail ?? DBNull.Value });
        cmd.Parameters.AddWithValue(JsonSerializer.Serialize(new { indexState = observed }));
        await cmd.ExecuteNonQueryAsync();
        Emit("attempt", new { migration.Id, attempt, state, sqlstate, observed });
    }

    async Task VerifyCatalogAsync(int version)
    {
        if (version == 0)
        {
            Require(!await ScalarAsync<bool>("SELECT pg_catalog.to_regclass('building.fixture_items') IS NOT NULL"), "unexpected_untracked_fixture_table");
            return;
        }
        Require(await ScalarAsync<bool>("""
            SELECT EXISTS(SELECT 1 FROM pg_catalog.pg_class c JOIN pg_catalog.pg_namespace n ON n.oid=c.relnamespace
                WHERE n.nspname='building' AND c.relname='fixture_items' AND c.relkind='r'
                AND c.relowner='own_building'::regrole AND c.relpersistence='p' AND NOT c.relrowsecurity
                AND NOT EXISTS(SELECT 1 FROM pg_catalog.pg_inherits WHERE inhrelid=c.oid OR inhparent=c.oid))
            """), "fixture_table_drift");
        await using (var cmd = new NpgsqlCommand("""
            SELECT attname,format_type(atttypid,atttypmod),attnotnull,attidentity::text
            FROM pg_catalog.pg_attribute WHERE attrelid='building.fixture_items'::regclass
                AND attnum>0 AND NOT attisdropped ORDER BY attnum
            """, connection))
        await using (var reader = await cmd.ExecuteReaderAsync())
        {
            var expected = version == 1 ? new[] { "id", "label" } : new[] { "id", "label", "note" };
            foreach (var column in expected)
            {
                Require(await reader.ReadAsync() && reader.GetString(0) == column &&
                    reader.GetString(1) == (column == "id" ? "bigint" : "text") &&
                    reader.GetBoolean(2) == (column != "note") && reader.GetString(3) == (column == "id" ? "a" : ""), "fixture_column_drift");
            }
            Require(!await reader.ReadAsync(), "unexpected_fixture_column");
        }
        Require(await ScalarAsync<bool>("""
            SELECT EXISTS(
                SELECT 1 FROM pg_catalog.pg_constraint p
                JOIN pg_catalog.pg_index i ON i.indexrelid=p.conindid
                JOIN pg_catalog.pg_class c ON c.oid=i.indexrelid
                WHERE p.conrelid='building.fixture_items'::regclass AND p.contype='p'
                AND p.conname='fixture_items_pkey' AND p.conkey=ARRAY[1]::smallint[]
                AND p.convalidated AND NOT p.condeferrable AND NOT p.condeferred
                AND c.relowner='own_building'::regrole AND c.relkind='i'
                AND i.indisvalid AND i.indisready AND i.indisunique AND i.indisprimary
                AND i.indnkeyatts=1 AND i.indnatts=1 AND pg_get_indexdef(c.oid,1,true)='id')
            """), "fixture_primary_key_drift");
        Require(await ScalarAsync<bool>("""
            SELECT EXISTS(
                SELECT 1 FROM pg_catalog.pg_class s
                JOIN pg_catalog.pg_namespace n ON n.oid=s.relnamespace
                JOIN pg_catalog.pg_sequence q ON q.seqrelid=s.oid
                WHERE n.nspname='building' AND s.relname='fixture_items_id_seq' AND s.relkind='S'
                AND s.relowner='own_building'::regrole AND s.relpersistence='p'
                AND pg_get_serial_sequence('building.fixture_items','id')::regclass=s.oid
                AND q.seqtypid='int8'::regtype AND q.seqstart=1 AND q.seqincrement=1
                AND q.seqmin=1 AND q.seqmax=9223372036854775807 AND q.seqcache=1 AND NOT q.seqcycle
                AND EXISTS(SELECT 1 FROM pg_catalog.pg_depend d WHERE d.classid='pg_class'::regclass
                    AND d.objid=s.oid AND d.refclassid='pg_class'::regclass
                    AND d.refobjid='building.fixture_items'::regclass AND d.refobjsubid=1 AND d.deptype='i'))
            """), "fixture_identity_sequence_drift");
        if (version >= 3) Require(await IndexStateAsync() == "valid", "completed_index_drift");
    }

    async Task<string> IndexStateAsync()
    {
        await using var cmd = new NpgsqlCommand("""
            SELECT c.relkind::text,c.relowner='own_building'::regrole,c.relpersistence::text,c.reloptions IS NULL,
                i.indisvalid,i.indisready,i.indisunique,i.indisprimary,i.indisexclusion,i.indnullsnotdistinct,
                i.indnkeyatts,i.indnatts,i.indexprs IS NULL,i.indpred IS NULL,a.amname,
                i.indrelid='building.fixture_items'::regclass,pg_get_indexdef(c.oid,1,true),i.indoption::text,
                NOT EXISTS(SELECT 1 FROM pg_catalog.pg_constraint WHERE conindid=c.oid),
                NOT EXISTS(SELECT 1 FROM pg_catalog.pg_depend WHERE classid='pg_class'::regclass AND objid=c.oid AND deptype='e'),
                i.indcollation[0]=(SELECT attcollation FROM pg_catalog.pg_attribute WHERE attrelid=i.indrelid AND attname='label'),
                (SELECT opcname='text_ops' FROM pg_catalog.pg_opclass WHERE oid=i.indclass[0])
            FROM pg_catalog.pg_class c JOIN pg_catalog.pg_namespace n ON n.oid=c.relnamespace
            LEFT JOIN pg_catalog.pg_index i ON i.indexrelid=c.oid
            LEFT JOIN pg_catalog.pg_am a ON a.oid=c.relam
            WHERE n.nspname='building' AND c.relname='fixture_items_label_idx'
            """, connection);
        await using var reader = await cmd.ExecuteReaderAsync();
        if (!await reader.ReadAsync()) return "absent";
        Require(reader.GetString(0) == "i" && reader.GetBoolean(1) && reader.GetString(2) == "p" && reader.GetBoolean(3), "conflicting_index_relation");
        Require(!reader.GetBoolean(6) && !reader.GetBoolean(7) && !reader.GetBoolean(8) && !reader.GetBoolean(9) &&
            reader.GetInt16(10) == 1 && reader.GetInt16(11) == 1 && reader.GetBoolean(12) && reader.GetBoolean(13) &&
            reader.GetString(14) == "btree" && reader.GetBoolean(15) && reader.GetString(16) == "label" &&
            reader.GetString(17) == "0" && reader.GetBoolean(18) && reader.GetBoolean(19) && reader.GetBoolean(20) && reader.GetBoolean(21),
            "conflicting_index_definition");
        return reader.GetBoolean(4) && reader.GetBoolean(5) ? "valid" : "invalid";
    }

    async Task HookAsync(string name)
    {
        if (hookDirectory is not null && File.Exists(Path.Combine(hookDirectory, name + ".arm")))
        {
            await File.WriteAllTextAsync(Path.Combine(hookDirectory, name + ".ready"), JsonSerializer.Serialize(new { name, backendPid = connection.ProcessID }, Json));
            Emit("barrier", new { name, backendPid = connection.ProcessID });
            using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(90));
            while (!File.Exists(Path.Combine(hookDirectory, name + ".release")))
                await Task.Delay(25, timeout.Token);
        }
        if (failAt == name) throw new InvalidOperationException("injected_failure_" + name);
    }

    async Task ExecuteAsync(string sql, NpgsqlTransaction? tx = null)
    {
        await using var cmd = new NpgsqlCommand(sql, connection, tx);
        await cmd.ExecuteNonQueryAsync();
    }

    async Task<T> ScalarAsync<T>(string sql)
    {
        await using var cmd = new NpgsqlCommand(sql, connection);
        return (T)(await cmd.ExecuteScalarAsync() ?? throw new InvalidOperationException("missing_query_result"));
    }

    static void Require(bool condition, string reason)
    {
        if (!condition) throw new InvalidOperationException(reason);
    }

    static string SafeError(Exception exception) => exception is InvalidOperationException &&
        Regex.IsMatch(exception.Message, "^[a-z_]+$") ? exception.Message : exception.GetType().Name;

    static void Emit(string name, object? data = null) => Console.WriteLine(JsonSerializer.Serialize(new { @event = name, data }, Json));
}

public sealed record Manifest(int SchemaVersion, string Profile, long SessionLock, long TransactionLock, Migration[] Migrations);
public sealed record Migration(string Id, string File, string Sha256, string Mode, int From, int To, IndexSpec? Index);
public sealed record IndexSpec(string Table, string Name, string[] Columns);
