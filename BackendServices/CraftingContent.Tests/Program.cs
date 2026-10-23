using ShatteredFrontier.CraftingContent;
using ShatteredFrontier.InventoryLedger;

var path = args.Length == 1 ? args[0] : "content/crafting/recipes_greenlands_starter.json";
var catalog = RecipeCatalog.LoadFiles([path]);
Check(catalog.Recipes.Count == 21, "starter, learned and gear recipe count");
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

var controls = new CraftingControls(); controls.Validate();
Check(controls.TrainerFee(250) == 125, "trainer fee rises with difficulty");
Check(CraftingQuality.Score(20, 20, 37, 0, controls) == 29, "low skill formula");
Check(CraftingQuality.Score(220, 20, 37, 0, controls) == 62, "high skill formula");
Check(CraftingQuality.Grade(24) == CraftGrade.Crude && CraftingQuality.Grade(25) == CraftGrade.Common &&
    CraftingQuality.Grade(89) == CraftGrade.Superior && CraftingQuality.Grade(90) == CraftGrade.Masterwork, "grade bounds");
Check(CraftingQuality.ParseGrade("item/wall_material_bundle", "item/wall_material_bundle/crude") == CraftGrade.Crude,
    "construction material accepts graded definition");
Check(CraftingQuality.Grade(CraftingQuality.Score(0, 250, 37, 5, controls)) == CraftGrade.Crude, "below skill crude");
var ingot = catalog.Get("recipe/iron_ingot");
Check(CraftingQuality.Discover(ingot, 150, 100, 0, 0, new HashSet<string>(), catalog,
    new CraftingControls(DiscoveryBase: 1)) == "recipe/iron_ingot_tempered", "forced discovery");
Check(CraftingQuality.Discover(ingot, 150, 100, 0, 0, new HashSet<string>(), catalog,
    new CraftingControls(DiscoveryBase: 0)) is null, "disabled discovery");
Check(CraftingQuality.Discover(ingot, 150, 100, 0, 0,
    new HashSet<string> { "recipe/iron_ingot_tempered" }, catalog, new CraftingControls(DiscoveryBase: 1)) is null,
    "already discovered stays known once");
var secret = System.Text.Encoding.UTF8.GetBytes("synthetic-test-only-secret-1234567890");
var roll = CraftingQuality.Roll("crf_quality", secret, controls);
Check(roll == CraftingQuality.Roll("crf_quality", secret, controls), "quality roll retry");
var gradedStore = new InMemoryInventoryLedgerStore(); var gradedLedger = new InventoryLedger(gradedStore);
gradedLedger.CreateContainer(new ContainerSpec("ctn_grade", OwnerKind.Character, "chr_test", ContainerKind.Backpack, 1000));
gradedLedger.Grant("grade_seed", "ctn_grade", 0, "item/iron_ingot/superior", 1, "test", "test");
var graded = new CraftAttemptService(catalog, gradedLedger);
var gradeAttempt = new CraftAttempt("grade_craft", "ctn_grade", 1, "recipe/iron_nails", "forge", 1);
var selected = new Dictionary<string, string> { ["item/iron_ingot"] = "item/iron_ingot/superior" };
Check(CraftingQuality.MaterialMidpoint(catalog.Get("recipe/iron_nails"), selected) == 82 &&
    CraftingQuality.MaterialMidpoint(catalog.Get("recipe/iron_nails"), new Dictionary<string, string>()) == 37,
    "selected material grade changes quality input");
var made = graded.ExecuteGraded(gradeAttempt, 220, selected, secret, controls);
Check(gradedLedger.Read("ctn_grade").Quantities.GetValueOrDefault("item/iron_ingot/superior") == 0, "selected grade consumed");
Check(gradedLedger.Read("ctn_grade").Quantities.GetValueOrDefault(CraftingQuality.DefinitionId("item/iron_nails", made.Grade)) == 8, "graded output");
Check(ReferenceEquals(made.Receipt, graded.ExecuteGraded(gradeAttempt, 220, selected, secret, controls).Receipt), "graded retry");
Check(CraftingQuality.StoredScore(gradedStore.Execute(s => s.Operations.TryGetValue("grade_craft", out var stored) ? stored.Fingerprint : "")) == made.QualityScore,
    "stored quality survives skill or secret changes");
Check(CraftingQuality.StoredDiscovery(gradedStore.Execute(s => s.Operations.TryGetValue("grade_craft", out var stored) ? stored.Fingerprint : "")) is null,
    "stored no-discovery decision survives replay");
Reject(() => graded.ExecuteGraded(gradeAttempt, 220, new Dictionary<string, string>(), secret, controls), "grade conflict");

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
Bad(json.Replace("\"item/boar_hide\"", "\"item/starter_tool_repair_kit\"", StringComparison.Ordinal), "cycle validation");
Bad(json.Replace("\"skill\": \"carpentry\"", "\"skill\": \"alchemy\"", StringComparison.Ordinal), "skill validation");
Bad(json.Replace("\"weight\": 1", "\"weight\": 101", StringComparison.Ordinal), "discovery weight");
Console.WriteLine("CraftingContent tests passed.");

static void Check(bool okay, string label) { if (!okay) throw new Exception(label); }
static void Reject(Action action, string label)
{
    try { action(); } catch (Exception ex) when (ex is CraftContentException or LedgerRejectedException) { return; }
    throw new Exception($"Expected rejection: {label}");
}
