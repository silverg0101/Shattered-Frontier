using System.Text.Json;
using ShatteredFrontier.WorldContent;

var combatOnly = args.Contains("--combat");
var path = args.First(a => a != "--combat");
var zone = ZoneLoader.Load(path);
void Check(bool pass, string name) { if (!pass) throw new Exception(name); Console.WriteLine("PASS " + name); }
void Broken(string name, Action<ZoneContent> mutate, string expected)
{
    var copy = JsonSerializer.Deserialize<ZoneContent>(JsonSerializer.Serialize(zone, ZoneLoader.JsonOptions), ZoneLoader.JsonOptions)!;
    mutate(copy);
    Check(WorldContentValidator.Validate(copy).Any(i => i.Contains(expected, StringComparison.Ordinal)), name);
}
if (combatOnly)
{
    var combatClock = new ManualClock();
    var slots = new SpawnSlotService(zone, new WorldTuning(), combatClock, new SeededSpawnRandom(2077));
    slots.Tick(DayPhase.Day);
    var expected = new Dictionary<string, string[]>
    {
        ["boar_farm_edge"] = ["npc/boar"], ["boar_forest_edge"] = ["npc/boar"],
        ["wolf_northwest_den"] = ["npc/wolf"],
        ["bandit_forest_trail_camp"] = ["npc/bandit_thug", "npc/bandit_archer"],
        ["wolf_alpha_northwest_den"] = ["npc/wolf_alpha"],
        ["bandit_captain_forest_trail_camp"] = ["npc/bandit_captain"]
    };
    foreach (var (id, definitions) in expected)
    {
        var group = zone.SpawnGroups.Single(g => g.GroupId == id);
        var active = slots.Snapshot(id).Where(s => s.State == SlotState.Active).ToArray();
        Check(active.Length == group.ActiveCount && active.All(s => definitions.Contains(s.DefinitionId)), $"{id} active definitions");
        foreach (var definition in definitions)
        {
            var ruleFile = Directory.GetFiles("content/creatures/loot_rules", "*.json")
                .Single(file => JsonDocument.Parse(File.ReadAllText(file)).RootElement.GetProperty("creature_id").GetString() == definition);
            using var rule = JsonDocument.Parse(File.ReadAllText(ruleFile));
            var tableId = rule.RootElement.GetProperty("table_id").GetString()!;
            var tableFile = Directory.GetFiles("content/loot", "*.json")
                .Single(file => JsonDocument.Parse(File.ReadAllText(file)).RootElement.GetProperty("id").GetString() == tableId);
            using var table = JsonDocument.Parse(File.ReadAllText(tableFile));
            var boss = definition is "npc/wolf_alpha" or "npc/bandit_captain";
            Check(rule.RootElement.GetProperty("unique_or_boss").GetBoolean() == boss &&
                (table.RootElement.TryGetProperty("single_drop", out var marker) && marker.GetBoolean()) == boss,
                $"{definition} bound loot route");
        }
    }
    foreach (var (id, definition, minDelay, maxDelay) in new[] {
        ("wolf_alpha_northwest_den", "npc/wolf_alpha", 600, 1200),
        ("bandit_captain_forest_trail_camp", "npc/bandit_captain", 1200, 2400) })
    {
        Check(slots.Snapshot(id).Count == 1, $"{definition} single active boss");
        slots.DefeatNpc(id, 0, SilverbrookCombatGroups.Tier(definition));
        var defeated = slots.Snapshot(id)[0];
        var delay = (defeated.RespawnAt!.Value - combatClock.GetUtcNow()).TotalSeconds;
        Check(defeated.State == SlotState.Respawning && delay >= minDelay && delay <= maxDelay,
            $"{definition} tier respawn after defeat");
        combatClock.Advance(TimeSpan.FromSeconds(delay + 1));
        slots.Tick(DayPhase.Day);
        Check(slots.Snapshot(id)[0] is { State: SlotState.Active, Cycle: 1 } &&
            slots.Snapshot(id)[0].DefinitionId == definition, $"{definition} reappears at its sole point");
    }
    return;
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
