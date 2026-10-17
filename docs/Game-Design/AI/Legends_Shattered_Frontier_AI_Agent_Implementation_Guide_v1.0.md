# Legends of the Shattered Frontier — AI: Agent Implementation Guide

Version 1.0 — Expanded Technical Design Authority edition (2026-10-17)

Status: Design target; not implemented. Names per Shared Registry v1.0.

Back-links: [AI entry](../Legends_Shattered_Frontier_AI_Architecture_v1.0.md) · [Shared Registry](../Legends_Shattered_Frontier_Shared_Registry_v1.0.md)

---

This guide tells coding agents how to build the AI domain: the zone-server `AIRuntime`, `ai-director-service` and `faction-service`. It assumes the repository layout proposals in the Repository Structure and UE5.8 Project Layout documents; paths below are proposals and become stable only when those documents' environment milestones create them.

## 1. Repository and module layout

```text
GameClient/Source/SFServer/AI/                 zone-server / instance-server runtime (server target only)
  Runtime/        AIRuntime subsystem, decision scheduler, LOD tiers, budget governor
  Brain/          blackboard, awareness state machine, working memory
  BehaviorTree/   node set (selector, sequence, parallel, utility_select, decorators), behavior_tree/ JSON loader
  Utility/        response curves, consideration inputs, action and target scoring, utility_profile/ loader
  Perception/     sight (batched LoS), hearing, stealth meter
  Social/         pack coordinator, assist, call for help, guards and offenders, civilians
  Schedules/      schedule/ loader, world-clock binding, virtual movement
  Navigation/     path request queue, stuck recovery
  DirectorLink/   ZoneLink client (RegionSample, EventProgress, memory events; directives, event commands)
  Events/         world-event stage executor (objective types, spawn sets, scaling, contribution counters)
  Tests/          automation specs (headless server)
Content/Frontier/Data/AI/                      behavior_tree/, utility_profile/, schedule/ assets (data only; no scripts)
BackendServices/ai-director/                   ai-director-service
  api/            lsf.ai_director_service.v1 protos
  pacing/         intensity, phases, budgets, spawn pressure
  selection/      eligibility, scoring, sampling, forced starts
  events/         lifecycle, participants, contribution, rewards worker
  factionai/      strategic tick, actions, NPC-attributed influence
  quests/         generation triggers, GenerationRequest builder, backoff
  memory/         npc_memories, region_history, narrative flags
  integrations/   quest, faction, reward, world, config, catalog clients
  store/          migrations for schema `director`, outbox relay
BackendServices/faction/                       faction-service
  api/            lsf.faction_service.v1 protos
  standing/       AdjustStanding, tiers, charisma, ledger, reversals
  control/        AddControlInfluence, caps, decay, control-change rule, daily tick
  consumers/      raid.world_effect.applied, progression.attribute.changed
  privacy/        ExportSubject, EraseSubject
  store/          migrations for schema `faction`, outbox relay
Tools/ai-validators/                           content validators (schedules vs giver bindings, essential flags, utility profiles)
```

Rules: the zone-server module never links backend service code; it talks to the director only through `ZoneLink`. `faction-service` and `ai-director-service` are separate deployables even when co-located, because they have different statefulness (Registry service registry).

## 2. Build order and milestones

| Milestone | Scope | Exit evidence |
| --- | --- | --- |
| A1 Faction core | `faction` schema, `GetStanding`, `AdjustStanding` (tiers, charisma, clamping, ledger, idempotency, reversal), outbox, `faction.reputation.changed` | Contract tests from Quest and Itemization callers pass; property tests P1–P4 |
| A2 Regional control | `AddControlInfluence` (caps order, war_override), `GetControl`, daily decay and control-change tick, raid consumer, `faction.control.changed` | F3.3 worked example reproduced exactly as a test; P5–P7 |
| A3 NPC runtime baseline | AIRuntime scheduler, BT node set, blackboard, awareness state machine, threat-based targeting (Combat), aggro radius, sight, leash/evade | Headless server test: pull, chase, evade, reset; L0 budget measured |
| A4 Utility and roles | Curves, action and target scoring, archetype trees and profiles, pack slots, fairness caps | Golden decision logs for the worked examples (NPC Behavior part) |
| A5 Perception and social | Hearing, stealth meter, assist, call for help, guards/offenders, civilians, morale and flee | Stealth timing fixture (4.5 s), morale examples, assist adds metric in soak |
| A6 Schedules, LOD, governor | schedule assets, virtual movement, LOD tiers, budget governor, stuck recovery, schedule validator | 250-player soak: AI ≤ 6 ms p95; governor never degrades L0 |
| A7 Director core | Leader lease, region state snapshot, ZoneLink, RegionSample ingestion, intensity/phases, budget, spawn directives | Replay test: same samples + seed → same directives |
| A8 World events | Definitions, lifecycle, stage executor on zone-server, scaling, contribution, `world.event.*`, reward worker | Caravan-attack end-to-end with fake reward-service; payload schema tests |
| A9 Integrations | LiveOps RPCs (request, bias, cancel), SubmitQuestOutcome, raid effect consumption, Faction AI tick, ProposeGeneratedQuests/InvalidateSource | LiveOps and Quest contract tests; §4.3 and §6.1 worked examples as tests |
| A10 Memory and privacy | Long-term NPC memory, region history, narrative flags, Export/EraseSubject in both services | Privacy integration test with Backend's orchestrator fake |
| A11 Hardening | Security signals, admin RPCs and audit, load tests, chaos (leader failover, zone loss) | Load and chaos matrix green; dashboards fed |

A1–A2 unblock Quest, Itemization and Raid and can ship before any NPC work. A3–A6 are independent of the backend and can proceed in parallel with A1–A2.

## 3. Invariants checklist

Faction service:

- [ ] **I1** `standings.value` is always within `[REPUTATION_MIN, REPUTATION_MAX]` and `tier = tier(value)`.
- [ ] **I2** For every (character, faction): `value = base_standing + Σ applied_delta` over the ledger (ledger replay equals current state).
- [ ] **I3** Every committed mutating call writes exactly one ledger or contribution row, one idempotency row (until purge), and its outbox rows in the same transaction.
- [ ] **I4** At most one reversal per original key; a reversal applies exactly `−applied` of the original, never re-multiplied.
- [ ] **I5** Influence per (realm, region, faction) is within `[0, INFLUENCE_MAX]`; weekly character and region caps are never exceeded by positive non-GM sources.
- [ ] **I6** A region changes controller at most once per daily tick and only at the tick.
- [ ] **I7** No gateway route can mutate faction state; `character_id` for player reads comes from the session.

Director:

- [ ] **I8** Every `wev_` instance follows `pending → active → resolved | failed` (or `pending → failed`); terminal states never change.
- [ ] **I9** Concurrency limits (per region, per realm epic) and cooldowns hold at every instant, including forced starts.
- [ ] **I10** Budget spent in any rolling 60 minutes ≤ `B_r` except for forced starts, which are logged with `forced = true`.
- [ ] **I11** Each eligible participant receives at most one reward grant per event (`<wev_id>:<character_id>` key).
- [ ] **I12** `world.event.ended.outcome` equals the stored terminal state; `participants[].eligible` matches the stored contribution and `C_MIN` at end time.
- [ ] **I13** Spawn multipliers sent are within `[M_MIN, M_MAX]`; zone-server reverts to 1.0 after 120 s without directives.
- [ ] **I14** No director output contains free text; generation requests contain ids and numbers only.
- [ ] **I15** A replayed tick (same region state, samples, seed) yields the same decision.

NPC runtime:

- [ ] **I16** Evading NPCs are immune and non-targetable; an NPC never flees more than once per engagement; flee paths stay inside the leash.
- [ ] **I17** Essential NPCs can never take damage or be targeted by hostile NPCs or characters.
- [ ] **I18** Fairness caps (3 consecutive utility targets, execute 3 s rule) are never exceeded.
- [ ] **I19** L0 NPCs are never degraded by the budget governor.
- [ ] **I20** Long-term memory never changes rewards, prices, quest availability or hostility.

## 4. Test matrix

### 4.1 Unit

| ID | Area | Case |
| --- | --- | --- |
| U1 | tiers | Boundaries: −21,001 → `hated`, −21,000 → `hostile`, −1 → `unfriendly`, 0 → `neutral`, 2,999/3,000, 41,999/42,000 |
| U2 | charisma | delta 250, cha 90 → 265; cha 200 → capped at reference → 275; negative delta never multiplied |
| U3 | clamp | value 44,900 + 250 → 45,000, `clamped = true`, `applied_delta = 100` |
| U4 | influence caps | per-call 30 → `INVALID_ARGUMENT`; character weekly 18 used + 5 → applied 2, `cap_reason = character_weekly` |
| U5 | decay | B = 300, I = 420 → 414.00 after one tick; I = 200 → 205.00 |
| U6 | control rule | F3.3 worked example day by day (days 8, 9, 11, 16) |
| U7 | aggro radius | common L20 vs L23 → 9 m; vs L14 → 18 m; floor 4 m at +10 levels |
| U8 | stealth | §3.6 example: 4.5 s detection; 9.7 s standing still; instant at 1.5 m |
| U9 | curves | Each curve kind at x = 0, 0.5, 1 with documented parameters; clamp to [0, 1] |
| U10 | action scoring | §4.4 caster example: kite_back 1.093 beats cast_primary 0.795; veto on zero consideration |
| U11 | target scoring | Feature values for a 5-player fixture; fairness cap forces a different target on the 4th use |
| U12 | morale | Both §7 examples (0 → flee; 0.65 → fight) |
| U13 | intensity | α = 0.0488 at 30 s; X = 1 for 10 min from 0 → I ≈ 0.63 |
| U14 | budget | P = 30 → B = 7; P = 300 → 20 (max); P = 0 → 4 (min) |
| U15 | spawn pressure | §3.4 example 1.6; clamps at 0.5 and 2.0 |
| U16 | selection | §4.3 example probabilities 0.539 / 0.236 / 0.225 |
| U17 | quest bias | §6.1 example triggers on day 5 at θ = 80 |
| U18 | contribution | §7.4 examples (0.96 gold, 0.40 bronze); median zero → a = 0 |
| U19 | reward weight | major gold tier → w 1.0; failed consolation 0.25; 13th event today → ×0.25 |
| U20 | memory decay | −40 after 14 days + 5 → −15 |

### 4.2 Property-based

| ID | Property |
| --- | --- |
| P1 | Any sequence of `AdjustStanding` calls (random deltas, sources, replays) keeps I1 and I2 |
| P2 | Replaying any call returns a byte-identical response with `replayed = true` |
| P3 | Reversal of any call restores the pre-call value unless clamping occurred in between, in which case the result equals the clamp of the exact arithmetic |
| P4 | Same key with a different request always fails with `IDEMPOTENCY_MISMATCH` |
| P5 | Random influence streams never break I5; weekly usage equals the sum of contributions in that week |
| P6 | Control evaluation is order-independent for calls within one day (same end state at the tick) |
| P7 | Tie-breaking by lowest `faction_id` makes control evaluation deterministic |
| P8 | Random region samples and seeds: director never violates I9 and I10 |
| P9 | Utility scores are in `[0, w_a × (1 + μ) + 0.001)`; a vetoed action is never chosen |
| P10 | Contribution `c ∈ [0, 1]` and is monotonic in presence and in action points |

### 4.3 Integration

| ID | Scenario | Expected |
| --- | --- | --- |
| IT1 | Quest turn-in saga calls `AdjustStanding` twice with the same key (retry) | One ledger row; one `faction.reputation.changed` |
| IT2 | reward-service revocation of a grant with a reputation line | Exact reversal; event with `reversal_of` |
| IT3 | `raid.world_effect.applied` delivered twice, then its reversal | One contribution; reversal subtracts exactly once |
| IT4 | LiveOps `RequestWorldEvent` (harvest raiders, min 2, max 6), one day simulated | 2–6 instances; forced start if behind; `causation_id` = request id on `world.event.started` |
| IT5 | `CancelWorldEventRequests(end_active = true)` during an active event | Event ends `failed`/`cancelled`; consolation grants |
| IT6 | Caravan attack end-to-end on a headless zone-server with 6 bot clients | stage_changed then ended `resolved`; eligible participants get one grant each |
| IT7 | Zone-server killed mid-event; replacement within 120 s | Stage re-issued with progress; no duplicate grants |
| IT8 | Same, replacement after 120 s | Ends `failed` with `server_lost`; consolation weight 0.5 |
| IT9 | Director leader failover during an active event | New leader resumes; no lost or duplicate events or grants |
| IT10 | `ProposeGeneratedQuests` returns `RESOURCE_EXHAUSTED` | Backoff 30 s, 60 s, 120 s; no queueing |
| IT11 | Event ends with generated quests sourced from it | Quest-service expires them (observed via `quest.instance.expired`) |
| IT12 | `resolve_series` raid effect while two series events are active | Both end `resolved` with `resolved_by_effect`; series no longer eligible |
| IT13 | Privacy erase of a character with standings, participants and memories | Rows deleted/pseudonymized per F4.1 and director rules; totals unchanged |
| IT14 | Quest-giver with `when_unavailable = flee` during an invasion | NPC moves to `safe_spot`; giver state `unavailable`; returns after calm |

### 4.4 Load and soak

| ID | Load | Target |
| --- | --- | --- |
| L1 | Zone with 250 bot players, 4,000 NPC brains, 600 at L0/L1 | AI frame ≤ 6 ms p95; total frame ≤ 33.3 ms p95 |
| L2 | 50 simultaneous pulls with packs of 5 and assist chains | No frame spike > 10 ms in AI; LoS queue drains within 3 frames |
| L3 | faction-service: 2,000 `AdjustStanding`/s, 500 `AddControlInfluence`/s per realm | p99 ≤ 50 ms; no version-conflict exhaustion |
| L4 | Daily tick with 120 contestable regions × 12 factions | Tick ≤ 5 s |
| L5 | Director: 120 regions, 40 zone-servers, 8 concurrent events | Region tick ≤ 50 ms p95; realm tick ≤ 10 s |
| L6 | Reward fan-out: world boss with 250 eligible participants | All grants delivered ≤ 60 s |

## 5. Acceptance criteria

1. All invariants I1–I20 have automated checks (unit, property or integration) that run in CI.
2. Every worked example in the AI parts is reproduced by a test with the documented numbers.
3. `faction.*` and `world.event.*` payloads validate against the schemas in this spec, with the documented `privacy_class` per field, in Analytics' schema registry.
4. Contract tests with Quest (`AdjustStanding`, `AddControlInfluence`, `GetStanding`, `ProposeGeneratedQuests`, `InvalidateSource`, `SubmitQuestOutcome`), Itemization (reward reputation lines and revocation), Raid (world effects), and LiveOps (request, bias, cancel) pass against fakes generated from the protos.
5. L1 and L5 load targets are met on the reference hardware named by the First MVP milestone.
6. Kill switches `ff.director.world_events_enabled`, `ff.director.quest_requests_enabled` and `ff.director.model_planner_enabled` take effect within one tick.

## 6. Do / Don't for coding agents

| Do | Don't |
| --- | --- |
| Use registry names exactly (`world_event_state`, `faction_standing_tier`, event names, ID prefixes) | Invent enum values or event names; propose them in the entry file's Registry additions |
| Put every tunable number behind its `tune.ai.*`, `tune.director.*` or `tune.faction.*` key | Hard-code constants that this spec gives a key |
| Make every mutating RPC idempotent and transactional with its outbox row | Publish events outside the commit transaction |
| Keep behavior trees and utility profiles as data assets | Embed per-NPC script code or logic in content |
| Route rewards through reward-service with `<wev_id>:<character_id>` keys | Credit currency or items directly |
| Test with the worked examples as golden values | Change a formula without updating its worked example and test |
| Report `leash_abuse` and participation anomalies to anticheat-service as signals | Raise detections or sanctions from AI services |
| Read faction control through `GetControl` or the `faction.control.changed` projection | Read the `faction` schema from the director |
| Degrade far NPCs under load | Degrade engaged NPCs or skip leash checks under load |
| Keep run-time model use behind its flag and the typed-action validator | Let any model output reach a service call unvalidated |
