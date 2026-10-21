using Grpc.Core;
using ShatteredFrontier.CraftingContent;
using ShatteredFrontier.InventoryLedger;
using Pb = ShatteredFrontier.Crafting.V1;

internal sealed class CraftingGrpcService(CraftAttemptService crafting, InventoryLedger ledger, RecipeCatalog recipes,
    ShatteredFrontier.InventoryLedger.Postgres.PostgresInventoryLedgerStore ledgerStore)
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
            _ = ledgerStore.Execute(s => s.Operations.ContainsKey(request.OperationId));
            var container = ledger.Read(request.ContainerId);
            if (container.Spec.OwnerKind != OwnerKind.Character || container.Spec.OwnerId != request.CharacterId)
                throw new GameplayRejectedException("Container is not owned by character.", StatusCode.PermissionDenied);
            var recipe = recipes.Get(request.RecipeId);
            LedgerReceipt receipt;
            try
            {
                receipt = crafting.Execute(new CraftAttempt(request.OperationId, request.ContainerId,
                    request.ExpectedContainerRevision, recipe.Id, recipe.Station, request.Count));
            }
            catch (LedgerRejectedException ex) when (ex.Message.Contains("Conflicting operation ID", StringComparison.Ordinal))
            { throw new GameplayRejectedException(ex.Message, StatusCode.AlreadyExists); }
            var reply = new Pb.CraftReply { OperationId = receipt.OperationId,
                ContainerRevision = receipt.Revisions.Single().After };
            reply.Changes.AddRange(receipt.Changes.Select(c => new Pb.ItemChange { DefinitionId = c.DefinitionId, Delta = c.Delta }));
            return reply;
        });
}
