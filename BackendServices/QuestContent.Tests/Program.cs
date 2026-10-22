using System.Text.Json;
using ShatteredFrontier.QuestContent;

var quest = QuestLoader.Load(args.Single());
void Check(bool pass, string name) { if (!pass) throw new Exception(name); Console.WriteLine("PASS " + name); }
QuestDefinition Copy() => JsonSerializer.Deserialize<QuestDefinition>(JsonSerializer.Serialize(quest, QuestLoader.JsonOptions), QuestLoader.JsonOptions)!;
void Broken(string name, Action<QuestDefinition> mutate, string expected)
{
    var copy = Copy(); mutate(copy);
    Check(QuestValidator.Validate(copy).Any(i => i.Contains(expected, StringComparison.Ordinal)), name);
}
Check(QuestValidator.Validate(quest).Count == 0, "authored quest validates");
Broken("bad quest id", q => q.Id = "bad", "quest id");
Broken("duplicate step", q => q.Steps[1].Id = q.Steps[0].Id, "duplicate");
Broken("forward prerequisite", q => q.Steps[0].Prerequisites.Add("defeat_boar"), "prerequisites");
Broken("bad target", q => q.Steps[4].Objectives[0].TargetId = "item/boar_hide", "kind/target");
Broken("bad node", q => q.Steps[0].Objectives[0].NodeId = "npc/boar", "node_id");
Broken("zero count", q => q.Steps[0].Objectives[0].Count = 0, "count");
Check(QuestValidator.Validate(quest, new QuestTuning(1)).Count == 0, "bounded count setting");
var overBound = Copy(); overBound.Steps[0].Objectives[0].Count = 2;
Check(QuestValidator.Validate(overBound, new QuestTuning(1)).Any(i => i.Contains("count outside", StringComparison.Ordinal)), "effective count limit");
try { new QuestTuning(0).Validate(); throw new Exception("invalid tuning accepted"); }
catch (ArgumentOutOfRangeException) { Console.WriteLine("PASS tuning bounds"); }
Broken("duplicate objective", q => q.Steps[2].Objectives[1].Key = q.Steps[2].Objectives[0].Key, "duplicate");
Broken("bad reward", q => q.Steps[4].Rewards[0].BundleId = "item/boar_hide", "rewards");
var emptyReward = Copy(); emptyReward.Steps[4].Rewards[0].Items.Clear();
Check(QuestValidator.Validate(emptyReward, knownItems: new HashSet<string>()).Any(i => i.Contains("reward items", StringComparison.Ordinal)), "empty authored reward lines rejected");
Broken("duplicate reward line", q => q.Steps[4].Rewards[0].Items.Add(q.Steps[4].Rewards[0].Items[0]), "reward items");
Broken("zero reward quantity", q => q.Steps[4].Rewards[0].Items[0].Quantity = 0, "reward items");
var unknownReward = Copy(); unknownReward.Steps[4].Rewards[0].Items[0].DefinitionId = "item/invented";
Check(QuestValidator.Validate(unknownReward, knownItems: new HashSet<string> { "item/rough_timber", "item/iron_nails", "item/starter_tool_repair_kit" }).Any(i => i.Contains("unknown", StringComparison.Ordinal)), "unknown reward item rejected");
Check(QuestValidator.ValidateAll([("a", quest), ("b", quest)]).Any(i => i.Contains("duplicate quest id", StringComparison.Ordinal)), "duplicate record");
var talk = Copy(); talk.Steps[0].Objectives[0] = new QuestObjective { Key = "meet", Kind = "talk_resident", TargetId = "resident_role/stall_keeper", Count = 1 };
var roles = new HashSet<string> { "stall_keeper" };
Check(QuestValidator.Validate(talk, knownRoles: roles).Count == 0, "talk_resident with a census role validates");
Check(QuestValidator.Validate(talk, knownRoles: new HashSet<string> { "smith" }).Any(i => i.Contains("unknown resident role", StringComparison.Ordinal)), "unknown resident role rejected");
talk.Steps[0].Objectives[0].TargetId = "npc/stall_keeper";
Check(QuestValidator.Validate(talk, knownRoles: roles).Any(i => i.Contains("kind/target", StringComparison.Ordinal)), "talk_resident needs a resident_role target");

var model = new QuestProgression(quest);
var t = DateTimeOffset.Parse("2026-10-21T12:00:00Z");
Check(model.Accept("chr_a", t) && !model.Accept("chr_a", t), "accept once");
ObjectiveEvent E(string op, string step, string kind, string target, int seconds, string? node = null, int quantity = 1) =>
    new(op, "chr_a", quest.Id, step, kind, target, node, quantity, t.AddSeconds(seconds));
Check(!model.Apply(E("early", "mine_iron", "gather", "item/iron_ore", 1)).Accepted, "future step ignored");
Check(!model.Apply(E("before", "harvest_oak", "gather", "item/oak_log", -1, "gathering_node/oak_tree")).Accepted, "preaccept ignored");
var oak = E("oak", "harvest_oak", "gather", "item/oak_log", 2, "gathering_node/oak_tree");
Check(model.Apply(oak).ActiveStepId == "mine_iron", "oak advances");
Check(!model.Apply(oak).Accepted, "old step replay rejected");
Check(!model.Apply(E("wrong-node", "mine_iron", "gather", "item/oak_log", 3)).Accepted, "wrong target rejected");
Check(model.Apply(E("iron", "mine_iron", "gather", "item/iron_ore", 4)).ActiveStepId == "refine_materials", "iron advances");
Check(!model.Apply(E("late", "refine_materials", "craft", "item/rough_timber", 3)).Accepted, "out of order timestamp rejected");
Check(model.Apply(E("timber", "refine_materials", "craft", "item/rough_timber", 5)).ActiveStepId == "refine_materials", "partial craft");
Check(!model.Apply(E("timber", "refine_materials", "craft", "item/rough_timber", 6)).Accepted, "same operation replay rejected");
Check(model.Apply(E("ingot", "refine_materials", "craft", "item/iron_ingot", 7)).ActiveStepId == "place_wall", "craft advances");
Check(model.Apply(E("wall", "place_wall", "build", "structure_tag/wall", 8)).ActiveStepId == "defeat_boar", "build advances");
var result = model.Apply(E("boar", "defeat_boar", "defeat", "npc/boar", 9));
Check(result.Accepted && result.Completed && result.Grants.Count == 1 && result.Grants[0].BundleId == "reward/frontier_founder_completion", "completion grants once");
Check(!model.Apply(E("boar", "defeat_boar", "defeat", "npc/boar", 9)).Accepted &&
    model.Apply(E("another", "defeat_boar", "defeat", "npc/boar", 10)).Grants.Count == 0, "terminal replays never grant");
Check(model.Accept("chr_b", t) && model.Apply(new ObjectiveEvent("oak", "chr_b", quest.Id, "harvest_oak", "gather", "item/oak_log", "gathering_node/oak_tree", 1, t.AddSeconds(1))).Accepted,
    "operation scope is per character");

// QST-02: offers, talk_npc, instances
var offered = Copy(); offered.Offer = new QuestOffer { Title = "T", Summary = "S", Boards = ["board/silverbrook"], GiverNpc = "npc/oswin", RequiresCompleted = ["quest/other"] };
Check(QuestValidator.Validate(offered).Count == 0 && QuestValidator.Validate(offered, knownNpcs: new HashSet<string> { "oswin" }).Count == 0, "offer validates");
void BrokenOffer(string name, Action<QuestOffer> mutate, string expected)
{
    var copy = Copy(); copy.Offer = new QuestOffer { Title = "T", Summary = "S", Boards = ["board/silverbrook"], GiverNpc = "npc/oswin" }; mutate(copy.Offer);
    Check(QuestValidator.Validate(copy, knownNpcs: new HashSet<string> { "oswin" }).Any(i => i.Contains(expected, StringComparison.Ordinal)), name);
}
BrokenOffer("offer without board or giver rejected", o => { o.Boards = []; o.GiverNpc = null; }, "unreachable");
BrokenOffer("more than 8 boards rejected", o => o.Boards = [.. Enumerable.Range(0, 9).Select(i => "board/b" + i)], "boards invalid");
BrokenOffer("duplicate board rejected", o => o.Boards = ["board/a", "board/a"], "boards invalid");
BrokenOffer("unknown giver npc rejected", o => o.GiverNpc = "npc/ghost", "unknown npc");
BrokenOffer("bad giver id rejected", o => o.GiverNpc = "oswin", "giver_npc invalid");
BrokenOffer("self prerequisite rejected", o => o.RequiresCompleted = [quest.Id], "requires_completed");
BrokenOffer("empty title rejected", o => o.Title = " ", "title/summary");
var talkNpc = Copy(); talkNpc.Steps[0].Objectives[0] = new QuestObjective { Key = "oswin", Kind = "talk_npc", TargetId = "npc/oswin", Count = 1 };
Check(QuestValidator.Validate(talkNpc, knownNpcs: new HashSet<string> { "oswin" }).Count == 0, "talk_npc with known npc validates");
Check(QuestValidator.Validate(talkNpc, knownNpcs: new HashSet<string> { "other" }).Any(i => i.Contains("unknown npc", StringComparison.Ordinal)), "talk_npc with unknown npc rejected");
var chained = Copy(); chained.Id = "quest/chained"; chained.Offer = new QuestOffer { Title = "T", Summary = "S", Boards = ["board/x"], RequiresCompleted = ["quest/missing"] };
Check(QuestValidator.ValidateAll([("a", quest), ("b", chained)]).Any(i => i.Contains("unknown quest quest/missing", StringComparison.Ordinal)), "requires_completed must name a known quest");
var withGiver = Copy(); withGiver.Offer = new QuestOffer { Title = "T", Summary = "S", GiverNpc = "npc/oswin" };
Check(QuestValidator.ValidateDialogueOffers([("oswin", quest.Id)], [withGiver]).Count == 0, "dialogue offer names its giver");
Check(QuestValidator.ValidateDialogueOffers([("other", quest.Id)], [withGiver]).Count == 1, "dialogue offer for a different npc rejected");
Check(QuestValidator.ValidateDialogueOffers([("oswin", "quest/none")], [withGiver]).Count == 1, "dialogue offer for unknown quest rejected");
var talkModel = new QuestProgression(talkNpc); talkModel.Accept("chr_t", t);
var talked = talkModel.Apply(new ObjectiveEvent("talk1", "chr_t", talkNpc.Id, talkNpc.Steps[0].Id, "talk_npc", "npc/oswin", null, 1, t.AddSeconds(1)));
Check(talked.Accepted && talked.ActiveStepId == talkNpc.Steps[1].Id, "talk_npc evidence advances");
var inst = new QuestProgression(quest); inst.Accept("chr_i", t, 2);
var instResult = inst.Apply(new ObjectiveEvent("i1", "chr_i", quest.Id, "harvest_oak", "gather", "item/oak_log", "gathering_node/oak_tree", 1, t.AddSeconds(1)));
Check(inst.Snapshot("chr_i")!.Instance == 2 && instResult.Accepted, "instance carried through snapshot");
var rest = new QuestProgression(quest); rest.Restore("chr_i", inst.Snapshot("chr_i")!);
Check(rest.Snapshot("chr_i")!.Instance == 2, "instance restored");
var multi = Copy(); multi.Steps[0].Rewards.Add(new QuestReward { BundleId = "reward/x" });
foreach (var (n, id) in new[] { (1, ""), (3, ":i3") })
{
    var m = new QuestProgression(multi); m.Accept("chr_g", t, n);
    var g = m.Apply(new ObjectiveEvent("g", "chr_g", multi.Id, "harvest_oak", "gather", "item/oak_log", "gathering_node/oak_tree", multi.Steps[0].Objectives[0].Count, t.AddSeconds(1)));
    Check(g.Grants.Single().OperationId == $"quest:chr_g:{multi.Id}:harvest_oak:reward/x{id}", "grant id for instance " + n);
}
