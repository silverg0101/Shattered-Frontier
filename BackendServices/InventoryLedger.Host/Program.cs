using System.Globalization;
using System.Net;
using System.Security.Cryptography;
using System.Text;
using Grpc.Core;
using Grpc.Core.Interceptors;
using Microsoft.AspNetCore.Server.Kestrel.Core;
using Npgsql;
using ShatteredFrontier.InventoryLedger.Postgres;
using Dom = ShatteredFrontier.InventoryLedger;
using Pb = ShatteredFrontier.InventoryLedger.V1;
using ProgressPb = ShatteredFrontier.Progression.V1;

static string Env(string name) => Environment.GetEnvironmentVariable(name) ?? "";
static void Refuse(string message) { Console.Error.WriteLine("INVENTORY_LEDGER_HOST_REFUSED " + message); Environment.Exit(2); }

// Fixture seed (runners only): `InventoryLedger.Host seed <containerId> <ownerId> <capacity> <definitionId> <quantity> <operationId>`
// ensures the character backpack and grants items through the same gRPC contract, so a test character can afford its builds.
if (args.Length == 7 && args[0] == "seed")
{
    if (!int.TryParse(Env("INVENTORY_LEDGER_PORT"), NumberStyles.None, CultureInfo.InvariantCulture, out var seedPort) || seedPort is < 1 or > 65535)
        Refuse("INVENTORY_LEDGER_PORT must be 1-65535.");
    var seedToken = Env("SF_INVENTORY_LEDGER_TOKEN");
    if (seedToken.Length == 0) Refuse("SF_INVENTORY_LEDGER_TOKEN is required.");
    using var channel = Grpc.Net.Client.GrpcChannel.ForAddress($"http://127.0.0.1:{seedPort}");
    var headers = new Metadata { { "authorization", "Bearer " + seedToken } };
    var client = new Pb.InventoryLedger.InventoryLedgerClient(channel);
    try
    {
        var view = client.EnsureContainer(new Pb.EnsureContainerRequest { Spec = new Pb.ContainerSpec { Id = args[1], OwnerKind = Pb.OwnerKind.Character,
            OwnerId = args[2], Kind = Pb.ContainerKind.Backpack, Capacity = int.Parse(args[3], CultureInfo.InvariantCulture) } }, headers, deadline: DateTime.UtcNow.AddSeconds(5));
        client.Grant(new Pb.GrantRequest { OperationId = args[6], ContainerId = args[1], ExpectedRevision = view.Revision, DefinitionId = args[4],
            Quantity = long.Parse(args[5], CultureInfo.InvariantCulture), Source = "fixture_seed", CausationId = args[6] }, headers, deadline: DateTime.UtcNow.AddSeconds(5));
        Console.Out.WriteLine($"INVENTORY_LEDGER_SEEDED container={args[1]} item={args[4]} quantity={args[5]}");
        return 0;
    }
    catch (RpcException ex) { Console.Out.WriteLine($"INVENTORY_LEDGER_SEED_FAILED code={ex.StatusCode}"); return 3; }
}

// Client probe: `InventoryLedger.Host get <containerId>` prints the container view and exits.
if (args.Length == 2 && args[0] == "get")
{
    if (!int.TryParse(Env("INVENTORY_LEDGER_PORT"), NumberStyles.None, CultureInfo.InvariantCulture, out var probePort) || probePort is < 1 or > 65535)
        Refuse("INVENTORY_LEDGER_PORT must be 1-65535.");
    var probeToken = Env("SF_INVENTORY_LEDGER_TOKEN");
    if (probeToken.Length == 0) Refuse("SF_INVENTORY_LEDGER_TOKEN is required.");
    using var channel = Grpc.Net.Client.GrpcChannel.ForAddress($"http://127.0.0.1:{probePort}");
    var headers = new Metadata { { "authorization", "Bearer " + probeToken } };
    try
    {
        var view = new Pb.InventoryLedger.InventoryLedgerClient(channel).GetContainer(
            new Pb.GetContainerRequest { ContainerId = args[1] }, headers, deadline: DateTime.UtcNow.AddSeconds(5));
        var items = view.Quantities.OrderBy(q => q.Key, StringComparer.Ordinal).Select(q => $" {q.Key}={q.Value}");
        Console.Out.WriteLine($"INVENTORY_LEDGER_VIEW container={view.Spec.Id} revision={view.Revision}{string.Concat(items)}");
        return 0;
    }
    catch (RpcException ex) { Console.Out.WriteLine($"INVENTORY_LEDGER_VIEW_FAILED code={ex.StatusCode}"); return 3; }
}

if (!int.TryParse(Env("INVENTORY_LEDGER_PORT"), NumberStyles.None, CultureInfo.InvariantCulture, out var port) || port is < 1 or > 65535)
    Refuse("INVENTORY_LEDGER_PORT must be 1-65535.");
var connection = Env("INVENTORY_LEDGER_CONNECTION");
if (connection.Length == 0) Refuse("INVENTORY_LEDGER_CONNECTION is required.");
var poolSettings = new LedgerPoolSettings();
poolSettings.Validate();
var pool = new NpgsqlConnectionStringBuilder(connection)
{
    Pooling = true, MinPoolSize = 0, MaxPoolSize = poolSettings.MaximumConnections,
    ApplicationName = "sf.inventory-ledger"
};
connection = pool.ConnectionString;
var token = Env("SF_INVENTORY_LEDGER_TOKEN");
if (token.Length == 0) Refuse("SF_INVENTORY_LEDGER_TOKEN is required.");
var fault = Env("INVENTORY_LEDGER_FAULT");
if (fault is not ("" or "drop_first_reply")) Refuse("INVENTORY_LEDGER_FAULT must be empty or drop_first_reply.");
// Alpha Starter Kit (DEC-02 alpha grant): off unless the realm enables it at start; contents are release-declared.
var kitSwitch = Env("INVENTORY_LEDGER_ALPHA_STARTER_KIT");
if (kitSwitch is not ("" or "on")) Refuse("INVENTORY_LEDGER_ALPHA_STARTER_KIT must be empty or on.");
var kit = kitSwitch == "on" ? new Dom.AlphaStarterKitSettings() : null;
kit?.Validate();

var builder = WebApplication.CreateBuilder(args);
builder.WebHost.UseSetting(WebHostDefaults.ServerUrlsKey, "");
builder.WebHost.ConfigureKestrel(o => o.Listen(IPAddress.Loopback, port, l => l.Protocols = HttpProtocols.Http2));
builder.Logging.SetMinimumLevel(LogLevel.Warning);
builder.Services.AddSingleton(new PostgresInventoryLedgerStore(connection));
builder.Services.AddSingleton(sp => new Dom.InventoryLedger(sp.GetRequiredService<PostgresInventoryLedgerStore>()));
builder.Services.AddSingleton(new LedgerOptions(Encoding.UTF8.GetBytes("Bearer " + token), fault == "drop_first_reply", kit));
builder.Services.AddGrpc(o => o.Interceptors.Add<TokenInterceptor>());

var app = builder.Build();
app.MapGrpcService<LedgerService>();
app.Lifetime.ApplicationStarted.Register(() => Console.Out.WriteLine($"INVENTORY_LEDGER_HOST_READY port={port}"));
app.Run();
return 0;

sealed record LedgerOptions(byte[] ExpectedAuthorization, bool DropFirstReply, Dom.AlphaStarterKitSettings? StarterKit);

// Release-declared operating setting; shared config publication activates on restart.
sealed record LedgerPoolSettings
{
    public int MaximumConnections { get; init; } = 12;
    public void Validate()
    {
        if (MaximumConnections is < 2 or > 32)
            throw new InvalidOperationException("Ledger pool bound must be 2..32 connections.");
    }
}

// Development auth fixture, not the DEC-04 session protocol.
sealed class TokenInterceptor(LedgerOptions options) : Interceptor
{
    public override Task<TResponse> UnaryServerHandler<TRequest, TResponse>(TRequest request, ServerCallContext context, UnaryServerMethod<TRequest, TResponse> continuation)
    {
        var header = context.RequestHeaders.GetValue("authorization") ?? "";
        if (!CryptographicOperations.FixedTimeEquals(Encoding.UTF8.GetBytes(header), options.ExpectedAuthorization))
            throw new RpcException(new Status(StatusCode.Unauthenticated, "Missing or invalid token."));
        return continuation(request, context);
    }
}

sealed class LedgerService(Dom.InventoryLedger ledger, PostgresInventoryLedgerStore store, LedgerOptions options) : Pb.InventoryLedger.InventoryLedgerBase
{
    private static int firstGrantSeen; // the service is created per call

    public override Task<Pb.ContainerView> EnsureContainer(Pb.EnsureContainerRequest request, ServerCallContext context) =>
        Run(context, () =>
        {
            var s = request.Spec ?? throw new RpcException(new Status(StatusCode.InvalidArgument, "Spec is required."));
            var view = ledger.CreateContainer(new Dom.ContainerSpec(s.Id, Map(s.OwnerKind), s.OwnerId, Map(s.Kind), s.Capacity));
            if (options.StarterKit is { } kit)
            {
                var before = view.Revision;
                view = kit.GrantOnce(ledger, view);
                if (view.Revision != before) Console.Out.WriteLine($"ALPHA_STARTER_KIT_GRANTED container={view.Spec.Id} owner={view.Spec.OwnerId} operation={kit.OperationId(view.Spec.OwnerId)}");
            }
            return ToPb(view);
        });

    public override Task<Pb.ContainerView> GetContainer(Pb.GetContainerRequest request, ServerCallContext context) =>
        Run(context, () => ToPb(ledger.Read(request.ContainerId)));

    public override Task<Pb.LedgerReceipt> Grant(Pb.GrantRequest r, ServerCallContext context) => GrantCore(r, context, false);

    public override Task<Pb.LedgerReceipt> GrantWithIntent(Pb.GrantRequest r, ServerCallContext context) => GrantCore(r, context, true);

    private Task<Pb.LedgerReceipt> GrantCore(Pb.GrantRequest r, ServerCallContext context, bool requireIntent)
    {
        var code = StatusCode.OK;
        try
        {
            return Run(context, () =>
            {
                if (requireIntent && r.FollowUpIntent is null)
                    throw new RpcException(new Status(StatusCode.InvalidArgument, "Follow-up intent is required."));
                PostgresInventoryLedgerStore.FollowUpIntent? intent = null;
                if (r.FollowUpIntent is { } supplied)
                {
                    byte[] bytes = supplied.Payload.ToByteArray();
                    if (supplied.PayloadVersion != 1 || bytes.Length is < 1 or > 65536 ||
                        supplied.PayloadDigest != Convert.ToHexStringLower(SHA256.HashData(bytes)))
                        throw new RpcException(new Status(StatusCode.InvalidArgument, "Invalid follow-up version or digest."));
                    ProgressPb.ApplyReceiptRequest parsed;
                    try { parsed = ProgressPb.ApplyReceiptRequest.Parser.ParseFrom(bytes); }
                    catch (Google.Protobuf.InvalidProtocolBufferException) { throw new RpcException(new Status(StatusCode.InvalidArgument, "Invalid follow-up payload.")); }
                    var container = ledger.Read(r.ContainerId);
                    if (container.Spec.OwnerKind != Dom.OwnerKind.Character || parsed.CharacterId != container.Spec.OwnerId ||
                        parsed.SourceReceiptId != r.OperationId || parsed.Kind != "gather" || parsed.Payload is null ||
                        parsed.Quest is null || parsed.Quest.OccurredAtUnixMs <= 0)
                        throw new RpcException(new Status(StatusCode.InvalidArgument, "Follow-up identity or payload mismatch."));
                    intent = new(r.OperationId, parsed.CharacterId, (int)supplied.PayloadVersion, bytes, supplied.PayloadDigest);
                }
                var receipt = ToPb(store.WithGrantIntent(r.OperationId, intent, () => ledger.Grant(
                    r.OperationId, r.ContainerId, r.ExpectedRevision, r.DefinitionId, r.Quantity, r.Source, r.CausationId)));
                if (options.DropFirstReply && Interlocked.Exchange(ref firstGrantSeen, 1) == 0)
                {
                    Console.Out.WriteLine($"INVENTORY_LEDGER_FAULT_INJECTED op={r.OperationId}");
                    throw new RpcException(new Status(StatusCode.Unavailable, "Injected lost reply; retry the operation ID."));
                }
                return receipt;
            });
        }
        catch (RpcException ex) { code = ex.StatusCode; throw; }
        finally { Console.Out.WriteLine($"INVENTORY_LEDGER_GRANT op={r.OperationId} container={r.ContainerId} status={Name(code)}"); }
    }

    private static string Name(StatusCode c) => c switch
    {
        StatusCode.OK => "OK",
        StatusCode.Aborted => "ABORTED",
        StatusCode.AlreadyExists => "ALREADY_EXISTS",
        StatusCode.NotFound => "NOT_FOUND",
        StatusCode.FailedPrecondition => "FAILED_PRECONDITION",
        StatusCode.Unavailable => "UNAVAILABLE",
        StatusCode.InvalidArgument => "INVALID_ARGUMENT",
        StatusCode.DeadlineExceeded => "DEADLINE_EXCEEDED",
        StatusCode.Cancelled => "CANCELLED",
        _ => c.ToString().ToUpperInvariant(),
    };

    // Checks the deadline before any store work, then translates ledger/store failures into gRPC statuses.
    private static Task<T> Run<T>(ServerCallContext context, Func<T> work)
    {
        if (context.CancellationToken.IsCancellationRequested)
            throw new RpcException(context.Deadline <= DateTime.UtcNow
                ? new Status(StatusCode.DeadlineExceeded, "Deadline exceeded.") : new Status(StatusCode.Cancelled, "Cancelled."));
        try { return Task.FromResult(work()); }
        catch (Dom.LedgerRejectedException ex)
        {
            var code = ex.Message switch
            {
                "Stale container revision." => StatusCode.Aborted,
                "Container ID already has a different specification." or "Conflicting operation ID reuse." or "Conflicting follow-up intent." => StatusCode.AlreadyExists,
                "Unknown container." => StatusCode.NotFound,
                _ => StatusCode.FailedPrecondition,
            };
            throw new RpcException(new Status(code, ex.Message));
        }
        catch (LedgerStoreUnavailableException) { throw new RpcException(new Status(StatusCode.Unavailable, "Inventory store unavailable; retry the operation ID.")); }
        catch (LedgerSchemaCompatibilityException) { throw new RpcException(new Status(StatusCode.Unavailable, "Inventory schema incompatible.")); }
    }

    private static RpcException Bad(string field) => new(new Status(StatusCode.InvalidArgument, $"{field} is unspecified or unknown."));

    private static Dom.OwnerKind Map(Pb.OwnerKind k) => k switch
    {
        Pb.OwnerKind.Character => Dom.OwnerKind.Character,
        Pb.OwnerKind.Account => Dom.OwnerKind.Account,
        Pb.OwnerKind.Guild => Dom.OwnerKind.Guild,
        Pb.OwnerKind.System => Dom.OwnerKind.System,
        _ => throw Bad("owner_kind"),
    };
    private static Pb.OwnerKind Map(Dom.OwnerKind k) => k switch
    {
        Dom.OwnerKind.Character => Pb.OwnerKind.Character,
        Dom.OwnerKind.Account => Pb.OwnerKind.Account,
        Dom.OwnerKind.Guild => Pb.OwnerKind.Guild,
        _ => Pb.OwnerKind.System,
    };
    private static Dom.ContainerKind Map(Pb.ContainerKind k) => k switch
    {
        Pb.ContainerKind.Equipment => Dom.ContainerKind.Equipment,
        Pb.ContainerKind.Backpack => Dom.ContainerKind.Backpack,
        Pb.ContainerKind.Bag => Dom.ContainerKind.Bag,
        Pb.ContainerKind.Bank => Dom.ContainerKind.Bank,
        Pb.ContainerKind.AccountBank => Dom.ContainerKind.AccountBank,
        Pb.ContainerKind.GuildBank => Dom.ContainerKind.GuildBank,
        Pb.ContainerKind.Mailbox => Dom.ContainerKind.Mailbox,
        Pb.ContainerKind.Escrow => Dom.ContainerKind.Escrow,
        Pb.ContainerKind.Buyback => Dom.ContainerKind.Buyback,
        Pb.ContainerKind.ConstructionCache => Dom.ContainerKind.ConstructionCache,
        Pb.ContainerKind.ConstructionRecovery => Dom.ContainerKind.ConstructionRecovery,
        _ => throw Bad("kind"),
    };
    private static Pb.ContainerKind Map(Dom.ContainerKind k) => k switch
    {
        Dom.ContainerKind.Equipment => Pb.ContainerKind.Equipment,
        Dom.ContainerKind.Backpack => Pb.ContainerKind.Backpack,
        Dom.ContainerKind.Bag => Pb.ContainerKind.Bag,
        Dom.ContainerKind.Bank => Pb.ContainerKind.Bank,
        Dom.ContainerKind.AccountBank => Pb.ContainerKind.AccountBank,
        Dom.ContainerKind.GuildBank => Pb.ContainerKind.GuildBank,
        Dom.ContainerKind.Mailbox => Pb.ContainerKind.Mailbox,
        Dom.ContainerKind.Escrow => Pb.ContainerKind.Escrow,
        Dom.ContainerKind.Buyback => Pb.ContainerKind.Buyback,
        Dom.ContainerKind.ConstructionCache => Pb.ContainerKind.ConstructionCache,
        _ => Pb.ContainerKind.ConstructionRecovery,
    };

    private static Pb.ContainerView ToPb(Dom.ContainerView v)
    {
        var view = new Pb.ContainerView
        {
            Spec = new Pb.ContainerSpec { Id = v.Spec.Id, OwnerKind = Map(v.Spec.OwnerKind), OwnerId = v.Spec.OwnerId, Kind = Map(v.Spec.Kind), Capacity = v.Spec.Capacity },
            Revision = v.Revision,
        };
        foreach (var (id, quantity) in v.Quantities) view.Quantities.Add(id, quantity);
        return view;
    }

    private static Pb.LedgerReceipt ToPb(Dom.LedgerReceipt r)
    {
        var receipt = new Pb.LedgerReceipt
        {
            OperationId = r.OperationId,
            Action = r.Action switch
            {
                Dom.LedgerAction.Grant => Pb.LedgerAction.Grant,
                Dom.LedgerAction.Consume => Pb.LedgerAction.Consume,
                Dom.LedgerAction.Transfer => Pb.LedgerAction.Transfer,
                Dom.LedgerAction.Convert => Pb.LedgerAction.Convert,
                _ => Pb.LedgerAction.Exchange,
            },
        };
        foreach (var c in r.Revisions) receipt.Revisions.Add(new Pb.RevisionChange { ContainerId = c.ContainerId, Before = c.Before, After = c.After });
        foreach (var c in r.Changes) receipt.Changes.Add(new Pb.QuantityChange { ContainerId = c.ContainerId, DefinitionId = c.DefinitionId, Delta = c.Delta });
        return receipt;
    }
}
