namespace ShatteredFrontier.WorldContent;

public sealed record TuningKey(string Key, string Label, string Unit, double Default, double Min, double Max,
    string Permission, string EffectiveTime);

public static class WorldTuningCatalog
{
    public static readonly IReadOnlyList<TuningKey> Keys =
    [
        new("tune.world.respawn_jitter", "Respawn timing variation", "fraction", .10, 0, .25, "world.tuning.publish", "next scheduled respawn"),
        new("tune.world.scarcity_respawn_k", "Scarcity regeneration slowdown", "multiplier slope", .5, 0, 1, "world.tuning.publish", "next scheduled gathering respawn"),
        new("tune.world.spawn_point_cooldown_cycles", "Spawn point rest", "cycles", 2, 0, 8, "world.tuning.publish", "next point selection"),
        new("tune.world.npc_respawn_s.common", "Common NPC respawn", "seconds", 300, 30, 3600, "world.tuning.publish", "next scheduled NPC respawn"),
        new("tune.world.npc_respawn_s.elite", "Elite NPC respawn", "seconds", 900, 60, 7200, "world.tuning.publish", "next scheduled NPC respawn"),
        new("tune.world.npc_respawn_s.mini_boss", "Mini boss respawn", "seconds", 1800, 300, 14400, "world.tuning.publish", "next scheduled NPC respawn"),
        new("tune.world.dynamic_respawn_k", "NPC population respawn slope", "fraction per player", .15, 0, 1, "world.tuning.publish", "next scheduled NPC respawn"),
        new("tune.world.dynamic_respawn_floor", "NPC respawn speed floor", "fraction", .35, .1, 1, "world.tuning.publish", "next scheduled NPC respawn")
    ];
}

public sealed class WorldTuning
{
    private readonly Dictionary<string, double> values;
    public WorldTuning(IReadOnlyDictionary<string, double>? overrides = null)
    {
        values = WorldTuningCatalog.Keys.ToDictionary(k => k.Key, k => k.Default, StringComparer.Ordinal);
        if (overrides is null) return;
        foreach (var (key, value) in overrides)
        {
            var declaration = WorldTuningCatalog.Keys.SingleOrDefault(k => k.Key == key)
                ?? throw new ArgumentException($"Unknown world tuning key {key}");
            if (!double.IsFinite(value) || value < declaration.Min || value > declaration.Max ||
                (key == "tune.world.spawn_point_cooldown_cycles" && value != Math.Truncate(value)))
                throw new ArgumentOutOfRangeException(nameof(overrides), $"{key} must be within [{declaration.Min}, {declaration.Max}] {declaration.Unit}");
            values[key] = value;
        }
    }
    public double Get(string key) => values[key];
    public int PointCooldownCycles => (int)Get("tune.world.spawn_point_cooldown_cycles");
}
