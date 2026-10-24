using System.Security.Cryptography;
using Npgsql;

namespace ShatteredFrontier.Persistence;

// Invoked only by the privileged migration command, before service startup.
public static class SchemaMigration
{
    public static void Apply(string connectionString, string schema, string? role = null)
    {
        var steps = schema switch
        {
            "inventory" => new (string File, string Installed)[] {
                ("Schema.Outbox.sql", "SELECT version>=2 FROM inventory.schema_version WHERE singleton=true"),
                ("Schema.GatheringNodes.sql", "SELECT to_regclass('inventory.gathering_schema_version') IS NOT NULL"),
                ("Schema.DefeatRewards.sql", "SELECT to_regclass('inventory.defeat_reward_intents') IS NOT NULL") },
            "character_state" => new (string File, string Installed)[] {
                ("Schema.RewardSnapshot.sql", "SELECT version>=2 FROM character_state.schema_version WHERE singleton=true"),
                ("Schema.Reward.sql", "SELECT EXISTS (SELECT 1 FROM information_schema.columns WHERE table_schema='character_state' AND table_name='reward_receipts' AND column_name='state')"),
                ("Schema.Position.sql", "SELECT version>=3 FROM character_state.schema_version WHERE singleton=true"),
                ("Schema.Recipes.sql", "SELECT version>=4 FROM character_state.schema_version WHERE singleton=true"),
                ("Schema.Appearance.sql", "SELECT version>=5 FROM character_state.schema_version WHERE singleton=true"),
                ("Schema.GatheringSkills.sql", "SELECT version>=6 FROM character_state.schema_version WHERE singleton=true"),
                ("Schema.Death.sql", "SELECT version>=7 FROM character_state.schema_version WHERE singleton=true") },
            "housing" => new (string File, string Installed)[] {
                ("Schema.Gameplay.sql", "SELECT version>=4 FROM housing.schema_version WHERE singleton=true"),
                ("Schema.Geometry.sql", "SELECT version>=5 FROM housing.schema_version WHERE singleton=true"),
                ("Schema.Construction.sql", "SELECT version>=6 FROM housing.schema_version WHERE singleton=true") },
            "exchange" => new (string File, string Installed)[] {
                ("Schema.Reads.sql", "SELECT to_regclass('exchange.exchange_active_market_page') IS NOT NULL AND to_regclass('exchange.exchange_pending_trade_a_page') IS NOT NULL") },
            "identity" => new (string File, string Installed)[] {
                ("Schema.sql", "SELECT to_regclass('identity.schema_version') IS NOT NULL") },
            _ => throw new ArgumentException($"Unknown schema: {schema}")
        };
        if (role is not null && !System.Text.RegularExpressions.Regex.IsMatch(role, "^[a-z_][a-z0-9_]*$"))
            throw new ArgumentException("Invalid migration role.");
        var resources = typeof(SchemaMigration).Assembly.GetManifestResourceNames()
            .Where(n => n.StartsWith(schema + ".Schema.", StringComparison.Ordinal) && n.EndsWith(".sql", StringComparison.Ordinal))
            .ToDictionary(n => n[(schema.Length + 1)..], n => n, StringComparer.Ordinal);
        foreach (var (file, _) in steps)
            if (!resources.ContainsKey(file)) throw new InvalidOperationException($"Missing embedded schema file: {schema}/{file}");
        var ordered = steps.Concat(resources.Keys.Except(steps.Select(s => s.File), StringComparer.Ordinal)
            .Order(StringComparer.Ordinal).Select(file => (file, "")));
        using var connection = new NpgsqlConnection(connectionString);
        connection.Open();
        if (role is not null)
            using (var setRole = new NpgsqlCommand($"SET ROLE {role}", connection)) setRole.ExecuteNonQuery();
        foreach (var (file, installed) in ordered)
        {
            using var transaction = connection.BeginTransaction();
            using (var lockCommand = new NpgsqlCommand("SELECT pg_advisory_xact_lock(hashtext($1), 736481)", connection, transaction))
            {
                lockCommand.Parameters.AddWithValue(schema);
                lockCommand.ExecuteNonQuery();
            }
            // The identity slice bootstraps its own base schema under the owner role.
            if (schema == "identity" && file == "Schema.sql")
            {
                using var bootstrap = new NpgsqlCommand("CREATE SCHEMA IF NOT EXISTS identity", connection, transaction);
                bootstrap.ExecuteNonQuery();
            }
            using (var check = new NpgsqlCommand("SELECT to_regclass($1) IS NOT NULL", connection, transaction))
            {
                check.Parameters.AddWithValue(schema + ".schema_version");
                if (check.ExecuteScalar() is not true && !(schema == "identity" && file == "Schema.sql"))
                    throw new InvalidOperationException($"{schema} base schema is not installed.");
            }
            using (var create = new NpgsqlCommand($"CREATE TABLE IF NOT EXISTS {schema}.applied_schema_files (file_name text PRIMARY KEY, sha256 text NOT NULL, applied_at timestamptz NOT NULL DEFAULT now())", connection, transaction))
                create.ExecuteNonQuery();
            using var stream = typeof(SchemaMigration).Assembly.GetManifestResourceStream(resources[file])!;
            using var reader = new StreamReader(stream);
            var sql = reader.ReadToEnd();
            var checksum = Convert.ToHexString(SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(sql))).ToLowerInvariant();
            using (var recorded = new NpgsqlCommand($"SELECT sha256 FROM {schema}.applied_schema_files WHERE file_name=$1", connection, transaction))
            {
                recorded.Parameters.AddWithValue(file);
                if (recorded.ExecuteScalar() is string previous)
                {
                    if (previous != checksum) throw new InvalidOperationException($"Changed schema file: {file}.");
                    transaction.Commit();
                    Console.WriteLine($"SKIP {schema}/{file}");
                    continue;
                }
            }
            var alreadyInstalled = false;
            if (installed.Length > 0)
            {
                using var probe = new NpgsqlCommand(installed, connection, transaction);
                alreadyInstalled = probe.ExecuteScalar() is true;
            }
            if (!alreadyInstalled)
            {
                // Existing scripts own their checks; remove their transaction wrapper so the
                // advisory lock and history insert commit atomically with the DDL.
                sql = string.Join('\n', sql.Split('\n').Where(line =>
                    line.Trim() is not "BEGIN;" and not "COMMIT;"));
                using var apply = new NpgsqlCommand(sql, connection, transaction);
                apply.ExecuteNonQuery();
            }
            using var mark = new NpgsqlCommand($"INSERT INTO {schema}.applied_schema_files(file_name,sha256) VALUES ($1,$2)", connection, transaction);
            mark.Parameters.AddWithValue(file);
            mark.Parameters.AddWithValue(checksum);
            mark.ExecuteNonQuery();
            transaction.Commit();
            Console.WriteLine($"{(alreadyInstalled ? "RECORD" : "APPLY")} {schema}/{file}");
        }
    }
}
