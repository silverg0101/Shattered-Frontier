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

internal sealed class GatheringGrpcService(PostgresGatheringStore store) : Pb.Gathering.GatheringBase
{
    public override Task<Pb.NodeReply> GetNode(Pb.GetNodeRequest request, ServerCallContext context) =>
        GrpcRun.Call(context, () =>
        {
            if (string.IsNullOrWhiteSpace(request.NodeId)) throw new ArgumentException("Node ID required.");
            return store.Get(request.NodeId);
        });

    public override Task<Pb.HarvestReply> Harvest(Pb.HarvestRequest request, ServerCallContext context) =>
        GrpcRun.Call(context, () => store.Harvest(request));
}
