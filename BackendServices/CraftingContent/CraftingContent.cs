using System.Text.Json;
using System.Text.RegularExpressions;
using ShatteredFrontier.InventoryLedger;

namespace ShatteredFrontier.CraftingContent;

public sealed record RecipeQuantity(string DefinitionId, int Quantity);
public sealed record Recipe(string Id, string Station, int DurationSeconds,
    IReadOnlyList<RecipeQuantity> Inputs, IReadOnlyList<RecipeQuantity> Outputs);
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

public sealed class CraftAttemptService(RecipeCatalog catalog, InventoryLedger.InventoryLedger ledger)
{
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
