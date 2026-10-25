using ShatteredFrontier.Persistence;

if (args is ["playable-init"] or ["playable-verify"]) return PlayableBootstrap.Run(args[0]);

if (args.Length == 0) { Console.Error.WriteLine("Usage: SchemaMigration.Tool [--role schema_owner] inventory|character_state|housing|exchange|group|chat|guild ... (SF_MIGRATION_CONNECTION required)"); return 2; }
var connection = Environment.GetEnvironmentVariable("SF_MIGRATION_CONNECTION")
    ?? throw new InvalidOperationException("SF_MIGRATION_CONNECTION is required.");
var index = 0;
string? role = null;
if (args[0] == "--role")
{
    if (args.Length < 3) throw new ArgumentException("--role requires an owner role and at least one schema.");
    role = args[1];
    index = 2;
}
foreach (var schema in args[index..]) SchemaMigration.Apply(connection, schema, role);
return 0;
