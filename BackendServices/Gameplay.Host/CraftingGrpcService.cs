using Grpc.Core;
using ShatteredFrontier.CraftingContent;
using ShatteredFrontier.InventoryLedger;
using Pb = ShatteredFrontier.Crafting.V1;

internal sealed class CraftingGrpcService(CraftAttemptService crafting, InventoryLedger ledger, RecipeCatalog recipes,
    ShatteredFrontier.InventoryLedger.Postgres.PostgresInventoryLedgerStore ledgerStore,
    ShatteredFrontier.CharacterState.Postgres.PostgresCharacterStateStore characters, CraftingControls controls,
    RecipeLearning learning, CraftSeedSecret seed)
    : Pb.Crafting.CraftingBase
{
    public override Task<Pb.CraftReply> Craft(Pb.CraftRequest request, ServerCallContext context) =>
        GrpcRun.Call(context, () =>
        {
            if (string.IsNullOrWhiteSpace(request.OperationId) || string.IsNullOrWhiteSpace(request.CharacterId) ||
                string.IsNullOrWhiteSpace(request.ContainerId) || string.IsNullOrWhiteSpace(request.RecipeId) ||
                request.ExpectedContainerRevision < 0 || request.Count is < 1 or > 20)
                throw new ArgumentException("Craft identity, revision, and count required.");
            // Probe the durable receipt before reading mutable container state. Convert
            // validates the full fingerprint and replays before revision comparison.
            var stored = ledgerStore.Execute(s => s.Operations.TryGetValue(request.OperationId, out var op) ? op : null);
            if (stored is not null && stored.Receipt.Action != LedgerAction.Convert)
                throw new GameplayRejectedException("Craft operation ID conflicts with another ledger action.", StatusCode.AlreadyExists);
            var container = ledger.Read(request.ContainerId);
            if (container.Spec.OwnerKind != OwnerKind.Character || container.Spec.OwnerId != request.CharacterId)
                throw new GameplayRejectedException("Container is not owned by character.", StatusCode.PermissionDenied);
            var recipe = recipes.Get(request.RecipeId);
            var progression = characters.ReadProgression(request.CharacterId)?.Result;
            if (recipe.Learn?.Sources.Contains("default") != true &&
                progression?.KnownRecipes?.Any(k => k.RecipeId == recipe.Id) != true)
                throw new GameplayRejectedException("recipe_unknown", StatusCode.FailedPrecondition);
            var grades = new Dictionary<string, string>(StringComparer.Ordinal);
            foreach (var input in request.InputGrades)
                if (!grades.TryAdd(input.DefinitionId, input.SelectedDefinitionId))
                    throw new ArgumentException("Duplicate input grade.");
            var roll = CraftingQuality.Roll(request.OperationId, seed.Bytes, controls);
            var score = stored is null ? CraftingQuality.Score(
                progression?.Skills.GetValueOrDefault("skill/" + recipe.Skill) ?? 0, recipe.Difficulty,
                CraftingQuality.MaterialMidpoint(recipe, grades), roll.Variance, controls)
                : CraftingQuality.StoredScore(stored.Fingerprint) ?? throw new GameplayRejectedException("Stored craft score missing.", StatusCode.AlreadyExists);
            var discovery = stored is null ? CraftingQuality.Discover(recipe,
                progression?.Skills.GetValueOrDefault("skill/" + recipe.Skill) ?? 0, score,
                roll.DiscoveryRoll, roll.Choice,
                (progression?.KnownRecipes ?? []).Select(k => k.RecipeId).ToHashSet(StringComparer.Ordinal), recipes, controls)
                : CraftingQuality.StoredDiscovery(stored.Fingerprint);
            LedgerReceipt receipt;
            GradedCraftResult graded;
            try
            {
                graded = crafting.ExecuteGraded(new CraftAttempt(request.OperationId, request.ContainerId,
                    request.ExpectedContainerRevision, recipe.Id, recipe.Station, request.Count),
                    progression?.Skills.GetValueOrDefault("skill/" + recipe.Skill) ?? 0, grades, seed.Bytes, controls,
                    score, discovery ?? "");
                receipt = graded.Receipt;
            }
            catch (LedgerRejectedException ex) when (ex.Message.Contains("Conflicting operation ID", StringComparison.Ordinal))
            { throw new GameplayRejectedException(ex.Message, StatusCode.AlreadyExists); }
            var reply = new Pb.CraftReply { OperationId = receipt.OperationId,
                ContainerRevision = receipt.Revisions.Single().After, QualityScore = graded.QualityScore,
                Grade = graded.Grade.ToString().ToLowerInvariant() };
            var previousDiscovery = characters.RecipeIntent(request.OperationId + "/discovery");
            discovery = previousDiscovery?.RecipeId ?? discovery;
            if (discovery is not null)
            {
                var target = recipes.Get(discovery);
                try
                {
                    var intent = characters.BeginRecipeLearn(request.OperationId + "/discovery", request.CharacterId,
                        target.Id, "discovery", recipe.Id, "", 0, 0, "skill/" + target.Skill,
                        target.Learn?.LearnSkillMin ?? Math.Max(0, target.Difficulty - 100), controls.MaxKnownRecipes);
                    learning.Complete(intent);
                    reply.DiscoveredRecipeId = discovery;
                }
                catch (ShatteredFrontier.CharacterState.Postgres.CharacterStoreRejectedException ex) when (
                    ex.Message is "already_known" or "known_recipe_limit" or "skill_too_low" or "recipe_learning_pending") { }
            }
            reply.Changes.AddRange(receipt.Changes.Select(c => new Pb.ItemChange { DefinitionId = c.DefinitionId, Delta = c.Delta }));
            return reply;
        });
}
