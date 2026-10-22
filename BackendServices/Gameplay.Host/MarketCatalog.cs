using System.Text.Json;

internal sealed record MarketGood(string DefinitionId, long FairValueCopper, bool Grades = false);
internal sealed record MarketDefinition(string Id, string ZoneId, string Board, bool Open, IReadOnlyList<MarketGood> Items,
    IReadOnlyDictionary<string, decimal> GradeMultipliers);

internal sealed class MarketCatalog
{
    private static readonly IReadOnlyDictionary<string, decimal> RequiredGradeMultipliers = new Dictionary<string, decimal>(StringComparer.Ordinal)
    { ["crude"] = .5m, ["fine"] = 1.5m, ["superior"] = 2.5m, ["masterwork"] = 4m };
    private readonly IReadOnlyDictionary<string, MarketDefinition> markets;
    private MarketCatalog(IReadOnlyDictionary<string, MarketDefinition> markets) => this.markets = markets;

    public static MarketCatalog Load(string directory)
    {
        var entries = new Dictionary<string, MarketDefinition>(StringComparer.Ordinal);
        foreach (var file in Directory.GetFiles(directory, "*.json").OrderBy(x => x, StringComparer.Ordinal))
        {
            using var document = JsonDocument.Parse(File.ReadAllText(file));
            var root = document.RootElement;
            static string Required(JsonElement element, string key) => element.GetProperty(key).GetString() is { Length: > 0 } value ? value : throw new FormatException($"Invalid {key}.");
            var id = Required(root, "id");
            var zone = Required(root, "zone_id");
            var board = Required(root, "board");
            if (!id.StartsWith("market/", StringComparison.Ordinal) || !zone.StartsWith("zone/", StringComparison.Ordinal) ||
                !board.StartsWith("market_board/", StringComparison.Ordinal)) throw new FormatException("Invalid market identity.");
            var multipliers = new Dictionary<string, decimal>(StringComparer.Ordinal);
            foreach (var property in root.GetProperty("grade_multipliers").EnumerateObject())
                if (!multipliers.TryAdd(property.Name, property.Value.GetDecimal())) throw new FormatException("Duplicate grade multiplier.");
            if (multipliers.Count != RequiredGradeMultipliers.Count || RequiredGradeMultipliers.Any(x =>
                !multipliers.TryGetValue(x.Key, out var value) || value != x.Value))
                throw new FormatException("Unsupported market grade multipliers.");
            var goods = new List<MarketGood>();
            var seen = new HashSet<string>(StringComparer.Ordinal);
            foreach (var row in root.GetProperty("items").EnumerateArray())
            {
                var item = Required(row, "definition_id");
                var fair = row.GetProperty("fair_value_copper").GetInt64();
                if (!item.StartsWith("item/", StringComparison.Ordinal) || item.Split('/').Length != 2 || fair is < 1 or > 1_000_000 || !seen.Add(item))
                    throw new FormatException("Invalid or duplicate market good.");
                goods.Add(new(item, fair, row.TryGetProperty("grades", out var grades) && grades.GetBoolean()));
            }
            if (goods.Count == 0 || !entries.TryAdd(id, new(id, zone, board, root.GetProperty("open").GetBoolean(), goods, multipliers)))
                throw new FormatException("Empty or duplicate market.");
        }
        if (entries.Count == 0) throw new FormatException("No market content.");
        return new(entries);
    }

    public bool HasOpenMarket(string id) => markets.TryGetValue(id, out var market) && market.Open;
    public long? FairValue(string marketId, string itemId)
    {
        if (!HasOpenMarket(marketId)) return null;
        var market = markets[marketId];
        foreach (var good in market.Items)
        {
            if (itemId == good.DefinitionId) return good.FairValueCopper;
            if (good.Grades && itemId.StartsWith(good.DefinitionId + "/", StringComparison.Ordinal) &&
                market.GradeMultipliers.TryGetValue(itemId[(good.DefinitionId.Length + 1)..], out var multiplier))
                return checked((long)decimal.Round(good.FairValueCopper * multiplier, 0, MidpointRounding.AwayFromZero));
        }
        return null;
    }
}
