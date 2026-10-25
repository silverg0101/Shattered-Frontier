using Grpc.Core;
using ShatteredFrontier.Rewards;
using Pb = ShatteredFrontier.Rewards.V1;

internal sealed class RewardGrpcService(DefeatRewardPayout payout, SharedDefeatDropPayout shared, WorldLootGateway loot) : Pb.Rewards.RewardsBase
{
    public override Task<Pb.LootSourcesReply> CreateCorpseLoot(Pb.CreateCorpseLootRequest request, ServerCallContext context) => GrpcRun.Call(context, () => loot.Create(request));
    public override Task<Pb.LootSourcesReply> ListLootSources(Pb.ListLootSourcesRequest request, ServerCallContext context) => GrpcRun.Call(context, () => loot.List(request.ZoneId));
    public override Task<Pb.LootSourceView> OpenLootSource(Pb.OpenLootSourceRequest request, ServerCallContext context) => GrpcRun.Call(context, () => loot.Open(request));
    public override Task<Pb.LootSourceView> TakeLoot(Pb.TakeLootRequest request, ServerCallContext context) => GrpcRun.Call(context, () => loot.Take(request));
    public override Task<Pb.LootSourceView> DropLoot(Pb.DropLootRequest request, ServerCallContext context) => GrpcRun.Call(context, () => loot.Drop(request));
    private static Pb.SharedDefeatDropReceipt SharedReceipt(SharedDropResult result)
    {
        var receipt = new Pb.SharedDefeatDropReceipt
        {
            DefeatEventId = result.EventId,
            ClaimantCharacterId = result.ClaimantId ?? "",
            LedgerOperationId = result.OperationId ?? "",
            ContainerRevision = result.Revision,
            RaidGroupId = result.RaidGroupId
        };
        receipt.Items.AddRange(result.Items.Select(i => new Pb.SharedDropItem
            { DefinitionId = i.DefinitionId, Quantity = i.Quantity }));
        return receipt;
    }

    public override Task<Pb.SharedDefeatDropReceipt> CreateSharedDefeatDrop(Pb.CreateSharedDefeatDropRequest request,
        ServerCallContext context) => GrpcRun.Call(context, () => SharedReceipt(shared.Create(request.DefeatEventId,
            request.CreatureId, request.TableId, request.KillerCharacterId, request.EligibleCharacterIds,
            request.KillerInProtectedTown, request.CreatureInProtectedTown, request.RaidGroupId)));

    public override Task<Pb.SharedDefeatDropReceipt> ClaimSharedDefeatDrop(Pb.ClaimSharedDefeatDropRequest request,
        ServerCallContext context) => GrpcRun.Call(context, () => SharedReceipt(shared.Claim(request.DefeatEventId,
            request.CharacterId, request.ContainerId)));

    public override Task<Pb.RewardReceipt> GrantDefeatReward(Pb.GrantDefeatRewardRequest request, ServerCallContext context) =>
        GrpcRun.Call(context, () =>
        {
            var creatureId = string.IsNullOrEmpty(request.CreatureId) && request.TableId == "loot/greenlands_silverbrook_boar"
                ? "npc/boar" : request.CreatureId;
            var outcome = payout.Grant(new DefeatEvent(request.DefeatEventId, request.KillerCharacterId,
                request.RecipientCharacterId, request.ContainerId, request.ExpectedContainerRevision,
                request.KillerInProtectedTown, request.BoarInProtectedTown), creatureId, request.TableId);
            var receipt = new Pb.RewardReceipt
            {
                Granted = outcome.Granted,
                SourceCode = BoarRewardService.SourceCode,
                LedgerOperationId = outcome.Granted ? outcome.OperationId : "",
                DefinitionId = outcome.Granted && outcome.Items.Count == 1 ? outcome.Items[0].DefinitionId : "",
                Quantity = outcome.Items.Count == 1 ? outcome.Items[0].Quantity : 0,
                ContainerRevision = outcome.Revision
            };
            receipt.Items.AddRange(outcome.Items.Select(i => new Pb.SharedDropItem
                { DefinitionId = i.DefinitionId, Quantity = i.Quantity }));
            return receipt;
        });
}
