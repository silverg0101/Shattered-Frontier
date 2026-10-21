using System.Security.Cryptography;
using Grpc.Core;
using Grpc.Net.Client;
using ShatteredFrontier.InventoryLedger.Postgres;
using Pb = ShatteredFrontier.Progression.V1;

// Release-declared operating controls. A published config snapshot can replace these
// through the shared LiveOps workflow; no process-local override is accepted.
internal sealed record ReceiptOutboxControls
{
    internal sealed record Declaration(string ProposedKey, string Label, string Help, string Type, string Unit,
        int Default, int Minimum, int Maximum, string Owner, string Permission, string Approval, string ApplyTime,
        string Scope = "realm", int Step = 1, string SafeFallback = "shipped default", bool ClientVisible = false,
        string Observability = "pending and parked counts", string Rollback = "next poll");
    public static IReadOnlyList<Declaration> Declarations { get; } =
    [
        new("gameplay.receipt_outbox.batch_size", "Receipt batch size", "Maximum pending receipts claimed per poll.",
            "integer", "rows", 32, 1, 256, "Gameplay Operations", "realm configuration operator", "normal LiveOps", "next poll"),
        new("gameplay.receipt_outbox.poll_interval", "Receipt poll interval", "Time between pending receipt scans.",
            "duration", "seconds", 2, 1, 60, "Gameplay Operations", "realm configuration operator", "normal LiveOps", "next poll"),
        new("gameplay.receipt_outbox.max_attempts", "Receipt maximum attempts", "Delivery attempts before an undeliverable receipt is parked for repair.",
            "integer", "attempts", 12, 1, 100, "Gameplay Operations", "realm configuration operator", "normal LiveOps", "next poll")
    ];
    public int BatchSize { get; init; } = 32;
    public TimeSpan PollInterval { get; init; } = TimeSpan.FromSeconds(2);
    public int MaxAttempts { get; init; } = 12;
    public void Validate()
    {
        if (BatchSize is < 1 or > 256 || PollInterval < TimeSpan.FromSeconds(1) ||
            PollInterval > TimeSpan.FromSeconds(60) || MaxAttempts is < 1 or > 100)
            throw new InvalidOperationException("Receipt outbox controls outside shipped bounds.");
    }
}

internal sealed class ReceiptOutboxRelay(PostgresInventoryLedgerStore store, ReceiptOutboxControls controls) : BackgroundService
{
    private static readonly Method<byte[], byte[]> RawApplyReceipt = new(MethodType.Unary,
        "sf.progression.v1.Progression", "ApplyReceipt",
        Marshallers.Create<byte[]>(bytes => bytes, bytes => bytes),
        Marshallers.Create<byte[]>(bytes => bytes, bytes => bytes));
    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        controls.Validate();
        var owner = Guid.NewGuid().ToString("N");
        var token = Environment.GetEnvironmentVariable("SF_GAMEPLAY_TOKEN")!;
        var port = Environment.GetEnvironmentVariable("GAMEPLAY_HOST_PORT")!;
        using var channel = GrpcChannel.ForAddress($"http://127.0.0.1:{port}");
        var invoker = channel.CreateCallInvoker();
        using var timer = new PeriodicTimer(controls.PollInterval);
        do
        {
            try
            {
                var (pending, parked) = store.OutboxCounts();
                Console.Out.WriteLine($"RECEIPT_OUTBOX pending={pending} parked={parked} batch={controls.BatchSize} poll_seconds={controls.PollInterval.TotalSeconds} max_attempts={controls.MaxAttempts}");
                var deliveries = store.ClaimOutbox(owner, controls.BatchSize, controls.MaxAttempts);
                await Task.WhenAll(deliveries.Select(async delivery =>
                {
                    if (Environment.GetEnvironmentVariable("SF_GAMEPLAY_FAULT_BEFORE_OUTBOX_SEND") == delivery.OperationId)
                        Environment.FailFast("INT-06 fixture: committed grant, claimed intent, stopped before send.");
                    try
                    {
                        if (delivery.Digest != Convert.ToHexStringLower(SHA256.HashData(delivery.Payload)))
                            throw new InvalidDataException("Payload digest mismatch.");
                        var request = Pb.ApplyReceiptRequest.Parser.ParseFrom(delivery.Payload);
                        if (request.SourceReceiptId != delivery.OperationId || request.CharacterId != delivery.CharacterId)
                            throw new InvalidDataException("Payload identity mismatch.");
                        var call = invoker.AsyncUnaryCall(RawApplyReceipt, null,
                            new CallOptions(new Metadata { { "authorization", "Bearer " + token } },
                                deadline: DateTime.UtcNow.AddSeconds(10), cancellationToken: stoppingToken),
                            delivery.Payload);
                        await call.ResponseAsync;
                        store.CompleteOutbox(delivery.OperationId, owner);
                    }
                    catch (Exception ex) when (ex is not OperationCanceledException)
                    {
                        var permanent = ex is InvalidDataException or Google.Protobuf.InvalidProtocolBufferException ||
                            ex is RpcException rpc && rpc.StatusCode is StatusCode.InvalidArgument or StatusCode.AlreadyExists;
                        var code = ex is RpcException r ? r.StatusCode.ToString() : ex.GetType().Name;
                        store.FailOutbox(delivery.OperationId, owner, code, permanent, controls.MaxAttempts);
                        Console.Error.WriteLine($"RECEIPT_OUTBOX_RETRY op={delivery.OperationId} code={code} attempts={delivery.Attempts} parked={permanent || delivery.Attempts >= controls.MaxAttempts}");
                    }
                }));
            }
            catch (Exception ex) when (ex is not OperationCanceledException)
            { Console.Error.WriteLine($"RECEIPT_OUTBOX_UNAVAILABLE {ex.GetType().Name}"); }
        } while (await timer.WaitForNextTickAsync(stoppingToken));
    }
}
