using Grpc.Core;
using ShatteredFrontier.Rewards;
using Pb = ShatteredFrontier.Rewards.V1;

internal sealed class RewardGrpcService(BoarRewardService rewards) : Pb.Rewards.RewardsBase
{
    public override Task<Pb.RewardReceipt> GrantDefeatReward(Pb.GrantDefeatRewardRequest request, ServerCallContext context) =>
        GrpcRun.Call(context, () =>
        {
            if (request.TableId != "loot/greenlands_silverbrook_boar")
                throw new ArgumentException("Unknown reward table.");
            var outcome = rewards.Grant(new DefeatEvent(request.DefeatEventId, request.KillerCharacterId,
                request.RecipientCharacterId, request.ContainerId, request.ExpectedContainerRevision,
                request.KillerInProtectedTown, request.BoarInProtectedTown));
            var change = outcome.Receipt?.Changes.SingleOrDefault();
            return new Pb.RewardReceipt
            {
                Granted = outcome.Decision == RewardDecision.Granted,
                SourceCode = outcome.SourceCode,
                LedgerOperationId = outcome.Receipt?.OperationId ?? "",
                DefinitionId = change?.DefinitionId ?? "",
                Quantity = change?.Delta ?? 0,
                ContainerRevision = outcome.Receipt?.Revisions.SingleOrDefault()?.After ?? 0
            };
        });
}
