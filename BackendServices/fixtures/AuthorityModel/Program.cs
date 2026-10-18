using System.Text.Json;

// An executable specification experiment, never a service/credential verifier or database implementation.
var results = new List<object>();
void Case(string name, Action body)
{
    body();
    results.Add(new { name, passed = true });
}
static void Assert(bool condition, string why)
{
    if (!condition) throw new InvalidOperationException(why);
}
Case("complete_requires_all_applied", () =>
{
    var m = Model.Prepared(); m.Decide(1, "w1", Decision.Commit);
    Assert(!m.Complete(1, "w1"), "Premature completion");
    m.Deliver(m.Lookup("inventory", Phase.Apply));
    Assert(!m.Complete(1, "w1") && !m.Nodes["inventory"].Released, "Partial application exposed output");
    m.Recover(); m.Check();
});
Case("immutable_decision_and_replay", () =>
{
    var m = Model.Prepared(); m.Decide(1, "w1", Decision.Commit); var origin = m.DecisionOrigin;
    Assert(!m.Decide(1, "w1", Decision.Abort), "Commit reversed");
    m.Recover(); m.Recover(); m.Check();
    Assert(m.DecisionOrigin == origin && m.Nodes.Values.All(n => n.Effects == 1), "Replay changed history");
});
Case("stale_coordinator_cannot_decide", () =>
{
    var m = Model.Prepared(); m.Takeover("w2");
    Assert(!m.Decide(1, "w1", Decision.Commit), "Stale worker committed");
    Assert(!m.Decide(2, "w2", Decision.Commit), "Missing barriers permitted decision");
    m.BarrierAll(); Assert(m.Decide(2, "w2", Decision.Commit), "Reconciled takeover cannot proceed"); m.Recover();
});
Case("absent_participant_barrier_tombstone", () =>
{
    var m = new Model(); var late = m.Lookup("inventory", Phase.Prepare); m.Takeover("w2");
    Assert(m.Deliver(m.Lookup("inventory", Phase.Barrier)), "Barrier did not create empty record");
    Assert(!m.Deliver(late) && !m.Nodes["inventory"].Prepared, "Delayed prepare recreated old authority");
});
Case("old_apply_before_barrier_reconciles", () =>
{
    var m = Model.Prepared(); m.Decide(1, "w1", Decision.Commit); var old = m.Lookup("inventory", Phase.Apply);
    m.Takeover("w2"); Assert(m.Deliver(old), "Before-barrier snapshot unexpectedly rejected");
    m.BarrierAll(); m.Recover(); m.Check(); Assert(m.Nodes["inventory"].Effects == 1, "Old application charged twice");
});
Case("old_apply_after_barrier_rejects", () =>
{
    var m = Model.Prepared(); m.Decide(1, "w1", Decision.Commit); var old = m.Lookup("inventory", Phase.Apply);
    m.Takeover("w2"); m.BarrierAll(); Assert(!m.Deliver(old), "Old phase crossed receiver barrier"); m.Recover();
});
Case("same_generation_wrong_worker_rejects", () =>
{
    var m = Model.Prepared(); var wrong = m.Lookup("inventory", Phase.Prepare) with { Worker = "impostor" };
    Assert(!m.Deliver(wrong), "Same generation changed worker");
});
Case("higher_generation_requires_barrier", () =>
{
    var m = Model.Prepared(); m.Takeover("w2");
    Assert(!m.Deliver(m.Lookup("inventory", Phase.Prepare)), "Phase raised generation without barrier");
});
Case("partial_barriers_cannot_issue_new_phase_authority", () =>
{
    var m = Model.Prepared(); m.Decide(1, "w1", Decision.Commit); m.Takeover("w2");
    m.Deliver(m.Lookup("inventory", Phase.Barrier));
    foreach (var phase in new[] { Phase.Prepare, Phase.Apply, Phase.Complete })
        Assert(!m.Deliver(m.Lookup("inventory", phase)), "Incomplete barrier set issued fresh authority");
    m.BarrierAll(); m.Recover(); m.Check();
});
Case("prepare_after_apply_or_complete_rejects", () =>
{
    var m = Model.Prepared(); var late = m.Lookup("inventory", Phase.Prepare); m.Decide(1, "w1", Decision.Commit);
    m.Deliver(m.Lookup("inventory", Phase.Apply)); Assert(!m.Deliver(late), "Prepare accepted after apply");
    m.Recover(); Assert(!m.Deliver(late), "Prepare accepted after completion"); m.Check();
});
Case("durable_decision_cannot_issue_fresh_prepare", () =>
{
    var m = Model.Prepared(); m.Decide(1, "w1", Decision.Commit);
    Assert(!m.Deliver(m.Lookup("inventory", Phase.Prepare)), "Decided action issued new prepare lookup");
    m.Recover(); m.Check();
});
Case("forged_higher_generation_cannot_create_barrier", () =>
{
    var m = Model.Prepared(); var forged = m.Lookup("inventory", Phase.Barrier) with { Generation = 999, Worker = "other" };
    Assert(!m.Deliver(forged) && m.Nodes["inventory"].Generation == 1, "Altered lookup evidence enrolled new generation");
});
Case("lookup_identity_and_evidence_binding", () =>
{
    var m = Model.Prepared(); m.Decide(1, "w1", Decision.Commit); var valid = m.Lookup("inventory", Phase.Apply);
    Assert(!m.Deliver(valid with { Authenticated = false }), "Unauthenticated evidence accepted");
    Assert(!m.Deliver(valid with { Intent = "other" }), "Changed intent accepted");
    Assert(!m.Deliver(valid with { Realm = "other" }), "Changed realm accepted");
    Assert(!m.Deliver(valid with { Action = "other" }), "Changed action accepted");
    Assert(!m.Deliver(valid with { Payload = "other" }), "Changed phase payload accepted");
    Assert(!m.Deliver(valid with { DecisionOrigin = "other" }), "Changed decision origin accepted");
});
Case("abort_tombstone_blocks_late_prepare", () =>
{
    var m = new Model(); var late = m.Lookup("inventory", Phase.Prepare);
    Assert(m.Decide(1, "w1", Decision.Abort), "Undecided abort refused");
    foreach (var p in m.Nodes.Keys) Assert(m.Deliver(m.Lookup(p, Phase.Abort)), "Abort receipt failed");
    Assert(!m.Deliver(late), "Terminal action prepared again");
    Assert(!m.Decide(1, "w1", Decision.Commit), "Abort reversed");
});
Case("generation_overflow_does_not_wrap", () =>
{
    var m = new Model { Generation = long.MaxValue };
    Assert(!m.Takeover("w2") && m.Generation == long.MaxValue, "Generation wrapped");
});
Case("missing_participant_holds_recovery_pending", () =>
{
    var m = Model.Prepared(); m.Decide(1, "w1", Decision.Commit); m.Takeover("w2");
    foreach (var p in new[] { "building", "inventory" }) m.Deliver(m.Lookup(p, Phase.Barrier));
    m.Recover(); Assert(!m.Certificate && m.Nodes.Values.All(n => !n.Released), "Missing barrier bypassed");
});
Case("new_takeover_invalidates_inflight_replacement", () =>
{
    var m = Model.Prepared(); m.Takeover("w2"); m.BarrierAll(); m.Takeover("w3");
    Assert(!m.Decide(2, "w2", Decision.Commit), "Intermediate worker committed");
    m.BarrierAll(); m.Recover(); m.Check();
});
Case("completion_keeps_original_decision_across_takeover", () =>
{
    var m = Model.Prepared(); m.Decide(1, "w1", Decision.Commit); m.Recover(); var origin = m.DecisionOrigin;
    var late = m.Lookup("inventory", Phase.Complete); m.Takeover("w2"); m.BarrierAll();
    Assert(!m.Deliver(late), "Delayed lower-generation completion crossed barrier");
    m.Recover(); Assert(m.DecisionOrigin == origin && m.Nodes.Values.All(n => n.Effects == 1), "Historical decision changed");
});
Case("restore_requires_fence_and_all_participant_reconciliation", () =>
{
    var m = Model.Prepared(); m.Decide(1, "w1", Decision.Commit); var origin = m.DecisionOrigin;
    var old = m.Lookup("inventory", Phase.Apply); var enrollment = new Enrollment(m);
    Assert(!enrollment.Open() && !enrollment.Fence(false), "Unproven enrollment opened");
    Assert(enrollment.Fence(true), "External proof input refused");
    foreach (var p in m.Nodes.Keys) enrollment.Stage(p);
    Assert(m.Nodes.Values.All(n => n.Incarnation == "initial"), "Staging silently changed active receiver tuple");
    Assert(!enrollment.Reconcile("world", true), "Reconciliation bypassed receiver barrier");
    m.Deliver(m.Lookup("inventory", Phase.Barrier));
    Assert(m.Nodes["inventory"].Incarnation == "restored" && m.Nodes["world"].Incarnation == "initial", "Receiver barrier transition was not local");
    Assert(!m.Deliver(m.Lookup("inventory", Phase.Apply)), "Partial restore barriers issued phase authority");
    enrollment.Reconcile("inventory", true);
    m.Deliver(m.Lookup("building", Phase.Barrier)); enrollment.Reconcile("building", true);
    Assert(!enrollment.Open(), "Partial reconciliation opened");
    m.Deliver(m.Lookup("world", Phase.Barrier));
    Assert(!enrollment.Reconcile("world", false), "Contradiction reconciled");
    enrollment.Reconcile("world", true);
    Assert(!m.Decide(1, "restored-worker", Decision.Commit), "Reconciliation permitted ordinary decision before OPEN");
    Assert(enrollment.Open(), "Complete enrollment failed");
    Assert(!m.Deliver(old), "Old incarnation packet admitted");
    m.Recover(); m.Check(); Assert(m.DecisionOrigin == origin, "Restore rewrote origin");
});
Case("restored_undecided_action_records_new_decision_origin", () =>
{
    var m = Model.Prepared(); var e = new Enrollment(m); e.Fence(true);
    foreach (var p in m.Nodes.Keys) e.Stage(p);
    m.BarrierAll();
    foreach (var p in m.Nodes.Keys) e.Reconcile(p, true);
    Assert(e.Open(), "Enrollment failed"); m.Recover(); m.Check();
    Assert(m.DecisionOrigin!.StartsWith("restored/", StringComparison.Ordinal), "New decision used obsolete incarnation");
});
Case("enrollment_replay_cannot_reset_new_delivery_generation", () =>
{
    var m = Model.Prepared(); var e = new Enrollment(m); e.Fence(true);
    foreach (var p in m.Nodes.Keys) e.Stage(p);
    m.BarrierAll();
    foreach (var p in m.Nodes.Keys) { Assert(e.Reconcile(p, true), "Reconciliation failed"); Assert(e.Reconcile(p, true), "Exact reconciliation replay failed"); }
    e.Open(); m.Takeover("later-worker");
    Assert(e.Open() && m.Generation == 2 && m.Worker == "later-worker", "Enrollment replay reset delivery authority");
});
var schedules = 0;
foreach (var order in Permutations([0, 1, 2, 3, 4]))
{
    var m = Model.Prepared(); m.Decide(1, "w1", Decision.Commit); var old = m.Lookup("inventory", Phase.Apply); m.Takeover("w2");
    foreach (var step in order)
    {
        if (step == 0)
        {
            var barrierHadWon = m.Nodes["inventory"].Generation == 2;
            var accepted = m.Deliver(old); Assert(!barrierHadWon || !accepted, "Schedule admitted stale phase after barrier");
        }
        else if (step == 4) m.Recover();
        else m.Deliver(m.Lookup(new[] { "building", "inventory", "world" }[step - 1], Phase.Barrier));
        m.Check();
    }
    m.Recover(); m.Check(); Assert(m.Nodes.Values.All(n => n.Released && n.Effects == 1), "Schedule failed final convergence"); schedules++;
}
results.Add(new { name = "all_takeover_interleavings", passed = true, schedules });
Console.WriteLine(JsonSerializer.Serialize(new { scope = "in-memory specification only; idealized verified lookup registry, external fencing proof and atomic durable storage assumed",
    cases = results.Count, schedules, results, passed = true }));

static IEnumerable<int[]> Permutations(int[] items)
{
    if (items.Length == 0) { yield return []; yield break; }
    for (var i = 0; i < items.Length; i++)
        foreach (var rest in Permutations(items.Where((_, index) => index != i).ToArray())) yield return [items[i], ..rest];
}

enum Decision { None, Commit, Abort }
enum Phase { Prepare, Barrier, Apply, Complete, Abort }
record Lookup(string Realm, string Action, string Intent, string Incarnation, long Generation, string Worker,
    string Recipient, Phase Phase, string Payload, bool Authenticated, string? DecisionOrigin, bool Certificate, int EvidenceId);
sealed class Node
{
    public long Generation; public string Worker = ""; public string Incarnation = "initial";
    public string? StagedIncarnation;
    public bool Exists, Prepared, Applied, Aborted, Released; public int Effects;
}
sealed class Model
{
    public Dictionary<string, Node> Nodes { get; } = new() { ["building"] = new(), ["inventory"] = new(), ["world"] = new() };
    public long Generation = 1; public string Worker = "w1", Incarnation = "initial"; public bool AdmissionOpen = true, RecoveryOnly;
    public Decision Decision; public string? DecisionOrigin; public bool Certificate;
    readonly Dictionary<string, long> barriers = []; bool recoveryGate;
    readonly HashSet<Lookup> verifiedLookups = []; int nextEvidence;
    public static Model Prepared() { var m = new Model(); foreach (var p in m.Nodes.Keys) AssertResult(m.Deliver(m.Lookup(p, Phase.Prepare))); return m; }
    public Lookup Lookup(string participant, Phase phase)
    {
        // This registry idealizes a trusted direct lookup, not real cryptography, transport or identity validation.
        var phaseMatchesState = phase switch
        {
            Phase.Barrier => true,
            Phase.Prepare => Decision == Decision.None,
            Phase.Apply => Decision == Decision.Commit,
            Phase.Abort => Decision == Decision.Abort,
            Phase.Complete => Decision == Decision.Commit && Certificate,
            _ => false
        };
        var canIssue = phaseMatchesState && (phase == Phase.Barrier && (AdmissionOpen || RecoveryOnly) || AdmissionOpen && Reconciled);
        var lookup = new Lookup("realm", "action", "intent", Incarnation, Generation, Worker,
            participant, phase, Payload(participant, phase), canIssue, DecisionOrigin, Certificate, ++nextEvidence);
        if (canIssue) verifiedLookups.Add(lookup);
        return lookup;
    }
    static string Payload(string p, Phase phase) => $"{p}/{phase}/intent";
    public bool Takeover(string worker)
    {
        if (!AdmissionOpen || Generation == long.MaxValue) return false;
        Generation++; Worker = worker; recoveryGate = true; barriers.Clear(); return true;
    }
    public void BarrierAll() { foreach (var p in Nodes.Keys) AssertResult(Deliver(Lookup(p, Phase.Barrier))); }
    public bool Deliver(Lookup l)
    {
        if (!(AdmissionOpen || RecoveryOnly && l.Phase == Phase.Barrier) || !l.Authenticated || !verifiedLookups.Contains(l) || l.Realm != "realm" || l.Action != "action" || l.Intent != "intent" ||
            !Nodes.TryGetValue(l.Recipient, out var n) || l.Incarnation != Incarnation ||
            l.Generation <= 0 || l.Payload != Payload(l.Recipient, l.Phase)) return false;
        if (l.Phase == Phase.Barrier)
        {
            if (l.Incarnation != n.Incarnation)
            {
                if (n.StagedIncarnation != l.Incarnation || l.Generation != 1) return false;
                n.Incarnation = l.Incarnation; n.StagedIncarnation = null;
            }
            else if (l.Generation < n.Generation || l.Generation == n.Generation && l.Worker != n.Worker) return false;
            n.Exists = true; n.Generation = l.Generation; n.Worker = l.Worker;
            // Only an acknowledgement for the coordinator's own current generation closes its gate.
            if (l.Generation == Generation && l.Worker == Worker) barriers[l.Recipient] = l.Generation;
            return true;
        }
        if (l.Incarnation != n.Incarnation) return false;
        if (!n.Exists)
        {
            if (l.Generation != 1) return false;
            n.Exists = true; n.Generation = 1; n.Worker = l.Worker;
        }
        if (l.Generation != n.Generation || l.Worker != n.Worker) return false;
        switch (l.Phase)
        {
            case Phase.Prepare:
                if (n.Aborted || n.Applied || n.Released) return false;
                n.Prepared = true; return true;
            case Phase.Abort:
                if (Decision != Decision.Abort || l.DecisionOrigin != DecisionOrigin || n.Applied) return false;
                n.Aborted = true; n.Prepared = false; return true;
            case Phase.Apply:
                if (Decision != Decision.Commit || l.DecisionOrigin != DecisionOrigin || !n.Prepared || n.Aborted) return false;
                if (!n.Applied) { n.Applied = true; n.Effects++; } return true;
            case Phase.Complete:
                if (!l.Certificate || !Certificate || l.DecisionOrigin != DecisionOrigin || !n.Applied) return false;
                n.Released = true; return true;
            default: return false;
        }
    }
    bool Current(long generation, string worker) => AdmissionOpen && generation == Generation && worker == Worker;
    bool Reconciled => !recoveryGate || Nodes.Keys.All(p => barriers.GetValueOrDefault(p) == Generation);
    public bool HasCurrentBarrier(string participant) => barriers.GetValueOrDefault(participant) == Generation && Nodes[participant].Incarnation == Incarnation;
    public bool Decide(long generation, string worker, Decision decision)
    {
        if (!Current(generation, worker) || !Reconciled || decision == Decision.None) return false;
        if (Decision != Decision.None) return Decision == decision;
        if (decision == Decision.Commit && Nodes.Values.Any(n => !n.Prepared)) return false;
        Decision = decision; DecisionOrigin = $"{Incarnation}/{generation}/{decision}"; return true;
    }
    public bool Complete(long generation, string worker)
    {
        if (!Current(generation, worker) || !Reconciled || Decision != Decision.Commit || Nodes.Values.Any(n => !n.Applied)) return false;
        Certificate = true; return true;
    }
    public void Recover()
    {
        if (!AdmissionOpen || !Reconciled) return;
        if (Decision == Decision.None && !Decide(Generation, Worker, Decision.Commit)) return;
        if (Decision == Decision.Abort) { foreach (var p in Nodes.Keys) Deliver(Lookup(p, Phase.Abort)); return; }
        foreach (var p in Nodes.Keys) Deliver(Lookup(p, Phase.Apply));
        if (Complete(Generation, Worker)) foreach (var p in Nodes.Keys) Deliver(Lookup(p, Phase.Complete));
    }
    public void StageRestoredControl()
    {
        Incarnation = "restored"; Generation = 1; Worker = "restored-worker"; recoveryGate = true; barriers.Clear();
        // Pending enrollment was staged independently; only each receiver's first mapped barrier changes its active tuple.
        RecoveryOnly = true; AdmissionOpen = false;
    }
    public void OpenRestored() { if (!Reconciled) throw new InvalidOperationException("Missing restore barriers"); RecoveryOnly = false; AdmissionOpen = true; }
    public void Check()
    {
        foreach (var n in Nodes.Values)
            if (n.Effects > 1 || n.Applied && Decision != Decision.Commit || n.Released && (!Certificate || !n.Applied) || n.Aborted && n.Applied)
                throw new InvalidOperationException("Action invariant violated");
        if (Certificate && (Decision != Decision.Commit || Nodes.Values.Any(n => !n.Applied))) throw new InvalidOperationException("Invalid completion");
    }
    static void AssertResult(bool result) { if (!result) throw new InvalidOperationException("Model setup failed"); }
}
sealed class Enrollment
{
    readonly Model model;
    public Enrollment(Model model) { this.model = model; model.AdmissionOpen = false; }
    bool fenced, opened, controlStaged; readonly HashSet<string> staged = []; readonly HashSet<string> reconciled = [];
    public bool Fence(bool independentlyVerifiedProof) { if (independentlyVerifiedProof) fenced = true; return independentlyVerifiedProof; }
    public void Stage(string p)
    {
        if (!fenced || !model.Nodes.ContainsKey(p)) throw new InvalidOperationException("Invalid staging");
        if (opened) return;
        staged.Add(p); model.Nodes[p].StagedIncarnation = "restored";
        if (staged.Count == model.Nodes.Count && !controlStaged) { model.StageRestoredControl(); controlStaged = true; }
    }
    public bool Reconcile(string p, bool consistent)
    {
        if (!fenced || staged.Count != model.Nodes.Count || !staged.Contains(p) || !model.HasCurrentBarrier(p) || !consistent) return false;
        reconciled.Add(p); return true;
    }
    public bool Open()
    {
        if (opened) return true;
        if (!fenced || reconciled.Count != model.Nodes.Count) return false;
        model.OpenRestored(); opened = true; return true;
    }
}
