using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace ShatteredFrontier.Rewards;

// Creature classification is content, never inferred from a loot table's name.
public sealed record CreatureLootRule([property: JsonRequired] string CreatureId,
    [property: JsonRequired] bool UniqueOrBoss, [property: JsonRequired] string TableId);
public sealed record LootItemLine(string DefinitionId, long Quantity);

public static class CreatureLootCatalog
{
    private static readonly JsonSerializerOptions Options = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower,
        UnmappedMemberHandling = JsonUnmappedMemberHandling.Disallow
    };

    public static IReadOnlyDictionary<string, CreatureLootRule> LoadFiles(IEnumerable<string> files,
        IReadOnlyDictionary<string, LootTable> tables)
    {
        var rules = new Dictionary<string, CreatureLootRule>(StringComparer.Ordinal);
        foreach (var file in files.OrderBy(x => x, StringComparer.Ordinal))
        {
            CreatureLootRule rule;
            try { rule = JsonSerializer.Deserialize<CreatureLootRule>(File.ReadAllText(file), Options)
                ?? throw new RewardContentException($"Empty creature loot rule: {file}"); }
            catch (JsonException ex) { throw new RewardContentException($"Invalid creature loot rule {file}: {ex.Message}"); }
            if (string.IsNullOrWhiteSpace(rule.CreatureId) ||
                !rule.CreatureId.StartsWith("npc/", StringComparison.Ordinal) ||
                string.IsNullOrWhiteSpace(rule.TableId) || !tables.TryGetValue(rule.TableId, out var table) ||
                rule.UniqueOrBoss != table.SingleDrop ||
                !rules.TryAdd(rule.CreatureId, rule))
                throw new RewardContentException($"Invalid or duplicate creature loot rule: {file}");
        }
        return rules;
    }

    public static IReadOnlyList<LootItemLine> RollShared(string defeatId, LootTable table)
    {
        LootCatalog.Validate(table);
        if (!table.SingleDrop) throw new RewardContentException("Ordinary tables require personal rolls.");
        if (string.IsNullOrWhiteSpace(defeatId)) throw new ArgumentException("Defeat ID required.");
        var seed = SHA256.HashData(Encoding.UTF8.GetBytes($"lsf-shared-loot/v1|{defeatId.Length}:{defeatId}|{table.Id}|{table.Version}"));
        var choice = (long)(BinaryPrimitives.ReadUInt64BigEndian(seed) % (ulong)table.Entries.Sum(e => (long)e.Weight!.Value));
        var lines = table.Guaranteed.Select(e => new LootItemLine(e.DefinitionId!, e.Quantity!.Min)).ToList();
        foreach (var entry in table.Entries)
        {
            choice -= entry.Weight!.Value;
            if (choice >= 0) continue;
            if (entry.Kind == "item") lines.Add(new LootItemLine(entry.DefinitionId!, entry.Quantity!.Min));
            break;
        }
        return lines.GroupBy(e => e.DefinitionId, StringComparer.Ordinal)
            .Select(g => new LootItemLine(g.Key, g.Sum(e => e.Quantity))).ToArray();
    }

    public static IReadOnlyList<LootItemLine> RollPersonal(string defeatId, LootTable table)
    {
        LootCatalog.Validate(table);
        if (table.SingleDrop) throw new RewardContentException("Single-drop tables require a shared claim.");
        if (string.IsNullOrWhiteSpace(defeatId)) throw new ArgumentException("Defeat ID required.");
        if (table.Id == "loot/greenlands_silverbrook_boar")
            return [new LootItemLine("item/boar_hide", BoarRewardService.Roll(defeatId, table))];
        // The personal stream also includes the table ID and version.
        var seed = SHA256.HashData(Encoding.UTF8.GetBytes($"lsf-personal-loot/v1|{defeatId.Length}:{defeatId}|{table.Id}|{table.Version}"));
        var choice = (long)(BinaryPrimitives.ReadUInt64BigEndian(seed) % (ulong)table.Entries.Sum(e => (long)e.Weight!.Value));
        var lines = table.Guaranteed.Select(e => new LootItemLine(e.DefinitionId!, e.Quantity!.Min)).ToList();
        foreach (var entry in table.Entries)
        {
            choice -= entry.Weight!.Value;
            if (choice >= 0) continue;
            if (entry.Kind == "item") lines.Add(new LootItemLine(entry.DefinitionId!, entry.Quantity!.Min));
            break;
        }
        return lines.GroupBy(e => e.DefinitionId, StringComparer.Ordinal)
            .Select(g => new LootItemLine(g.Key, g.Sum(e => e.Quantity))).ToArray();
    }
}
