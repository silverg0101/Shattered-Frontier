// Shipped server controls. A compatible shared configuration workflow can publish a
// future snapshot at a death-event boundary; clients cannot set these values.
internal sealed record DeathPenaltyTuning(int MinimumCharacterLevel = 10, int WearPercentOfMaximum = 5)
{
    public const string MinimumLevelKey = "item.death_wear_min_level";
    public const string WearPercentKey = "item.death_wear_percent";
    public const string Permission = "gameplay_tuning_operator";
    public const string EffectiveTime = "next_death_event";
    public void Validate()
    {
        if (MinimumCharacterLevel is < 1 or > 1000 || WearPercentOfMaximum is < 0 or > 100)
            throw new ArgumentOutOfRangeException(nameof(DeathPenaltyTuning), "Level must be 1-1000 and wear must be 0-100 percent of maximum durability.");
    }
}
