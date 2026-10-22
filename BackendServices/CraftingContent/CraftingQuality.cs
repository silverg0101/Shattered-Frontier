using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;

namespace ShatteredFrontier.CraftingContent;

// Release-authored effective values. The configuration service publishes only bounded values,
// captured once for a craft attempt; grade thresholds and the formula are versioned code.
public sealed record CraftingControls(
    int QSkillMax = 50, int SkillMarginLow = 100, int SkillMarginFull = 200,
    int QMaterialMax = 20, int QToolMax = 10, int DefaultToolQuality = 50,
    int QualityVariance = 5, double DiscoveryBase = .02,
    double TrainingPerDifficultyCopper = .5,
    int MaxKnownRecipes = 500)
{
    public const string Permission = "crafting.tuning.publish";
    public const string EffectiveTime = "new attempts";
    public sealed record Declaration(string Key, string Label, string Help, string Type, string Unit,
        double Default, double Minimum, double Maximum, double Step,
        string Permission = CraftingControls.Permission, string ApplyTime = CraftingControls.EffectiveTime,
        string Scope = "realm", string SafeFallback = "shipped default", string Approval = "normal LiveOps",
        bool ClientVisible = true, string Observability = "effective crafting snapshot and receipt",
        string Rollback = "next attempt");
    public static IReadOnlyList<Declaration> Declarations { get; } =
    [
        new("tune.crafting.q_skill_max", "Skill quality contribution", "Maximum quality points from skill.", "integer", "points", 50, 0, 100, 1),
        new("tune.crafting.skill_margin_low", "Skill quality start margin", "Skill margin below difficulty that starts quality gain.", "integer", "skill", 100, 0, 500, 1),
        new("tune.crafting.skill_margin_full", "Skill quality full margin", "Skill margin above difficulty for full quality gain.", "integer", "skill", 200, 1, 1000, 1),
        new("tune.crafting.q_material_max", "Material quality contribution", "Maximum quality points from materials.", "integer", "points", 20, 0, 100, 1),
        new("tune.crafting.q_tool_max", "Tool quality contribution", "Maximum quality points from tools.", "integer", "points", 10, 0, 100, 1),
        new("tune.crafting.default_tool_quality", "Default tool quality", "Tool quality until tool grades are enabled.", "integer", "quality", 50, 0, 100, 1),
        new("tune.crafting.quality_variance", "Craft quality variation", "Maximum random quality points in either direction.", "integer", "points", 5, 0, 25, 1),
        new("tune.crafting.discovery_base", "Practice discovery rate", "Base chance to discover a recipe after a craft.", "probability", "probability", .02, 0, .5, .001),
        new("tune.crafting.trainer_cost_per_difficulty_copper", "Trainer fee by difficulty", "Copper charged per recipe difficulty point.", "decimal", "copper per difficulty point", .5, 0, 50, .1),
        new("tune.crafting.max_known_recipes", "Known recipe limit", "Maximum learned recipes per character.", "integer", "recipes", 500, 50, 5000, 1)
    ];
    public void Validate()
    {
        if (QSkillMax is < 0 or > 100 || SkillMarginLow is < 0 or > 500 || SkillMarginFull is < 1 or > 1000 ||
            QMaterialMax is < 0 or > 100 || QToolMax is < 0 or > 100 || DefaultToolQuality is < 0 or > 100 ||
            QualityVariance is < 0 or > 25 || !double.IsFinite(DiscoveryBase) || DiscoveryBase is < 0 or > .5 ||
            !double.IsFinite(TrainingPerDifficultyCopper) || TrainingPerDifficultyCopper is < 0 or > 50 ||
            MaxKnownRecipes is < 50 or > 5000 || QSkillMax + QMaterialMax + QToolMax > 100)
            throw new ArgumentOutOfRangeException(nameof(CraftingControls));
    }
    public int TrainerFee(int difficulty) => difficulty is < 0 or > 1100 ? throw new ArgumentOutOfRangeException(nameof(difficulty))
        : checked((int)Math.Floor(TrainingPerDifficultyCopper * difficulty + .5));
}

public enum CraftGrade { Crude, Common, Fine, Superior, Masterwork }

public static class CraftingQuality
{
    public static int? StoredScore(string? fingerprint)
    {
        var match = Regex.Match(fingerprint ?? "", @"\|quality=(\d{1,3})", RegexOptions.CultureInvariant);
        return match.Success && int.TryParse(match.Groups[1].Value, out var score) && score is >= 0 and <= 100 ? score : null;
    }
    public static string? StoredDiscovery(string? fingerprint)
    {
        var match = Regex.Match(fingerprint ?? "", @"\|discovery=(none|recipe/[a-z][a-z0-9_]*)", RegexOptions.CultureInvariant);
        if (!match.Success) throw new CraftContentException("Stored craft discovery missing.");
        return match.Groups[1].Value == "none" ? null : match.Groups[1].Value;
    }
    public static CraftGrade Grade(int score) => score switch
    {
        < 25 => CraftGrade.Crude, < 50 => CraftGrade.Common, < 75 => CraftGrade.Fine,
        < 90 => CraftGrade.Superior, _ => CraftGrade.Masterwork
    };
    public static int Midpoint(CraftGrade grade) => grade switch
    {
        CraftGrade.Crude => 12, CraftGrade.Common => 37, CraftGrade.Fine => 62,
        CraftGrade.Superior => 82, _ => 95
    };
    public static string DefinitionId(string baseId, CraftGrade grade) => grade == CraftGrade.Common
        ? baseId : baseId + "/" + grade.ToString().ToLowerInvariant();
    public static CraftGrade ParseGrade(string baseId, string selectedId)
    {
        if (selectedId == baseId) return CraftGrade.Common;
        foreach (var grade in Enum.GetValues<CraftGrade>())
            if (grade != CraftGrade.Common && selectedId == DefinitionId(baseId, grade)) return grade;
        throw new CraftContentException($"Invalid input grade: {selectedId}");
    }
    public static double MaterialMidpoint(Recipe recipe, IReadOnlyDictionary<string, string> selected)
    {
        long weighted = 0, quantity = 0;
        foreach (var input in recipe.Inputs.Where(i => i.QualityInput))
        {
            var grade = ParseGrade(input.DefinitionId, selected.GetValueOrDefault(input.DefinitionId, input.DefinitionId));
            weighted += (long)input.Quantity * Midpoint(grade);
            quantity += input.Quantity;
        }
        return quantity == 0 ? 37 : (double)weighted / quantity;
    }
    public static int Score(int skill, int difficulty, double materialMidpoint, int variance, CraftingControls controls)
    {
        controls.Validate();
        if (skill is < 0 or > 1000 || difficulty is < 0 or > 1100 || !double.IsFinite(materialMidpoint) || materialMidpoint is < 0 or > 100 ||
            variance < -controls.QualityVariance || variance > controls.QualityVariance) throw new ArgumentOutOfRangeException();
        if (skill < difficulty) return Math.Clamp((int)Math.Floor(controls.QMaterialMax * materialMidpoint / 100d +
            controls.QToolMax * controls.DefaultToolQuality / 100d + variance + .5), 0, 24);
        double q = controls.QSkillMax * Math.Clamp((skill - difficulty + controls.SkillMarginLow) /
            (double)(controls.SkillMarginLow + controls.SkillMarginFull), 0, 1) +
            controls.QMaterialMax * materialMidpoint / 100d +
            controls.QToolMax * controls.DefaultToolQuality / 100d + variance;
        return Math.Clamp((int)Math.Floor(q + .5), 0, 100);
    }
    public static (int Variance, double DiscoveryRoll, int Choice) Roll(string operationId, byte[] secret, CraftingControls controls)
    {
        controls.Validate();
        if (string.IsNullOrWhiteSpace(operationId) || secret.Length < 16) throw new ArgumentException("Craft seed and secret required.");
        var bytes = HMACSHA256.HashData(secret, Encoding.UTF8.GetBytes(operationId));
        int width = 2 * controls.QualityVariance + 1;
        return ((int)(BitConverter.ToUInt32(bytes, 0) % width) - controls.QualityVariance,
            BitConverter.ToUInt32(bytes, 4) / ((double)uint.MaxValue + 1),
            (int)(BitConverter.ToUInt32(bytes, 8) % int.MaxValue));
    }
    public static string? Discover(Recipe recipe, int skill, int score, double roll, int choice,
        IReadOnlySet<string> known, RecipeCatalog catalog, CraftingControls controls)
    {
        var eligible = (recipe.Discovers ?? []).Where(d => !known.Contains(d.RecipeId) &&
            skill >= (catalog.Get(d.RecipeId).Learn?.LearnSkillMin ?? Math.Max(0, catalog.Get(d.RecipeId).Difficulty - 100))).ToArray();
        if (eligible.Length == 0 || roll >= controls.DiscoveryBase * score / 100d) return null;
        int pick = choice % eligible.Sum(d => d.Weight);
        foreach (var candidate in eligible) { pick -= candidate.Weight; if (pick < 0) return candidate.RecipeId; }
        return null;
    }
}
