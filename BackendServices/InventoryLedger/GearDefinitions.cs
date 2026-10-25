using System.Text.Json;

namespace ShatteredFrontier.InventoryLedger;

public static class GearDefinitions
{
    private static readonly HashSet<string> Ids = Load();

    public static bool IsGear(string definitionId)
    {
        var slash = definitionId.IndexOf('/', "item/".Length);
        return Ids.Contains(slash < 0 ? definitionId : definitionId[..slash]);
    }

    private static HashSet<string> Load()
    {
        using var stream = typeof(GearDefinitions).Assembly.GetManifestResourceStream("GearDefinitions.json")
            ?? throw new InvalidOperationException("Gear definitions are missing from the ledger build.");
        using var document = JsonDocument.Parse(stream);
        return document.RootElement.GetProperty("gear").EnumerateArray()
            .Select(x => x.GetProperty("definition_id").GetString()!)
            .ToHashSet(StringComparer.Ordinal);
    }
}
