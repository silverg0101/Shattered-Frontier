using System.Globalization;
using System.Diagnostics.CodeAnalysis;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using Npgsql;

namespace AuthorityDurable;

internal sealed class Store(string connectionString, string fixtureId, string systemId, string controlRoot)
{
    private string LocalOwner => new NpgsqlConnectionStringBuilder(connectionString).Database?.Split('_').Last() switch
    { "coordinator" => "geometry", "material" => "material", "terrain" => "terrain", _ => throw new RuleException("WRONG_DATABASE") };
    internal static string Hash(string value) => Convert.ToHexStringLower(SHA256.HashData(Encoding.UTF8.GetBytes(value)));
    internal static string Fingerprint(params string[] parts) => Hash(JsonSerializer.Serialize(parts));
    internal static void Require([DoesNotReturnIf(false)] bool value, string reason) { if (!value) throw new RuleException(reason); }
    internal static readonly string[] Participants = ["geometry", "material", "terrain"];
    internal async Task<NpgsqlConnection> Open()
    {
        var b = new NpgsqlConnectionStringBuilder(connectionString);
        Require(b.Host == "127.0.0.1" && b.Port > 0 && b.Port <= 65535, "UNOWNED_ENDPOINT");
        var suffix = b.Database?.Split('_').Last() ?? "";
        Require(new[] { "coordinator", "material", "terrain" }.Contains(suffix) &&
            b.Database == "ad_" + Guid.Parse(fixtureId).ToString("N") + "_" + suffix, "WRONG_DATABASE");
        Require(b.Username == "svc_ad_" + suffix || b.Username == "ro_ad_" + suffix, "WRONG_ROLE");
        b.Pooling = false; b.Timeout = 5; b.CommandTimeout = 10; b.IncludeErrorDetail = false;
        var c = new NpgsqlConnection(b.ConnectionString);
        try
        {
            await c.OpenAsync();
            Require(await Scalar(c, "SELECT count(*) FROM fixture.identity WHERE fixture_id=@id", ("id", Guid.Parse(fixtureId))) == "1", "WRONG_MARKER");
            Require(await Scalar(c, "SELECT fixture_control.system_identifier()::text") == systemId, "WRONG_CLUSTER");
            await Sql(c, "SET lock_timeout='2s'; SET statement_timeout='8s'; SET idle_in_transaction_session_timeout='15s'; SET synchronous_commit=on; SET search_path=pg_catalog");
            return c;
        }
        catch { await c.DisposeAsync(); throw; }
    }
    internal static async Task<int> Sql(NpgsqlConnection c, string sql, params (string, object)[] ps)
    {
        await using var cmd = new NpgsqlCommand(sql, c);
        foreach (var (key, value) in ps) cmd.Parameters.AddWithValue(key, value);
        return await cmd.ExecuteNonQueryAsync();
    }
    internal static async Task<string> Scalar(NpgsqlConnection c, string sql, params (string, object)[] ps)
    {
        await using var cmd = new NpgsqlCommand(sql, c);
        foreach (var (key, value) in ps) cmd.Parameters.AddWithValue(key, value);
        return Convert.ToString(await cmd.ExecuteScalarAsync(), CultureInfo.InvariantCulture) ?? "";
    }
    internal static async Task<Dictionary<string, string>?> Row(NpgsqlConnection c, string sql, params (string, object)[] ps)
    {
        await using var cmd = new NpgsqlCommand(sql, c);
        foreach (var (key, value) in ps) cmd.Parameters.AddWithValue(key, value);
        await using var reader = await cmd.ExecuteReaderAsync();
        if (!await reader.ReadAsync()) return null;
        var row = new Dictionary<string, string>();
        for (var i = 0; i < reader.FieldCount; i++) row[reader.GetName(i)] = Convert.ToString(reader.GetValue(i), CultureInfo.InvariantCulture) ?? "";
        return row;
    }
    internal static Evidence ToEvidence(Dictionary<string, string> p, string owner) => new(owner, p["state"], p["prepared"], p["applied"], p["completed"], p["decision_digest"],
        p["id"], p["realm"], p["origin"], p["hash"], p["profile"], p["fixed_set"], p["incarnation"], long.Parse(p["generation"], CultureInfo.InvariantCulture), p["worker"]);
    internal async Task<Lookup> Lookup(string id, string participant, string phase)
    {
        await using var c = await Open();
        Require(LocalOwner == "geometry", "COORDINATOR_DATABASE_REQUIRED");
        var a = await Row(c, "SELECT * FROM fixture.actions WHERE id=@id", ("id", id));
        Require(a != null, "UNKNOWN");
        var l = new Lookup { Id = id, Participant = participant, Phase = phase,
            Realm = a!["realm"], Origin = a["origin"], Hash = a["hash"], Profile = a["profile"],
            Incarnation = a["incarnation"], Generation = long.Parse(a["generation"], CultureInfo.InvariantCulture),
            Worker = a["worker"], Decision = a["decision"], DecisionDigest = a["decision_digest"],
            Completion = phase == "COMPLETE" ? a["completion"] : "", Recovering = bool.Parse(a["recovering"]),
            Evidence = JsonSerializer.Deserialize<List<Evidence>>(a[phase == "COMPLETE" ? "applied_set" : "prepared_set"], Program.Json)! };
        l.Payload = Payload(l);
        return l;
    }
    internal static string Payload(Lookup l) => Fingerprint("phase-payload-v1", l.Id, l.Participant, l.Phase, l.Hash, l.DecisionDigest, l.Completion);
    internal async Task Hook(Request r, string point)
    {
        if (r.Hook != point) return;
        Require(r.HookId.Length is > 0 and < 80 && r.HookId.All(char.IsAsciiLetterOrDigit), "BAD_HOOK");
        var marker = Path.Combine(controlRoot, r.HookId);
        await File.WriteAllTextAsync(marker + ".reached", point);
        var deadline = DateTime.UtcNow.AddSeconds(20);
        while (!File.Exists(marker + ".release"))
        {
            Require(DateTime.UtcNow < deadline, "HOOK_TIMEOUT");
            await Task.Delay(25);
        }
    }
    internal async Task<Reply> Execute(Request r)
    {
        Require(Participants.Contains(r.Owner), "BAD_OWNER");
        Require(r.Id.Length <= 64 && r.Value.Length <= 256 && r.Key.Length <= 128, "BOUNDS");
        await using var c = await Open();
        Require(r.Owner == LocalOwner, "WRONG_RECIPIENT");
        if (r.Operation is "CREATE" or "TAKEOVER" or "DECIDE" or "CERTIFY" or "ACK" or "COORD_RESTORE" or "DELIVERY_ACK")
            Require(LocalOwner == "geometry", "COORDINATOR_DATABASE_REQUIRED");
        if (r.Operation == "SEED")
        {
            await Sql(c, "INSERT INTO fixture.scope(id,state,incarnation,manifest) VALUES(1,'OPEN','origin-1','initial') ON CONFLICT DO NOTHING; INSERT INTO fixture.resources VALUES(1,1000),(2,1000) ON CONFLICT DO NOTHING");
            return new Reply();
        }
        if (r.Operation == "RESET")
        {
            // Fixed catalogue reset between independent synthetic test cases, never a product cleanup API.
            await using var reset = await c.BeginTransactionAsync();
            await Sql(c, "DELETE FROM fixture.audit; DELETE FROM fixture.acks; DELETE FROM fixture.outbox; DELETE FROM fixture.dedup; DELETE FROM fixture.effects; DELETE FROM fixture.holds; DELETE FROM fixture.mappings; DELETE FROM fixture.participants; DELETE FROM fixture.actions; UPDATE fixture.resources SET quantity=1000; UPDATE fixture.scope SET state='OPEN',incarnation='origin-1',manifest='initial',pending='',revision=revision+1 WHERE id=1");
            await reset.CommitAsync(); return new Reply();
        }
        if (r.Operation == "READ")
        {
            var read = await Row(c, "SELECT * FROM fixture.participants WHERE id=@id", ("id", r.Id));
            return new Reply { Status = read == null ? "ABSENT" : "OK", Evidence = read == null ? null : ToEvidence(read, r.Owner) };
        }
        await using var tx = await c.BeginTransactionAsync();
        var exclusive = r.Operation is "FREEZE" or "STAGE" or "OPEN";
        var gate = await Row(c, "SELECT * FROM fixture.scope WHERE id=1 FOR " + (exclusive ? "UPDATE" : "SHARE"));
        Require(gate != null, "UNSEEDED");
        await Hook(r, "gate");
        await Sql(c, "INSERT INTO fixture.audit(action_id,operation,phase) VALUES(@id,@op,'gate')", ("id", r.Id), ("op", r.Operation));
        Reply reply;
        switch (r.Operation)
        {
            case "CREATE":
                Require(gate!["state"] == "OPEN", "GATE_CLOSED");
                Require(r.DeadlineSeconds is >= 5 and <= 30, "DEADLINE_SETTING_BOUNDS");
                await Sql(c, "INSERT INTO fixture.actions(id,realm,origin,hash,profile,incarnation,generation,worker,deadline,fixed_set) VALUES(@id,'synthetic-realm','origin-1',@hash,'stage-s-v1',@inc,@gen,@worker,clock_timestamp()+make_interval(secs=>@seconds),'geometry,material,terrain')",
                    ("id", r.Id), ("hash", Hash(r.Id)), ("inc", r.Incarnation), ("gen", r.Generation), ("worker", r.Worker), ("seconds", r.DeadlineSeconds));
                reply = new Reply(); break;
            case "TAKEOVER": case "DECIDE": case "CERTIFY": case "ACK": case "COORD_RESTORE": case "DELIVERY_ACK":
                reply = await Coordinator(c, r, gate!); break;
            case "BARRIER": case "PREPARE": case "COMMIT": case "ABORT": case "COMPLETE":
                reply = await Participant(c, r, gate!); break;
            case "FREEZE":
                await Sql(c, "UPDATE fixture.scope SET state='FROZEN',revision=revision+1 WHERE id=1"); reply = new Reply(); break;
            case "STAGE":
                Require(r.Value == "TRUSTED_TEST_PROOF" && r.Manifest.Length > 0, "PROOF_OR_GATE");
                if (gate!["state"] == "RECONCILING")
                {
                    Require(gate["pending"] == r.Incarnation && gate["manifest"] == r.Manifest && await Scalar(c, "SELECT count(*) FROM fixture.mappings WHERE action_id=@id AND new_incarnation=@inc AND manifest=@m AND worker=@w", ("id", r.Id), ("inc", r.Incarnation), ("m", r.Manifest), ("w", r.Worker)) == "1", "ENROLLMENT_CONFLICT");
                    reply = new Reply(); break;
                }
                Require(gate["state"] == "FROZEN", "PROOF_OR_GATE");
                Require(await Sql(c, "INSERT INTO fixture.mappings(action_id,origin,old_incarnation,old_manifest,new_incarnation,old_generation,old_worker,manifest,worker) VALUES(@id,'origin-1',@old,@oldm,@new,@oldg,@oldw,@m,@w) ON CONFLICT(action_id) DO UPDATE SET manifest=fixture.mappings.manifest WHERE fixture.mappings.manifest=excluded.manifest AND fixture.mappings.new_incarnation=excluded.new_incarnation AND fixture.mappings.worker=excluded.worker AND fixture.mappings.old_generation=excluded.old_generation AND fixture.mappings.old_worker=excluded.old_worker",
                    ("id", r.Id), ("old", gate["incarnation"]), ("oldm", gate["manifest"]), ("new", r.Incarnation), ("oldg", r.Generation), ("oldw", r.PreviousWorker), ("m", r.Manifest), ("w", r.Worker)) == 1, "ENROLLMENT_CONFLICT");
                await Sql(c, "UPDATE fixture.scope SET state='RECONCILING',pending=@inc,manifest=@m,revision=revision+1 WHERE id=1", ("inc", r.Incarnation), ("m", r.Manifest));
                reply = new Reply(); break;
            case "OPEN":
                Require(r.Value == "TRUSTED_TEST_RECONCILED" && gate!["state"] == "RECONCILING", "RECONCILIATION_REQUIRED");
                Require(await Scalar(c, "SELECT count(*) FROM fixture.participants WHERE incarnation<>@inc", ("inc", gate["pending"])) == "0", "OLD_TUPLES");
                Require(await Scalar(c, "SELECT count(*) FROM fixture.mappings m LEFT JOIN fixture.participants p ON p.id=m.action_id WHERE p.id IS NULL OR p.incarnation<>m.new_incarnation OR p.origin<>m.origin OR p.worker<>m.worker OR p.manifest<>m.manifest") == "0", "MAPPED_BARRIER_REQUIRED");
                await Sql(c, "UPDATE fixture.scope SET state='OPEN',incarnation=pending,pending='',revision=revision+1 WHERE id=1");
                reply = new Reply(); break;
            default: throw new RuleException("UNKNOWN_OPERATION");
        }
        await Hook(r, "effect");
        if (r.Rollback) throw new RuleException("INJECTED_ROLLBACK");
        await tx.CommitAsync();
        await Hook(r, "commit");
        return reply;
    }
    private static void Binding(Lookup l, Request r)
    {
        Require(l.Issuer == "trusted-stage-s-building-oracle" && l.Id == r.Id && l.Participant == r.Owner && l.Phase == r.Operation, "LOOKUP_BINDING");
        Require(l.Realm == "synthetic-realm" && l.Profile == "stage-s-v1" && l.Hash == Hash(l.Id) && l.FixedSet == "geometry,material,terrain" && l.Origin == "origin-1", "IDENTITY_CONFLICT");
        Require(l.Generation > 0 && l.Worker is "worker-1" or "worker-2" && l.Payload == Payload(l), "PAYLOAD_OR_WORKER");
    }
    private async Task<Reply> Participant(NpgsqlConnection c, Request r, Dictionary<string, string> gate)
    {
        var l = r.Lookup ?? throw new RuleException("LOOKUP_REQUIRED"); Binding(l, r);
        var barrier = r.Operation == "BARRIER";
        Require(gate["state"] == "OPEN" || (barrier && gate["state"] == "RECONCILING"), "GATE_CLOSED");
        Require(l.Incarnation == (gate["state"] == "RECONCILING" ? gate["pending"] : gate["incarnation"]), "INCARNATION_FENCED");
        if (!barrier) Require(!l.Recovering, "BARRIERS_REQUIRED");
        if (gate["state"] == "RECONCILING")
            Require(await Scalar(c, "SELECT count(*) FROM fixture.mappings WHERE action_id=@id AND origin=@o AND new_incarnation=@inc AND manifest=@m AND worker=@w",
                ("id", r.Id), ("o", l.Origin), ("inc", l.Incarnation), ("m", gate["manifest"]), ("w", l.Worker)) == "1", "MAPPING_REQUIRED");
        if (r.Operation == "PREPARE") Require(l.Decision == "" && !l.Recovering, "AUTHORITATIVE_PHASE");
        if (r.Operation is "COMMIT" or "ABORT") Require(l.Decision == r.Operation && l.DecisionDigest.Length == 64, "AUTHORITATIVE_PHASE");
        if (r.Operation == "COMPLETE") Require(l.Decision == "COMMIT" && l.Completion.Length == 64, "AUTHORITATIVE_PHASE");
        var p = await Row(c, "SELECT * FROM fixture.participants WHERE id=@id FOR UPDATE", ("id", r.Id));
        if (p == null)
        {
            Require(barrier || (l.Generation == 1 && l.Incarnation == "origin-1"), "BARRIER_REQUIRED");
            if (l.Incarnation != l.Origin)
                Require(await Scalar(c, "SELECT count(*) FROM fixture.mappings WHERE action_id=@id AND origin=@o AND new_incarnation=@inc AND manifest=@m AND worker=@w", ("id", r.Id), ("o", l.Origin), ("inc", l.Incarnation), ("m", gate["manifest"]), ("w", l.Worker)) == "1", "MAPPING_REQUIRED");
            await Sql(c, "INSERT INTO fixture.participants(id,realm,origin,hash,profile,fixed_set,incarnation,generation,worker,manifest) VALUES(@id,@realm,@origin,@hash,@profile,@fixed,@inc,@gen,@w,@m) ON CONFLICT DO NOTHING",
                ("id", r.Id), ("realm", l.Realm), ("origin", l.Origin), ("hash", l.Hash), ("profile", l.Profile), ("fixed", l.FixedSet), ("inc", l.Incarnation), ("gen", l.Generation), ("w", l.Worker), ("m", gate["manifest"]));
            p = await Row(c, "SELECT * FROM fixture.participants WHERE id=@id FOR UPDATE", ("id", r.Id));
        }
        Require(p!["realm"] == l.Realm && p["origin"] == l.Origin && p["hash"] == l.Hash && p["profile"] == l.Profile && p["fixed_set"] == l.FixedSet, "IDENTITY_CONFLICT");
        if (p["incarnation"] != l.Incarnation)
        {
            Require(barrier && l.Generation == 1 && gate["state"] == "RECONCILING", "INCARNATION_FENCED");
            Require(await Scalar(c, "SELECT count(*) FROM fixture.mappings WHERE action_id=@id AND origin=@o AND old_incarnation=@old AND old_manifest=@oldm AND old_generation=@oldg AND old_worker=@oldw AND new_incarnation=@new AND manifest=@m AND worker=@w",
                ("id", r.Id), ("o", l.Origin), ("old", p["incarnation"]), ("oldm", p["manifest"]), ("oldg", long.Parse(p["generation"], CultureInfo.InvariantCulture)), ("oldw", p["worker"]), ("new", l.Incarnation), ("m", gate["manifest"]), ("w", l.Worker)) == "1", "MAPPING_REQUIRED");
        }
        else
        {
            var pg = long.Parse(p["generation"], CultureInfo.InvariantCulture);
            Require(l.Generation >= pg, "FENCED");
            Require(l.Generation != pg || l.Worker == p["worker"], "WORKER_CONFLICT");
            Require(l.Generation == pg || barrier, "BARRIER_REQUIRED");
        }
        await Hook(r, "row");
        if (barrier)
        {
            await Sql(c, "UPDATE fixture.participants SET incarnation=@inc,generation=@gen,worker=@w,manifest=@m,revision=revision+1 WHERE id=@id",
                ("inc", l.Incarnation), ("gen", l.Generation), ("w", l.Worker), ("m", gate["manifest"]), ("id", r.Id));
        }
        else
        {
            Require(r.Operation != "PREPARE" || p["state"] is "EMPTY" or "PREPARED", "TERMINAL_PREPARE");
            var key = r.Key.Length > 0 ? r.Key : Fingerprint("rpc-step-v1", r.Id, l.Incarnation, l.Generation.ToString(CultureInfo.InvariantCulture), l.Worker, r.Owner, r.Operation);
            var requestHash = Hash(JsonSerializer.Serialize(l, Program.Json));
            var prior = await Row(c, "SELECT * FROM fixture.dedup WHERE step_key=@k", ("k", key));
            if (prior != null) { Require(prior["request_hash"] == requestHash, "IDEMPOTENCY_MISMATCH"); return new Reply { Status = "REPLAY", Evidence = ToEvidence(p, r.Owner) }; }
            var state = p["state"];
            if (r.Operation == "PREPARE")
            {
                Require(state is "EMPTY" or "PREPARED", "TERMINAL_PREPARE");
                if (state == "EMPTY")
                {
                    // Shared scope lock permits different actions to contend on these same sorted rows.
                    await Sql(c, "SELECT id FROM fixture.resources WHERE id IN(1,2) ORDER BY id FOR UPDATE");
                    await Hook(r, "resources");
                    Require(long.Parse(await Scalar(c, "SELECT min(r.quantity-COALESCE((SELECT sum(h.quantity) FROM fixture.holds h WHERE h.resource_id=r.id),0)) FROM fixture.resources r WHERE id IN(1,2)"), CultureInfo.InvariantCulture) > 0, "INSUFFICIENT");
                    await Sql(c, "INSERT INTO fixture.holds VALUES(@id,1,1),(@id,2,1); UPDATE fixture.participants SET state='PREPARED',prepared=@d,revision=revision+1 WHERE id=@id", ("id", r.Id), ("d", Fingerprint("prepared-v1", r.Id, r.Owner)));
                }
            }
            else if (r.Operation == "COMMIT")
            {
                Require(state is "PREPARED" or "APPLIED" or "COMPLETED", "PREPARE_REQUIRED");
                Require(l.Evidence.Any(e => e.Participant == r.Owner && e.Prepared == p["prepared"]), "PREPARED_MISMATCH");
                Require(p["decision_digest"] == "" || p["decision_digest"] == l.DecisionDigest, "DECISION_CONFLICT");
                if (state == "PREPARED")
                {
                    await Sql(c, "SELECT id FROM fixture.resources WHERE id IN(1,2) ORDER BY id FOR UPDATE");
                    await Sql(c, "UPDATE fixture.resources SET quantity=quantity-1 WHERE id IN(1,2); DELETE FROM fixture.holds WHERE action_id=@id; INSERT INTO fixture.effects(action_id,debit) VALUES(@id,2); UPDATE fixture.participants SET state='APPLIED',applied=@d,decision_digest=@dd,revision=revision+1 WHERE id=@id", ("id", r.Id), ("d", Fingerprint("applied-v1", r.Id, r.Owner, l.DecisionDigest)), ("dd", l.DecisionDigest));
                }
            }
            else if (r.Operation == "ABORT")
            {
                Require(state is "EMPTY" or "PREPARED" or "ABORTED", "COMMIT_CANNOT_ABORT");
                Require(p["decision_digest"] == "" || p["decision_digest"] == l.DecisionDigest, "DECISION_CONFLICT");
                await Sql(c, "DELETE FROM fixture.holds WHERE action_id=@id; UPDATE fixture.participants SET state='ABORTED',decision_digest=@dd,revision=revision+1 WHERE id=@id", ("id", r.Id), ("dd", l.DecisionDigest));
            }
            else if (r.Operation == "COMPLETE")
            {
                Require(state is "APPLIED" or "COMPLETED", "APPLY_REQUIRED");
                Require(l.Evidence.Count == 3 && l.Evidence.Select(e => e.Participant).Order().SequenceEqual(Participants) && l.Evidence.Any(e => e.Participant == r.Owner && e.Applied == p["applied"] && e.Decision == p["decision_digest"]), "APPLIED_MISMATCH");
                Require(l.Completion == Certificate(l.DecisionDigest, l.Evidence) && p["decision_digest"] == l.DecisionDigest, "CERTIFICATE_CONFLICT");
                await Sql(c, "UPDATE fixture.effects SET output_held=false,output_available=true WHERE action_id=@id; UPDATE fixture.participants SET state='COMPLETED',completed=@d,revision=revision+1 WHERE id=@id", ("id", r.Id), ("d", l.Completion));
            }
            await Sql(c, "INSERT INTO fixture.dedup VALUES(@k,@h,'OK')", ("k", key), ("h", requestHash));
            await Sql(c, "INSERT INTO fixture.outbox(step_key,action_id,kind,phase,digest) VALUES(@k,@id,'PARTICIPANT',@phase,@d) ON CONFLICT DO NOTHING", ("k", key), ("id", r.Id), ("phase", r.Operation), ("d", requestHash));
        }
        p = await Row(c, "SELECT * FROM fixture.participants WHERE id=@id", ("id", r.Id));
        return new Reply { Evidence = ToEvidence(p!, r.Owner), Lookup = l };
    }
    internal static string Certificate(string decision, List<Evidence> es) => Fingerprint("certificate-v1", decision, JsonSerializer.Serialize(es.OrderBy(e => e.Participant).Select(e => new { e.Participant, e.Applied })));
    private async Task<Reply> Coordinator(NpgsqlConnection c, Request r, Dictionary<string, string> gate)
    {
        var a = await Row(c, "SELECT * FROM fixture.actions WHERE id=@id FOR UPDATE", ("id", r.Id));
        Require(a != null, "UNKNOWN");
        Require(a!["incarnation"] == r.Incarnation && a["generation"] == r.Generation.ToString(CultureInfo.InvariantCulture) && a["worker"] == r.Worker, "COORDINATOR_FENCED");
        await Hook(r, "row");
        if (r.Operation == "COORD_RESTORE")
        {
            Require(gate["state"] == "RECONCILING" && r.Manifest == gate["manifest"] && r.Value == "TRUSTED_TEST_FRONTIER", "RESTORE_PROOF_REQUIRED");
            Require(await Scalar(c, "SELECT count(*) FROM fixture.mappings WHERE action_id=@id AND origin=@o AND old_incarnation=@old AND old_manifest='initial' AND old_generation=@oldg AND old_worker=@oldw AND new_incarnation=@new AND manifest=@m AND worker='worker-2'",
                ("id", r.Id), ("o", a["origin"]), ("old", a["incarnation"]), ("oldg", r.Generation), ("oldw", r.Worker), ("new", gate["pending"]), ("m", r.Manifest)) == "1", "MAPPING_REQUIRED");
            await Sql(c, "UPDATE fixture.actions SET incarnation=@inc,generation=1,worker='worker-2',recovering=true,revision=revision+1 WHERE id=@id", ("inc", gate["pending"]), ("id", r.Id));
            return new Reply();
        }
        Require((gate["state"] == "OPEN" && gate["incarnation"] == r.Incarnation) ||
            (r.Operation == "ACK" && gate["state"] == "RECONCILING" && gate["pending"] == r.Incarnation), "GATE_CLOSED");
        if (r.Operation == "DELIVERY_ACK")
        {
            var decisionStep = r.Key == Fingerprint("coordinator-outbox-v1", r.Id, "DECIDE") && r.Value == a["decision_digest"] && a["decision"] is "COMMIT" or "ABORT";
            var completionStep = r.Key == Fingerprint("coordinator-outbox-v1", r.Id, "CERTIFY") && r.Value == a["completion"] && a["completion"].Length == 64;
            Require(decisionStep || completionStep, "OUTBOX_ACK_CONFLICT");
            Require(await Sql(c, "UPDATE fixture.outbox SET acknowledged=true WHERE step_key=@key AND action_id=@id AND kind='COORDINATOR' AND phase=@phase AND digest=@d", ("key", r.Key), ("id", r.Id), ("phase", completionStep ? "COMPLETE" : a["decision"]), ("d", r.Value)) == 1, "OUTBOX_ACK_CONFLICT");
            return new Reply();
        }
        if (r.Operation == "TAKEOVER")
        {
            Require(r.Generation < long.MaxValue, "OVERFLOW");
            await Sql(c, "UPDATE fixture.actions SET generation=generation+1,worker=@w,recovering=true,revision=revision+1 WHERE id=@id", ("w", r.Value), ("id", r.Id));
            return new Reply();
        }
        if (r.Operation == "ACK")
        {
            Require(r.Evidence.Count == 1 && Participants.Contains(r.Evidence[0].Participant), "ACK_BINDING");
            Require(r.Lookup != null && r.Lookup.Id == r.Id && r.Lookup.Incarnation == r.Incarnation && r.Lookup.Generation == r.Generation && r.Lookup.Worker == r.Worker && r.Lookup.Participant == r.Evidence[0].Participant && r.Lookup.Phase == "BARRIER", "ACK_BINDING");
            var e = r.Evidence[0];
            Require(e.ActionId == r.Id && e.Realm == a["realm"] && e.Origin == a["origin"] && e.Hash == a["hash"] && e.Profile == a["profile"] && e.FixedSet == a["fixed_set"] && e.Incarnation == r.Incarnation && e.Generation == r.Generation && e.Worker == r.Worker, "ACK_RECEIPT_BINDING");
            await Sql(c, "INSERT INTO fixture.acks VALUES(@id,@inc,@gen,@w,@p,@e) ON CONFLICT DO NOTHING", ("id", r.Id), ("inc", r.Incarnation), ("gen", r.Generation), ("w", r.Worker), ("p", r.Evidence[0].Participant), ("e", JsonSerializer.Serialize(r.Evidence[0], Program.Json)));
            if (await Scalar(c, "SELECT count(*) FROM fixture.acks WHERE action_id=@id AND incarnation=@inc AND generation=@g AND worker=@w", ("id", r.Id), ("inc", r.Incarnation), ("g", r.Generation), ("w", r.Worker)) == "3")
                await Sql(c, "UPDATE fixture.actions SET recovering=false,revision=revision+1 WHERE id=@id", ("id", r.Id));
            return new Reply();
        }
        if (bool.Parse(a["recovering"]))
        {
            Require(await Scalar(c, "SELECT count(*) FROM fixture.acks WHERE action_id=@id AND incarnation=@inc AND generation=@g AND worker=@w", ("id", r.Id), ("inc", r.Incarnation), ("g", r.Generation), ("w", r.Worker)) == "3", "BARRIERS_REQUIRED");
        }
        Require(r.Evidence.Count == 3 && r.Evidence.Select(e => e.Participant).Order().SequenceEqual(Participants), "FIXED_SET_REQUIRED");
        Require(r.Evidence.All(e => e.ActionId == r.Id && e.Realm == a["realm"] && e.Origin == a["origin"] && e.Hash == a["hash"] && e.Profile == a["profile"] && e.FixedSet == a["fixed_set"]), "EVIDENCE_IDENTITY_CONFLICT");
        if (r.Operation == "DECIDE")
        {
            Require(r.Value is "COMMIT" or "ABORT", "BAD_DECISION");
            Require(a["decision"] == "" || a["decision"] == r.Value, "IMMUTABLE_DECISION");
            if (a["decision"] == "")
            {
                if (r.Value == "COMMIT")
                {
                    Require(await Scalar(c, "SELECT clock_timestamp()<deadline FROM fixture.actions WHERE id=@id", ("id", r.Id)) == "True", "DEADLINE_EXPIRED");
                    Require(r.Evidence.All(e => e.State == "PREPARED" && e.ActionId == r.Id && e.Hash == a["hash"] && e.Prepared == Fingerprint("prepared-v1", r.Id, e.Participant)), "PREPARED_SET_REQUIRED");
                }
                var digest = Fingerprint("decision-v1", r.Id, a["origin"], r.Incarnation, r.Generation.ToString(CultureInfo.InvariantCulture), r.Worker, r.Value, JsonSerializer.Serialize(r.Evidence.OrderBy(e => e.Participant), Program.Json));
                await Sql(c, "UPDATE fixture.actions SET decision=@d,decision_digest=@h,prepared_set=@e,recovering=false,revision=revision+1 WHERE id=@id", ("d", r.Value), ("h", digest), ("e", JsonSerializer.Serialize(r.Evidence, Program.Json)), ("id", r.Id));
            }
        }
        else
        {
            var originalPrepared = JsonSerializer.Deserialize<List<Evidence>>(a["prepared_set"], Program.Json)!;
            Require(a["decision"] == "COMMIT" && r.Evidence.All(e => e.State is "APPLIED" or "COMPLETED" && e.Decision == a["decision_digest"] &&
                e.Applied == Fingerprint("applied-v1", r.Id, e.Participant, a["decision_digest"]) && originalPrepared.Any(p => p.Participant == e.Participant && p.Prepared == e.Prepared)), "ALL_APPLIED_REQUIRED");
            var cert = Certificate(a["decision_digest"], r.Evidence);
            Require(a["completion"] == "" || a["completion"] == cert, "IMMUTABLE_CERTIFICATE");
            await Sql(c, "UPDATE fixture.actions SET completion=@cert,applied_set=@e,recovering=false,revision=revision+1 WHERE id=@id", ("cert", cert), ("e", JsonSerializer.Serialize(r.Evidence, Program.Json)), ("id", r.Id));
        }
        var final = await Row(c, "SELECT decision_digest,completion FROM fixture.actions WHERE id=@id", ("id", r.Id));
        await Sql(c, "INSERT INTO fixture.outbox(step_key,action_id,kind,phase,digest) VALUES(@key,@id,'COORDINATOR',@phase,@digest) ON CONFLICT DO NOTHING", ("key", Fingerprint("coordinator-outbox-v1", r.Id, r.Operation)), ("id", r.Id), ("phase", r.Operation == "DECIDE" ? r.Value : "COMPLETE"), ("digest", final![r.Operation == "DECIDE" ? "decision_digest" : "completion"]));
        return new Reply();
    }
}
