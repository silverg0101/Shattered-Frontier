using ClaimTerritory;

static void Check(bool condition, string message)
{
    if (!condition) throw new Exception(message);
}

var zone = ProtectedZone.Load(args.Length > 0 ? args[0] : "content/world/zones/greenlands_silverbrook.json");
var clock = new FakeClock(new DateTimeOffset(2026, 1, 1, 0, 0, 0, TimeSpan.Zero));
var archived = new List<string>();
var archiveReady = false;
var service = new ClaimService(zone, clock, c => { if (!archiveReady) return false; archived.Add(c.Id); return true; });
try { new ClaimSettings(GraceDays: 6).Validate(); throw new Exception("accepted invalid grace"); }
catch (ArgumentOutOfRangeException) { }
Check(service.Reserve("oversize", 0, "clm_big", "owner", new Point(900, 900), 41).Reason == Denial.InvalidRequest, "size ceiling");
Check(service.Reserve("edge", 0, "clm_edge", "owner", new Point(1240, 900), 40).Reason == Denial.OutsideZone, "zone boundary");
var protectedAttempt = service.Reserve("protected", 0, "clm_bad", "owner", new Point(0, 0), 40);
Check(protectedAttempt.Reason == Denial.ProtectedArea && service.Revision == 0, "protected town reservation");
var a = service.Reserve("first", 0, "clm_one", "owner", new Point(900, 900), 40);
Check(a.Accepted && a.Claim?.State == ClaimState.Active, "first claim");
Check(ReferenceEquals(a, service.Reserve("first", 0, "clm_one", "owner", new Point(900, 900), 40)), "same receipt on retry");
Check(service.Reserve("first", 0, "clm_other", "owner", new Point(900, 900), 40).Reason == Denial.OperationConflict, "changed retry");
Check(service.Reserve("second", 1, "clm_two", "other", new Point(920, 900), 40).Reason == Denial.ClaimSpacing, "overlap");
Check(service.Reserve("buffer", 1, "clm_three", "other", new Point(944, 900), 40).Reason == Denial.ClaimSpacing, "clear buffer");
Check(service.Reserve("entitlement", 1, "clm_four", "owner", new Point(1000, 900), 40).Reason == Denial.EntitlementUsed, "one per character");
Check(service.Check("clm_one", "visitor", ClaimRight.Build).Reason == Denial.PermissionDenied, "build denial");
Check(service.Check("clm_one", "visitor", ClaimRight.OpenDoors).Reason == Denial.PermissionDenied, "door denial");
Check(service.Check("clm_one", "visitor", ClaimRight.UseContainers).Reason == Denial.PermissionDenied, "container denial");
var grant = service.Grant("grant", "clm_one", "owner", "visitor", ClaimRight.Build | ClaimRight.OpenDoors, 1);
Check(grant.Accepted && service.Check("clm_one", "visitor", ClaimRight.Build).Allowed, "grant build");
Check(service.Check("clm_one", "visitor", ClaimRight.OpenDoors).Allowed, "grant door");
Check(service.Check("clm_one", "visitor", ClaimRight.UseContainers).Reason == Denial.PermissionDenied, "container separate");
Check(service.Grant("stale", "clm_one", "owner", "visitor", ClaimRight.UseContainers, 1).Reason == Denial.StaleRevision, "stale revision");
var revoke = service.Revoke("revoke", "clm_one", "owner", "visitor", ClaimRight.Build, grant.Claim!.Revision);
Check(revoke.Accepted && service.Check("clm_one", "visitor", ClaimRight.Build).Reason == Denial.PermissionDenied, "revoke");
Check(service.Check("clm_one", "visitor", ClaimRight.OpenDoors).Allowed, "remaining right");
clock.Advance(TimeSpan.FromDays(2));
Check(service.Claims.Single().State == ClaimState.Grace, "grace");
clock.Advance(TimeSpan.FromDays(7));
Check(service.Claims.Single().State == ClaimState.Delinquent && service.Check("clm_one", "owner", ClaimRight.Build).Reason == Denial.Unpaid, "delinquency");
Check(service.Check("clm_one", "visitor", ClaimRight.OpenDoors).Allowed, "entry through delinquency");
clock.Advance(TimeSpan.FromDays(29));
Check(service.Claims.Single().State == ClaimState.Notice && archived.Count == 0, "archive failure holds reservation");
Check(service.Reserve("held", service.Revision, "clm_held", "other", new Point(900, 900), 40).Reason == Denial.ClaimSpacing, "land held until archive");
archiveReady = true;
var archivedClaim = service.Claims.Single();
Check(archivedClaim.State == ClaimState.Archived && archived.Count == 1, "archive completed once");
Check(service.Reserve("new", service.Revision, "clm_new", "other", new Point(900, 900), 40).Accepted, "released land");
Check(service.Recover("recover-bad", "clm_one", "visitor", archivedClaim.Revision).Reason == Denial.NotOwner, "owner-only recovery");
Check(service.Recover("recover", "clm_one", "owner", archivedClaim.Revision).Claim?.State == ClaimState.Reclaimed, "recover archive");
Check(archived.Count == 1, "no duplicate archive");

var concurrent = new ClaimService(zone, clock, _ => true);
var start = new ManualResetEventSlim(false);
var contenders = Enumerable.Range(0, 16).Select(i => Task.Run(() =>
{
    start.Wait();
    return concurrent.Reserve($"race-{i}", 0, $"clm_race{i}", $"owner{i}", new Point(900, 900), 40);
})).ToArray();
start.Set(); Task.WaitAll(contenders);
Check(contenders.Count(t => t.Result.Accepted) == 1 && concurrent.Claims.Count == 1, "exclusive concurrent winner");

var recoveryClock = new FakeClock(new DateTimeOffset(2026, 1, 1, 0, 0, 0, TimeSpan.Zero));
var recovery = new ClaimService(zone, recoveryClock, _ => true);
var original = recovery.Reserve("r", 0, "clm_recovery", "owner", new Point(900, 900), 40);
recoveryClock.Advance(TimeSpan.FromDays(3));
var lapsed = recovery.Claims.Single();
Check(lapsed.State == ClaimState.Grace, "lapsed state");
Check(recovery.Pay("pay", lapsed.Id, "owner", lapsed.Revision, 3).Claim?.State == ClaimState.Active, "payment restores active");
Check(recovery.Check(lapsed.Id, "owner", ClaimRight.Build).Allowed, "build restored");
var gridSettings = new ClaimGridSettings();
var bandMap = ClaimBandMap.Load("content/world/zones/greenlands_silverbrook.json",
    "GameClient/ArtSource/Greenlands/silverbrook-town-layout.json", gridSettings);
Check(bandMap.Band(new ClaimCell(900, 900)) == ClaimBand.Wilderness &&
    bandMap.BlockReason(new ClaimCell(900, 900)) == "", "authored wilderness band");
Check(bandMap.BlockReason(new ClaimCell(0, 0)) != "", "town clearance");
var grid = new ClaimGridService(bandMap, clock, gridSettings);
var footprint = new[] { new ClaimCell(900, 900), new ClaimCell(901, 900), new ClaimCell(900, 901), new ClaimCell(901, 901) };
var firstGrid = grid.Grow("g1", "clm_grid1", "owner", 1, footprint, 0, 100);
Check(firstGrid.Accepted && firstGrid.Claim!.Cells.Count == 4 && grid.Allocated("owner") == 3m, "four priced cells");
Check(grid.Grow("g1", "clm_grid1", "owner", 1, footprint, 0, 100) == firstGrid, "growth replay");
Check(grid.Grow("g1", "clm_grid1", "owner", 1, footprint[..1], 0, 100).Reason == "operation_conflict", "growth conflict");
Check(grid.Contains("clm_grid1", footprint[0], 152) && !grid.Contains("clm_grid1", footprint[0], 161), "TER-02 vertical band");
Check(grid.Grow("far", "clm_grid1", "owner", 1, [new ClaimCell(920, 900)], 1, 100).Reason == "outside_reach", "reach gate");
var secondGrid = grid.Grow("g2", "clm_grid2", "owner", 1, [new ClaimCell(1050, 800)], grid.Revision, 100);
var thirdGrid = grid.Grow("g3", "clm_grid3", "owner", 1, [new ClaimCell(600, 800)], grid.Revision, 100);
Check(secondGrid.Accepted && thirdGrid.Accepted, "three claims share budget");
Check(grid.Grow("g4", "clm_grid4", "owner", 1, [new ClaimCell(1100, 800)], grid.Revision, 100).Reason == "claim_slots", "fourth claim denied");
Check(grid.Grow("contend", "clm_other", "other", 1, footprint, grid.Revision, 100).Reason == "claim_spacing", "other owner spacing");
var shrunk = grid.Shrink("shrink", "clm_grid1", "owner", footprint[..1], firstGrid.Claim!.Revision);
Check(shrunk.Accepted && grid.Allocated("owner") == 2.25m, "shrink returns points");
var empty = grid.Shrink("empty", "clm_grid1", "owner", [], shrunk.Claim!.Revision);
Check(empty.Accepted && grid.Claims.Count == 3, "empty claim waits");
clock.Advance(TimeSpan.FromHours(25));
Check(grid.Claims.Count == 2, "empty claim archives");
Check(new ClaimGridSettings(AlphaFixedPoints: 0).Budget(100) == 4400, "shipped level budget");
var footprintGrid = new ClaimGridService(bandMap, clock, gridSettings);
var slab = (from x in Enumerable.Range(900, 4) from y in Enumerable.Range(900, 4)
    select new ClaimCell(x, y)).ToArray();
var slabGrowth = footprintGrid.GrowForFootprint("slab", "clm_footprint", "builder", 1, slab, 0, 100);
Check(slabGrowth.Accepted && slabGrowth.Claim!.Cells.Count == 144 &&
    footprintGrid.Allocated("builder") == 108m, "4 by 4 slab prices its complete 12 by 12 reach ring");
var wall = new ClaimCell(907, 900);
var wallGrowth = footprintGrid.GrowForFootprint("wall", "clm_footprint", "builder", 1,
    [wall], slabGrowth.Claim!.Revision, 100);
Check(wallGrowth.Accepted && wallGrowth.Claim!.Cells.Count > 144, "edge wall grows the sparse mask");
Check(footprintGrid.GrowForFootprint("remote", "clm_footprint", "builder", 1,
    [new ClaimCell(920, 900)], wallGrowth.Claim!.Revision, 100).Reason == "outside_reach",
    "remote piece refused before pricing its ring");
var release = footprintGrid.ShrinkToFootprints("remove-wall", "clm_footprint", "builder",
    slab, wallGrowth.Claim!.Revision);
Check(release.Accepted && release.Claim!.Cells.Count == 144 &&
    footprintGrid.Allocated("builder") == 108m, "wall removal releases only unsupported priced cells");
var emptyFootprint = footprintGrid.ShrinkToFootprints("remove-slab", "clm_footprint", "builder",
    [], release.Claim!.Revision);
Check(emptyFootprint.Accepted && emptyFootprint.Claim!.Cells.Count == 0, "last piece empties claim");
var replaced = footprintGrid.GrowForFootprint("replace-slab", "clm_footprint", "builder", 1,
    slab, emptyFootprint.Claim!.Revision, 100);
Check(replaced.Accepted && replaced.Claim!.EmptySince is null && replaced.Claim.Cells.Count == 144,
    "placing within the recovery window keeps the same claim: " + replaced.Reason);
Console.WriteLine("ClaimTerritory: focused assertions passed");

sealed class FakeClock(DateTimeOffset initial) : TimeProvider
{
    private DateTimeOffset now = initial;
    public override DateTimeOffset GetUtcNow() => now;
    public void Advance(TimeSpan interval) => now += interval;
}
