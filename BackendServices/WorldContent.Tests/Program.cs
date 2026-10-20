using System.Text.Json;
using ShatteredFrontier.WorldContent;

var path = args.Single();
var zone = ZoneLoader.Load(path);
void Check(bool pass, string name) { if (!pass) throw new Exception(name); Console.WriteLine("PASS " + name); }
void Broken(string name, Action<ZoneContent> mutate, string expected)
{
    var copy = JsonSerializer.Deserialize<ZoneContent>(JsonSerializer.Serialize(zone, ZoneLoader.JsonOptions), ZoneLoader.JsonOptions)!;
    mutate(copy);
    Check(WorldContentValidator.Validate(copy).Any(i => i.Contains(expected, StringComparison.Ordinal)), name);
}
Check(WorldContentValidator.Validate(zone).Count == 0, "generated zone validates");
Broken("N >= 2K", z => z.SpawnGroups.First(g => g.SpawnKind == "gathering_node").ActiveCount = 999, "N >= 2K");
Broken("protected hostile point", z => z.SpawnGroups.First(g => g.SpawnKind == "npc").Points[0].Position = [0, 0], "protected area");
Broken("level range", z => z.LevelAreas[0].ContentLevel = 999, "outside level_range");
Check(WorldContentValidator.ValidateAll([(path, zone), ("duplicate", zone)]).Any(i => i.Contains("zone_index", StringComparison.Ordinal)), "zone_index uniqueness");

var clock = new ManualClock();
var service = new SpawnSlotService(zone, new WorldTuning(), clock, new SeededSpawnRandom(42));
var gather = zone.SpawnGroups.First(g => g.SpawnKind == "gathering_node");
var npc = zone.SpawnGroups.First(g => g.SpawnKind == "npc");
service.Tick(DayPhase.Day);
var initial = service.Snapshot(gather.GroupId);
Check(initial.Count(s => s.State == SlotState.Active) == gather.ActiveCount, "initial gathering slots active");
var firstPoint = initial[0].PointId;
for (var cycle = 0; cycle < 2; cycle++)
{
    service.Defeat(gather.GroupId, 0, 100);
    var due = service.Snapshot(gather.GroupId)[0].RespawnAt!.Value;
    var seconds = (due - clock.GetUtcNow()).TotalSeconds;
    Check(seconds is >= 90 and <= 110, "jitter bounds");
    clock.Advance(TimeSpan.FromSeconds(111));
    service.Tick(DayPhase.Day);
    Check(service.Snapshot(gather.GroupId)[0].PointId != firstPoint, "no point reused within two cycles");
}
service.SetHostileSpawning(false);
service.Tick(DayPhase.Day);
Check(service.Snapshot(npc.GroupId).All(s => s.State == SlotState.Disabled && s.PointId is null), "hostile disabled");
service.SetHostileSpawning(true);
service.Tick(DayPhase.Day);
Check(service.Snapshot(npc.GroupId).Any(s => s.State == SlotState.Active), "hostile resumes");
service.DefeatNpc(npc.GroupId, 0, "common", 6);
var npcDelay = (service.Snapshot(npc.GroupId)[0].RespawnAt!.Value - clock.GetUtcNow()).TotalSeconds;
Check(npcDelay is >= 150 and <= 190, "NPC dynamic respawn with bounded jitter");
service.Tick(DayPhase.Day, directorPressure: 1);
Check(service.Snapshot(npc.GroupId).Count == 2 * npc.ActiveCount, "director pressure slot ceiling");
var phased = JsonSerializer.Deserialize<ZoneContent>(JsonSerializer.Serialize(zone, ZoneLoader.JsonOptions), ZoneLoader.JsonOptions)!;
phased.SpawnGroups.First().PhaseFilter = "night";
var phasedClock = new ManualClock();
var phasedService = new SpawnSlotService(phased, new WorldTuning(), phasedClock, new SeededSpawnRandom(42));
phasedService.Tick(DayPhase.Day);
Check(phasedService.Snapshot(gather.GroupId).All(s => s.State == SlotState.Dormant), "phase filter dormancy");
phasedService.Tick(DayPhase.Night);
Check(phasedService.Snapshot(gather.GroupId).Any(s => s.State == SlotState.Active), "phase wakeup");
Check(WorldTuningCatalog.Keys.Count == 8, "typed tuning catalog");

sealed class ManualClock : TimeProvider
{
    private DateTimeOffset now = new(2026, 10, 20, 0, 0, 0, TimeSpan.Zero);
    public override DateTimeOffset GetUtcNow() => now;
    public void Advance(TimeSpan duration) => now += duration;
}
