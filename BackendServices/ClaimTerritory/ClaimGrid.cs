using System.Text.Json;

namespace ClaimTerritory;

public enum ClaimBand { Near, Middle, Wilderness }
public readonly record struct ClaimCell(int X, int Y);
public sealed record PricedCell(ClaimCell Cell, ClaimBand Band, decimal Points);
public sealed record GridClaim(string Id, string Owner, ClaimBand Band, double AnchorBaseZ,
    int BandHeightMetres, IReadOnlyList<PricedCell> Cells, long Revision, DateTimeOffset? EmptySince);
public sealed record GridReceipt(bool Accepted, string Reason, long Revision, GridClaim? Claim = null);

// One realm-scoped economy tier 3 snapshot. Rules and prices are shipped; only these bounded
// values can be published through the shared configuration workflow.
public sealed record ClaimGridSettings(
    int PointsBase = 400, int PointsPerLevel = 40, int AlphaFixedPoints = 1200,
    int MaxClaims = 3, int NearMetres = 150, int MiddleMetres = 500,
    decimal NearCost = 2m, decimal MiddleCost = 1m, decimal WildernessCost = .75m,
    int NearCap = 400, int MiddleCap = 1600, int WildernessCap = 3600,
    int NearHeight = 20, int MiddleHeight = 30, int WildernessHeight = 60,
    int ReachMetres = 4, int SpacingMetres = 5, int RoadClearMetres = 6,
    int SpawnClearMetres = 25, int BorderClearMetres = 16, int EmptyClaimHours = 24,
    bool UpkeepEnabled = false)
{
    public const string Prefix = "tune.building.claim.";
    public const string Permission = "config-service economy tier 3";
    public void Validate()
    {
        if (PointsBase is < 100 or > 2000 || PointsPerLevel is < 0 or > 200 ||
            AlphaFixedPoints is < 0 or > 4000 || MaxClaims is < 1 or > 5 ||
            PointsBase % 50 != 0 || PointsPerLevel % 10 != 0 || AlphaFixedPoints % 100 != 0 ||
            NearMetres is < 50 or > 400 || MiddleMetres is < 200 or > 1500 || NearMetres >= MiddleMetres ||
            NearCost is < .1m or > 10m || MiddleCost is < .1m or > 10m || WildernessCost is < .1m or > 10m ||
            NearCost < MiddleCost || MiddleCost < WildernessCost ||
            NearCost * 20 % 1 != 0 || MiddleCost * 20 % 1 != 0 || WildernessCost * 20 % 1 != 0 ||
            NearCap is < 100 or > 1600 || MiddleCap is < 400 or > 10000 || WildernessCap is < 1600 or > 10000 ||
            NearCap > MiddleCap || MiddleCap > WildernessCap ||
            NearCap % 100 != 0 || MiddleCap % 100 != 0 || WildernessCap % 100 != 0 ||
            NearHeight is < 10 or > 60 || MiddleHeight is < 10 or > 60 || WildernessHeight is < 10 or > 60 ||
            NearHeight > MiddleHeight || MiddleHeight > WildernessHeight ||
            NearHeight % 5 != 0 || MiddleHeight % 5 != 0 || WildernessHeight % 5 != 0 ||
            ReachMetres is < 2 or > 8 || SpacingMetres is < 5 or > 20 ||
            RoadClearMetres is < 2 or > 50 || SpawnClearMetres is < 2 or > 50 ||
            BorderClearMetres is < 2 or > 50 || EmptyClaimHours is < 1 or > 168)
            throw new ArgumentOutOfRangeException(nameof(ClaimGridSettings));
    }
    public int Budget(int level) => AlphaFixedPoints > 0 ? AlphaFixedPoints : checked(PointsBase + PointsPerLevel * Math.Clamp(level, 1, 100));
    public decimal Cost(ClaimBand band) => band switch { ClaimBand.Near => NearCost, ClaimBand.Middle => MiddleCost, _ => WildernessCost };
    public int Cap(ClaimBand band) => band switch { ClaimBand.Near => NearCap, ClaimBand.Middle => MiddleCap, _ => WildernessCap };
    public int Height(ClaimBand band) => band switch { ClaimBand.Near => NearHeight, ClaimBand.Middle => MiddleHeight, _ => WildernessHeight };
}

// Derived from authored content. All distances use the centre of a one-metre cell.
public sealed class ClaimBandMap
{
    private readonly List<Point[]> towns = [];
    private readonly List<Point[]> protectedAreas = [];
    private readonly List<(Point Position, int Clearance)> spawns = [];
    private readonly List<Point[]> roads = [];
    private readonly double minX, minY, maxX, maxY;
    private readonly ClaimGridSettings settings;
    private ClaimBandMap(JsonElement zone, JsonElement layout, ClaimGridSettings settings)
    {
        this.settings = settings;
        var extent = zone.GetProperty("extent");
        minX = extent[0][0].GetDouble(); minY = extent[0][1].GetDouble();
        maxX = extent[1][0].GetDouble(); maxY = extent[1][1].GetDouble();
        foreach (var area in zone.GetProperty("areas").EnumerateArray())
        {
            if (area.GetProperty("safety").GetString() != "protected") continue;
            var polygon = area.GetProperty("polygon").EnumerateArray().Select(Parse).ToArray();
            protectedAreas.Add(polygon);
            if (area.TryGetProperty("purpose", out var purpose) && purpose.GetString() == "settlement") towns.Add(polygon);
        }
        if (zone.TryGetProperty("respawn_points", out var points))
            spawns.AddRange(points.EnumerateArray().Select(p => (Parse(p.GetProperty("position")), settings.SpawnClearMetres)));
        if (zone.TryGetProperty("default_spawn", out var spawn)) spawns.Add((Parse(spawn.GetProperty("position")), 50));
        if (layout.TryGetProperty("roads", out var roadRoot))
            foreach (var category in new[] { "primary", "secondary", "trail" })
                if (roadRoot.TryGetProperty(category, out var group) && group.ValueKind == JsonValueKind.Object)
                    foreach (var road in group.EnumerateObject())
                        if (road.Value.ValueKind == JsonValueKind.Array)
                            roads.Add(road.Value.EnumerateArray().Select(Parse).ToArray());
        if (towns.Count == 0) throw new InvalidDataException("No established settlement polygon.");
    }
    public static ClaimBandMap Load(string zonePath, string layoutPath, ClaimGridSettings? settings = null)
    {
        var effective = settings ?? new(); effective.Validate();
        using var zone = JsonDocument.Parse(File.ReadAllText(zonePath));
        using var layout = JsonDocument.Parse(File.ReadAllText(layoutPath));
        return new(zone.RootElement, layout.RootElement, effective);
    }
    private static Point Parse(JsonElement value) => new(value[0].GetDouble(), value[1].GetDouble());
    private static double SegmentDistance(Point p, Point a, Point b)
    {
        var dx = b.X - a.X; var dy = b.Y - a.Y;
        var t = dx * dx + dy * dy == 0 ? 0 : Math.Clamp(((p.X-a.X)*dx+(p.Y-a.Y)*dy)/(dx*dx+dy*dy), 0, 1);
        return Math.Sqrt(Math.Pow(p.X-a.X-t*dx, 2) + Math.Pow(p.Y-a.Y-t*dy, 2));
    }
    private static double Distance(Point p, Point[] path, bool closed)
    {
        var result = double.PositiveInfinity;
        for (var i = 0; i < path.Length - 1; i++) result = Math.Min(result, SegmentDistance(p, path[i], path[i+1]));
        if (closed && path.Length > 2) result = Math.Min(result, SegmentDistance(p, path[^1], path[0]));
        return result;
    }
    private static bool Inside(Point p, Point[] polygon)
    {
        var inside = false;
        for (var i = 0; i < polygon.Length; i++)
        {
            var a = polygon[i]; var b = polygon[(i+1)%polygon.Length];
            if ((a.Y > p.Y) != (b.Y > p.Y) && p.X < (b.X-a.X)*(p.Y-a.Y)/(b.Y-a.Y)+a.X) inside = !inside;
        }
        return inside;
    }
    public ClaimBand Band(ClaimCell cell)
    {
        var p = new Point(cell.X+.5, cell.Y+.5);
        var distance = towns.Min(t => Inside(p, t) ? 0 : Distance(p, t, true));
        return distance <= settings.NearMetres ? ClaimBand.Near : distance <= settings.MiddleMetres ? ClaimBand.Middle : ClaimBand.Wilderness;
    }
    public string BlockReason(ClaimCell cell)
    {
        var p = new Point(cell.X+.5, cell.Y+.5);
        if (p.X < minX + settings.BorderClearMetres || p.X > maxX - settings.BorderClearMetres ||
            p.Y < minY + settings.BorderClearMetres || p.Y > maxY - settings.BorderClearMetres) return "border";
        if (protectedAreas.Any(poly => Inside(p, poly) || Distance(p, poly, true) < settings.SpacingMetres)) return "protected";
        if (spawns.Any(s => SegmentDistance(p, s.Position, s.Position) < s.Clearance)) return "spawn";
        if (roads.Any(r => Distance(p, r, false) < settings.RoadClearMetres)) return "road";
        return "";
    }
}

// This model is deterministic and process-local; the durable adapter must commit a GridReceipt
// in the same Housing decision as the structure and inventory reservation.
public sealed class ClaimGridService(ClaimBandMap map, TimeProvider clock, ClaimGridSettings? initialSettings = null)
{
    private readonly object gate = new();
    private readonly Dictionary<string, GridClaim> claims = new(StringComparer.Ordinal);
    private readonly Dictionary<string, (string Payload, GridReceipt Receipt)> operations = new(StringComparer.Ordinal);
    private ClaimGridSettings settings = Checked(initialSettings ?? new());
    private long revision;
    private static ClaimGridSettings Checked(ClaimGridSettings value) { value.Validate(); return value; }
    public void SetEffectiveSettings(ClaimGridSettings value) { lock (gate) settings = Checked(value); }
    public long Revision { get { lock (gate) return revision; } }
    public IReadOnlyList<GridClaim> Claims { get { lock (gate) { ExpireEmpty(); return claims.Values.ToArray(); } } }
    public decimal Allocated(string owner) { lock (gate) { ExpireEmpty(); return claims.Values.Where(c => c.Owner == owner).Sum(c => c.Cells.Sum(x => x.Points)); } }
    public int Remaining(string owner, int level) => Math.Max(0, settings.Budget(level) - (int)Math.Ceiling(Allocated(owner)));
    private void ExpireEmpty()
    {
        foreach (var claim in claims.Values.Where(c => c.EmptySince is not null &&
            clock.GetUtcNow() - c.EmptySince >= TimeSpan.FromHours(settings.EmptyClaimHours)).ToArray())
        { claims.Remove(claim.Id); revision++; }
    }
    private GridReceipt Execute(string op, string payload, Func<GridReceipt> apply)
    {
        lock (gate)
        {
            if (string.IsNullOrWhiteSpace(op) || op.Length > 128) return new(false, "invalid_request", revision);
            if (operations.TryGetValue(op, out var old)) return old.Payload == payload ? old.Receipt : new(false, "operation_conflict", revision);
            ExpireEmpty();
            var result = apply(); operations.Add(op, (payload, result)); return result;
        }
    }
    public GridReceipt Grow(string op, string id, string owner, int level, IEnumerable<ClaimCell> proposed,
        long expectedRevision, double anchorBaseZ)
    {
        var cells = proposed.Distinct().OrderBy(c => c.X).ThenBy(c => c.Y).ToArray();
        var payload = $"grow|{id}|{owner}|{level}|{expectedRevision}|{anchorBaseZ:R}|{string.Join(';', cells)}";
        return Execute(op, payload, () =>
        {
            if (string.IsNullOrWhiteSpace(id) || !id.StartsWith("clm_", StringComparison.Ordinal) ||
                string.IsNullOrWhiteSpace(owner) || cells.Length is 0 or > 3600 || !double.IsFinite(anchorBaseZ))
                return new(false, "invalid_request", revision);
            claims.TryGetValue(id, out var prior);
            if (prior is null && expectedRevision != revision || prior is not null && prior.Revision != expectedRevision)
                return new(false, "stale_revision", revision);
            if (prior is not null && prior.Owner != owner) return new(false, "not_owner", revision);
            if (prior is null && claims.Values.Count(c => c.Owner == owner) >= settings.MaxClaims)
                return new(false, "claim_slots", revision);
            var existing = prior?.Cells.ToDictionary(c => c.Cell) ?? new Dictionary<ClaimCell, PricedCell>();
            foreach (var cell in cells)
            {
                if (existing.ContainsKey(cell)) continue;
                var blocked = map.BlockReason(cell);
                if (blocked.Length != 0) return new(false, "claim_blocked:" + blocked, revision);
                if (claims.Values.Any(c => c.Id != id && c.Cells.Any(x =>
                    Math.Abs(x.Cell.X-cell.X) <= settings.SpacingMetres && Math.Abs(x.Cell.Y-cell.Y) <= settings.SpacingMetres)))
                    return new(false, "claim_spacing", revision);
                var band = map.Band(cell);
                existing.Add(cell, new(cell, band, settings.Cost(band)));
            }
            var claimBand = prior?.Band ?? map.Band(cells[0]);
            var connected = new HashSet<ClaimCell>(prior?.Cells.Select(c => c.Cell) ?? [cells[0]]);
            var pending = new HashSet<ClaimCell>(cells);
            while (pending.RemoveWhere(c => connected.Any(p =>
                Math.Abs(c.X-p.X) <= settings.ReachMetres && Math.Abs(c.Y-p.Y) <= settings.ReachMetres)) is var added && added > 0)
            {
                foreach (var c in cells.Where(c => !pending.Contains(c))) connected.Add(c);
            }
            if (pending.Count != 0)
                return new(false, "outside_reach", revision);
            if (existing.Count > settings.Cap(claimBand)) return new(false, "claim_cap", revision);
            var allocated = claims.Values.Where(c => c.Owner == owner && c.Id != id).Sum(c => c.Cells.Sum(x => x.Points)) + existing.Values.Sum(c => c.Points);
            if (allocated > settings.Budget(level)) return new(false, "claim_points", revision);
            var next = new GridClaim(id, owner, claimBand, prior?.AnchorBaseZ ?? anchorBaseZ,
                prior?.BandHeightMetres ?? settings.Height(claimBand), existing.Values.ToArray(),
                (prior?.Revision ?? 0)+1, null);
            claims[id] = next; revision++;
            return new(true, "", next.Revision, next);
        });
    }
    public GridReceipt Shrink(string op, string id, string owner, IEnumerable<ClaimCell> retained, long expectedRevision)
    {
        var cells = retained.Distinct().OrderBy(c => c.X).ThenBy(c => c.Y).ToArray();
        return Execute(op, $"shrink|{id}|{owner}|{expectedRevision}|{string.Join(';', cells)}", () =>
        {
            if (!claims.TryGetValue(id, out var prior)) return new(false, "claim_missing", revision);
            if (prior.Revision != expectedRevision) return new(false, "stale_revision", prior.Revision);
            if (prior.Owner != owner) return new(false, "not_owner", prior.Revision);
            var existing = prior.Cells.ToDictionary(c => c.Cell);
            if (cells.Any(c => !existing.ContainsKey(c))) return new(false, "invalid_request", prior.Revision);
            var next = prior with { Cells = cells.Select(c => existing[c]).ToArray(), Revision = prior.Revision+1,
                EmptySince = cells.Length == 0 ? clock.GetUtcNow() : null };
            claims[id] = next; revision++;
            return new(true, "", next.Revision, next);
        });
    }
    public bool Contains(string id, ClaimCell cell, double z)
    {
        lock (gate) return claims.TryGetValue(id, out var claim) &&
            z >= claim.AnchorBaseZ - 8 && z <= claim.AnchorBaseZ + claim.BandHeightMetres &&
            claim.Cells.Any(c => c.Cell == cell);
    }
}
