using Grpc.Core;
using ShatteredFrontier.Rewards;
using Pb = ShatteredFrontier.Rewards.V1;

internal sealed class RewardGrpcService(DefeatRewardPayout payout) : Pb.Rewards.RewardsBase
{
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
