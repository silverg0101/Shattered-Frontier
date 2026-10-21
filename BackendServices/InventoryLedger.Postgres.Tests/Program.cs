using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;
using System.Security.Cryptography;
using System.Text;

static void Check(bool ok, string name)
{
    if (!ok) throw new Exception(name);
    Console.WriteLine("PASS " + name);
}
var connection = Environment.GetEnvironmentVariable("INVENTORY_LEDGER_CONNECTION") ?? throw new InvalidOperationException("Missing fixture connection.");
var ledger = new InventoryLedger(new PostgresInventoryLedgerStore(connection));
const string id = "ctn_durable";
const string item = "item/ore";
const string bar = "item/bar";
switch (args[0])
{
    case "seed":
        ledger.CreateContainer(new(id, OwnerKind.Character, "chr_durable", ContainerKind.Backpack, 100));
        var grant = ledger.Grant("grant-1", id, 0, item, 6, "fixture", "fixture-1");
        Check(grant.Revisions[0].After == 1 && ledger.Read(id).Quantities[item] == 6, "grant acknowledged durably");
        break;
    case "restart":
        Check(ledger.Read(id).Quantities[item] == 6, "grant survives process and database restart");
        var retry = ledger.Grant("grant-1", id, 0, item, 6, "fixture", "fixture-1");
        Check(retry.Revisions[0].Before == 0 && retry.Revisions[0].After == 1 && ledger.Read(id).Revision == 1, "reply-lost retry returns original receipt");
        var converted = ledger.Convert("convert-1", id, 1, "recipe/bar", [new(item, 2)], [new(bar, 1)]);
        Check(converted.Revisions[0].After == 2, "convert acknowledged durably");
        break;
    case "concurrent":
        var createSpec = new ContainerSpec("ctn_create_race",OwnerKind.Character,"chr_durable",ContainerKind.Backpack,100);
        using (var createBarrier = new Barrier(2))
        {
            var creates = Enumerable.Range(0,2).Select(_=>Task.Run(() =>
            {
                createBarrier.SignalAndWait(TimeSpan.FromSeconds(10));
                return ledger.CreateContainer(createSpec);
            })).ToArray();
            Task.WaitAll(creates);
            Check(creates.All(t=>t.Result.Spec==createSpec && t.Result.Revision==0),"concurrent create returns the same container view");
        }
        Check(ledger.Read(id).Quantities[item] == 4 && ledger.Read(id).Quantities[bar] == 1, "convert survives process and database restart");
        var ready = new CountdownEvent(2);
        var start = new ManualResetEventSlim(false);
        var tasks = new[] { "race-a", "race-b" }.Select(op => Task.Run(() =>
        {
            ready.Signal();
            start.Wait();
            try { return ledger.Consume(op, id, 2, item, 1, "fixture") is not null; }
            catch (LedgerRejectedException) { return false; }
        })).ToArray();
        if (!ready.Wait(TimeSpan.FromSeconds(10))) throw new Exception("Concurrent workers did not become ready.");
        start.Set();
        Task.WaitAll(tasks);
        ready.Dispose();
        start.Dispose();
        Check(tasks.Count(t => t.Result) == 1 && ledger.Read(id).Revision == 3 && ledger.Read(id).Quantities[item] == 3, "one concurrent revision winner");
        var parallelStore = new PostgresInventoryLedgerStore(connection);
        var left = "ctn_parallel_left";
        var right = "ctn_parallel_right";
        ledger.CreateContainer(new(left,OwnerKind.Character,"chr_durable",ContainerKind.Backpack,100));
        ledger.CreateContainer(new(right,OwnerKind.Character,"chr_durable",ContainerKind.Backpack,100));
        var singleWatch = System.Diagnostics.Stopwatch.StartNew();
        parallelStore.Execute(s => { s.Containers.TryGetValue(left,out _); Thread.Sleep(350); return 0; });
        singleWatch.Stop();
        using (var bothInside = new Barrier(2))
        {
            var parallelWatch = System.Diagnostics.Stopwatch.StartNew();
            var simultaneous = new[] { left, right }.Select(container => Task.Run(() => parallelStore.Execute(s =>
            {
                Check(s.Containers.TryGetValue(container,out _), "parallel container exists");
                var entered = bothInside.SignalAndWait(TimeSpan.FromSeconds(3));
                Thread.Sleep(350);
                return entered;
            }))).ToArray();
            Task.WaitAll(simultaneous);
            parallelWatch.Stop();
            Check(simultaneous.All(t => t.Result) && parallelWatch.Elapsed < 2*singleWatch.Elapsed,
                "distinct containers execute concurrently");
        }
        var sameOp = new[] { 0, 1 }.Select(_ => Task.Run(() => ledger.Grant("parallel-same-op",left,0,item,1,"fixture","parallel"))).ToArray();
        Task.WaitAll(sameOp);
        Check(sameOp[0].Result.OperationId == sameOp[1].Result.OperationId &&
            sameOp[0].Result.Revisions.SequenceEqual(sameOp[1].Result.Revisions) && ledger.Read(left).Revision == 1,
            "same operation replays after one commit");
        ledger.Grant("parallel-right-seed",right,0,item,1,"fixture","parallel");
        var opposing = new[] {
            Task.Run(() => { try { ledger.Transfer("parallel-left-to-right",left,1,right,1,item,1); return true; } catch (LedgerRejectedException) { return false; } }),
            Task.Run(() => { try { ledger.Transfer("parallel-right-to-left",right,1,left,1,item,1); return true; } catch (LedgerRejectedException) { return false; } })
        };
        Task.WaitAll(opposing);
        Check(opposing.Count(t => t.Result) == 1, "opposite transfers finish without deadlock");
        break;
    case "outage":
        try { ledger.Grant("outage-1", id, 3, item, 1, "fixture", "fixture-2"); throw new Exception("outage accepted mutation"); }
        catch (LedgerStoreUnavailableException) { Console.WriteLine("PASS outage fails closed with retryable error"); }
        break;
    case "recover":
        Check(ledger.Read(id).Revision == 3 && ledger.Read(id).Quantities[item] == 3, "outage left no mutation");
        var recovered = ledger.Grant("outage-1", id, 3, item, 1, "fixture", "fixture-2");
        Check(recovered.Revisions[0].After == 4 && ledger.Read(id).Quantities[item] == 4, "same operation recovers");
        break;
    case "classification":
        var store = new PostgresInventoryLedgerStore(connection);
        try
        {
            store.Execute(state =>
            {
                var invalid = new ContainerSpec("ctn_invalid", OwnerKind.Character, "chr_durable", ContainerKind.Backpack, 0);
                state.Containers.Add(invalid.Id, new ContainerRecord(invalid));
                return true;
            });
            throw new Exception("constraint violation was accepted");
        }
        catch (LedgerRejectedException) { Console.WriteLine("PASS SQL class 23 is non-retryable"); }
        Check(ledger.Read(id).Revision == 4, "constraint rejection rolls back");
        break;
    case "outbox-order":
        var outboxStore = new PostgresInventoryLedgerStore(connection);
        var outboxLedger = new InventoryLedger(outboxStore);
        outboxLedger.CreateContainer(new("ctn_outbox", OwnerKind.Character, "chr_outbox", ContainerKind.Backpack, 100));
        for (var i = 1; i <= 3; i++)
        {
            var op = $"outbox-{i}";
            var payload = Encoding.UTF8.GetBytes(op);
            var intent = new PostgresInventoryLedgerStore.FollowUpIntent(op, "chr_outbox", 1, payload,
                Convert.ToHexStringLower(SHA256.HashData(payload)));
            outboxStore.WithGrantIntent(op, intent, () => outboxLedger.Grant(op, "ctn_outbox", i - 1, item, 1, "fixture", op));
        }
        var first = outboxStore.ClaimOutbox("worker-a", 32, 12);
        Check(first.Count == 1 && first[0].OperationId == "outbox-1", "first character receipt claimed alone");
        Check(outboxStore.ClaimOutbox("worker-b", 32, 12).Count == 0, "leased earlier receipt blocks later receipt");
        outboxStore.CompleteOutbox("outbox-1", "worker-a");
        var second = outboxStore.ClaimOutbox("worker-a", 32, 12);
        Check(second.Count == 1 && second[0].OperationId == "outbox-2", "next receipt follows delivery");
        outboxStore.FailOutbox("outbox-2", "worker-a", "INVALID_ARGUMENT", true, 12);
        Check(outboxStore.ClaimOutbox("worker-b", 32, 12).Count == 0 && outboxStore.OutboxCounts() == (1, 1),
            "parked receipt blocks later character work");
        outboxStore.RequeueParkedOutbox("outbox-2");
        Check(outboxStore.OutboxCounts() == (2, 0), "repair returns parked receipt to pending");
        var repaired = outboxStore.ClaimOutbox("worker-b", 32, 12);
        Check(repaired.Count == 1 && repaired[0].OperationId == "outbox-2" &&
            repaired[0].Payload.SequenceEqual(Encoding.UTF8.GetBytes("outbox-2")), "repair retains original bytes and order");
        outboxStore.CompleteOutbox("outbox-2", "worker-b");
        Check(outboxStore.ClaimOutbox("worker-b", 32, 12).Single().OperationId == "outbox-3",
            "later receipt resumes after repaired receipt");
        break;
    case "post-backup-write":
        ledger.Grant("after-backup", id, 4, item, 2, "fixture", "fixture-3");
        Check(ledger.Read(id).Revision == 5 && ledger.Read(id).Quantities[item] == 6, "source advanced after backup");
        break;
    case "restored":
        var restored = ledger.Read(id);
        Check(restored.Revision == 4 && restored.Quantities[item] == 4 && restored.Quantities[bar] == 1, "restore equals backup point, not later source");
        var replay = ledger.Grant("grant-1", id, 0, item, 6, "fixture", "fixture-1");
        Check(replay.Revisions[0].After == 1 && ledger.Read(id).Revision == 4, "restored receipt replays without mutation");
        break;
    case "newer-schema":
        try { ledger.Grant("blocked-newer", id, 4, item, 1, "fixture", "fixture-4"); throw new Exception("newer schema accepted write"); }
        catch (LedgerSchemaCompatibilityException) { Console.WriteLine("PASS newer schema refuses runtime write"); }
        break;
    default: throw new ArgumentException("Unknown test phase.");
}
