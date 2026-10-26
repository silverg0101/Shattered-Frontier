using ShatteredFrontier.GatheringNodes;
using ShatteredFrontier.WorldContent;

var passed = 0;
var path = args.Length == 1 ? args[0] : "content/world/zones/greenlands_silverbrook.json";
var zone = ZoneLoader.Load(path);
var clock = new ManualClock();
var definitions = new[]
{
    new NodeDefinition("gathering_node/oak_tree", ResourceKind.Wood, "resource/oak_wood", 2, 3),
    new NodeDefinition("gathering_node/stone_outcrop", ResourceKind.Ore, "resource/stone", 1, 2)
};
var settings = new NodeSettings(30, .1, RegrowthMode.FullRespawn);
// The shipped record makes every tree and rock a node (K = N): each is pinned to its own point and regrows there.
var everyNode = new GatheringNodeService(zone, definitions, clock, "silverbrook-test", settings);
var gathering = zone.SpawnGroups.Where(g => g.SpawnKind == "gathering_node" && !g.GroupId.EndsWith("_iron_ore")).ToArray();
Check(everyNode.Snapshot().Count == gathering.Sum(g => g.Points.Count), "every tree and rock point is an active node");
Check(everyNode.Snapshot().All(n => n.Id.EndsWith("/" + gathering.Single(g => g.GroupId == n.GroupId).Points
    .FindIndex(p => p.PointId == n.PointId))), "node i stands on point i");
var pinned = everyNode.Snapshot().First(n => n.Kind == ResourceKind.Ore);
var pinnedCut = everyNode.Harvest(new("pinned", "p1", pinned.Id, 1));
clock.Set(pinnedCut.RegrowAt!.Value);
Check(everyNode.Snapshot().Single(n => n.Id == pinned.Id) is { Generation: 2, Charges: 1 } renewedPin && renewedPin.PointId == pinned.PointId,
    "a K = N node regrows on its own point");
// Rotation (K < N) on the same record with half its points active.
foreach (var g in zone.SpawnGroups.Where(g => g.SpawnKind == "gathering_node")) g.ActiveCount = g.Points.Count / 2;
var resized = new GatheringNodeService(zone, definitions, clock, "silverbrook-test", settings);
resized.ImportState(everyNode.ExportState());
Check(resized.Snapshot().All(n => n is { Generation: 1 }), "a changed zone record starts every node fresh");
clock = new ManualClock();
var service = new GatheringNodeService(zone, definitions, clock, "silverbrook-test", settings);
var nodes = service.Snapshot();
Check(nodes.Any(n => n.Kind == ResourceKind.Wood) && nodes.Any(n => n.Kind == ResourceKind.Ore), "real wood and mined stone groups loaded");
Check(nodes.Count == gathering.Sum(g => g.Points.Count / 2), "active counts from zone record");
var ore = nodes.First(n => n.Kind == ResourceKind.Ore);
var first = service.Harvest(new("ore-first", "p1", ore.Id, ore.Generation));
Check(first.Status == HarvestStatus.Granted && first.Quantity == 2 && first.RemainingCharges == 0, "ore depletion and yield");
Check(service.Harvest(new("ore-first", "p1", ore.Id, ore.Generation)) == first, "retry returns identical receipt");
Check(service.Harvest(new("ore-depleted", "p2", ore.Id, ore.Generation)).Status == HarvestStatus.Depleted, "stable depleted reason");
Check(first.RegrowAt > clock.GetUtcNow().AddSeconds(27) && first.RegrowAt < clock.GetUtcNow().AddSeconds(33), "bounded jitter");
var secondService = new GatheringNodeService(zone, definitions, clock, "silverbrook-test", settings);
Check(secondService.Harvest(new("ore-first", "p1", ore.Id, ore.Generation)).RegrowAt == first.RegrowAt, "deterministic jitter");
clock.Set(first.RegrowAt!.Value);
var renewed = service.Snapshot().Single(n => n.Id == ore.Id);
Check(renewed.Generation == 2 && renewed.Charges == 1, "full respawn on injected clock");
Check(renewed.PointId != ore.PointId, "depleted slot rotates to another free pool point");
var activeBeforeRestart = service.Snapshot().Where(n => n.Charges > 0)
    .Select(n => (n.Id, n.PointId, n.Generation)).OrderBy(n => n.Id).ToArray();
var rotatedRestart = new GatheringNodeService(zone, definitions, clock, "silverbrook-test", settings);
rotatedRestart.ImportState(service.ExportState());
Check(rotatedRestart.Snapshot().Where(n => n.Charges > 0)
    .Select(n => (n.Id, n.PointId, n.Generation)).OrderBy(n => n.Id).SequenceEqual(activeBeforeRestart),
    "deplete, rotate, restart retains the same active point set");
Check(service.Harvest(new("old-generation", "p1", ore.Id, 1)).Status == HarvestStatus.StaleGeneration, "stale generation rejected");
Check(service.Harvest(new("ore-first", "p1", ore.Id, 1)) == first, "old retry retains original result");

var wood = service.Snapshot().First(n => n.Kind == ResourceKind.Wood);
var cut = service.Harvest(new("wood-first", "p1", wood.Id, wood.Generation));
Check(cut.Status == HarvestStatus.Granted && cut.Quantity == 3 && cut.RemainingCharges == 1, "wood yield and remaining charge");
var races = await Task.WhenAll(Task.Run(() => service.Harvest(new("race-a", "p1", wood.Id, wood.Generation))),
    Task.Run(() => service.Harvest(new("race-b", "p2", wood.Id, wood.Generation))));
Check(races.Count(r => r.Status == HarvestStatus.Granted) == 1 && races.Count(r => r.Status == HarvestStatus.Depleted) == 1,
    "last charge granted once");

var partialClock = new ManualClock();
var partial = new GatheringNodeService(zone, definitions, partialClock, "silverbrook-test", new(30, 0, RegrowthMode.PartialRegrowth));
var partialWood = partial.Snapshot().First(n => n.Kind == ResourceKind.Wood);
partial.Harvest(new("partial-1", "p1", partialWood.Id, 1));
partial.Harvest(new("partial-2", "p1", partialWood.Id, 1));
partialClock.Set(partialClock.GetUtcNow().AddSeconds(30));
Check(partial.Snapshot().Single(n => n.Id == partialWood.Id) is { Charges: 1, Generation: 2 }, "partial regrowth adds one charge");
partialClock.Set(partialClock.GetUtcNow().AddSeconds(30));
Check(partial.Snapshot().Single(n => n.Id == partialWood.Id) is { Charges: 2, Generation: 3, RegrowAt: null }, "partial regrowth reaches cap");
try { new NodeSettings(10, .1, RegrowthMode.FullRespawn).Validate(); throw new Exception("invalid settings accepted"); }
catch (ArgumentOutOfRangeException) { }

var ironProfile = zone.SpawnGroups.Single(g => g.GroupId == "eastwood_iron_ore").Definitions.Single().GrowthProfile!;
Check(WorldContentValidator.ValidGrowthProfile(ironProfile), "shipped growth profile valid");
var invalidProfiles = new[]
{
    new GrowthProfile(),
    new GrowthProfile { Stages = [new() { Name = "only", DurationFraction = 0, YieldFraction = 1 }] },
    new GrowthProfile { Stages = [new() { Name = "a", DurationFraction = 1, YieldFraction = .4 }, new() { Name = "b", YieldFraction = .4 }] },
    new GrowthProfile { Stages = [new() { Name = "a", DurationFraction = 1, YieldFraction = 0 }, new() { Name = "b", YieldFraction = 1 }] },
    new GrowthProfile { Stages = [new() { Name = "a", DurationFraction = 1, YieldFraction = .4 }, new() { Name = "b", YieldFraction = .9 }] },
    new GrowthProfile { Stages = [new() { Name = "a", DurationFraction = 1, YieldFraction = .6 }, new() { Name = "b", YieldFraction = 1 }] }
};
foreach (var invalid in invalidProfiles)
{
    Check(!WorldContentValidator.ValidGrowthProfile(invalid), "invalid growth profile rejected");
    var original = zone.SpawnGroups.Single(g => g.GroupId == "eastwood_iron_ore").Definitions.Single().GrowthProfile;
    zone.SpawnGroups.Single(g => g.GroupId == "eastwood_iron_ore").Definitions.Single().GrowthProfile = invalid;
    Check(WorldContentValidator.Validate(zone).Any(i => i.Contains("invalid growth_profile")), "zone validator rejects growth profile");
    zone.SpawnGroups.Single(g => g.GroupId == "eastwood_iron_ore").Definitions.Single().GrowthProfile = original;
}

var growthClock = new ManualClock();
var ironDefinition = new NodeDefinition("gathering_node/iron_vein", ResourceKind.Ore, "resource/iron", 5, 1);
var growth = new GatheringNodeService(zone, [ironDefinition], growthClock, "growth-test", new(30, 0, RegrowthMode.FullRespawn));
var iron = growth.Snapshot().First();
Check(iron.GrowthStage == "mature", "initial profiled node is mature");
for (var i = 0; i < 5; i++)
    Check(growth.Harvest(new($"iron-{i}", "p1", iron.Id, 1)).Status == HarvestStatus.Granted, "initial mature charge");
var maturity = growth.Snapshot().Single(n => n.Id == iron.Id).RegrowAt!.Value;
growthClock.Set(maturity.AddSeconds(-15.001));
Check(growth.Snapshot().Single(n => n.Id == iron.Id) is { Charges: 0, GrowthStage: null }, "not emerged before growth window");
growthClock.Set(maturity.AddSeconds(-15));
var young = growth.Snapshot().Single(n => n.Id == iron.Id);
Check(young is { Generation: 2, Charges: 2, GrowthStage: "young" }, "young stage and rounded charges");
Check(young.StageStartedAt == growthClock.GetUtcNow(), "stage start exposed");
Check(young.NextTransitionAt == growthClock.GetUtcNow().AddSeconds(7.5), "young stage transition deadline exposed");
Check(growth.Harvest(new("iron-young", "p1", iron.Id, 2)).RemainingCharges == 1, "young charge consumed");
var restartedGrowth = new GatheringNodeService(zone, [ironDefinition], growthClock, "growth-test", new(30, 0, RegrowthMode.FullRespawn));
restartedGrowth.ImportState(growth.ExportState());
Check(restartedGrowth.Snapshot().Single(n => n.Id == iron.Id) is
    { Generation: 2, Charges: 1, GrowthStage: "young" }, "mid-growth restart retains the consumed charge");
Check(restartedGrowth.Snapshot().Single(n => n.Id == iron.Id).NextTransitionAt == young.NextTransitionAt,
    "mid-growth restart retains the host-owned stage deadline");
growthClock.Set(maturity.AddSeconds(-7.5));
Check(growth.Snapshot().Single(n => n.Id == iron.Id) is { Charges: 2, GrowthStage: "growing" }, "half rounding and prior harvest retained");
Check(restartedGrowth.Snapshot().Single(n => n.Id == iron.Id) is { Charges: 2, GrowthStage: "growing" },
    "restarted node advances with the saved consumed charge");
growthClock.Set(maturity);
Check(growth.Snapshot().Single(n => n.Id == iron.Id) is { Charges: 4, GrowthStage: "mature" }, "maturity reaches full cap less prior harvest");
Check(restartedGrowth.Snapshot().Single(n => n.Id == iron.Id) is { Charges: 4, GrowthStage: "mature" },
    "restarted node reaches the same mature charge count");
Check(growth.Snapshot().Single(n => n.Id == iron.Id).NextTransitionAt is null, "mature stage has no pending transition");
var floorClock = new ManualClock();
try { _ = new GatheringNodeService(zone, [ironDefinition with { Charges = 1 }], floorClock, "bad-floor", new(30, 0, RegrowthMode.FullRespawn)); throw new Exception("rounded early advantage accepted"); }
catch (ArgumentException) { }
var ironSpawn = zone.SpawnGroups.Single(g => g.GroupId == "eastwood_iron_ore").Definitions.Single();
ironSpawn.GrowthProfile = new GrowthProfile { Stages =
    [new() { Name = "young", DurationFraction = .5, YieldFraction = .1 },
     new() { Name = "growing", DurationFraction = .5, YieldFraction = .5 },
     new() { Name = "mature", YieldFraction = 1 }] };
var floorGrowth = new GatheringNodeService(zone, [ironDefinition with { Charges = 2 }], floorClock, "floor-test", new(30, 0, RegrowthMode.FullRespawn));
ironSpawn.GrowthProfile = ironProfile;
var floorNode = floorGrowth.Snapshot().First();
floorGrowth.Harvest(new("floor-initial-1", "p1", floorNode.Id, 1));
var floorDue = floorGrowth.Harvest(new("floor-initial-2", "p1", floorNode.Id, 1)).RegrowAt!.Value;
floorClock.Set(floorDue.AddSeconds(-15));
Check(floorGrowth.Snapshot().Single(n => n.Id == floorNode.Id) is { Charges: 1, GrowthStage: "young" }, "minimum one charge floor");
Console.WriteLine($"GatheringNodes: {passed} passed, 0 failed");

void Check(bool condition, string label)
{
    if (!condition) throw new Exception(label);
    passed++;
}

sealed class ManualClock : TimeProvider
{
    private DateTimeOffset now = new(2026, 1, 1, 0, 0, 0, TimeSpan.Zero);
    public override DateTimeOffset GetUtcNow() => now;
    public void Set(DateTimeOffset value) => now = value;
}
