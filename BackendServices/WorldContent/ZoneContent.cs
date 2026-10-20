using System.Text.Json;

namespace ShatteredFrontier.WorldContent;

public sealed class ZoneContent
{
    public string Id { get; set; } = "";
    public int ZoneIndex { get; set; }
    public double[] Origin { get; set; } = [];
    public double[][] Extent { get; set; } = [];
    public int[] LevelRange { get; set; } = [];
    public SpawnPoint? DefaultSpawn { get; set; }
    public List<SpawnPoint> RespawnPoints { get; set; } = [];
    public List<LevelArea> LevelAreas { get; set; } = [];
    public List<ZoneArea> Areas { get; set; } = [];
    public List<SpawnGroup> SpawnGroups { get; set; } = [];
}

public sealed class SpawnPoint
{
    public string PointId { get; set; } = "";
    public double[] Position { get; set; } = [];
    public double Facing { get; set; }
}

public sealed class SpawnDefinition
{
    public string DefinitionId { get; set; } = "";
    public int Weight { get; set; }
}

public sealed class SpawnGroup
{
    public string GroupId { get; set; } = "";
    public string SpawnKind { get; set; } = "";
    public List<SpawnDefinition> Definitions { get; set; } = [];
    public List<SpawnPoint> Points { get; set; } = [];
    public int ActiveCount { get; set; }
    public double[][] Area { get; set; } = [];
    public string PhaseFilter { get; set; } = "";
    public string LayerPolicy { get; set; } = "";
    public int? RespawnSOverride { get; set; }
}

public sealed class LevelArea
{
    public string AreaId { get; set; } = "";
    public double[][] Polygon { get; set; } = [];
    public int ContentLevel { get; set; }
}

public sealed class ZoneArea
{
    public string AreaId { get; set; } = "";
    public string Safety { get; set; } = "";
    public double[][] Polygon { get; set; } = [];
    public List<double[][]> Polygons { get; set; } = [];
}

public static class ZoneLoader
{
    public static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower,
        PropertyNameCaseInsensitive = true
    };

    public static ZoneContent Load(string path) =>
        JsonSerializer.Deserialize<ZoneContent>(File.ReadAllText(path), JsonOptions)
        ?? throw new JsonException($"Empty zone: {path}");

    public static IReadOnlyList<(string Path, ZoneContent Zone)> LoadDirectory(string directory) =>
        Directory.GetFiles(directory, "*.json", SearchOption.TopDirectoryOnly)
            .OrderBy(path => path, StringComparer.Ordinal)
            .Select(path => (path, Load(path))).ToArray();
}
