using ShatteredFrontier.CraftingContent;
using System.Text.Json;

if (args.Length != 1 || !Directory.Exists(args[0]))
{
    Console.Error.WriteLine("Usage: CraftingContent.Tool <recipe-directory>");
    return 2;
}
try
{
    var catalog = RecipeCatalog.LoadFiles(Directory.EnumerateFiles(args[0], "*.json"));
    var dialogueDirectory = Path.Combine(Directory.GetParent(args[0])?.FullName ?? "", "dialogue");
    if (Directory.Exists(dialogueDirectory))
        foreach (var file in Directory.EnumerateFiles(dialogueDirectory, "*.json"))
        {
            using var doc = JsonDocument.Parse(File.ReadAllText(file));
            foreach (var choice in doc.RootElement.GetProperty("nodes").EnumerateObject()
                         .SelectMany(n => n.Value.GetProperty("choices").EnumerateArray()))
            {
                if (!choice.TryGetProperty("action", out var action) ||
                    !action.GetString()!.StartsWith("learn:", StringComparison.Ordinal)) continue;
                var recipe = catalog.Get(action.GetString()!["learn:".Length..]);
                if (recipe.Learn?.Trainers?.Contains("npc/" + Path.GetFileNameWithoutExtension(file)) != true)
                    throw new CraftContentException($"Dialogue learn action has no matching trainer: {file}");
            }
        }
    Console.WriteLine($"Validated {catalog.Recipes.Count} crafting recipes.");
    return 0;
}
catch (CraftContentException ex)
{
    Console.Error.WriteLine(ex.Message);
    return 1;
}
