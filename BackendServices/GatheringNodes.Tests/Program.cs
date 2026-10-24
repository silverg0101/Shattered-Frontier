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
var service = new GatheringNodeService(zone, definitions, clock, "silverbrook-test", settings);
var nodes = service.Snapshot();
Check(nodes.Any(n => n.Kind == ResourceKind.Wood) && nodes.Any(n => n.Kind == ResourceKind.Ore), "real wood and mined stone groups loaded");
Check(nodes.Count(n => n.Kind == ResourceKind.Wood) == 8 && nodes.Count(n => n.Kind == ResourceKind.Ore) == 6, "active counts from zone record");
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
Check(growth.Harvest(new("iron-young", "p1", iron.Id, 2)).RemainingCharges == 1, "young charge consumed");
growthClock.Set(maturity.AddSeconds(-7.5));
Check(growth.Snapshot().Single(n => n.Id == iron.Id) is { Charges: 2, GrowthStage: "growing" }, "half rounding and prior harvest retained");
growthClock.Set(maturity);
Check(growth.Snapshot().Single(n => n.Id == iron.Id) is { Charges: 4, GrowthStage: "mature" }, "maturity reaches full cap less prior harvest");
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
