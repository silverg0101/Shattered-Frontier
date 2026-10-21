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
    ResourceKind Kind, int Generation, int Charges, int MaxCharges, DateTimeOffset? RegrowAt);
public sealed record HarvestCommand(string OperationId, string PlayerId, string NodeId, int Generation);
public sealed record HarvestResult(HarvestStatus Status, string NodeId, int Generation, string? ResourceId,
    int Quantity, int RemainingCharges, DateTimeOffset? RegrowAt);

public sealed class GatheringNodeService
{
    private sealed class Node(string id, string groupId, string pointId, NodeDefinition definition)
    {
        public readonly string Id = id, GroupId = groupId, PointId = pointId;
        public readonly NodeDefinition Definition = definition;
        public int Generation = 1;
        public int Charges = definition.Charges;
        public DateTimeOffset? RegrowAt;
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
                nodes.Add(id, new Node(id, group.GroupId, point.PointId, catalogue[selected.DefinitionId]));
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
                if (node.Charges == 0) Schedule(node);
                result = new(HarvestStatus.Granted, node.Id, node.Generation, node.Definition.ResourceId,
                    node.Definition.YieldPerHarvest, node.Charges, node.RegrowAt);
            }
            receipts.Add(command.OperationId, (command, result));
            return result;
        }
    }

    private NodeSnapshot View(Node n) => new(n.Id, n.GroupId, n.PointId, n.Definition.Id,
        n.Definition.Kind, n.Generation, n.Charges, n.Definition.Charges, n.RegrowAt);

    private void Schedule(Node node)
    {
        var u = Unit($"jitter|{node.Id}|{node.Generation}");
        node.RegrowAt = clock.GetUtcNow().AddSeconds(settings.RespawnSeconds * (1 + settings.JitterFraction * (2 * u - 1)));
    }

    private void Advance()
    {
        var now = clock.GetUtcNow();
        foreach (var node in nodes.Values)
            while (node.RegrowAt is { } due && due <= now)
            {
                node.Generation++;
                node.Charges = settings.Regrowth == RegrowthMode.FullRespawn ? node.Definition.Charges : node.Charges + 1;
                node.RegrowAt = null;
                if (settings.Regrowth == RegrowthMode.PartialRegrowth && node.Charges < node.Definition.Charges)
                {
                    var u = Unit($"jitter|{node.Id}|{node.Generation}");
                    node.RegrowAt = due.AddSeconds(settings.RespawnSeconds * (1 + settings.JitterFraction * (2 * u - 1)));
                }
            }
    }

    private double Unit(string key)
    {
        var bytes = SHA256.HashData(Encoding.UTF8.GetBytes($"{seed}|{key}"));
        return BinaryPrimitives.ReadUInt64BigEndian(bytes.AsSpan(0, 8)) / 18446744073709551616.0;
    }
}
