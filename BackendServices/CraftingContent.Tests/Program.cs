using ShatteredFrontier.CraftingContent;
using ShatteredFrontier.InventoryLedger;

var path = args.Length == 1 ? args[0] : "content/crafting/recipes_greenlands_starter.json";
var catalog = RecipeCatalog.LoadFiles([path]);
Check(catalog.Recipes.Count == 9, "starter recipe count");
var store = new InMemoryInventoryLedgerStore();
var ledger = new InventoryLedger(store);
ledger.CreateContainer(new ContainerSpec("ctn_craft_test", OwnerKind.Character, "chr_test", ContainerKind.Backpack, 1000));
ledger.Grant("grant_log", "ctn_craft_test", 0, "item/oak_log", 1, "gathering_output", "hrv_test");
var service = new CraftAttemptService(catalog, ledger);
var attempt = new CraftAttempt("crf_test", "ctn_craft_test", 1, "recipe/rough_timber", "sawmill", 1);
var first = service.Execute(attempt);
Check(ReferenceEquals(first, service.Execute(attempt)), "retry returns stored receipt");
Check(ledger.Read("ctn_craft_test").Quantities.GetValueOrDefault("item/rough_timber") == 4, "committed output");
Check(ledger.Read("ctn_craft_test").Revision == 2, "single commit");
Reject(() => service.Execute(attempt with { Count = 2 }), "conflicting retry");
Reject(() => service.Execute(new CraftAttempt("crf_missing", "ctn_craft_test", 2, "recipe/iron_ingot", "forge", 1)), "missing input");
Check(ledger.Read("ctn_craft_test").Revision == 2, "rejection did not mutate");
Reject(() => service.Execute(attempt with { OperationId = "crf_wrong", Station = "forge" }), "wrong station");

var json = File.ReadAllText(path);
void Bad(string replacement, string label)
{
    var temp = Path.GetTempFileName();
    try { File.WriteAllText(temp, replacement); Reject(() => RecipeCatalog.LoadFiles([temp]), label); }
    finally { File.Delete(temp); }
}
Bad(json.Replace("\"station\": \"sawmill\"", "\"station\": \"invalid\"", StringComparison.Ordinal), "station validation");
Bad(json.Replace("\"duration_seconds\": 5", "\"duration_seconds\": 0", StringComparison.Ordinal), "duration validation");
Bad(json.Replace("\"quantity\": 1", "\"quantity\": 0", StringComparison.Ordinal), "quantity validation");
Bad(json.Replace("\"recipe/iron_ingot\"", "\"recipe/rough_timber\"", StringComparison.Ordinal), "unique IDs");
Bad(json.Replace("\"item/boar_hide\", \"quantity\": 1", "\"item/starter_tool_repair_kit\", \"quantity\": 1", StringComparison.Ordinal), "cycle validation");
Console.WriteLine("CraftingContent tests passed.");

static void Check(bool okay, string label) { if (!okay) throw new Exception(label); }
static void Reject(Action action, string label)
{
    try { action(); } catch (Exception ex) when (ex is CraftContentException or LedgerRejectedException) { return; }
    throw new Exception($"Expected rejection: {label}");
}
