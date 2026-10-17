# Legends of the Shattered Frontier — Raid Part 4: Agent Implementation Guide
### Version 1.0 — Expanded Technical Design Authority edition (2026-10-17)

> Status: Design target; not implemented. Names per Shared Registry v1.0.
>
> Entry: [Raid Architecture](../Legends_Shattered_Frontier_Raid_Architecture_v1.0.md) · Registry: [Shared Registry](../Legends_Shattered_Frontier_Shared_Registry_v1.0.md)

Guide for coding agents implementing the raid parts of `group-service`, `instance-service`, and `instance-server`. The current Node.js proof of concept does not implement raids; this is the target design. Raid work starts only after the Dungeon spec's instance run lifecycle, lockout framework, encounter framework, and party service exist, because every raid module extends them.

---

## 1. Repository and module layout

Paths follow the Repository Structure doc's service-per-folder convention (adjust to its final form; keep module boundaries).

```text
services/
  group-service/
    src/raid/
      raid_group_model.*          # RaidGroup aggregate, invariants, version checks
      raid_group_store_redis.*    # Lua scripts: join/leave/move atomic with party exclusivity
      raid_group_store_pg.*       # durable snapshot writer
      ranks_permissions.*         # permission matrix (Part 1, Ranks and permissions)
      loot_settings.*             # cooldown + engaged guard
      ready_check_raid.*          # raid fields on Dungeon ready check
      listings.*
      grpc_raid_group_service.*   # lsf.group_service.v1.RaidGroupService
  instance-service/
    src/raid/
      raid_run.*                  # RequestRaidRun wrapper over Dungeon CreateRun
      gating.*                    # available(e, run, d) predicate
      eligibility.*               # reward eligibility (Part 2, Eligibility check)
      lockout_policy_raid.*       # loot/progress kinds, extend, reset handler
      attempt_ingest.*            # ReportRaidAttempt* handlers, kill transaction
      kill_validation.*           # Part 3, Kill validation
      raid_progress.*             # rpr_ updates
      first_kills.*               # hold, record, revoke
      world_effects.*             # influence formula, effect publication
      bonus_rolls.*
      events_raid.*               # payload builders, schema registry IDs
      grpc_raid_service.*         # lsf.instance_service.v1.RaidService
    migrations/instance/NNNN_raid_*.sql
    migrations/group/NNNN_raid_*.sql
game/ (UE5.8)
  Source/LSFServer/Raid/
    RaidScaling.*                 # Part 1 formulas, frozen at engage
    RaidPhaseController.*         # phase graph inside engaged
    RaidEnrageController.*
    RaidMechanics/                # one validator per raid_mechanic_kind
    RaidEncounterScript.*         # JSON script interpreter (closed op set)
    RaidKillEvidence.*
content/
  raids/<raid_slug>/raid.json
  raids/<raid_slug>/encounters/<encounter_slug>.json
tools/
  raid-lint/                      # content linter (§5)
  raid-sim/                       # headless encounter simulator for H_ref and timeline checks
```

---

## 2. Build order and milestones

| Milestone | Scope | Exit criteria |
| --- | --- | --- |
| R0 Prerequisites | Dungeon frameworks available: runs, lockouts, encounter state machine, parties, ready check | Dungeon acceptance tests green |
| R1 Raid group | RaidGroup CRUD, convert party, subgroups, ranks, permissions, ready check raid fields, registered `group.raid_group.*` | Unit + property tests (§4) pass; 25-member group load test |
| R2 Raid runs and gating | `RequestRaidRun`, raid_runs tables, difficulty switch rules, wing/boss gating, attunement and skip checks, `CheckRaidEligibility` | Gating table tests for all four difficulties |
| R3 Scaling and encounter runtime | instance-server scaling freeze, phase controller, enrage, arena seal/leash, script interpreter, 4 mechanic validators (`soak`, `interrupt`, `tank_swap`, `dps_check`) | Sim reproduces Part 1 worked examples exactly; one test boss killable in PTR |
| R4 Kill transaction | Attempt ingest, kill validation, eligibility, loot/progress lockouts, rewards via reward-service, `encounter.*` raid fields | Idempotency and crash-recovery integration tests |
| R5 Progression | RaidProgress, realm firsts (hold/record/revoke), extend/reset, Frontier Resolve, tuning versions on attempts | Realm-first race test with concurrent kills |
| R6 Loot | Personal/need-greed/master/council integration with loot-service, pity keys, tokens, bonus rolls, legendary fragments | Loot parity sim within 5%; council quorum tests |
| R7 World integration | Weekly influence, narrative flags, world event triggers, reversals | Faction-service consumes without duplicates under replay |
| R8 Remaining mechanics + hardening | All 14 mechanic validators, security detectors wiring, analytics dashboards, load tests | Acceptance criteria §6 met |

---

## 3. Invariants checklist

1. A character is in at most one party or raid group at any time.
2. RaidGroup: exactly one `leader`; ≤ 4 `assistant`; ≤ 25 members; each subgroup ≤ 5; loot master is leader/assistant when mode is `master_loot`/`council`; council size 2–4 when mode is `council`.
3. Loot settings never change while an encounter in the group's run is `engaged`, and changes apply only after the 60 s cooldown.
4. Scaling values for an attempt are computed once at engage and never change; they are persisted with the attempt.
5. Mythic attempts always have `n_eng` = 20 and `hp_mult` = 1.
6. At most one live (`active`/`extended`) loot lockout per (character, raid, difficulty, encounter); at most one live progress lockout per (character, raid, difficulty).
7. A character with a live progress lockout can only enter its `run_id` for that raid and difficulty.
8. Lockouts are written before any reward request; reward requests use `raidkill:{attempt_id}:{character_id}`; a given attempt never yields two grants for one character.
9. Only attempts with outcome `defeated` and `validation_status = passed` create lockouts, RaidProgress kills, first kills, world-effect inputs, or rewards.
10. At most one `recorded` first kill per (scope, scope_id, raid, difficulty, encounter).
11. Every world effect has a unique `application_key`; faction-service dedupes by `event_id`.
12. No raid module writes another service's tables; items/currency are created only via reward-service → inventory/economy.
13. Encounter scripts use only the closed op set; content cannot execute code.
14. Every published payload validates against its registered schema; raid fields live under `raid`.
15. Enum values used match the Shared Registry; any future unmerged proposal is explicitly scoped and feature-flagged in its owning spec.

---

## 4. Test matrix

### 4.1 Unit

| Area | Cases |
| --- | --- |
| Scaling | N = 10, 14, 20, 25 for `raid_large` reproduce Part 1 table exactly (H, adds, add_hp_corr, targets, cres); N below `N_min` clamps; mythic forces 20; `round` is half-even |
| H_ref authoring | Part 1 four-difficulty table reproduced (1.10 M / 1.32 M / 1.57 M / 1.86 M; enrage 486/488/496/504) |
| Enrage | Heroic `T_enrage` 496 ⇒ `T_soft` 422; ×1.30 at t = 452 |
| Frontier Resolve | w = 9 → 0, 10 → 0.05, 13 → 0.10, 22 → 0.25, 30 → 0.30; mythic 0 before next tier |
| Pity | 5 dry heroic ⇒ 0.42; 10 dry ⇒ 0.60 cap; mythic 5 dry ⇒ 0.38; group mode triggers at 8 |
| Drops | `drops(17)` = 3 + Bernoulli(0.4); `drops(5)` = 1 exactly |
| Influence | Part 2 worked example ⇒ 7.27; cap enforced at 15 |
| Gating | normal: wing first boss then any order; heroic: strict order; skip opens wing 3 without marking kills |
| Permissions | every cell of the Part 1 permission matrix, both allowed and denied |
| Eligibility | late joiner, loot-locked, AFK (active_fraction 0.39 ⇒ ineligible), progress-locked to other run |

### 4.2 Property-based

- For random N in flex range and γ ∈ [1.00, 1.08]: `|H/N − H_ref/N_ref| / (H_ref/N_ref) ≤ 0.03` (lint band holds), and `adds × add_hp_corr = a_ref × s` within 1e-9.
- For random sequences of join/leave/move/promote operations: RaidGroup invariants 1–2 always hold; versions strictly increase.
- For random kill/reset/extend sequences across weeks: lockout invariants 6–7 hold; extension_count never exceeds 3; extended lockouts keep `run_id`.
- For random replays/duplications of `ReportRaidAttemptEnded`: exactly one grant per eligible character, exactly one set of lockouts.
- Deterministic RNG: same `(attempt_id, mechanic_id, instance_seq)` ⇒ same target set.

### 4.3 Integration

| Scenario | Expected |
| --- | --- |
| Full kill flow with personal loot (20 players, 2 loot-locked) | 18 grants, 18 loot lockouts, 2 `ineligible` entries, `encounter.attempt.defeated` with raid fields, `raid.progress.updated` |
| Crash after commit, before reward calls | On restart, outbox replays grant requests; reward-service dedupes; no duplicates |
| Council: loot master offline during window | Fallback to need/greed among interested; `loot.drop.assigned` with fallback method |
| Loot mode change 30 s before pull | Kill uses previous mode; `group.raid_group.updated` recorded |
| Mythic progress lockout then attempt to join other mythic run | `RAID_PROGRESS_LOCKED` |
| Extend then reset | Lockout returns `active`, same run, defeated set intact, `instance.lockout.reset` |
| Two guilds kill same mythic boss 300 ms apart | First by `ended_at` recorded; second stays a normal kill; one `raid.progress.first_kill_recorded` per scope |
| Invalidate a first kill | Revocation event, grants revoked, next valid kill promoted, world-effect reversal published |
| Leash exploit | Attempt `invalidated`, detection raised, no lockouts |
| Weekly reset with world links | One `raid.world_effect.applied` per realm × link; replaying the reset job publishes nothing new |

### 4.4 Load

- 2,000 concurrent raid groups per realm cluster with membership churn at 5 ops/s per group: group-service p99 < 50 ms.
- Simultaneous kills: 300 kill transactions within 10 s (Tuesday reset rush) for one realm: instance-service p99 kill transaction < 250 ms; reward fan-out drains < 30 s.
- `encounter.mechanic.failed` batching: 25-player mythic attempt at worst case (all players failing every second) ≤ 1 event/s per attempt, ≤ 50 failures per event.
- instance-server: raid validators add ≤ 1.5 ms per 30 Hz tick with 25 players and 40 adds.

---

## 5. Content linter rules (`tools/raid-lint`)

1. IDs match registry formats; content slugs immutable once published.
2. `group_size_class` ∈ {`raid_small`, `raid_large`}; `raid_small` declares no `mythic`.
3. Every difficulty has `h_ref`, `t_target`, loot table, reward bundle.
4. `h_ref` within ±10% of the Part 1 authoring formula unless `tuning_override_reason`.
5. Each difficulty step adds ≥ 1 mechanic or escalation; mythic has ≥ 1 mythic-only mechanic.
6. All phases reachable; any `hp_below` phase has `min_duration`; intermissions have `max_duration` and penalty.
7. Every `mechanic` ∈ `raid_mechanic_kind`; required params present per kind.
8. Script ops ∈ closed op set.
9. Encounter coordination layers ≥ 3 (Part 1, Team mechanics and role coordination).
10. Token slots cover all 5 set slots across the raid.
11. World links reference existing `faction`, `world_region`, `world_event` IDs; caps set.
12. Reward weights (`gold_weight`, `marks_weight`) < 5.0.

---

## 6. Acceptance criteria

- All unit, property, integration, and load tests in §4 pass.
- A 20-player PTR group and a 12-player PTR group each kill the test boss on normal with per-player boss health within 3% of each other.
- A mythic attempt cannot be pulled with 19 or 21 members inside.
- Kill-to-reward latency p95 < 5 s; first kill published within hold + 10 s.
- Wipe funnel, mechanic failure rate, and loot parity dashboards populate from PTR data.
- No raid code path writes outside the `group`/`instance` schemas; static check enforced in CI.
- Registry-canonical names are used as registered; any future unmerged proposal is explicitly scoped and feature-flagged in the owning spec.

---

## 7. Pitfalls

- **Re-scaling mid-fight.** Recomputing health when someone disconnects makes kills easier by leaving — freeze at engage.
- **Granting before locking.** Rewards before lockouts allow double rewards on retries or concurrent runs.
- **Trusting client positions or hits.** All mechanic validation uses server positions and server resolve ticks.
- **Per-character world influence.** Influence counts kills per run, not participants, or big realms swamp control.
- **Forking Dungeon frameworks.** Raids add tables and fields; they do not copy or replace Dungeon run/lockout/encounter code.
- **Loot mode races.** Changing loot settings and killing in the same second must resolve to the previous settings.
- **Week keys.** Weekly reset differs per datacenter region; compute `week_key` from the region's reset schedule, never from UTC Monday.
- **Using stale registry names.** Resolve names against the Shared Registry, including registered raid-group events, progress events, revocation events, and enums.

---

## 8. Do / Don't for coding agents

| Do | Don't |
| --- | --- |
| Read the Registry and Dungeon spec frameworks before writing raid code | Invent new event names, enums, or ID prefixes without a Registry additions entry |
| Use `tune.raid.*` keys from config-service for every number in Parts 1–2 | Hard-code health, chances, cooldowns, or weeks |
| Write lockouts, progress, and first-kill rows in one transaction with an outbox for events and grant requests | Call reward-service inside the DB transaction |
| Use idempotency keys `raidkill:{attempt_id}:{character_id}` and `attempt-end:{attempt_id}` | Generate random idempotency keys on retry |
| Persist scaling, seed, and tuning version on every attempt | Recompute historical values from current tuning |
| Report suspicious kills to anticheat-service and let Security decide sanctions | Sanction or delete items from raid code |
| Keep raid payload fields under `raid` on Dungeon-owned events | Change Dungeon base payload fields |
| Run only targeted tests for the module you changed | Run the full suite unless asked |
| Update `docs/dependency-map.md` in the same change when files, routes, or tests change (per repo AGENTS.md) | Edit the Registry directly |
