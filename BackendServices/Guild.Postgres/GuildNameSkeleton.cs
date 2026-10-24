using System.Globalization;
using System.Text;

namespace ShatteredFrontier.Guild.Postgres;

// Shares IdentityGateway's pinned UTS #39 data and folding algorithm. Guild input is ASCII by design.
public static class GuildNameSkeleton
{
    private static readonly Lazy<IReadOnlyDictionary<int, string>> Map = new(Load);
    public static string For(string value)
    {
        var folded = value.Normalize(NormalizationForm.FormKC).ToLowerInvariant()
            .Replace("ß", "ss").Replace('ς', 'σ').Normalize(NormalizationForm.FormD);
        var output = new StringBuilder();
        foreach (var rune in folded.EnumerateRunes())
            output.Append(Map.Value.TryGetValue(rune.Value, out var target) ? target : rune.ToString());
        return output.ToString().Normalize(NormalizationForm.FormD).ToLowerInvariant();
    }
    private static IReadOnlyDictionary<int, string> Load()
    {
        var result = new Dictionary<int, string>();
        using var stream = typeof(GuildNameSkeleton).Assembly.GetManifestResourceStream("Guild.confusables.txt")
            ?? throw new InvalidDataException("Missing pinned Unicode confusables data.");
        using var reader = new StreamReader(stream);
        while (reader.ReadLine() is { } line)
        {
            var data = line.Split('#', 2)[0].Split(';');
            if (data.Length < 2) continue;
            if (!int.TryParse(data[0].Trim(), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out var codePoint)) continue;
            var target = new StringBuilder();
            foreach (var hex in data[1].Split(' ', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
                target.Append(new Rune(int.Parse(hex, NumberStyles.HexNumber, CultureInfo.InvariantCulture)).ToString());
            result[codePoint] = target.ToString();
        }
        if (result.Count < 1000) throw new InvalidDataException("Incomplete Unicode confusables data.");
        return result;
    }
}
