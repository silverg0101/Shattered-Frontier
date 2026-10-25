using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

namespace ShatteredFrontier.InventoryLedger;

// Versioned metadata lives in the same store transaction as the escrow and its
// receipt. No second item database, deferred delete or grant-after-consume saga.
public sealed record WorldLootSource(string Id, string ZoneId, double X, double Y, double Z,
    string CreatureId, string MeshPath, bool Ground, bool Shared, string[] EligibleCharacters,
    double ScaleX = 1, double ScaleY = 1, double ScaleZ = 1, double Pitch = 0, double Yaw = 0, double Roll = 0);
public sealed record WorldLootView(WorldLootSource Source, ContainerView Container);
public sealed record WorldLootSeed(WorldLootSource Source, IReadOnlyList<ItemQuantity> Items, IReadOnlyList<ItemInstance> Instances);

public sealed class WorldLoot(IInventoryLedgerStore store)
{
    public const string WorldOwner = "sys_world_loot";
    private static string ZoneOwner(string zoneId) => WorldOwner + ":" + zoneId;
    private static string MetadataKey(string id) => "world-loot-source/v1:" + id;
    public static void ReserveDefeatRoute(IInventoryLedgerStore store, string eventId, string route) => store.Execute(state => ReserveRoute(state, eventId, route));
    private static bool ReserveRoute(LedgerState state, string eventId, string route)
    {
        var key = "defeat-delivery-route/v1:" + eventId;
        if (state.Operations.TryGetValue(key, out var prior))
        {
            if (prior.Fingerprint != route) throw new LedgerRejectedException("Defeat already assigned to another delivery route.");
        }
        else state.Operations.Add(key, new(route, new(key, LedgerAction.Grant, [], [])));
        return true;
    }
    public static string SourceId(string eventId, string recipient) => "ctn_loot_" +
        Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(eventId.Length + ":" + eventId + recipient))).ToLowerInvariant();

    public WorldLootView Create(WorldLootSource source, IReadOnlyList<ItemQuantity> items,
        IReadOnlyList<ItemInstance> instances) => store.Execute(state => CreateInState(state, source, items, instances));

    public IReadOnlyList<WorldLootView>? ReplayDefeat(string eventId, string admission) => store.Execute(state => ReplayDefeat(state, eventId, admission));
    private static IReadOnlyList<WorldLootView>? ReplayDefeat(LedgerState state, string eventId, string admission)
    {
        if (!state.Operations.TryGetValue("world-loot-defeat/v1:" + eventId, out var saved)) return null;
        using var doc = JsonDocument.Parse(saved.Fingerprint);
        if (doc.RootElement.GetProperty("Admission").GetString() != admission) throw new LedgerRejectedException("Conflicting defeat snapshot reuse.");
        return doc.RootElement.GetProperty("Sources").Deserialize<string[]>()!.Select(id => View(state, id)).ToArray();
    }
    public IReadOnlyList<WorldLootView> CreateDefeat(string eventId, string admission, IReadOnlyList<WorldLootSeed> sources) => store.Execute(state =>
    {
        if (ReplayDefeat(state, eventId, admission) is { } replay) return replay;
        ReserveRoute(state, eventId, "corpse");
        var result = sources.OrderBy(x => x.Source.Id, StringComparer.Ordinal)
            .Select(x => CreateInState(state, x.Source, x.Items, x.Instances)).ToArray();
        var key = "world-loot-defeat/v1:" + eventId;
        state.Operations.Add(key, new(JsonSerializer.Serialize(new { Admission = admission, Sources = result.Select(x => x.Source.Id).ToArray() }), new(key, LedgerAction.Grant, [], [])));
        return result;
    });

    private static WorldLootView CreateInState(LedgerState state, WorldLootSource source, IReadOnlyList<ItemQuantity> items,
        IReadOnlyList<ItemInstance> instances)
    {
        ValidateSource(source);
        if (source.Ground) throw new LedgerRejectedException("Ground loot must debit an owned backpack.");
        var fingerprint = JsonSerializer.Serialize(new { Source = source, Items = items, Instances = instances });
            var key = MetadataKey(source.Id);
            if (state.Operations.TryGetValue(key, out var saved))
            {
                if (saved.Fingerprint != fingerprint) throw new LedgerRejectedException("Conflicting source reuse.");
                return View(state, source.Id);
            }
            if (state.Containers.TryGetValue(source.Id, out _)) throw new LedgerRejectedException("Source container already exists.");
            var container = new ContainerRecord(new(source.Id, OwnerKind.System, ZoneOwner(source.ZoneId), ContainerKind.Escrow, 10000));
            foreach (var line in items)
            {
                if (!line.DefinitionId.StartsWith("item/", StringComparison.Ordinal) || line.Quantity <= 0)
                    throw new LedgerRejectedException("Invalid loot item.");
                container.Quantities[line.DefinitionId] = checked(container.Quantities.GetValueOrDefault(line.DefinitionId) + line.Quantity);
            }
            if (container.Quantities.Values.Sum() > container.Spec.Capacity) throw new LedgerRejectedException("Loot capacity exceeded.");
            foreach (var instance in instances) container.Instances.Add(instance.Id, instance);
            foreach (var group in instances.GroupBy(x => x.DefinitionId))
                if (group.Count() > container.Quantities.GetValueOrDefault(group.Key)) throw new LedgerRejectedException("Invalid loot instances.");
            state.Containers.Add(source.Id, container);
            container.Revision = 1;
            state.Operations.Add(key, new(fingerprint, new(key, LedgerAction.Grant,
                [new(source.Id, 0, 1)], container.Quantities.Select(x => new QuantityChange(source.Id, x.Key, x.Value)).ToArray(),
                instances.Select(x => x.Id).ToArray())));
            return new WorldLootView(source, Snapshot(container));
    }

    public IReadOnlyList<WorldLootView> List(string zoneId) => store.FindContainers(OwnerKind.System, ZoneOwner(zoneId), ContainerKind.Escrow)
        .OrderBy(x => x.Id, StringComparer.Ordinal).Select(spec => store.Execute(state => View(state, spec.Id)))
        .Where(view => view.Source.ZoneId == zoneId && view.Container.Quantities.Count != 0).ToArray();

    public WorldLootView Open(string id, string characterId) => store.Execute(state =>
    {
        var view = View(state, id); Authorize(view.Source, characterId);
        CheckClaimant(state, id, characterId, false);
        return view;
    });

    public LedgerReceipt Take(string operationId, string id, string actorId, string backpackId,
        long sourceRevision, long backpackRevision, string definitionId, long quantity, string instanceId)
    {
        var fingerprint = JsonSerializer.Serialize(new { id, actorId, backpackId, sourceRevision, backpackRevision, definitionId, quantity, instanceId });
        return store.Execute(state =>
        {
            if (Replay(state, operationId, fingerprint) is { } replay) return replay;
            var source = ReadSource(state, id); Authorize(source, actorId);
            // Locks are taken in the adapter's canonical order, before mutation.
            foreach (var key in new[] { id, backpackId }.OrderBy(x => x, StringComparer.Ordinal)) _ = state.Containers[key];
            var from = state.Containers[id]; var to = OwnedBackpack(state, backpackId, actorId);
            if (from.Revision != sourceRevision || to.Revision != backpackRevision) throw new LedgerRejectedException("Stale container revision.");
            if (source.Shared) CheckClaimant(state, id, actorId, true);
            return Move(state, operationId, fingerprint, from, to, definitionId, quantity, instanceId);
        });
    }

    public WorldLootView Drop(string operationId, WorldLootSource source, string actorId, string backpackId,
        long backpackRevision, string definitionId, long quantity, string instanceId)
    {
        ValidateSource(source);
        if (!source.Ground || source.Shared || source.EligibleCharacters.Length != 0) throw new LedgerRejectedException("Invalid ground source.");
        var fingerprint = JsonSerializer.Serialize(new { source, actorId, backpackId, backpackRevision, definitionId, quantity, instanceId });
        return store.Execute(state =>
        {
            if (Replay(state, operationId, fingerprint) is not null) return View(state, source.Id);
            var metadata = MetadataKey(source.Id);
            if (state.Operations.ContainsKey(metadata)) throw new LedgerRejectedException("Ground source already exists.");
            foreach (var key in new[] { source.Id, backpackId }.OrderBy(x => x, StringComparer.Ordinal)) state.Containers.TryGetValue(key, out _);
            var from = OwnedBackpack(state, backpackId, actorId);
            if (from.Revision != backpackRevision) throw new LedgerRejectedException("Stale container revision.");
            if (state.Containers.TryGetValue(source.Id, out _)) throw new LedgerRejectedException("Ground source already exists.");
            var to = new ContainerRecord(new(source.Id, OwnerKind.System, ZoneOwner(source.ZoneId), ContainerKind.Escrow, 10000));
            state.Containers.Add(source.Id, to);
            Move(state, operationId, fingerprint, from, to, definitionId, quantity, instanceId);
            state.Operations.Add(metadata, new(JsonSerializer.Serialize(new { Source = source }), new(metadata, LedgerAction.Transfer, [], [])));
            return new WorldLootView(source, Snapshot(to));
        });
    }

    private static LedgerReceipt Move(LedgerState state, string operationId, string fingerprint,
        ContainerRecord from, ContainerRecord to, string definitionId, long quantity, string instanceId)
    {
        if (quantity <= 0 || !definitionId.StartsWith("item/", StringComparison.Ordinal) || from.Spec.Id == to.Spec.Id ||
            from.Quantities.GetValueOrDefault(definitionId) < quantity) throw new LedgerRejectedException("Insufficient source quantity.");
        if (checked(to.Quantities.Values.Sum() + quantity) > to.Spec.Capacity) throw new LedgerRejectedException("Backpack full.");
        var copies = from.Instances.Values.Where(x => x.DefinitionId == definitionId).OrderBy(x => x.Id, StringComparer.Ordinal).ToArray();
        ItemInstance[] moving = [];
        if (copies.Length != 0)
        {
            if (instanceId.Length != 0)
            {
                if (quantity != 1 || !from.Instances.TryGetValue(instanceId, out var chosen) || chosen.DefinitionId != definitionId)
                    throw new LedgerRejectedException("Item instance unavailable.");
                moving = [chosen];
            }
            else
            {
                if (copies.LongLength < quantity) throw new LedgerRejectedException("Item instance count mismatch.");
                moving = copies.Take(checked((int)quantity)).ToArray();
            }
        }
        else if (instanceId.Length != 0) throw new LedgerRejectedException("Item instance unavailable.");
        var remaining = from.Quantities[definitionId] - quantity;
        if (remaining == 0) from.Quantities.Remove(definitionId); else from.Quantities[definitionId] = remaining;
        to.Quantities[definitionId] = checked(to.Quantities.GetValueOrDefault(definitionId) + quantity);
        foreach (var instance in moving) { from.Instances.Remove(instance.Id); to.Instances.Add(instance.Id, instance); }
        var receipt = new LedgerReceipt(operationId, LedgerAction.Transfer,
            [new(from.Spec.Id, from.Revision, ++from.Revision), new(to.Spec.Id, to.Revision, ++to.Revision)],
            [new(from.Spec.Id, definitionId, -quantity), new(to.Spec.Id, definitionId, quantity)], moving.Select(x => x.Id).ToArray());
        state.Operations.Add(operationId, new(fingerprint, receipt));
        return receipt;
    }

    private static LedgerReceipt? Replay(LedgerState state, string id, string fingerprint)
    {
        if (string.IsNullOrWhiteSpace(id)) throw new LedgerRejectedException("Operation ID required.");
        if (!state.Operations.TryGetValue(id, out var saved)) return null;
        if (saved.Fingerprint != fingerprint) throw new LedgerRejectedException("Conflicting operation ID reuse.");
        return saved.Receipt;
    }
    private static void CheckClaimant(LedgerState state, string id, string characterId, bool claim)
    {
        var key = "world-loot-claim/v1:" + id;
        if (state.Operations.TryGetValue(key, out var saved))
        { if (saved.Fingerprint != characterId) throw new LedgerRejectedException("Shared source already claimed."); }
        else if (claim) state.Operations.Add(key, new(characterId, new(key, LedgerAction.Transfer, [], [])));
    }
    private static void Authorize(WorldLootSource source, string characterId)
    {
        if (string.IsNullOrWhiteSpace(characterId) || !source.Ground && !source.EligibleCharacters.Contains(characterId, StringComparer.Ordinal))
            throw new LedgerRejectedException("Loot eligibility denied.");
    }
    private static ContainerRecord OwnedBackpack(LedgerState state, string id, string actorId)
    {
        var bag = state.Containers[id];
        if (bag.Spec.OwnerKind != OwnerKind.Character || bag.Spec.OwnerId != actorId || bag.Spec.Kind != ContainerKind.Backpack)
            throw new LedgerRejectedException("Backpack owner mismatch.");
        return bag;
    }
    private static void ValidateSource(WorldLootSource source)
    {
        if (!source.Id.StartsWith("ctn_loot_", StringComparison.Ordinal) || string.IsNullOrWhiteSpace(source.ZoneId) ||
            !double.IsFinite(source.X) || !double.IsFinite(source.Y) || !double.IsFinite(source.Z) ||
            !double.IsFinite(source.ScaleX) || !double.IsFinite(source.ScaleY) || !double.IsFinite(source.ScaleZ) ||
            source.ScaleX == 0 || source.ScaleY == 0 || source.ScaleZ == 0 ||
            !double.IsFinite(source.Pitch) || !double.IsFinite(source.Yaw) || !double.IsFinite(source.Roll) ||
            !source.Ground && source.EligibleCharacters.Length == 0) throw new LedgerRejectedException("Invalid loot source.");
    }
    private static WorldLootSource ReadSource(LedgerState state, string id)
    {
        if (!state.Operations.TryGetValue(MetadataKey(id), out var metadata)) throw new LedgerRejectedException("Loot source missing.");
        using var doc = JsonDocument.Parse(metadata.Fingerprint);
        return doc.RootElement.GetProperty("Source").Deserialize<WorldLootSource>() ?? throw new LedgerRejectedException("Loot metadata invalid.");
    }
    private static WorldLootView View(LedgerState state, string id) => new(ReadSource(state, id), Snapshot(state.Containers[id]));
    private static ContainerView Snapshot(ContainerRecord c) => new(c.Spec, c.Revision,
        new Dictionary<string, long>(c.Quantities), c.Instances.Values.OrderBy(x => x.Id, StringComparer.Ordinal).ToArray());
}
