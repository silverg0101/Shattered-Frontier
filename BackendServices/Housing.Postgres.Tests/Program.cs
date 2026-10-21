using ShatteredFrontier.Housing.Postgres;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;
using Ledger = ShatteredFrontier.InventoryLedger.InventoryLedger;

var connection = Environment.GetEnvironmentVariable("HOUSING_CONNECTION") ?? throw new Exception("Missing HOUSING_CONNECTION.");
var housing = new PostgresHousingStore(connection);
var ledgerStore = new PostgresInventoryLedgerStore(connection);
var ledger = new Ledger(ledgerStore);
var phase = args.Single();
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
        var first = housing.Begin(Request("one",0,0));
        Check(first.State == "Pending","First prepare failed.");
        ledger.Consume("one/consume","ctn_bag",1,first.MaterialId,1,"construction_material_consumed");
        Check(housing.Reconcile("one",ledgerStore).State == "Committed","First commit failed.");
        var second = housing.Begin(Request("two",1,1));
        ledger.Consume("two/consume","ctn_bag",2,second.MaterialId,1,"construction_material_consumed");
        Console.WriteLine("PASS seed, acknowledged commit and interrupted consume");
        break;
    case "restart":
        Check(housing.Claim("clm_test")?.OwnerId == "chr_owner","Claim lost on restart.");
        Check(housing.Structures("clm_test").Count == 1,"Committed structure lost on restart.");
        Check(housing.Reconcile("two",ledgerStore).State == "Committed","Confirmed debit was not committed on recovery.");
        Check(housing.Reconcile("two",ledgerStore).Revision == 2,"Recovery replay changed revision.");
        Check(housing.Begin(Request("two",1,1)).State == "Committed","Retry did not return the same decision.");
        var noDebit = housing.Begin(Request("three",2,2));
        Check(noDebit.State == "Pending","Third prepare failed.");
        Check(housing.Reconcile("three",ledgerStore).State == "Compensated","Unconfirmed debit was not compensated.");
        Check(ledger.Read("ctn_bag").Quantities["item/foundation_material_bundle"] == 8,"Recovery guessed a refund.");
        Console.WriteLine("PASS immediate-stop replay, exact receipt, no guessed refund");
        break;
    case "concurrent":
        var wins = 0;
        Parallel.ForEach(new[]{"left","right"},id => {
            try { if (housing.Begin(Request(id,3,2)).State == "Pending") Interlocked.Increment(ref wins); }
            catch (HousingRejectedException) { }
        });
        Check(wins == 1,"Same-cell contention had more than one winner.");
        try { housing.Begin(Request("denied",4,2) with { ActorId="intruder" }); throw new Exception("Unauthorized build accepted."); }
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
