using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using ShatteredFrontier.InventoryLedger;

namespace ShatteredFrontier.Rewards;

public sealed class RewardContentException(string message) : Exception(message);
public sealed record CountRange(int Min, int Max);
public sealed record LootEntry(string Kind, string? DefinitionId, CountRange? Quantity, int? Weight);
public sealed record LootTable(string Id, int Version, CountRange Rolls, string Mode,
    IReadOnlyList<LootEntry> Guaranteed, IReadOnlyList<LootEntry> Entries);

public static class LootCatalog
{
    private static readonly JsonSerializerOptions Options = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower,
        UnmappedMemberHandling = JsonUnmappedMemberHandling.Disallow,
        PropertyNameCaseInsensitive = false
    };

    public static IReadOnlyDictionary<string, LootTable> LoadFiles(IEnumerable<string> files)
    {
        var tables = new Dictionary<string, LootTable>(StringComparer.Ordinal);
        foreach (var file in files.OrderBy(x => x, StringComparer.Ordinal))
        {
            LootTable table;
            try { table = JsonSerializer.Deserialize<LootTable>(File.ReadAllText(file), Options)
                ?? throw new RewardContentException($"Empty loot table: {file}"); }
            catch (JsonException ex) { throw new RewardContentException($"Invalid loot JSON {file}: {ex.Message}"); }
            Validate(table);
            if (!tables.TryAdd(table.Id, table)) throw new RewardContentException($"Duplicate loot table: {table.Id}");
        }
        if (tables.Count == 0) throw new RewardContentException("No loot tables found.");
        return tables;
    }

    public static void Validate(LootTable table)
    {
        if (string.IsNullOrWhiteSpace(table.Id) || !table.Id.StartsWith("loot/", StringComparison.Ordinal) ||
            table.Version < 1 || table.Mode != "weighted_one" || table.Rolls is null ||
            table.Rolls.Min != 1 || table.Rolls.Max != 1 || table.Entries is null || table.Guaranteed is null ||
            table.Entries.Count == 0 || table.Entries.Count > 100 || table.Guaranteed.Count > 100)
            throw new RewardContentException("Unsupported or invalid MVP loot table.");
        foreach (var entry in table.Guaranteed)
            ValidateEntry(entry, false);
        foreach (var entry in table.Entries)
            ValidateEntry(entry, true);
        if (table.Entries.Sum(e => (long)e.Weight!.Value) > int.MaxValue)
            throw new RewardContentException("Loot weights exceed supported total.");
    }

    private static void ValidateEntry(LootEntry entry, bool weighted)
    {
        if (entry is null || (weighted ? entry.Weight is null or <= 0 : entry.Weight is not null))
            throw new RewardContentException("Invalid loot weight.");
        if (entry.Kind == "nothing" && weighted && entry.DefinitionId is null && entry.Quantity is null) return;
        if (entry.Kind != "item" || entry.DefinitionId is null ||
            !entry.DefinitionId.StartsWith("item/", StringComparison.Ordinal) ||
            entry.Quantity is null || entry.Quantity.Min < 1 || entry.Quantity.Max != entry.Quantity.Min || entry.Quantity.Max > 100)
            throw new RewardContentException("Invalid or unsupported loot entry.");
    }
}

public sealed record DefeatEvent(string Id, string KillerCharacterId, string RecipientCharacterId,
    string ContainerId, long ExpectedRevision, bool KillerInProtectedTown, bool BoarInProtectedTown);
public enum RewardDecision { Granted, Ineligible }
public sealed record RewardOutcome(RewardDecision Decision, LedgerReceipt? Receipt, string SourceCode);

public static class BoarEligibility
{
    public static bool CanGrant(DefeatEvent defeat) =>
        !string.IsNullOrWhiteSpace(defeat.KillerCharacterId) &&
        !string.IsNullOrWhiteSpace(defeat.RecipientCharacterId) &&
        !defeat.KillerInProtectedTown && !defeat.BoarInProtectedTown;
}

public sealed class BoarRewardService
{
    private readonly ShatteredFrontier.InventoryLedger.InventoryLedger ledger;
    private readonly LootTable table;
    private readonly object gate = new();
    private readonly Dictionary<(string EventId, string RecipientId), (DefeatEvent Event, RewardOutcome Outcome)> decisions = new();
    public const string SourceCode = "loot_drop";

    public BoarRewardService(ShatteredFrontier.InventoryLedger.InventoryLedger ledger, LootTable table)
    {
        ValidateBoarTable(table);
        this.ledger = ledger;
        this.table = table;
    }

    public static void ValidateBoarTable(LootTable table)
    {
        LootCatalog.Validate(table);
        if (table.Id != "loot/greenlands_silverbrook_boar" ||
            table.Guaranteed.Any(e => e.DefinitionId != "item/boar_hide") ||
            table.Entries.Any(e => e.Kind == "item" && e.DefinitionId is not ("item/boar_hide" or "item/recipe_scroll_iron_brackets")) ||
            table.Guaranteed.Sum(e => e.Quantity!.Min) != 2)
            throw new RewardContentException("Boar reward table must ship two guaranteed boar hides.");
    }

    public RewardOutcome Grant(DefeatEvent defeat)
    {
        if (string.IsNullOrWhiteSpace(defeat.Id) || string.IsNullOrWhiteSpace(defeat.ContainerId) || defeat.ExpectedRevision < 0)
            throw new ArgumentException("Defeat identity, container and revision are required.");
        lock (gate)
        {
            var key = (defeat.Id, defeat.RecipientCharacterId);
            if (decisions.TryGetValue(key, out var prior))
            {
                if (prior.Event != defeat) throw new InvalidOperationException("Conflicting defeat event ID reuse.");
                return prior.Outcome;
            }
            RewardOutcome outcome;
            if (!BoarEligibility.CanGrant(defeat)) outcome = new(RewardDecision.Ineligible, null, SourceCode);
            else
            {
                var quantity = Roll(defeat.Id + ":" + defeat.RecipientCharacterId, table);
                if (quantity == 0) throw new RewardContentException("Boar table must grant an item.");
                var operationId = $"defeat:{defeat.Id.Length}:{defeat.Id}:{defeat.RecipientCharacterId}";
                var receipt = ledger.Grant(operationId, defeat.ContainerId, defeat.ExpectedRevision,
                    "item/boar_hide", quantity, SourceCode, defeat.Id);
                outcome = new(RewardDecision.Granted, receipt, SourceCode);
            }
            decisions.Add(key, (defeat, outcome));
            return outcome;
        }
    }

    public static int Roll(string defeatId, LootTable table)
    {
        LootCatalog.Validate(table);
        if (string.IsNullOrWhiteSpace(defeatId)) throw new ArgumentException("Defeat ID required.");
        // MVP SHA-256 stream; a production roll key and audited roll store remain separate work.
        var seed = SHA256.HashData(Encoding.UTF8.GetBytes($"lsf-boar-loot/v1|{defeatId.Length}:{defeatId}|{table.Id}|{table.Version}"));
        var draw = BinaryPrimitives.ReadUInt64BigEndian(seed);
        var total = table.Entries.Sum(e => (long)e.Weight!.Value);
        var choice = (long)(draw % (ulong)total);
        var count = table.Guaranteed.Sum(e => e.Quantity!.Min);
        foreach (var entry in table.Entries)
        {
            choice -= entry.Weight!.Value;
            if (choice >= 0) continue;
            if (entry.Kind == "item") count = checked(count + entry.Quantity!.Min);
            break;
        }
        return count;
    }
}
