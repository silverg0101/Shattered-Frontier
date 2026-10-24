using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Text;
using ShatteredFrontier.WorldContent;

namespace ShatteredFrontier.GatheringNodes;

public enum ResourceKind { Wood, Ore }
public enum RegrowthMode { FullRespawn, PartialRegrowth }
public enum HarvestStatus { Granted, Depleted, StaleGeneration, UnknownNode }

public sealed record NodeDefinition(string Id, ResourceKind Kind, string ResourceId, int Charges, int YieldPerHarvest);
public sealed record NodeSettings(int RespawnSeconds, double JitterFraction, RegrowthMode Regrowth, int PointCooldownCycles = 2)
{
    public static NodeSettings Shipped { get; } = new(300, .10, RegrowthMode.FullRespawn);
    public void Validate()
    {
        if (RespawnSeconds is < 30 or > 3600) throw new ArgumentOutOfRangeException(nameof(RespawnSeconds));
        if (!double.IsFinite(JitterFraction) || JitterFraction is < 0 or > .25) throw new ArgumentOutOfRangeException(nameof(JitterFraction));
        if (!Enum.IsDefined(Regrowth)) throw new ArgumentOutOfRangeException(nameof(Regrowth));
        if (PointCooldownCycles is < 0 or > 8) throw new ArgumentOutOfRangeException(nameof(PointCooldownCycles));
    }
}
public sealed record NodeSnapshot(string Id, string GroupId, string PointId, string DefinitionId,
    ResourceKind Kind, int Generation, int Charges, int MaxCharges, DateTimeOffset? RegrowAt,
    string? GrowthStage = null, DateTimeOffset? StageStartedAt = null, DateTimeOffset? NextTransitionAt = null);
public sealed record HarvestCommand(string OperationId, string PlayerId, string NodeId, int Generation);
public sealed record HarvestResult(HarvestStatus Status, string NodeId, int Generation, string? ResourceId,
    int Quantity, int RemainingCharges, DateTimeOffset? RegrowAt);
public sealed record DurableNodeState(string Id, string DefinitionId, int MaxCharges, int Generation, int Charges, int Harvested,
    DateTimeOffset? RegrowAt, DateTimeOffset? GrowthStartedAt, DateTimeOffset? StageStartedAt,
    int StageIndex, double ScheduledRespawnSeconds, string? PointId = null, NodePointVisit[]? RecentPoints = null);
public sealed record NodePointVisit(string PointId, int Generation);

public sealed class GatheringNodeService
{
    private sealed class Node(string id, string groupId, string pointId, NodeDefinition definition, GrowthProfile? growthProfile)
    {
        public readonly string Id = id, GroupId = groupId;
        public string PointId = pointId;
        public readonly List<NodePointVisit> RecentPoints = [new(pointId, 1)];
        public readonly NodeDefinition Definition = definition;
        public readonly GrowthProfile? GrowthProfile = growthProfile;
        public int Generation = 1;
        public int Charges = definition.Charges;
        public int Harvested;
        public DateTimeOffset? RegrowAt;
        public DateTimeOffset? GrowthStartedAt;
        public DateTimeOffset? StageStartedAt;
        public int StageIndex;
        public double ScheduledRespawnSeconds;
    }

    private readonly object gate = new();
    private readonly TimeProvider clock;
    private readonly string seed;
    private readonly NodeSettings settings;
    private readonly Dictionary<string, Node> nodes = new(StringComparer.Ordinal);
    private readonly Dictionary<string, string[]> poolPoints = new(StringComparer.Ordinal);
    private readonly Dictionary<string, string> legacyPointIds = new(StringComparer.Ordinal);
    private readonly Dictionary<string, (HarvestCommand Command, HarvestResult Result)> receipts = new(StringComparer.Ordinal);

    public GatheringNodeService(ZoneContent zone, IEnumerable<NodeDefinition> definitions, TimeProvider clock,
        string seed, NodeSettings? settings = null)
    {
        ArgumentNullException.ThrowIfNull(zone);
        ArgumentNullException.ThrowIfNull(definitions);
        this.clock = clock ?? throw new ArgumentNullException(nameof(clock));
        this.seed = !string.IsNullOrWhiteSpace(seed) ? seed : throw new ArgumentException("Seed required", nameof(seed));
        this.settings = settings ?? NodeSettings.Shipped;
        this.settings.Validate();
        var catalogue = definitions.ToDictionary(d => d.Id, StringComparer.Ordinal);
        foreach (var d in catalogue.Values)
            if (string.IsNullOrWhiteSpace(d.Id) || string.IsNullOrWhiteSpace(d.ResourceId) ||
                d.Charges is < 1 or > 20 || d.YieldPerHarvest is < 1 or > 100 || !Enum.IsDefined(d.Kind))
                throw new ArgumentOutOfRangeException(nameof(definitions));
        foreach (var group in zone.SpawnGroups.Where(g => g.SpawnKind == "gathering_node"))
        {
            if (group.ActiveCount < 0 || group.ActiveCount > group.Points.Count)
                throw new ArgumentException("Invalid gathering active count", nameof(zone));
            var supported = group.Definitions.Where(d => catalogue.ContainsKey(d.DefinitionId) && d.Weight > 0).ToArray();
            if (supported.Length == 0) continue;
            poolPoints.Add(group.GroupId, group.Points.Select(p => p.PointId).ToArray());
            var initialFree = group.Points.ToList();
            for (int i = 0; i < group.ActiveCount; i++)
            {
                var pointIndex = Math.Min((int)(Unit($"point|{zone.Id}|{group.GroupId}|{i}|initial") * initialFree.Count), initialFree.Count - 1);
                var point = initialFree[pointIndex];
                initialFree.RemoveAt(pointIndex);
                var total = supported.Sum(d => d.Weight);
                var pick = Unit($"definition|{zone.Id}|{group.GroupId}|{i}") * total;
                var selected = supported.First(d => (pick -= d.Weight) < 0);
                var id = $"{zone.Id}/{group.GroupId}/{i}";
                legacyPointIds.Add(id, group.Points[i].PointId);
                if (selected.GrowthProfile is { } profile && !WorldContentValidator.ValidGrowthProfile(profile))
                    throw new ArgumentException("Invalid gathering growth profile", nameof(zone));
                if (selected.GrowthProfile is not null && this.settings.Regrowth != RegrowthMode.FullRespawn)
                    throw new ArgumentException("Growth profiles require full respawn", nameof(settings));
                var definition = catalogue[selected.DefinitionId];
                if (selected.GrowthProfile is { } growth)
                {
                    var start = 0.0;
                    foreach (var stage in growth.Stages)
                    {
                        var cap = Math.Max(1, (int)Math.Round(definition.Charges * stage.YieldFraction,
                            MidpointRounding.AwayFromZero));
                        if ((double)cap / definition.Charges > 1 - growth.GrowFraction + start * growth.GrowFraction + 1e-9)
                            throw new ArgumentException("Growth profile gives an early harvest advantage for these charges", nameof(zone));
                        start += stage.DurationFraction;
                    }
                }
                var captured = selected.GrowthProfile is { } source
                    ? new GrowthProfile { GrowFraction = source.GrowFraction, Stages = source.Stages.Select(s => new GrowthStage
                        { Name = s.Name, DurationFraction = s.DurationFraction, YieldFraction = s.YieldFraction }).ToList() }
                    : null;
                nodes.Add(id, new Node(id, group.GroupId, point.PointId, definition, captured));
            }
        }
    }

    public IReadOnlyList<NodeSnapshot> Snapshot()
    {
        lock (gate) { Advance(); return nodes.Values.OrderBy(n => n.Id, StringComparer.Ordinal).Select(View).ToArray(); }
    }

    // The store holds a database lock while importing, advancing, and exporting this complete state.
    public IReadOnlyList<DurableNodeState> ExportState()
    {
        lock (gate) return nodes.Values.OrderBy(n => n.Id, StringComparer.Ordinal)
            .Select(n => new DurableNodeState(n.Id, n.Definition.Id, n.Definition.Charges, n.Generation, n.Charges, n.Harvested, n.RegrowAt,
                n.GrowthStartedAt, n.StageStartedAt, n.StageIndex, n.ScheduledRespawnSeconds, n.PointId, n.RecentPoints.ToArray())).ToArray();
    }

    public void ImportState(IReadOnlyList<DurableNodeState> saved)
    {
        lock (gate)
        {
            if (saved.Count != nodes.Count || saved.Select(n => n.Id).Distinct(StringComparer.Ordinal).Count() != nodes.Count)
                throw new InvalidOperationException("Gathering node topology differs from saved state.");
            foreach (var state in saved)
            {
                if (!nodes.TryGetValue(state.Id, out var node) || state.DefinitionId != node.Definition.Id ||
                    state.MaxCharges != node.Definition.Charges || state.Generation < 1 ||
                    state.Charges < 0 || state.Charges > node.Definition.Charges || state.Harvested < 0 ||
                    state.StageIndex < 0 || state.StageIndex >= (node.GrowthProfile?.Stages.Count ?? 1) ||
                    !double.IsFinite(state.ScheduledRespawnSeconds) || state.ScheduledRespawnSeconds < 0 ||
                    !poolPoints[node.GroupId].Contains(state.PointId ?? legacyPointIds[node.Id], StringComparer.Ordinal) ||
                    (state.RecentPoints is not null && state.RecentPoints.Any(p =>
                        !poolPoints[node.GroupId].Contains(p.PointId, StringComparer.Ordinal) || p.Generation < 1 || p.Generation > state.Generation)))
                    throw new InvalidOperationException("Invalid saved gathering node state.");
                node.Generation = state.Generation; node.Charges = state.Charges; node.Harvested = state.Harvested;
                node.RegrowAt = state.RegrowAt; node.GrowthStartedAt = state.GrowthStartedAt;
                node.StageStartedAt = state.StageStartedAt; node.StageIndex = state.StageIndex;
                node.ScheduledRespawnSeconds = state.ScheduledRespawnSeconds;
                node.PointId = state.PointId ?? legacyPointIds[node.Id];
                node.RecentPoints.Clear();
                node.RecentPoints.AddRange(state.RecentPoints ?? [new(node.PointId, node.Generation)]);
            }
            foreach (var group in nodes.Values.GroupBy(n => n.GroupId))
                if (group.Where(n => n.Charges > 0).Select(n => n.PointId).Distinct(StringComparer.Ordinal).Count() != group.Count(n => n.Charges > 0))
                    throw new InvalidOperationException("Saved gathering nodes overlap at an active pool point.");
        }
    }

    public HarvestResult Harvest(HarvestCommand command)
    {
        ArgumentNullException.ThrowIfNull(command);
        if (string.IsNullOrWhiteSpace(command.OperationId) || string.IsNullOrWhiteSpace(command.PlayerId) || string.IsNullOrWhiteSpace(command.NodeId))
            throw new ArgumentException("Operation, player and node IDs are required", nameof(command));
        lock (gate)
        {
            if (receipts.TryGetValue(command.OperationId, out var prior))
            {
                if (prior.Command != command) throw new InvalidOperationException("Operation ID reused with different command");
                return prior.Result;
            }
            Advance();
            HarvestResult result;
            if (!nodes.TryGetValue(command.NodeId, out var node))
                result = new(HarvestStatus.UnknownNode, command.NodeId, 0, null, 0, 0, null);
            else if (command.Generation != node.Generation)
                result = new(HarvestStatus.StaleGeneration, node.Id, node.Generation, null, 0, node.Charges, node.RegrowAt);
            else if (node.Charges == 0)
                result = new(HarvestStatus.Depleted, node.Id, node.Generation, null, 0, 0, node.RegrowAt);
            else
            {
                node.Charges--;
                node.Harvested++;
                if (node.Charges == 0) Schedule(node);
                result = new(HarvestStatus.Granted, node.Id, node.Generation, node.Definition.ResourceId,
                    node.Definition.YieldPerHarvest, node.Charges, node.RegrowAt);
            }
            receipts.Add(command.OperationId, (command, result));
            return result;
        }
    }

    private NodeSnapshot View(Node n)
    {
        DateTimeOffset? next = n.RegrowAt;
        if (n.RegrowAt is { } due && n.GrowthProfile is { } growth)
            next = due - TimeSpan.FromSeconds(n.ScheduledRespawnSeconds * growth.GrowFraction);
        else if (n.GrowthStartedAt is { } started && n.GrowthProfile is { } profile && n.StageIndex < profile.Stages.Count - 1)
            next = started.AddSeconds(n.ScheduledRespawnSeconds * profile.GrowFraction *
                profile.Stages.Take(n.StageIndex + 1).Sum(s => s.DurationFraction));
        return new(n.Id, n.GroupId, n.PointId, n.Definition.Id, n.Definition.Kind, n.Generation, n.Charges,
            n.Definition.Charges, n.RegrowAt,
            n.GrowthProfile is null || n.Charges == 0 ? null : n.GrowthStartedAt is null
                ? n.GrowthProfile.Stages[^1].Name : n.GrowthProfile.Stages[n.StageIndex].Name,
            n.StageStartedAt, next);
    }

    private void Schedule(Node node)
    {
        node.GrowthStartedAt = null;
        node.StageStartedAt = null;
        var u = Unit($"jitter|{node.Id}|{node.Generation}");
        node.ScheduledRespawnSeconds = settings.RespawnSeconds * (1 + settings.JitterFraction * (2 * u - 1));
        node.RegrowAt = clock.GetUtcNow().AddSeconds(node.ScheduledRespawnSeconds);
    }

    private void Rotate(Node node)
    {
        var occupied = nodes.Values.Where(n => n.GroupId == node.GroupId && n != node && n.Charges > 0)
            .Select(n => n.PointId).ToHashSet(StringComparer.Ordinal);
        var available = poolPoints[node.GroupId].Where(p => !occupied.Contains(p)).ToArray();
        if (available.Length == 0) throw new InvalidOperationException("Gathering pool has no free point.");
        var recent = node.RecentPoints.Where(p => node.Generation - p.Generation <= settings.PointCooldownCycles)
            .Select(p => p.PointId).ToHashSet(StringComparer.Ordinal);
        var preferred = available.Where(p => !recent.Contains(p)).ToArray();
        if (preferred.Length > 0) available = preferred;
        var pick = Unit($"point|{node.Id}|{node.Generation}");
        node.PointId = available[Math.Min((int)(pick * available.Length), available.Length - 1)];
        node.RecentPoints.Add(new(node.PointId, node.Generation));
        node.RecentPoints.RemoveAll(p => node.Generation - p.Generation > settings.PointCooldownCycles);
    }

    private void Advance()
    {
        var now = clock.GetUtcNow();
        foreach (var node in nodes.Values)
        {
            if (node.GrowthProfile is { } profile && settings.Regrowth == RegrowthMode.FullRespawn)
            {
                if (node.RegrowAt is { } matureAt)
                {
                    // The growth window is a fraction of this scheduled respawn interval.
                    var emergeAt = matureAt - TimeSpan.FromSeconds(node.ScheduledRespawnSeconds * profile.GrowFraction);
                    if (now >= emergeAt)
                    {
                        node.Generation++;
                        Rotate(node);
                        node.Harvested = 0;
                        node.GrowthStartedAt = emergeAt;
                        node.RegrowAt = null;
                    }
                }
                if (node.GrowthStartedAt is { } started)
                {
                    var elapsed = (now - started).TotalSeconds;
                    var start = 0.0;
                    for (var i = 0; i < profile.Stages.Count; i++)
                    {
                        if (i > 0 && elapsed < start * node.ScheduledRespawnSeconds * profile.GrowFraction) break;
                        node.StageIndex = i;
                        node.StageStartedAt = started.AddSeconds(start * node.ScheduledRespawnSeconds * profile.GrowFraction);
                        start += profile.Stages[i].DurationFraction;
                    }
                    var cap = Math.Max(1, (int)Math.Round(node.Definition.Charges * profile.Stages[node.StageIndex].YieldFraction,
                        MidpointRounding.AwayFromZero));
                    node.Charges = Math.Max(0, cap - node.Harvested);
                }
                continue;
            }
            while (node.RegrowAt is { } due && due <= now)
            {
                var wasDepleted = node.Charges == 0;
                node.Generation++;
                if (wasDepleted) Rotate(node);
                node.Charges = settings.Regrowth == RegrowthMode.FullRespawn ? node.Definition.Charges : node.Charges + 1;
                node.RegrowAt = null;
                node.Harvested = 0;
                if (settings.Regrowth == RegrowthMode.PartialRegrowth && node.Charges < node.Definition.Charges)
                {
                    var u = Unit($"jitter|{node.Id}|{node.Generation}");
                    node.RegrowAt = due.AddSeconds(settings.RespawnSeconds * (1 + settings.JitterFraction * (2 * u - 1)));
                }
            }
        }
    }

    private double Unit(string key)
    {
        var bytes = SHA256.HashData(Encoding.UTF8.GetBytes($"{seed}|{key}"));
        return BinaryPrimitives.ReadUInt64BigEndian(bytes.AsSpan(0, 8)) / 18446744073709551616.0;
    }
}
