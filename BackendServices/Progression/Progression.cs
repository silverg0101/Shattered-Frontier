namespace ShatteredFrontier.Progression;

public static class ProgressionRules
{
    public const int FormulaVersion = 1;
    public const int LevelCap = 100, SkillMax = 1000, UnspecializedCap = 900;
    public const int GlobalSkillCap = 7200, TrackSkillCap = 4000;
    public const string Logging = "skill/logging", Mining = "skill/mining", Smithing = "skill/smithing",
        Carpentry = "skill/carpentry", Leatherworking = "skill/leatherworking";
    public static int XpToNext(int level) => level < 1 ? throw new ArgumentOutOfRangeException(nameof(level))
        : checked((int)(10 * Math.Round((400 * Math.Pow(level, 1.5) + 600) / 10, MidpointRounding.AwayFromZero)));
    public static double SkillFactor(int value) => 1 / Math.Max(1, value / 100d);
    public static double BandFactor(int value) => value >= 900 ? .5 : value >= 700 ? .8 : value >= 500 ? .9 : 1;
    public static double DifficultyFactor(int difference) => difference is < -150 or > 250 ? 0 : Math.Exp(-Math.Pow((difference - 25) / 100d, 2) / 2);
    public static double Fatigue(int activeMinutes, ProgressionSettings s) => activeMinutes <= s.FatigueFreeMinutes ? 1
        : Math.Max(s.FatigueFloor, 1 - (1 - s.FatigueFloor) * (activeMinutes - s.FatigueFreeMinutes) / (double)(s.FatigueWindowMinutes - s.FatigueFreeMinutes));
    public static int HourlyGainLimit(int value, ProgressionSettings s) => (int)Math.Ceiling(2 * s.ReferenceAttemptsPerHour * s.BaseGainRate * SkillFactor(value) * BandFactor(value) * 3);
    public static double GainChance(int value, int difficulty, double seconds, bool success, double bonus, double fatigue, double antiMacro, ProgressionSettings s)
    {
        double weight = Math.Clamp(seconds / s.AttemptWeightSeconds, .2, 3);
        return Math.Min(s.MaximumGainChance, s.BaseGainRate * weight * SkillFactor(value) * BandFactor(value)
            * DifficultyFactor(difficulty - value) * (success ? 1 : .5) * Math.Clamp(bonus, 1, 3)) * fatigue * antiMacro;
    }
}

// Immutable, release-authored effective snapshot. The shared configuration workflow owns publication.
public sealed record ProgressionSettings(
    double BaseGainRate = .15, double MaximumGainChance = .9, double AttemptWeightSeconds = 6,
    int FatigueWindowMinutes = 240, int FatigueFreeMinutes = 90, double FatigueFloor = .25,
    int RepetitionWindowMinutes = 60, int GatheringRepeatThreshold = 80, int CraftingRepeatThreshold = 60,
    int GainWindowMinutes = 60, int ReferenceAttemptsPerHour = 600,
    int RestedNormalMinutesPerMinute = 4, int RestedAreaMinutesPerMinute = 2,
    int RestedCapMinutes = 600, double RestedXpMultiplier = 1.5, double RestedGainMultiplier = 1.75,
    int KillsPerLevel = 150, double AdditionalPartyShare = .2)
{
    public void Validate()
    {
        if (BaseGainRate is < .01 or > .5 || MaximumGainChance is < .1 or > .9 || AttemptWeightSeconds is < 1 or > 60 ||
            FatigueWindowMinutes is < 60 or > 480 || FatigueFreeMinutes < 1 || FatigueFreeMinutes >= FatigueWindowMinutes ||
            FatigueFloor is < .1 or > 1 || RepetitionWindowMinutes is < 10 or > 240 || GatheringRepeatThreshold is < 1 or > 1000 ||
            CraftingRepeatThreshold is < 1 or > 1000 || GainWindowMinutes is < 10 or > 240 || ReferenceAttemptsPerHour is < 1 or > 2000 ||
            RestedNormalMinutesPerMinute is < 1 or > 60 || RestedAreaMinutesPerMinute is < 1 or > 60 ||
            RestedCapMinutes is < 1 or > 1440 || !double.IsFinite(RestedXpMultiplier) || RestedXpMultiplier is < 1 or > 3 ||
            !double.IsFinite(RestedGainMultiplier) || RestedGainMultiplier is < 1 or > 3 ||
            KillsPerLevel is < 1 or > 1000 || !double.IsFinite(AdditionalPartyShare) || AdditionalPartyShare is < 0 or > 1)
            throw new ArgumentOutOfRangeException(nameof(ProgressionSettings));
    }
}

public enum ReceiptKind { GatheringHarvest, CraftConvert, EnemyDefeat }
public sealed record ActionReceipt(string ReceiptId, string CharacterId, ReceiptKind Kind, bool Accepted,
    string? SkillId = null, int Difficulty = 0, double ActionSeconds = 6, ulong ContextHash = 0,
    bool Success = true, int EnemyLevel = 1, double EnemyTierMultiplier = 1, int EligiblePartySize = 1,
    bool SessionEnd = false, bool InRestArea = false, int ReferencePartyLevel = 0);
public sealed record ProgressionResult(bool Applied, int SkillGain, int XpGain, int Level, double GainChance, double RestedMinutes = 0);
public sealed record ProgressionSnapshot(long LifetimeXp, Dictionary<string, int> Skills, ProgressionSettings Settings, string[] Receipts,
    Dictionary<string, long[]> ActiveMinutes, PracticeSnapshot[] Practices, GainSnapshot[] Gains,
    KnownRecipe[]? KnownRecipes = null, double RestedMinutes = 0,
    DateTimeOffset? LastSessionEndAt = null, bool EndedInRestArea = false, long? LastRestedActiveMinute = null);
public sealed record KnownRecipe(string RecipeId, string Source, DateTimeOffset LearnedAt);
public sealed record PracticeSnapshot(DateTimeOffset At, string Skill, ulong Context);
public sealed record GainSnapshot(DateTimeOffset At, string Skill);

public sealed class CharacterProgression
{
    private readonly HashSet<string> receipts = new(StringComparer.Ordinal);
    private readonly Dictionary<string, int> skills = new(StringComparer.Ordinal)
        { [ProgressionRules.Logging] = 0, [ProgressionRules.Mining] = 0, [ProgressionRules.Smithing] = 0,
            [ProgressionRules.Carpentry] = 0, [ProgressionRules.Leatherworking] = 0 };
    private readonly List<KnownRecipe> knownRecipes = [];
    private readonly Dictionary<string, HashSet<long>> activeMinutes = new(StringComparer.Ordinal);
    private readonly List<(DateTimeOffset At, string Skill, ulong Context)> practices = [];
    private readonly List<(DateTimeOffset At, string Skill)> gains = [];
    private readonly TimeProvider clock;
    private readonly Func<double> roll;
    private readonly ProgressionSettings settings;
    public string CharacterId { get; }
    public long LifetimeXp { get; private set; }
    public int Level { get; private set; } = 1;
    public double RestedMinutes { get; private set; }
    public DateTimeOffset? LastSessionEndAt { get; private set; }
    public bool EndedInRestArea { get; private set; }
    public long? LastRestedActiveMinute { get; private set; }
    public IReadOnlyDictionary<string, int> Skills => skills;

    public CharacterProgression(string characterId, TimeProvider clock, Func<double> roll, ProgressionSettings? settings = null,
        IReadOnlyDictionary<string, int>? initialSkills = null, long initialXp = 0, ProgressionSnapshot? snapshot = null)
    {
        if (string.IsNullOrWhiteSpace(characterId) || clock is null || roll is null || initialXp < 0) throw new ArgumentException("Invalid progression initialization");
        CharacterId = characterId; this.clock = clock; this.roll = roll; this.settings = snapshot?.Settings ?? settings ?? new(); this.settings.Validate();
        if (initialSkills is not null)
            foreach (var (id, value) in initialSkills)
            {
                if (!skills.ContainsKey(id) || value is < 0 or > ProgressionRules.UnspecializedCap) throw new ArgumentOutOfRangeException(nameof(initialSkills));
                skills[id] = value;
            }
        LifetimeXp = initialXp;
        while (Level < ProgressionRules.LevelCap && LifetimeXp >= TotalFor(Level + 1)) Level++;
        if (snapshot is not null)
        {
            if (!double.IsFinite(snapshot.RestedMinutes) || snapshot.RestedMinutes < 0 || snapshot.RestedMinutes > this.settings.RestedCapMinutes)
                throw new ArgumentException("Invalid rested snapshot.");
            RestedMinutes = snapshot.RestedMinutes;
            LastSessionEndAt = snapshot.LastSessionEndAt;
            EndedInRestArea = snapshot.EndedInRestArea;
            LastRestedActiveMinute = snapshot.LastRestedActiveMinute;
            knownRecipes.AddRange(snapshot.KnownRecipes ?? []);
            foreach (var id in snapshot.Receipts) receipts.Add(id);
            foreach (var (id, minutes) in snapshot.ActiveMinutes) activeMinutes.Add(id, new HashSet<long>(minutes));
            practices.AddRange(snapshot.Practices.Select(p => (p.At, p.Skill, p.Context)));
            gains.AddRange(snapshot.Gains.Select(g => (g.At, g.Skill)));
        }
    }

    public ProgressionSnapshot Snapshot() => new(LifetimeXp, new(skills, StringComparer.Ordinal), settings, [.. receipts],
        activeMinutes.ToDictionary(p => p.Key, p => p.Value.ToArray(), StringComparer.Ordinal),
        practices.Select(p => new PracticeSnapshot(p.At, p.Skill, p.Context)).ToArray(),
        gains.Select(g => new GainSnapshot(g.At, g.Skill)).ToArray(), knownRecipes.ToArray(),
        RestedMinutes, LastSessionEndAt, EndedInRestArea, LastRestedActiveMinute);

    public bool BeginSession()
    {
        if (LastSessionEndAt is not { } ended) return false;
        var elapsed = Math.Max(0, (clock.GetUtcNow() - ended).TotalMinutes);
        RestedMinutes = Math.Min(settings.RestedCapMinutes, RestedMinutes + elapsed /
            (EndedInRestArea ? settings.RestedAreaMinutesPerMinute : settings.RestedNormalMinutesPerMinute));
        LastSessionEndAt = null;
        EndedInRestArea = false;
        return true;
    }

    public void EndSession(bool inRestArea)
    {
        if (LastSessionEndAt is not null) throw new InvalidOperationException("Session already ended.");
        LastSessionEndAt = clock.GetUtcNow();
        EndedInRestArea = inRestArea;
    }

    public static long TotalFor(int level)
    {
        if (level is < 1 or > ProgressionRules.LevelCap) throw new ArgumentOutOfRangeException(nameof(level));
        long total = 0; for (int i = 1; i < level; i++) total += ProgressionRules.XpToNext(i); return total;
    }

    public ProgressionResult Apply(ActionReceipt receipt)
    {
        if (receipt.CharacterId != CharacterId || string.IsNullOrWhiteSpace(receipt.ReceiptId)) throw new ArgumentException("Invalid receipt owner or id");
        if (!receipt.Accepted) return new(false, 0, 0, Level, 0, RestedMinutes);
        if (receipts.Contains(receipt.ReceiptId)) return new(false, 0, 0, Level, 0, RestedMinutes);
        if (!receipt.SessionEnd && receipt.InRestArea) throw new ArgumentException("Rest area requires session end.");
        BeginSession();
        if (receipt.Kind is ReceiptKind.GatheringHarvest or ReceiptKind.CraftConvert)
        {
            string skill = receipt.SkillId ?? "";
            if (receipt.Kind == ReceiptKind.GatheringHarvest ? skill is not (ProgressionRules.Logging or ProgressionRules.Mining) :
                skill is not (ProgressionRules.Smithing or ProgressionRules.Carpentry or ProgressionRules.Leatherworking))
                throw new ArgumentException("Receipt skill does not match accepted action");
            if (receipt.Difficulty is < 0 or > 1100 || !double.IsFinite(receipt.ActionSeconds) || receipt.ActionSeconds <= 0) throw new ArgumentOutOfRangeException(nameof(receipt));
        }
        else if (receipt.Kind == ReceiptKind.EnemyDefeat)
        {
            if (receipt.EnemyLevel is < 1 or > 200 || receipt.EnemyTierMultiplier is not (1 or 2.5 or 5 or 20) || receipt.EligiblePartySize is < 1 or > 5 ||
                receipt.ReferencePartyLevel is < 0 or > ProgressionRules.LevelCap)
                throw new ArgumentOutOfRangeException(nameof(receipt));
        }
        else throw new ArgumentOutOfRangeException(nameof(receipt));
        receipts.Add(receipt.ReceiptId);
        var now = clock.GetUtcNow();
        var activeMinute = now.ToUnixTimeSeconds() / 60;
        bool rested = RestedMinutes > 0;
        int xp = 0, gain = 0; double chance = 0;
        if (receipt.Kind is ReceiptKind.GatheringHarvest or ReceiptKind.CraftConvert)
        {
            string id = receipt.SkillId ?? "";
            practices.RemoveAll(p => p.At <= now.AddMinutes(-settings.RepetitionWindowMinutes));
            gains.RemoveAll(g => g.At <= now.AddMinutes(-settings.GainWindowMinutes));
            var minutes = activeMinutes.TryGetValue(id, out var set) ? set : activeMinutes[id] = [];
            minutes.RemoveWhere(m => m <= now.AddMinutes(-settings.FatigueWindowMinutes).ToUnixTimeSeconds() / 60);
            minutes.Add(now.ToUnixTimeSeconds() / 60);
            practices.Add((now, id, receipt.ContextHash));
            int repeats = practices.Count(p => p.Skill == id && p.Context == receipt.ContextHash);
            int threshold = receipt.Kind == ReceiptKind.CraftConvert ? settings.CraftingRepeatThreshold : settings.GatheringRepeatThreshold;
            double anti = repeats <= threshold ? 1 : Math.Max(.2, threshold / (double)repeats);
            double fatigue = ProgressionRules.Fatigue(minutes.Count, settings);
            int value = skills[id];
            if (value < ProgressionRules.UnspecializedCap)
            {
                chance = ProgressionRules.GainChance(value, receipt.Difficulty, receipt.ActionSeconds, receipt.Success,
                    rested ? settings.RestedGainMultiplier : 1, fatigue, anti, settings);
                int hourGains = gains.Count(g => g.Skill == id);
                double sample = roll();
                if (!double.IsFinite(sample) || sample < 0 || sample >= 1) throw new InvalidOperationException("Roll must be in [0, 1)");
                if (sample < chance && hourGains < ProgressionRules.HourlyGainLimit(value, settings) &&
                    skills.Values.Sum() < ProgressionRules.GlobalSkillCap &&
                    (receipt.Kind == ReceiptKind.CraftConvert ? value : skills[ProgressionRules.Logging] + skills[ProgressionRules.Mining]) < ProgressionRules.TrackSkillCap)
                { skills[id]++; gains.Add((now, id)); gain = 1; }
            }
            double weight = Math.Clamp(receipt.ActionSeconds / settings.AttemptWeightSeconds, .2, 3);
            xp = (int)Math.Round(Math.Round(ProgressionRules.XpToNext(Level) / 12d, MidpointRounding.AwayFromZero) * .012 * weight *
                ProgressionRules.DifficultyFactor(receipt.Difficulty - value) * fatigue * anti *
                (rested ? settings.RestedXpMultiplier : 1), MidpointRounding.AwayFromZero);
        }
        else if (receipt.Kind == ReceiptKind.EnemyDefeat)
        {
            int referenceLevel = receipt.ReferencePartyLevel == 0 ? Level : receipt.ReferencePartyLevel;
            int delta = receipt.EnemyLevel - referenceLevel;
            double levelDiff = delta >= 5 ? 1.2 : delta >= -5 ? 1 + .04 * delta : Math.Max(0, .8 - .15 * (-delta - 5));
            double group = (1 + settings.AdditionalPartyShare * (receipt.EligiblePartySize - 1)) / receipt.EligiblePartySize;
            xp = (int)Math.Round(ProgressionRules.XpToNext(referenceLevel) / (double)settings.KillsPerLevel * receipt.EnemyTierMultiplier * levelDiff * group *
                (rested ? settings.RestedXpMultiplier : 1), MidpointRounding.AwayFromZero);
        }
        if (LastRestedActiveMinute != activeMinute)
        {
            RestedMinutes = Math.Max(0, RestedMinutes - 1);
            LastRestedActiveMinute = activeMinute;
        }
        LifetimeXp += xp;
        while (Level < ProgressionRules.LevelCap && LifetimeXp >= TotalFor(Level + 1)) Level++;
        if (receipt.SessionEnd) EndSession(receipt.InRestArea);
        return new(true, gain, xp, Level, chance, RestedMinutes);
    }
}
