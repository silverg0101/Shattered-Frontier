using ShatteredFrontier.Rewards;

if (args.Length != 1 || !Directory.Exists(args[0]))
{
    Console.Error.WriteLine("Usage: Rewards.Tool <content/loot directory>");
    return 2;
}
try
{
    var tables = LootCatalog.LoadFiles(Directory.EnumerateFiles(args[0], "*.json"));
    LootCatalog.Validate(tables["loot/greenlands_silverbrook_boar"]);
    Console.WriteLine($"Validated {tables.Count} loot table(s).");
    return 0;
}
catch (Exception ex) when (ex is RewardContentException or IOException or UnauthorizedAccessException or KeyNotFoundException)
{
    Console.Error.WriteLine(ex.Message);
    return 1;
}
