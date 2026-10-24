using Grpc.Core;
using Google.Protobuf;
using CharacterAppearance;
using System.Text.RegularExpressions;
using ShatteredFrontier.CharacterState.Postgres;
using ShatteredFrontier.Progression;
using ShatteredFrontier.QuestContent;
using Pb = ShatteredFrontier.Progression.V1;

internal sealed class ProgressionGrpcService(PostgresCharacterStateStore store, IReadOnlyDictionary<string, QuestDefinition> quests, QuestRewardPayout payout,
    ShatteredFrontier.InventoryLedger.Postgres.PostgresInventoryLedgerStore ledgerStore,
    ShatteredFrontier.Exchange.Postgres.PostgresExchangeStore exchange, StarterKitOptions starterKit,
    RecipeLearning learning, AppearanceAllowlist appearanceAllowlist, AppearanceValidator appearanceValidator,
    Func<double> skillRoll, ProgressionSettings progressionSettings) : Pb.Progression.ProgressionBase
{
    private static readonly IReadOnlySet<ushort> NoCosmetics = new HashSet<ushort>();

    public override Task<Pb.CreateCharacterReply> CreateCharacter(Pb.CreateCharacterRequest request, ServerCallContext context) =>
        GrpcRun.Call(context, () =>
        {
            if (string.IsNullOrWhiteSpace(request.CharacterId)) throw new ArgumentException("Character ID required.");
            byte[] packet = request.AppearancePacket.ToByteArray();
            var validation = appearanceValidator.ValidatePacked(packet, NoCosmetics, NoCosmetics);
            if (!validation.Valid || !AppearanceCodec.TryUnpack(packet, out var appearance) || appearance!.FaceIndex is null)
                throw new GameplayRejectedException("APPEARANCE_INVALID", StatusCode.InvalidArgument);
            return new Pb.CreateCharacterReply { AppearanceRevision = store.SaveAppearance(request.CharacterId, 0,
                packet, appearanceValidator, NoCosmetics, NoCosmetics) };
        });

    public override Task<Pb.LearnRecipeReply> LearnRecipe(Pb.LearnRecipeRequest request, ServerCallContext context) =>
        GrpcRun.Call(context, () =>
        {
            var result = learning.Learn(request.OperationId, request.CharacterId, request.RecipeId,
                request.Source, request.TrainerNpc, request.ScrollContainer, request.ExpectedContainerRevision);
            return new Pb.LearnRecipeReply { RecipeId = request.RecipeId, ProgressionRevision = result.Revision,
                CostCopper = result.Cost };
        });
    public override Task<Pb.SavePositionReply> SavePosition(Pb.SavePositionRequest request, ServerCallContext context) =>
        GrpcRun.Call(context, () =>
        {
            if (string.IsNullOrWhiteSpace(request.CharacterId) || request.Position is null ||
                !Regex.IsMatch(request.Position.ZoneId, "^[a-z0-9_]{1,64}$", RegexOptions.CultureInvariant) ||
                !ValidCoordinate(request.Position.X) || !ValidCoordinate(request.Position.Y) ||
                !ValidCoordinate(request.Position.Z) || !float.IsFinite(request.Position.Yaw) ||
                Math.Abs(request.Position.Yaw) > 360)
                throw new ArgumentException("Invalid character position.");
            var p = request.Position;
            return new Pb.SavePositionReply { PositionRevision = store.SavePosition(request.CharacterId,
                request.ExpectedPositionRevision, new CharacterPositionSnapshot(p.ZoneId, p.X, p.Y, p.Z, p.Yaw)) };
        });

    private static bool ValidCoordinate(double value) => double.IsFinite(value) && Math.Abs(value) <= 1e7;

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
                    TimeProvider.System, skillRoll, progressionSettings);
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
                if (!quests.TryGetValue(q.QuestId, out var quest)) throw new ArgumentException("Unknown quest.");
                var occurred = DateTimeOffset.FromUnixTimeMilliseconds(q.OccurredAtUnixMs);
                if (q.Kind is "accept" or "abandon")
                {
                    var done = q.Kind == "accept"
                        ? store.AcceptQuest(quest, request.CharacterId, occurred, request.SourceReceiptId,
                            [.. (quest.Offer?.RequiresCompleted ?? []).Select(id => quests.TryGetValue(id, out var p) ? p : throw new ArgumentException("Unknown prerequisite quest."))])
                        : store.AbandonQuest(quest, request.CharacterId, request.SourceReceiptId);
                    reply.QuestAccepted = done.Result.Accepted;
                    reply.QuestReason = done.Result.Reason;
                    reply.ActiveStepId = done.Result.ActiveStepId ?? "";
                    reply.QuestCompleted = done.Result.Completed;
                    reply.QuestRevision = done.Revision;
                    return reply;
                }
                var containers = ledgerStore.FindContainers(ShatteredFrontier.InventoryLedger.OwnerKind.Character,
                    request.CharacterId, ShatteredFrontier.InventoryLedger.ContainerKind.Backpack);
                if (containers.Count != 1)
                    throw new GameplayRejectedException("Character must have exactly one reward backpack.", StatusCode.FailedPrecondition);
                var rewardContainerId = containers[0].Id;
                var eventReceipt = new ObjectiveEvent(request.SourceReceiptId, request.CharacterId, q.QuestId,
                    q.StepId, q.Kind, q.TargetId, string.IsNullOrEmpty(q.NodeId) ? null : q.NodeId, q.Quantity, occurred);
                var result = store.ApplyQuest(quest, eventReceipt, request.ExpectedQuestRevision, rewardContainerId);
                reply.QuestAccepted = result.Result.Accepted;
                reply.QuestReason = result.Result.Accepted ? "" : result.Result.Reason;
                reply.ActiveStepId = result.Result.ActiveStepId ?? "";
                reply.QuestCompleted = result.Result.Completed;
                reply.QuestRevision = result.Revision;
                reply.RewardBundleIds.AddRange(result.Result.Grants.Select(g => g.BundleId));
                foreach (var grant in result.Result.Grants)
                {
                    if (store.Reward(grant.OperationId)?.ContainerId != rewardContainerId)
                        throw new GameplayRejectedException("Reward container conflicts with original receipt.", StatusCode.AlreadyExists);
                    reply.GrantedItems.AddRange(payout.Resolve(grant.OperationId).Select(i =>
                        new Pb.GrantedItem { DefinitionId = i.DefinitionId, Quantity = i.Quantity }));
                }
            }
            return reply;
        });

    public override Task<Pb.CharacterStateReply> GetCharacterState(Pb.GetCharacterStateRequest request, ServerCallContext context) =>
        GrpcRun.Call(context, () =>
        {
            if (string.IsNullOrWhiteSpace(request.CharacterId)) throw new ArgumentException("Character ID required.");
            // The character restore on connect is first spawn: the kit purse is credited once (idempotent operation ID).
            if (starterKit.Kit is { PurseCopper: > 0 } kit && request.CharacterId.StartsWith("chr_", StringComparison.Ordinal))
                exchange.Credit(kit.PurseOperationId(request.CharacterId), request.CharacterId, kit.PurseCopper, "starter_kit");
            var progression = store.ReadProgression(request.CharacterId);
            var state = new Pb.CharacterStateReply
            {
                ProgressionRevision = progression?.Revision ?? 0,
                LifetimeXp = progression?.Result.LifetimeXp ?? 0,
                QuestRevision = 0,
                FaceId = "Orlando"
            };
            var storedAppearance = store.ReadAppearance(request.CharacterId);
            if (storedAppearance is not null)
            {
                var packet = storedAppearance.Result;
                if (!appearanceValidator.ValidatePacked(packet, NoCosmetics, NoCosmetics).Valid ||
                    !AppearanceCodec.TryUnpack(packet, out var appearance))
                    throw new CharacterStoreRejectedException("Stored appearance is invalid.");
                state.AppearancePacket = ByteString.CopyFrom(packet);
                if (appearance!.FaceIndex is byte faceIndex) state.FaceId = appearanceAllowlist.Faces[faceIndex].Id;
            }
            state.Level = progression is null ? 1 : new CharacterProgression(request.CharacterId,
                TimeProvider.System, () => .5, snapshot: progression.Result).Level;
            if (progression is not null)
            {
                foreach (var (id, value) in progression.Result.Skills)
                {
                    state.Skills.Add(id, value);
                    if (id is "skill/logging" or "skill/mining") state.GatheringSkills.Add(id, value);
                }
                state.KnownRecipes.AddRange((progression.Result.KnownRecipes ?? []).Select(k => new Pb.KnownRecipe
                {
                    RecipeId = k.RecipeId, Source = k.Source, LearnedAtUnixMs = k.LearnedAt.ToUnixTimeMilliseconds()
                }));
            }
            var pending = store.PendingRewardQuests(request.CharacterId);
            foreach (var row in store.ReadQuests(request.CharacterId))
            {
                if (!quests.TryGetValue(row.QuestId, out var def)) continue;
                var done = row.State.StepIndex >= def.Steps.Count;
                var qs = new Pb.QuestState { QuestId = row.QuestId, State = row.State.Abandoned ? "abandoned" : done ? "completed" : "active",
                    ActiveStepId = row.State.Abandoned || done ? "" : def.Steps[row.State.StepIndex].Id, ContentRevision = row.ContentRevision,
                    Revision = row.Revision, RewardPending = pending.Contains(row.QuestId) };
                if (qs.State == "active")
                    foreach (var objective in def.Steps[row.State.StepIndex].Objectives) qs.Counts.Add(objective.Key, row.State.Counts.GetValueOrDefault(objective.Key));
                state.Quests.Add(qs);
                if (row.QuestId != "quest/frontier_founder" || row.State.Abandoned) continue;
                state.QuestRevision = row.Revision; state.QuestCompleted = done; state.ActiveStepId = qs.ActiveStepId;
                foreach (var (k, v) in qs.Counts) state.ObjectiveCounts.Add(k, v);
            }
            var position = store.ReadPosition(request.CharacterId);
            if (position is not null)
            {
                state.PositionRevision = position.Revision;
                state.Position = new Pb.CharacterPosition { ZoneId = position.Result.ZoneId,
                    X = position.Result.X, Y = position.Result.Y, Z = position.Result.Z, Yaw = position.Result.Yaw };
            }
            return state;
        });
}
