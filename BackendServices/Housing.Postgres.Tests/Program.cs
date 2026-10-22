using ShatteredFrontier.Housing.Postgres;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;
using Ledger = ShatteredFrontier.InventoryLedger.InventoryLedger;

var connection = Environment.GetEnvironmentVariable("HOUSING_CONNECTION") ?? throw new Exception("Missing HOUSING_CONNECTION.");
var housing = new PostgresHousingStore(connection);
var ledgerStore = new PostgresInventoryLedgerStore(connection);
var ledger = new Ledger(ledgerStore);
var phase = args.Single();
var now = DateTimeOffset.UtcNow;
static void Check(bool value, string message) { if (!value) throw new Exception(message); }
static HousingPlaceRequest Request(string id,int x,long revision) => new(id,$"fingerprint-{id}","clm_test","chr_owner",0,revision,$"str_{id}",new(x,0,0),"piece/foundation","item/foundation_material_bundle",1,"ctn_bag",revision);
var Hold = PostgresHousingStore.HoldSpec("chr_owner").Id;
void Reserve(string id, long bagRevision)
{
    var hold = ledger.CreateContainer(PostgresHousingStore.HoldSpec("chr_owner"));
    ledger.Transfer(id + "/reserve", "ctn_bag", bagRevision, hold.Spec.Id, hold.Revision, "item/foundation_material_bundle", 1);
}
switch (phase)
{
    case "seed":
        var claim = new HousingClaim("clm_test","chr_owner","Active",0,0,DateTimeOffset.UtcNow.AddDays(1),null,null,new Dictionary<string,int>{{"builder",1}});
        housing.SaveClaim("claim/create","create",claim,-1);
        Check(housing.SaveClaim("claim/create","create",claim,-1).Id == claim.Id,"Claim replay failed.");
        ledger.CreateContainer(new("ctn_bag",OwnerKind.Character,"chr_owner",ContainerKind.Backpack,100));
        ledger.Grant("materials","ctn_bag",0,"item/foundation_material_bundle",10,"fixture","fixture");
        var first = housing.Begin(Request("one",0,0) with { ExpectedContainerRevision=1 },now);
        Check(first.State == "Pending","First prepare failed.");
        Reserve("one",1);
        Check(ledger.Read(Hold).Quantities[first.MaterialId] == 1,"Reservation did not hold the materials.");
        Check(housing.Reconcile("one",ledgerStore).State == "Committed","First commit failed.");
        Check(!ledger.Read(Hold).Quantities.ContainsKey(first.MaterialId),"Commit did not debit the hold.");
        housing.Begin(Request("two",1,1) with { ExpectedContainerRevision=2 },now);
        Reserve("two",2);
        housing.Begin(Request("decided",5,1) with { ExpectedContainerRevision=3 },now);
        Reserve("decided",3);
        Check(housing.Decide("decided",ledgerStore,now).Decision == "Commit","Commit decision was not recorded.");
        Console.WriteLine("PASS seed, reserve and debit, interrupted reservation, killed after commit decision");
        break;
    case "restart":
        Check(housing.Claim("clm_test")?.OwnerId == "chr_owner","Claim lost on restart.");
        Check(housing.Structures("clm_test").Count == 1,"Committed structure lost on restart.");
        Check(housing.Reconcile("two",ledgerStore).State == "Committed","Confirmed reservation was not committed on recovery.");
        Check(housing.Reconcile("two",ledgerStore).Revision == 2,"Recovery replay changed revision.");
        Check(housing.Begin(Request("two",1,1) with { ExpectedContainerRevision=2 },now).State == "Committed","Retry did not return the same decision.");
        Check(housing.Placement("decided") is { State: "Pending", Decision: "Commit" },"Commit decision lost on restart.");
        var rolled = housing.Reconcile("decided",ledgerStore);
        Check(rolled.State == "Committed" && rolled.Revision == 3 && rolled.ConsumeReceipt?.OperationId == "decided/consume","Commit after restart did not roll forward.");
        var parallelA = housing.Begin(Request("parallel-a",10,3) with { ExpectedContainerRevision=4 },now);
        var parallelB = housing.Begin(Request("parallel-b",11,3) with { ExpectedContainerRevision=5 },now);
        Reserve("parallel-a",4);
        Reserve("parallel-b",5);
        Check(housing.Reconcile("parallel-a",ledgerStore).Revision == 4,"First same-revision placement failed.");
        Check(housing.Reconcile("parallel-b",ledgerStore).Revision == 5,"Second same-revision placement failed.");
        var noDebit = housing.Begin(Request("three",2,5) with { ExpectedContainerRevision=6 },now);
        Check(noDebit.State == "Pending","Third prepare failed.");
        Check(housing.Reconcile("three",ledgerStore) is { State: "Compensated", Decision: "Abort" },"Unconfirmed reservation was not aborted.");
        var wrong = housing.Begin(Request("wrong-revision",12,5) with { ExpectedContainerRevision=6 },now);
        try { housing.Reconcile("wrong-revision",new FailingStore(new LedgerStoreUnavailableException("offline",new Exception()))); throw new Exception("Ledger outage accepted."); }
        catch (HousingUnavailableException) { }
        try { housing.Reconcile("wrong-revision",new FailingStore(new LedgerRejectedException("rejected"))); throw new Exception("Ledger rejection accepted."); }
        catch (HousingRejectedException) { }
        Check(housing.Placement("wrong-revision")?.State == "Pending","Ledger error changed placement.");
        var injected = new InMemoryInventoryLedgerStore();
        injected.Execute(s => { s.Operations.Add("wrong-revision/reserve",new StoredOperation("fixture",new LedgerReceipt(
            "wrong-revision/reserve",LedgerAction.Transfer,[new RevisionChange("ctn_bag",5,6),new RevisionChange(Hold,9,10)],
            [new QuantityChange("ctn_bag",wrong.MaterialId,-1),new QuantityChange(Hold,wrong.MaterialId,1)]))); return 0; });
        try { housing.Reconcile("wrong-revision",injected); throw new Exception("Wrong container revision accepted."); }
        catch (HousingRejectedException) { }
        Check(housing.Placement("wrong-revision") is { State: "Pending", Decision: null },"Mismatched receipt changed placement.");
        housing.Reconcile("wrong-revision",ledgerStore);
        Check(housing.Begin(Request("three",2,5) with { ExpectedContainerRevision=6 },now).State == "Compensated","Compensated retry changed state.");
        try { Reserve("three",6); throw new Exception("Late reservation accepted."); }
        catch (LedgerRejectedException) { }
        Check(ledger.Read("ctn_bag").Quantities["item/foundation_material_bundle"] == 5 && ledger.Read(Hold).Quantities.Count == 0,
            "Late reservation or guessed refund broke conservation.");
        Check(housing.Reconcile("three",ledgerStore).State == "Compensated","Compensated replay changed state.");
        // A grant revoked between reservation and decision aborts the action and releases the hold to the builder.
        var revoke = new HousingClaim("clm_revoke","chr_owner","Active",0,0,now.AddDays(1),null,null,new Dictionary<string,int>{{"chr_builder",1}});
        housing.SaveClaim("claim/revoke","revoke",revoke,-1);
        ledger.CreateContainer(new("ctn_builder",OwnerKind.Character,"chr_builder",ContainerKind.Backpack,100));
        ledger.Grant("builder-materials","ctn_builder",0,"item/foundation_material_bundle",2,"fixture","fixture");
        housing.Begin(Request("revoked",0,0) with { ClaimId="clm_revoke", ActorId="chr_builder", ContainerId="ctn_builder", ExpectedContainerRevision=1 },now);
        var builderHold = ledger.CreateContainer(PostgresHousingStore.HoldSpec("chr_builder"));
        ledger.Transfer("revoked/reserve","ctn_builder",1,builderHold.Spec.Id,builderHold.Revision,"item/foundation_material_bundle",1);
        housing.SaveClaim("claim/revoke-edit","revoke-edit",revoke with { Revision=1, PermissionRevision=1, Grants=new Dictionary<string,int>() },0);
        var released = housing.Reconcile("revoked",ledgerStore);
        Check(released is { State: "Compensated", Decision: "Abort" } && released.ConsumeReceipt?.OperationId == "revoked/release","Revoked action was not aborted.");
        Check(ledger.Read("ctn_builder").Quantities["item/foundation_material_bundle"] == 2 && ledger.Read(builderHold.Spec.Id).Quantities.Count == 0,
            "Abort did not release the held materials.");
        Check(housing.Structures("clm_revoke").Count == 0,"Aborted action placed a structure.");
        Console.WriteLine("PASS restart, commit roll-forward, two same-revision commits, fenced abort, release on revoke and conservation");
        break;
    case "concurrent":
        var wins = 0;
        var ready = new CountdownEvent(2);
        var release = new ManualResetEventSlim();
        var workers = new[]{"left","right"}.Select(id => Task.Run(() => {
            var worker = new PostgresHousingStore(connection);
            ready.Signal(); release.Wait();
            try { if (worker.Begin(Request(id,3,5) with { ExpectedContainerRevision=6 },now).State == "Pending") Interlocked.Increment(ref wins); }
            catch (HousingRejectedException) { }
        })).ToArray();
        Check(ready.Wait(TimeSpan.FromSeconds(10)),"Workers did not reach the barrier.");
        release.Set(); Task.WaitAll(workers);
        ready.Dispose(); release.Dispose();
        Check(wins == 1,"Same-cell contention had more than one winner.");
        foreach (var suffix in new[] { "a", "b" })
            housing.ReserveClaim("claim/parallel-"+suffix,"parallel-"+suffix,"clm_parallel_"+suffix,
                "chr_parallel_"+suffix,suffix=="a"?1000:2000,1000,10,1,0,40,5,now);
        using (var start = new ManualResetEventSlim())
        {
            var separate = new[] { "a", "b" }.Select(suffix => Task.Run(() =>
            {
                start.Wait();
                return new PostgresHousingStore(connection).Begin(Request("parallel-claim-"+suffix,30,0) with
                    { ClaimId="clm_parallel_"+suffix, ActorId="chr_parallel_"+suffix, ExpectedContainerRevision=5 },now);
            })).ToArray();
            start.Set(); Task.WaitAll(separate);
            Check(separate.All(t => t.Result.State == "Pending"),"Separate claims place concurrently.");
            foreach (var placement in separate)
                Check(housing.Reconcile(placement.Result.OperationId,ledgerStore).State == "Compensated","Parallel claim cleanup failed.");
        }
        var timed = new HousingClaim("clm_time","chr_owner","Active",0,0,now.AddMinutes(-1),null,null,new Dictionary<string,int>());
        housing.SaveClaim("claim/time","time",timed,-1);
        var timedRequest = Request("timed",20,0) with { ClaimId="clm_time", ExpectedContainerRevision=5 };
        try { housing.Begin(timedRequest,now); throw new Exception("Expired active claim accepted."); }
        catch (HousingRejectedException) { }
        housing.SaveClaim("claim/grace","grace",timed with { State="Grace", Revision=1, GraceEnds=now.AddDays(1) },0);
        Check(housing.Begin(timedRequest,now).State == "Pending","Grace build denied.");
        Check(housing.Reconcile("timed",ledgerStore).State == "Compensated","Grace cleanup failed.");
        try { housing.SaveClaim("claim/invalid","invalid",timed with { Id="clm_invalid", State="Invalid" },-1); throw new Exception("Invalid claim state accepted."); }
        catch (HousingRejectedException) { }
        try { housing.Begin(Request("denied",4,5) with { ActorId="intruder", ExpectedContainerRevision=6 },now); throw new Exception("Unauthorized build accepted."); }
        catch (HousingRejectedException) { }
        var old = housing.Claim("clm_test")!;
        var edited = old with { Revision=1, PermissionRevision=1, Grants=new Dictionary<string,int>() };
        housing.SaveClaim("claim/edit","edit",edited,0);
        Check(housing.SaveClaim("claim/create","create",old,-1).Revision == 0,"Claim retry did not return original receipt.");
        try { housing.SaveClaim("claim/stale","stale",edited,0); throw new Exception("Stale claim edit accepted."); }
        catch (HousingRejectedException) { }
        Console.WriteLine("PASS one cell winner, permission denial, revision CAS, original claim receipt");
        break;
    case "outage":
        try { housing.Claim("clm_test"); throw new Exception("Outage read succeeded."); }
        catch (HousingUnavailableException) { Console.WriteLine("PASS outage fails closed"); }
        break;
    case "newer":
        try { housing.Claim("clm_test"); throw new Exception("Newer schema accepted."); }
        catch (HousingSchemaCompatibilityException) { Console.WriteLine("PASS newer schema refused"); }
        break;
    default: throw new Exception("Unknown phase.");
}

sealed class FailingStore(Exception error) : IInventoryLedgerStore
{
    public T Execute<T>(Func<LedgerState,T> transaction) => throw error;
}
