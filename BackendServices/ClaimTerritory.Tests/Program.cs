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
Console.WriteLine("ClaimTerritory: focused assertions passed");

sealed class FakeClock(DateTimeOffset initial) : TimeProvider
{
    private DateTimeOffset now = initial;
    public override DateTimeOffset GetUtcNow() => now;
    public void Advance(TimeSpan interval) => now += interval;
}
