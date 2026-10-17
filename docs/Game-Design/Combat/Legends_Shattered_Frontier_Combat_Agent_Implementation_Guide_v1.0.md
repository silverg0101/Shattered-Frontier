# Legends of the Shattered Frontier — Combat: Agent Implementation Guide
### Version 1.0 — Expanded Technical Design Authority edition (2026-10-17)

Status: Design target; not implemented. Names per [Shared Registry v1.0](../Legends_Shattered_Frontier_Shared_Registry_v1.0.md).
Entry file: [Combat spec](../Legends_Shattered_Frontier_Combat_System_v0.1.md). Part 4 of 4.

Audience: coding agents and engineers implementing combat in the UE5.8 dedicated server build (`zone-server` / `instance-server`), the client prediction layer, and the offline tool `lsf-combatmath`. Read the entry file and Parts 1–3 first; this part tells you in which order to build, what must always hold, and how to prove it.

The current Node.js proof of concept (`server/`, `public/`) does not implement this design (see `docs/dependency-map.md`).

---

## Repository and module layout

Proposal consistent with the UE5.8 Project Layout spec (`GameClient/Source/…`) and Repository Structure spec (`Tools/`). Create directories only when a milestone needs them.

```text
GameClient/Source/
  SFCombat/                          # runtime module, compiled into client and server targets
    Public/Formulas/                 # pure C++ (no UObject, no I/O, no clock, RNG injected)
      AttributeMath.h                #   Part 1 attributes, resources (attributes, health, mana, stamina, regen)
      RatingMath.h                   #   Part 1 ratings, haste, crit, avoidance, pen (K(L), haste, crit, avoidance, pen)
      ThroughputMath.h               #   Part 1 Reference throughput (σ(L), U_ref, DPS_ref)
      DamageMath.h                   #   Part 2 damage, mitigation (damage, mitigation, NPC damage)
      ThreatMath.h                   #   Part 2 Threat
      CcMath.h                       #   Part 3 CC/DR (DR, CC reduction)
    Public/Model/                    # ability/effect definitions (POD structs parsed from content packs)
    Private/Pipeline/                # per-hit resolution pipeline (server authoritative; client uses for predictions)
    Private/Effects/                 # effect instances, stacking, ticking, procs
    Private/Rng/                     # PCG32 streams, fixed five-draw consumption
  SFServer/Combat/                   # server-only adapters
    CombatEntityComponent            # per-ent_ runtime state (resources, effects, cooldowns, cast, DR)
    ThreatTableComponent             # NPC threat tables
    KillCreditTracker                # contribution counters, credit resolution
    CombatIntentHandler              # UseAbility/CancelCast, Security validators call-in
    ProfileLoader                    # GetCombatProfile + ComputeEquipmentStats, refresh on deltas
    PracticeReporter                 # PracticeRecord batching → SubmitPracticeBatch
    KillXpReporter                   # GrantKillXp per party chunk
    CombatOutbox                     # local journal + relay to domain.combat
    CombatTelemetry                  # damage summaries, profile samples, deaths, PvP kills
    HandoffSnapshot                  # combat_snapshot serialize/deserialize
Tools/lsf-combatmath/                # offline calculator, publish validators, simulation harness
  formulas/                          # same formulas, generated from or tested against golden vectors
  validators/                        # content and tuning publish checks (Publish-time validators)
  sim/                               # rotation, TTK, PvP duel and tank-band simulations
shared/combat-golden-vectors/        # JSON vectors consumed by SFCombat tests and lsf-combatmath
```

Rules:

1. `Public/Formulas` is the only place formulas live. Pipeline code calls them; it never inlines math.
2. Every function in `Formulas` takes tuning values as an explicit struct (`FCombatTuning`) snapshotted from `config-service`; no global reads.
3. Client code may call `Formulas` for predicted tooltips and floating numbers, never to decide outcomes.
4. `lsf-combatmath` and `SFCombat` must agree on every golden vector to 1e-9 relative error (doubles) and exactly on rounded integers.

---

## Build order and milestones

| Milestone | Scope | Exit evidence |
| --- | --- | --- |
| C0 Formulas | `Formulas/*` + golden vectors for every worked example in Parts 1–3 | U1–U20 green; vectors shared with `lsf-combatmath` |
| C1 Entity and profile | `CombatEntityComponent`, `ProfileLoader`, attribute evaluation order, derived stats, regen, `combat_state` machine, out-of-combat token | I1–I3 |
| C2 Pipeline | five-draw RNG, hit/avoid/crit/mitigation/block/absorb, `CombatResult`, auto-attacks, GCD, cooldowns, haste rules, casting/pushback/interrupt | U21–U30, P1–P4, I4 |
| C3 Effects | effect model, stacking rules, DoT/HoT, absorbs, procs (PPM, ICD), party/raid scope, CC + DR | U31–U40, P5–P6 |
| C4 NPC side | NPC profiles from Character templates, `HP_npc`, archetype autos, `dmg_coef` abilities, NPC mitigation, tier rule, CC immunities, threat tables, taunt, switching | U41–U48, I5 |
| C5 Credit and events | kill credit, death/resurrection, `combat.*` events, outbox journal + relay, `GrantKillXp`, practice reporting, wear counters | I6–I9, load L1 |
| C6 Handoff and config | `combat_snapshot`, tuning hot-apply, content version adoption, kill switch | I10–I12 |
| C7 Tooling gates | `lsf-combatmath` validators wired into content and tuning publish; balance simulations | V1–V9 |
| C8 Load and soak | 250 entities in combat per zone process at full tick, 24 h soak with outbox crash tests | L1–L4 |

Dependencies: C1 needs `progression-service.GetCombatProfile` (Character spec, milestone M5) and `inventory-service.ComputeEquipmentStats` (Itemization); stub both with fixtures until available. C5 needs Character `GrantKillXp` and `SubmitPracticeBatch` and the event-bus topics. Security validators (Security spec, S3) consume C2's cooldown, GCD and cast timers.

---

## Invariants checklist

| ID | Invariant | Enforced by | Verified by |
| --- | --- | --- | --- |
| CB-1 | Every combat outcome is computed on the server; clients never send damage, healing, crit, haste, cooldown or resource values | intent protocol (Part 3) | protocol fuzz test I4 |
| CB-2 | All caps (Part 1, Soft Caps) are applied after summing all sources | `Formulas` | property P2 |
| CB-3 | `h ≤ HASTE_MAX`, `gcd_eff ≥ GCD_MIN`, cooldown haste only when `haste_scaled_cooldown` and `cd ≤ 30 s` | `RatingMath`, publish validator | P3, V2 |
| CB-4 | Exactly five uniform draws per hit, in fixed order, from the attacker's stream | `Rng` | U22, replay test I11 |
| CB-5 | Tier multipliers touch only NPC health (`T`) and archetype damage (`√T`) | `DamageMath` | U43 |
| CB-6 | NPC attributes never change NPC health, damage or resistances | `DamageMath` | U44 |
| CB-7 | Character effects never grant hard immunity; damage-taken product ≥ 0.10 | effect validator, pipeline | V4, P4 |
| CB-8 | Overkill contributes to nothing (threat, credit, summaries) | pipeline | U27 |
| CB-9 | `credited_character_ids` is sorted, de-duplicated, and every listed character satisfies a credit clause | `KillCreditTracker` | P7 |
| CB-10 | Every `combat.npc.killed` is journaled before any `GrantKillXp` or `IssueGrant` call for that kill | `CombatOutbox` | I7 crash test |
| CB-11 | `GrantKillXp` idempotency key `<srvi_id>:<ent_id>:<character_id>` is stable across retries | `KillXpReporter` | I8 |
| CB-12 | `σ(L)` table in config equals the value recomputed from current inputs (± 0.1 %) | tuning publish validator | V1 |
| CB-13 | Effects persist across zones only with `persists_through_zone`; threat and credit never cross servers | `HandoffSnapshot` | I10 |
| CB-14 | Displacement always obtains a World teleport token before moving the entity | `Effects/displace` | I12 |
| CB-15 | `luck` never enters any combat roll | code review rule + grep check in CI (`luck` referenced only in `AttributeMath` luck_factor export) | V9 |
| CB-16 | Practice records are never produced against `xp_eligible = false`, gray or evading targets | `PracticeReporter` | U39 |

---

## Publish-time validators (`lsf-combatmath`)

| ID | Applies to | Rule | Failure |
| --- | --- | --- | --- |
| V1 | tuning publish touching any σ input | recompute `σ(L)` for L 1–100; compare with `tune.combat.throughput_scale` | block |
| V2 | `ability/*` | `haste_scaled_cooldown` false when `cooldown_s > 30`; `gcd_class: none` needs `t_ref_s`; `range_m` > 0 unless `self` | block |
| V3 | `ability/*` | `modifier` inside Part 3 Throughput budget bounds | block |
| V4 | `effect/*` | `immunity` op only on NPC/encounter effects; `damage_taken_mod` values ≥ −0.5 per effect | block |
| V5 | kits (abilities grouped by `skill_id` and role tag) | sustained MPS within ±5 % of role target; warn 5–10 %, block > 10 % | warn/block |
| V6 | `npc/*` combat block | tier in registry set; `armor_class`/`resist_class` valid; `npc_dps_coef` within ±50 % of archetype default unless `boss` | block |
| V7 | item procs (`proc_<effect>`) | expected MPS ≤ 0.08 per item | block |
| V8 | mastery trees (Character) | converted node effects within Character's power budget | block (Character owns the budget; Combat supplies the conversion) |
| V9 | source tree (CI) | no reference to `luck` in pipeline code | block |

---

## Test matrix

### Unit tests (golden vectors)

| ID | Case | Expected |
| --- | --- | --- |
| U1 | `Health(con 150, L100)` | 4,850 |
| U2 | `Mana(int 150, L100)` | 4,100 |
| U3 | `Stamina(con 150, dex 43)` | 979 |
| U4 | `luck_factor(90)`, `(150)`, `(210)` | 0.06, 0.10, 0.10 |
| U5 | `combat_move_mult(dex 130, 300 bp)` | 1.07 |
| U6 | `combat_move_mult(dex 200, 1,400 bp)` | 1.15 |
| U7 | `K_crit(50)` | 722.58 |
| U8 | crit at L100, 757 crit rating, dex 10 | min(0.60, 0.05 + 0.002 + 0.351) = 0.403 |
| U9 | level invariance: 560/(560+1,400) vs 289/(289+722.6) | both 0.2857 ± 0.0005 |
| U10 | haste 0.30: cd 12 s → 8.4 s; GCD 1.05 s; cast 2.5 → 1.75 s | exact |
| U11 | haste 0.60: GCD | 0.75 s |
| U12 | 45 s defensive at haste 0.60 | 45 s |
| U13 | crit damage with 100 rating at L100 | 2.4065 |
| U14 | tank avoidance (500 dodge, 600 parry, L100) scaled | 0.234 / 0.266 |
| U15 | block 700 rating + shield skill at ref | 0.60; reduction with str 250 = 0.36 |
| U16 | armor pen 350 at L100 vs `d_ref` | ×1.20 |
| U17 | `skill_mult` sword 50 at L10 | 0.879 |
| U18 | miss vs elite L31, ref player L30 | 0.018 |
| U19 | `σ(1)`, `σ(30)`, `σ(100)` | 0.4047, 0.3246, 0.3098 (± 0.0005) |
| U20 | `DPS_ref(100, 460 epic)` | 304.0 ± 0.5 |
| U21 | auto-attack example (Part 2, auto-attack worked example) | 306 |
| U22 | draws consumed for a spell (cannot be dodged) | 5 |
| U23 | Sundering Blow crit example | 324 |
| U24 | frost tank hit through resist example | 1,684 |
| U25 | AoE 8 targets factor | 0.7906 |
| U26 | armor 5,000 vs L50 | 0.50 |
| U27 | overkill: 500 damage on 120 HP NPC | threat 120, credit 120 |
| U28 | crit chance cap with 2,000 crit rating + effects 0.3 | 0.60 |
| U29 | resistance fire example | 0.268 |
| U30 | damage-taken product of five −30 % effects | 0.168 (above the floor); six −40 % → 0.10 |
| U31 | stack rule `stack` max 3, 4th application | 3 stacks, duration refreshed |
| U32 | `replace_if_stronger` weaker re-application | ignored |
| U33 | DoT refresh with 4 s left of 12 s | 15.6 s (capped at 1.3 × 12) |
| U34 | PPM 2 on 3.4 s swing | 0.1133 |
| U35 | PPM proc ICD 10 s, two triggers 3 s apart | second suppressed |
| U36 | stun DR sequence with 15 % reduction (Part 3 example) | 3.4 / 1.7 / 0.85 / 0 s |
| U37 | DR reset after 18.0 s | full duration |
| U38 | fear breaks at 10 % max health damage | removed on the hit that crosses |
| U39 | practice record vs `xp_eligible=false` | none |
| U40 | party scope inside raid subgroup | only 5 subgroup members |
| U41 | `HP_npc` elite L31 open world | 4,449 |
| U42 | elite brute swing | 169 |
| U43 | world_boss archetype damage factor | 4.472 |
| U44 | NPC with `wis` 300 vs `medium` resist coefficient | resistance unchanged by `wis` |
| U45 | threat example (Part 2, threat worked example) | 2,100; 200 per NPC; switch at 1.333 |
| U46 | taunt sets 2,800 → 3,080 and forces 3 s | exact |
| U47 | taunt DR on same NPC | 3 / 1.5 / 0.75 / immune |
| U48 | healing threat split with 0 NPCs engaged | no threat |

### Property-based tests

| ID | Property |
| --- | --- |
| P1 | For any rating ≥ 0 and L in 1..100: `0 ≤ R/(R+K(L)) < 1` and monotonic in R |
| P2 | For random source combinations, every capped stat ≤ its cap; removing a source never increases the stat |
| P3 | For any `h` in [0, 1]: `gcd_eff ∈ [GCD_MIN, GCD_BASE]`; `cooldown_eff ≥ (1 − HASTE_MAX) × cd` |
| P4 | For any effect set on a character: damage taken > 0 for raw damage ≥ 10 after mitigation |
| P5 | Effect stacking is order-independent for `refresh`, `stack`, `replace_if_stronger` given equal timestamps |
| P6 | DR never yields a duration above base, and the 4th application within the window is 0 |
| P7 | Kill credit: every listed ID satisfies a clause; no ID appears twice; list sorted |
| P8 | Replaying an attempt input log with the same seeds yields identical `hit_outcome` sequences |

### Integration tests

| ID | Scenario | Pass condition |
| --- | --- | --- |
| I1 | Zone entry with fixtures for `GetCombatProfile` / `ComputeEquipmentStats` | entity built; abilities usable only after both loads |
| I2 | `stats_version` push mid-session (equip ring with +100 crit) | crit recomputed next tick; no in-flight cast changed |
| I3 | `EquipItem` with out-of-combat token while `in_combat` | inventory rejects (token missing/expired) |
| I4 | Modified client sends `UseAbility` every 50 ms and forged damage fields | rejects with `combat_cooldown`; extra fields ignored; `security.action.rejected` emitted |
| I5 | 5-player pull of a mixed pack (Dungeon worked example 4) | threat switching and taunt per spec; elites immune to charm |
| I6 | Open-world elite kill by 2 players + 1 healer partner at 50 m | 3 credited (healer by party radius ≤ 60 m); `GrantKillXp` one call |
| I7 | Kill the zone-server process between journal append and relay | after restart, event delivered once per `event_id` downstream (consumer dedupe) and XP granted once (idempotency) |
| I8 | `GrantKillXp` timeout and retry | same key; progression returns replay; no double XP |
| I9 | `combat.damage.summarized` across a 150 s fight with a death at 130 s | windows 60 s, 60 s, 10 s (death) |
| I10 | Handoff mid-combat with a DoT (`persists_through_zone`) and a cooldown | DoT and cooldown continue on target server; threat cleared |
| I11 | Same attempt replayed headless | identical decisions and `hit_outcome` sequence |
| I12 | Charge ability across a gap | teleport token issued before movement; no `movement_teleport` reject |

### Load and soak

| ID | Scenario | Target |
| --- | --- | --- |
| L1 | 250 entities `in_combat`, 40 casting AoE, at full simulation tick | p99 tick time within World's tick budget; ≤ 5 µs per hit |
| L2 | 20-player raid boss, 25 adds, 400 effect instances | no tick overrun > 2 consecutive ticks |
| L3 | Outbox: 2,000 kills/min for 1 h with bus outage of 10 min | zero loss; backlog drained < 5 min after recovery |
| L4 | 24 h soak with random tuning publishes every 15 min | no crash, no stale σ, memory flat |

---

## Acceptance criteria

1. All U, P and I tests above pass in CI on the server target; golden vectors match `lsf-combatmath`.
2. Validators V1–V9 run in the content and tuning publish pipeline and block on failure.
3. Simulation report (Part 2, Balance checks by simulation) passes for the launch content pack: common-fight band, tank band, PvP TTK band, reference throughput.
4. Analytics receives `combat.*` events with the payloads in the entry file; `m_ttk_vs_ref_ratio` for launch dungeons is within 0.8–1.25 on PTR.
5. Security's combat validators use Combat's `gcd_eff` and `haste_scaled_cooldown` (entry file, Open contracts).

---

## Pitfalls

| Pitfall | Consequence | Avoid by |
| --- | --- | --- |
| Applying `(1 − h)` to every cooldown | 3-minute defensives drop to 45 s at max haste | honor `haste_scaled_cooldown` |
| Using `gcd × (1 − h)` without the floor | GCD 0.375 s at max haste; validators accept bot spam | `gcd_eff` |
| Rolling crit before avoidance, or skipping unused draws | replays diverge; crit rates drift with avoidance | fixed five-draw order |
| Feeding NPC Constitution into health | NPC health swings ±5 % with spawn variance and double-scales with tiers | `HP_npc` from `H_ref` |
| Multiplying tiers into armor/ratings | elites become unhittable | tier rule |
| Adding percent modifiers multiplicatively within a category | stacking explosions | categories (Part 2) |
| Counting overkill toward credit | kill stealing with one big hit | overkill = 0 |
| Emitting `combat.npc.killed` after XP RPCs | crash loses quest progress | journal first (CB-10) |
| Reading `luck` in combat | violates design pillar | CB-15 |
| Using float time or wall clock in decisions | non-reproducible replays | integer server ms, decision tick |
| Forgetting σ on heals or absorbs | healing 3× too strong | σ applies to all character-sourced amounts |
| Re-implementing formulas in Blueprint | drift between client prediction and server | call `SFCombat` from Blueprint wrappers only |

---

## Do / Don't for coding agents

| Do | Don't |
| --- | --- |
| Put every formula in `Public/Formulas` with a golden vector | Inline math in pipeline, UI or Blueprint |
| Take tuning as an explicit struct snapshot | Read `config-service` values mid-calculation |
| Use registry names exactly (`npc_tier`, `reject_reason`, event names) | Invent event names, enum values or prefixes; propose them in the entry file's Registry additions instead |
| Use registered enums (`hit_outcome`, `combat_state`, …) according to the Shared Registry | Introduce cross-service names without checking the Registry |
| Journal `combat.npc.killed` before side effects | Call other services' databases; use their RPCs |
| Snapshot effect magnitudes at application | Recompute running DoTs when tuning changes |
| Sort iteration by entity ID in decision code | Iterate hash maps in anything that affects outcomes |
| Keep NPC damage on `H_ref × dmg_coef` | Derive NPC damage from NPC Strength or PhysicalPower |
| Update `docs/dependency-map.md` when module roles or commands change | Change Security, Dungeon or Raid keys from Combat code; raise cross-spec requests |
