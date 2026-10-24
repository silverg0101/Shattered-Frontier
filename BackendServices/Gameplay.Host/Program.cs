using System.Net;
using System.Security.Cryptography;
using System.Text;
using Grpc.Core;
using Grpc.Core.Interceptors;
using Microsoft.AspNetCore.Server.Kestrel.Core;
using ShatteredFrontier.CharacterState.Postgres;
using ShatteredFrontier.Housing.Postgres;
using ShatteredFrontier.InventoryLedger.Postgres;
using ShatteredFrontier.QuestContent;
using ShatteredFrontier.Rewards;
using ShatteredFrontier.Progression;
using ShatteredFrontier.GatheringNodes;
using ShatteredFrontier.WorldContent;
using ShatteredFrontier.CraftingContent;
using ShatteredFrontier.Exchange.Postgres;
using CharacterAppearance;
using ClaimTerritory;
using DomLedger = ShatteredFrontier.InventoryLedger.InventoryLedger;

static string Required(string name) => Environment.GetEnvironmentVariable(name) is { Length: > 0 } value
    ? value : throw new InvalidOperationException($"{name} is required.");

if (args is ["outbox-status"])
{
    var controls = new ReceiptOutboxControls(); controls.Validate();
    var (pending, parked) = new PostgresInventoryLedgerStore(Required("GAMEPLAY_LEDGER_CONNECTION")).OutboxCounts();
    Console.WriteLine($"RECEIPT_OUTBOX pending={pending} parked={parked} batch={controls.BatchSize} poll_seconds={controls.PollInterval.TotalSeconds} max_attempts={controls.MaxAttempts}");
    return;
}
if (args is ["outbox-requeue", var operationId])
{
    new PostgresInventoryLedgerStore(Required("GAMEPLAY_LEDGER_CONNECTION")).RequeueParkedOutbox(operationId);
    Console.WriteLine($"RECEIPT_OUTBOX_REQUEUED op={operationId}");
    return;
}

if (args.Length == 1 && args[0].StartsWith("fixture-geometry-", StringComparison.Ordinal))
{
    HousingGeometryFixture.Run(args[0]);
    return;
}
if (args.Length == 1 && args[0].StartsWith("fixture-", StringComparison.Ordinal))
{
    GameplayFixtureClient.Run(args[0]);
    return;
}

var portText = Required("GAMEPLAY_HOST_PORT");
if (!int.TryParse(portText, out var port) || port is < 1 or > 65535)
    throw new InvalidOperationException("GAMEPLAY_HOST_PORT must be 1-65535.");
var pools = new DatabasePoolSettings();
var ledgerConnection = pools.For("InventoryLedger", Required("GAMEPLAY_LEDGER_CONNECTION"));
var characterConnection = pools.For("CharacterState", Required("GAMEPLAY_CHARACTER_CONNECTION"));
var housingConnection = pools.For("Housing", Required("GAMEPLAY_HOUSING_CONNECTION"));
var exchangeConnection = pools.For("Exchange", Required("GAMEPLAY_EXCHANGE_CONNECTION"));
var token = Required("SF_GAMEPLAY_TOKEN");
var contentRoot = Required("GAMEPLAY_CONTENT_ROOT");
var appearanceAllowlist = AppearanceAllowlist.Load(Path.Combine(contentRoot, "appearance", "human.json"));
var boar = LootCatalog.LoadFiles([Path.Combine(contentRoot, "loot", "greenlands_starter.json")])["loot/greenlands_silverbrook_boar"];
var questFiles = QuestLoader.LoadDirectory(Path.Combine(contentRoot, "quests"));
var questIssues = QuestValidator.ValidateAll(questFiles);
if (questIssues.Count != 0) throw new InvalidOperationException("Quest content invalid: " + string.Join("; ", questIssues));
IReadOnlyDictionary<string, QuestDefinition> quests = questFiles.ToDictionary(f => f.Quest.Id, f => f.Quest, StringComparer.Ordinal);
var zone = ProtectedZone.Load(Path.Combine(contentRoot, "world", "zones", "greenlands_silverbrook.json"));
var gatheringZone = ZoneLoader.Load(Path.Combine(contentRoot, "world", "zones", "greenlands_silverbrook.json"));
var gatheringTuning = GatheringHostTuning.Shipped; gatheringTuning.Validate();
var fixtureGatheringRespawn = Environment.GetEnvironmentVariable("GAMEPLAY_FIXTURE_GATHERING_RESPAWN_SECONDS");
if (fixtureGatheringRespawn is not null && (Environment.GetEnvironmentVariable("GAMEPLAY_FIXTURE_MODE") != "gathering" ||
    !int.TryParse(fixtureGatheringRespawn, out var seconds) || seconds != 30))
    throw new InvalidOperationException("Gathering fixture respawn requires gathering fixture mode and 30 seconds.");
var recipes = RecipeCatalog.LoadFiles([Path.Combine(contentRoot, "crafting", "recipes_greenlands_starter.json")]);
var markets = MarketCatalog.Load(Path.Combine(contentRoot, "markets"));
var ledgerStore = new PostgresInventoryLedgerStore(ledgerConnection);
// Alpha Starter Kit purse (MKT-01 owner answer): off unless the realm enables it, like the ledger host kit switch.
var kitSwitch = Environment.GetEnvironmentVariable("GAMEPLAY_ALPHA_STARTER_KIT") ?? "";
if (kitSwitch is not ("" or "on")) throw new InvalidOperationException("GAMEPLAY_ALPHA_STARTER_KIT must be empty or on.");
var starterKit = new StarterKitOptions(kitSwitch == "on" ? new ShatteredFrontier.InventoryLedger.AlphaStarterKitSettings() : null);
starterKit.Kit?.Validate();

var builder = WebApplication.CreateBuilder(args);
builder.WebHost.UseSetting(WebHostDefaults.ServerUrlsKey, "");
builder.WebHost.ConfigureKestrel(o => o.Listen(IPAddress.Loopback, port, l => l.Protocols = HttpProtocols.Http2));
builder.Logging.SetMinimumLevel(LogLevel.Warning);
builder.Services.AddSingleton(new AuthOptions(Encoding.UTF8.GetBytes("Bearer " + token)));
builder.Services.AddSingleton(pools);
builder.Services.AddSingleton(starterKit);
builder.Services.AddSingleton(ledgerStore);
builder.Services.AddSingleton(new DomLedger(ledgerStore));
builder.Services.AddSingleton(new BoarRewardService(new DomLedger(ledgerStore), boar));
builder.Services.AddSingleton(new DefeatRewardPayout(ledgerStore, boar, ledgerConnection));
builder.Services.AddHostedService<DefeatRewardRecovery>();
builder.Services.AddSingleton(new PostgresCharacterStateStore(characterConnection));
// Owned reconnect fixtures can seed skill rolls without changing the production roll source.
var fixtureSkillSeed = Environment.GetEnvironmentVariable("GAMEPLAY_FIXTURE_SKILL_SEED");
if (fixtureSkillSeed is not null && (!int.TryParse(fixtureSkillSeed, out _) ||
    Environment.GetEnvironmentVariable("GAMEPLAY_FIXTURE_MODE") != "reconnect"))
    throw new InvalidOperationException("GAMEPLAY_FIXTURE_SKILL_SEED requires the reconnect fixture mode and an integer seed.");
var skillRandom = fixtureSkillSeed is null ? null : new Random(int.Parse(fixtureSkillSeed));
builder.Services.AddSingleton<Func<double>>(() => skillRandom?.NextDouble() ?? Random.Shared.NextDouble());
var progressionSettings = new ShatteredFrontier.Progression.ProgressionSettings(); progressionSettings.Validate();
builder.Services.AddSingleton(progressionSettings);
builder.Services.AddSingleton(appearanceAllowlist);
builder.Services.AddSingleton(new AppearanceValidator(appearanceAllowlist));
builder.Services.AddSingleton<QuestRewardPayout>();
builder.Services.AddSingleton(new GameplayRecoverySettings());
builder.Services.AddHostedService<GameplayRecoveryService>();
builder.Services.AddSingleton(new ReceiptOutboxControls());
builder.Services.AddHostedService<ReceiptOutboxRelay>();
builder.Services.AddSingleton(new PostgresHousingStore(housingConnection));
builder.Services.AddSingleton(new PostgresExchangeStore(exchangeConnection));
builder.Services.AddSingleton(quests);
builder.Services.AddSingleton(zone);
builder.Services.AddSingleton(gatheringZone);
builder.Services.AddSingleton<WorldSlotsGrpcService>();
builder.Services.AddSingleton(new PostgresGatheringStore(ledgerConnection, gatheringZone, gatheringTuning,
    fixtureGatheringRespawn is null ? gatheringTuning.Lifecycle : new NodeSettings(30, 0, RegrowthMode.FullRespawn), ledgerStore));
builder.Services.AddHostedService<GatheringGrantRecovery>();
builder.Services.AddSingleton(new ClaimSettings());
builder.Services.AddSingleton(recipes);
builder.Services.AddSingleton(markets);
var craftingControls = new CraftingControls(); craftingControls.Validate();
builder.Services.AddSingleton(craftingControls);
builder.Services.AddSingleton(new CraftSeedSecret(SHA256.HashData(Encoding.UTF8.GetBytes(token))));
builder.Services.AddSingleton<RecipeLearning>();
builder.Services.AddSingleton(new CraftAttemptService(recipes, new DomLedger(ledgerStore)));
builder.Services.AddGrpc(o => o.Interceptors.Add<AuthInterceptor>());
var app = builder.Build();
app.MapGrpcService<RewardGrpcService>();
app.MapGrpcService<GatheringGrpcService>();
app.MapGrpcService<WorldSlotsGrpcService>();
app.MapGrpcService<ProgressionGrpcService>();
app.MapGrpcService<HousingGrpcService>();
app.MapGrpcService<CraftingGrpcService>();
app.MapGrpcService<ExchangeGrpcService>();
app.Lifetime.ApplicationStarted.Register(() => Console.Out.WriteLine($"GAMEPLAY_HOST_READY port={port}"));
app.Run();

internal sealed record AuthOptions(byte[] Expected);
internal sealed record StarterKitOptions(ShatteredFrontier.InventoryLedger.AlphaStarterKitSettings? Kit);
internal sealed record CraftSeedSecret(byte[] Bytes);
internal sealed class GameplayRejectedException(string message, StatusCode code) : InvalidOperationException(message)
{
    public StatusCode Code { get; } = code;
}
internal sealed class AuthInterceptor(AuthOptions options) : Interceptor
{
    public override Task<TResponse> UnaryServerHandler<TRequest, TResponse>(TRequest request, ServerCallContext context,
        UnaryServerMethod<TRequest, TResponse> continuation)
    {
        var actual = Encoding.UTF8.GetBytes(context.RequestHeaders.GetValue("authorization") ?? "");
        if (!CryptographicOperations.FixedTimeEquals(actual, options.Expected))
            throw new RpcException(new Status(StatusCode.Unauthenticated, "Invalid bearer token."));
        if (context.Deadline == DateTime.MaxValue)
            throw new RpcException(new Status(StatusCode.InvalidArgument, "A deadline is required."));
        return continuation(request, context);
    }
}

internal static class GrpcRun
{
    public static Task<T> Call<T>(ServerCallContext context, Func<T> work)
    {
        if (context.CancellationToken.IsCancellationRequested || context.Deadline <= DateTime.UtcNow)
            throw new RpcException(new Status(StatusCode.DeadlineExceeded, "Deadline exceeded."));
        try
        {
            using var housingCall = PostgresHousingStore.UseCallContext(context.CancellationToken, context.Deadline);
            var result = work();
            return Task.FromResult(result);
        }
        catch (RpcException) { throw; }
        catch (GameplayRejectedException ex) { throw new RpcException(new Status(ex.Code, ex.Message)); }
        catch (CraftContentException ex) { throw new RpcException(new Status(StatusCode.InvalidArgument, ex.Message)); }
        catch (ShatteredFrontier.InventoryLedger.LedgerRejectedException ex) { throw Reject(ex.Message); }
        catch (CharacterStoreRejectedException ex) { throw Reject(ex.Message); }
        catch (HousingRejectedException ex) { throw new RpcException(new Status(ex.Code switch
        {
            HousingRejectionCode.InvalidArgument => StatusCode.InvalidArgument,
            HousingRejectionCode.AlreadyExists => StatusCode.AlreadyExists,
            HousingRejectionCode.PermissionDenied => StatusCode.PermissionDenied,
            _ => StatusCode.FailedPrecondition
        }, ex.Message)); }
        catch (OperationCanceledException) { throw new RpcException(new Status(StatusCode.DeadlineExceeded, "Deadline exceeded.")); }
        catch (ShatteredFrontier.InventoryLedger.Postgres.LedgerStoreUnavailableException) { throw Unavailable(); }
        catch (LedgerSchemaCompatibilityException) { throw Unavailable(); }
        catch (CharacterStoreUnavailableException) { throw Unavailable(); }
        catch (CharacterSchemaCompatibilityException) { throw Unavailable(); }
        catch (HousingUnavailableException) { throw Unavailable(); }
        catch (HousingSchemaCompatibilityException) { throw Unavailable(); }
        catch (ExchangeUnavailableException) { throw Unavailable(); }
        catch (ExchangeSchemaCompatibilityException) { throw Unavailable(); }
        catch (Npgsql.NpgsqlException) { throw Unavailable(); }
        catch (ExchangeRejectedException ex) { throw new RpcException(new Status(
            ex.Message == "Operation ID conflict." ? StatusCode.AlreadyExists : StatusCode.FailedPrecondition, ex.Message)); }
        catch (Exception ex) when (ex is ArgumentException or RewardContentException) { throw new RpcException(new Status(StatusCode.InvalidArgument, ex.Message)); }
        catch (InvalidOperationException ex) { throw Reject(ex.Message); }
    }
    private static RpcException Unavailable() => new(new Status(StatusCode.Unavailable, "Store unavailable; retry the same operation ID."));
    private static RpcException Reject(string message) => new(new Status(StatusCode.FailedPrecondition, message));
}
