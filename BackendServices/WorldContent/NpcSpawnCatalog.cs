using System.Text.Json;

namespace ShatteredFrontier.WorldContent;

// Release-bound metadata from the same NPC catalogue consumed by Unreal. No species dispatch.
public sealed class NpcSpawnCatalog
{
    private readonly IReadOnlyDictionary<string, string> tiers;
    private NpcSpawnCatalog(IReadOnlyDictionary<string, string> tiers) => this.tiers = tiers;

    public static NpcSpawnCatalog Load(string path)
    {
        using var json = JsonDocument.Parse(File.ReadAllText(path));
        var root = json.RootElement;
        if (root.GetProperty("schema_version").GetInt32() != 3) throw new JsonException("Unsupported NPC schema.");
        var entries = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var row in root.GetProperty("profiles").EnumerateArray())
        {
            var id = row.GetProperty("definition_id").GetString() ?? "";
            var tier = row.GetProperty("respawn_tier").GetString() ?? "";
            if (!id.StartsWith("npc/", StringComparison.Ordinal) || id.Length == 4 ||
                id[4..].Any(c => c is not (>= 'a' and <= 'z') and not (>= '0' and <= '9') and not '_') ||
                tier is not ("common" or "elite" or "mini_boss") || !entries.TryAdd(id, tier))
                throw new JsonException($"Invalid or duplicate NPC spawn profile: {id}");
        }
        if (entries.Count == 0) throw new JsonException("Empty NPC catalogue.");
        return new NpcSpawnCatalog(entries);
    }

    public string Tier(string? definitionId) => definitionId is not null && tiers.TryGetValue(definitionId, out var tier)
        ? tier : throw new ArgumentException($"Unknown NPC profile: {definitionId}");

    public IReadOnlyDictionary<string, SpawnGroup> Groups(ZoneContent zone)
    {
        var groups = zone.SpawnGroups.Where(g => g.SpawnKind == "npc").ToDictionary(g => g.GroupId, StringComparer.Ordinal);
        foreach (var group in groups.Values)
        foreach (var definition in group.Definitions) _ = Tier(definition.DefinitionId);
        return groups;
    }
}
