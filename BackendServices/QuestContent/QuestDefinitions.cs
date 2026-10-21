using System.Text.Json;
using System.Text.RegularExpressions;

namespace ShatteredFrontier.QuestContent;

public sealed class QuestDefinition
{
    public string Id { get; set; } = "";
    public int Revision { get; set; }
    public string ZoneId { get; set; } = "";
    public List<QuestStep> Steps { get; set; } = [];
}

public sealed class QuestStep
{
    public string Id { get; set; } = "";
    public List<string> Prerequisites { get; set; } = [];
    public List<QuestObjective> Objectives { get; set; } = [];
    public List<QuestReward> Rewards { get; set; } = [];
}

public sealed class QuestObjective
{
    public string Key { get; set; } = "";
    public string Kind { get; set; } = "";
    public string TargetId { get; set; } = "";
    public string? NodeId { get; set; }
    public int Count { get; set; }
}

public sealed class QuestReward
{
    public string BundleId { get; set; } = "";
    public List<QuestRewardItem> Items { get; set; } = [];
}

public sealed class QuestRewardItem
{
    public string DefinitionId { get; set; } = "";
    public long Quantity { get; set; }
}

public static class QuestLoader
{
    public static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower,
        PropertyNameCaseInsensitive = true,
        UnmappedMemberHandling = System.Text.Json.Serialization.JsonUnmappedMemberHandling.Disallow
    };

    public static QuestDefinition Load(string path) => JsonSerializer.Deserialize<QuestDefinition>(File.ReadAllText(path), JsonOptions)
        ?? throw new JsonException($"Empty quest: {path}");

    public static IReadOnlyList<(string Path, QuestDefinition Quest)> LoadDirectory(string directory) =>
        Directory.GetFiles(directory, "*.json", SearchOption.TopDirectoryOnly)
            .OrderBy(p => p, StringComparer.Ordinal).Select(p => (p, Load(p))).ToArray();
}

public static class QuestValidator
{
    private static readonly Regex ContentId = new("^[a-z_]+/[a-z0-9_]+$", RegexOptions.CultureInvariant);
    private static readonly Regex Slug = new("^[a-z][a-z0-9_]*$", RegexOptions.CultureInvariant);

    public static IReadOnlyList<string> Validate(QuestDefinition quest, QuestTuning? tuning = null, IReadOnlySet<string>? knownItems = null)
    {
        tuning ??= new QuestTuning();
        tuning.Validate();
        var issues = new List<string>();
        if (!ValidId(quest.Id, "quest") || quest.Revision < 1) issues.Add("quest id/revision invalid");
        if (!ValidId(quest.ZoneId, "zone")) issues.Add("zone_id invalid");
        if (quest.Steps is null || quest.Steps.Count is < 1 or > 20) { issues.Add("steps count invalid"); return issues; }
        var seen = new HashSet<string>(StringComparer.Ordinal);
        foreach (var step in quest.Steps)
        {
            if (step is null) { issues.Add("step null"); continue; }
            if (!Slug.IsMatch(step.Id ?? "") || !seen.Add(step.Id ?? "")) issues.Add($"step id invalid/duplicate: {step.Id}");
            if (step.Prerequisites is null || step.Prerequisites.Distinct(StringComparer.Ordinal).Count() != step.Prerequisites.Count ||
                step.Prerequisites.Any(p => !seen.Contains(p))) issues.Add($"step {step.Id}: prerequisites must be unique earlier steps");
            if (step.Objectives is null || step.Objectives.Count == 0) { issues.Add($"step {step.Id}: objectives missing"); continue; }
            var keys = new HashSet<string>(StringComparer.Ordinal);
            foreach (var obj in step.Objectives)
            {
                if (obj is null) { issues.Add($"step {step.Id}: objective null"); continue; }
                if (!Slug.IsMatch(obj.Key ?? "") || !keys.Add(obj.Key ?? "")) issues.Add($"step {step.Id}: objective key invalid/duplicate");
                var prefix = obj.Kind switch { "gather" or "craft" => "item", "defeat" => "npc", "build" => "structure_tag", _ => "" };
                if (prefix.Length == 0 || !ValidId(obj.TargetId, prefix)) issues.Add($"step {step.Id}/{obj.Key}: kind/target invalid");
                if (obj.NodeId is not null && (obj.Kind != "gather" || !ValidId(obj.NodeId, "gathering_node"))) issues.Add($"step {step.Id}/{obj.Key}: node_id invalid");
                if (obj.Count < 1 || obj.Count > tuning.MaxObjectiveCount) issues.Add($"step {step.Id}/{obj.Key}: count outside 1..{tuning.MaxObjectiveCount}");
            }
            if (step.Rewards is null || step.Rewards.Any(r => r is null || !ValidId(r.BundleId, "reward")) ||
                step.Rewards.Select(r => r.BundleId).Distinct(StringComparer.Ordinal).Count() != step.Rewards.Count)
                issues.Add($"step {step.Id}: rewards invalid/duplicate");
            if (step.Rewards is not null)
                foreach (var reward in step.Rewards.Where(r => r is not null))
                    if (reward.Items is null || (reward.Items.Count == 0 && knownItems is not null) || reward.Items.Any(i => i is null ||
                        !ValidId(i.DefinitionId, "item") || i.Quantity is < 1 or > 1000000 ||
                        (knownItems is not null && !knownItems.Contains(i.DefinitionId))) ||
                        reward.Items.Select(i => i.DefinitionId).Distinct(StringComparer.Ordinal).Count() != reward.Items.Count)
                        issues.Add($"step {step.Id}: reward items invalid/unknown/duplicate");
        }
        return issues;
    }

    public static IReadOnlyList<string> ValidateAll(IReadOnlyList<(string Path, QuestDefinition Quest)> files, IReadOnlySet<string>? knownItems = null)
    {
        var issues = new List<string>();
        var ids = new HashSet<string>(StringComparer.Ordinal);
        foreach (var (path, quest) in files)
        {
            issues.AddRange(Validate(quest, knownItems: knownItems).Select(i => $"{path}: {i}"));
            if (!ids.Add(quest.Id)) issues.Add($"{path}: duplicate quest id {quest.Id}");
        }
        return issues;
    }

    private static bool ValidId(string? id, string prefix) => id is not null && ContentId.IsMatch(id) && id.StartsWith(prefix + "/", StringComparison.Ordinal);
}

public sealed record QuestTuning(int MaxObjectiveCount = 500)
{
    public const string Key = "tune.quest.max_objective_count";
    public const string Label = "Maximum objective count";
    public const string Unit = "units";
    public const string Permission = "LiveOps approved configuration publisher";
    public const string EffectiveTime = "New quest acceptances after configuration publication";
    public void Validate()
    {
        if (MaxObjectiveCount is < 1 or > 5000) throw new ArgumentOutOfRangeException(nameof(MaxObjectiveCount));
    }
}
