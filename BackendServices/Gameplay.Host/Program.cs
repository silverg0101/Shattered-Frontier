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
using ShatteredFrontier.CraftingContent;
using ClaimTerritory;
using DomLedger = ShatteredFrontier.InventoryLedger.InventoryLedger;

static string Required(string name) => Environment.GetEnvironmentVariable(name) is { Length: > 0 } value
    ? value : throw new InvalidOperationException($"{name} is required.");

var portText = Required("GAMEPLAY_HOST_PORT");
if (!int.TryParse(portText, out var port) || port is < 1 or > 65535)
    throw new InvalidOperationException("GAMEPLAY_HOST_PORT must be 1-65535.");
var ledgerConnection = Required("GAMEPLAY_LEDGER_CONNECTION");
var characterConnection = Required("GAMEPLAY_CHARACTER_CONNECTION");
var housingConnection = Required("GAMEPLAY_HOUSING_CONNECTION");
var token = Required("SF_GAMEPLAY_TOKEN");
var contentRoot = Required("GAMEPLAY_CONTENT_ROOT");
var boar = LootCatalog.LoadFiles([Path.Combine(contentRoot, "loot", "greenlands_starter.json")])["loot/greenlands_silverbrook_boar"];
var quest = QuestLoader.Load(Path.Combine(contentRoot, "quests", "greenlands_frontier_founder.json"));
var zone = ProtectedZone.Load(Path.Combine(contentRoot, "world", "zones", "greenlands_silverbrook.json"));
var recipes = RecipeCatalog.LoadFiles([Path.Combine(contentRoot, "crafting", "recipes_greenlands_starter.json")]);
var ledgerStore = new PostgresInventoryLedgerStore(ledgerConnection);

var builder = WebApplication.CreateBuilder(args);
builder.WebHost.UseSetting(WebHostDefaults.ServerUrlsKey, "");
builder.WebHost.ConfigureKestrel(o => o.Listen(IPAddress.Loopback, port, l => l.Protocols = HttpProtocols.Http2));
builder.Logging.SetMinimumLevel(LogLevel.Warning);
builder.Services.AddSingleton(new AuthOptions(Encoding.UTF8.GetBytes("Bearer " + token)));
builder.Services.AddSingleton(ledgerStore);
builder.Services.AddSingleton(new DomLedger(ledgerStore));
builder.Services.AddSingleton(new BoarRewardService(new DomLedger(ledgerStore), boar));
builder.Services.AddSingleton(new PostgresCharacterStateStore(characterConnection));
builder.Services.AddSingleton(new PostgresHousingStore(housingConnection));
builder.Services.AddSingleton(quest);
builder.Services.AddSingleton(zone);
builder.Services.AddSingleton(new ClaimSettings());
builder.Services.AddSingleton(recipes);
builder.Services.AddGrpc(o => o.Interceptors.Add<AuthInterceptor>());
var app = builder.Build();
app.MapGrpcService<RewardGrpcService>();
app.MapGrpcService<ProgressionGrpcService>();
app.MapGrpcService<HousingGrpcService>();
app.Lifetime.ApplicationStarted.Register(() => Console.Out.WriteLine($"GAMEPLAY_HOST_READY port={port}"));
app.Run();

internal sealed record AuthOptions(byte[] Expected);
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
            var result = work();
            if (context.CancellationToken.IsCancellationRequested || context.Deadline <= DateTime.UtcNow)
                throw new RpcException(new Status(StatusCode.DeadlineExceeded, "Deadline exceeded."));
            return Task.FromResult(result);
        }
        catch (RpcException) { throw; }
        catch (ShatteredFrontier.InventoryLedger.LedgerRejectedException ex) { throw Reject(ex.Message); }
        catch (CharacterStoreRejectedException ex) { throw Reject(ex.Message); }
        catch (HousingRejectedException ex) { throw Reject(ex.Message); }
        catch (ShatteredFrontier.InventoryLedger.Postgres.LedgerStoreUnavailableException) { throw Unavailable(); }
        catch (LedgerSchemaCompatibilityException) { throw Unavailable(); }
        catch (CharacterStoreUnavailableException) { throw Unavailable(); }
        catch (CharacterSchemaCompatibilityException) { throw Unavailable(); }
        catch (HousingUnavailableException) { throw Unavailable(); }
        catch (HousingSchemaCompatibilityException) { throw Unavailable(); }
        catch (Exception ex) when (ex is ArgumentException or RewardContentException) { throw new RpcException(new Status(StatusCode.InvalidArgument, ex.Message)); }
        catch (InvalidOperationException ex) { throw Reject(ex.Message); }
    }
    private static RpcException Unavailable() => new(new Status(StatusCode.Unavailable, "Store unavailable; retry the same operation ID."));
    private static RpcException Reject(string message) => new(new Status(
        message.Contains("conflict", StringComparison.OrdinalIgnoreCase) || message.Contains("reused", StringComparison.OrdinalIgnoreCase)
            ? StatusCode.AlreadyExists : StatusCode.FailedPrecondition, message));
}
