using Grpc.Core;
using ShatteredFrontier.Rewards;
using Pb = ShatteredFrontier.Rewards.V1;

internal sealed class RewardGrpcService(DefeatRewardPayout payout, SharedDefeatDropPayout shared) : Pb.Rewards.RewardsBase
{
    private static Pb.SharedDefeatDropReceipt SharedReceipt(SharedDropResult result)
    {
        var receipt = new Pb.SharedDefeatDropReceipt
        {
            DefeatEventId = result.EventId,
            ClaimantCharacterId = result.ClaimantId ?? "",
            LedgerOperationId = result.OperationId ?? "",
            ContainerRevision = result.Revision
        };
        receipt.Items.AddRange(result.Items.Select(i => new Pb.SharedDropItem
            { DefinitionId = i.DefinitionId, Quantity = i.Quantity }));
        return receipt;
    }

    public override Task<Pb.SharedDefeatDropReceipt> CreateSharedDefeatDrop(Pb.CreateSharedDefeatDropRequest request,
        ServerCallContext context) => GrpcRun.Call(context, () => SharedReceipt(shared.Create(request.DefeatEventId,
            request.CreatureId, request.TableId, request.KillerCharacterId, request.EligibleCharacterIds,
            request.KillerInProtectedTown, request.CreatureInProtectedTown)));

    public override Task<Pb.SharedDefeatDropReceipt> ClaimSharedDefeatDrop(Pb.ClaimSharedDefeatDropRequest request,
        ServerCallContext context) => GrpcRun.Call(context, () => SharedReceipt(shared.Claim(request.DefeatEventId,
            request.CharacterId, request.ContainerId)));

    public override Task<Pb.RewardReceipt> GrantDefeatReward(Pb.GrantDefeatRewardRequest request, ServerCallContext context) =>
        GrpcRun.Call(context, () =>
        {
            if (request.TableId != "loot/greenlands_silverbrook_boar")
                throw new ArgumentException("Unknown reward table.");
            var outcome = payout.Grant(new DefeatEvent(request.DefeatEventId, request.KillerCharacterId,
                request.RecipientCharacterId, request.ContainerId, request.ExpectedContainerRevision,
                request.KillerInProtectedTown, request.BoarInProtectedTown), request.TableId);
            return new Pb.RewardReceipt
            {
                Granted = outcome.Granted,
                SourceCode = BoarRewardService.SourceCode,
                LedgerOperationId = outcome.Granted ? outcome.OperationId : "",
                DefinitionId = outcome.Granted ? "item/boar_hide" : "",
                Quantity = outcome.Quantity,
                ContainerRevision = outcome.Revision
            };
        });
}
