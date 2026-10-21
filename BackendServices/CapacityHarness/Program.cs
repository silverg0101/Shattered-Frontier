using System.Collections.Concurrent;
using System.Diagnostics;
using System.Text.Json;
using Grpc.Core;
using Grpc.Net.Client;
using Npgsql;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;
using C = ShatteredFrontier.Crafting.V1;
using E = ShatteredFrontier.Exchange.V1;
using H = ShatteredFrontier.Housing.V1;
using P = ShatteredFrontier.Progression.V1;
using R = ShatteredFrontier.Rewards.V1;

if (args.Length == 1 && args[0] == "journey") { JourneySeed.Run(); return; }

static int Option(string[] args, string name, int fallback, int min, int max)
{
    var at = Array.IndexOf(args, name);
    var value = at < 0 ? fallback : at + 1 < args.Length && int.TryParse(args[at + 1], out var parsed) ? parsed : throw new ArgumentException($"Invalid {name}");
    if (value < min || value > max) throw new ArgumentOutOfRangeException(name, $"Allowed {min}..{max}");
    return value;
}
var participants = Option(args, "--participants", 10, 1, 100);
var warmup = Option(args, "--warmup-seconds", 30, 0, 300);
var seconds = Option(args, "--seconds", 900, 1, 7200);
var think = Option(args, "--think-ms", 1000, 100, 5000);
var seed = Option(args, "--seed", 20261021, 0, int.MaxValue);
var port = Environment.GetEnvironmentVariable("GAMEPLAY_HOST_PORT") ?? throw new Exception("GAMEPLAY_HOST_PORT required");
var token = Environment.GetEnvironmentVariable("SF_GAMEPLAY_TOKEN") ?? throw new Exception("SF_GAMEPLAY_TOKEN required");
var connection = Environment.GetEnvironmentVariable("GAMEPLAY_LEDGER_CONNECTION") ?? throw new Exception("GAMEPLAY_LEDGER_CONNECTION required");
var output = Environment.GetEnvironmentVariable("CAPACITY_OUTPUT") ?? throw new Exception("CAPACITY_OUTPUT required");
var ledger = new InventoryLedger(new PostgresInventoryLedgerStore(connection));
var samples = new ConcurrentBag<Sample>();
var gatherCount = participants * 40 / 100;
var buildCount = participants * 20 / 100;
var fightCount = participants * 20 / 100;
var tradeCount = (participants + 5) * 10 / 100;
string Role(int i) => i < gatherCount ? "gather/craft" :
    i < gatherCount + buildCount ? "build" :
    i < gatherCount + buildCount + fightCount ? "fight/quest" :
    i < gatherCount + buildCount + fightCount + tradeCount ? "trade" : "reconnect";
var runId = $"cap_{participants}_{seed}";
var measureStart = DateTimeOffset.MaxValue;
var end = DateTimeOffset.MaxValue;
var counts = new ConcurrentDictionary<string, int>();
var staleLogged = new ConcurrentDictionary<string, byte>();
using var claimSeedGate = new SemaphoreSlim(8);
var ready = 0;
var startGate = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);

async Task Participant(int index)
{
    var id = $"chr_{runId}_{index}";
    var bag = $"ctn_{runId}_{index}";
    var escrow = $"ctn_esc_{runId}_{index}";
    var role = Role(index);
    var rng = new Random(unchecked(seed * 397 ^ index));
    ledger.CreateContainer(new ContainerSpec(bag, OwnerKind.Character, id, ContainerKind.Backpack, 10000));
    if (role == "trade") ledger.CreateContainer(new ContainerSpec(escrow, OwnerKind.Character, id, ContainerKind.Escrow, 10000));
    using var channel = GrpcChannel.ForAddress($"http://127.0.0.1:{port}");
    var headers = new Metadata { { "authorization", "Bearer " + token } };
    var craft = new C.Crafting.CraftingClient(channel);
    var house = new H.Housing.HousingClient(channel);
    var reward = new R.Rewards.RewardsClient(channel);
    var progress = new P.Progression.ProgressionClient(channel);
    var exchange = new E.Exchange.ExchangeClient(channel);
    string claimId = $"clm_{runId}_{index}";
    if (role == "build")
    {
        var slot = index - gatherCount;
        var east = participants >= 50;
        var north = participants is 25 or 100;
        var x = (east ? 1150 : -1150) + (east ? -1 : 1) * (slot % 5) * 55;
        var y = (north ? 1100 : -1100) + (north ? -1 : 1) * (slot / 5) * 55;
        await claimSeedGate.WaitAsync();
        try
        {
            await Record("Housing.ClaimPlot", () => house.ClaimPlotAsync(new H.ClaimPlotRequest { OperationId = $"claim/{runId}/{index}", ClaimId = claimId,
                OwnerId = id, CenterX = x, CenterY = y, SideMetres = 40, PrepaidDays = 1 }, headers, deadline: DateTime.UtcNow.AddSeconds(10)).ResponseAsync);
        }
        finally { claimSeedGate.Release(); }
    }
    if (Interlocked.Increment(ref ready) == participants)
    {
        measureStart = DateTimeOffset.UtcNow.AddSeconds(warmup);
        end = measureStart.AddSeconds(seconds);
        startGate.SetResult();
    }
    await startGate.Task;
    // Spread the first cycle across the think interval after the setup barrier.
    await Task.Delay((int)((long)index * think / participants));
    var cycle = 0;
    while (DateTimeOffset.UtcNow < end)
    {
        cycle++;
        var op = $"{runId}/{index}/{cycle}";
        try
        {
            if (role == "gather/craft")
            {
                ledger.Grant($"fixture/gather/{op}", bag, ledger.Read(bag).Revision, "item/oak_log", 2, "gathering", op);
                await Record("Progression.ApplyReceipt", () => progress.ApplyReceiptAsync(new P.ApplyReceiptRequest { SourceReceiptId = $"gather/{op}", Kind = "gather", CharacterId = id,
                    ExpectedProgressionRevision = counts.GetOrAdd(id, 0), Payload = new P.ActionPayload { SkillId = "skill/logging", Difficulty = 1, ActionSeconds = 6, Success = true } }, headers,
                    deadline: DateTime.UtcNow.AddSeconds(10)).ResponseAsync, result => counts[id] = checked((int)result.ProgressionRevision));
                await Record("Crafting.Craft", () => craft.CraftAsync(new C.CraftRequest { OperationId = $"craft/{op}", CharacterId = id, ContainerId = bag,
                    RecipeId = "recipe/rough_timber", Count = 1, ExpectedContainerRevision = ledger.Read(bag).Revision }, headers, deadline: DateTime.UtcNow.AddSeconds(10)).ResponseAsync);
            }
            else if (role == "build")
            {
                ledger.Grant($"fixture/bundle/{op}", bag, ledger.Read(bag).Revision, "item/wall_material_bundle", 1, "craft", op);
                var place = await Record("Housing.Place", () => house.PlaceAsync(new H.PlaceRequest { OperationId = $"place/{op}", ClaimId = claimId, ActorId = id,
                    StructureId = $"str_{op.Replace('/', '_')}", ContentId = "piece/wall", MaterialId = "item/wall_material_bundle", MaterialQuantity = 1,
                    ContainerId = bag, CellX = (cycle - 1) % 30, CellY = ((cycle - 1) / 30) % 30, CellZ = 0, ExpectedStructureRevision = counts.GetOrAdd(id, 0),
                    ExpectedContainerRevision = ledger.Read(bag).Revision }, headers, deadline: DateTime.UtcNow.AddSeconds(10)).ResponseAsync,
                    result => counts[id] = checked((int)result.StructureRevision));
                await Record("Housing.Remove", () => house.RemoveAsync(new H.RemoveRequest { OperationId = $"remove/{op}", ClaimId = claimId, ActorId = id,
                    StructureId = place.StructureId, ExpectedStructureRevision = counts[id], ExpectedContainerRevision = ledger.Read(bag).Revision,
                    RemovalKind = "undo" }, headers, deadline: DateTime.UtcNow.AddSeconds(10)).ResponseAsync,
                    result => counts[id] = checked((int)result.StructureRevision));
            }
            else if (role == "fight/quest")
            {
                await Record("Rewards.GrantDefeatReward", () => reward.GrantDefeatRewardAsync(new R.GrantDefeatRewardRequest { DefeatEventId = $"defeat/{op}",
                    KillerCharacterId = id, RecipientCharacterId = id, ContainerId = bag, ExpectedContainerRevision = ledger.Read(bag).Revision,
                    TableId = "loot/greenlands_silverbrook_boar" }, headers, deadline: DateTime.UtcNow.AddSeconds(10)).ResponseAsync);
                await Record("Progression.ApplyReceipt", () => progress.ApplyReceiptAsync(new P.ApplyReceiptRequest { SourceReceiptId = $"defeat/{op}", Kind = "defeat", CharacterId = id,
                    ExpectedProgressionRevision = counts.GetOrAdd(id, 0), Payload = new P.ActionPayload { SkillId = "", Difficulty = 1, ActionSeconds = 6,
                        Success = true, EnemyLevel = 1, EnemyTierMultiplier = 1, EligiblePartySize = 1 } }, headers, deadline: DateTime.UtcNow.AddSeconds(10)).ResponseAsync,
                    result => counts[id] = checked((int)result.ProgressionRevision));
            }
            else if (role == "trade")
            {
                ledger.Grant($"fixture/trade/{op}", bag, ledger.Read(bag).Revision, "item/oak_log", 1, "gathering", op);
                var listing = $"lst_{op.Replace('/', '_')}";
                await Record("Exchange.ListItem", () => exchange.ListItemAsync(new E.ListItemRequest { OperationId = $"list/{op}", ListingId = listing, SellerId = id,
                    SourceContainerId = bag, EscrowContainerId = escrow, ItemId = "item/oak_log", Quantity = 1, UnitPrice = 1, ListingFee = 0,
                    CreatedAtUnixMs = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() }, headers, deadline: DateTime.UtcNow.AddSeconds(10)).ResponseAsync);
                await Record("Exchange.CancelListing", () => exchange.CancelListingAsync(new E.ListingActionRequest { OperationId = $"cancel/{op}", ListingId = listing,
                    SellerId = id }, headers, deadline: DateTime.UtcNow.AddSeconds(10)).ResponseAsync);
                await Record("Exchange.Wallet", () => exchange.WalletAsync(new E.WalletRequest { OwnerId = id }, headers, deadline: DateTime.UtcNow.AddSeconds(10)).ResponseAsync);
            }
            else
            {
                using var fresh = GrpcChannel.ForAddress($"http://127.0.0.1:{port}");
                await Record("Progression.GetCharacterState", () => new P.Progression.ProgressionClient(fresh).GetCharacterStateAsync(
                    new P.GetCharacterStateRequest { CharacterId = id }, headers, deadline: DateTime.UtcNow.AddSeconds(10)).ResponseAsync);
            }
        }
        catch (RpcException ex)
        {
            if (role == "build")
            {
                var expected = counts.GetOrAdd(id, 0);
                try
                {
                    var actual = await house.ListStructuresAsync(new H.ListStructuresRequest { ClaimId = claimId }, headers,
                        deadline: DateTime.UtcNow.AddSeconds(10));
                    counts[id] = checked((int)actual.StructureRevision);
                    if (staleLogged.TryAdd(id, 0))
                        Console.Error.WriteLine($"CAPACITY_BUILDER_REFRESH claim={claimId} cycle={cycle} status={ex.StatusCode} expected={expected} actual={actual.StructureRevision}");
                }
                catch (RpcException) { if (staleLogged.TryAdd(id, 0)) Console.Error.WriteLine($"CAPACITY_BUILDER_REFRESH claim={claimId} cycle={cycle} actual=unavailable"); }
            }
            else if (role is "gather/craft" or "fight/quest")
            {
                try
                {
                    var actual = await progress.GetCharacterStateAsync(new P.GetCharacterStateRequest { CharacterId = id }, headers,
                        deadline: DateTime.UtcNow.AddSeconds(10));
                    counts[id] = checked((int)actual.ProgressionRevision);
                }
                catch (RpcException) { /* Retry the read after the next ambiguous call. */ }
            }
        }
        catch (Exception ex) { samples.Add(new Sample("cycle", -1, ex.GetType().Name + ": " + ex.Message, DateTimeOffset.UtcNow >= measureStart)); }
        await Task.Delay(think + rng.Next(-think / 4, think / 4 + 1));
    }
}

async Task<T> Record<T>(string rpc, Func<Task<T>> action, Action<T>? after = null)
{
    var sw = Stopwatch.StartNew();
    try
    {
        await Task.Delay(25);
        var result = await action();
        await Task.Delay(25);
        sw.Stop();
        after?.Invoke(result);
        samples.Add(new Sample(rpc, sw.Elapsed.TotalMilliseconds, "OK", DateTimeOffset.UtcNow >= measureStart));
        return result;
    }
    catch (RpcException ex)
    {
        sw.Stop(); samples.Add(new Sample(rpc, sw.Elapsed.TotalMilliseconds, ex.StatusCode + ": " + ex.Status.Detail, DateTimeOffset.UtcNow >= measureStart)); throw;
    }
}

await Task.WhenAll(Enumerable.Range(0, participants).Select(Participant));
var measured = samples.Where(x => x.Measured).ToArray();
var audit = await Audit(connection);
var auditDrainSeconds = 0;
while (!audit.Passed && auditDrainSeconds < 30)
{
    await Task.Delay(2000);
    auditDrainSeconds += 2;
    audit = await Audit(connection);
}
var report = new
{
    participants, seed, warmupSeconds = warmup, measuredSeconds = seconds, thinkMilliseconds = think,
    auditDrainSeconds,
    roles = Enumerable.Range(0, participants).GroupBy(Role).ToDictionary(g => g.Key, g => g.Count()),
    rpcs = measured.GroupBy(x => x.Rpc).OrderBy(g => g.Key).Select(g => new { rpc = g.Key, calls = g.Count(), accepted = g.Count(x => x.Status == "OK"),
        errors = g.Count(x => x.Status != "OK"), errorRate = Math.Round(g.Count(x => x.Status != "OK") / (double)g.Count(), 4),
        unavailable = g.Count(x => x.Status.StartsWith("Unavailable") || x.Status.StartsWith("DeadlineExceeded")), unavailableRate = Math.Round(g.Count(x => x.Status.StartsWith("Unavailable") || x.Status.StartsWith("DeadlineExceeded")) / (double)g.Count(), 4),
        errorKinds = g.Where(x => x.Status != "OK").GroupBy(x => x.Status).ToDictionary(x => x.Key, x => x.Count()),
        p50Ms = Percentile(g.Where(x => x.Status == "OK").Select(x => x.Ms), .50), p95Ms = Percentile(g.Where(x => x.Status == "OK").Select(x => x.Ms), .95),
        p99Ms = Percentile(g.Where(x => x.Status == "OK").Select(x => x.Ms), .99) }).ToArray(),
    acceptedPerSecond = measured.Count(x => x.Status == "OK") / (double)seconds, audit
};
await File.WriteAllTextAsync(output, JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }));
Console.WriteLine($"participants={participants} accepted={measured.Count(x => x.Status == "OK")} errors={measured.Count(x => x.Status != "OK")} audit={audit.Passed}");
if (!audit.Passed || measured.Length == 0 || measured.Count(x => x.Status != "OK") / (double)measured.Length >= .01)
    Environment.ExitCode = 1;

static double Percentile(IEnumerable<double> values, double p)
{
    var ordered = values.Order().ToArray();
    return ordered.Length == 0 ? 0 : Math.Round(ordered[(int)Math.Ceiling(p * ordered.Length) - 1], 2);
}

static async Task<AuditResult> Audit(string connection)
{
    await using var db = new NpgsqlConnection(connection);
    await db.OpenAsync();
    var checks = new Dictionary<string, long>();
    async Task Check(string name, string sql)
    {
        await using var cmd = new NpgsqlCommand(sql, db);
        checks[name] = Convert.ToInt64(await cmd.ExecuteScalarAsync());
    }
    await Check("duplicate ledger operation ids", "SELECT count(*) FROM (SELECT operation_id FROM inventory.operation_receipts GROUP BY operation_id HAVING count(*)>1) x");
    await Check("duplicate housing/exchange/quest operation ids", "SELECT count(*) FROM (SELECT operation_id FROM housing.placements GROUP BY operation_id HAVING count(*)>1 UNION ALL SELECT operation_id FROM exchange.operations GROUP BY operation_id HAVING count(*)>1 UNION ALL SELECT operation_id FROM character_state.reward_receipts GROUP BY operation_id HAVING count(*)>1) x");
    await Check("inventory receipt quantity mismatches", "WITH changes AS (SELECT e->>'ContainerId' AS container_id,e->>'DefinitionId' AS item,sum((e->>'Delta')::bigint) AS qty FROM inventory.operation_receipts r CROSS JOIN LATERAL jsonb_array_elements(r.receipt->'Changes') e GROUP BY 1,2), actual AS (SELECT container_id,definition_id AS item,quantity FROM inventory.entries) SELECT count(*) FROM changes FULL JOIN actual USING(container_id,item) WHERE coalesce(changes.qty,0)<>coalesce(actual.quantity,0)");
    await Check("housing debit mismatches", "SELECT count(*) FROM housing.placements p LEFT JOIN inventory.operation_receipts r ON r.operation_id=p.operation_id||'/consume' WHERE p.state='Pending' OR (p.state IN ('Committed','Removed') AND NOT EXISTS(SELECT 1 FROM jsonb_array_elements(r.receipt->'Changes') e WHERE e->>'ContainerId'=p.container_id AND e->>'DefinitionId'=p.material_id AND (e->>'Delta')::bigint=-p.material_quantity))");
    await Check("housing refund mismatches", "SELECT count(*) FROM housing.removals m JOIN housing.placements p ON p.operation_id=m.placement_operation_id LEFT JOIN inventory.operation_receipts r ON r.operation_id=m.operation_id||'/refund' WHERE m.state='Pending' OR (m.state='Committed' AND m.refund_quantity>0 AND NOT EXISTS(SELECT 1 FROM jsonb_array_elements(r.receipt->'Changes') e WHERE e->>'ContainerId'=p.container_id AND e->>'DefinitionId'=p.material_id AND (e->>'Delta')::bigint=m.refund_quantity))");
    await Check("exchange listing movement mismatches", "SELECT count(*) FROM exchange.listings l LEFT JOIN exchange.operations o ON o.receipt->>'SubjectId'=l.id AND o.operation_id LIKE 'list/%' LEFT JOIN inventory.operation_receipts r ON r.operation_id=o.operation_id||'/escrow' LEFT JOIN exchange.operations c ON c.receipt->>'SubjectId'=l.id AND c.operation_id LIKE 'cancel/%' LEFT JOIN inventory.operation_receipts back ON back.operation_id=c.operation_id||'/return' WHERE l.state='pending' OR (l.state IN ('active','cancelled','expired','sold') AND (NOT EXISTS(SELECT 1 FROM jsonb_array_elements(r.receipt->'Changes') e WHERE e->>'ContainerId'=l.seller_container_id AND e->>'DefinitionId'=l.item_id AND (e->>'Delta')::bigint=-l.quantity) OR NOT EXISTS(SELECT 1 FROM jsonb_array_elements(r.receipt->'Changes') e WHERE e->>'ContainerId'=l.escrow_id AND e->>'DefinitionId'=l.item_id AND (e->>'Delta')::bigint=l.quantity))) OR (l.state='cancelled' AND (NOT EXISTS(SELECT 1 FROM jsonb_array_elements(back.receipt->'Changes') e WHERE e->>'ContainerId'=l.escrow_id AND e->>'DefinitionId'=l.item_id AND (e->>'Delta')::bigint=-l.quantity) OR NOT EXISTS(SELECT 1 FROM jsonb_array_elements(back.receipt->'Changes') e WHERE e->>'ContainerId'=l.seller_container_id AND e->>'DefinitionId'=l.item_id AND (e->>'Delta')::bigint=l.quantity)))");
    await Check("quest reward mismatches", "SELECT count(*) FROM character_state.reward_receipts q LEFT JOIN inventory.operation_receipts r ON r.operation_id=q.operation_id WHERE (q.state='Complete' AND r.operation_id IS NULL) OR q.state='Pending' OR EXISTS(SELECT 1 FROM character_state.reward_receipts q2 WHERE q2.character_id=q.character_id AND q2.quest_id=q.quest_id AND q2.step_id=q.step_id AND q2.bundle_id=q.bundle_id AND q2.operation_id<>q.operation_id)");
    await Check("missing completed quest coverage", "SELECT CASE WHEN EXISTS(SELECT 1 FROM character_state.reward_receipts WHERE state='Complete') THEN 0 ELSE 1 END");
    await Check("wallet entry sum mismatches", "SELECT count(*) FROM exchange.wallets w LEFT JOIN (SELECT owner_id,sum(delta) AS total,max(sequence) AS last FROM exchange.wallet_entries GROUP BY owner_id) e USING(owner_id) WHERE w.balance<>coalesce(e.total,0) OR w.revision<>coalesce(e.last,0)");
    return new AuditResult(checks.Values.All(x => x == 0), checks);
}

record Sample(string Rpc, double Ms, string Status, bool Measured);
record AuditResult(bool Passed, Dictionary<string, long> Checks);
