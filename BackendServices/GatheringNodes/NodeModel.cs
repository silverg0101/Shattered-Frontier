using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Text;
using ShatteredFrontier.WorldContent;

namespace ShatteredFrontier.GatheringNodes;

public enum ResourceKind { Wood, Ore }
public enum RegrowthMode { FullRespawn, PartialRegrowth }
public enum HarvestStatus { Granted, Depleted, StaleGeneration, UnknownNode }

public sealed record NodeDefinition(string Id, ResourceKind Kind, string ResourceId, int Charges, int YieldPerHarvest);
public sealed record NodeSettings(int RespawnSeconds, double JitterFraction, RegrowthMode Regrowth)
{
    public static NodeSettings Shipped { get; } = new(300, .10, RegrowthMode.FullRespawn);
    public void Validate()
    {
        if (RespawnSeconds is < 30 or > 3600) throw new ArgumentOutOfRangeException(nameof(RespawnSeconds));
        if (!double.IsFinite(JitterFraction) || JitterFraction is < 0 or > .25) throw new ArgumentOutOfRangeException(nameof(JitterFraction));
        if (!Enum.IsDefined(Regrowth)) throw new ArgumentOutOfRangeException(nameof(Regrowth));
    }
}
public sealed record NodeSnapshot(string Id, string GroupId, string PointId, string DefinitionId,
    ResourceKind Kind, int Generation, int Charges, int MaxCharges, DateTimeOffset? RegrowAt,
    string? GrowthStage = null, DateTimeOffset? StageStartedAt = null, DateTimeOffset? NextTransitionAt = null);
public sealed record HarvestCommand(string OperationId, string PlayerId, string NodeId, int Generation);
public sealed record HarvestResult(HarvestStatus Status, string NodeId, int Generation, string? ResourceId,
    int Quantity, int RemainingCharges, DateTimeOffset? RegrowAt);

public sealed class GatheringNodeService
{
    private sealed class Node(string id, string groupId, string pointId, NodeDefinition definition, GrowthProfile? growthProfile)
    {
        public readonly string Id = id, GroupId = groupId, PointId = pointId;
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
            for (int i = 0; i < group.ActiveCount; i++)
            {
                var point = group.Points[i];
                var total = supported.Sum(d => d.Weight);
                var pick = Unit($"definition|{zone.Id}|{group.GroupId}|{i}") * total;
                var selected = supported.First(d => (pick -= d.Weight) < 0);
                var id = $"{zone.Id}/{group.GroupId}/{i}";
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
                node.Generation++;
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
