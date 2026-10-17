# Legends of the Shattered Frontier — Security Part 4: Agent Implementation Guide and Secure Coding Rules

Version 1.0 — Expanded Technical Design Authority edition (2026-10-17)

Status: Design target; not implemented. Names per Shared Registry v1.0.

Entry file: [Security and Anti-Cheat Architecture](../Legends_Shattered_Frontier_Security_and_Anti_Cheat_Architecture_v1.0.md) · Registry: [Shared Registry v1.0](../Legends_Shattered_Frontier_Shared_Registry_v1.0.md)

---

## 1. Audience

Coding agents and engineers implementing `anticheat-service`, `enforcement-service`, `audit-service`, the security parts of `auth-service` and `gateway`, the server-authority validators inside `zone-server`/`instance-server`, and the client integrity module. Section 7 (secure coding rules) applies to **every coding agent in every repository** of the project, not only security services.

---

## 2. Repository and module layout

Follows the Repository Structure doc (`/GameClient`, `/GameServer`, `/BackendServices`, `/Launcher`, `/Infrastructure`, `/Tools`).

```text
/BackendServices
  /shared
    /security-policy        # policy engine library: permission strings, role map, linked-account checks
    /audit-client           # Append wrapper: audit-before-act helper, canonical JSON (RFC 8785)
    /idempotency            # idempotency table + middleware (7-day retention)
    /privacy                # field classification annotations, HMAC helpers, log scrubber
  /anticheat-service
    /api                    # lsf.anticheat_service.v1 protos + generated stubs
    /ingest                 # ReportSignals, ReportIntegrity, bus consumers
    /features               # windowed feature extractors (pure functions)
    /detectors              # rule evaluator (sandboxed expression subset) + definition loader
    /risk                   # risk score (pure function of detections) + band logic
    /store                  # schema `anticheat` migrations and repositories
  /enforcement-service
    /api  /cases  /sanctions  /appeals  /evidence  /rollback (saga + taint trace)  /store
  /audit-service
    /api  /chain (hashing, stream heads)  /anchor (WORM writer, Merkle root)  /verify  /store
  /auth-service/security    # credentials, MFA, devices, sessions, login risk, cooloffs
  /gateway/security         # proof-of-possession verification, rate limiting, connect tickets
/GameServer
  /Source/LSFServer/Security
    MovementValidator.*     # envelope budget, vertical, teleport tokens, clock ratio
    CombatValidator.*       # range, LOS, facing, cooldown, cast, resource, state
    InteractValidator.*
    RejectRollup.*          # 10 s rollups → ReportSignals
    SignalSummaries.*       # timing summaries, path digests
/GameClient
  /Source/LSFClient/Integrity   # challenge response, module enumeration, report signing
/Launcher/security              # manifest signature verification, pinned keys, device key
/Infrastructure/security        # WORM buckets, KMS keys, network policies, admin network
/Tools/security
  /detector-lint                # validates detector/ definitions (schema, expression subset, shadow flag)
  /replay-harness               # replays recorded movement/combat traces through validators
```

Content: `detector/<slug>` definitions live in the content repository under a **server-only** pack and are validated by `/Tools/security/detector-lint` in CI.

---

## 3. Build order and milestones

Milestones are ordered by dependency; each milestone ends with its acceptance criteria met.

| # | Milestone | Contents | Depends on |
| --- | --- | --- | --- |
| S0 | Shared foundations | `security-policy`, `audit-client`, `idempotency`, `privacy` libraries; secure coding CI checks (§7.3) | — |
| S1 | Audit service | Append with hash chain, stream heads, append-only triggers, anchors to WORM, verifier | S0 |
| S2 | Account security | Argon2id, breached-password check, MFA (WebAuthn, TOTP, recovery), device keys, PoP session binding, login risk, cooling-off, connect tickets | S0, S1 |
| S3 | Server authority validators | Movement, combat, interact validators; reject rollups; replay harness | Combat formulas available in game server |
| S4 | Invariants and reconciliation | INV-1..INV-13 checks with inventory/economy/reward owners; deterministic detectors | S1, owning services |
| S5 | Anticheat core | ReportSignals, detection store, detector evaluator, stream detectors (movement, combat, invariants), risk score, events | S3, S4 |
| S6 | Enforcement core | Cases, sanctions, CheckRestrictions, appeals, evidence sealing, admin APIs, admin-portal console | S1, S5 |
| S7 | Rollback saga | Taint trace, plan/approve/execute, restore saga, deficits | S6, owning services' compensation APIs |
| S8 | Client integrity | Signed manifests (with Client delivery), challenge/attest, deny-list | S2, S5 |
| S9 | Batch detectors | Bot, trade funnel, market detectors in analytics-pipeline; SubmitBatchDetections | S5, Analytics warehouse |
| S10 | Operations | Alerts, dashboards, runbook drills, chaos tests for audit-before-act | all |

Launch-blocking threats (Part 1, MMO threat catalog) require S0–S7 and S9's bot and RMT detectors before `prod`.

---

## 4. Invariants checklist

Every PR touching security-relevant code states which of these it can affect and which test proves it.

| ID | Invariant |
| --- | --- |
| SEC-1 | The client never decides a gameplay outcome; every intent is validated server-side (Part 1, Server authority). |
| SEC-2 | INV-1..INV-13 (Part 1) hold after every operation and after every rollback step. |
| SEC-3 | Risk score is a pure, deterministic function of stored detections, labels and time; recomputation reproduces the stored value. |
| SEC-4 | `account_security` and `staff` category detections never contribute to a player's risk score. |
| SEC-5 | Case transitions only follow the Part 2 state machine; `closed` is terminal. |
| SEC-6 | `permanent_ban` requires two distinct staff identities. |
| SEC-7 | Every privileged action has an audit record appended **before** the action takes effect; append failure ⇒ action not performed. |
| SEC-8 | Audit records cannot be updated or deleted; every stream verifies from genesis or last anchor. |
| SEC-9 | Rollback execution matches the approved plan hash; steps are idempotent (RB-1..RB-8). |
| SEC-10 | No p2/p3 field appears in logs, metrics labels, URLs, or bus events not classed for it. |
| SEC-11 | Every mutating RPC is idempotent for 7 days. |
| SEC-12 | Sessions are bound to device keys; a token without valid proof is rejected. |
| SEC-13 | Detector thresholds and deny-lists never ship to clients. |
| SEC-14 | Interim sanctions auto-lift if unreviewed within 24 h. |

---

## 5. Test matrix

Run only the tests that cover the change (repository AGENTS.md rule); the matrix lists what must exist.

### 5.1 Unit tests

| Area | Concrete cases |
| --- | --- |
| Movement budget | 7.0 m/s at 50 ms samples never rejects over 10,000 samples with ±30 ms jitter; 1.5× rejects within 2.6 s; 3× within 0.6 s; 12 m blink with no token rejects `movement_teleport`; same blink with valid unexpired token accepts and consumes token; expired token rejects |
| Vertical | jump apex 1.2 m + step accepted; 2.5 m rise with no ground contact rejected `movement_vertical`; hover 0.6 s rejected `movement_airtime`; fall from 20 m (airtime ≈ 2.02 s + jump) accepted |
| Clock ratio | ρ = 1.00 ± 0.03 with jitter → no signal; ρ = 1.10 sustained → signal |
| Combat | cooldown example (cd 12 s, haste 0.30): 8.30 s rejected, 8.36 s accepted; GCD at haste 0.30 = 1.05 s and at haste 0.60 floors at 0.75 s; 45 s cooldown without `haste_scaled_cooldown` unaffected by haste; haste input 0.9 clamped to 0.75; range with rewind capped at 200 ms; LOS blocked by wall; resource deducted atomically with ability start |
| Rate limits | token bucket refill math; 10× threshold for counting toward detector |
| Risk score | worked examples 1–4 (Part 2) reproduce 330, 511, 701, 861, 10 and 0 exactly (rounded); repeat damping k=3 → 0.25; false_positive relabel → recompute |
| Detector evaluator | rejects expressions with function calls outside allow-list, loops, attribute access; evaluation time bound enforced |
| Trade funnel | worked example → 23.6; guild-bank edges excluded |
| Case state machine | every allowed transition succeeds; every disallowed (e.g. `closed → open`, `dismissed → actioned`) fails with `FAILED_PRECONDITION` |
| Ladder | offense number counts only actioned cases in category within 365 d |
| Audit chain | genesis hash; single-field tamper detected at the tampered record; reordering detected; crypto-shredded record still verifies |
| Login risk | worked example 75 → MFA + cooling-off; with MFA 45 → MFA only |
| Argon2id | parameters stored; rehash-on-login when params upgraded |

### 5.2 Property-based tests

| Property | Generator |
| --- | --- |
| For any interleaving of trade/mail/market operations with retries and crashes, INV-1, INV-7, INV-8, INV-10 hold | random op sequences, injected failures between escrow phases, duplicate deliveries |
| Idempotent replay: applying any mutating RPC N times with same key = applying once | random RPCs and N ∈ [1, 5] |
| Movement validator never accepts cumulative displacement > ∫ v_eff dt × (1 + ε) + T_burst × v_max + n × s_slack | random speed schedules, jitter, packet loss |
| Risk score ∈ [0, 1000], monotone non-increasing in absence of new detections (except floor) | random detection histories |
| Rollback then restore returns every innocent holder to ≥ their original value and every account's holdings to pre-rollback (except offender) | random taint graphs ≤ 3 hops |
| Taint trace never assigns more taint than the origin amount | random transfer graphs |
| Audit verify(chain) = true iff no record modified | random chains with random mutations |

### 5.3 Integration tests

| Scenario | Expected |
| --- | --- |
| Speed-hack bot client on test realm (replay harness) | rejects → `security.action.rejected` rollup → `detector/speed_envelope` → `security.score.changed` → case opened at `high` |
| Duplicate trade completion delivered twice by bus | one transfer; second returns original result; no `item_ledger_mismatch` |
| Forced crash of zone-server during zone handoff with item in transit | item in exactly one location after recovery |
| Dupe rollback end-to-end (worked example Part 2: 40 swords) | 15 destroyed from offender, 25 innocent buyers refunded 30,000 c each, deficit 250,000 c, `enforcement.rollback.executed` totals match |
| Appeal overturn | sanctions lifted, detections `false_positive`, score recomputed, restore saga completes, compensation issued via liveops-service |
| `permanent_ban` with single approver | stays `pending_approval`; login unaffected until second approval |
| audit-service unavailable during GM item grant | grant fails closed; no item created |
| Refresh token reuse | token family revoked; `auth.session.ended` (`refresh_reuse`); `detector/login_anomaly` raised; risk score unchanged |
| Session token used from different device | gateway rejects `session_binding` |
| Integrity challenge with stale nonce | `FAILED_PRECONDITION`; outcome not recorded as pass |
| CheckRestrictions during enforcement-service outage | chat fail-open; banned account still denied via local deny cache; high-value trade held |

### 5.4 Load and resilience tests

| Test | Target |
| --- | --- |
| Movement validation per zone-server | 2,000 characters × 20 samples/s with < 5% tick budget |
| ReportSignals ingestion | 50,000 rollups/s per region, p99 < 50 ms |
| Risk recompute | 5,000 detections/s, p99 < 100 ms |
| Audit append | 2,000 appends/s per stream category/region, p99 < 20 ms |
| CheckRestrictions | 100,000 rps cached, p99 < 5 ms |
| Ban wave | 20,000 sanctions in one wave within rate limit, no consumer lag > 60 s |
| Login storm | 10× normal login rate with 80% failures: legitimate logins p95 < 3 s via queue |

---

## 6. Acceptance criteria

1. All worked examples in Parts 1–3 reproduce exactly in unit tests.
2. Invariant reconciliation runs hourly on staging with 0 violations for 7 consecutive days of synthetic load including fault injection.
3. Replay harness: 0 rejects on 50 recorded legitimate sessions (including 250 ms RTT, 2% loss); ≥ 99% of scripted speed/teleport/fly cheats rejected within Part 1 time-to-first-reject targets.
4. Detector activation gate (shadow ≥ one weekly reset, precision thresholds) enforced by tooling, not convention.
5. Audit chain verification and WORM anchors operational; tamper drill detected within 1 h.
6. Every `*Admin` RPC has a declared permission and a test proving denial for an unprivileged role and for linked-account targets.
7. Event payloads validate against registered schemas; no p2/p3 field in p1-classed events (schema lint).
8. Runbooks RB-IR-1, -2, -4 and -6 drilled on staging.

---

## 7. Secure coding rules (all repositories, all coding agents)

### 7.1 Rules

1. **Server authority.** Never accept an outcome from a client (damage, position result, loot, completion, currency). Accept intents, validate, decide server-side.
2. **Validate at every trust boundary.** Type, range, length, finiteness, enum membership, ID prefix format (`<prefix>_<ULID>`), content ID format (`<kind>/<slug>`). Reject unknown fields on external inputs.
3. **Parameterized queries only.** No string-built SQL; no dynamic table/column names from input.
4. **No `eval`, no dynamic code loading** from data, content packs or configuration. Expression evaluation uses the shared sandboxed evaluator.
5. **Idempotency.** Every mutating RPC takes and honors `idempotency_key`; consumers deduplicate events by `event_id`.
6. **Money and quantities are integers.** Currency in int64 minor units; check overflow on every arithmetic operation; never floating point.
7. **Atomic transfers.** Item and currency movements happen in one transaction with ledger entries; escrow for multi-party operations; serializable isolation or explicit row locks.
8. **Only owning services write their data.** No cross-schema writes; no shared DB credentials.
9. **Audit-before-act** for privileged and economic actions via `audit-client`; fail closed.
10. **Secrets** come from the secret manager at runtime; never in code, config files, logs, test fixtures committed to the repo, or container images. Test secrets are generated, clearly marked, and valid only for local/dev.
11. **Cryptography** uses vetted libraries and the algorithms named in this spec (TLS 1.3, AES-256-GCM/ChaCha20-Poly1305, Ed25519/P-256, SHA-256, HMAC-SHA256, Argon2id). No custom crypto; no MD5/SHA-1 for security purposes; constant-time comparison for secrets and MACs.
12. **Randomness** for tokens, nonces and loot uses a CSPRNG (loot RNG may be a seeded server-side PRNG per Itemization spec, never client-seeded).
13. **Privacy.** Annotate fields with `privacy_class`; never log p2/p3 values; never put personal data in URLs or metric labels; hash IPs with the realm-month HMAC before analytics.
14. **Authorization** checks happen in the service, not only in `admin-portal` or `gateway`; default deny.
15. **Dependencies** are pinned with lockfile hashes; new dependencies require license and vulnerability check; no install scripts from unpinned sources.
16. **Debug and cheat commands** are compiled out of `prod` builds (`#if !UE_BUILD_SHIPPING` or equivalent) and rejected server-side for player sessions.
17. **Error messages** to clients are generic; internal reason codes and stack traces never leave the server.
18. **Timeouts and limits** on every network call, message size and collection size from input.
19. **Logging** uses structured logs with `correlation_id`; log scrubber is enabled; tokens never logged.
20. **Deserialization** of untrusted data uses schema-validated formats (protobuf, JSON with schema); no native object deserialization of untrusted input.

### 7.2 Do / Don't

| Do | Don't |
| --- | --- |
| Use `security-policy`, `audit-client`, `idempotency`, `privacy` shared libraries | Re-implement permission checks or hash chains locally |
| Emit structured rejects with registry-style reason codes | Silently drop invalid intents without counting them |
| Add a property test when touching transfers, escrow, rewards or rollbacks | Rely only on example-based tests for concurrency code |
| Put new detectors in shadow mode first | Activate a detector with `auto_action` without two-person approval |
| Use tuning keys `tune.security.*` for thresholds | Hard-code thresholds in validators |
| Propose new names under Registry additions | Invent event names, enum values or ID prefixes inline |
| Treat client integrity results as signals | Ban on a client report alone |
| Use compensating entries for corrections | `UPDATE` ledger rows or delete audit rows |
| Keep detector logic server-side and confidential | Expose risk scores, detector names or thresholds in client-facing APIs or error messages |
| Fail closed for privileged actions and bans | Fail open on audit or authorization errors |

### 7.3 CI checks (S0)

* Static analysis for SQL string concatenation, `eval`/dynamic loading, weak crypto, hard-coded secrets (secret scanner), floating-point currency types in economy/inventory modules.
* Schema lint: event payload fields carry `privacy_class`; p1 events contain no p2/p3 fields.
* Detector lint for `detector/` content (schema, expression allow-list, shadow flag on new versions).
* Dependency audit and SBOM generation; signature/provenance on release artifacts.
* Admin API lint: every `*Admin` RPC declares a permission.

---

## 8. Pitfalls

1. **Using client timestamps for movement credit** — lets clock-acceleration hacks earn budget. Credit only server time; use client time only for the ρ ratio signal.
2. **Rolling up rejects in the bus** — one event per reject turns a cheat into a DDoS. Roll up at the producer.
3. **Penalizing ATO victims** — login anomalies are protective; never add them to the risk score.
4. **Detecting dupes but rolling back holders** — the holder is not necessarily the duper; freeze fast, attribute by ledger, then act.
5. **Unbounded taint traces** — limit hops and window; otherwise half the economy is "tainted".
6. **Reusing item IDs on restore** — INV-2 forbids it; re-create with provenance.
7. **Lag compensation without a cap** — unlimited rewind enables shooting into the past; cap at 200 ms.
8. **Desync bugs look like cheating** — a zone-wide reject storm usually means a server bug; `security_reject_storm` alert routes to gameplay on-call first.
9. **Interim sanctions forgotten** — auto-lift after 24 h unreviewed (SEC-14).
10. **Audit append outside the transaction boundary** — audit-before-act means append first, then act with the `audit_id` as causation; if the act fails, append an outcome record (`failed`) rather than deleting.
11. **Leaking detection timing** — immediate bans teach cheat developers what is detected; batch bans in waves except for economy-critical dupes.
12. **Trusting `admin-portal` UI checks** — services enforce permissions themselves.
