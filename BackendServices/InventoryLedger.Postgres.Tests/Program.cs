using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;
using System.Security.Cryptography;
using System.Text;

static void Check(bool ok, string name)
{
    if (!ok) throw new Exception(name);
    Console.WriteLine("PASS " + name);
}
static void Reject(Action action, string name)
{
    try { action(); }
    catch (LedgerRejectedException) { Console.WriteLine("PASS " + name); return; }
    throw new Exception(name);
}
var connection = Environment.GetEnvironmentVariable("INVENTORY_LEDGER_CONNECTION") ?? throw new InvalidOperationException("Missing fixture connection.");
var ledger = new InventoryLedger(new PostgresInventoryLedgerStore(connection));
const string id = "ctn_durable";
const string item = "item/ore";
const string bar = "item/bar";
switch (args[0])
{
    case "world-loot-seed": WorldLootDurableChecks.Seed(connection); break;
    case "world-loot-recover": WorldLootDurableChecks.Recover(connection); break;
    case "death-wear-seed":
    {
        const string character = "chr_death_wear";
        const string equipment = "ctn_death_wear_equipment";
        const string gear = "item/hide_jerkin";
        const string secondGear = "item/iron_shortsword";
        var conditionStore = new PostgresInventoryLedgerStore(connection);
        var legacy = conditionStore.ReadGearInstances("ctn_legacy_gear");
        Check(legacy.Count == 2 && legacy.Select(x => x.Id).Distinct().Count() == 2 &&
            legacy.All(x => x.DefinitionId == gear && x.DurabilityPercent == 65),
            "migration expands each legacy stack copy at stored durability");
        var legacyIds = legacy.Select(x => x.Id).Order().ToArray();
        Check(legacyIds.SequenceEqual(conditionStore.ReadGearInstances("ctn_legacy_gear").Select(x => x.Id).Order()),
            "migration replay retains instance IDs");
        const string outputs = "ctn_death_wear_outputs";
        ledger.CreateContainer(new(outputs, OwnerKind.Character, character, ContainerKind.Backpack, 16));
        var bundle = ledger.GrantBundle("death-quest-bundle", outputs, 0,
            [new("item/hide_cap", 1), new("item/ore", 1)], "quest_reward", "quest-reward");
        var craft = ledger.Convert("death-graded-craft", outputs, 1, "recipe/gloves",
            [new("item/ore", 1)], [new("item/hide_gloves/fine", 1)]);
        Check(bundle.InstanceIds?.Count == 1 && craft.InstanceIds?.Count == 1 &&
            conditionStore.ReadGearInstances(outputs).Count == 2,
            "bundle and graded craft output commit separate gear instances");
        const string backpack = "ctn_death_wear_pack";
        ledger.CreateContainer(new(backpack, OwnerKind.Character, character, ContainerKind.Backpack, 16));
        var twoCopies = ledger.Grant("death-two-copy-grant", backpack, 0, gear, 2, "fixture", "death-two-copy");
        Check(twoCopies.InstanceIds?.Count == 2 && twoCopies.InstanceIds.Distinct().Count() == 2 &&
            twoCopies.InstanceIds.SequenceEqual(ledger.Grant("death-two-copy-grant", backpack, 0, gear, 2, "fixture", "death-two-copy").InstanceIds!),
            "two gear copies and grant replay retain distinct identities");
        ledger.CreateContainer(new(equipment, OwnerKind.Character, character, ContainerKind.Equipment, 16));
        var firstId = twoCopies.InstanceIds![0]; var secondId = twoCopies.InstanceIds[1];
        var equip = ledger.Transfer("death-equip-first", backpack, 1, equipment, 0, gear, 1, firstId);
        Check(equip.InstanceIds!.SequenceEqual([firstId]) &&
            ledger.Transfer("death-equip-first", backpack, 1, equipment, 0, gear, 1, firstId).InstanceIds!.SequenceEqual([firstId]),
            "selected gear instance survives transfer replay");
        Reject(() => ledger.Transfer("death-equip-first", backpack, 1, equipment, 0, gear, 1, secondId),
            "replay cannot select another copy");
        ledger.Grant("death-second-gear-grant", equipment, 1, secondGear, 1, "fixture", "death-second-gear");
        string Event(int n) => $"00000000-0000-0000-0000-{n:000000000000}";
        Check(conditionStore.ApplyDeathWear(Event(1), character, 9, 10, 5) == 0 &&
            conditionStore.ReadGearCondition(equipment)[gear] == 100, "level below 10 has no death wear");
        Check(conditionStore.ApplyDeathWear(Event(2), character, 10, 10, 5) == 2 &&
            conditionStore.ReadGearCondition(equipment)[gear] == 95 &&
            conditionStore.ReadGearCondition(equipment)[secondGear] == 95 &&
            conditionStore.ReadGearInstances(backpack).Single(x => x.Id == secondId).DurabilityPercent == 100,
            "level 10 death wears equipped copies but leaves the bag copy pristine");
        Check(conditionStore.ApplyDeathWear(Event(2), character, 10, 10, 5) == 0 &&
            conditionStore.ReadGearCondition(equipment)[gear] == 95, "death event replay applies no additional wear");
        Check(conditionStore.ApplyDeathWear(Event(2), character, 10, 20, 7) == 0 &&
            conditionStore.ReadGearCondition(equipment)[gear] == 95,
            "replay retains the original event tuning after a later tuning change");
        ledger.Transfer("death-unequip-first", equipment, 2, backpack, 2, gear, 1, firstId);
        ledger.Transfer("death-equip-second", backpack, 3, equipment, 3, gear, 1, secondId);
        conditionStore.ApplyDeathWear(Event(3), character, 10, 10, 5);
        Check(conditionStore.ReadGearInstances(equipment).Single(x => x.Id == secondId).DurabilityPercent == 95 &&
            conditionStore.ReadGearInstances(backpack).Single(x => x.Id == firstId).DurabilityPercent == 95,
            "two copies wear independently across equip swaps");
        ledger.Transfer("death-unequip-second", equipment, 4, backpack, 4, gear, 1, secondId);
        ledger.Transfer("death-reequip-first", backpack, 5, equipment, 5, gear, 1, firstId);
        conditionStore.ApplyDeathWear(Event(4), character, 10, 10, 5);
        Check(conditionStore.ReadGearInstances(equipment).Single(x => x.Id == firstId).DurabilityPercent == 90 &&
            conditionStore.ReadGearInstances(backpack).Single(x => x.Id == secondId).DurabilityPercent == 95,
            "wear follows the selected instance, not the definition");
        for (int n = 5; n <= 22; ++n) conditionStore.ApplyDeathWear(Event(n), character, 10, 10, 5);
        Check(conditionStore.ReadGearCondition(equipment)[gear] == 0 &&
            conditionStore.ReadGearCondition(equipment)[secondGear] == 0 &&
            ledger.Read(equipment).Quantities[gear] == 1 && ledger.Read(equipment).Quantities[secondGear] == 1,
            "twenty deaths floor at zero without dropping or unequipping gear");
        conditionStore.ApplyDeathWear(Event(23), character, 10, 10, 5);
        Check(conditionStore.ReadGearCondition(equipment)[gear] == 0, "further death cannot make durability negative");
        break;
    }
    case "death-wear-restart":
    {
        const string character = "chr_death_wear";
        const string equipment = "ctn_death_wear_equipment";
        const string gear = "item/hide_jerkin";
        var conditionStore = new PostgresInventoryLedgerStore(connection);
        Check(conditionStore.ReadGearInstances("ctn_legacy_gear").Count == 2 &&
            conditionStore.ReadGearInstances("ctn_legacy_gear").All(x => x.DurabilityPercent == 65),
            "migrated copies survive database restart");
        Check(conditionStore.ReadGearInstances("ctn_death_wear_outputs").Count == 2 &&
            ledger.Receipt("death-graded-craft")?.InstanceIds?.Count == 1,
            "bundle and graded craft identities survive restart");
        Check(conditionStore.ReadGearCondition(equipment)[gear] == 0 && ledger.Read(equipment).Quantities[gear] == 1,
            "zero condition and equipment ownership survive database restart");
        Check(conditionStore.ApplyDeathWear("00000000-0000-0000-0000-000000000002", character, 10, 10, 5) == 0 &&
            conditionStore.ReadGearCondition(equipment)[gear] == 0, "death replay after restart remains idempotent");
        break;
    }
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
