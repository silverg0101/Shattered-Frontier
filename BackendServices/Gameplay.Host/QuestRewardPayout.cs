using Grpc.Core;
using ShatteredFrontier.CharacterState.Postgres;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;
using ShatteredFrontier.QuestContent;

internal sealed class QuestRewardPayout(PostgresCharacterStateStore state, PostgresInventoryLedgerStore ledgerStore)
{
    public IReadOnlyList<ItemQuantity> Resolve(string operationId)
    {
        var intent = state.Reward(operationId) ?? throw new InvalidOperationException("Reward intent missing.");
        if (string.IsNullOrWhiteSpace(intent.ContainerId)) throw new InvalidOperationException("Reward container missing.");
        var items = intent.Items.Select(i => new ItemQuantity(i.DefinitionId, i.Quantity)).ToArray();
        if (items.Length == 0 || items.Any(i => string.IsNullOrWhiteSpace(i.DefinitionId) || i.Quantity <= 0)) throw new InvalidOperationException("Reward snapshot invalid.");
        var fingerprint = "quest-reward-v1|" + intent.ContainerId + "|" +
            string.Join("|", items.OrderBy(i => i.DefinitionId, StringComparer.Ordinal).Select(i => $"{i.DefinitionId}:{i.Quantity}"));
        var receipt = ledgerStore.Execute(s =>
        {
            if (s.Operations.TryGetValue(operationId, out var prior))
            {
                if (prior.Fingerprint != fingerprint) throw new InvalidOperationException("Reward operation conflicts with intent.");
                return prior.Receipt;
            }
            if (!s.Containers.TryGetValue(intent.ContainerId, out var container) ||
                container.Spec.OwnerKind != OwnerKind.Character || container.Spec.OwnerId != intent.CharacterId)
                throw new GameplayRejectedException("Reward backpack missing or owner changed.", StatusCode.FailedPrecondition);
            long total;
            try
            {
                total = container.Quantities.Values.Aggregate(0L, (sum, value) => checked(sum + value));
                foreach (var item in items) total = checked(total + item.Quantity);
            }
            catch (OverflowException) { throw new GameplayRejectedException("Reward quantity overflow.", StatusCode.FailedPrecondition); }
            if (total > container.Spec.Capacity)
                throw new GameplayRejectedException("Reward backpack full; intent remains pending.", StatusCode.FailedPrecondition);
            foreach (var item in items)
                container.Quantities[item.DefinitionId] = checked(container.Quantities.GetValueOrDefault(item.DefinitionId) + item.Quantity);
            var before = container.Revision++;
            var committed = new LedgerReceipt(operationId, LedgerAction.Grant,
                [new RevisionChange(intent.ContainerId, before, container.Revision)],
                items.Select(i => new QuantityChange(intent.ContainerId, i.DefinitionId, i.Quantity)).ToArray());
            s.Operations.Add(operationId, new StoredOperation(fingerprint, committed));
            return committed;
        });
        if (receipt.Action != LedgerAction.Grant || receipt.Changes.Count != items.Length ||
            items.Any(item => !receipt.Changes.Any(c => c.ContainerId == intent.ContainerId &&
                c.DefinitionId == item.DefinitionId && c.Delta == item.Quantity)))
            throw new InvalidOperationException("Reward ledger grant differs from intent.");
        if (Environment.GetEnvironmentVariable("SF_GAMEPLAY_FAULT_AFTER_QUEST_GRANT") == operationId)
            Environment.FailFast("Injected post-reward host fault.");
        state.CompleteReward(operationId);
        return items;
    }
}
