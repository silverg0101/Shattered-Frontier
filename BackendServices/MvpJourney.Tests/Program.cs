using ClaimTerritory;
using ShatteredFrontier.Exchange;
using ShatteredFrontier.GatheringNodes;
using ShatteredFrontier.MvpJourney;
using ShatteredFrontier.Progression;
using ShatteredFrontier.Rewards;

static void Check(bool ok, string message) { if (!ok) throw new Exception(message); }
var clock = new FixedClock();
var journey = new Journey("content/world/zones/greenlands_silverbrook.json",
    "content/quests/greenlands_frontier_founder.json", ["content/crafting/recipes_greenlands_starter.json"], clock,
    [new("gathering_node/oak_tree", ResourceKind.Wood, "item/oak_log", 2, 1),
     new("gathering_node/iron_vein", ResourceKind.Ore, "item/iron_ore", 2, 1)], "silverbrook-journey",
    "content/loot/greenlands_starter.json", () => 0);
journey.RegisterCharacter("chr_ada", 100);
journey.RegisterCharacter("chr_ben", 100);
var replay = new List<Func<JourneyResult>>();
var expectedSkills = new Dictionary<string, int> { [ProgressionRules.Logging] = 0,
    [ProgressionRules.Mining] = 0, [ProgressionRules.Smithing] = 0 };
long expectedXp = 0;
void Practice(JourneyResult result, string skill)
{
    int before = expectedSkills[skill];
    var gain = result.SkillResults?.Single() ?? throw new Exception("missing practice receipt");
    var settings = new ProgressionSettings();
    double chance = ProgressionRules.GainChance(before, 25, 6, true, 1, 1, 1, settings);
    int xp = (int)Math.Round(Math.Round(ProgressionRules.XpToNext(1) / 12d, MidpointRounding.AwayFromZero) * .012 *
        ProgressionRules.DifficultyFactor(25 - before), MidpointRounding.AwayFromZero);
    Check(gain.Applied && gain.SkillGain == 1 && gain.XpGain == xp && Math.Abs(gain.GainChance - chance) < 1e-12,
        "skill gain follows Progression formula: " + result.OperationId);
    expectedSkills[skill]++;
    expectedXp += xp;
}
void Run(Func<JourneyResult> command, Action<JourneyResult>? verify = null)
{
    var result = command();
    verify?.Invoke(result);
    replay.Add(() => { Check(ReferenceEquals(result, command()), "replay receipt identity: " + result.OperationId); return result; });
}
var wood = journey.Nodes.Snapshot().First(n => n.Kind == ResourceKind.Wood);
var iron = journey.Nodes.Snapshot().Where(n => n.Kind == ResourceKind.Ore).Take(2).ToArray();
Check(iron.Length == 2, "authored iron nodes");
Run(() => journey.Harvest("oak-1", "chr_ada", wood.Id, wood.Generation), r => { Check(r.Progress?.Accepted == true, "oak objective"); Practice(r, ProgressionRules.Logging); });
Run(() => journey.Harvest("oak-2", "chr_ada", wood.Id, wood.Generation), r => Practice(r, ProgressionRules.Logging));
Run(() => journey.Harvest("iron-1", "chr_ada", iron[0].Id, iron[0].Generation), r => { Check(r.Progress?.Accepted == true, "iron objective"); Practice(r, ProgressionRules.Mining); });
Run(() => journey.Harvest("iron-2", "chr_ada", iron[1].Id, iron[1].Generation), r => Practice(r, ProgressionRules.Mining));
Run(() => journey.Craft("timber", "chr_ada", "recipe/rough_timber", 2), r => { Check(r.Progress?.Accepted == true, "timber objective"); Practice(r, ProgressionRules.Smithing); });
Run(() => journey.Craft("ingot", "chr_ada", "recipe/iron_ingot", 2), r => { Check(r.Progress?.Accepted == true, "ingot objective"); Practice(r, ProgressionRules.Smithing); });
Run(() => journey.Craft("nails", "chr_ada", "recipe/iron_nails"), r => Practice(r, ProgressionRules.Smithing));
Run(() => journey.Craft("wall-bundle", "chr_ada", "recipe/wall_material_bundle"), r => Practice(r, ProgressionRules.Smithing));
Run(() => journey.Claim("claim", "chr_ada", "clm_ada", new Point(900, 900)), r => Check(r.Claim?.Accepted == true, "claim placed"));
Run(() => journey.PlaceWall("wall", "chr_ada", "clm_ada"), r => Check(r.Progress?.Accepted == true, "wall objective"));
int expectedHides = BoarRewardService.Roll("boar:chr_ada", journey.BoarLoot);
Run(() => journey.DefeatBoar("boar", "chr_ada"), r =>
{
    Check(r.Progress?.Completed == true && r.Progress.Grants.Count == 1, "one quest reward");
    Check(r.Items?.OperationId == "defeat:4:boar:chr_ada" && r.Items.Changes.Single(c => c.DefinitionId == "item/boar_hide").Delta == expectedHides,
        "hide grant comes from deterministic loot roll");
    var defeat = r.SkillResults?.Single() ?? throw new Exception("missing defeat practice");
    int xp = (int)Math.Round(ProgressionRules.XpToNext(1) / 150d, MidpointRounding.AwayFromZero);
    Check(defeat.Applied && defeat.SkillGain == 0 && defeat.XpGain == xp, "defeat XP formula");
    expectedXp += xp;
});
var offerA = new TradeOffer("chr_ada", new(Journey.Bag("chr_ada"), Journey.Escrow("chr_ada"),
    Journey.Bag("chr_ben"), "item/rough_timber", 1), 0);
var offerB = new TradeOffer("chr_ben", null, 0);
Run(() => journey.TradeOpen("trade-open", "trd_silverbrook", offerA, offerB), r => Check(r.Trade?.Accepted == true, "trade open"));
Run(() => journey.TradeLock("trade-lock-a", "trd_silverbrook", "chr_ada"), r => Check(r.Trade?.Accepted == true, "seller lock"));
Run(() => journey.TradeLock("trade-lock-b", "trd_silverbrook", "chr_ben"), r => Check(r.Trade?.Accepted == true, "buyer lock"));
Run(() => journey.TradeAccept("trade-accept-a", "trd_silverbrook", "chr_ada"), r => Check(r.Trade?.Accepted == true, "seller accept"));
Run(() => journey.TradeAccept("trade-accept-b", "trd_silverbrook", "chr_ben"), r => Check(r.Trade?.Accepted == true, "buyer accept"));
Check(journey.Exchange.TradeState("trd_silverbrook") == "completed", "settled trade");
var ids = new[] { "chr_ada", "chr_ben" };
long Sum(string item) => ids.Sum(c => journey.Items.Read(Journey.Bag(c)).Quantities.GetValueOrDefault(item) +
    journey.Items.Read(Journey.Escrow(c)).Quantities.GetValueOrDefault(item));
Check(Sum("item/oak_log") == 0 && Sum("item/iron_ore") == 0 && Sum("item/rough_timber") == 2 &&
    Sum("item/iron_ingot") == 1 && Sum("item/iron_nails") == 4 && Sum("item/wall_material_bundle") == 0 &&
    Sum("item/frontier_founder_completion_bundle") == 1 && Sum("item/boar_hide") == expectedHides,
    "material conservation across sources, conversions, build sink and rewards");
Check(journey.Items.Read(Journey.Bag("chr_ben")).Quantities.GetValueOrDefault("item/rough_timber") == 1, "trade delivery");
var ada = journey.Characters["chr_ada"];
Check(ada.LifetimeXp == expectedXp && expectedSkills.All(pair => ada.Skills[pair.Key] == pair.Value), "progression totals");
var bagRevision = journey.Items.Read(Journey.Bag("chr_ada")).Revision;
foreach (var command in replay) command();
Check(Sum("item/frontier_founder_completion_bundle") == 1 && Sum("item/rough_timber") == 2 &&
    Sum("item/boar_hide") == expectedHides && journey.Items.Read(Journey.Bag("chr_ada")).Revision == bagRevision &&
    ada.LifetimeXp == expectedXp && expectedSkills.All(pair => ada.Skills[pair.Key] == pair.Value) &&
    journey.Exchange.TradeState("trd_silverbrook") == "completed", "replay leaves items and progression unchanged");
try { journey.Harvest("oak-1", "chr_ben", wood.Id, wood.Generation); throw new Exception("conflicting operation accepted"); }
catch (InvalidOperationException) { }
Console.WriteLine($"MvpJourney: {replay.Count} commands replayed; quest reward once; trade settled; inventory conserved.");

sealed class FixedClock : TimeProvider
{
    public override DateTimeOffset GetUtcNow() => new(2026, 10, 21, 12, 0, 0, TimeSpan.Zero);
}
