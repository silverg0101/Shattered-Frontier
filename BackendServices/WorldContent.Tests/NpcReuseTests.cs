using System.Text.Json;
using ShatteredFrontier.WorldContent;

internal static class NpcReuseTests
{
    public static int Run(string path)
    {
        void Check(bool pass, string name) { if (!pass) throw new Exception(name); Console.WriteLine("PASS " + name); }
        var original = ZoneLoader.Load(path);
        var shipped = NpcSpawnCatalog.Load("content/creatures/profiles.json");
        var fixtures = NpcSpawnCatalog.Load("tests/fixtures/npc-profiles/profiles.json");
        Check(shipped.Groups(original).Count == 6, "all six shipped NPC groups admitted");
        Check(shipped.Tier("npc/wolf_alpha") == "elite" && shipped.Tier("npc/bandit_captain") == "mini_boss",
            "existing boss respawn tiers preserved");
        var copy = JsonSerializer.Deserialize<ZoneContent>(JsonSerializer.Serialize(original, ZoneLoader.JsonOptions), ZoneLoader.JsonOptions)!;
        var rangerGroup = copy.SpawnGroups.Single(g => g.GroupId == "bandit_forest_trail_camp");
        copy.SpawnGroups.RemoveAll(g => g.SpawnKind == "npc" && g != rangerGroup);
        rangerGroup.GroupId = "rangers_across_the_river";
        rangerGroup.Definitions = [new SpawnDefinition { DefinitionId = "npc/reuse_archer_fixture", Weight = 1 }];
        Check(WorldContentValidator.Validate(copy).Count == 0, "arbitrary NPC group and seventh definition are valid zone content");
        Check(fixtures.Groups(copy).ContainsKey(rangerGroup.GroupId), "declared arbitrary group admitted without C++/C# name branches");
        var clock = new ManualClock();
        var service = new SpawnSlotService(copy, new WorldTuning(), clock, new SeededSpawnRandom(2077));
        service.Tick(DayPhase.Day);
        Check(service.Snapshot(rangerGroup.GroupId).All(s => s.State == SlotState.Active && s.DefinitionId == "npc/reuse_archer_fixture"),
            "seventh content-only definition populates live slots");
        service.DefeatNpc(rangerGroup.GroupId, 0, fixtures.Tier("npc/reuse_archer_fixture"));
        Check(service.Snapshot(rangerGroup.GroupId)[0].State == SlotState.Respawning, "seventh definition follows authored common respawn tier");
        bool rejected = false;
        try { _ = shipped.Groups(copy); } catch (ArgumentException) { rejected = true; }
        Check(rejected, "unknown zone definition rejected rather than assigned common defaults");
        Check(original.SpawnGroups.Where(g => g.SpawnKind == "npc").All(g => g.Points.Select((p, i) =>
            p.NpcDefensive == (g.GroupId.StartsWith("boar_", StringComparison.Ordinal) && i % 4 >= 2)).All(v => v)),
            "authored defensive points preserve every previous spawn choice");
        var temporary = Path.GetTempFileName();
        try
        {
            File.WriteAllText(temporary, File.ReadAllText("tests/fixtures/npc-profiles/profiles.json")
                .Replace("\"respawn_tier\": \"common\"", "\"respawn_tier\": \"unknown\"", StringComparison.Ordinal));
            rejected = false;
            try { _ = NpcSpawnCatalog.Load(temporary); } catch (JsonException) { rejected = true; }
            Check(rejected, "unknown respawn tier rejected at content admission");
        }
        finally { File.Delete(temporary); }
        return 0;
    }

    private sealed class ManualClock : TimeProvider
    {
        public override DateTimeOffset GetUtcNow() => DateTimeOffset.UnixEpoch;
    }
}
