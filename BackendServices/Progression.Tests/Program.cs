using ShatteredFrontier.Progression;

static void Check(bool ok, string name) { if (!ok) throw new Exception(name); Console.WriteLine("PASS " + name); }
static void Near(double actual, double expected, double epsilon, string name) => Check(Math.Abs(actual - expected) <= epsilon, name);
var defaults = new ProgressionSettings();
Near(ProgressionRules.DifficultyFactor(-150), .216, .001, "trivial boundary");
Near(ProgressionRules.DifficultyFactor(25), 1, 1e-9, "optimal difficulty");
Near(ProgressionRules.GainChance(620, 660, 2.5, true, 1.925, 1, 1, defaults), .01727, .00002, "worked example A");
Near(ProgressionRules.GainChance(455, 480, 12, false, 1, .7, .8, defaults), .01846, .00002, "worked example B");
Check(ProgressionRules.HourlyGainLimit(50, defaults) == 540 && ProgressionRules.HourlyGainLimit(620, defaults) == 79 && ProgressionRules.HourlyGainLimit(950, defaults) == 29, "hourly caps");
Check(ProgressionRules.XpToNext(50) == 142020 && ProgressionRules.XpToNext(100) == 400600, "XP curve");
Check(ProgressionRules.Fatigue(90, defaults) == 1 && Math.Abs(ProgressionRules.Fatigue(150, defaults) - .7) < 1e-9 && ProgressionRules.Fatigue(240, defaults) == .25, "fatigue boundaries");
var clock = new ManualClock();
var progress = new CharacterProgression("chr_test", clock, () => 0);
var harvest = new ActionReceipt("harvest-1", "chr_test", ReceiptKind.GatheringHarvest, true, ProgressionRules.Logging, 25, 6, 11);
var first = progress.Apply(harvest);
Check(first.SkillGain == 1 && first.XpGain > 0 && progress.Skills[ProgressionRules.Logging] == 1, "harvest gains skill and XP");
Check(!progress.Apply(harvest).Applied && progress.LifetimeXp == first.XpGain, "replay idempotent");
var restoredProgress = new CharacterProgression("chr_test", clock, () => 0, snapshot: progress.Snapshot());
Check(restoredProgress.LifetimeXp == progress.LifetimeXp &&
    restoredProgress.Skills[ProgressionRules.Logging] == 1 &&
    !restoredProgress.Apply(harvest).Applied, "snapshot restores XP, skill and receipt");
var veteran = new CharacterProgression("chr_veteran", clock, () => 0,
    initialXp: CharacterProgression.TotalFor(10));
Check(new CharacterProgression("chr_veteran", clock, () => 0, snapshot: veteran.Snapshot()).Level == 10,
    "snapshot restores level ten");
Check(!progress.Apply(harvest with { ReceiptId = "rejected", Accepted = false }).Applied, "rejected receipt ignored");
var craft = progress.Apply(new("craft-1", "chr_test", ReceiptKind.CraftConvert, true, ProgressionRules.Smithing, 25, 6, 12));
Check(craft.SkillGain == 1 && progress.Skills[ProgressionRules.Smithing] == 1, "craft skill mapping");
var carpentry = progress.Apply(new("craft-carpentry", "chr_test", ReceiptKind.CraftConvert, true,
    ProgressionRules.Carpentry, 25, 6, 13));
Check(carpentry.Applied && progress.Skills.ContainsKey(ProgressionRules.Carpentry) &&
    progress.Skills.ContainsKey(ProgressionRules.Leatherworking), "crafting skill registry");
var nearCap = new CharacterProgression("chr_cap", clock, () => 0, initialSkills: new Dictionary<string, int> { [ProgressionRules.Mining] = 900 });
Check(nearCap.Apply(new("cap", "chr_cap", ReceiptKind.GatheringHarvest, true, ProgressionRules.Mining, 925)).SkillGain == 0, "unspecialized cap");
var level = new CharacterProgression("chr_level", clock, () => 0, initialXp: CharacterProgression.TotalFor(2) - 1);
Check(level.Level == 1, "level lower boundary");
Check(level.Apply(new("kill", "chr_level", ReceiptKind.EnemyDefeat, true, EnemyLevel: 1)).Level == 2, "enemy defeat levels up");
Check(!level.Apply(new("kill", "chr_level", ReceiptKind.EnemyDefeat, true)).Applied, "kill replay idempotent");
var partyA = new CharacterProgression("chr_party_a", clock, () => 0);
var partyB = new CharacterProgression("chr_party_b", clock, () => 0);
var sharedA = partyA.Apply(new("party/kill", "chr_party_a", ReceiptKind.EnemyDefeat, true,
    EnemyLevel: 1, EligiblePartySize: 2, ReferencePartyLevel: 1));
var sharedB = partyB.Apply(new("party/kill", "chr_party_b", ReceiptKind.EnemyDefeat, true,
    EnemyLevel: 1, EligiblePartySize: 2, ReferencePartyLevel: 1));
Check(sharedA.XpGain == 4 && sharedB.XpGain == 4 && !partyA.Apply(new("party/kill", "chr_party_a", ReceiptKind.EnemyDefeat, true)).Applied,
    "eligible party members receive scaled kill XP once each");
Check(partyA.Apply(new("party/grey", "chr_party_a", ReceiptKind.EnemyDefeat, true,
    EnemyLevel: 2, EligiblePartySize: 2, ReferencePartyLevel: 20)).XpGain == 0,
    "highest eligible party level makes grey kills grant zero");
try { partyB.Apply(new("party/bad", "chr_party_b", ReceiptKind.EnemyDefeat, true,
    EnemyLevel: 1, EligiblePartySize: 2, ReferencePartyLevel: 101)); throw new Exception("invalid party level accepted"); }
catch (ArgumentOutOfRangeException) { Console.WriteLine("PASS party reference level bound"); }
var repeatSettings = new ProgressionSettings(GatheringRepeatThreshold: 1, RepetitionWindowMinutes: 60);
var repeated = new CharacterProgression("chr_repeat", clock, () => .999, repeatSettings);
double p1 = repeated.Apply(new("r1", "chr_repeat", ReceiptKind.GatheringHarvest, true, ProgressionRules.Logging, 25, 6, 42)).GainChance;
double p2 = repeated.Apply(new("r2", "chr_repeat", ReceiptKind.GatheringHarvest, true, ProgressionRules.Logging, 25, 6, 42)).GainChance;
Near(p2, p1 / 2, 1e-9, "repetition damping");
clock.Advance(TimeSpan.FromMinutes(61));
Near(repeated.Apply(new("r3", "chr_repeat", ReceiptKind.GatheringHarvest, true, ProgressionRules.Logging, 25, 6, 42)).GainChance, p1, 1e-9, "repetition window expiry");
var rateSettings = new ProgressionSettings(ReferenceAttemptsPerHour: 1);
var rate = new CharacterProgression("chr_rate", clock, () => 0, rateSettings);
Check(rate.Apply(new("g1", "chr_rate", ReceiptKind.GatheringHarvest, true, ProgressionRules.Logging, 25, 6, 1)).SkillGain == 1, "first hourly gain");
Check(rate.Apply(new("g2", "chr_rate", ReceiptKind.GatheringHarvest, true, ProgressionRules.Logging, 26, 6, 2)).SkillGain == 0, "hourly gain limit");
clock.Advance(TimeSpan.FromMinutes(61));
Check(rate.Apply(new("g3", "chr_rate", ReceiptKind.GatheringHarvest, true, ProgressionRules.Logging, 27, 6, 3)).SkillGain == 1, "hourly limit window expiry");
try { new ProgressionSettings(FatigueFreeMinutes: 240).Validate(); throw new Exception("invalid setting accepted"); }
catch (ArgumentOutOfRangeException) { Console.WriteLine("PASS bounded settings"); }

var restClock = new ManualClock();
var rested = new CharacterProgression("chr_rested", restClock, () => 0);
rested.EndSession(false);
restClock.Advance(TimeSpan.FromMinutes(40));
Check(rested.BeginSession() && Math.Abs(rested.RestedMinutes - 10) < 1e-9, "normal logout accrues at 1:4");
Check(!rested.BeginSession() && Math.Abs(rested.RestedMinutes - 10) < 1e-9, "session start accrues once");
var restSnapshot = rested.Snapshot();
rested = new CharacterProgression("chr_rested", restClock, () => 0, snapshot: restSnapshot);
var restedKill = rested.Apply(new("rest-kill", "chr_rested", ReceiptKind.EnemyDefeat, true, EnemyLevel: 1));
Check(restedKill.XpGain == 10 && Math.Abs(rested.RestedMinutes - 9) < 1e-9, "rested kill XP and first active minute debit");
Check(!rested.Apply(new("rest-kill", "chr_rested", ReceiptKind.EnemyDefeat, true)).Applied &&
    Math.Abs(rested.RestedMinutes - 9) < 1e-9, "replay does not debit rest");
var secondKill = rested.Apply(new("rest-kill-2", "chr_rested", ReceiptKind.EnemyDefeat, true, EnemyLevel: 1));
Check(secondKill.XpGain == 10 && Math.Abs(rested.RestedMinutes - 9) < 1e-9, "same wall minute debits once");
restClock.Advance(TimeSpan.FromMinutes(1));
var unRested = new CharacterProgression("chr_rested", restClock, () => 0,
    snapshot: rested.Snapshot() with { RestedMinutes = 0 });
var practiceReceipt = new ActionReceipt("rest-practice", "chr_rested", ReceiptKind.GatheringHarvest, true,
    ProgressionRules.Logging, 25, 18, 77);
var ordinaryPractice = unRested.Apply(practiceReceipt);
var restedPractice = rested.Apply(practiceReceipt);
Check(restedPractice.XpGain > ordinaryPractice.XpGain &&
    Math.Abs(restedPractice.GainChance / ordinaryPractice.GainChance - 1.75) < 1e-9,
    "rested practice boosts XP and skill chance");
Check(Math.Abs(rested.RestedMinutes - 8) < 1e-9, "practice minute debits rest");
var finalPractice = rested.Apply(practiceReceipt with { ReceiptId = "rest-final", SessionEnd = true, InRestArea = true });
Check(finalPractice.Applied && rested.LastSessionEndAt == restClock.GetUtcNow() && rested.EndedInRestArea,
    "final practice atomically closes session in rest area");
Check(!rested.Apply(practiceReceipt with { ReceiptId = "rest-final", SessionEnd = true, InRestArea = true }).Applied,
    "final practice replay does not restart session");
restClock.Advance(TimeSpan.FromMinutes(2000));
rested = new CharacterProgression("chr_rested", restClock, () => 0, snapshot: rested.Snapshot());
Check(rested.BeginSession() && rested.RestedMinutes == 600, "rest area accrues at 1:2 and caps at 600");
try { new ProgressionSettings(RestedCapMinutes: 0).Validate(); throw new Exception("invalid rested cap accepted"); }
catch (ArgumentOutOfRangeException) { Console.WriteLine("PASS rested cap bound"); }
try { new ProgressionSettings(RestedXpMultiplier: double.NaN).Validate(); throw new Exception("invalid rested multiplier accepted"); }
catch (ArgumentOutOfRangeException) { Console.WriteLine("PASS rested multiplier bound"); }

sealed class ManualClock : TimeProvider
{
    private DateTimeOffset now = DateTimeOffset.Parse("2026-01-01T00:00:00Z");
    public override DateTimeOffset GetUtcNow() => now;
    public void Advance(TimeSpan duration) => now += duration;
}
