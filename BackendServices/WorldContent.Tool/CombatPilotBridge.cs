using System.Text.Json;
using ShatteredFrontier.WorldContent;

// Development-only, finite command bridge. Unreal owns actors; WorldContent owns slot transitions.
internal static class CombatPilotBridge
{
    private sealed class FileSlots(string path) : ISpawnSlotStore
    {
        private readonly Dictionary<string, List<SlotSnapshot>> groups = File.Exists(path)
            ? JsonSerializer.Deserialize<Dictionary<string, List<SlotSnapshot>>>(File.ReadAllText(path), JsonOptions) ?? []
            : [];
        public IReadOnlyList<SlotSnapshot>? Load(string zoneId, string groupId) =>
            groups.TryGetValue(groupId, out var value) ? value : null;
        public void Save(string zoneId, string groupId, IReadOnlyList<SlotSnapshot> slots) => groups[groupId] = slots.ToList();
        public void Flush()
        {
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            var temporary = path + ".new";
            File.WriteAllText(temporary, JsonSerializer.Serialize(groups, JsonOptions));
            File.Move(temporary, path, true);
        }
    }

    private static readonly JsonSerializerOptions JsonOptions = new() { IncludeFields = true };

    public static int Run(string[] args)
    {
        // zone, state, tick|defeat, [group, slot, players], hostileSpawning
        if (args.Length < 4 || args[2] is not ("tick" or "defeat") ||
            !bool.TryParse(args[^1], out var hostileSpawning)) return 2;
        try
        {
            var zone = ZoneLoader.Load(args[0]);
            var store = new FileSlots(args[1]);
            var service = new SpawnSlotService(zone, new WorldTuning(), TimeProvider.System,
                new SeededSpawnRandom(2077), store);
            if (!hostileSpawning) service.SetHostileSpawning(false);
            if (args[2] == "defeat")
            {
                if (args.Length != 7 || !int.TryParse(args[4], out var slot) ||
                    !int.TryParse(args[5], out var players)) return 2;
                var group = zone.SpawnGroups.SingleOrDefault(g => g.GroupId == args[3] && SilverbrookCombatGroups.Contains(g.GroupId));
                if (group is null) return 2;
                var current = service.Snapshot(group.GroupId).SingleOrDefault(s => s.SlotIndex == slot);
                if (current is null || current.State != SlotState.Active) return 2;
                service.DefeatNpc(args[3], slot, SilverbrookCombatGroups.Tier(current.DefinitionId), players);
            }
            service.Tick(DayPhase.Day);
            foreach (var group in zone.SpawnGroups.Where(g => SilverbrookCombatGroups.Contains(g.GroupId)))
            {
                var slots = service.Snapshot(group.GroupId);
                store.Save(zone.Id, group.GroupId, slots);
                foreach (var slot in slots)
                {
                    var point = group.Points.FirstOrDefault(p => p.PointId == slot.PointId);
                    Console.WriteLine(FormattableString.Invariant(
                        $"SLOT|{group.GroupId}|{slot.SlotIndex}|{slot.Cycle}|{slot.State}|{point?.PointId ?? "-"}|{point?.Position[0] ?? 0:R}|{point?.Position[1] ?? 0:R}|{point?.Facing ?? 0:R}|{slot.DefinitionId ?? "-"}"));
                }
            }
            store.Flush();
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex.Message);
            return 1;
        }
    }
}
