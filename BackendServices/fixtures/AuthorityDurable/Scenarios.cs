using System.Diagnostics;
using System.Reflection;
using System.Text.Json;
using Npgsql;

namespace AuthorityDurable;

internal sealed class Scenarios(Config config)
{
    private readonly List<object> results = [];
    private readonly List<object> witnesses = [];
    private int failed;
    private int serial;
    private readonly List<Process> children = [];
    internal async Task Cleanup()
    {
        foreach (var child in children)
        {
            if (!child.HasExited) { child.Kill(true); await child.WaitForExitAsync(); }
            child.Dispose();
        }
        children.Clear();
    }
    private static readonly string[] Owners = Store.Participants;
    private string Connection(string owner) => owner switch { "geometry" => config.Coordinator, "material" => config.Material, "terrain" => config.Terrain, _ => throw new RuleException("OWNER") };
    private Store Db(string owner) => new(Connection(owner), config.FixtureId, config.SystemIdentifier, config.ControlRoot);
    private static void Check([System.Diagnostics.CodeAnalysis.DoesNotReturnIf(false)] bool value, string why) => Store.Require(value, why);
    private sealed record Child(Process Process, string ReplyPath);
    private async Task<Child> Start(string owner, Request r, string? connectionOverride = null, string? requestOwner = null)
    {
        r.Owner = requestOwner ?? owner;
        var token = "request" + Interlocked.Increment(ref serial);
        var input = Path.Combine(config.ControlRoot, token + ".json");
        var output = Path.Combine(config.ControlRoot, token + ".reply.json");
        await File.WriteAllTextAsync(input, JsonSerializer.Serialize(r, Program.Json));
        var info = new ProcessStartInfo(config.Dotnet) { UseShellExecute = false, CreateNoWindow = true, RedirectStandardError = true, RedirectStandardOutput = true };
        info.ArgumentList.Add(Assembly.GetExecutingAssembly().Location);
        info.ArgumentList.Add("worker"); info.ArgumentList.Add(input); info.ArgumentList.Add(output);
        foreach (var key in info.Environment.Keys.Where(k => k.StartsWith("AD_", StringComparison.OrdinalIgnoreCase) || k.StartsWith("PG", StringComparison.OrdinalIgnoreCase)).ToArray()) info.Environment.Remove(key);
        info.Environment["AD_CONNECTION"] = connectionOverride ?? Connection(owner);
        info.Environment["AD_FIXTURE_ID"] = config.FixtureId;
        info.Environment["AD_SYSTEM_ID"] = config.SystemIdentifier;
        info.Environment["AD_CONTROL_ROOT"] = config.ControlRoot;
        var process = Process.Start(info) ?? throw new RuleException("CHILD_START");
        children.Add(process); return new Child(process, output);
    }
    private static async Task<Reply> Finish(Child child)
    {
        using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(30));
        try { await child.Process.WaitForExitAsync(timeout.Token); }
        catch { if (!child.Process.HasExited) child.Process.Kill(true); throw; }
        Check(child.Process.ExitCode == 0 && File.Exists(child.ReplyPath), "WORKER_FAILED");
        return JsonSerializer.Deserialize<Reply>(await File.ReadAllTextAsync(child.ReplyPath), Program.Json)!;
    }
    private async Task<Reply> Call(string owner, Request request, string expected = "OK")
    {
        var reply = await Finish(await Start(owner, request));
        witnesses.Add(new { operation = request.Operation, owner, status = reply.Status });
        Check(reply.Status == expected, request.Operation + "_EXPECTED_" + expected + "_GOT_" + reply.Status);
        return reply;
    }
    private async Task Case(string name, Func<Task> body)
    {
        witnesses.Clear();
        var passed = true;
        string? reason = null;
        try
        {
            foreach (var owner in Owners) await Call(owner, new Request { Operation = "RESET" });
            await body();
        }
        catch (Exception ex)
        {
            failed++; passed = false;
            reason = ex is RuleException ? ex.Message : ex.GetType().Name;
        }
        finally
        {
            await Cleanup();
        }
        var snapshots = await Snapshots();
        results.Add(new { name, passed, reason, witnesses = witnesses.ToArray(), snapshots });
    }
    private async Task<Dictionary<string, object>> Snapshots()
    {
        var snapshot = new Dictionary<string, object>();
        foreach (var owner in Owners)
        {
            var rows = new Dictionary<string, object>();
            try
            {
                await using var c = await Db(owner).Open();
                // Fixed table whitelist; finite synthetic rows only, no credentials or player data.
                foreach (var table in new[] { "scope", "actions", "participants", "resources", "holds", "effects", "dedup", "outbox", "acks", "mappings", "audit" })
                {
                    var json = await Store.Scalar(c, "SELECT COALESCE(jsonb_agg(to_jsonb(q)), '[]'::jsonb)::text FROM (SELECT * FROM fixture." + table + " ORDER BY 1 LIMIT 64) q");
                    rows[table] = JsonSerializer.Deserialize<JsonElement>(json);
                }
                rows["limits"] = "At most 64 sorted rows per fixed fixture table; case reset follows snapshot.";
            }
            catch (Exception e) { rows["snapshotFailure"] = e is RuleException ? e.Message : e.GetType().Name; }
            snapshot[owner] = rows;
        }
        return snapshot;
    }
    private async Task Create(string id = "a", long generation = 1) => await Call("geometry", new Request { Operation = "CREATE", Id = id, Generation = generation });
    private async Task<Lookup> Lookup(string owner, string phase, string id = "a") => await Db("geometry").Lookup(id, owner, phase);
    private async Task<Reply> Phase(string owner, string phase, string id = "a", string expected = "OK", Lookup? lookup = null, string key = "") =>
        await Call(owner, new Request { Operation = phase, Id = id, Lookup = lookup ?? await Lookup(owner, phase, id), Key = key }, expected);
    private async Task<List<Evidence>> Evidence(string id = "a")
    {
        var es = new List<Evidence>();
        foreach (var o in Owners)
        {
            var r = await Finish(await Start(o, new Request { Operation = "READ", Id = id }));
            es.Add(r.Evidence ?? new Evidence(o, r.Status == "ABSENT" ? "EMPTY" : "UNKNOWN", "", "", "", "", id,
                "synthetic-realm", "origin-1", Store.Hash(id), "stage-s-v1", "geometry,material,terrain"));
        }
        return es;
    }
    private async Task Decide(string value, string expected = "OK", string id = "a", long generation = 1, string worker = "worker-1", string inc = "origin-1") =>
        await Call("geometry", new Request { Operation = "DECIDE", Id = id, Value = value, Evidence = await Evidence(id), Generation = generation, Worker = worker, Incarnation = inc }, expected);
    private async Task PrepareAll(string id = "a") { foreach (var o in Owners) await Phase(o, "PREPARE", id); }
    private async Task Prepared() { await Create(); await PrepareAll(); }
    private async Task Committed() { await Prepared(); await Decide("COMMIT"); }
    private async Task Applied() { await Committed(); foreach (var o in Owners) await Phase(o, "COMMIT"); }
    private async Task Certify(string expected = "OK", long generation = 1, string worker = "worker-1") =>
        await Call("geometry", new Request { Operation = "CERTIFY", Id = "a", Evidence = await Evidence(), Generation = generation, Worker = worker }, expected);
    private async Task Takeover(long generation = 1, string worker = "worker-1", string expected = "OK") =>
        await Call("geometry", new Request { Operation = "TAKEOVER", Id = "a", Generation = generation, Worker = worker, Value = worker == "worker-1" ? "worker-2" : "worker-1" }, expected);
    private async Task Barriers()
    {
        foreach (var owner in Owners)
        {
            var b = await Phase(owner, "BARRIER"); var l = b.Lookup!;
            await Call("geometry", new Request { Operation = "ACK", Id = "a", Incarnation = l.Incarnation, Generation = l.Generation, Worker = l.Worker, Lookup = l, Evidence = [b.Evidence!] });
        }
    }
    private async Task<string> Scalar(string owner, string sql, params (string, object)[] ps)
    {
        await using var c = await Db(owner).Open(); return await Store.Scalar(c, sql, ps);
    }
    private async Task WaitHook(string hook)
    {
        var end = DateTime.UtcNow.AddSeconds(12);
        while (!File.Exists(Path.Combine(config.ControlRoot, hook + ".reached"))) { Check(DateTime.UtcNow < end, "HOOK_NOT_REACHED"); await Task.Delay(25); }
    }
    private async Task Release(string hook) => await File.WriteAllTextAsync(Path.Combine(config.ControlRoot, hook + ".release"), "release");
    private async Task Kill(Child child) { child.Process.Kill(true); await child.Process.WaitForExitAsync(); }
    private async Task NoOutput(string owner) => Check(await Scalar(owner, "SELECT count(*) FROM fixture.effects WHERE output_available") == "0", "INCOMPLETE_OUTPUT_AVAILABLE");
    internal async Task<int> Run(string output)
    {
        Directory.CreateDirectory(config.ControlRoot);
        Check(Guid.TryParse(config.FixtureId, out _) && config.SystemIdentifier.All(char.IsAsciiDigit) && config.SystemIdentifier.Length > 0, "CONFIG_IDENTITY");
        var schema = await File.ReadAllTextAsync(Path.Combine(AppContext.BaseDirectory, "Schema.sql"));
        Check(Store.Hash(schema) == config.DdlSha256.ToLowerInvariant(), "DDL_HASH");
        foreach (var o in Owners) await Call(o, new Request { Operation = "SEED" });
        await Case("F01_absent_barrier_late_prepare_and_abort_tombstone", async () =>
        {
            await Create(); var stale = await Lookup("material", "PREPARE"); await Takeover();
            await Phase("material", "BARRIER"); await Phase("material", "PREPARE", lookup: stale, expected: "FENCED");
            await Barriers(); await Decide("ABORT", generation: 2, worker: "worker-2");
            await Phase("terrain", "ABORT");
            var terminal = await Lookup("terrain", "PREPARE"); terminal.Decision = ""; terminal.DecisionDigest = ""; terminal.Payload = Store.Payload(terminal);
            await Phase("terrain", "PREPARE", lookup: terminal, expected: "TERMINAL_PREPARE");
            Check((await Evidence()).Single(e => e.Participant == "terrain").State == "ABORTED", "TOMBSTONE_LOST");
        });
        await Case("F02_old_apply_before_and_after_barrier", async () =>
        {
            await Committed(); var before = await Lookup("material", "COMMIT"); var after = await Lookup("terrain", "COMMIT");
            await Takeover(); await Phase("material", "COMMIT", lookup: before);
            var ack = await Phase("material", "BARRIER"); Check(ack.Evidence!.Applied.Length == 64, "RECEIPT_NOT_RECONCILED");
            await Phase("terrain", "BARRIER"); await Phase("terrain", "COMMIT", lookup: after, expected: "FENCED");
            await Barriers(); foreach (var o in Owners) await Phase(o, "COMMIT", expected: o == "material" ? "OK" : "OK");
            await Certify(generation: 2, worker: "worker-2"); foreach (var o in Owners) await Phase(o, "COMPLETE");
            Check(await Scalar("material", "SELECT count(*) FROM fixture.effects") == "1", "DUPLICATE_EFFECT");
        });
        await Case("F03_same_generation_replay_worker_and_identity_conflicts", async () =>
        {
            await Prepared(); await Phase("material", "BARRIER"); await Phase("material", "BARRIER");
            var wrong = await Lookup("material", "BARRIER"); wrong.Worker = "worker-2";
            await Phase("material", "BARRIER", lookup: wrong, expected: "WORKER_CONFLICT");
            var bad = await Lookup("material", "BARRIER"); bad.Hash = new string('0', 64);
            await Phase("material", "BARRIER", lookup: bad, expected: "IDENTITY_CONFLICT");
            var issuer = await Lookup("material", "BARRIER"); issuer.Issuer = "untrusted";
            await Phase("material", "BARRIER", lookup: issuer, expected: "LOOKUP_BINDING");
        });
        await Case("F03_bound_ack_and_framed_dedup_keys", async () =>
        {
            Check(Store.Fingerprint("ab", "c") != Store.Fingerprint("a", "bc"), "UNFRAMED_TUPLE_ALIAS");
            Check(Store.Fingerprint("stage-s-v1", "a", "bc") == "06f9ff12bfb84dedf0dbdb2f7dc89d2a29b7383bd8122bb28d1c60a23f44a028", "FIXTURE_BYTE_VECTOR_1");
            Check(Store.Fingerprint("stage-s-v1", "ab", "c") == "53aca9f109b35e28e967c6c2c9d946499339c1a130b7d4388c9a0b1414094d70", "FIXTURE_BYTE_VECTOR_2");
            await Create(); await Phase("material", "PREPARE", key: "fixed-replay-key");
            var old = await Phase("material", "BARRIER"); await Takeover();
            var current = await Lookup("material", "BARRIER");
            await Call("geometry", new Request { Operation = "ACK", Id = "a", Generation = 2, Worker = "worker-2", Lookup = current, Evidence = [old.Evidence!] }, "ACK_RECEIPT_BINDING");
            await Barriers(); await Phase("material", "PREPARE", key: "fixed-replay-key", expected: "IDEMPOTENCY_MISMATCH");
        });
        await Case("F04_fresh_post_decision_prepare_and_delayed_abort", async () =>
        {
            await Prepared(); var beforeDecision = await Lookup("material", "PREPARE"); await Decide("COMMIT");
            await Phase("material", "PREPARE", expected: "AUTHORITATIVE_PHASE");
            await Decide("ABORT", "IMMUTABLE_DECISION");
            await Phase("material", "COMMIT"); await Phase("material", "PREPARE", expected: "AUTHORITATIVE_PHASE");
            await Phase("material", "PREPARE", lookup: beforeDecision, expected: "TERMINAL_PREPARE");
            await Create("b"); await Decide("ABORT", id: "b");
            await Phase("terrain", "PREPARE", "b", "AUTHORITATIVE_PHASE");
            await Phase("terrain", "ABORT", "b");
        });
        await Case("F05_takeover_coordinator_CAS_completion_and_outbox", async () =>
        {
            await Prepared(); var prepared = await Evidence(); await Takeover();
            // Reuse immutable prepared evidence; do not spend the five-second undecided budget rereading it for negative CAS cases.
            await Call("geometry", new Request { Operation = "DECIDE", Id = "a", Value = "COMMIT", Evidence = prepared }, "COORDINATOR_FENCED");
            await Call("geometry", new Request { Operation = "DECIDE", Id = "a", Value = "COMMIT", Evidence = prepared, Generation = 2, Worker = "worker-2" }, "BARRIERS_REQUIRED");
            await Barriers(); await Call("geometry", new Request { Operation = "DECIDE", Id = "a", Value = "COMMIT", Evidence = prepared, Generation = 2, Worker = "worker-2" });
            var immutable = (await Lookup("geometry", "COMMIT")).DecisionDigest;
            await Phase("material", "COMMIT"); await Certify("ALL_APPLIED_REQUIRED", 2, "worker-2"); await NoOutput("material");
            foreach (var o in new[] { "geometry", "terrain" }) await Phase(o, "COMMIT");
            await Certify(generation: 2, worker: "worker-2");
            await Takeover(2, "worker-2"); await Barriers();
            Check((await Lookup("geometry", "COMPLETE")).DecisionDigest == immutable, "ORIGINAL_DECISION_CHANGED");
            foreach (var o in Owners) { await Phase(o, "COMPLETE"); await Phase(o, "COMPLETE", expected: "REPLAY"); }
            Check(await Scalar("geometry", "SELECT count(*) FROM fixture.outbox WHERE phase='COMPLETE'") != "0", "OUTBOX_MISSING");
        });
        await Case("F05_fresh_phases_blocked_until_all_barriers_reconcile", async () =>
        {
            await Applied(); await Certify(); await Takeover();
            await Phase("material", "BARRIER");
            await Phase("material", "COMMIT", expected: "BARRIERS_REQUIRED");
            await Phase("material", "COMPLETE", expected: "BARRIERS_REQUIRED");
            await Barriers(); await Phase("material", "COMMIT");
            await Phase("material", "COMPLETE");
            await Phase("material", "COMMIT", expected: "REPLAY");
        });
        await Case("F06_overflow_missing_lookup_and_ambiguous_commit", async () =>
        {
            await Create(generation: long.MaxValue); await Takeover(long.MaxValue, expected: "OVERFLOW");
            var unknown = false; try { await Lookup("material", "PREPARE", "missing"); } catch (RuleException e) { unknown = e.Message == "UNKNOWN"; }
            Check(unknown, "UNKNOWN_CREATED_AUTHORITY");
            await Create("b"); var l = await Lookup("material", "PREPARE", "b");
            var child = await Start("material", new Request { Operation = "PREPARE", Id = "b", Lookup = l, Hook = "commit", HookId = "lostreply" });
            await WaitHook("lostreply"); await Kill(child);
            Check((await Evidence("b")).Single(e => e.Participant == "material").State == "PREPARED", "COMMITTED_REPLY_LOSS_LOST_STATE");
            await Phase("material", "PREPARE", "b", "REPLAY", l);
        });
        await Case("storage_crash_rollback_atomic_effect_receipt_outbox", async () =>
        {
            await Committed(); var l = await Lookup("material", "COMMIT");
            var child = await Start("material", new Request { Operation = "COMMIT", Id = "a", Lookup = l, Hook = "effect", HookId = "uncommitted" });
            await WaitHook("uncommitted"); await Kill(child);
            Check(await Scalar("material", "SELECT count(*) FROM fixture.effects") == "0", "UNCOMMITTED_EFFECT_ESCAPED");
            Check((await Evidence()).Single(e => e.Participant == "material").State == "PREPARED", "CRASH_CHANGED_RECEIPT");
            await Call("material", new Request { Operation = "COMMIT", Id = "a", Lookup = l, Rollback = true }, "INJECTED_ROLLBACK");
            Check(await Scalar("material", "SELECT count(*) FROM fixture.effects") == "0", "ROLLBACK_EFFECT_ESCAPED");
            await Phase("material", "COMMIT", lookup: l); await Phase("material", "COMMIT", lookup: l, expected: "REPLAY");
            Check(await Scalar("material", "SELECT sum(debit) FROM fixture.effects") == "2", "DEBIT_NOT_EXACTLY_ONCE");
            Check(await Scalar("material", "SELECT sum(quantity) FROM fixture.resources") == "1998", "CONSERVATION_FAILURE");
        });
        await Case("storage_unique_insert_and_shared_gate_resource_contention", async () =>
        {
            await Create(); var l = await Lookup("material", "PREPARE");
            var one = await Start("material", new Request { Operation = "PREPARE", Id = "a", Lookup = l });
            var two = await Start("material", new Request { Operation = "PREPARE", Id = "a", Lookup = l });
            var replies = new[] { await Finish(one), await Finish(two) };
            Check(replies.Count(x => x.Status == "OK") == 1 && replies.Count(x => x.Status == "REPLAY") == 1, "UNIQUE_INSERT_RACE");
            await Create("b"); await Create("c");
            var first = await Start("material", new Request { Operation = "PREPARE", Id = "b", Lookup = await Lookup("material", "PREPARE", "b"), Hook = "resources", HookId = "resourcesheld" });
            await WaitHook("resourcesheld");
            var second = await Start("material", new Request { Operation = "PREPARE", Id = "c", Lookup = await Lookup("material", "PREPARE", "c"), Hook = "row", HookId = "secondrow" });
            await WaitHook("secondrow"); // Two actions simultaneously hold compatible scope locks and distinct action locks.
            await Release("secondrow");
            await Task.Delay(100); Check(!second.Process.HasExited, "RESOURCE_LOCK_DID_NOT_BLOCK");
            var waiters = await Scalar("material", "SELECT count(*) FROM pg_catalog.pg_stat_activity WHERE usename=current_user AND wait_event_type='Lock'");
            Check(waiters != "0", "NO_LOCK_WAIT_WITNESS"); witnesses.Add(new { observation = "resource_lock_waiters_after_two_shared_gate_holders", count = waiters });
            await Release("resourcesheld"); Check((await Finish(first)).Status == "OK" && (await Finish(second)).Status == "OK", "RESOURCE_CONTENTION_FAILED");
            Check(await Scalar("material", "SELECT count(*) FROM fixture.holds") == "6", "HOLDS_NOT_ALL_OR_NONE");
        });
        await Case("F09_scope_freeze_conflicts_with_shared_phase_and_rechecks_gate", async () =>
        {
            await Create(); var stale = await Lookup("material", "PREPARE");
            var holder = await Start("material", new Request { Operation = "BARRIER", Id = "a", Lookup = await Lookup("material", "BARRIER"), Hook = "gate", HookId = "sharedgate" });
            await WaitHook("sharedgate");
            var freeze = await Start("material", new Request { Operation = "FREEZE", Id = "a" });
            Check((await Finish(freeze)).Status == "SQL:55P03", "FREEZE_DID_NOT_CONFLICT");
            await Release("sharedgate"); Check((await Finish(holder)).Status == "OK", "SHARED_GATE_FAILED");
            await Call("material", new Request { Operation = "FREEZE", Id = "a" });
            await Phase("material", "PREPARE", lookup: stale, expected: "GATE_CLOSED");
        });
        await Case("F07_F08_F10_partial_restore_mapping_and_new_incarnation_barrier", async () =>
        {
            await Create(generation: 100); foreach (var o in Owners) await Phase(o, "BARRIER");
            foreach (var o in Owners) await Call(o, new Request { Operation = "FREEZE", Id = "a" });
            await Call("material", new Request { Operation = "STAGE", Id = "a", Incarnation = "restore-2", Worker = "worker-2", Manifest = "manifest-2", Value = "missing" }, "PROOF_OR_GATE");
            foreach (var o in Owners) await Call(o, new Request { Operation = "STAGE", Id = "a", Generation = 100, Incarnation = "restore-2", Worker = "worker-2", Manifest = "manifest-2", Value = "TRUSTED_TEST_PROOF" });
            Check(await Scalar("material", "SELECT generation FROM fixture.participants WHERE id='a'") == "100", "STAGING_SWITCHED_TUPLE");
            await Call("geometry", new Request { Operation = "COORD_RESTORE", Id = "a", Generation = 100, Manifest = "manifest-2", Value = "TRUSTED_TEST_FRONTIER" });
            await Phase("material", "PREPARE", expected: "GATE_CLOSED");
            var wrong = await Lookup("terrain", "BARRIER"); wrong.Id = "unmapped"; wrong.Hash = Store.Hash("unmapped"); wrong.Payload = Store.Payload(wrong);
            await Phase("terrain", "BARRIER", "unmapped", "MAPPING_REQUIRED", wrong);
            foreach (var o in Owners) { await Phase(o, "BARRIER"); await Phase(o, "BARRIER"); }
            await Barriers();
            Check(await Scalar("material", "SELECT generation FROM fixture.participants WHERE id='a'") == "1", "NEW_INCARNATION_NOT_RESET");
            var old = await Lookup("material", "BARRIER"); old.Incarnation = "origin-1"; old.Generation = 101;
            await Phase("material", "BARRIER", lookup: old, expected: "INCARNATION_FENCED");
            await Call("material", new Request { Operation = "OPEN", Id = "a" }, "RECONCILIATION_REQUIRED");
            foreach (var o in Owners) await Call(o, new Request { Operation = "OPEN", Id = "a", Value = "TRUSTED_TEST_RECONCILED" });
            Check((await Lookup("geometry", "BARRIER")).Origin == "origin-1", "ORIGIN_REWRITTEN");
        });
        await Case("F08_missing_prepare_contradictory_receipt_and_certificate", async () =>
        {
            await Applied(); await Certify(); var cert = await Lookup("material", "COMPLETE"); cert.Completion = new string('0', 64); cert.Payload = Store.Payload(cert);
            await Phase("material", "COMPLETE", lookup: cert, expected: "CERTIFICATE_CONFLICT"); await NoOutput("material");
            await Create("b"); var forged = await Lookup("terrain", "COMMIT", "b"); forged.Decision = "COMMIT"; forged.DecisionDigest = new string('a', 64); forged.Payload = Store.Payload(forged);
            await Phase("terrain", "COMMIT", "b", "PREPARE_REQUIRED", forged);
        });
        await Case("F08_coordinator_rejects_mixed_action_and_profile_receipts", async () =>
        {
            await Prepared(); var prepared = await Evidence();
            var mixed = prepared.Select(e => e.Participant == "material" ? e with { ActionId = "other-action" } : e).ToList();
            await Call("geometry", new Request { Operation = "DECIDE", Id = "a", Value = "COMMIT", Evidence = mixed }, "EVIDENCE_IDENTITY_CONFLICT");
            Check((await Lookup("geometry", "COMMIT")).Decision == "", "MIXED_DECISION_PERSISTED");
            await Call("geometry", new Request { Operation = "DECIDE", Id = "a", Value = "COMMIT", Evidence = prepared });
            foreach (var o in Owners) await Phase(o, "COMMIT");
            var applied = await Evidence();
            mixed = applied.Select(e => e.Participant == "terrain" ? e with { Profile = "other-profile" } : e).ToList();
            await Call("geometry", new Request { Operation = "CERTIFY", Id = "a", Evidence = mixed }, "EVIDENCE_IDENTITY_CONFLICT");
            Check((await Lookup("geometry", "COMPLETE")).Completion == "", "MIXED_CERTIFICATE_PERSISTED");
        });
        await Case("F07_F08_absent_mapped_barrier_blocks_open_and_unmapped_restore", async () =>
        {
            await Create(); await Create("b");
            foreach (var o in Owners) await Call(o, new Request { Operation = "FREEZE", Id = "a" });
            foreach (var o in Owners) await Call(o, new Request { Operation = "STAGE", Id = "a", Incarnation = "restore-2", Worker = "worker-2", Manifest = "manifest-2", Value = "TRUSTED_TEST_PROOF" });
            await Call("geometry", new Request { Operation = "COORD_RESTORE", Id = "b", Manifest = "manifest-2", Value = "TRUSTED_TEST_FRONTIER" }, "MAPPING_REQUIRED");
            await Call("material", new Request { Operation = "OPEN", Id = "a", Value = "TRUSTED_TEST_RECONCILED" }, "MAPPED_BARRIER_REQUIRED");
            await Call("geometry", new Request { Operation = "COORD_RESTORE", Id = "a", Manifest = "manifest-2", Value = "TRUSTED_TEST_FRONTIER" });
            await Barriers();
            foreach (var o in Owners) await Call(o, new Request { Operation = "OPEN", Id = "a", Value = "TRUSTED_TEST_RECONCILED" });
            var unmapped = await Lookup("material", "BARRIER"); unmapped.Id = "b"; unmapped.Hash = Store.Hash("b"); unmapped.Payload = Store.Payload(unmapped);
            await Phase("material", "BARRIER", "b", "MAPPING_REQUIRED", unmapped);
        });
        await Case("storage_DB_clock_after_action_lock_deadline", async () =>
        {
            await Prepared(); var prepared = await Evidence();
            await using (var c = await Db("geometry").Open())
                await Store.Sql(c, "UPDATE fixture.actions SET deadline=clock_timestamp()+interval '1200 milliseconds' WHERE id='a'");
            var decide = await Start("geometry", new Request { Operation = "DECIDE", Id = "a", Value = "COMMIT", Evidence = prepared, Hook = "row", HookId = "deadlineclock" });
            await WaitHook("deadlineclock");
            Check(await Scalar("geometry", "SELECT clock_timestamp()<deadline FROM fixture.actions WHERE id='a'") == "True", "DEADLINE_HOOK_STARTED_TOO_LATE");
            await Task.Delay(1300); await Release("deadlineclock");
            Check((await Finish(decide)).Status == "DEADLINE_EXPIRED", "STALE_TRANSACTION_CLOCK_USED");
            Check((await Lookup("geometry", "COMMIT")).Decision == "", "EXPIRED_COMMIT_PERSISTED");
            await Decide("ABORT");
        });
        await Case("storage_outbox_relay_reentry_lost_ack_certificate_worker_crash", async () =>
        {
            await Committed(); await RelayPending(false); await RelayPending(true);
            Dictionary<string, string>? participantMessage;
            await using (var c = await Db("geometry").Open())
                participantMessage = await Store.Row(c, "SELECT * FROM fixture.outbox WHERE kind='PARTICIPANT' AND action_id='a' AND phase='COMMIT'");
            Check(participantMessage != null, "PARTICIPANT_OUTBOX_MISSING");
            var refused = false;
            try { await RelayRow(participantMessage, false); } catch (RuleException e) { refused = e.Message == "PENDING_CATALOGUE_INVALID"; }
            Check(refused, "PARTICIPANT_MESSAGE_DISPATCHED_AS_COORDINATOR");
            await Call("geometry", new Request { Operation = "DELIVERY_ACK", Id = "a", Key = participantMessage["step_key"], Value = participantMessage["digest"] }, "OUTBOX_ACK_CONFLICT");
            var certificate = await Start("geometry", new Request { Operation = "CERTIFY", Id = "a", Evidence = await Evidence(), Hook = "commit", HookId = "certreply" });
            await WaitHook("certreply"); await Kill(certificate);
            await RelayPending(false); await RelayPending(true);
            foreach (var o in Owners) Check((await Evidence()).Single(e => e.Participant == o).State == "COMPLETED", "RELAY_DID_NOT_COMPLETE");
            Check(await Scalar("geometry", "SELECT count(*) FROM fixture.outbox WHERE kind='COORDINATOR' AND action_id='a' AND phase IN('COMMIT','COMPLETE') AND NOT acknowledged") == "0", "DELIVERY_NOT_ACKED");
        });
        await Case("F06_retained_upstream_socket_partition", RetainedSocket);
        await Case("F06_owner_database_unavailable_and_same_action_recovery", OwnerOutage);
        await Case("storage_runtime_readonly_cross_database_role_denial", Roles);
        await Case("storage_database_local_recipient_and_coordinator_ownership", async () =>
        {
            await Create();
            var wrong = await Finish(await Start("material", new Request { Operation = "BARRIER", Id = "a", Lookup = await Lookup("geometry", "BARRIER") }, requestOwner: "geometry"));
            Check(wrong.Status == "WRONG_RECIPIENT", "MATERIAL_EMITTED_GEOMETRY_EVIDENCE");
            await Call("material", new Request { Operation = "CREATE", Id = "receiver-action" }, "COORDINATOR_DATABASE_REQUIRED");
            Check(await Scalar("material", "SELECT count(*) FROM fixture.actions") == "0" && await Scalar("material", "SELECT count(*) FROM fixture.participants") == "0", "WRONG_OWNER_MUTATED_STATE");
            var denied = false; try { await Db("material").Lookup("a", "material", "BARRIER"); } catch (RuleException e) { denied = e.Message == "COORDINATOR_DATABASE_REQUIRED"; }
            Check(denied, "RECEIVER_ISSUED_COORDINATOR_LOOKUP");
        });
        await Case("storage_actual_immediate_PostgreSQL_restart_WAL_recovery", Restart);
        var report = new
        {
            fixtureId = config.FixtureId, profile = "stage-s-v1", stage = "S", passed = failed == 0,
            caseGroups = results.Count, failed, results,
            source = "Three isolated PostgreSQL databases, owning runtime credentials, child worker processes; deterministic shared/exclusive gate and sorted resource lock witnesses. Outbox relay is driver reentry, not a separate relay-process restart.",
            stubs = new[] { "Driver supplies trusted synthetic Building lookup/evidence; issuer equality is not authentication.", "TRUSTED_TEST_PROOF/FRONTIER/RECONCILED are explicit test oracles, never external fence/reopen proof.", "Synthetic prepared authorization substitutes for B-03 admission grants; no grant/revocation/handoff implementation." },
            unproven = new[] { "Production B-03/B-04/B-05 acceptance and protected interfaces", "mTLS/SPIRE/PKI, real remote authorization and encoding compatibility", "Old-primary/workload exclusion, external proof survival, real backup restore/erasure/downstream eligibility", "Independent-cluster/AZ/region or power-loss durability, production performance and physical geometry activation" },
            skipped = Array.Empty<string>(), applicableFailureExitCode = failed == 0 ? 0 : 1
        };
        await File.WriteAllTextAsync(output, JsonSerializer.Serialize(report, Program.Json));
        Console.WriteLine($"Stage S: {results.Count - failed}/{results.Count} case groups passed; trusted authority/proof stubs; no production closure.");
        return failed == 0 ? 0 : 1;
    }
    private async Task Roles()
    {
        foreach (var owner in Owners)
        {
            await using var c = await Db(owner).Open();
            foreach (var sql in new[] { "CREATE TABLE fixture.denied(x int)", "UPDATE fixture.identity SET fixture_id=fixture_id", "SET ROLE own_ad_" + (owner == "geometry" ? "coordinator" : owner) })
            {
                var denied = false; try { await Store.Sql(c, sql); } catch (PostgresException e) { denied = e.SqlState == "42501"; } Check(denied, "RUNTIME_ROLE_DENIAL");
            }
            var ro = owner switch { "geometry" => config.CoordinatorReadOnly, "material" => config.MaterialReadOnly, _ => config.TerrainReadOnly };
            await using var reader = await new Store(ro, config.FixtureId, config.SystemIdentifier, config.ControlRoot).Open();
            Check(await Store.Scalar(reader, "SELECT count(*) FROM fixture.resources") == "2", "READER_SELECT_FAILED");
            var writeDenied = false; try { await Store.Sql(reader, "UPDATE fixture.resources SET quantity=quantity"); } catch (PostgresException e) { writeDenied = e.SqlState == "42501"; } Check(writeDenied, "READER_WRITE_ALLOWED");
            var cross = new NpgsqlConnectionStringBuilder(Connection(owner)) { Database = new NpgsqlConnectionStringBuilder(Connection(owner == "material" ? "terrain" : "material")).Database, Pooling = false, Timeout = 3 };
            var blocked = false;
            try { await using var wrong = new NpgsqlConnection(cross.ConnectionString); await wrong.OpenAsync(); }
            catch (PostgresException e) { blocked = e.SqlState is "28000" or "42501"; }
            Check(blocked, "CROSS_DATABASE_CONNECTION_ALLOWED");
        }
    }
    private async Task RelayPending(bool acknowledge)
    {
        // Durable test relay: only the fixed coordinator decision/completion catalogue is executable.
        foreach (var phase in new[] { "COMMIT", "COMPLETE" })
        {
            Dictionary<string, string>? pending;
            await using (var c = await Db("geometry").Open())
                pending = await Store.Row(c, "SELECT * FROM fixture.outbox WHERE kind='COORDINATOR' AND action_id='a' AND phase=@phase AND NOT acknowledged ORDER BY step_key LIMIT 1", ("phase", phase));
            if (pending == null) continue;
            await RelayRow(pending, acknowledge);
        }
    }
    private async Task RelayRow(Dictionary<string, string> pending, bool acknowledge)
    {
        var phase = pending["phase"];
        Check(pending["kind"] == "COORDINATOR" && pending["action_id"] == "a" && phase is "COMMIT" or "COMPLETE" &&
            pending["step_key"] == Store.Fingerprint("coordinator-outbox-v1", "a", phase == "COMPLETE" ? "CERTIFY" : "DECIDE"), "PENDING_CATALOGUE_INVALID");
        foreach (var o in Owners)
        {
            var l = await Lookup(o, phase);
            Check(pending["digest"] == (phase == "COMMIT" ? l.DecisionDigest : l.Completion), "PENDING_EVIDENCE_CHANGED");
            var r = await Finish(await Start(o, new Request { Operation = phase, Id = "a", Lookup = l }));
            Check(r.Status is "OK" or "REPLAY", "RELAY_PHASE_REJECTED_" + r.Status);
        }
        if (acknowledge)
        {
            var l = await Lookup("geometry", phase);
            await Call("geometry", new Request { Operation = "DELIVERY_ACK", Id = "a", Key = pending["step_key"], Value = pending["digest"], Incarnation = l.Incarnation, Generation = l.Generation, Worker = l.Worker });
        }
    }
    private static async Task WaitFile(string path, int seconds = 12)
    {
        var deadline = DateTime.UtcNow.AddSeconds(seconds);
        while (!File.Exists(path)) { Check(DateTime.UtcNow < deadline, "CONTROL_FILE_TIMEOUT"); await Task.Delay(25); }
    }
    private async Task RetainedSocket()
    {
        await Create(); await Create("b");
        var proxyRoot = Path.Combine(config.ControlRoot, "retained-socket"); Directory.CreateDirectory(proxyRoot);
        var info = new ProcessStartInfo(config.Dotnet) { UseShellExecute = false, CreateNoWindow = true, RedirectStandardError = true, RedirectStandardOutput = true };
        info.ArgumentList.Add(Assembly.GetExecutingAssembly().Location); info.ArgumentList.Add("proxy");
        info.ArgumentList.Add(new NpgsqlConnectionStringBuilder(config.Material).Port.ToString(System.Globalization.CultureInfo.InvariantCulture)); info.ArgumentList.Add(proxyRoot);
        foreach (var key in info.Environment.Keys.Where(k => k.StartsWith("AD_", StringComparison.OrdinalIgnoreCase) || k.StartsWith("PG", StringComparison.OrdinalIgnoreCase)).ToArray()) info.Environment.Remove(key);
        info.Environment["AD_FIXTURE_ID"] = config.FixtureId;
        var proxy = Process.Start(info) ?? throw new RuleException("PROXY_START"); children.Add(proxy);
        await WaitFile(Path.Combine(proxyRoot, "ready.json"));
        using var ready = JsonDocument.Parse(await File.ReadAllTextAsync(Path.Combine(proxyRoot, "ready.json")));
        var throughProxy = new NpgsqlConnectionStringBuilder(config.Material) { Port = ready.RootElement.GetProperty("port").GetInt32() };
        var worker = await Start("material", new Request { Operation = "PREPARE", Id = "a", Lookup = await Lookup("material", "PREPARE"), Hook = "resources", HookId = "proxypaused" }, throughProxy.ConnectionString);
        await WaitHook("proxypaused");
        await File.WriteAllTextAsync(Path.Combine(proxyRoot, "drop"), "drop"); await WaitFile(Path.Combine(proxyRoot, "clientDropped"));
        await Kill(worker);
        await Phase("material", "PREPARE", "b", "SQL:55P03");
        Check(await Scalar("material", "SELECT count(*) FROM pg_catalog.pg_stat_activity WHERE usename=current_user AND state='idle in transaction'") != "0", "UPSTREAM_TRANSACTION_NOT_RETAINED");
        await File.WriteAllTextAsync(Path.Combine(proxyRoot, "stop"), "stop"); await WaitFile(Path.Combine(proxyRoot, "upstreamClosed"));
        await proxy.WaitForExitAsync(); Check(proxy.ExitCode == 0, "PROXY_FAULT_NOT_EXECUTED");
        await Phase("material", "PREPARE", "b");
        Check((await Evidence()).Single(e => e.Participant == "material").State == "EMPTY", "PARTITIONED_UNCOMMITTED_PREPARE_ESCAPED");
        Check(await Scalar("material", "SELECT count(*) FROM fixture.holds") == "2", "PARTITION_CREATED_EXTRA_HOLDS");
    }
    private async Task Outage(bool offline)
    {
        var request = Path.Combine(config.ControlRoot, "owner-outage.request");
        var complete = Path.Combine(config.ControlRoot, "owner-outage.complete");
        if (File.Exists(complete)) File.Delete(complete);
        var requestId = "outage" + Guid.NewGuid().ToString("N");
        var temp = request + ".tmp";
        await File.WriteAllTextAsync(temp, JsonSerializer.Serialize(new { fixtureId = config.FixtureId, requestId, owner = "material", offline }, Program.Json));
        File.Move(temp, request, true);
        await WaitFile(complete, 30);
        using var doc = JsonDocument.Parse(await File.ReadAllTextAsync(complete)); var a = doc.RootElement;
        Check(a.GetProperty("fixtureId").GetString() == config.FixtureId && a.GetProperty("requestId").GetString() == requestId && a.GetProperty("owner").GetString() == "material" && a.GetProperty("offline").GetBoolean() == offline, "OUTAGE_ACK_MISMATCH");
    }
    private async Task OwnerOutage()
    {
        await Committed();
        await Outage(true);
        try
        {
            await Phase("material", "COMMIT", expected: "SQL:55000");
            await Phase("geometry", "COMMIT");
            Check((await Lookup("geometry", "COMMIT")).Decision == "COMMIT", "OTHER_OWNER_UNAVAILABLE");
            await Certify("ALL_APPLIED_REQUIRED"); await NoOutput("geometry");
        }
        finally { await Outage(false); }
        await Phase("material", "COMMIT"); await Phase("terrain", "COMMIT"); await Certify();
        foreach (var o in Owners) await Phase(o, "COMPLETE");
    }
    private async Task Restart()
    {
        await Applied();
        var before = await Evidence();
        var requestId = "restart" + Guid.NewGuid().ToString("N");
        var requestPath = Path.Combine(config.ControlRoot, "restart.request");
        var completePath = Path.Combine(config.ControlRoot, "restart.complete");
        Check(!File.Exists(requestPath) && !File.Exists(completePath), "RESTART_PROTOCOL_REUSED");
        await File.WriteAllTextAsync(requestPath + ".tmp", JsonSerializer.Serialize(new { fixtureId = config.FixtureId, requestId }, Program.Json));
        File.Move(requestPath + ".tmp", requestPath);
        var end = DateTime.UtcNow.AddSeconds(60);
        while (!File.Exists(completePath)) { Check(DateTime.UtcNow < end, "RESTART_NOT_ACKNOWLEDGED"); await Task.Delay(100); }
        using var ack = JsonDocument.Parse(await File.ReadAllTextAsync(completePath));
        var root = ack.RootElement;
        Check(root.GetProperty("fixtureId").GetString() == config.FixtureId && root.GetProperty("requestId").GetString() == requestId && root.GetProperty("mode").GetString() == "immediate" && root.GetProperty("systemIdentifier").GetString() == config.SystemIdentifier, "RESTART_ACK_MISMATCH");
        Check(JsonSerializer.Serialize(before) == JsonSerializer.Serialize(await Evidence()), "WAL_RECEIPT_MISMATCH");
        foreach (var o in Owners) { await NoOutput(o); Check(await Scalar(o, "SELECT sum(quantity) FROM fixture.resources") == "1998", "WAL_CONSERVATION_FAILURE"); }
        await Certify(); foreach (var o in Owners) await Phase(o, "COMPLETE");
    }
}
