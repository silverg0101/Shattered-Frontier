using Grpc.Core;
using ShatteredFrontier.GatheringNodes;
using Pb = ShatteredFrontier.Gathering.V1;

// Captured at host start. Realm tuning publication must validate this snapshot before a compatible release;
// the fixture-only shortened timer below never changes production defaults or a live schedule.
internal sealed record GatheringHostTuning(int OakCharges, int IronCharges, int StoneCharges, int UnitsPerHarvest,
    NodeSettings Lifecycle)
{
    public static GatheringHostTuning Shipped { get; } = new(3, 5, 3, 1, NodeSettings.Shipped);
    public void Validate()
    {
        if (OakCharges is < 1 or > 20 || IronCharges is < 1 or > 20 || StoneCharges is < 1 or > 20 ||
            UnitsPerHarvest is < 1 or > 100) throw new ArgumentOutOfRangeException(nameof(GatheringHostTuning));
        Lifecycle.Validate();
    }
    public NodeDefinition[] Definitions() =>
    [
        new("gathering_node/oak_tree", ResourceKind.Wood, "item/oak_log", OakCharges, UnitsPerHarvest),
        new("gathering_node/iron_vein", ResourceKind.Ore, "item/iron_ore", IronCharges, UnitsPerHarvest),
        new("gathering_node/stone_outcrop", ResourceKind.Ore, "item/rough_stone", StoneCharges, UnitsPerHarvest)
    ];
}

internal sealed class GatheringReplyStore
{
    public object Gate { get; } = new();
    public Dictionary<string, (Pb.HarvestRequest Request, Pb.HarvestReply Reply)> Receipts { get; } = new(StringComparer.Ordinal);
}

internal sealed class GatheringGrpcService(GatheringNodeService nodes, GatheringReplyStore replies) : Pb.Gathering.GatheringBase
{
    private static Pb.NodeReply Wire(NodeSnapshot n) => new()
    {
        NodeId = n.Id, DefinitionId = n.DefinitionId, Generation = n.Generation,
        Charges = n.Charges, MaxCharges = n.MaxCharges,
        RegrowAtUnixMs = n.RegrowAt?.ToUnixTimeMilliseconds() ?? 0,
        GrowthStage = n.GrowthStage ?? "",
        NextTransitionUnixMs = n.NextTransitionAt?.ToUnixTimeMilliseconds() ?? 0
    };

    public override Task<Pb.NodeReply> GetNode(Pb.GetNodeRequest request, ServerCallContext context) =>
        GrpcRun.Call(context, () =>
        {
            if (string.IsNullOrWhiteSpace(request.NodeId)) throw new ArgumentException("Node ID required.");
            var node = nodes.Snapshot().FirstOrDefault(n => n.Id == request.NodeId);
            if (node is null) throw new GameplayRejectedException("Unknown gathering node.", StatusCode.NotFound);
            return Wire(node);
        });

    public override Task<Pb.HarvestReply> Harvest(Pb.HarvestRequest request, ServerCallContext context) =>
        GrpcRun.Call(context, () =>
        {
            if (string.IsNullOrWhiteSpace(request.OperationId) || string.IsNullOrWhiteSpace(request.CharacterId) ||
                string.IsNullOrWhiteSpace(request.NodeId) || request.Generation < 1)
                throw new ArgumentException("Harvest identity and generation required.");
            lock (replies.Gate)
            {
                if (replies.Receipts.TryGetValue(request.OperationId, out var prior))
                {
                    if (!prior.Request.Equals(request)) throw new InvalidOperationException("Operation ID reused with different harvest.");
                    return prior.Reply;
                }
                var result = nodes.Harvest(new(request.OperationId, request.CharacterId, request.NodeId, request.Generation));
                var node = nodes.Snapshot().FirstOrDefault(n => n.Id == request.NodeId);
                var reply = new Pb.HarvestReply
                {
                    Status = result.Status switch
                    {
                        HarvestStatus.Granted => "granted", HarvestStatus.Depleted => "depleted",
                        HarvestStatus.StaleGeneration => "stale_generation", _ => "unknown_node"
                    },
                    ResourceId = result.ResourceId ?? "", Quantity = result.Quantity,
                    Node = node is null ? null : Wire(node)
                };
                replies.Receipts.Add(request.OperationId, (request.Clone(), reply));
                return reply;
            }
        });
}
