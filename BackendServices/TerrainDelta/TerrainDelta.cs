namespace ShatteredFrontier.TerrainDelta;

public enum TerrainKind { Dig, Raise, Flatten, Paint }
public readonly record struct RegionKey(string ZoneId, int X, int Y);
public readonly record struct IntegerAnchor(int X, int Y, int Z);
public sealed record TerrainEdit(string OperationId, RegionKey Region, string CharacterId, string ClaimId,
    IntegerAnchor Anchor, TerrainKind Kind, int Magnitude);
public sealed record TerrainReceipt(string OperationId, RegionKey Region, long Revision, bool Tombstone, string? CompensatedOperationId);
public sealed record RegionEntry(TerrainReceipt Receipt, TerrainEdit? Edit);
public sealed record RegionListing(long Revision, IReadOnlyList<RegionEntry> Entries);
public sealed class TerrainConflictException(string message) : InvalidOperationException(message);

public static class TerrainBoundary
{
    // Storage envelope, in integer cells and signed-density units. This is a release-bound safety limit.
    public const int MaxMagnitude = 32767;
    public const int MaxAnchorCell = 32767;
    public static void ValidateId(string value, string name)
    {
        if (string.IsNullOrWhiteSpace(value) || value.Length > 128 ||
            !System.Text.RegularExpressions.Regex.IsMatch(value, "\\A[a-zA-Z0-9_.:-]+\\z"))
            throw new ArgumentException($"Invalid {name}.", name);
    }
    public static void ValidateRegion(RegionKey region)
    {
        ValidateId(region.ZoneId, nameof(region.ZoneId));
        if (Math.Abs((long)region.X) > MaxAnchorCell || Math.Abs((long)region.Y) > MaxAnchorCell)
            throw new ArgumentOutOfRangeException(nameof(region));
    }
    public static void Validate(TerrainEdit edit)
    {
        ValidateId(edit.OperationId, nameof(edit.OperationId)); ValidateRegion(edit.Region);
        ValidateId(edit.CharacterId, nameof(edit.CharacterId)); ValidateId(edit.ClaimId, nameof(edit.ClaimId));
        if (Math.Abs((long)edit.Anchor.X) > MaxAnchorCell || Math.Abs((long)edit.Anchor.Y) > MaxAnchorCell ||
            Math.Abs((long)edit.Anchor.Z) > MaxAnchorCell) throw new ArgumentOutOfRangeException(nameof(edit.Anchor));
        if (!Enum.IsDefined(edit.Kind)) throw new ArgumentOutOfRangeException(nameof(edit.Kind));
        if (edit.Magnitude is < 1 or > MaxMagnitude) throw new ArgumentOutOfRangeException(nameof(edit.Magnitude));
    }
}

public sealed class InMemoryTerrainDeltaStore
{
    private readonly object gate = new();
    private readonly Dictionary<RegionKey, List<RegionEntry>> regions = [];
    private readonly Dictionary<string, (object Request, TerrainReceipt Receipt)> receipts = [];
    public TerrainReceipt Apply(TerrainEdit edit, long expectedRevision)
    {
        TerrainBoundary.Validate(edit);
        if (expectedRevision < 0) throw new ArgumentOutOfRangeException(nameof(expectedRevision));
        lock (gate)
        {
            if (receipts.TryGetValue(edit.OperationId, out var prior))
                return prior.Request.Equals(edit) ? prior.Receipt : throw new TerrainConflictException("Operation ID conflict.");
            var entries = Entries(edit.Region);
            if (entries.Count != expectedRevision) throw new TerrainConflictException("Region revision conflict.");
            var receipt = new TerrainReceipt(edit.OperationId, edit.Region, expectedRevision + 1, false, null);
            entries.Add(new RegionEntry(receipt, edit)); receipts.Add(edit.OperationId, (edit, receipt));
            return receipt;
        }
    }
    public TerrainReceipt Compensate(string operationId, string targetOperationId, RegionKey region,
        string characterId, string claimId, long expectedRevision)
    {
        TerrainBoundary.ValidateId(operationId, nameof(operationId)); TerrainBoundary.ValidateId(targetOperationId, nameof(targetOperationId));
        TerrainBoundary.ValidateRegion(region); TerrainBoundary.ValidateId(characterId, nameof(characterId));
        TerrainBoundary.ValidateId(claimId, nameof(claimId));
        if (expectedRevision < 0) throw new ArgumentOutOfRangeException(nameof(expectedRevision));
        var request = (operationId, targetOperationId, region, characterId, claimId);
        lock (gate)
        {
            if (receipts.TryGetValue(operationId, out var prior))
                return prior.Request.Equals(request) ? prior.Receipt : throw new TerrainConflictException("Operation ID conflict.");
            var entries = Entries(region);
            if (entries.Count != expectedRevision) throw new TerrainConflictException("Region revision conflict.");
            if (!receipts.TryGetValue(targetOperationId, out var target) || target.Receipt.Region != region ||
                target.Receipt.Tombstone || target.Request is not TerrainEdit edit ||
                edit.CharacterId != characterId || edit.ClaimId != claimId ||
                entries.Any(e => e.Receipt.CompensatedOperationId == targetOperationId))
                throw new TerrainConflictException("Compensation target unavailable or authority mismatch.");
            var receipt = new TerrainReceipt(operationId, region, expectedRevision + 1, true, targetOperationId);
            entries.Add(new RegionEntry(receipt, null)); receipts.Add(operationId, (request, receipt));
            return receipt;
        }
    }
    public RegionListing ListRegion(RegionKey region, long sinceRevision = 0)
    {
        TerrainBoundary.ValidateRegion(region);
        if (sinceRevision < 0) throw new ArgumentOutOfRangeException(nameof(sinceRevision));
        lock (gate)
        {
            var entries = Entries(region);
            if (sinceRevision > entries.Count) throw new TerrainConflictException("Revision exceeds head.");
            return new RegionListing(entries.Count, entries.Where(e => e.Receipt.Revision > sinceRevision).ToArray());
        }
    }
    private List<RegionEntry> Entries(RegionKey region)
    {
        if (!regions.TryGetValue(region, out var list)) regions.Add(region, list = []);
        return list;
    }
}
