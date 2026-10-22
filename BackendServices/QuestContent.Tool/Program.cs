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
    // Resident roles are the townsfolk census archetypes (talk_resident objectives); the file's other keys are settings.
    var archetypes = Path.Combine(Directory.GetParent(content)?.FullName ?? "", "GameClient", "ArtSource", "Townsfolk", "townsfolk-archetypes.json");
    using var archetypeDocument = System.Text.Json.JsonDocument.Parse(File.ReadAllText(archetypes));
    var knownRoles = archetypeDocument.RootElement.EnumerateObject()
        .Where(p => p.Value.ValueKind == System.Text.Json.JsonValueKind.Object && p.Name != "default")
        .Select(p => p.Name).ToHashSet(StringComparer.Ordinal);
    // NPC ids are the dialogue file stems; dialogue offers must name a quest that lists that NPC as giver.
    var dialogue = Directory.GetFiles(Path.Combine(content, "dialogue"), "*.json");
    var knownNpcs = dialogue.Select(p => Path.GetFileNameWithoutExtension(p)!).ToHashSet(StringComparer.Ordinal);
    var offers = new List<(string, string)>();
    foreach (var path in dialogue)
    {
        using var doc = System.Text.Json.JsonDocument.Parse(File.ReadAllText(path));
        if (doc.RootElement.TryGetProperty("offer", out var offer) && offer.TryGetProperty("quest_id", out var id) && id.GetString() is { } questId)
            offers.Add((Path.GetFileNameWithoutExtension(path), questId));
    }
    var issues = QuestValidator.ValidateAll(files, knownItems, knownRoles, knownNpcs).Concat(QuestValidator.ValidateDialogueOffers(offers, files.Select(f => f.Quest))).ToList();
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
