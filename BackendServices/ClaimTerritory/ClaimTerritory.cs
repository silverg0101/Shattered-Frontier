using System.Text.Json;

namespace ClaimTerritory;

public enum ClaimState { Active, Grace, Delinquent, Notice, Archived, Reclaimed }
[Flags] public enum ClaimRight { Build = 1, OpenDoors = 2, UseContainers = 4 }
public enum Denial { None, InvalidRequest, OperationConflict, StaleRevision, EntitlementUsed, OutsideZone, ProtectedArea, ClaimSpacing, ClaimMissing, NotOwner, PermissionDenied, Unpaid, Archived, RecoveryPending }
public readonly record struct Point(double X, double Y);
public sealed record ClaimView(string Id, string Owner, Point Center, int SideMetres, ClaimState State, long Revision,
    DateTimeOffset FundedUntil, DateTimeOffset? GraceEnds, DateTimeOffset? NoticeEnds,
    IReadOnlyDictionary<string, ClaimRight> Grants);
public sealed record Receipt(bool Accepted, Denial Reason, long Revision, ClaimView? Claim = null);
public sealed record Access(bool Allowed, Denial Reason, long PermissionRevision);

// Values arrive as a versioned effective configuration snapshot. Spatial defaults are shipped
// personal-plot content limits; changing their ceilings requires a compatible content release.
public sealed record ClaimSettings(int SideMetres = 40, int SpacingMetres = 5,
    int GraceDays = 7, int ReclamationDays = 30, int FinalNoticeDays = 7)
{
    public const string Permission = "config-service economy tier 3";
    public void Validate()
    {
        if (SideMetres is < 10 or > 40 || SpacingMetres is < 5 or > 20 ||
            GraceDays is < 7 or > 30 || ReclamationDays is < 30 or > 90 ||
            FinalNoticeDays is < 7 or > 30 || GraceDays > ReclamationDays)
            throw new ArgumentOutOfRangeException(nameof(ClaimSettings));
    }
}

public sealed class ProtectedZone
{
    public string ZoneId { get; }
    public double MinX { get; }
    public double MinY { get; }
    public double MaxX { get; }
    public double MaxY { get; }
    public IReadOnlyList<IReadOnlyList<Point>> Polygons { get; }

    private ProtectedZone(string id, double minX, double minY, double maxX, double maxY, List<IReadOnlyList<Point>> polygons)
        => (ZoneId, MinX, MinY, MaxX, MaxY, Polygons) = (id, minX, minY, maxX, maxY, polygons);

    public static ProtectedZone Load(string path)
    {
        using var doc = JsonDocument.Parse(File.ReadAllText(path));
        var root = doc.RootElement;
        var extent = root.GetProperty("extent");
        var a = extent[0]; var b = extent[1];
        var polygons = new List<IReadOnlyList<Point>>();
        foreach (var area in root.GetProperty("areas").EnumerateArray())
        {
            if (area.GetProperty("safety").GetString() != "protected") continue;
            polygons.Add(area.GetProperty("polygon").EnumerateArray()
                .Select(p => new Point(p[0].GetDouble(), p[1].GetDouble())).ToArray());
        }
        return new(root.GetProperty("id").GetString()!, a[0].GetDouble(), a[1].GetDouble(),
            b[0].GetDouble(), b[1].GetDouble(), polygons);
    }

    internal bool ContainsSquare(Point center, int side)
    {
        var h = side / 2.0;
        return center.X - h >= MinX && center.X + h <= MaxX && center.Y - h >= MinY && center.Y + h <= MaxY;
    }

    internal bool HitsProtected(Point center, int side)
    {
        var h = side / 2.0;
        var square = new[] { new Point(center.X-h, center.Y-h), new Point(center.X+h, center.Y-h),
            new Point(center.X+h, center.Y+h), new Point(center.X-h, center.Y+h) };
        foreach (var polygon in Polygons)
        {
            if (square.Any(p => InPolygon(p, polygon)) || polygon.Any(p => InSquare(p, center, h))) return true;
            for (var i = 0; i < 4; i++)
                for (var j = 0; j < polygon.Count; j++)
                    if (Intersect(square[i], square[(i+1)%4], polygon[j], polygon[(j+1)%polygon.Count])) return true;
        }
        return false;
    }

    private static bool InSquare(Point p, Point c, double h) => Math.Abs(p.X-c.X) <= h && Math.Abs(p.Y-c.Y) <= h;
    private static bool InPolygon(Point p, IReadOnlyList<Point> poly)
    {
        var inside = false;
        for (int i = 0, j = poly.Count-1; i < poly.Count; j = i++)
        {
            var a = poly[i]; var b = poly[j];
            if ((a.Y > p.Y) != (b.Y > p.Y) && p.X < (b.X-a.X)*(p.Y-a.Y)/(b.Y-a.Y)+a.X) inside = !inside;
        }
        return inside;
    }
    private static double Cross(Point a, Point b, Point c) => (b.X-a.X)*(c.Y-a.Y)-(b.Y-a.Y)*(c.X-a.X);
    private static bool Intersect(Point a, Point b, Point c, Point d)
    {
        const double e = 1e-9;
        var abC = Cross(a,b,c); var abD = Cross(a,b,d); var cdA = Cross(c,d,a); var cdB = Cross(c,d,b);
        if (Math.Abs(abC) < e && OnSegment(a,b,c) || Math.Abs(abD) < e && OnSegment(a,b,d) ||
            Math.Abs(cdA) < e && OnSegment(c,d,a) || Math.Abs(cdB) < e && OnSegment(c,d,b)) return true;
        return (abC > 0) != (abD > 0) && (cdA > 0) != (cdB > 0);
    }
    private static bool OnSegment(Point a, Point b, Point p) =>
        p.X >= Math.Min(a.X,b.X)-1e-9 && p.X <= Math.Max(a.X,b.X)+1e-9 &&
        p.Y >= Math.Min(a.Y,b.Y)-1e-9 && p.Y <= Math.Max(a.Y,b.Y)+1e-9;
}

// A single lock stands in for an exclusive durable reservation. The injected archive callback
// must commit the complete construction/terrain/inventory recovery before returning true.
public sealed class ClaimService(ProtectedZone zone, TimeProvider clock, Func<ClaimView, bool> archive,
    ClaimSettings? initialSettings = null)
{
    private sealed class Entry(string id, string owner, Point center, int side, DateTimeOffset funded)
    {
        public string Id = id, Owner = owner;
        public Point Center = center;
        public int Side = side;
        public ClaimState State = ClaimState.Active;
        public long Revision = 1;
        public DateTimeOffset FundedUntil = funded;
        public DateTimeOffset? GraceEnds, NoticeEnds;
        public int GraceDays, ReclamationDays, FinalNoticeDays;
        public Dictionary<string, ClaimRight> Grants = new(StringComparer.Ordinal);
        public ClaimView View() => new(Id, Owner, Center, Side, State, Revision, FundedUntil, GraceEnds, NoticeEnds,
            new Dictionary<string, ClaimRight>(Grants, StringComparer.Ordinal));
    }
    private readonly object gate = new();
    private readonly List<Entry> entries = [];
    private readonly Dictionary<string, (string Payload, Receipt Receipt)> operations = new(StringComparer.Ordinal);
    private ClaimSettings settings = Validate(initialSettings ?? new());
    private long revision;
    public long Revision { get { lock (gate) return revision; } }
    public IReadOnlyList<ClaimView> Claims { get { lock (gate) { Refresh(); return entries.Select(e => e.View()).ToArray(); } } }

    // A new effective snapshot applies to future reservations and future unpaid episodes only.
    public void SetEffectiveSettings(ClaimSettings next) { lock (gate) settings = Validate(next); }
    private static ClaimSettings Validate(ClaimSettings value) { value.Validate(); return value; }
    private DateTimeOffset Now => clock.GetUtcNow();
    private static bool Live(Entry e) => e.State is not ClaimState.Archived and not ClaimState.Reclaimed;
    private Entry? Find(string id) => entries.FirstOrDefault(e => e.Id == id);
    private Receipt Execute(string op, string payload, Func<Receipt> action)
    {
        lock (gate)
        {
            if (string.IsNullOrWhiteSpace(op) || op.Length > 128) return new(false, Denial.InvalidRequest, revision);
            if (operations.TryGetValue(op, out var prior))
                return prior.Payload == payload ? prior.Receipt : new(false, Denial.OperationConflict, revision);
            Refresh();
            var result = action();
            operations.Add(op, (payload, result));
            return result;
        }
    }

    public Receipt Reserve(string op, long expectedWorldRevision, string claimId, string owner, Point center, int sideMetres, int prepaidDays = 1)
        => Execute(op, $"reserve|{expectedWorldRevision}|{claimId}|{owner}|{center.X:R}|{center.Y:R}|{sideMetres}|{prepaidDays}", () =>
        {
            if (expectedWorldRevision != revision) return new(false, Denial.StaleRevision, revision);
            if (string.IsNullOrWhiteSpace(claimId) || !claimId.StartsWith("clm_", StringComparison.Ordinal) ||
                string.IsNullOrWhiteSpace(owner) || !double.IsFinite(center.X) || !double.IsFinite(center.Y) ||
                sideMetres < 10 || sideMetres > settings.SideMetres || prepaidDays is < 1 or > 30 || Find(claimId) != null)
                return new(false, Denial.InvalidRequest, revision);
            if (entries.Any(e => e.Owner == owner && Live(e))) return new(false, Denial.EntitlementUsed, revision);
            if (!zone.ContainsSquare(center, sideMetres)) return new(false, Denial.OutsideZone, revision);
            if (zone.HitsProtected(center, sideMetres + 2*settings.SpacingMetres)) return new(false, Denial.ProtectedArea, revision);
            if (entries.Any(e => Live(e) && Math.Abs(e.Center.X-center.X) < (e.Side+sideMetres)/2.0+settings.SpacingMetres &&
                Math.Abs(e.Center.Y-center.Y) < (e.Side+sideMetres)/2.0+settings.SpacingMetres))
                return new(false, Denial.ClaimSpacing, revision);
            var claim = new Entry(claimId, owner, center, sideMetres, Now.AddDays(prepaidDays));
            entries.Add(claim); revision++;
            return new(true, Denial.None, revision, claim.View());
        });

    public Receipt Grant(string op, string id, string actor, string target, ClaimRight rights, long expectedRevision)
        => ChangeGrant(op, id, actor, target, rights, expectedRevision, false);
    public Receipt Revoke(string op, string id, string actor, string target, ClaimRight rights, long expectedRevision)
        => ChangeGrant(op, id, actor, target, rights, expectedRevision, true);
    private Receipt ChangeGrant(string op, string id, string actor, string target, ClaimRight rights, long expected, bool revoke)
        => Execute(op, $"grant|{revoke}|{id}|{actor}|{target}|{rights}|{expected}", () =>
        {
            var e = Find(id);
            if (e == null) return new(false, Denial.ClaimMissing, revision);
            if (e.Revision != expected) return new(false, Denial.StaleRevision, e.Revision);
            if (e.Owner != actor) return new(false, Denial.NotOwner, e.Revision);
            if (!Live(e)) return new(false, Denial.Archived, e.Revision);
            if (string.IsNullOrWhiteSpace(target) || target == actor || rights == 0 || (rights & ~AllRights) != 0)
                return new(false, Denial.InvalidRequest, e.Revision);
            e.Grants.TryGetValue(target, out var prior);
            var next = revoke ? prior & ~rights : prior | rights;
            if (next == 0) e.Grants.Remove(target); else e.Grants[target] = next;
            e.Revision++; revision++;
            return new(true, Denial.None, e.Revision, e.View());
        });
    private const ClaimRight AllRights = ClaimRight.Build | ClaimRight.OpenDoors | ClaimRight.UseContainers;

    public Access Check(string id, string actor, ClaimRight right)
    {
        lock (gate)
        {
            Refresh();
            var e = Find(id);
            if (e == null) return new(false, Denial.ClaimMissing, 0);
            if (!Live(e)) return new(false, Denial.Archived, e.Revision);
            if (right == 0 || (right & ~AllRights) != 0) return new(false, Denial.InvalidRequest, e.Revision);
            if (right.HasFlag(ClaimRight.Build) && e.State is ClaimState.Delinquent or ClaimState.Notice)
                return new(false, Denial.Unpaid, e.Revision);
            if (actor == e.Owner) return new(true, Denial.None, e.Revision);
            return e.Grants.TryGetValue(actor, out var grants) && (grants & right) == right
                ? new(true, Denial.None, e.Revision) : new(false, Denial.PermissionDenied, e.Revision);
        }
    }

    public Receipt Pay(string op, string id, string actor, long expectedRevision, int prepaidDays)
        => Execute(op, $"pay|{id}|{actor}|{expectedRevision}|{prepaidDays}", () =>
        {
            var e = Find(id);
            if (e == null) return new(false, Denial.ClaimMissing, revision);
            if (e.Revision != expectedRevision) return new(false, Denial.StaleRevision, e.Revision);
            if (e.Owner != actor) return new(false, Denial.NotOwner, e.Revision);
            if (!Live(e)) return new(false, Denial.Archived, e.Revision);
            if (prepaidDays is < 1 or > 30) return new(false, Denial.InvalidRequest, e.Revision);
            e.FundedUntil = Now.AddDays(prepaidDays);
            e.GraceEnds = e.NoticeEnds = null; e.State = ClaimState.Active;
            e.Revision++; revision++;
            return new(true, Denial.None, e.Revision, e.View());
        });

    public Receipt Recover(string op, string id, string actor, long expectedRevision)
        => Execute(op, $"recover|{id}|{actor}|{expectedRevision}", () =>
        {
            var e = Find(id);
            if (e == null) return new(false, Denial.ClaimMissing, revision);
            if (e.Revision != expectedRevision) return new(false, Denial.StaleRevision, e.Revision);
            if (e.Owner != actor) return new(false, Denial.NotOwner, e.Revision);
            if (e.State != ClaimState.Archived) return new(false, Denial.RecoveryPending, e.Revision);
            e.State = ClaimState.Reclaimed; e.Revision++; revision++;
            return new(true, Denial.None, e.Revision, e.View());
        });

    private void Refresh()
    {
        var now = Now;
        foreach (var e in entries.Where(Live))
        {
            if (e.State == ClaimState.Active && now >= e.FundedUntil)
            {
                e.State = ClaimState.Grace;
                e.GraceDays = settings.GraceDays; e.ReclamationDays = settings.ReclamationDays;
                e.FinalNoticeDays = settings.FinalNoticeDays;
                e.GraceEnds = e.FundedUntil.AddDays(e.GraceDays);
                e.Revision++; revision++;
            }
            if (e.State == ClaimState.Grace && now >= e.GraceEnds)
            { e.State = ClaimState.Delinquent; e.Revision++; revision++; }
            if (e.State == ClaimState.Delinquent && now >= e.FundedUntil.AddDays(e.ReclamationDays))
            { e.State = ClaimState.Notice; e.NoticeEnds = e.FundedUntil.AddDays(e.ReclamationDays + e.FinalNoticeDays); e.Revision++; revision++; }
            if (e.State == ClaimState.Notice && now >= e.NoticeEnds)
            {
                if (archive(e.View())) { e.State = ClaimState.Archived; e.Revision++; revision++; }
            }
        }
    }
}
