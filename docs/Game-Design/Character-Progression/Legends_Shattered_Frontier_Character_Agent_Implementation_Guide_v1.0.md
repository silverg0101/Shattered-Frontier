# Character Progression — Agent Implementation Guide

Version 1.0 — Expanded Technical Design Authority edition (2026-10-17)

Status: Design target; not implemented. Names per Shared Registry v1.0.

Entry: [Character Progression Architecture](../Legends_Shattered_Frontier_Character_Progression_Architecture_v1.0.md) · Registry: [Shared Registry v1.0](../Legends_Shattered_Frontier_Shared_Registry_v1.0.md)

This guide tells coding agents how to build `progression-service` and its shared formula library. It does not authorize work by itself; the user's task and the repository's `AGENTS.md` define scope and test policy. The current Node.js proof of concept (`server/`, `public/`) does not implement any of this (see `docs/dependency-map.md`).

---

## 1. Repository and module layout (proposed)

The Backend spec allows C#/.NET, Rust, or Go. The layout is language-neutral; names below use Go conventions as an example.

```text
services/progression-service/
  api/proto/lsf/progression_service/v1/progression.proto   # Service Interfaces part, gRPC API
  cmd/progression-service/main.go
  internal/formulas/        # PURE functions only: xp curve, level-from-xp, kill/practice XP,
                            # SF/BF/DF, gain chance, DR, AM, attribute gain, NPC profile, training cost
  internal/domain/          # CharacterProgression aggregate, commit pipeline (caps, donors, locks),
                            # level-up, prestige, stage machine, specialization/mastery rules
  internal/windows/         # Redis Lua scripts + snapshot to practice_state
  internal/store/           # PostgreSQL repositories, ledger writer, outbox, idempotency
  internal/achievements/    # event consumer, criteria evaluators, seasonal windows
  internal/rpc/             # gRPC handlers, caller authorization, validation
  internal/tuning/          # tune.progression.* loader with typed defaults and bounds
  internal/content/         # content-pack reader + publish-time validators (skills, trees, specs, achievements, npc templates)
  migrations/               # DDL from Service Interfaces part
  testdata/golden/          # formula vectors (JSON) shared with other runtimes
shared/progression-golden-vectors/   # same vectors consumed by UE C++ NPC profile code and tooling
```

Rules: `internal/formulas` has no I/O, no clock, no RNG (RNG is injected). Everything that the simulation servers must reproduce (NPC profiles, difficulty hint colours, effective skill value) is covered by golden vectors.

---

## 2. Build order and milestones

| Milestone | Scope | Exit evidence |
| --- | --- | --- |
| M0 Formulas | `internal/formulas` + golden vectors for every formula and worked example in the Leveling and Skills parts | All worked examples reproduce exactly (see §4.1) |
| M1 Core store | Migrations, aggregate load/save with `FOR UPDATE` + `version`, ledger, outbox relay, idempotency table, `InitializeCharacter`, `GetProgression` | Integration tests I1–I3 |
| M2 XP and levels | `GrantXp`, `GrantKillXp`, level crossings, LP from levels, attribute points, prestige, `progression.xp.granted` / `level.reached` / `lp.granted` / `prestige.rank_reached` | Level table reproduces; multi-level grant emits ordered events |
| M3 Skills core | `SubmitPracticeBatch` with gain formula (DR = AM = 1), caps/locks/donors, attribute side-gains, practice XP, band events, `ChangeLock`, `AllocateAttributePoints` | Property tests P1–P5 |
| M4 Anti-grind | Redis windows, focus fatigue, rested accrual/consumption, diversity, AM components, `G_HARD`, anticheat signals, snapshot/restore | Unit U10–U16; integration I6 |
| M5 LP sinks | `TrainSkill` (with economy debit), mastery unlock/reset/activation, specializations select/remove, `GetCombatProfile` | Integration I7–I9 |
| M6 Endgame | Achievements consumer (all criterion types), seasonal windows, titles, account unlocks, catch-up, stage machine | Integration I10–I12 |
| M7 Hardening | `ApplyRollback`, `AdminAdjust` with audit, load tests, analytics event validation in schema registry | Load L1–L3; property P6–P7 |

Dependencies outside this service: `content-catalog-service` (packs), `config-service` (tuning), `economy-service` (training debit), `reward-service` (caller), `anticheat-service` (signals), `audit-service`. Use fakes behind interfaces until each is available.

---

## 3. Invariants checklist

Every invariant below must hold after every committed transaction; tests reference these IDs.

| ID | Invariant |
| --- | --- |
| INV-1 | `level == level_from_xp(xp_total)` clamped to `LEVEL_CAP`; `prestige_xp < xp_to_next(LEVEL_CAP)` unless rank = `PRESTIGE_RANK_MAX`. |
| INV-2 | `0 ≤ skill.value ≤ effective_cap(skill)` except a skill whose specialization was removed may hold 901–1000 (stored) while effective value is 900. |
| INV-3 | `Σ skill.value ≤ GLOBAL_SKILL_CAP`; per track `≤ TRACK_SKILL_CAP`. |
| INV-4 | `ATTRIBUTE_MIN ≤ attribute.value ≤ ATTRIBUTE_NATURAL_CAP`; `Σ attributes ≤ ATTRIBUTE_TOTAL_NATURAL_CAP`. |
| INV-5 | `lp_balance ≥ 0` and `lp_balance == Σ lp ledger deltas`. |
| INV-6 | Every change to level, XP, LP, skill, attribute, node, specialization, title, stage has exactly one ledger entry and exactly one outbox event in the same transaction. |
| INV-7 | A skill only increases when its lock is `up`; a `down` skill only decreases through donation, rollback, or GM; a `locked` skill never changes through play. |
| INV-8 | Mastery node ownership satisfies tier prerequisites at unlock time; at most one node per `exclusive_group`. Activation is computed, not stored. |
| INV-9 | At most `MAX_SPECIALIZATIONS` selections, one per skill, slot unlocked by level. |
| INV-10 | `stage` equals the highest stage whose gates hold, except stages never regress without `cause = rollback`. |
| INV-11 | Idempotent replay returns identical results and writes nothing. |
| INV-12 | No XP, LP, skill, or attribute ever derives from `currency/crowns` or a `store_purchase` source. |
| INV-13 | Derived values recomputed from the ledger equal stored values (audit job). |
| INV-14 | Gains per skill per rolling 60 min ≤ `G_HARD(S)`. |

---

## 4. Test matrix

Run only the tests covering the change (repository policy); the full suite only when explicitly requested.

### 4.1 Unit tests (formulas)

| ID | Case | Expected |
| --- | --- | --- |
| U1 | `xp_to_next(1)`, `(50)`, `(99)`, `(100)` | 1,000; 142,020; 394,620; 400,600 |
| U2 | `xp_total_for(100)` | 15,859,950 |
| U3 | `level_from_xp(2,787,479)` / `(2,787,480)` | 49 / 50 |
| U4 | Kill XP worked example (level 28 vs level 30 elite, rested, 3 tracks) | 1,778 |
| U5 | `LD(−11)`, `LD(−6)`, `LD(0)`, `LD(7)` | 0; 0.65; 1.0; 1.2 |
| U6 | `G(5)` | 0.36 |
| U7 | `SF(0)`, `SF(50)`, `SF(620)` | 1; 1; 0.16129 |
| U8 | DF at d = −151, −150, 25, 250, 251 | 0; 0.216; 1.000; 0.080; 0 |
| U9 | Gain example A | P = 0.01727 (±1e-5) |
| U10 | Gain example B | P = 0.01846 (±1e-5) |
| U11 | DR at F = 90, 150, 240, 300 | 1; 0.70; 0.25; 0.25 |
| U12 | `AM_rep` crafting n = 60, 75, 400 | 1; 0.8; 0.2 |
| U13 | `AM_timing` with 200 identical intervals | 0.25; with CV 0.2 → 1 |
| U14 | `G_HARD(50)`, `(620)`, `(950)` | 540; 79; 29 |
| U15 | Rested accrual: 480 min offline normal; 480 in rest area; 5,000 offline | +120; +240; capped 600 |
| U16 | `M_bonus` with rested, div 1.25, event 1.5, cu 1.5 | capped at 3.0 |
| U17 | Attribute gain P for str 60 w .6; int 80 w .4 | 0.0465; 0.0273 |
| U18 | Training cost 0 → 300 | 60 LP, 30,000 copper |
| U19 | NPC worked example (L30 elite skirmisher) | dex 73, con 36, daggers 373, tactics 292 (no tier factor on attributes) |
| U20 | `lp_level` cumulative at 10/50/100 | 45; 345; 945 |
| U21 | Prestige example (395,000 + 33,383) | rank +1, remainder 27,783 |

### 4.2 Property-based tests

| ID | Property |
| --- | --- |
| P1 | For random sequences of practice records, locks, and lock changes: INV-2, INV-3, INV-7 hold after every step. |
| P2 | Donation: when a cap binds and a `down` donor exists, totals stay constant and the gained skill increases by exactly 1. |
| P3 | Random attribute gains/allocations: INV-4 holds; donation picks the highest `down` attribute. |
| P4 | `level_from_xp` is monotone non-decreasing; `xp_to_next` strictly increasing for L ≥ 1. |
| P5 | Gain chance P ∈ [0, P_MAX] for all inputs; P = 0 when DF = 0. |
| P6 | Rollback to time T then replay of the same inputs yields the same state (excluding RNG-driven gains, which are compared via ledger replay). |
| P7 | Ledger recomputation equals stored aggregate (INV-5, INV-13) after random mixed operation sequences including rollbacks. |

### 4.3 Integration tests

| ID | Scenario | Expected |
| --- | --- | --- |
| I1 | `InitializeCharacter` with 3 skills (100/50/0) and 40 attribute points | 35 skill rows, 8 attribute rows, total 120, stage `new_character` |
| I2 | Same `GrantXp` idempotency key twice | One ledger entry, identical responses |
| I3 | Same key, different amount | `ALREADY_EXISTS` |
| I4 | `GrantXp` crossing levels 9 → 12 | 3 `progression.level.reached` in order, LP 5+6+6 = 17, 6 attribute points |
| I5 | Practice batch at global cap 7200 with no `down` skill | No gain, `m_cap_blocked_gains` incremented, no event |
| I6 | Redis flushed mid-session | Windows restored from `practice_state` (≤ 60 s stale); DR not reset to 1 for a fatigued skill |
| I7 | `TrainSkill` when economy debit fails | No LP spent, no skill change |
| I8 | Select specialization at level 49 / skill 699 | `FAILED_PRECONDITION` (level) / (skill) |
| I9 | Remove specialization after cooldown with 2 legendary nodes | LP refund 50 + 80 = 130; effective value 900; events `specialization.removed`†, `mastery.tree_reset`† |
| I10 | Seasonal achievement unfinished at `liveops.season.ended` | State `expired`; no reward |
| I11 | Same domain event delivered twice to achievement consumer | Counter increments once |
| I12 | First character reaches L100 | `catch_up` unlock `level` granted once; alt gets ×1.5 XP below level 90 |
| I13 | `ApplyRollback` that leaves LP negative | Newest mastery nodes revoked until LP ≥ 0; stage recomputed; audit record written |
| I14 | Sub-batch exceeding duration budget | Rejected with `duration_budget_exceeded`; integrity signal sent |

### 4.4 Load tests

| ID | Profile | Target |
| --- | --- | --- |
| L1 | 10,000 concurrent characters, 0.5 practice records/s each, 2 s flush from 40 producers | p99 `SubmitPracticeBatch` < 150 ms; no lock waits > 50 ms p99 |
| L2 | Reward burst: 2,000 `GrantXp` per second (raid wipe-to-kill, quest turn-ins) | p99 < 80 ms; outbox lag < 2 s |
| L3 | Achievement consumer at 20,000 domain events/s | Consumer lag < 30 s; zero duplicates |

---

## 5. Acceptance criteria

1. Every formula's worked example in the Leveling and Skills parts is reproduced by U-tests and golden vectors.
2. All registered `progression.*` events validate against the payload tables in the Service Interfaces part, including `progression.stage.changed`, `progression.mastery.tree_reset`, and `progression.specialization.removed`.
3. INV-1 … INV-14 are checked by tests or by the nightly audit job (INV-13).
4. A simulated 180-hour typical-player bot (scripted reward and practice mix) reaches level 100 within the time-to-cap band and the skill band targets of the balancing tables.
5. A simulated 24/7 macro profile (single context, regular timing) is dampened to ≤ 30% of a human's gains per hour after the first 2 hours and produces anticheat signals.
6. Load targets L1–L3 met on the reference environment.
7. Tuning changes (`config.tuning.published`) take effect without restart and are recorded in ledger `content_version`/tuning version.

---

## 6. Pitfalls

- **Dividing by skill.** Combat's raw formula divides by zero at skill 0; always use `max(1, S/100)`.
- **Rolling on the simulation server.** Zone-servers report facts; gain rolls happen only in progression-service. Do not "optimize" by moving the roll.
- **Client time.** `action_seconds` comes from authored data; `input_at` is server receive time. Never accept client timestamps.
- **Multiplier creep.** Cap `M_bonus` (3.0), `M_activity` (3.0), and `M_reward` (2.5) before applying DR/AM; LiveOps must not stack past caps.
- **Event explosion.** Coalesce per skill per sub-batch; do not emit one event per gain check.
- **Lost fatigue state.** Persist Redis windows; resetting DR on reconnect is an exploit.
- **Applying rested/diversity to quest rewards.** Reward XP uses only `M_reward`.
- **Stage regressions.** Only rollback may move stage backward.
- **Node refund on skill drop.** Nodes deactivate when a band is lost; they are not refunded.
- **Tier multiplier double-scaling.** Never multiply NPC attributes by creature tier; tiers scale health/damage only per Dungeon spec scaling (Skills part, NPC parity, Creature tier rule).
- **Floating point drift.** Compute probabilities in float64; store only integers (XP, LP, values). Rounding is `round half away from zero` everywhere.

---

## 7. Do / Don't for coding agents

| Do | Don't |
| --- | --- |
| Use Registry names exactly (`skill/<slug>`, `pgl_`, `progression.skill.gained`). | Invent event names or enum values; propose them in the entry file's Registry additions instead. |
| Read tuning through `internal/tuning` with registry defaults and bounds. | Hard-code constants outside `internal/tuning`. |
| Write ledger + outbox + state in one transaction. | Publish events directly to the bus from handlers. |
| Make every mutating RPC idempotent with stored responses. | Rely on client retries being unique. |
| Keep `internal/formulas` pure and covered by golden vectors. | Read clocks, Redis, or config inside formulas. |
| Validate content packs at publish time (tree structure, power budget, weights). | Trust content at runtime without the validators. |
| Report anti-macro signals to anticheat-service. | Sanction, ban, or roll back from progression-service on your own. |
| Treat `p2_personal` fields as restricted in logs. | Log raw AM/DR evidence with account identifiers at info level. |
| Update `docs/dependency-map.md` when files, routes, or commands change in an implementation task. | Edit the Registry or other domains' specs from an implementation task. |
