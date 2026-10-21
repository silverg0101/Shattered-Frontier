using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;

static void Check(bool ok, string name)
{
    if (!ok) throw new Exception(name);
    Console.WriteLine("PASS " + name);
}
var connection = args[1];
var ledger = new InventoryLedger(new PostgresInventoryLedgerStore(connection));
const string id = "ctn_durable";
const string item = "item/ore";
const string bar = "item/bar";
switch (args[0])
{
    case "seed":
        ledger.CreateContainer(new(id, OwnerKind.Character, "chr_durable", ContainerKind.Backpack, 100));
        var grant = ledger.Grant("grant-1", id, 0, item, 6, "fixture", "fixture-1");
        Check(grant.Revisions[0].After == 1 && ledger.Read(id).Quantities[item] == 6, "grant acknowledged durably");
        break;
    case "restart":
        Check(ledger.Read(id).Quantities[item] == 6, "grant survives process and database restart");
        var retry = ledger.Grant("grant-1", id, 0, item, 6, "fixture", "fixture-1");
        Check(retry.Revisions[0].Before == 0 && retry.Revisions[0].After == 1 && ledger.Read(id).Revision == 1, "reply-lost retry returns original receipt");
        var converted = ledger.Convert("convert-1", id, 1, "recipe/bar", [new(item, 2)], [new(bar, 1)]);
        Check(converted.Revisions[0].After == 2, "convert acknowledged durably");
        break;
    case "concurrent":
        Check(ledger.Read(id).Quantities[item] == 4 && ledger.Read(id).Quantities[bar] == 1, "convert survives process and database restart");
        var tasks = new[] { "race-a", "race-b" }.Select(op => Task.Run(() =>
        {
            try { return ledger.Consume(op, id, 2, item, 1, "fixture") is not null; }
            catch (LedgerRejectedException) { return false; }
        })).ToArray();
        Task.WaitAll(tasks);
        Check(tasks.Count(t => t.Result) == 1 && ledger.Read(id).Revision == 3 && ledger.Read(id).Quantities[item] == 3, "one concurrent revision winner");
        break;
    case "outage":
        try { ledger.Grant("outage-1", id, 3, item, 1, "fixture", "fixture-2"); throw new Exception("outage accepted mutation"); }
        catch (LedgerStoreUnavailableException) { Console.WriteLine("PASS outage fails closed with retryable error"); }
        break;
    case "recover":
        Check(ledger.Read(id).Revision == 3 && ledger.Read(id).Quantities[item] == 3, "outage left no mutation");
        var recovered = ledger.Grant("outage-1", id, 3, item, 1, "fixture", "fixture-2");
        Check(recovered.Revisions[0].After == 4 && ledger.Read(id).Quantities[item] == 4, "same operation recovers");
        break;
    default: throw new ArgumentException("Unknown test phase.");
}
