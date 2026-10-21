using ShatteredFrontier.CharacterState.Postgres;
using ShatteredFrontier.Progression;
using ShatteredFrontier.QuestContent;
using Npgsql;

static void Check(bool value, string label) { if (!value) throw new Exception(label); Console.WriteLine("PASS " + label); }
var connection = Environment.GetEnvironmentVariable("CHARACTER_STATE_CONNECTION") ?? throw new Exception("Missing fixture connection.");
var store = new PostgresCharacterStateStore(connection);
var quest = new QuestDefinition { Id = "quest/durable", Revision = 1, ZoneId = "zone/test", Steps = [new QuestStep { Id = "first", Objectives = [new QuestObjective { Key = "ore", Kind = "gather", TargetId = "item/ore", Count = 1 }], Rewards = [new QuestReward { BundleId = "reward/first" }] }] };
var event1 = new ObjectiveEvent("source-quest-1", "character-1", quest.Id, "first", "gather", "item/ore", null, 1, DateTimeOffset.Parse("2026-01-01T00:01:00Z"));
static ActionReceipt Kill(string id) => new(id, "character-1", ReceiptKind.EnemyDefeat, true);
var practice = new ActionReceipt("source-skill-1", "character-skill", ReceiptKind.GatheringHarvest, true, ProgressionRules.Logging, 25);
switch (args[0])
{
    case "seed":
        Check(store.ApplyProgression(Kill("source-xp-1"), 0, TimeProvider.System, () => .5).Revision == 1, "XP acknowledged");
        Check(store.ApplyProgression(practice, 0, TimeProvider.System, () => 0).Result.SkillGain == 1, "skill gain acknowledged");
        Check(store.AcceptQuest(quest, "character-1", DateTimeOffset.Parse("2026-01-01T00:00:00Z")) == 0, "quest accepted");
        var first = store.ApplyQuest(quest, event1, 0);
        Check(first.Revision == 1 && first.Result.Completed && first.Result.Grants.Count == 1, "quest step and reward receipt acknowledged");
        break;
    case "restart":
        var xp = store.ApplyProgression(Kill("source-xp-1"), 0, TimeProvider.System, () => .5);
        Check(xp.Revision == 1 && xp.Result.XpGain > 0, "XP survives immediate stop and replays original result");
        Check(store.ReadProgression("character-1")?.Result.LifetimeXp == xp.Result.XpGain, "committed XP state survives restart");
        Check(store.ApplyProgression(practice, 0, TimeProvider.System, () => .99).Result.SkillGain == 1, "skill gain survives and replays without reroll");
        Check(store.ReadProgression("character-skill")?.Result.Skills[ProgressionRules.Logging] == 1, "committed skill state survives restart");
        var questReplay = store.ApplyQuest(quest, event1, 0);
        Check(questReplay.Revision == 1 && questReplay.Result.Grants.Count == 1, "quest and reward replay without second advance");
        Check(store.ReadQuest(quest, "character-1")?.Result.StepIndex == 1, "committed quest step survives restart");
        break;
    case "concurrent":
        var ready = new CountdownEvent(2); var start = new ManualResetEventSlim();
        var tasks = new[] { "race-a", "race-b" }.Select(id => Task.Run(() =>
        {
            ready.Signal(); start.Wait();
            try { return store.ApplyProgression(Kill(id), 1, TimeProvider.System, () => .5).Revision == 2; }
            catch (CharacterStoreRejectedException) { return false; }
        })).ToArray();
        Check(ready.Wait(TimeSpan.FromSeconds(10)), "workers ready"); start.Set(); Task.WaitAll(tasks);
        Check(tasks.Count(t => t.Result) == 1, "one concurrent revision winner");
        using (var blocker = new NpgsqlConnection(connection))
        {
            blocker.Open();
            using var held = blocker.BeginTransaction();
            using (var lockCommand = new NpgsqlCommand("SELECT pg_advisory_xact_lock(hashtext('sf.character:' || $1))", blocker, held))
            {
                lockCommand.Parameters.AddWithValue("held-character");
                lockCommand.ExecuteNonQuery();
            }
            var position = new CharacterPositionSnapshot("zone_test", 0, 0, 0, 0);
            var heldWrite = Task.Run(() => store.SavePosition("held-character", 0, position));
            var otherWrite = Task.Run(() => store.SavePosition("other-character", 0, position));
            try
            {
                Check(otherWrite.Wait(TimeSpan.FromSeconds(10)) && otherWrite.Result == 1,
                    "different character writes do not serialize behind held lock");
                Check(!heldWrite.IsCompleted, "same character write waits for held lock");
            }
            finally { held.Commit(); }
            Check(heldWrite.Wait(TimeSpan.FromSeconds(10)) && heldWrite.Result == 1,
                "held character write resumes after lock release");
        }
        break;
    case "outage":
        try { store.ApplyProgression(Kill("outage"), 2, TimeProvider.System, () => .5); throw new Exception("outage accepted write"); }
        catch (CharacterStoreUnavailableException) { Console.WriteLine("PASS outage fails closed"); }
        break;
    case "recover":
        Check(store.ApplyProgression(Kill("outage"), 2, TimeProvider.System, () => .5).Revision == 3, "same source recovers after outage");
        Check(store.ApplyProgression(Kill("outage"), 2, TimeProvider.System, () => .5).Revision == 3, "recovery replay does not double gain");
        break;
    case "classification":
        try { store.ApplyProgression(new ActionReceipt("bad", "character\0bad", ReceiptKind.EnemyDefeat, true), 0, TimeProvider.System, () => .5); throw new Exception("SQL data error accepted"); }
        catch (CharacterStoreRejectedException) { Console.WriteLine("PASS SQL class 22 is non-retryable"); }
        break;
    case "newer":
        try { store.ApplyProgression(Kill("newer"), 3, TimeProvider.System, () => .5); throw new Exception("newer schema accepted"); }
        catch (CharacterSchemaCompatibilityException) { Console.WriteLine("PASS newer schema refused"); }
        break;
    default: throw new ArgumentException("Unknown phase.");
}
