using ShatteredFrontier.CharacterState.Postgres;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;

internal sealed class QuestRewardPayout(PostgresCharacterStateStore state, PostgresInventoryLedgerStore ledgerStore)
{
    private readonly InventoryLedger ledger = new(ledgerStore);

    public void Resolve(string operationId)
    {
        var intent = state.Reward(operationId) ?? throw new InvalidOperationException("Reward intent missing.");
        if (string.IsNullOrWhiteSpace(intent.ContainerId)) throw new InvalidOperationException("Reward container missing.");
        if (intent.BundleId != "reward/frontier_founder_completion")
            throw new InvalidOperationException("Unknown reward bundle.");
        const string item = "item/frontier_founder_completion_bundle";
        var container = ledger.Read(intent.ContainerId);
        if (container.Spec.OwnerKind != OwnerKind.Character || container.Spec.OwnerId != intent.CharacterId)
            throw new InvalidOperationException("Reward container owner changed.");
        var existing = ledgerStore.Execute(s => s.Operations.TryGetValue(operationId, out var op) ? op : null);
        if (existing is null)
        {
            try { ledger.Grant(operationId, intent.ContainerId, container.Revision, item, 1, "quest_reward", operationId); }
            catch (LedgerRejectedException)
            {
                // Another host may have committed the same deterministic operation concurrently.
            }
            existing = ledgerStore.Execute(s => s.Operations.TryGetValue(operationId, out var op) ? op : null);
        }
        if (existing?.Receipt is not { Action: LedgerAction.Grant } receipt || receipt.Changes.Count != 1 ||
            receipt.Changes[0].ContainerId != intent.ContainerId || receipt.Changes[0].DefinitionId != item ||
            receipt.Changes[0].Delta != 1)
            throw new InvalidOperationException("Reward ledger grant is absent or differs from intent.");
        if (Environment.GetEnvironmentVariable("SF_GAMEPLAY_FAULT_AFTER_QUEST_GRANT") == operationId)
            Environment.FailFast("Injected post-reward host fault.");
        state.CompleteReward(operationId);
    }
}
