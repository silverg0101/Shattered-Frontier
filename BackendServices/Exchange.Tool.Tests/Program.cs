var content = MarketCatalog.Load(Path.Combine("content", "markets"));
static void Check(bool okay, string name) { if (!okay) throw new Exception(name); }
Check(content.HasOpenMarket("market/silverbrook"), "Silverbrook open");
Check(!content.HasOpenMarket("market/eastford"), "unknown market rejected");
Check(content.FairValue("market/silverbrook", "item/rough_timber") == 6, "common grade");
Check(content.FairValue("market/silverbrook", "item/rough_timber/crude") == 3, "crude grade");
Check(content.FairValue("market/silverbrook", "item/rough_timber/fine") == 9, "fine grade");
Check(content.FairValue("market/silverbrook", "item/rough_timber/superior") == 15, "superior grade");
Check(content.FairValue("market/silverbrook", "item/rough_timber/masterwork") == 24, "masterwork grade");
Check(content.FairValue("market/silverbrook", "item/iron_axe") is null, "gear refused");
Check(content.FairValue("market/silverbrook", "item/oak_log/fine") is null, "raw grade refused");
Check(content.FairValue("market/eastford", "item/rough_timber") is null, "market isolation");
var temp = Path.Combine(Path.GetTempPath(), "market-content-test-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(temp);
try
{
    File.WriteAllText(Path.Combine(temp, "duplicate.json"), """{"id":"market/test","zone_id":"zone/test","board":"market_board/test","open":true,"grade_multipliers":{"crude":0.5,"fine":1.5,"superior":2.5,"masterwork":4.0},"items":[{"definition_id":"item/log","fair_value_copper":1},{"definition_id":"item/log","fair_value_copper":2}]}""");
    try { _ = MarketCatalog.Load(temp); throw new Exception("Duplicate item accepted"); }
    catch (FormatException) { }
    File.WriteAllText(Path.Combine(temp, "duplicate.json"), File.ReadAllText(Path.Combine("content", "markets", "silverbrook.json")).Replace("\"fine\": 1.5", "\"fine\": 3.0"));
    try { _ = MarketCatalog.Load(temp); throw new Exception("Changed grade multiplier accepted"); }
    catch (FormatException) { }
}
finally { Directory.Delete(temp, true); }
Console.WriteLine("PASS market content, grade values, denials and duplicate validation");
