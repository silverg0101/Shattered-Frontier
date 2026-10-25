namespace ShatteredFrontier.WorldContent;

public static class SilverbrookCombatGroups
{
    public static bool Contains(string id) => id is
        "boar_farm_edge" or "boar_forest_edge" or "wolf_northwest_den" or
        "bandit_forest_trail_camp" or "wolf_alpha_northwest_den" or "bandit_captain_forest_trail_camp";

    public static string Tier(string? definitionId) => definitionId switch
    {
        "npc/wolf_alpha" => "elite", "npc/bandit_captain" => "mini_boss", _ => "common"
    };
}
