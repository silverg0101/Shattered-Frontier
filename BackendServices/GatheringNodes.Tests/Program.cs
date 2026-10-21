using ShatteredFrontier.GatheringNodes;
using ShatteredFrontier.WorldContent;

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
Console.WriteLine("GatheringNodes: all focused checks passed");

static void Check(bool condition, string label)
{
    if (!condition) throw new Exception(label);
}

sealed class ManualClock : TimeProvider
{
    private DateTimeOffset now = new(2026, 1, 1, 0, 0, 0, TimeSpan.Zero);
    public override DateTimeOffset GetUtcNow() => now;
    public void Set(DateTimeOffset value) => now = value;
}
