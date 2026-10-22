using System.Text.Json;
using System.Text.RegularExpressions;
using ShatteredFrontier.InventoryLedger;

namespace ShatteredFrontier.CraftingContent;

public sealed record RecipeQuantity(string DefinitionId, int Quantity, bool QualityInput = false);
public sealed record RecipeLearn(string[] Sources, int? LearnSkillMin = null, string[]? Trainers = null);
public sealed record RecipeDiscovery(string RecipeId, int Weight);
public sealed record Recipe(string Id, string Station, int DurationSeconds,
    IReadOnlyList<RecipeQuantity> Inputs, IReadOnlyList<RecipeQuantity> Outputs,
    string Skill = "", int Difficulty = 0, RecipeLearn? Learn = null,
    IReadOnlyList<RecipeDiscovery>? Discovers = null);
public sealed record RecipeFile(IReadOnlyList<Recipe> Recipes);
public sealed class CraftContentException(string message) : InvalidOperationException(message);

public sealed class RecipeCatalog
{
    private static readonly JsonSerializerOptions JsonOptions = new() { PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower, PropertyNameCaseInsensitive = false };
    private static readonly Regex IdPattern = new("^[a-z]+/[a-z][a-z0-9_]*$", RegexOptions.CultureInvariant);
    private static readonly HashSet<string> Stations = ["sawmill", "forge", "workbench"];
    private readonly Dictionary<string, Recipe> recipes;
    public IReadOnlyCollection<Recipe> Recipes => recipes.Values;

    private RecipeCatalog(Dictionary<string, Recipe> recipes) => this.recipes = recipes;
    public Recipe Get(string id) => recipes.TryGetValue(id, out var recipe) ? recipe : throw new CraftContentException($"Unknown recipe: {id}");

    public static RecipeCatalog LoadFiles(IEnumerable<string> paths)
    {
        var all = new Dictionary<string, Recipe>(StringComparer.Ordinal);
        foreach (var path in paths.OrderBy(x => x, StringComparer.Ordinal))
        {
            RecipeFile? file;
            try { file = JsonSerializer.Deserialize<RecipeFile>(File.ReadAllText(path), JsonOptions); }
            catch (Exception ex) when (ex is JsonException or IOException) { throw new CraftContentException($"Invalid recipe file {path}: {ex.Message}"); }
            if (file?.Recipes is null || file.Recipes.Count == 0) throw new CraftContentException($"No recipes in {path}.");
            foreach (var recipe in file.Recipes)
            {
                Validate(recipe);
                if (!all.TryAdd(recipe.Id, recipe)) throw new CraftContentException($"Duplicate recipe ID: {recipe.Id}");
            }
        }
        if (all.Count == 0) throw new CraftContentException("No recipe files found.");
        foreach (var recipe in all.Values)
            foreach (var discovery in recipe.Discovers ?? [])
                if (!all.TryGetValue(discovery.RecipeId, out var target) ||
                    target.Learn?.Sources.Contains("discovery", StringComparer.Ordinal) != true)
                    throw new CraftContentException($"Invalid discovery target: {discovery.RecipeId}");
        CheckCycles(all.Values);
        return new RecipeCatalog(all);
    }

    private static void Validate(Recipe? recipe)
    {
        if (recipe is null || !ValidId(recipe.Id, "recipe") || !Stations.Contains(recipe.Station) ||
            recipe.DurationSeconds is < 1 or > 3600 || recipe.Inputs is null || recipe.Outputs is null ||
            recipe.Inputs.Count is < 1 or > 8 || recipe.Outputs.Count is < 1 or > 8)
            throw new CraftContentException("Invalid recipe identity, station, duration, or slot count.");
        foreach (var slots in new[] { recipe.Inputs, recipe.Outputs })
        {
            var seen = new HashSet<string>(StringComparer.Ordinal);
            foreach (var slot in slots)
                if (slot is null || !ValidId(slot.DefinitionId, "item") || slot.Quantity is < 1 or > 1000 || !seen.Add(slot.DefinitionId))
                    throw new CraftContentException($"Invalid or duplicate item slot in {recipe.Id}.");
        }
        if (recipe.Inputs.Any(i => recipe.Outputs.Any(o => i.DefinitionId == o.DefinitionId)))
            throw new CraftContentException($"Recipe transforms an item into itself: {recipe.Id}.");
        if (recipe.Skill is not ("carpentry" or "smithing" or "leatherworking") || recipe.Difficulty is < 0 or > 1100 ||
            recipe.Learn is null || recipe.Learn.Sources is null || recipe.Learn.Sources.Length == 0 ||
            recipe.Learn.Sources.Distinct(StringComparer.Ordinal).Count() != recipe.Learn.Sources.Length ||
            recipe.Learn.Sources.Any(s => s is not ("default" or "trainer" or "scroll" or "discovery")) ||
            recipe.Learn.LearnSkillMin is < 0 or > 1000 ||
            (recipe.Learn.Sources.Contains("trainer") && (recipe.Learn.Trainers is null || recipe.Learn.Trainers.Length == 0)) ||
            (recipe.Learn.Trainers?.Any(t => !ValidId(t, "npc")) ?? false) ||
            recipe.Outputs.Any(o => o.QualityInput) ||
            recipe.Discovers?.Any(d => !ValidId(d.RecipeId, "recipe") || d.Weight is < 1 or > 100) == true ||
            (recipe.Discovers?.Sum(d => d.Weight) ?? 0) > 100)
            throw new CraftContentException($"Invalid crafting skill, learning, or quality fields in {recipe.Id}.");
    }

    private static bool ValidId(string? id, string kind) => id is not null && IdPattern.IsMatch(id) && id.StartsWith(kind + "/", StringComparison.Ordinal);

    private static void CheckCycles(IEnumerable<Recipe> recipes)
    {
        var edges = new Dictionary<string, HashSet<string>>(StringComparer.Ordinal);
        foreach (var recipe in recipes)
            foreach (var input in recipe.Inputs)
            {
                if (!edges.TryGetValue(input.DefinitionId, out var next)) edges[input.DefinitionId] = next = new(StringComparer.Ordinal);
                foreach (var output in recipe.Outputs) next.Add(output.DefinitionId);
            }
        var active = new HashSet<string>(StringComparer.Ordinal);
        var done = new HashSet<string>(StringComparer.Ordinal);
        bool Visit(string id)
        {
            if (active.Contains(id)) return true;
            if (!done.Add(id)) return false;
            active.Add(id);
            if (edges.TryGetValue(id, out var next)) foreach (var child in next) if (Visit(child)) return true;
            active.Remove(id);
            return false;
        }
        foreach (var id in edges.Keys) if (Visit(id)) throw new CraftContentException("Recipe item dependency cycle.");
    }
}

public sealed record CraftAttempt(string OperationId, string ContainerId, long ExpectedRevision,
    string RecipeId, string Station, int Count);
public sealed record GradedCraftResult(LedgerReceipt Receipt, int QualityScore, CraftGrade Grade);

public sealed class CraftAttemptService(RecipeCatalog catalog, InventoryLedger.InventoryLedger ledger)
{
    public GradedCraftResult ExecuteGraded(CraftAttempt attempt, int skill,
        IReadOnlyDictionary<string, string> inputGrades, byte[] secret, CraftingControls controls,
        int? replayScore = null, string discoveryId = "")
    {
        if (attempt.Count is < 1 or > 20) throw new CraftContentException("Craft count must be 1–20.");
        var recipe = catalog.Get(attempt.RecipeId);
        if (attempt.Station != recipe.Station || inputGrades.Keys.Any(k => !recipe.Inputs.Any(i => i.DefinitionId == k)))
            throw new CraftContentException("Wrong station or input grade key.");
        var inputs = recipe.Inputs.Select(i => new ItemQuantity(
            inputGrades.TryGetValue(i.DefinitionId, out var selected)
                ? CraftingQuality.DefinitionId(i.DefinitionId, CraftingQuality.ParseGrade(i.DefinitionId, selected))
                : i.DefinitionId, checked((long)i.Quantity * attempt.Count))).ToArray();
        var seed = CraftingQuality.Roll(attempt.OperationId, secret, controls);
        var score = replayScore ?? CraftingQuality.Score(skill, recipe.Difficulty,
            CraftingQuality.MaterialMidpoint(recipe, inputGrades), seed.Variance, controls);
        if (score is < 0 or > 100) throw new CraftContentException("Invalid replay quality score.");
        var grade = CraftingQuality.Grade(score);
        var outputs = recipe.Outputs.Select(o => new ItemQuantity(CraftingQuality.DefinitionId(o.DefinitionId, grade),
            checked((long)o.Quantity * attempt.Count))).ToArray();
        var receipt = ledger.Convert(attempt.OperationId, attempt.ContainerId, attempt.ExpectedRevision,
            $"{recipe.Id}|quality={score}|discovery={(discoveryId == "" ? "none" : discoveryId)}", inputs, outputs);
        return new(receipt, score, grade);
    }
    public LedgerReceipt Execute(CraftAttempt attempt)
    {
        if (attempt.Count is < 1 or > 20) throw new CraftContentException("Craft count must be 1–20.");
        var recipe = catalog.Get(attempt.RecipeId);
        if (attempt.Station != recipe.Station) throw new CraftContentException("Wrong crafting station.");
        static ItemQuantity[] Scale(IReadOnlyList<RecipeQuantity> slots, int count) =>
            slots.Select(x => new ItemQuantity(x.DefinitionId, checked((long)x.Quantity * count))).ToArray();
        return ledger.Convert(attempt.OperationId, attempt.ContainerId, attempt.ExpectedRevision, recipe.Id,
            Scale(recipe.Inputs, attempt.Count), Scale(recipe.Outputs, attempt.Count));
    }
}
