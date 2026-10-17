# Legends of the Shattered Frontier — Quest Agent Implementation Guide

Version 1.0 — Expanded Technical Design Authority edition (2026-10-17)

Status: Design target; not implemented. Names per Shared Registry v1.0.

Back-links: [Quest entry](../Legends_Shattered_Frontier_Quest_Architecture_v1.0.md) · [Shared Registry](../Legends_Shattered_Frontier_Shared_Registry_v1.0.md)

---

This guide is for coding agents implementing `quest-service`. Read the entry file, then the Model and Objectives part, before writing code. The Rewards, Data and API part is the contract: DDL, protobuf, and event payloads. Do not deviate from those names.

## 1. Repository and module layout

Language: Go, one of the Backend spec's recommended stacks. The module boundaries below apply equally in C# or Rust.

```
/BackendServices/quest-service/
  cmd/quest-service/main.go          # wiring only: config, DB, Redis, bus, gRPC server
  api/proto/lsf/quest_service/v1/     # quest_service.proto (generated code under gen/)
  internal/
    model/          # pure types: QuestDefinition, Objective, Instance, states, enums (registry values)
    content/        # content-pack loader + definition validator (shared with pack build tool)
    statemachine/   # pure transition function: (instance, command|evidence) -> (instance', effects[])
    objectives/     # evidence matchers per objective type, delta computation
    rewards/        # w_eff computation, repeat multipliers, limits (pure)
    choices/        # consequence planning (pure) + appliers (RPC clients)
    givers/         # giver bindings, dialog-state projection
    generation/     # template grammar, slot binding, selection, constraints, dedup
    textgen/        # precomputed variants, LLM client adapter, guardrail checks, cache
    campaigns/      # thresholds, CP formulas, caps, stage CAS, tiering
    sharing/        # party share rules, group bonus
    store/          # PostgreSQL repositories, migrations (schema `quest`), outbox/inbox
    index/          # Redis objective index
    consumer/       # event-bus consumers (inbox -> fan-out -> statemachine)
    saga/           # turn-in saga runner + retry worker
    grpcapi/        # handlers, auth (mTLS identity -> caller class), error mapping
    clients/        # reward-service, inventory-service, faction-service, ai-director, anticheat, config
    events/         # quest.* payload structs + envelope builder (registry envelope)
    jobs/           # expiry sweeper, campaign recompute, archive, counters cleanup
  migrations/       # 0001_quest_schema.sql ...
  testdata/         # content packs, event fixtures, golden outputs
  tools/questlint/  # CLI validator for authored content (used by content CI)
```

Rules:

- `model`, `statemachine`, `objectives`, `rewards`, `generation` (excluding live projections), `campaigns` (formulas), and `choices` (planning) are **pure**: no I/O, no clock. Pass `now` and the RNG seed explicitly.
- Only `store` touches PostgreSQL, and only `index` touches Redis. Handlers and consumers orchestrate.
- `tools/questlint` and the runtime validator share `content/validator.go`. There must be no second implementation.

## 2. Build order and milestones

Each milestone ends with its tests green and a demo scenario.

| # | Milestone | Content | Demo / exit criterion |
| --- | --- | --- | --- |
| M1 | Skeleton and schema | Migrations, outbox/inbox, envelope builder, gRPC server with health, config/flags client | `quest.instance.offered` emitted for a fixture through the outbox relay |
| M2 | Definitions and validator | Content loader, JSON schema, referential checks, chain DAG checks, `questlint` | `quest/frontier_founder` passes. Fixtures with cycles, bad weights, or `currency/crowns` fail with clear messages. |
| M3 | State machine and log RPCs | Offer/accept/decline/abandon, prerequisites (projections stubbed), active cap, unique open instance | Property tests on transitions pass |
| M4 | Objective engine | Consumer, objective index, matchers for `kill`, `gather`, `craft`, `collect`, `explore`, `interact`, `build`, `reach_skill`, `complete_instance`, `defeat_encounter`; signals RPC with allowlist | `frontier_founder` completes end-to-end from fixture `world.structure.placed` events plus an interact signal |
| M5 | Turn-in saga and rewards | `w_eff`, repeat multipliers, limits, `deliver` consumption, IssueGrant, retries, `turn_in_pending` | Chaos test: reward-service fails 50 % of calls and every instance still ends with exactly one grant |
| M6 | Chains, choices, givers | Choice consequences, exclusive locks, flags, chain advancement, giver dialog states, fallbacks | Branch fixture: both branches completable; exclusive quests fail correctly |
| M7 | Sharing and group bonus | ShareQuest, shared kill credit, `escort`/`defend` group signals, `g_mult` | 5-member party scenario |
| M8 | Generation | Templates, grammar, seeded selection and binding, constraints, dedup, expiry and consolation, rate limits, precomputed text | Replay determinism: identical request → identical `gqd_` definitions |
| M9 | LLM text guardrails | Adapter behind `ff.quest.llm_text_enabled`, cache, checks, review queue hooks | With the LLM stubbed to return adversarial outputs, 100 % fall back |
| M10 | Campaigns | Schedule intake, thresholds, CP, caps, rate gate, stage CAS, tier rewards, recompute job, archive | Load test: 5,000 contributions/s with exactly one stage transition |
| M11 | Analytics and security | Metrics, anticheat feature summaries, admin RPCs with audit | Dashboards fed from a staging soak |

Dependencies on other services can be stubbed via `clients/` interfaces until those services exist. Each stub must enforce the same idempotency semantics as the real service.

## 3. Invariants checklist

Every invariant here must have at least one automated test (§4).

1. **I1 Single open instance**: at most one instance in `offered|active|ready_to_turn_in` per `(character_id, quest_id, generated_quest_id)`. Enforced by the DB partial unique index.
2. **I2 Legal transitions only**: transitions follow the state diagram in the Model part. Terminal states never change.
3. **I3 Monotonic progress**: `0 ≤ current ≤ required`. Decreases only for `build` destruction, `collect` snapshots, and hold rechecks.
4. **I4 Order gating**: no progress in order group n+1 before group n is complete.
5. **I5 Exactly-once reward**: per instance, at most one `reward_grant_id`, and the IssueGrant idempotency key is `<qst>:turn_in`. Consolation uses the same key.
6. **I6 Exactly-once evidence**: each `(event_id, character_id)` and each `signal_id` affects state at most once.
7. **I7 Weight bounds**: `w_eff ≤ w_ceiling` (1.5 generated; < 5.0 authored without review). `currency/crowns` never appears in a quest bundle.
8. **I8 L clamp**: reward level = `clamp(Lc, level_range.min, level_range.max)`.
9. **I9 Limits**: completions per period never exceed configured limits, even under concurrent turn-ins (counter row lock or `UPDATE … WHERE count < limit`).
10. **I10 Choices immutable**: one row per `(character, quest, choice_key)`. Abandonment does not delete it.
11. **I11 Exclusive groups**: a character never has completed or active quests from two different refs of one exclusive group.
12. **I12 Turn-in reachability**: every `turn_in` quest has an essential giver or ≥ 1 fallback (validator).
13. **I13 Generated determinism**: same `request_id` + content version + projections snapshot → same output. Replays return the stored result.
14. **I14 Generated content references only the pinned pack**.
15. **I15 Campaign conservation**: `stage.progress_cp_x2 = Σ contributions.cp_x2` after every recompute. The display counter may lag but is never persisted as truth.
16. **I16 Single stage transition**: exactly one `active → stage_complete` per stage.
17. **I17 Threshold monotonicity**: `threshold_cp` never increases after lock.
18. **I18 No client authority**: no RPC reachable from the gateway can change objective progress.
19. **I19 Outbox atomicity**: every state change and its `quest.*` event commit in the same transaction.
20. **I20 Envelope compliance**: event names are exactly the 13 registry `quest.*` names. Envelope fields follow registry §10.1. Absent context is omitted, not null.

## 4. Test matrix

### 4.1 Unit tests (pure packages)

| Area | Test cases |
| --- | --- |
| rewards | Founder L1 → `w_eff 0.9375`, gold 265, LP 2. Example A (L22 story): XP 938 with `xp_to_next = 9,000`, gold 13,788, LP 3. Example B (L65 daily veteran, 3 members): gold 46,005, marks 25, LP 0. Over-level factor 0.1 floor. Marks are zero when Lc = 59 and non-zero when Lc = 60. Half-away rounding (2.5 → 3). |
| rewards limits | 12th daily allowed, 13th rejected. Dynamic completion 16 gets 0.25 multipliers. Period boundary at each datacenter's daily reset (na 15:00 UTC, eu 04:00, apac 22:00, sa 18:00). |
| statemachine | Every (state, command) pair: legal ones produce the expected state and effects; illegal ones produce `QUEST_WRONG_STATE`. Auto completion skips `ready_to_turn_in`. A hold-check failure returns the instance to `active`. |
| objectives | Kill with credited and non-credited characters. Shared credit inside and outside 100 m. Late event (`occurred_at` before accept − 5 s) ignored. Order gating. Build destroyed at 59 s subtracts, at 61 s does not. Collect snapshot decreases. |
| generation | Grammar parse errors. Scarcity count (s = 0.8 on [5,15] → 7). Region cap `4 + ceil(30/5) = 10`. Dedup collision → re-seed. Seeded determinism. Feasibility rejects for insufficient spawns or nodes. Weight 1.51 rejected. |
| expiry | Consolation `p = 4/7 = 0.571` → weight `0.8 × 0.571 × 0.75 = 0.343`. `p = 0.49` → expired. `ready_to_turn_in` grace. |
| textgen | Over-length rejected. Unknown proper noun rejected. Number mismatch rejected. Currency mention rejected. Classifier "borderline" sent to the review queue. Timeout → fallback. Flag off → no LLM call. |
| campaigns | `T = max(20,000, 0.35 × 8,000 × 150) = 420,000`. Small realm `T = 31,500`. `copper_per_cp(40) = 1,050`. Deliver CP 31. Soft cap: raw 151 → accepted 150.5. Hard cap at 3F. Rate gate 400 CP/h. Tiers at 0.1/0.5/1.0 share. Failed-stage reward factor 0.5. |
| choices | Reputation delta bound ±500. Influence weekly cap 20. Exclusive lock fails other open instances. |
| givers | Dialog-state priority order. Relocate/hide/flee policies. Unreachable for 1,800 s → remote turn-in. |

### 4.2 Property-based tests

- **Random command and evidence sequences** (≥ 10,000 runs) against the state machine. I1–I4 and I10–I11 hold. Terminal states are never left.
- **Reward monotonicity**: for fixed L, `w_eff` is non-decreasing in `d_mult` and `g_mult`, and gold is non-decreasing in L.
- **Campaign caps**: for any contribution sequence, `accepted ≤ 2F`, `accepted ≤ raw`, and `Σ accepted = progress`.
- **Generation**: for random projections, every published gqd passes the validator re-run; no two published gqds share a signature.
- **Idempotency**: duplicating any subset of events or RPCs in any order yields the same final state as the deduplicated sequence.

### 4.3 Integration tests (PostgreSQL, Redis, bus, stub services)

| # | Scenario | Expected |
| --- | --- | --- |
| IT1 | `quest/frontier_founder` full flow: zone entry → offer → accept → 3 × `world.structure.placed` → interact signal → turn-in | Events in order: offered, accepted, progressed ×3 (or coalesced), objective.completed ×2, turned_in, completed. One IssueGrant with key `qst…:turn_in`. Flag `onboarding.founder_done = 1`. |
| IT2 | Turn-in with reward-service down for 10 min | `turn_in_pending` true; after recovery one grant, state `completed`, `m_quest_turn_in_pending_age` alert fired. |
| IT3 | Deliver quest with items moved away before turn-in | `HOLD_CHECK_FAILED`, state back to `active`, no consumption. |
| IT4 | Party of 5 shares a kill quest; one member is 150 m away | 4 credited; `g_mult = 1.15` for those completing within 300 s. |
| IT5 | Branch choice then abandon then re-accept | Choice still recorded and objective auto-satisfied; no second reputation delta. |
| IT6 | World event ends while 20 characters hold the generated quest at mixed progress | Consolation/expired per `p`; offered instances expired; gqd `expired`. |
| IT7 | Replay of the same `ProposeGeneratedQuests` request | Identical result from `generation_requests`. |
| IT8 | Region at cap | `RESOURCE_EXHAUSTED`, `remaining_region_budget = 0`. |
| IT9 | Campaign stage threshold crossed by 50 concurrent contributors | Exactly one `quest.campaign.stage_reached`. Late contributors get `CAMPAIGN_NOT_ACTIVE` and their items are not consumed. |
| IT10 | Signal from an unauthorised service identity | `PERMISSION_DENIED`, anticheat report sent. |
| IT11 | Giver faction loses region control | Dialog `unavailable`; turn-in via fallback NPC; `used_fallback = true`. |
| IT12 | Enforcement progression rollback | Instance annotated; grant revoked through reward-service; no re-grant. |

### 4.4 Load tests

| Test | Target |
| --- | --- |
| Inbound evidence | 20,000 domain events/s per realm cluster; p99 event lag ≤ 2 s; no consumer lag growth over 1 h |
| Turn-in | 500 turn-ins/s; p99 latency ≤ 300 ms (excluding downstream retries) |
| Giver state | 5,000 `GetGiverStates` batch calls/s (20 NPCs each); p99 ≤ 30 ms with cache |
| Generation | 50 requests/s realm-wide; p99 ≤ 250 ms without LLM |
| Campaign | 5,000 contributions/s; recompute job < 60 s for 10 M rows |
| Soak | 10,000 CCU simulated (World spec phase 3) for 24 h; no outbox backlog > 10 s |

## 5. Acceptance criteria

The service is accepted when all of these hold:

1. All invariants I1–I20 have passing tests; the property tests run in CI.
2. IT1–IT12 pass against stub services, and IT1, IT2, and IT9 pass against real staging services.
3. Load targets in §4.4 are met in `ptr`.
4. All 13 `quest.*` events are registered in the schema registry with the payload tables from the Rewards, Data and API part. Analytics confirms ingestion.
5. `questlint` runs in content CI and blocks a pack that fails validation.
6. Kill switches `ff.quest.generation_enabled`, `ff.quest.llm_text_enabled`, `ff.quest.sharing_enabled`, and `ff.quest.campaigns_enabled` take effect within 10 s without a restart.
7. Measured faucet and XP share dashboards exist and are populated from staging soak data.
8. Security review signs off on items 1–13 of Security considerations.

## 6. Pitfalls

- **Treating the registry functions as Quest-owned.** `xp_to_next` belongs to Character, and the scale constants belong to Economy/Character. Quest only passes `(L, w)`. Never hard-code XP tables.
- **Counting events before accept.** Cumulative objectives must check `occurred_at ≥ accepted_at − 5 s`. Forgetting this lets players pre-farm.
- **Using the Redis index as truth.** Always re-verify the instance state and filters from PostgreSQL inside the transaction.
- **Non-deterministic generation.** Never use wall-clock time or a global RNG in `generation`. Seed from `request_id` and pass the dedup bucket explicitly.
- **Blocking on the LLM.** Text must never delay publishing or offering. Publish with fallback text and swap later.
- **Emitting events outside the outbox.** A crash between commit and publish loses events, or publishes events for rolled-back changes.
- **Mid-stage threshold edits.** The only allowed adjustment is downward through the audited admin RPC.
- **Consuming items before validating state.** The turn-in saga validates state, distance, and option first, then consumes, then grants.
- **Forgetting reset time zones.** Daily and weekly periods follow the character's datacenter region schedule from `config-service`, not UTC midnight.
- **Inventing event names.** `quest.instance.ready` does not exist. Use `quest.objective.completed` with `instance_ready`.
- **Assuming proposed registry additions are canonical.** `world.structure.placed` and `world.poi.discovered` are proposed. Code against an adapter, so the topic and payload can change without touching the objective engine.

## 7. Do / Don't for coding agents

| Do | Don't |
| --- | --- |
| Use registry names verbatim: states, enums, events, ID prefixes | Add new states, events, or enum values without a registry addition |
| Keep formulas pure and table-tested with the worked examples in this spec | Duplicate formulas in handlers or SQL |
| Put every mutating RPC behind an idempotency key persisted ≥ 7 days | Rely on client retries being rare |
| Route all rewards through `reward-service` with source `quest_reward` | Call inventory-service or economy-service to create items or currency directly |
| Read tuning from `config-service` with the defaults given here | Hard-code tunables that have a `tune.quest.*` key |
| Validate generated quests with the same validator as authored content | Write a "lightweight" generator-only validator |
| Report suspicious patterns to `anticheat-service` and keep local limits simple | Implement sanctions or risk scoring in quest-service |
| Pin `content_version` and `definition_revision` on every instance | Re-interpret old instances with new definitions (except the `dynamic` re-pin rule) |
| Write the audit record for every admin RPC | Expose admin RPCs to the gateway |
| Update `docs/dependency-map.md` when the code lands (repository rule) | Edit the Shared Registry from implementation work |
