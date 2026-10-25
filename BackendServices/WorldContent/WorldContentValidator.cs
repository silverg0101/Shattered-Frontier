namespace ShatteredFrontier.WorldContent;

public static class WorldContentValidator
{
    public static IReadOnlyList<string> ValidateAll(IEnumerable<(string Path, ZoneContent Zone)> files)
    {
        var issues = new List<string>();
        var indices = new Dictionary<int, string>();
        var ids = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var (path, zone) in files)
        {
            issues.AddRange(Validate(zone).Select(issue => $"{path}: {issue}"));
            if (indices.TryGetValue(zone.ZoneIndex, out var other))
                issues.Add($"{path}: zone_index {zone.ZoneIndex} duplicates {other}");
            else indices[zone.ZoneIndex] = path;
            if (ids.TryGetValue(zone.Id, out other))
                issues.Add($"{path}: zone id {zone.Id} duplicates {other}");
            else ids[zone.Id] = path;
        }
        return issues;
    }

    public static IReadOnlyList<string> Validate(ZoneContent zone)
    {
        var issues = new List<string>();
        if (!zone.Id.StartsWith("zone/", StringComparison.Ordinal) || zone.ZoneIndex < 1)
            issues.Add("invalid zone id or zone_index");
        if (!ValidPoint(zone.Origin) || zone.Extent.Length != 2 || zone.Extent.Any(p => !ValidPoint(p)) ||
            (zone.Extent.Length == 2 && (zone.Extent[0][0] >= zone.Extent[1][0] || zone.Extent[0][1] >= zone.Extent[1][1])))
            issues.Add("invalid origin or extent");
        if (zone.LevelRange.Length != 2 || zone.LevelRange[0] < 1 || zone.LevelRange[0] > zone.LevelRange[1])
            issues.Add("invalid level_range");
        if (zone.DefaultSpawn is null || !zone.RespawnPoints.Any(p => p.PointId == zone.DefaultSpawn.PointId && SamePosition(p.Position, zone.DefaultSpawn.Position)))
            issues.Add("default_spawn must match a respawn point");
        Unique(zone.RespawnPoints.Select(p => p.PointId), "respawn point", issues);
        foreach (var point in zone.RespawnPoints)
            if (!InExtent(point.Position, zone.Extent)) issues.Add($"respawn point {point.PointId} outside extent");
        Unique(zone.LevelAreas.Select(a => a.AreaId), "level area", issues);
        foreach (var area in zone.LevelAreas)
        {
            if (!ValidPolygon(area.Polygon) || area.Polygon.Any(p => !InExtent(p, zone.Extent))) issues.Add($"level area {area.AreaId} invalid polygon or outside extent");
            if (zone.LevelRange.Length == 2 && (area.ContentLevel < zone.LevelRange[0] || area.ContentLevel > zone.LevelRange[1]))
                issues.Add($"level area {area.AreaId} content level outside level_range");
        }
        Unique(zone.Areas.Select(a => a.AreaId), "area", issues);
        foreach (var area in zone.Areas)
        {
            if (area.Polygon.Length > 0 && !ValidPolygon(area.Polygon)) issues.Add($"area {area.AreaId} invalid polygon");
            if (area.Polygons.Any(p => !ValidPolygon(p))) issues.Add($"area {area.AreaId} invalid polygons");
        }
        Unique(zone.SpawnGroups.Select(g => g.GroupId), "spawn group", issues);
        foreach (var group in zone.SpawnGroups)
        {
            var label = $"group {group.GroupId}";
            if (group.SpawnKind == "npc" && (!NpcToken(group.GroupId) || group.Points.Any(p => !NpcToken(p.PointId))))
                issues.Add($"{label} invalid NPC group or point token");
            if (group.SpawnKind is not ("gathering_node" or "npc")) issues.Add($"{label} invalid spawn_kind");
            if (group.PhaseFilter is not ("any" or "day" or "night")) issues.Add($"{label} invalid phase_filter");
            if (group.LayerPolicy is not ("all_layers" or "primary_only")) issues.Add($"{label} invalid layer_policy");
            if (group.ActiveCount < 1 || group.Points.Count < group.ActiveCount) issues.Add($"{label} invalid active_count or point count");
            if (group.SpawnKind == "gathering_node" && group.Points.Count < 2 * group.ActiveCount)
                issues.Add($"{label} gathering groups need N >= 2K");
            if (group.Definitions.Count == 0 || group.Definitions.Any(d => d.Weight <= 0 || !d.DefinitionId.StartsWith(group.SpawnKind + "/", StringComparison.Ordinal)))
                issues.Add($"{label} invalid definitions or weights");
            Unique(group.Definitions.Select(d => d.DefinitionId), $"{label} definition", issues);
            foreach (var definition in group.Definitions)
            {
                if (definition.GrowthProfile is { } profile)
                {
                    if (group.SpawnKind != "gathering_node" || !ValidGrowthProfile(profile))
                        issues.Add($"{label} definition {definition.DefinitionId} invalid growth_profile");
                }
            }
            Unique(group.Points.Select(p => p.PointId), $"{label} point", issues);
            if (!ValidPolygon(group.Area)) issues.Add($"{label} invalid population area");
            if (group.RespawnSOverride is <= 0) issues.Add($"{label} invalid respawn_s_override");
            foreach (var point in group.Points)
            {
                if (!InExtent(point.Position, zone.Extent))
                    issues.Add($"{label} point {point.PointId} outside extent");
                if (!zone.LevelAreas.Any(a => Contains(a.Polygon, point.Position)))
                    issues.Add($"{label} point {point.PointId} has no level area");
                if (group.SpawnKind == "npc" && IsProtected(zone, point.Position))
                    issues.Add($"{label} hostile point {point.PointId} inside protected area");
            }
            if (ValidPolygon(group.Area) && !zone.LevelAreas.Any(a => group.Area.All(p => Contains(a.Polygon, p))))
                issues.Add($"{label} population area has no containing level area");
        }
        return issues;
    }

    public static bool IsProtected(ZoneContent zone, double[] point) =>
        zone.Areas.Any(a => a.Safety == "protected" &&
            (Contains(a.Polygon, point) || a.Polygons.Any(poly => Contains(poly, point))));

    public static bool Contains(double[][] polygon, double[] point)
    {
        if (!ValidPolygon(polygon) || !ValidPoint(point)) return false;
        var inside = false;
        for (var i = 0; i < polygon.Length; i++)
        {
            var a = polygon[i]; var b = polygon[(i + 1) % polygon.Length];
            var cross = (point[0] - a[0]) * (b[1] - a[1]) - (point[1] - a[1]) * (b[0] - a[0]);
            if (Math.Abs(cross) < 1e-7 && point[0] >= Math.Min(a[0], b[0]) - 1e-7 && point[0] <= Math.Max(a[0], b[0]) + 1e-7 &&
                point[1] >= Math.Min(a[1], b[1]) - 1e-7 && point[1] <= Math.Max(a[1], b[1]) + 1e-7) return true;
            if ((a[1] > point[1]) != (b[1] > point[1]) && point[0] < (b[0] - a[0]) * (point[1] - a[1]) / (b[1] - a[1]) + a[0]) inside = !inside;
        }
        return inside;
    }

    private static bool NpcToken(string value) => value.Length > 0 &&
        value.All(c => c is >= 'a' and <= 'z' or >= '0' and <= '9' or '_');
    private static bool ValidPoint(double[] p) => p.Length >= 2 && p.Take(2).All(double.IsFinite);
    public static bool ValidGrowthProfile(GrowthProfile profile)
    {
        if (!double.IsFinite(profile.GrowFraction) || profile.GrowFraction is <= 0 or >= 1 ||
            profile.Stages is null || profile.Stages.Count is < 2 or > 4) return false;
        var start = 0.0;
        var previousYield = 0.0;
        for (var i = 0; i < profile.Stages.Count; i++)
        {
            var stage = profile.Stages[i];
            if (stage is null || string.IsNullOrWhiteSpace(stage.Name) ||
                !double.IsFinite(stage.DurationFraction) || !double.IsFinite(stage.YieldFraction) ||
                stage.DurationFraction < 0 || (i < profile.Stages.Count - 1 && stage.DurationFraction <= 0) ||
                (i == profile.Stages.Count - 1 && stage.DurationFraction != 0) ||
                stage.YieldFraction <= previousYield || stage.YieldFraction > 1 ||
                stage.YieldFraction > 1 - profile.GrowFraction + start * profile.GrowFraction + 1e-9)
                return false;
            previousYield = stage.YieldFraction;
            start += stage.DurationFraction;
        }
        return Math.Abs(start - 1) <= 1e-9 && previousYield == 1;
    }
    private static bool ValidPolygon(double[][] p) => p.Length >= 3 && p.All(ValidPoint);
    private static bool InExtent(double[] p, double[][] e) => ValidPoint(p) && e.Length == 2 && e.All(ValidPoint) &&
        p[0] >= e[0][0] && p[0] <= e[1][0] && p[1] >= e[0][1] && p[1] <= e[1][1];
    private static bool SamePosition(double[] a, double[] b) => ValidPoint(a) && ValidPoint(b) && a[0] == b[0] && a[1] == b[1];
    private static void Unique(IEnumerable<string> ids, string label, List<string> issues)
    {
        var seen = new HashSet<string>(StringComparer.Ordinal);
        foreach (var id in ids)
            if (string.IsNullOrWhiteSpace(id) || !seen.Add(id)) issues.Add($"duplicate or empty {label} id {id}");
    }
}
