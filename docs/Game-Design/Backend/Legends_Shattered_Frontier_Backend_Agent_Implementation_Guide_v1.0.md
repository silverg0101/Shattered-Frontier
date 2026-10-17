# Legends of the Shattered Frontier — Backend Part 4: Agent Implementation Guide
## Version 1.0 — Expanded Technical Design Authority edition (2026-10-17)

Design target; not implemented. Names per Shared Registry v1.0.

Entry file: [Backend Architecture](../Legends_Shattered_Frontier_Backend_Architecture_v1.0.md) · Registry: [Shared Registry](../Legends_Shattered_Frontier_Shared_Registry_v1.0.md)

---

## 1. Purpose

Instructions for coding agents implementing the Backend-owned services and platform libraries. The specification parts are normative; this guide orders the work, lists invariants, and defines the tests that prove each milestone. The current Node.js proof of concept (`server/`, `public/`) does not implement any of this (Registry §0, rule 4); do not retrofit it.

---

## 2. Repository layout

Per the Repository Structure spec, `BackendServices/` is introduced only with a chosen implementation, and the language, migration tool, store topology and local provisioning must be recorded in a decision first. The Backend spec allows C#/.NET, Rust or Go; the layout below is language-neutral (Go-style names as example) and is a proposal until that decision exists.

```text
BackendServices/
  Shared/                          transport primitives only (Repository Structure: no gameplay authority)
    proto/lsf/<service_snake>/v1/  canonical .proto per service (single contract location)
    platform/
      idempotency/                 Part 2 §3 handler + table template
      outbox/                      outbox writer, relay, schema check
      inbox/                       consumer dedupe
      saga/                        saga runner (claim, step, compensate, stuck)
      rpc/                         deadlines, retries, breaker, ErrorInfo helpers, metadata
      authz/                       authz.yaml loader, default deny
      crypto/                      data-key client (GetDataKey), HMAC helpers
      envelope/                    Registry §10.1 envelope builder
      privacy/                     PrivacyParticipant server scaffolding
      transfer/                    TransferParticipant server scaffolding
  auth-service/                    account core (Backend) + security methods (Security) as separate modules
    internal/accountcore/  internal/privacy/  internal/security/   (security/ owned by Security spec)
    Migrations/
  character-service/
    internal/lifecycle/ names/ presence/ queue/ checkpoint/ transfer/
    Migrations/
  gateway/
    internal/routes/ ratelimit/ gates/ push/
  content-catalog-service/
    internal/publish/ lint/ validators/ activation/ delivery/
    Migrations/
  chat-service/  guild-service/
Infrastructure/
  k8s/<environment>/<dc_region>/   GitOps manifests
  terraform/                       clusters, buckets, KMS, networks
```

Module rule: `auth-service/internal/accountcore` must not import `internal/security` storage packages; it calls the security module's interface (entry file, Service ownership contracts, rule 1). Lint this with an import-boundary check in CI.

---

## 3. Build order and milestones

| Milestone | Scope | Done when |
| --- | --- | --- |
| B0 Platform libraries | `Shared/platform/*`: idempotency, outbox/relay, inbox, saga runner, rpc helpers, envelope, authz | U-1…U-12 and P-1…P-4 pass; a sample service uses all of them |
| B1 Persistence baseline | local provisioning of PostgreSQL (one cluster, schemas `auth`, `character`, `content_catalog`, `chat`, `guild`), Redis, Kafka-compatible bus, object storage emulator; migration runner | migrations apply from empty and N−1 → N; role grants verified (I-1) |
| B2 Account core | `CreateAccount`, `GetAccount`, ToS, directory, `SetAccountState`, session record, `ListSessionsOverlapping`; `auth.account.created`, `auth.session.*` | I-2, I-3; Security's `BeginLogin`/`CompleteLogin` stubbed behind its interface |
| B3 Gateway | route table, pipeline order, rate limits, `SetLoginGate`, `SetMinClientBuild`, `PushClientNotice`, push channel | I-4…I-6; login check order produces the right `login_failure_reason` for every check (I-7) |
| B4 Character core | creation saga, names, slots, deletion/restore/purge, `character.profile.*` | I-8…I-11 |
| B5 World entry | `EnterWorld`, presence, realm queue, checkpoints, `character.world.*`, `character.zone.changed` (with World stubs) | I-12…I-14, L-2 |
| B6 Content catalog | publish, lint, server-only packs, activation, `GetPack`, `CheckRollbackSafety`, `content.*` events | I-15…I-18 |
| B7 Privacy workflow | intake, states, `GetLegalHold` client, phases, export bundle, tombstones, events | I-19…I-24, P-5 |
| B8 Realm transfer | saga, participants in character-service plus reference participant stubs for others | I-25…I-28, P-6 |
| B9 Chat, mail, guild core | channels, mail saga, guild ranks, succession, `guild.*` | I-29…I-31 |
| B10 Operations | dashboards, SLO alerts, backup/restore automation with erasure replay, failover drill in staging | O-1…O-5, L-1, L-3 |

Each milestone ships its metrics (Part 3, Platform metrics) and its rows in `docs/dependency-map.md` (repository rule).

---

## 4. Invariants checklist

| # | Invariant | Where enforced |
| --- | --- | --- |
| BK-1 | A service writes only its own schema; no cross-schema grants | DB roles + CI grant lint |
| BK-2 | Every mutating RPC is idempotent by `(rpc, idempotency_key)`; same key + different hash → `ALREADY_EXISTS IDEMPOTENCY_MISMATCH` | platform idempotency handler |
| BK-3 | A domain event exists iff its state change committed (outbox in the same transaction) | outbox writer |
| BK-4 | A consumer applies an event at most once per consumer group (inbox in the same transaction) | inbox |
| BK-5 | Saga steps after the pivot are never compensated; before the pivot every completed step has a compensation | saga runner + saga definitions |
| BK-6 | `account_state` changes only through `SetAccountState` with CAS and an audit record written first | auth account core |
| BK-7 | No player login while `account_state ∈ {erasing, erased}`; no world entry while `pending_deletion` | auth gate, `EnterWorld` |
| BK-8 | Exactly one of `auth.login.succeeded` / `auth.login.failed` per login `correlation_id`; `reason` follows the Part 1 check order | auth-service |
| BK-9 | At most one live character per `(account, realm, slot)`; at most one `game` session per account | unique index; session supersede |
| BK-10 | Name keys are unique per realm across `live`, `deleted_hold`, `quarantine`, `transfer_hold` | `name_reservations` PK |
| BK-11 | A location checkpoint never moves `state_version` backwards | `CheckpointCharacter` guard |
| BK-12 | No server-only definition ID or object hash appears in any client-visible pack, client manifest, gateway response or player-facing message | catalog lint, release assertion, gateway response filter test |
| BK-13 | A privacy step never erases data under an active hold scope; the request never completes while a step is neither `done`, `held` nor `skipped` | workflow + participants |
| BK-14 | Restored databases replay erasure tombstones before serving traffic | restore job gate |
| BK-15 | A realm transfer creates or destroys no currency or item; staged digest equals committed digest | participants + orchestrator |
| BK-16 | A character is never mutable on two realms at once; `transferring` blocks all mutations | `TransferParticipant.PrepareTransfer` lock |
| BK-17 | p2/p3 values never appear in logs, traces, event payloads above their declared `privacy_class`, or URLs | log scrubber, schema check |
| BK-18 | `content_version` is strictly increasing per environment and packs are immutable | sequence + object lock |

---

## 5. Test matrix

### 5.1 Unit

| ID | Case | Expected |
| --- | --- | --- |
| U-1 | idempotent handler: same key, same body, twice | one business effect; identical responses |
| U-2 | same key, different body | `ALREADY_EXISTS IDEMPOTENCY_MISMATCH`; no effect |
| U-3 | deterministic failure (`FAILED_PRECONDITION`) then retry same key after state changed | stored failure returned |
| U-4 | transient failure (`UNAVAILABLE` from dependency) then retry | re-executed; success stored |
| U-5 | deadline propagation with 40 ms remaining | no downstream call; `DEADLINE_EXCEEDED` |
| U-6 | breaker: 12 of 20 calls fail in 10 s | opens; next call fails fast; probe after 5 s |
| U-7 | token bucket C = 30, r = 10/s, 45 requests in 1 s after idle | 40 allowed, 5 `rate_limited` with `retry_after_s = 1` |
| U-8 | realm queue ETA: position 120, λ_out = 0.1/s, admit_n = 0 | `eta_s = 1,200` |
| U-9 | name folding: `Arwen` vs `Αrwen` (Greek alpha), `ARWEN` | same `name_key` |
| U-10 | name pattern: `Al`, `Al'-ex`, `Ab3c`, `Мирa` (mixed script) | all rejected |
| U-11 | `m_login_success_rate` from the Part 1 worked example counts | 0.9838 |
| U-12 | privacy deadlines: verified 2026-11-02 10:00 | grace end 11-16 10:00; `deadline_at` 11-30 10:00 |

### 5.2 Property-based

| ID | Property |
| --- | --- |
| P-1 | For any interleaving of N concurrent identical requests (N ≤ 50), exactly one business effect and N identical responses. |
| P-2 | For any sequence of crashes injected between outbox insert, commit, publish and mark, every committed row is eventually published at least once and no uncommitted row is ever published. |
| P-3 | For any shuffled, duplicated delivery of a consumer's input events, the consumer's final state equals in-order single delivery. |
| P-4 | For any saga with k steps and a permanent failure injected at step j: j before pivot → state `compensated` and all compensations ran once; j after pivot → `stuck` and no compensation ran. |
| P-5 | For any random subject (accounts with 0–8 characters, items, wallets, guild leadership, mail both directions, holds over random scopes), after the workflow: no row keyed by the subject's IDs remains outside held scopes and retained ledgers; held scopes are untouched; totals of other players' wallets and items are unchanged. |
| P-6 | For any random character state, a transfer that commits leaves per-service digests equal before and after, total currency and item counts unchanged, and `faction.standings` rows identical (`value`, `tier`, `version`); an aborted transfer leaves the source byte-identical. |

### 5.3 Integration

| ID | Scenario | Expected |
| --- | --- | --- |
| I-1 | service role tries `SELECT` on another schema | permission denied |
| I-2 | `CreateAccount` replay with same key | same `account_id`, one `auth.account.created` |
| I-3 | second game login on the same account | first session ends `superseded`; `auth.session.ended` |
| I-4 | `SetLoginGate(eu-2)` then player login on `eu-2` and GM login | player `login_gated` (no `auth.login.failed`); GM allowed |
| I-5 | `SetMinClientBuild(10542)`; login with 10530 | `auth.login.failed reason = client_outdated, decided_by = gateway` |
| I-6 | Redis down for rate limits | per-pod fallback; login routes fail closed |
| I-7 | one login per check in the Part 1 order, each failing at its step | reasons `timeout`, `rate_limited`, `client_outdated`, `server_error`, `bad_credentials`, `session_binding`, `mfa_failed`, `challenge_failed`, `banned`, `suspended`, `account_locked` |
| I-8 | creation saga with progression returning `UNAVAILABLE` 3 times then OK | character `active`; one starter grant |
| I-9 | creation saga with permanent progression failure | character `purged`; name free immediately; no `character.profile.created` |
| I-10 | delete level-25 character, restore on day 29, delete again, wait 30 days | restored once; purged after second window; name in quarantine 90 days |
| I-11 | delete level-5 character | purged at next job run (no restore window) |
| I-12 | `EnterWorld` at capacity; reconnect within 300 s after `disconnect` | first queued; reconnect bypasses up to hard cap |
| I-13 | checkpoint with older `state_version` after handoff | `STALE_CHECKPOINT`; stored location unchanged |
| I-14 | snapshot zone disabled by `ff.content.disabled_definitions` | enters at safe location |
| I-15 | publish `client_visible` pack containing `detector/…` | publication fails lint |
| I-16 | client manifest generation with a server-only object hash injected | release assertion fails; `sev1` raised |
| I-17 | activation of version V on cohort; services report loaded | ≥ 99 % pods report V within 5 min; `content.version.activated` once |
| I-18 | `CheckRollbackSafety` where a new item definition has live instances | unsafe; blocking IDs listed |
| I-19 | delete request: submit, cancel on day 3 | `cancelled`; account `active`; events requested → cancelled |
| I-20 | delete request with an open case (hold scopes `enforcement_evidence`, `chat`) | `completed_with_holds`; chat excerpts retained; `HoldReleased` → `completed` |
| I-21 | delete while a character is `transferring` | transfer aborted (`account_deletion`) before phase 1 |
| I-22 | subject leads a guild with members; another guild where subject is sole member | leadership passes by succession; second guild `disbanded` with assets frozen 30 days |
| I-23 | export then delete ("export then delete" UI) | export completes first; delete proceeds |
| I-24 | `GetLegalHold` unavailable at processing start | phase 1 does not start; retry; alert after 72 h |
| I-25 | transfer with open market listing and party membership | rejected with `ECONOMY_PENDING`, `IN_GROUP` together |
| I-26 | transfer with destination name collision | completed; `rename_required`; `EnterWorld` refused until rename |
| I-27 | transfer where inventory `CommitTransfer` fails after pivot | retried until success; character stays `transferring`; `stuck` alert at 30 min |
| I-28 | transfer of character with 12 faction standings and active lockouts | standings rows identical on destination; lockouts carried; marks weekly counter carried |
| I-29 | mail with gold to a recipient under Economy transfer hold | mail `held`; delivered on `economy.hold.released` |
| I-30 | unread mail with attachments expires | returned to sender; second expiry destroys with `item_destroyed` |
| I-31 | guild leader inactive 90 days | leader changed by rule; `guild.leader.changed cause = leader_inactive` |

### 5.4 Load and resilience

| ID | Scenario | Pass criteria |
| --- | --- | --- |
| L-1 | 2 × phase peak CCU synthetic players (login, enter world, checkpoints, trades, quests) for 2 h | tier-0 SLOs met; outbox lag p99 ≤ 2 s; no saga `stuck` |
| L-2 | login storm: 20 % of realm CCU reconnects within 60 s after a zone-server loss | queue bypass honored; login p99 ≤ 2 s; `m_login_success_rate` ≥ 98 % |
| L-3 | kill one availability zone during L-1 | no data loss; recovery ≤ 5 min |
| O-1 | monthly restore test per cluster | row counts and ledger checksums match; erasure replay ran |
| O-2 | region-site failover drill in staging | runbook steps within targets (Part 3, Region-site failover runbook) |
| O-3 | relay leader killed mid-batch | no duplicate business effect downstream; duplicates absorbed by inboxes |
| O-4 | DR replica lag injected to 6 min | `m_db_replication_lag_s` alert `sev2` |
| O-5 | DLQ: poison event to a consumer | moved after 10 attempts; `sev3`; replay tool works and is audited |

---

## 6. Acceptance criteria (Backend v1)

1. Every RPC in the Backend RPC tables (Parts 1–3) exists with its authz entry, idempotency where mutating, and documented errors.
2. Every `auth.*`, `character.*`, `content.*` and `guild.*` event validates against its payload table and is registered in the schema registry.
3. Login failures in staging over a 24 h soak carry a valid `login_failure_reason` on 100 % of `auth.login.failed`.
4. A full delete request (with and without holds) passes I-19…I-24 and P-5 in an environment containing every participant (stubs allowed only for services not yet built, each stub recording calls).
5. A realm transfer passes I-25…I-28 and P-6 with real `progression`, `inventory`, `economy` and `faction` participants.
6. No server-only definition reaches a client artifact (I-15, I-16 green in the release pipeline).
7. Restore and failover drills meet Part 3 targets.

---

## 7. Pitfalls

- **Treating Redis as durable.** Presence and queues are Redis-authoritative by design; currency, items, XP and ledgers never are. A cache miss must fall through to PostgreSQL, not to "zero".
- **Fresh idempotency keys on retry.** A retry with a new key is a new request. Orchestrators derive keys from saga IDs.
- **Publishing events outside the transaction** ("commit, then publish") loses events on crash; use the outbox.
- **Choreographed multi-service changes.** If two services must both change or neither, write a saga with an orchestrator.
- **Cross-schema joins for convenience.** Even read-only joins create hidden coupling and break schema portability; call the owning service or consume its events.
- **Leaking existence.** Login and account-creation errors must not reveal whether an email exists; erased accounts look like unknown identifiers.
- **Names on events.** Character and guild names are never in payloads; consumers look them up.
- **Forgetting the hold check** on a code path that deletes subject data (character purge uses the same holds as account deletion).
- **Single-schema restores without reconciliation.** Always follow the owning spec's runbook.
- **Server-only leakage through errors.** A rejected action must not echo a `detector/` ID or loot-weight detail in the player message.

---

## 8. Do / Don't for coding agents

| Do | Don't |
| --- | --- |
| Use registry names exactly (services, prefixes, enums, events, `tune.*`, `ff.*`, `m_*`) | Invent a name without adding it to the entry file's Registry additions |
| Put every mutation, its idempotency row and its outbox row in one transaction | Write to another service's schema, even in tests |
| Return `ErrorInfo.reason` in UPPER_SNAKE_CASE with canonical gRPC codes | Return free-text errors clients must parse |
| Propagate `correlation_id` and deadlines on every hop | Retry mutating calls without the original key |
| Encrypt p2/p3 with the account data key; HMAC lookup fields | Log email, IP, device data, chat bodies or tokens |
| Derive behavior thresholds from `tune.backend.*` keys with the documented defaults | Hard-code cooldowns, windows or caps |
| Implement `PrivacyParticipant` and `TransferParticipant` in every new data-owning service | Ship a data-owning service without erasure and transfer support |
| Keep Security-owned tables and RPCs behind Security's module interface | Read `auth.credentials`, `mfa_factors` or `devices` from account-core code |
| Update `docs/dependency-map.md` when a change alters dependencies, file roles, routes, tests or commands | Edit the Shared Registry directly |
