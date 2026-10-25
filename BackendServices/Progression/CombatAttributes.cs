namespace ShatteredFrontier.Progression;

public sealed record BaseAttributes(int Strength, int Dexterity, int Constitution,
    int Intelligence, int Wisdom, int Willpower);
public sealed record CombatIdentity(string Archetype, BaseAttributes Base);
public sealed record CombatAttributeSnapshot(string Archetype, int Level, BaseAttributes Attributes);

// Release-authored, typed inputs. An authorized combat-balance operator publishes through the shared
// configuration workflow. Identity inputs take effect at creation; curve inputs at the next server snapshot.
public sealed record CombatAttributeTuning
{
    public int AttributeFloor { get; init; } = 10;
    public int CreationPoints { get; init; } = 40;
    public int CreationNaturalTotal { get; init; } = 120;
    public int LevelTenNaturalTotal { get; init; } = 150;
    public int LevelTwentyFiveNaturalTotal { get; init; } = 210;
    public BaseAttributes Fighter { get; init; } = new(30, 15, 25, 10, 10, 10);
    public BaseAttributes Guardian { get; init; } = new(25, 10, 30, 10, 10, 15);
    public BaseAttributes Archer { get; init; } = new(10, 40, 20, 10, 10, 10);
    public BaseAttributes Mage { get; init; } = new(10, 10, 10, 40, 20, 10);
    public BaseAttributes Healer { get; init; } = new(10, 10, 10, 20, 40, 10);
    public BaseAttributes Shadow { get; init; } = new(20, 40, 10, 10, 10, 10);

    public void Validate()
    {
        if (AttributeFloor is < 1 or > 50 || CreationPoints is < 1 or > 100 ||
            CreationNaturalTotal is < 80 or > 200 || LevelTenNaturalTotal is < 100 or > 300 ||
            LevelTwentyFiveNaturalTotal is < 150 or > 400 ||
            CreationNaturalTotal != 8 * AttributeFloor + CreationPoints ||
            !(CreationNaturalTotal < LevelTenNaturalTotal && LevelTenNaturalTotal < LevelTwentyFiveNaturalTotal))
            throw new ArgumentOutOfRangeException(nameof(CombatAttributeTuning));
        foreach (var archetype in Archetypes)
            ValidateBase(BaseFor(archetype));
    }

    public static readonly string[] Archetypes = ["fighter", "guardian", "archer", "mage", "healer", "shadow"];
    public BaseAttributes BaseFor(string archetype) => archetype switch
    {
        "fighter" => Fighter, "guardian" => Guardian, "archer" => Archer,
        "mage" => Mage, "healer" => Healer, "shadow" => Shadow,
        _ => throw new ArgumentOutOfRangeException(nameof(archetype))
    };
    public CombatIdentity StarterIdentity => new("fighter", Fighter);
    public CombatIdentity IdentityFor(string archetype)
    {
        Validate();
        return new(archetype, BaseFor(archetype));
    }

    public void ValidateBase(BaseAttributes a)
    {
        var values = new[] { a.Strength, a.Dexterity, a.Constitution, a.Intelligence, a.Wisdom, a.Willpower };
        if (values.Any(v => v < AttributeFloor || v > AttributeFloor + 30) ||
            values.Sum() != 6 * AttributeFloor + CreationPoints)
            throw new ArgumentOutOfRangeException(nameof(a));
    }

    public CombatAttributeSnapshot Snapshot(CombatIdentity identity, int level)
    {
        Validate();
        if (!Archetypes.Contains(identity.Archetype, StringComparer.Ordinal))
            throw new ArgumentOutOfRangeException(nameof(identity));
        ValidateBase(identity.Base);
        int l = Math.Clamp(level, 1, 20);
        double total = l <= 10
            ? CreationNaturalTotal + (LevelTenNaturalTotal - CreationNaturalTotal) * (l - 1) / 9d
            : LevelTenNaturalTotal + (LevelTwentyFiveNaturalTotal - LevelTenNaturalTotal) * (l - 10) / 15d;
        int Scaled(int value) => AttributeFloor + (int)Math.Round(
            (total - 8 * AttributeFloor) * (value - AttributeFloor) / CreationPoints,
            MidpointRounding.AwayFromZero);
        var b = identity.Base;
        return new(identity.Archetype, l, new(Scaled(b.Strength), Scaled(b.Dexterity),
            Scaled(b.Constitution), Scaled(b.Intelligence), Scaled(b.Wisdom), Scaled(b.Willpower)));
    }
}
