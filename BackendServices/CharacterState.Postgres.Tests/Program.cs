using ShatteredFrontier.CharacterState.Postgres;
using ShatteredFrontier.Progression;
using ShatteredFrontier.QuestContent;
using CharacterAppearance;
using Npgsql;

static void Check(bool value, string label) { if (!value) throw new Exception(label); Console.WriteLine("PASS " + label); }
var connection = Environment.GetEnvironmentVariable("CHARACTER_STATE_CONNECTION");
if (string.IsNullOrWhiteSpace(connection)) { Console.WriteLine("SKIP live PostgreSQL fixture unavailable (CHARACTER_STATE_CONNECTION unset)"); return; }
var store = new PostgresCharacterStateStore(connection);
var quest = new QuestDefinition { Id = "quest/durable", Revision = 1, ZoneId = "zone/test", Steps = [new QuestStep { Id = "first", Objectives = [new QuestObjective { Key = "ore", Kind = "gather", TargetId = "item/ore", Count = 1 }], Rewards = [new QuestReward { BundleId = "reward/first" }] }] };
var event1 = new ObjectiveEvent("source-quest-1", "character-1", quest.Id, "first", "gather", "item/ore", null, 1, DateTimeOffset.Parse("2026-01-01T00:01:00Z"));
static ActionReceipt Kill(string id) => new(id, "character-1", ReceiptKind.EnemyDefeat, true);
var practice = new ActionReceipt("source-skill-1", "character-skill", ReceiptKind.GatheringHarvest, true, ProgressionRules.Logging, 25);
switch (args[0])
{
    case "seed":
        var deathPoint = new CharacterPositionSnapshot("silverbrook", 12, 20, 30, 0);
        var respawnPoint = new CharacterPositionSnapshot("silverbrook", 100, 200, 30, 0);
        var deathId = "00000000-0000-0000-0000-000000000001";
        var death = store.ReportDeath(deathId, "character-death", 100000, deathPoint, respawnPoint, 4000);
        Check(death.IsDead && death.RespawnAtUnixMs == 104000, "death persisted with deadline");
        Check(store.ReadDeathLevel(deathId, "character-death") == 1, "death captures character level durably");
        const string veteran = "character-death-level10";
        const string veteranDeath = "00000000-0000-0000-0000-000000000099";
        var veteranProgression = new ProgressionSnapshot(CharacterProgression.TotalFor(10),
            new Dictionary<string, int>(), new ProgressionSettings(), [],
            new Dictionary<string, long[]>(), [], []);
        using (var db = new NpgsqlConnection(connection))
        {
            db.Open();
            using var insert = new NpgsqlCommand("INSERT INTO character_state.progression(character_id,revision,state) VALUES ($1,1,$2::jsonb)", db);
            insert.Parameters.AddWithValue(veteran);
            insert.Parameters.AddWithValue(System.Text.Json.JsonSerializer.Serialize(veteranProgression));
            insert.ExecuteNonQuery();
        }
        store.ReportDeath(veteranDeath, veteran, 100000, deathPoint, respawnPoint, 4000);
        Check(store.ReadDeathLevel(veteranDeath, veteran) == 10, "level ten is captured from server progression");
        store.ReportDeath(veteranDeath, veteran, 100000, deathPoint, respawnPoint, 4000, 20, 7);
        Check(store.ReadDeathPenalty(veteranDeath, veteran) == (10, 10, 5),
            "death keeps its tuning snapshot when replayed after a tuning change");
        Check(store.ReportDeath(deathId, "character-death", 100000, deathPoint, respawnPoint, 4000) == death, "death replay stable");
        try { store.ReportDeath(deathId, "character-death", 100000, deathPoint, respawnPoint, 5000); throw new Exception("death payload replay accepted"); }
        catch (CharacterStoreRejectedException) { Check(true, "death event payload conflict rejected"); }
        var appearanceValidator = new AppearanceValidator(AppearanceAllowlist.Load("content/appearance/human.json"));
        var appearancePacket = AppearanceCodec.Pack(new Appearance(2, 0, 1, 1, 128, 128,
            new byte[8], [0, 0, 0, 0], [0, 0, 0, 0], [], 5));
        Check(store.ReadAppearance("character-appearance") is null, "appearance initially absent");
        Check(store.SaveAppearance("character-appearance", 0, appearancePacket, appearanceValidator, new HashSet<ushort>(), new HashSet<ushort>()) == 1,
            "validated appearance saved");
        Check(store.ReadAppearance("character-appearance") is { Revision: 1, Result: var packet } && packet.SequenceEqual(appearancePacket),
            "appearance bytes loaded");
        try { store.SaveAppearance("character-appearance", 1, AppearanceCodec.Pack(new Appearance(2, 0, 1, 1, 128, 128,
            new byte[8], [0, 0, 0, 0], [0, 0, 0, 0], [], 0)), appearanceValidator, new HashSet<ushort>(), new HashSet<ushort>());
            throw new Exception("invalid appearance accepted"); }
        catch (CharacterStoreRejectedException ex) { Check(ex.Message == "APPEARANCE_INVALID", "invalid appearance rejected"); }
        try { store.SaveAppearance("character-appearance", 0, appearancePacket, appearanceValidator, new HashSet<ushort>(), new HashSet<ushort>());
            throw new Exception("stale appearance accepted"); }
        catch (CharacterStoreRejectedException) { Console.WriteLine("PASS stale appearance rejected"); }
        var changedFace = (byte[])appearancePacket.Clone(); changedFace[^1] = 8;
        Check(store.SaveAppearance("character-appearance", 1, changedFace, appearanceValidator, new HashSet<ushort>(), new HashSet<ushort>()) == 2 &&
            store.ReadAppearance("character-appearance") is { Revision: 2, Result: var updatedPacket } && updatedPacket.SequenceEqual(changedFace),
            "face change saved with revision");
        Check(store.ApplyProgression(Kill("source-xp-1"), 0, TimeProvider.System, () => .5).Revision == 1, "XP acknowledged");
        Check(store.ApplyProgression(practice, 0, TimeProvider.System, () => 0).Result.SkillGain == 1, "skill gain acknowledged");
        var t0 = DateTimeOffset.Parse("2026-01-01T00:00:00Z");
        var accepted = store.AcceptQuest(quest, "character-1", t0, "accept-1", []);
        Check(accepted.Revision == 0 && accepted.Result is { Accepted: true, ActiveStepId: "first" }, "quest accepted");
        Check(store.AcceptQuest(quest, "character-1", t0.AddHours(1), "accept-1", []) is { Revision: 0, Result: { Accepted: true, ActiveStepId: "first" } }, "accept replays same reply");
        Check(store.AcceptQuest(quest, "character-1", t0, "accept-2", []).Result is { Accepted: false, Reason: "duplicate" }, "second accept is duplicate");
        var gated = new QuestDefinition { Id = "quest/gated", Revision = 1, ZoneId = "zone/test", Steps = quest.Steps };
        Check(store.AcceptQuest(gated, "character-1", t0, "accept-gated-1", [quest]).Result.Reason == "prerequisite_missing", "unfinished prerequisite refused");
        Check(store.ReadQuests("character-1").Count == 1, "refusal stored no row");
        var first = store.ApplyQuest(quest, event1, 0);
        Check(first.Revision == 1 && first.Result.Completed && first.Result.Grants.Count == 1, "quest step and reward receipt acknowledged");
        Check(store.AcceptQuest(quest, "character-1", t0, "accept-3", []).Result.Reason == "completed", "completed quest not re-accepted");
        Check(store.AcceptQuest(gated, "character-1", t0, "accept-gated-2", [quest]).Result.Accepted, "completed prerequisite allows accept");
        Check(store.AbandonQuest(quest, "character-1", "abandon-done").Result.Reason == "not_active", "completed quest cannot be abandoned");
        var two = new QuestDefinition { Id = "quest/two", Revision = 1, ZoneId = "zone/test", Steps = [
            new QuestStep { Id = "a", Objectives = [new QuestObjective { Key = "ore", Kind = "gather", TargetId = "item/ore", Count = 2 }], Rewards = [new QuestReward { BundleId = "reward/a" }] },
            new QuestStep { Id = "b", Prerequisites = ["a"], Objectives = [new QuestObjective { Key = "gem", Kind = "gather", TargetId = "item/gem", Count = 1 }] }] };
        ObjectiveEvent Ev(string op, string step, string target, int sec) => new(op, "character-3", two.Id, step, "gather", target, null, 1, t0.AddSeconds(sec));
        Check(store.ApplyQuest(two, Ev("pre", "a", "item/ore", 1), 0).Result.Reason == "quest not accepted", "evidence before accept does not throw");
        Check(store.AcceptQuest(two, "character-3", t0, "t-accept-1", []).Revision == 0, "multi-step quest accepted");
        Check(store.ApplyQuest(two, Ev("o1", "a", "item/ore", 1), 0).Result.Accepted, "first ore counted");
        var step1 = store.ApplyQuest(two, Ev("o2", "a", "item/ore", 2), 1);
        Check(step1.Result.Grants.Single().OperationId == "quest:character-3:quest/two:a:reward/a", "instance 1 grant id has no suffix");
        var gem = Ev("g1", "b", "item/gem", 3);
        Check(store.AbandonQuest(two, "character-3", "t-abandon-1").Result is { Accepted: true } && store.AbandonQuest(two, "character-3", "t-abandon-1").Revision == 3, "abandon and replay");
        Check(store.AbandonQuest(two, "character-3", "t-abandon-2").Result.Reason == "not_active", "abandoned quest cannot be abandoned again");
        Check(store.ApplyQuest(two, gem, 3).Result.Reason == "quest not accepted", "evidence after abandon refused");
        Check(store.ReadQuests("character-3").Single().State.Abandoned, "row reads abandoned");
        var again = store.AcceptQuest(two, "character-3", t0.AddMinutes(1), "t-accept-2", []);
        Check(again.Revision == 4 && again.Result.Accepted, "re-accept resets abandoned row");
        var fresh = store.ReadQuest(two, "character-3")!.Result;
        Check(fresh is { Instance: 2, StepIndex: 0, Abandoned: false } && fresh.Counts.Count == 0 && fresh.Operations.Length == 0, "re-accepted quest starts from zero");
        Check(store.ApplyQuest(two, Ev("o1", "a", "item/ore", 1), 3).Result.Accepted == true &&
            store.ReadQuest(two, "character-3")!.Result.StepIndex == 0, "old operation replays its stored reply without touching the new instance");
        ObjectiveEvent Ev2(string op, int sec) => new(op, "character-3", two.Id, "a", "gather", "item/ore", null, 1, t0.AddMinutes(1).AddSeconds(sec));
        store.ApplyQuest(two, Ev2("n1", 1), 4);
        var step1b = store.ApplyQuest(two, Ev2("n2", 2), 5);
        Check(step1b.Result.Grants.Single().OperationId == "quest:character-3:quest/two:a:reward/a:i2" && store.Reward(step1b.Result.Grants[0].OperationId) is not null &&
            store.Reward("quest:character-3:quest/two:a:reward/a") is not null, "re-accepted completion pays a second distinct reward intent");
        Check(store.PendingRewardQuests("character-3").Contains("quest/two") && !store.PendingRewardQuests("character-2").Contains("quest/two"), "pending reward quests listed");
        break;
    case "restart":
        Check(store.ReadDeath("character-death") is { IsDead: true, RespawnAtUnixMs: 104000 }, "dead state survives restart");
        Check(store.ReadDeathLevel("00000000-0000-0000-0000-000000000001", "character-death") == 1,
            "death level survives restart");
        Check(store.ReadDeathLevel("00000000-0000-0000-0000-000000000099", "character-death-level10") == 10,
            "level ten death snapshot survives restart");
        Check(store.ReadDeathPenalty("00000000-0000-0000-0000-000000000099", "character-death-level10") == (10, 10, 5),
            "death tuning snapshot survives restart");
        Check(!store.ClearDeath("character-death", "00000000-0000-0000-0000-000000000001").IsDead, "respawn clears dead state");
        Check(!store.ClearDeath("character-death", "00000000-0000-0000-0000-000000000001").IsDead, "respawn clear replays");
        Check(store.ReadPosition("character-death")?.Result.X == 100, "respawn point committed with clear");
        Check(!store.ReportDeath("00000000-0000-0000-0000-000000000001", "character-death", 100000,
            new("silverbrook", 12, 20, 30, 0), new("silverbrook", 100, 200, 30, 0), 4000).IsDead, "death replay after clear stays alive");
        const string resurrectionEvent = "00000000-0000-0000-0000-000000000002";
        Check(store.ReportDeath(resurrectionEvent, "character-death", 110000,
            new("silverbrook", 25, 30, 40, 0), new("silverbrook", 200, 300, 40, 0), 60000).IsDead,
            "second death recorded");
        Check(!store.ClearDeath("character-death", resurrectionEvent, true).IsDead &&
            store.ReadPosition("character-death")?.Result.X == 25, "resurrection commits death position");
        Check(!store.ClearDeath("character-death", resurrectionEvent, true).IsDead &&
            store.ReadPosition("character-death")?.Result.X == 25, "resurrection clear replay keeps position");
        Check(store.ReadAppearance("character-appearance") is { Revision: 2, Result: var restoredPacket } &&
            restoredPacket[0] == 2 && restoredPacket[^1] == 8, "appearance survives restart");
        var xp = store.ApplyProgression(Kill("source-xp-1"), 0, TimeProvider.System, () => .5);
        Check(xp.Revision == 1 && xp.Result.XpGain > 0, "XP survives immediate stop and replays original result");
        Check(store.ReadProgression("character-1")?.Result.LifetimeXp == xp.Result.XpGain, "committed XP state survives restart");
        Check(store.ApplyProgression(practice, 0, TimeProvider.System, () => .99).Result.SkillGain == 1, "skill gain survives and replays without reroll");
        Check(store.ReadProgression("character-skill")?.Result.Skills[ProgressionRules.Logging] == 1, "committed skill state survives restart");
        var questReplay = store.ApplyQuest(quest, event1, 0);
        Check(questReplay.Revision == 1 && questReplay.Result.Grants.Count == 1, "quest and reward replay without second advance");
        Check(store.ReadQuest(quest, "character-1")?.Result.StepIndex == 1, "committed quest step survives restart");
        Check(store.ReadQuests("character-3").Single().State.Instance == 2 && store.PendingRewardQuests("character-1").Contains("quest/durable"), "quest log and pending reward survive restart");
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
