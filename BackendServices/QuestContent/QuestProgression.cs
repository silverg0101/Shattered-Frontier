namespace ShatteredFrontier.QuestContent;

public sealed record ObjectiveEvent(string OperationId, string CharacterId, string QuestId, string StepId,
    string Kind, string TargetId, string? NodeId, int Quantity, DateTimeOffset OccurredAt);
public sealed record RewardGrant(string OperationId, string CharacterId, string QuestId, string StepId, string BundleId);
public sealed record ProgressResult(bool Accepted, string Reason, string? ActiveStepId, bool Completed,
    IReadOnlyList<RewardGrant> Grants);

public sealed class QuestProgression
{
    private readonly QuestDefinition _quest;
    private readonly Dictionary<string, CharacterState> _characters = new(StringComparer.Ordinal);

    private sealed class CharacterState(DateTimeOffset acceptedAt)
    {
        public DateTimeOffset AcceptedAt { get; } = acceptedAt;
        public DateTimeOffset LastOccurredAt { get; set; } = acceptedAt;
        public int StepIndex { get; set; }
        public Dictionary<string, int> Counts { get; } = new(StringComparer.Ordinal);
        public HashSet<string> Operations { get; } = new(StringComparer.Ordinal);
        public HashSet<string> GrantedSteps { get; } = new(StringComparer.Ordinal);
    }

    public QuestProgression(QuestDefinition quest)
    {
        var issues = QuestValidator.Validate(quest);
        if (issues.Count != 0) throw new ArgumentException(string.Join("; ", issues), nameof(quest));
        _quest = quest;
    }

    public bool Accept(string characterId, DateTimeOffset acceptedAt) =>
        !string.IsNullOrWhiteSpace(characterId) && _characters.TryAdd(characterId, new CharacterState(acceptedAt));

    public ProgressResult Apply(ObjectiveEvent evidence)
    {
        if (!_characters.TryGetValue(evidence.CharacterId, out var state) || evidence.QuestId != _quest.Id)
            return Reject("quest not accepted", null, false);
        if (state.StepIndex == _quest.Steps.Count) return Reject("quest completed", null, true);
        var step = _quest.Steps[state.StepIndex];
        if (string.IsNullOrWhiteSpace(evidence.OperationId) || state.Operations.Contains(evidence.OperationId))
            return Reject("duplicate operation", step.Id, false);
        if (evidence.StepId != step.Id || evidence.OccurredAt < state.AcceptedAt || evidence.OccurredAt < state.LastOccurredAt)
            return Reject("stale event", step.Id, false);
        if (evidence.Quantity < 1) return Reject("quantity invalid", step.Id, false);
        var objective = step.Objectives.FirstOrDefault(o => o.Kind == evidence.Kind && o.TargetId == evidence.TargetId &&
            (o.NodeId is null || o.NodeId == evidence.NodeId));
        if (objective is null) return Reject("objective mismatch", step.Id, false);
        if (state.Counts.GetValueOrDefault(objective.Key) >= objective.Count) return Reject("objective complete", step.Id, false);
        state.Operations.Add(evidence.OperationId);
        state.LastOccurredAt = evidence.OccurredAt;
        state.Counts[objective.Key] = (int)Math.Min(objective.Count,
            state.Counts.GetValueOrDefault(objective.Key) + (long)evidence.Quantity);
        var grants = new List<RewardGrant>();
        if (step.Objectives.All(o => state.Counts.GetValueOrDefault(o.Key) >= o.Count))
        {
            if (state.GrantedSteps.Add(step.Id))
                grants.AddRange(step.Rewards.Select(r => new RewardGrant(
                    $"quest:{evidence.CharacterId}:{_quest.Id}:{step.Id}:{r.BundleId}", evidence.CharacterId, _quest.Id, step.Id, r.BundleId)));
            state.StepIndex++;
            state.Counts.Clear();
        }
        var complete = state.StepIndex == _quest.Steps.Count;
        return new ProgressResult(true, "accepted", complete ? null : _quest.Steps[state.StepIndex].Id, complete, grants);
    }

    private static ProgressResult Reject(string reason, string? step, bool completed) => new(false, reason, step, completed, []);
}
