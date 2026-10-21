using System.Collections.ObjectModel;
using System.Globalization;
using System.Text;
using System.Text.Json;

namespace ShatteredFrontier.InventoryLedger;

public enum ContainerKind { Equipment, Backpack, Bag, Bank, AccountBank, GuildBank, Mailbox, Escrow, Buyback, ConstructionCache, ConstructionRecovery }
public enum OwnerKind { Character, Account, Guild, System }
public enum LedgerAction { Grant, Consume, Transfer, Convert, Exchange }

public sealed record ContainerSpec(string Id, OwnerKind OwnerKind, string OwnerId, ContainerKind Kind, int Capacity);
public sealed record ContainerView(ContainerSpec Spec, long Revision, IReadOnlyDictionary<string, long> Quantities);
public sealed record RevisionChange(string ContainerId, long Before, long After);
public sealed record QuantityChange(string ContainerId, string DefinitionId, long Delta);
public sealed record LedgerReceipt(string OperationId, LedgerAction Action, IReadOnlyList<RevisionChange> Revisions,
    IReadOnlyList<QuantityChange> Changes);
public sealed record ItemQuantity(string DefinitionId, long Quantity);
public sealed record EscrowDelivery(string FromId, long FromRevision, string ToId, long ToRevision, string DefinitionId, long Quantity);

public sealed class LedgerRejectedException(string message) : InvalidOperationException(message);

// The adapter must commit the complete working state (including the receipt) durably before Execute returns.
// A failed callback must publish nothing. Concurrent writes to the same operation or container serialize.
// Lock order: advisory operation lock first, then container rows in sorted ID order.
public interface IInventoryLedgerStore
{
    T Execute<T>(Func<LedgerState, T> transaction);
    IReadOnlyList<ContainerSpec> FindContainers(OwnerKind ownerKind, string ownerId, ContainerKind kind) =>
        Execute(state => state.Containers.Values.Where(c => c.Spec.OwnerKind == ownerKind && c.Spec.OwnerId == ownerId && c.Spec.Kind == kind)
            .Select(c => c.Spec).ToArray());
}

public sealed class LedgerState
{
    public ContainerIndex Containers { get; }
    public OperationIndex Operations { get; }

    public LedgerState(Func<string, ContainerRecord?>? containerMiss = null, Func<IEnumerable<string>>? allContainerIds = null,
        Func<string, StoredOperation?>? operationMiss = null)
    {
        Containers = new ContainerIndex(containerMiss, allContainerIds);
        Operations = new OperationIndex(operationMiss);
    }

    public LedgerState Copy()
    {
        var copy = new LedgerState();
        foreach (var (id, record) in Containers.Loaded) copy.Containers.Add(id, record.Copy());
        foreach (var (id, operation) in Operations.Loaded) copy.Operations.Add(id, operation);
        return copy;
    }
}

public sealed class OperationIndex(Func<string, StoredOperation?>? miss = null)
{
    private readonly Dictionary<string, StoredOperation> cache = new(StringComparer.Ordinal);
    private readonly List<KeyValuePair<string, StoredOperation>> added = [];
    public IEnumerable<KeyValuePair<string, StoredOperation>> Loaded => cache;
    public IEnumerable<KeyValuePair<string, StoredOperation>> Added => added;
    public bool TryGetValue(string key, out StoredOperation value)
    {
        if (cache.TryGetValue(key, out value!)) return true;
        var found = miss?.Invoke(key);
        if (found is null) return false;
        cache.Add(key, found);
        value = found;
        return true;
    }
    public bool ContainsKey(string key) => TryGetValue(key, out _);
    public void Add(string key, StoredOperation value)
    {
        if (ContainsKey(key)) throw new ArgumentException("Duplicate operation ID.", nameof(key));
        cache.Add(key, value);
        added.Add(new(key, value));
    }
}

public sealed class ContainerIndex(Func<string, ContainerRecord?>? miss = null, Func<IEnumerable<string>>? allIds = null)
{
    private readonly Dictionary<string, ContainerRecord> cache = new(StringComparer.Ordinal);
    private readonly Dictionary<string, ContainerRecord> original = new(StringComparer.Ordinal);
    public IEnumerable<KeyValuePair<string, ContainerRecord>> Loaded => cache;
    public bool TryGetOriginal(string id, out ContainerRecord value) => original.TryGetValue(id, out value!);
    public bool TryGetValue(string id, out ContainerRecord value)
    {
        if (cache.TryGetValue(id, out value!)) return true;
        var found = miss?.Invoke(id);
        if (found is null) return false;
        cache.Add(id, found);
        original.Add(id, found.Copy());
        value = found;
        return true;
    }
    public void Add(string id, ContainerRecord value)
    {
        if (TryGetValue(id, out _)) throw new ArgumentException("Duplicate container ID.", nameof(id));
        cache.Add(id, value);
    }
    public ContainerRecord this[string id] => TryGetValue(id, out var value) ? value : throw new KeyNotFoundException(id);
    public IEnumerable<ContainerRecord> Values
    {
        get
        {
            if (allIds is not null)
                foreach (var id in allIds().OrderBy(x => x, StringComparer.Ordinal)) TryGetValue(id, out _);
            return cache.Values;
        }
    }
}

public sealed record StoredOperation(string Fingerprint, LedgerReceipt Receipt);

public sealed class ContainerRecord(ContainerSpec spec)
{
    public ContainerSpec Spec { get; } = spec;
    public long Revision { get; set; }
    public Dictionary<string, long> Quantities { get; } = new(StringComparer.Ordinal);
    public ContainerRecord Copy()
    {
        var copy = new ContainerRecord(Spec) { Revision = Revision };
        foreach (var (id, quantity) in Quantities) copy.Quantities.Add(id, quantity);
        return copy;
    }
}

public sealed class InMemoryInventoryLedgerStore : IInventoryLedgerStore
{
    private readonly object gate = new();
    private LedgerState state = new();

    public T Execute<T>(Func<LedgerState, T> transaction)
    {
        lock (gate)
        {
            var working = state.Copy();
            var result = transaction(working);
            state = working;
            return result;
        }
    }
}

public sealed class InventoryLedger(IInventoryLedgerStore store)
{
    // Trusted settlement hook: all escrow deliveries commit as one inventory operation.
    public LedgerReceipt DeliverEscrowBatch(string operationId, IReadOnlyList<EscrowDelivery> deliveries)
    {
        Require(operationId);
        if (deliveries.Count == 0) throw new LedgerRejectedException("Empty escrow delivery.");
        foreach (var move in deliveries) ValidateItems([new ItemQuantity(move.DefinitionId, move.Quantity)]);
        var fingerprint = JsonSerializer.Serialize(deliveries);
        return store.Execute(state =>
        {
            if (state.Operations.TryGetValue(operationId, out var prior))
            {
                if (prior.Fingerprint != fingerprint) throw new LedgerRejectedException("Conflicting operation ID reuse.");
                return prior.Receipt;
            }
            var expected = new Dictionary<string, long>(StringComparer.Ordinal);
            foreach (var move in deliveries)
            {
                if (expected.TryGetValue(move.FromId, out var from) && from != move.FromRevision ||
                    expected.TryGetValue(move.ToId, out var to) && to != move.ToRevision)
                    throw new LedgerRejectedException("Conflicting expected revisions.");
                expected[move.FromId] = move.FromRevision;
                expected[move.ToId] = move.ToRevision;
                if (move.FromId == move.ToId) throw new LedgerRejectedException("Delivery requires distinct containers.");
            }
            var records = expected.Keys.OrderBy(x => x, StringComparer.Ordinal)
                .ToDictionary(id => id, id => Get(state, id), StringComparer.Ordinal);
            foreach (var move in deliveries)
                if (records[move.FromId].Spec.Kind != ContainerKind.Escrow || records[move.ToId].Spec.Kind == ContainerKind.Escrow)
                    throw new LedgerRejectedException("Delivery requires escrow source and ordinary destination.");
            foreach (var (id, revision) in expected)
                if (records[id].Revision != revision) throw new LedgerRejectedException("Stale container revision.");
            var changes = new List<QuantityChange>();
            foreach (var move in deliveries)
            {
                var src = records[move.FromId]; var dst = records[move.ToId];
                var available = src.Quantities.GetValueOrDefault(move.DefinitionId);
                if (available < move.Quantity) throw new LedgerRejectedException("Insufficient escrow quantity.");
                var remaining = available - move.Quantity;
                if (remaining == 0) src.Quantities.Remove(move.DefinitionId); else src.Quantities[move.DefinitionId] = remaining;
                long next;
                try { next = checked(dst.Quantities.GetValueOrDefault(move.DefinitionId) + move.Quantity); }
                catch (OverflowException) { throw new LedgerRejectedException("Quantity overflow."); }
                dst.Quantities[move.DefinitionId] = next;
                changes.Add(new(move.FromId, move.DefinitionId, -move.Quantity));
                changes.Add(new(move.ToId, move.DefinitionId, move.Quantity));
            }
            foreach (var record in records.Values)
            {
                long total;
                try { total = record.Quantities.Values.Aggregate(0L, (a, b) => checked(a + b)); }
                catch (OverflowException) { throw new LedgerRejectedException("Capacity overflow."); }
                if (total > record.Spec.Capacity) throw new LedgerRejectedException("Container capacity exceeded.");
            }
            var revisions = records.Select(p => new RevisionChange(p.Key, p.Value.Revision, checked(p.Value.Revision + 1))).ToArray();
            foreach (var record in records.Values) record.Revision++;
            var receipt = new LedgerReceipt(operationId, LedgerAction.Exchange, Array.AsReadOnly(revisions), changes.AsReadOnly());
            state.Operations.Add(operationId, new StoredOperation(fingerprint, receipt));
            return receipt;
        });
    }
    public ContainerView CreateContainer(ContainerSpec spec)
    {
        ValidateSpec(spec);
        return store.Execute(state =>
        {
            if (state.Containers.TryGetValue(spec.Id, out var existing))
            {
                if (existing.Spec != spec) throw new LedgerRejectedException("Container ID already has a different specification.");
                return View(existing);
            }
            var record = new ContainerRecord(spec);
            state.Containers.Add(spec.Id, record);
            return View(record);
        });
    }

    public ContainerView Read(string containerId) => store.Execute(state => View(Get(state, containerId)));

    public LedgerReceipt Grant(string operationId, string containerId, long expectedRevision, string definitionId, long quantity, string source, string causationId) =>
        Apply(operationId, LedgerAction.Grant, [containerId], [expectedRevision], [new ItemQuantity(definitionId, quantity)], [], source, causationId);

    public LedgerReceipt Consume(string operationId, string containerId, long expectedRevision, string definitionId, long quantity, string sink) =>
        Apply(operationId, LedgerAction.Consume, [containerId], [expectedRevision], [new ItemQuantity(definitionId, quantity)], [], sink, "");

    public LedgerReceipt Transfer(string operationId, string fromId, long fromRevision, string toId, long toRevision, string definitionId, long quantity) =>
        Apply(operationId, LedgerAction.Transfer, [fromId, toId], [fromRevision, toRevision], [new ItemQuantity(definitionId, quantity)], [], "", "");

    public LedgerReceipt Convert(string operationId, string containerId, long expectedRevision, string recipeId,
        IReadOnlyList<ItemQuantity> inputs, IReadOnlyList<ItemQuantity> outputs) =>
        Apply(operationId, LedgerAction.Convert, [containerId], [expectedRevision], inputs, outputs, recipeId, "");

    private LedgerReceipt Apply(string operationId, LedgerAction action, string[] ids, long[] revisions,
        IReadOnlyList<ItemQuantity> inputs, IReadOnlyList<ItemQuantity> outputs, string context, string causationId)
    {
        Require(operationId); Require(context, action is LedgerAction.Grant or LedgerAction.Consume or LedgerAction.Convert);
        Require(causationId, action == LedgerAction.Grant);
        if (ids.Length != revisions.Length || ids.Distinct(StringComparer.Ordinal).Count() != ids.Length)
            throw new LedgerRejectedException("Containers must be distinct with one expected revision each.");
        if (action == LedgerAction.Convert && (inputs.Count == 0 || outputs.Count == 0))
            throw new LedgerRejectedException("A recipe needs inputs and outputs.");
        ValidateItems(inputs);
        ValidateItems(outputs);
        var canonical = new StringBuilder("v1|");
        void Field(string value) => canonical.Append(value.Length.ToString(CultureInfo.InvariantCulture)).Append(':').Append(value).Append('|');
        Field(((int)action).ToString(CultureInfo.InvariantCulture));
        Field(ids.Length.ToString(CultureInfo.InvariantCulture));
        foreach (var id in ids) Field(id);
        foreach (var revision in revisions) Field(revision.ToString(CultureInfo.InvariantCulture));
        void Items(IReadOnlyList<ItemQuantity> values)
        {
            Field(values.Count.ToString(CultureInfo.InvariantCulture));
            foreach (var value in values.OrderBy(x => x.DefinitionId, StringComparer.Ordinal))
            {
                Field(value.DefinitionId);
                Field(value.Quantity.ToString(CultureInfo.InvariantCulture));
            }
        }
        Items(inputs);
        Items(outputs);
        Field(context);
        Field(causationId);
        var fingerprint = canonical.ToString();
        return store.Execute(state =>
        {
            if (state.Operations.TryGetValue(operationId, out var prior))
            {
                if (prior.Fingerprint != fingerprint) throw new LedgerRejectedException("Conflicting operation ID reuse.");
                return prior.Receipt;
            }
            foreach (var id in ids.OrderBy(x => x, StringComparer.Ordinal)) Get(state, id);
            var records = ids.Select(id => Get(state, id)).ToArray();
            for (var i = 0; i < ids.Length; i++)
                if (records[i].Revision != revisions[i]) throw new LedgerRejectedException("Stale container revision.");
            if (action == LedgerAction.Transfer && (records[0].Spec.OwnerKind != records[1].Spec.OwnerKind ||
                records[0].Spec.OwnerId != records[1].Spec.OwnerId))
                throw new LedgerRejectedException("Cross-owner transfers require escrow integration.");

            var changes = new List<QuantityChange>();
            void Change(int index, ItemQuantity item, long sign)
            {
                var record = records[index];
                var old = record.Quantities.GetValueOrDefault(item.DefinitionId);
                long next;
                try { next = checked(old + checked(sign * item.Quantity)); }
                catch (OverflowException) { throw new LedgerRejectedException("Quantity overflow."); }
                if (next < 0) throw new LedgerRejectedException("Insufficient quantity.");
                if (next == 0) record.Quantities.Remove(item.DefinitionId);
                else record.Quantities[item.DefinitionId] = next;
                changes.Add(new QuantityChange(ids[index], item.DefinitionId, sign * item.Quantity));
            }
            if (action == LedgerAction.Grant) foreach (var item in inputs) Change(0, item, 1);
            if (action == LedgerAction.Consume) foreach (var item in inputs) Change(0, item, -1);
            if (action == LedgerAction.Transfer) foreach (var item in inputs) { Change(0, item, -1); Change(1, item, 1); }
            if (action == LedgerAction.Convert)
            {
                foreach (var item in inputs) Change(0, item, -1);
                foreach (var item in outputs) Change(0, item, 1);
            }
            foreach (var record in records)
            {
                long total;
                try { total = record.Quantities.Values.Aggregate(0L, (sum, value) => checked(sum + value)); }
                catch (OverflowException) { throw new LedgerRejectedException("Capacity overflow."); }
                if (total > record.Spec.Capacity) throw new LedgerRejectedException("Container capacity exceeded.");
            }
            var revisionChanges = records.Select((record, i) => new RevisionChange(ids[i], record.Revision, checked(record.Revision + 1))).ToArray();
            foreach (var record in records) record.Revision++;
            var receipt = new LedgerReceipt(operationId, action, Array.AsReadOnly(revisionChanges), changes.AsReadOnly());
            state.Operations.Add(operationId, new StoredOperation(fingerprint, receipt));
            return receipt;
        });
    }

    private static ContainerRecord Get(LedgerState state, string id) =>
        state.Containers.TryGetValue(id, out var record) ? record : throw new LedgerRejectedException("Unknown container.");
    private static ContainerView View(ContainerRecord record) => new(record.Spec, record.Revision,
        new ReadOnlyDictionary<string, long>(new Dictionary<string, long>(record.Quantities, StringComparer.Ordinal)));
    private static void Require(string value, bool required = true)
    {
        if (required && string.IsNullOrWhiteSpace(value)) throw new LedgerRejectedException("Required identifier or context is missing.");
    }
    private static void ValidateItems(IReadOnlyList<ItemQuantity> items)
    {
        foreach (var item in items)
            if (item.Quantity <= 0 || string.IsNullOrWhiteSpace(item.DefinitionId) || !item.DefinitionId.StartsWith("item/", StringComparison.Ordinal))
                throw new LedgerRejectedException("Item quantities must be positive and use item/ definition IDs.");
        if (items.Select(x => x.DefinitionId).Distinct(StringComparer.Ordinal).Count() != items.Count)
            throw new LedgerRejectedException("Duplicate item definition in one operation.");
    }
    private static void ValidateSpec(ContainerSpec spec)
    {
        Require(spec.Id); Require(spec.OwnerId);
        if (!spec.Id.StartsWith("ctn_", StringComparison.Ordinal) || spec.Capacity is < 1 or > 10000)
            throw new LedgerRejectedException("Container ID or bounded capacity is invalid.");
        var prefix = spec.OwnerKind switch { OwnerKind.Character => "chr_", OwnerKind.Account => "acc_", OwnerKind.Guild => "gld_", _ => "sys_" };
        if (!spec.OwnerId.StartsWith(prefix, StringComparison.Ordinal)) throw new LedgerRejectedException("Owner ID does not match owner kind.");
    }
}
