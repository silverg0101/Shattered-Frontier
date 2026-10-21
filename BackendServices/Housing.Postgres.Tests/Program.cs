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
        ledger.Consume("one/consume","ctn_bag",1,first.MaterialId,1,"construction_material_consumed");
        Check(housing.Reconcile("one",ledgerStore).State == "Committed","First commit failed.");
        var second = housing.Begin(Request("two",1,1) with { ExpectedContainerRevision=2 },now);
        ledger.Consume("two/consume","ctn_bag",2,second.MaterialId,1,"construction_material_consumed");
        Console.WriteLine("PASS seed, acknowledged commit and interrupted consume");
        break;
    case "restart":
        Check(housing.Claim("clm_test")?.OwnerId == "chr_owner","Claim lost on restart.");
        Check(housing.Structures("clm_test").Count == 1,"Committed structure lost on restart.");
        Check(housing.Reconcile("two",ledgerStore).State == "Committed","Confirmed debit was not committed on recovery.");
        Check(housing.Reconcile("two",ledgerStore).Revision == 2,"Recovery replay changed revision.");
        Check(housing.Begin(Request("two",1,1) with { ExpectedContainerRevision=2 },now).State == "Committed","Retry did not return the same decision.");
        var parallelA = housing.Begin(Request("parallel-a",10,2) with { ExpectedContainerRevision=3 },now);
        var parallelB = housing.Begin(Request("parallel-b",11,2) with { ExpectedContainerRevision=4 },now);
        ledger.Consume("parallel-a/consume","ctn_bag",3,parallelA.MaterialId,1,"construction_material_consumed");
        ledger.Consume("parallel-b/consume","ctn_bag",4,parallelB.MaterialId,1,"construction_material_consumed");
        Check(housing.Reconcile("parallel-a",ledgerStore).Revision == 3,"First same-revision placement failed.");
        Check(housing.Reconcile("parallel-b",ledgerStore).Revision == 4,"Second same-revision placement failed.");
        var noDebit = housing.Begin(Request("three",2,4) with { ExpectedContainerRevision=5 },now);
        Check(noDebit.State == "Pending","Third prepare failed.");
        Check(housing.Reconcile("three",ledgerStore).State == "Compensated","Unconfirmed debit was not compensated.");
        var wrong = housing.Begin(Request("wrong-revision",12,4) with { ExpectedContainerRevision=5 },now);
        try { housing.Reconcile("wrong-revision",new FailingStore(new LedgerStoreUnavailableException("offline",new Exception()))); throw new Exception("Ledger outage accepted."); }
        catch (HousingUnavailableException) { }
        try { housing.Reconcile("wrong-revision",new FailingStore(new LedgerRejectedException("rejected"))); throw new Exception("Ledger rejection accepted."); }
        catch (HousingRejectedException) { }
        Check(housing.Placement("wrong-revision")?.State == "Pending","Ledger error changed placement.");
        var injected = new InMemoryInventoryLedgerStore();
        injected.Execute(s => { s.Operations.Add("wrong-revision/consume",new StoredOperation("fixture",new LedgerReceipt(
            "wrong-revision/consume",LedgerAction.Consume,[new RevisionChange("ctn_bag",4,5)],
            [new QuantityChange("ctn_bag",wrong.MaterialId,-1)]))); return 0; });
        try { housing.Reconcile("wrong-revision",injected); throw new Exception("Wrong container revision accepted."); }
        catch (HousingRejectedException) { }
        Check(housing.Placement("wrong-revision")?.State == "Pending","Mismatched receipt changed placement.");
        housing.Reconcile("wrong-revision",ledgerStore);
        Check(housing.Begin(Request("three",2,4) with { ExpectedContainerRevision=5 },now).State == "Compensated","Compensated retry changed state.");
        try { ledger.Consume("three/consume","ctn_bag",5,noDebit.MaterialId,1,"construction_material_consumed"); throw new Exception("Late consume accepted."); }
        catch (LedgerRejectedException) { }
        Check(ledger.Read("ctn_bag").Quantities["item/foundation_material_bundle"] == 6,"Late debit or guessed refund broke conservation.");
        Check(housing.Reconcile("three",ledgerStore).State == "Compensated","Compensated replay changed state.");
        Console.WriteLine("PASS restart, two same-revision commits, fenced compensation and conservation");
        break;
    case "concurrent":
        var wins = 0;
        var ready = new CountdownEvent(2);
        var release = new ManualResetEventSlim();
        var workers = new[]{"left","right"}.Select(id => Task.Run(() => {
            var worker = new PostgresHousingStore(connection);
            ready.Signal(); release.Wait();
            try { if (worker.Begin(Request(id,3,4) with { ExpectedContainerRevision=5 },now).State == "Pending") Interlocked.Increment(ref wins); }
            catch (HousingRejectedException) { }
        })).ToArray();
        Check(ready.Wait(TimeSpan.FromSeconds(10)),"Workers did not reach the barrier.");
        release.Set(); Task.WaitAll(workers);
        ready.Dispose(); release.Dispose();
        Check(wins == 1,"Same-cell contention had more than one winner.");
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
        try { housing.Begin(Request("denied",4,4) with { ActorId="intruder", ExpectedContainerRevision=5 },now); throw new Exception("Unauthorized build accepted."); }
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
