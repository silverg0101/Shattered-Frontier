using ShatteredFrontier.WorldContent;

if (args.Length >= 4 && args[0] == "combat-pilot")
    return CombatPilotBridge.Run(args[1..]);

if (args.Length != 1)
{
    Console.Error.WriteLine("Usage: WorldContent.Tool <content/world/zones directory>");
    return 2;
}
try
{
    var files = ZoneLoader.LoadDirectory(args[0]);
    if (files.Count == 0) { Console.Error.WriteLine("No zone JSON files found."); return 2; }
    var issues = WorldContentValidator.ValidateAll(files);
    foreach (var issue in issues) Console.Error.WriteLine(issue);
    if (issues.Count > 0) return 1;
    Console.WriteLine($"Validated {files.Count} world zone record(s).");
    return 0;
}
catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or System.Text.Json.JsonException)
{
    Console.Error.WriteLine(ex.Message);
    return 2;
}
