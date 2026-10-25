using Npgsql;
using ShatteredFrontier.Persistence;
using System.Text.RegularExpressions;

// Owner-only LAN development bootstrap. Never called by a runtime service.
internal static class PlayableBootstrap
{
    private static readonly (string Schema, string Base, int Version)[] Stores =
        [("inventory", "InventoryLedger", 3), ("character_state", "CharacterState", 10),
         ("housing", "Housing", 6), ("exchange", "Exchange", 3), ("group", "", 1)];
    private static string Owner(string schema) => "sf_" + schema + "_owner";
    private static string Service(string schema) => "sf_" + schema + "_svc";
    private static string Q(string name) => '"' + name + '"';
    private static string Password(string schema)
    {
        var value = Environment.GetEnvironmentVariable("SF_PLAYABLE_" + schema.ToUpperInvariant() + "_PASSWORD") ?? "";
        return Regex.IsMatch(value, "^[a-f0-9]{64}$") ? value : throw new InvalidOperationException("Private password absent.");
    }
    private static void Execute(NpgsqlConnection connection, string sql, NpgsqlTransaction? transaction = null)
    {
        using var command = new NpgsqlCommand(sql, connection, transaction);
        command.ExecuteNonQuery();
    }
    private static object? Scalar(NpgsqlConnection connection, string sql)
    {
        using var command = new NpgsqlCommand(sql, connection);
        return command.ExecuteScalar();
    }
    public static int Run(string mode)
    {
        var stage = "private-input";
        try
        {
            var raw = Environment.GetEnvironmentVariable("SF_PLAYABLE_ADMIN_CONNECTION") ?? throw new InvalidOperationException();
            var admin = new NpgsqlConnectionStringBuilder(raw) { Pooling = false, IncludeErrorDetail = false };
            if (admin.Host != "127.0.0.1" || admin.Port != 5432 || admin.Database != "sf_playable" || admin.Username != "sf_admin")
                throw new InvalidOperationException();
            foreach (var store in Stores) _ = Password(store.Schema);
            using var connection = new NpgsqlConnection(admin.ConnectionString);
            connection.Open();
            if (mode == "playable-init")
            {
                stage = "fresh-database";
                Execute(connection, "SELECT pg_advisory_lock(736485)");
                if (Convert.ToInt64(Scalar(connection, "SELECT count(*) FROM pg_namespace WHERE nspname NOT IN ('public','information_schema') AND nspname NOT LIKE 'pg_%'")) != 0 ||
                    Convert.ToInt64(Scalar(connection, "SELECT count(*) FROM pg_class c JOIN pg_namespace n ON n.oid=c.relnamespace WHERE n.nspname='public'")) != 0)
                    throw new InvalidOperationException();
                using (var transaction = connection.BeginTransaction())
                {
                    Execute(connection, "REVOKE ALL ON DATABASE sf_playable FROM PUBLIC; REVOKE ALL ON SCHEMA public FROM PUBLIC;", transaction);
                    foreach (var store in Stores)
                    {
                        // All identifiers are fixed code; the generated private passwords are hex only.
                        Execute(connection, $"CREATE ROLE {Owner(store.Schema)} NOLOGIN NOSUPERUSER NOCREATEDB NOCREATEROLE NOREPLICATION NOBYPASSRLS; " +
                            $"GRANT CREATE ON DATABASE sf_playable TO {Owner(store.Schema)}; " +
                            $"CREATE ROLE {Service(store.Schema)} LOGIN NOSUPERUSER NOCREATEDB NOCREATEROLE NOINHERIT NOREPLICATION NOBYPASSRLS PASSWORD '{Password(store.Schema)}'; " +
                            $"GRANT CONNECT ON DATABASE sf_playable TO {Service(store.Schema)}; CREATE SCHEMA {Q(store.Schema)} AUTHORIZATION {Owner(store.Schema)};", transaction);
                        if (store.Base.Length != 0)
                        {
                            Execute(connection, $"SET LOCAL ROLE {Owner(store.Schema)}", transaction);
                            Execute(connection, File.ReadAllText($"/opt/services/base-schema/{store.Base}.sql"), transaction);
                            Execute(connection, "RESET ROLE", transaction);
                        }
                    }
                    transaction.Commit();
                }
                stage = "owner-migrations";
                foreach (var store in Stores) SchemaMigration.Apply(admin.ConnectionString, store.Schema, Owner(store.Schema));
                stage = "runtime-grants";
                foreach (var store in Stores)
                {
                    Execute(connection, $"GRANT USAGE ON SCHEMA {Q(store.Schema)} TO {Service(store.Schema)}; " +
                        $"GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA {Q(store.Schema)} TO {Service(store.Schema)}; " +
                        $"GRANT USAGE,SELECT ON ALL SEQUENCES IN SCHEMA {Q(store.Schema)} TO {Service(store.Schema)}; " +
                        $"REVOKE INSERT,UPDATE,DELETE ON {Q(store.Schema)}.schema_version,{Q(store.Schema)}.applied_schema_files FROM {Service(store.Schema)};");
                }
                Execute(connection, "REVOKE INSERT,UPDATE,DELETE ON inventory.gathering_schema_version FROM sf_inventory_svc");
                // GearRepair's existing single transaction uses the inventory connection.
                Execute(connection, "GRANT USAGE ON SCHEMA exchange TO sf_inventory_svc; " +
                    "GRANT SELECT ON exchange.schema_version,exchange.settlements TO sf_inventory_svc; " +
                    "GRANT SELECT,INSERT,UPDATE ON exchange.wallets TO sf_inventory_svc; " +
                    "GRANT INSERT ON exchange.wallet_entries TO sf_inventory_svc; " +
                    "GRANT SELECT,INSERT ON exchange.operations TO sf_inventory_svc;");
            }
            stage = "runtime-verification";
            foreach (var store in Stores)
            {
                if (Scalar(connection, $"SELECT rolcanlogin OR rolsuper OR rolcreatedb OR rolcreaterole OR rolreplication OR rolbypassrls FROM pg_roles WHERE rolname='{Owner(store.Schema)}'") is not false)
                    throw new InvalidOperationException();
                var runtime = new NpgsqlConnectionStringBuilder(admin.ConnectionString) { Username = Service(store.Schema), Password = Password(store.Schema) };
                using var service = new NpgsqlConnection(runtime.ConnectionString);
                service.Open();
                if (Convert.ToInt32(Scalar(service, $"SELECT version FROM {Q(store.Schema)}.schema_version WHERE singleton=true")) != store.Version ||
                    Scalar(service, "SELECT rolsuper OR rolcreatedb OR rolcreaterole OR rolreplication OR rolbypassrls FROM pg_roles WHERE rolname=current_user") is not false ||
                    Scalar(service, "SELECT has_database_privilege(current_database(),'CREATE') OR has_schema_privilege('public','CREATE')") is not false ||
                    Scalar(service, $"SELECT has_schema_privilege('{store.Schema}','CREATE')") is not false ||
                    Scalar(service, $"SELECT has_table_privilege('{store.Schema}.schema_version','UPDATE') OR has_table_privilege('{store.Schema}.applied_schema_files','UPDATE')") is not false ||
                    Convert.ToInt64(Scalar(service, "SELECT count(*) FROM pg_auth_members m JOIN pg_roles r ON m.member=r.oid WHERE r.rolname=current_user")) != 0)
                    throw new InvalidOperationException();
                foreach (var other in Stores.Where(other => other.Schema != store.Schema && !(store.Schema == "inventory" && other.Schema == "exchange")))
                    if (Scalar(service, $"SELECT has_schema_privilege('{other.Schema}','USAGE')") is not false) throw new InvalidOperationException();
            }
            Console.WriteLine("PLAYABLE_DATABASE_VERIFIED schemas=5 runtime_roles=5 ddl=false owner_membership=false");
            return 0;
        }
        catch (Exception)
        {
            // Never emit connection strings, passwords, statements or provider exception text.
            Console.Error.WriteLine($"PLAYABLE_DATABASE_REFUSED stage={stage}");
            return 2;
        }
    }
}
