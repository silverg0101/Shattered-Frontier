using Grpc.Core;
using ShatteredFrontier.CharacterState.Postgres;
using ShatteredFrontier.Progression;
using ShatteredFrontier.QuestContent;
using Pb = ShatteredFrontier.Progression.V1;

internal sealed class ProgressionGrpcService(PostgresCharacterStateStore store, QuestDefinition quest) : Pb.Progression.ProgressionBase
{
    public override Task<Pb.ApplyReceiptReply> ApplyReceipt(Pb.ApplyReceiptRequest request, ServerCallContext context) =>
        GrpcRun.Call(context, () =>
        {
            if (string.IsNullOrWhiteSpace(request.SourceReceiptId) || string.IsNullOrWhiteSpace(request.CharacterId) ||
                (request.Kind != "quest" && request.Payload is null) ||
                (request.Kind == "quest" && request.Quest is null))
                throw new ArgumentException("Receipt identity and payload required.");
            var kind = request.Kind switch
            {
                "gather" => ReceiptKind.GatheringHarvest,
                "craft" => ReceiptKind.CraftConvert,
                "defeat" => ReceiptKind.EnemyDefeat,
                "quest" => (ReceiptKind?)null,
                _ => throw new ArgumentException("Unknown receipt kind.")
            };
            var reply = new Pb.ApplyReceiptReply();
            if (kind is not null)
            {
                var p = request.Payload;
                var action = new ActionReceipt(request.SourceReceiptId, request.CharacterId, kind.Value, true,
                    p.SkillId, p.Difficulty, p.ActionSeconds, p.ContextHash, p.Success,
                    p.EnemyLevel, p.EnemyTierMultiplier, p.EligiblePartySize);
                var progression = store.ApplyProgression(action, request.ExpectedProgressionRevision,
                    TimeProvider.System, () => Random.Shared.NextDouble());
                reply.ProgressionApplied = progression.Result.Applied;
                reply.SkillGain = progression.Result.SkillGain;
                reply.XpGain = progression.Result.XpGain;
                reply.Level = progression.Result.Level;
                reply.ProgressionRevision = progression.Revision;
            }
            else
            {
                var progression = store.ReadProgression(request.CharacterId);
                reply.ProgressionRevision = progression?.Revision ?? 0;
                reply.Level = progression is null ? 1 : new CharacterProgression(request.CharacterId,
                    TimeProvider.System, () => .5, snapshot: progression.Result).Level;
            }
            if (request.Quest is not null)
            {
                var q = request.Quest;
                if (q.QuestId != quest.Id) throw new ArgumentException("Unknown quest.");
                var occurred = DateTimeOffset.FromUnixTimeMilliseconds(q.OccurredAtUnixMs);
                store.AcceptQuest(quest, request.CharacterId, occurred);
                var eventReceipt = new ObjectiveEvent(request.SourceReceiptId, request.CharacterId, q.QuestId,
                    q.StepId, q.Kind, q.TargetId, string.IsNullOrEmpty(q.NodeId) ? null : q.NodeId, q.Quantity, occurred);
                var result = store.ApplyQuest(quest, eventReceipt, request.ExpectedQuestRevision);
                reply.QuestAccepted = result.Result.Accepted;
                reply.ActiveStepId = result.Result.ActiveStepId ?? "";
                reply.QuestCompleted = result.Result.Completed;
                reply.QuestRevision = result.Revision;
                reply.RewardBundleIds.AddRange(result.Result.Grants.Select(g => g.BundleId));
            }
            return reply;
        });

    public override Task<Pb.CharacterStateReply> GetCharacterState(Pb.GetCharacterStateRequest request, ServerCallContext context) =>
        GrpcRun.Call(context, () =>
        {
            if (string.IsNullOrWhiteSpace(request.CharacterId)) throw new ArgumentException("Character ID required.");
            var progression = store.ReadProgression(request.CharacterId);
            var state = new Pb.CharacterStateReply
            {
                ProgressionRevision = progression?.Revision ?? 0,
                LifetimeXp = progression?.Result.LifetimeXp ?? 0,
                QuestRevision = 0
            };
            state.Level = progression is null ? 1 : new CharacterProgression(request.CharacterId,
                TimeProvider.System, () => .5, snapshot: progression.Result).Level;
            if (progression is not null)
                foreach (var (id, value) in progression.Result.Skills) state.Skills.Add(id, value);
            var q = store.ReadQuest(quest, request.CharacterId);
            if (q is not null)
            {
                state.QuestRevision = q.Revision;
                state.QuestCompleted = q.Result.StepIndex >= quest.Steps.Count;
                state.ActiveStepId = state.QuestCompleted ? "" : quest.Steps[q.Result.StepIndex].Id;
            }
            return state;
        });
}
