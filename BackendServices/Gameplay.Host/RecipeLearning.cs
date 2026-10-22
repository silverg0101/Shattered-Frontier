using Grpc.Core;
using ShatteredFrontier.CharacterState.Postgres;
using ShatteredFrontier.CraftingContent;
using ShatteredFrontier.Exchange.Postgres;
using ShatteredFrontier.InventoryLedger;

internal sealed class RecipeLearning(RecipeCatalog catalog, CraftingControls controls,
    PostgresCharacterStateStore characters, PostgresExchangeStore exchange, InventoryLedger ledger)
{
    public (long Revision, long Cost) Learn(string operationId, string characterId, string recipeId,
        string source, string trainerNpc, string scrollContainer, long expectedContainerRevision)
    {
        if (string.IsNullOrWhiteSpace(operationId) || string.IsNullOrWhiteSpace(characterId) ||
            !characterId.StartsWith("chr_", StringComparison.Ordinal) || expectedContainerRevision < 0)
            throw new ArgumentException("Invalid learn identity or revision.");
        var recipe = catalog.Get(recipeId);
        if (source is not ("trainer" or "scroll") || recipe.Learn?.Sources.Contains(source) != true)
            throw new GameplayRejectedException("recipe_source_unavailable", StatusCode.FailedPrecondition);
        string sourceId; long cost;
        if (source == "trainer")
        {
            if (recipe.Learn.Trainers?.Contains(trainerNpc) != true || scrollContainer != "")
                throw new GameplayRejectedException("trainer_unavailable", StatusCode.FailedPrecondition);
            sourceId = trainerNpc; cost = controls.TrainerFee(recipe.Difficulty);
        }
        else
        {
            if (trainerNpc != "" || string.IsNullOrWhiteSpace(scrollContainer)) throw new ArgumentException("Scroll container required.");
            var container = ledger.Read(scrollContainer);
            if (container.Spec.OwnerKind != OwnerKind.Character || container.Spec.OwnerId != characterId)
                throw new GameplayRejectedException("scroll_container_not_owned", StatusCode.PermissionDenied);
            sourceId = "item/recipe_scroll_" + recipeId["recipe/".Length..]; cost = 0;
        }
        var intent = characters.BeginRecipeLearn(operationId, characterId, recipeId, source, sourceId,
            scrollContainer, expectedContainerRevision, cost, "skill/" + recipe.Skill,
            recipe.Learn.LearnSkillMin ?? Math.Max(0, recipe.Difficulty - 100), controls.MaxKnownRecipes);
        return (Complete(intent), cost);
    }

    public long Complete(RecipeLearnIntent intent)
    {
        if (intent.State == "Complete") return characters.ReadProgression(intent.CharacterId)?.Revision ?? 0;
        if (intent.Source == "trainer")
        {
            try { exchange.TrainingDebit(intent.OperationId + "/training", intent.CharacterId, intent.CostCopper); }
            catch (ExchangeRejectedException ex) when (ex.Message == "Insufficient spendable gold.")
            {
                characters.CancelRecipeLearn(intent);
                throw new GameplayRejectedException("insufficient_gold", StatusCode.FailedPrecondition);
            }
        }
        else if (intent.Source == "scroll")
        {
            ledger.Consume(intent.OperationId + "/scroll", intent.ContainerId, intent.ExpectedContainerRevision,
                intent.SourceId, 1, "recipe_learning");
            if (Environment.GetEnvironmentVariable("SF_GAMEPLAY_FAULT_AFTER_SCROLL_CONSUME") == intent.OperationId)
                Environment.FailFast("CRF-01 fixture: scroll consumed, stopped before knowledge write.");
        }
        return characters.CompleteRecipeLearn(intent, DateTimeOffset.UtcNow);
    }
}
