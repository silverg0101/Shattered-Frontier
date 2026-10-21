using System.Text.Json;

namespace AuthorityDurable;

internal static class Program
{
    internal static readonly JsonSerializerOptions Json = new() { PropertyNameCaseInsensitive = true, WriteIndented = true };
    internal static async Task<int> Main(string[] args)
    {
        try
        {
            if (args.Length == 3 && args[0] == "proxy")
                return await SocketProxy.Run(int.Parse(args[1], System.Globalization.CultureInfo.InvariantCulture), args[2]);
            if (args.Length == 3 && args[0] == "driver")
            {
                var scenarios = new Scenarios(JsonSerializer.Deserialize<Config>(await File.ReadAllTextAsync(args[1]), Json)!);
                try { return await scenarios.Run(args[2]); }
                finally { await scenarios.Cleanup(); }
            }
            if (args.Length == 3 && args[0] == "worker")
            {
                using var lifetime = new Timer(_ => Environment.Exit(124), null, TimeSpan.FromSeconds(50), Timeout.InfiniteTimeSpan);
                var request = JsonSerializer.Deserialize<Request>(await File.ReadAllTextAsync(args[1]), Json)!;
                var store = new Store(Environment.GetEnvironmentVariable("AD_CONNECTION")!,
                    Environment.GetEnvironmentVariable("AD_FIXTURE_ID")!, Environment.GetEnvironmentVariable("AD_SYSTEM_ID")!,
                    Environment.GetEnvironmentVariable("AD_CONTROL_ROOT")!);
                Reply reply;
                try { reply = await store.Execute(request); }
                catch (Npgsql.PostgresException e) { reply = new Reply { Status = "SQL:" + e.SqlState }; }
                catch (RuleException e) { reply = new Reply { Status = e.Message }; }
                await File.WriteAllTextAsync(args[2], JsonSerializer.Serialize(reply, Json));
                return 0;
            }
            Console.Error.WriteLine("Use driver <private-config.json> <results.json> or worker <request.json> <reply.json>.");
            return 2;
        }
        catch (Exception e)
        {
            // Exception messages may include connection strings or server details. Emit type only.
            Console.Error.WriteLine("AuthorityDurable failed: " + e.GetType().Name);
            return 1;
        }
    }
}

internal sealed class RuleException(string message) : Exception(message);
internal sealed class Config
{
    public string FixtureId { get; set; } = "";
    public string Coordinator { get; set; } = "";
    public string Material { get; set; } = "";
    public string Terrain { get; set; } = "";
    public string CoordinatorReadOnly { get; set; } = "";
    public string MaterialReadOnly { get; set; } = "";
    public string TerrainReadOnly { get; set; } = "";
    public string SystemIdentifier { get; set; } = "";
    public string ControlRoot { get; set; } = "";
    public string AllowedRoot { get; set; } = "";
    public int UndecidedDeadlineSeconds { get; set; } = 15;
    public string Dotnet { get; set; } = "";
    public string DdlSha256 { get; set; } = "";
}
// Mutable participation summary is separate from immutable prepared/applied digests.
internal sealed record Evidence(string Participant, string State, string Prepared, string Applied, string Completed, string Decision,
    string ActionId = "", string Realm = "", string Origin = "", string Hash = "", string Profile = "", string FixedSet = "",
    string Incarnation = "", long Generation = 0, string Worker = "");
internal sealed class Lookup
{
    public string Issuer { get; set; } = "trusted-stage-s-building-oracle";
    public string Id { get; set; } = "";
    public string Realm { get; set; } = "synthetic-realm";
    public string Origin { get; set; } = "origin-1";
    public string Hash { get; set; } = "";
    public string Profile { get; set; } = "stage-s-v1";
    public string Incarnation { get; set; } = "origin-1";
    public long Generation { get; set; } = 1;
    public string Worker { get; set; } = "worker-1";
    public string Participant { get; set; } = "";
    public string Phase { get; set; } = "";
    public string Payload { get; set; } = "";
    public string FixedSet { get; set; } = "geometry,material,terrain";
    public string Decision { get; set; } = "";
    public string DecisionDigest { get; set; } = "";
    public string Completion { get; set; } = "";
    public bool Recovering { get; set; }
    public List<Evidence> Evidence { get; set; } = [];
}
internal sealed class Request
{
    public string Operation { get; set; } = "";
    public int DeadlineSeconds { get; set; }
    public string Id { get; set; } = "";
    public string Owner { get; set; } = "";
    public Lookup? Lookup { get; set; }
    public string Value { get; set; } = "";
    public string Hook { get; set; } = "";
    public string HookId { get; set; } = "";
    public bool Rollback { get; set; }
    public long Generation { get; set; } = 1;
    public string Worker { get; set; } = "worker-1";
    public string Incarnation { get; set; } = "origin-1";
    public string Manifest { get; set; } = "";
    public string PreviousWorker { get; set; } = "worker-1";
    public string Key { get; set; } = "";
    public List<Evidence> Evidence { get; set; } = [];
}
internal sealed class Reply
{
    public string Status { get; set; } = "OK";
    public Lookup? Lookup { get; set; }
    public Evidence? Evidence { get; set; }
    public long Count { get; set; }
}
