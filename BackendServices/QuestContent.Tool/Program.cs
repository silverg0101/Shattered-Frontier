using ShatteredFrontier.QuestContent;

if (args.Length != 1) { Console.Error.WriteLine("Usage: QuestContent.Tool <content/quests directory>"); return 2; }
try
{
    var files = QuestLoader.LoadDirectory(args[0]);
    if (files.Count == 0) { Console.Error.WriteLine("No quest JSON files found."); return 2; }
    var content = Directory.GetParent(Path.GetFullPath(args[0]))?.FullName ?? throw new IOException("Quest content root missing.");
    var knownItems = new HashSet<string>(StringComparer.Ordinal)
    {
        "item/oak_log", "item/iron_ore", "item/rough_stone", "item/boar_hide"
    };
    foreach (var folder in new[] { "crafting", "loot" })
        foreach (var path in Directory.GetFiles(Path.Combine(content, folder), "*.json"))
        {
            using var document = System.Text.Json.JsonDocument.Parse(File.ReadAllText(path));
            Collect(document.RootElement);
        }
    void Collect(System.Text.Json.JsonElement element)
    {
        if (element.ValueKind == System.Text.Json.JsonValueKind.Array)
            foreach (var child in element.EnumerateArray()) Collect(child);
        if (element.ValueKind != System.Text.Json.JsonValueKind.Object) return;
        foreach (var property in element.EnumerateObject())
        {
            if (property.NameEquals("definition_id") && property.Value.ValueKind == System.Text.Json.JsonValueKind.String)
                knownItems.Add(property.Value.GetString()!);
            else Collect(property.Value);
        }
    }
    var issues = QuestValidator.ValidateAll(files, knownItems);
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
