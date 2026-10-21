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
Check(QuestValidator.ValidateAll([("a", quest), ("b", quest)]).Any(i => i.Contains("duplicate quest id", StringComparison.Ordinal)), "duplicate record");

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
