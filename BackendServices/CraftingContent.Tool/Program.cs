using ShatteredFrontier.CraftingContent;

if (args.Length != 1 || !Directory.Exists(args[0]))
{
    Console.Error.WriteLine("Usage: CraftingContent.Tool <recipe-directory>");
    return 2;
}
try
{
    var catalog = RecipeCatalog.LoadFiles(Directory.EnumerateFiles(args[0], "*.json"));
    Console.WriteLine($"Validated {catalog.Recipes.Count} crafting recipes.");
    return 0;
}
catch (CraftContentException ex)
{
    Console.Error.WriteLine(ex.Message);
    return 1;
}
