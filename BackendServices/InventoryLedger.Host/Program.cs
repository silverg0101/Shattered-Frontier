using System.Globalization;
using System.Net;
using System.Security.Cryptography;
using System.Text;
using Grpc.Core;
using Grpc.Core.Interceptors;
using Microsoft.AspNetCore.Server.Kestrel.Core;
using ShatteredFrontier.InventoryLedger.Postgres;
using Dom = ShatteredFrontier.InventoryLedger;
using Pb = ShatteredFrontier.InventoryLedger.V1;

static string Env(string name) => Environment.GetEnvironmentVariable(name) ?? "";
static void Refuse(string message) { Console.Error.WriteLine("INVENTORY_LEDGER_HOST_REFUSED " + message); Environment.Exit(2); }

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
var token = Env("SF_INVENTORY_LEDGER_TOKEN");
if (token.Length == 0) Refuse("SF_INVENTORY_LEDGER_TOKEN is required.");
var fault = Env("INVENTORY_LEDGER_FAULT");
if (fault is not ("" or "drop_first_reply")) Refuse("INVENTORY_LEDGER_FAULT must be empty or drop_first_reply.");

var builder = WebApplication.CreateBuilder(args);
builder.WebHost.UseSetting(WebHostDefaults.ServerUrlsKey, "");
builder.WebHost.ConfigureKestrel(o => o.Listen(IPAddress.Loopback, port, l => l.Protocols = HttpProtocols.Http2));
builder.Logging.SetMinimumLevel(LogLevel.Warning);
builder.Services.AddSingleton(new Dom.InventoryLedger(new PostgresInventoryLedgerStore(connection)));
builder.Services.AddSingleton(new LedgerOptions(Encoding.UTF8.GetBytes("Bearer " + token), fault == "drop_first_reply"));
builder.Services.AddGrpc(o => o.Interceptors.Add<TokenInterceptor>());

var app = builder.Build();
app.MapGrpcService<LedgerService>();
app.Lifetime.ApplicationStarted.Register(() => Console.Out.WriteLine($"INVENTORY_LEDGER_HOST_READY port={port}"));
app.Run();
return 0;

sealed record LedgerOptions(byte[] ExpectedAuthorization, bool DropFirstReply);

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

sealed class LedgerService(Dom.InventoryLedger ledger, LedgerOptions options) : Pb.InventoryLedger.InventoryLedgerBase
{
    private static int firstGrantSeen; // the service is created per call

    public override Task<Pb.ContainerView> EnsureContainer(Pb.EnsureContainerRequest request, ServerCallContext context) =>
        Run(context, () =>
        {
            var s = request.Spec ?? throw new RpcException(new Status(StatusCode.InvalidArgument, "Spec is required."));
            return ToPb(ledger.CreateContainer(new Dom.ContainerSpec(s.Id, Map(s.OwnerKind), s.OwnerId, Map(s.Kind), s.Capacity)));
        });

    public override Task<Pb.ContainerView> GetContainer(Pb.GetContainerRequest request, ServerCallContext context) =>
        Run(context, () => ToPb(ledger.Read(request.ContainerId)));

    public override Task<Pb.LedgerReceipt> Grant(Pb.GrantRequest r, ServerCallContext context)
    {
        var code = StatusCode.OK;
        try
        {
            return Run(context, () =>
            {
                var receipt = ToPb(ledger.Grant(r.OperationId, r.ContainerId, r.ExpectedRevision, r.DefinitionId, r.Quantity, r.Source, r.CausationId));
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
                "Container ID already has a different specification." or "Conflicting operation ID reuse." => StatusCode.AlreadyExists,
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
