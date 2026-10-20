namespace ShatteredFrontier.WorldContent;

public enum SlotState { Active, Respawning, Dormant, Disabled }
public enum DayPhase { Day, Night }

public sealed record SlotSnapshot(int SlotIndex, int Cycle, SlotState State, string? PointId,
    string? DefinitionId, DateTimeOffset? RespawnAt, IReadOnlyList<(string PointId, int Cycle)> RecentPoints);

public interface ISpawnSlotStore
{
    IReadOnlyList<SlotSnapshot>? Load(string zoneId, string groupId);
    void Save(string zoneId, string groupId, IReadOnlyList<SlotSnapshot> slots);
}

public interface ISpawnRandom
{
    double NextUnit(string zoneId, string groupId, int slotIndex, int cycle, string purpose);
}

public sealed class SeededSpawnRandom(int seed) : ISpawnRandom
{
    public double NextUnit(string zoneId, string groupId, int slotIndex, int cycle, string purpose)
    {
        var bytes = System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(
            $"{seed}|{zoneId}|{groupId}|{slotIndex}|{cycle}|{purpose}"));
        return System.Buffers.Binary.BinaryPrimitives.ReadUInt64BigEndian(bytes.AsSpan(0, 8)) / 18446744073709551616.0;
    }
}

public sealed class SpawnSlotService
{
    private sealed class Slot(int index, DateTimeOffset now)
    {
        public int Index = index;
        public int Cycle;
        public SlotState State = SlotState.Respawning;
        public string? PointId;
        public string? DefinitionId;
        public DateTimeOffset? RespawnAt = now;
        public List<(string PointId, int Cycle)> Recent = [];
    }

    private readonly ZoneContent zone;
    private readonly WorldTuning tuning;
    private readonly TimeProvider clock;
    private readonly ISpawnRandom random;
    private readonly ISpawnSlotStore? store;
    private readonly Dictionary<string, List<Slot>> slots;
    private readonly HashSet<string> disabledGroups = new(StringComparer.Ordinal);

    public SpawnSlotService(ZoneContent zone, WorldTuning tuning, TimeProvider clock, ISpawnRandom random, ISpawnSlotStore? store = null)
    {
        var issues = WorldContentValidator.Validate(zone);
        if (issues.Count > 0) throw new ArgumentException(string.Join("; ", issues), nameof(zone));
        this.zone = zone; this.tuning = tuning; this.clock = clock; this.random = random; this.store = store;
        slots = zone.SpawnGroups.ToDictionary(g => g.GroupId, g =>
        {
            var restored = store?.Load(zone.Id, g.GroupId);
            return restored is null ? Enumerable.Range(0, g.ActiveCount).Select(i => new Slot(i, clock.GetUtcNow())).ToList() :
                restored.Select(s => new Slot(s.SlotIndex, clock.GetUtcNow())
                {
                    Cycle = s.Cycle, State = s.State, PointId = s.PointId, DefinitionId = s.DefinitionId,
                    RespawnAt = s.RespawnAt, Recent = s.RecentPoints.ToList()
                }).ToList();
        }, StringComparer.Ordinal);
    }

    public IReadOnlyList<SlotSnapshot> Snapshot(string groupId) => slots[groupId]
        .Select(s => new SlotSnapshot(s.Index, s.Cycle, s.State, s.PointId, s.DefinitionId, s.RespawnAt, s.Recent.ToArray())).ToArray();

    public void SaveEligible(Func<SpawnGroup, bool> persistNpc) // Caller decides elite/rare from definition catalogue.
    {
        if (store is null) return;
        foreach (var group in zone.SpawnGroups.Where(g => g.SpawnKind == "gathering_node" || persistNpc(g)))
            store.Save(zone.Id, group.GroupId, Snapshot(group.GroupId));
    }

    public void SetHostileSpawning(bool enabled)
    {
        foreach (var group in zone.SpawnGroups.Where(g => g.SpawnKind == "npc"))
        {
            if (enabled) disabledGroups.Remove(group.GroupId); else disabledGroups.Add(group.GroupId);
            foreach (var slot in slots[group.GroupId])
            {
                slot.PointId = null; slot.DefinitionId = null;
                slot.State = enabled ? SlotState.Respawning : SlotState.Disabled;
                slot.RespawnAt = enabled ? clock.GetUtcNow() : null;
            }
        }
    }

    public void Tick(DayPhase phase, double scarcityIndex = 1, int playersInArea = 0, double directorPressure = 0,
        Func<SpawnGroup, bool>? cellAwake = null)
    {
        if (!double.IsFinite(scarcityIndex) || scarcityIndex < 0 || scarcityIndex > 2 || playersInArea < 0 || !double.IsFinite(directorPressure))
            throw new ArgumentOutOfRangeException(nameof(scarcityIndex));
        var now = clock.GetUtcNow();
        foreach (var group in zone.SpawnGroups)
        {
            var groupSlots = slots[group.GroupId];
            var target = group.SpawnKind == "npc"
                ? Math.Clamp((int)Math.Round(group.ActiveCount * (1 + Math.Clamp(directorPressure, -.5, 1)), MidpointRounding.AwayFromZero), 0, 2 * group.ActiveCount)
                : group.ActiveCount;
            while (groupSlots.Count < target) groupSlots.Add(new Slot(groupSlots.Count, now));
            var phaseMatches = group.PhaseFilter == "any" || group.PhaseFilter == phase.ToString().ToLowerInvariant();
            foreach (var slot in groupSlots)
            {
                if (disabledGroups.Contains(group.GroupId)) { slot.State = SlotState.Disabled; slot.PointId = null; slot.DefinitionId = null; continue; }
                if (!phaseMatches)
                {
                    if (slot.State != SlotState.Dormant) { slot.State = SlotState.Dormant; slot.PointId = null; slot.DefinitionId = null; slot.RespawnAt = null; }
                    continue;
                }
                if (slot.State == SlotState.Dormant || slot.State == SlotState.Disabled) { slot.State = SlotState.Respawning; slot.RespawnAt = now; }
                // Pressure reductions retire only inactive extra slots; active NPCs stay until their next defeat.
                if (slot.Index >= target || slot.State != SlotState.Respawning || slot.RespawnAt > now || cellAwake?.Invoke(group) == false) continue;
                var occupied = groupSlots.Where(s => s.State == SlotState.Active).Select(s => s.PointId).ToHashSet(StringComparer.Ordinal);
                var available = group.Points.Where(p => !occupied.Contains(p.PointId) &&
                    !(group.SpawnKind == "npc" && WorldContentValidator.IsProtected(zone, p.Position))).ToArray();
                if (available.Length == 0) continue;
                var recent = groupSlots.SelectMany(s => s.Recent).Where(p => slot.Cycle - p.Cycle <= tuning.PointCooldownCycles && slot.Cycle - p.Cycle >= 0)
                    .Select(p => p.PointId).ToHashSet(StringComparer.Ordinal);
                var preferred = available.Where(p => !recent.Contains(p.PointId)).ToArray();
                if (preferred.Length > 0) available = preferred;
                var u = random.NextUnit(zone.Id, group.GroupId, slot.Index, slot.Cycle, "point");
                var point = available[Math.Min((int)(u * available.Length), available.Length - 1)];
                var total = group.Definitions.Sum(d => d.Weight);
                var pick = random.NextUnit(zone.Id, group.GroupId, slot.Index, slot.Cycle, "definition") * total;
                var definition = group.Definitions.First(d => (pick -= d.Weight) < 0);
                slot.PointId = point.PointId; slot.DefinitionId = definition.DefinitionId;
                slot.State = SlotState.Active; slot.RespawnAt = null;
                slot.Recent.Add((point.PointId, slot.Cycle));
                slot.Recent.RemoveAll(p => slot.Cycle - p.Cycle > tuning.PointCooldownCycles);
            }
        }
    }

    public void Defeat(string groupId, int slotIndex, double baseRespawnSeconds, double scarcityIndex = 1, int playersInArea = 0)
    {
        var group = zone.SpawnGroups.Single(g => g.GroupId == groupId);
        var slot = slots[groupId].Single(s => s.Index == slotIndex);
        if (slot.State != SlotState.Active) throw new InvalidOperationException("Only an active slot can be depleted or defeated.");
        if (!double.IsFinite(baseRespawnSeconds) || baseRespawnSeconds <= 0 || scarcityIndex is < 0 or > 2 || playersInArea < 0)
            throw new ArgumentOutOfRangeException(nameof(baseRespawnSeconds));
        var factor = group.SpawnKind == "gathering_node"
            ? 1 + tuning.Get("tune.world.scarcity_respawn_k") * Math.Max(0, scarcityIndex - 1)
            : Math.Max(tuning.Get("tune.world.dynamic_respawn_floor"),
                1 / (1 + tuning.Get("tune.world.dynamic_respawn_k") * Math.Max(0, playersInArea - 1)));
        var u = random.NextUnit(zone.Id, groupId, slotIndex, slot.Cycle + 1, "jitter");
        var jitter = 1 + tuning.Get("tune.world.respawn_jitter") * (2 * u - 1);
        slot.Cycle++; slot.State = SlotState.Respawning; slot.PointId = null; slot.DefinitionId = null;
        slot.RespawnAt = clock.GetUtcNow().AddSeconds((group.RespawnSOverride ?? baseRespawnSeconds) * factor * jitter);
    }

    public void DefeatNpc(string groupId, int slotIndex, string tier, int playersInArea = 0) =>
        Defeat(groupId, slotIndex, tuning.Get("tune.world.npc_respawn_s." + tier), 1, playersInArea);
}
