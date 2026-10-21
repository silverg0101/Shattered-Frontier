using ShatteredFrontier.QuestContent;

if (args.Length != 1) { Console.Error.WriteLine("Usage: QuestContent.Tool <content/quests directory>"); return 2; }
try
{
    var files = QuestLoader.LoadDirectory(args[0]);
    if (files.Count == 0) { Console.Error.WriteLine("No quest JSON files found."); return 2; }
    var issues = QuestValidator.ValidateAll(files);
    foreach (var issue in issues) Console.Error.WriteLine(issue);
    if (issues.Count != 0) return 1;
    Console.WriteLine($"Validated {files.Count} quest record(s).");
    return 0;
}
catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or System.Text.Json.JsonException)
{
    Console.Error.WriteLine(ex.Message);
    return 2;
}
