# Legends of the Shattered Frontier — Combat Part 5: Energy, Starter Abilities, Taunt and Interrupt
### Version 0.1 — design draft (2026-10-25), from [owner decisions 2026-10-25](../../decisions/OWNER-2026-10-25-decisions.md)

Scope: the energy rule, the level 1–20 starter kits, the published taunt and interrupt abilities with their enhanced ladder, and sprint drain. Formulas, the ability JSON model, GCD, casting, threat and DR stay in Parts 1–3; this part only authors values into them. Everything here is design authority for content packs; nothing is implemented by this document.

## 1. Energy model

Two pools, one rule. **Stamina** (body) and **Mana** (mind) are both *energy*; every character has both at every level (Part 1 formulas, no class gate). The rule:

| Rule | Value |
| --- | --- |
| Every ability costs energy | exactly one of `cost.stamina_pct` / `cost.mana_pct` is > 0; publish-time validation rejects zero-cost and dual-cost abilities (toggles pay on activation) |
| Which pool | governing skill decides: weapon skills, `shield_defense`, `tactics`, `archery`, `unarmed`, `stealth` → Stamina; `magic`, `healing` → Mana |
| Costs are fractions of max pool | unchanged from Part 3; a bigger pool buys more casts, not cheaper casts |
| Hybrids | any character may learn any kit; a warrior casting `Mend` pays Mana from the pool Part 1 already gives him (int 10 → 300 + 10 × L) at the `skill/healing` floor (`skill_mult` 0.80 at skill 0) and improves by use. The pool and skill, not a class, are the hybrid tax |
| Sprint | continuous Stamina cost, §5 |
| Presentation | HUD shows two energy bars, Stamina and Mana, both labelled energy in tooltips; no third pool |

One pool was rejected: it would make hybrid healing free for melee builds and would change Part 1/Part 3 formulas and the ability JSON the casting pilot already uses.

### 1.1 Pool and regeneration keys (existing Part 1, restated with bounds)

| Key | Type / unit | Default | Bounds | Step | Scope / apply |
| --- | --- | --- | --- | --- | --- |
| `tune.combat.regen_delay_s` | float s | 5 | 0–15 | 0.5 | realm / `immediate` |
| `tune.combat.mana_regen_combat_base` | fraction of max /s | 0.003 | 0–0.02 | 0.0005 | realm / `immediate` |
| `tune.combat.mana_regen_wis_coef` | fraction /s per Wisdom | 0.00002 | 0–0.0001 | 0.000005 | realm / `immediate` |
| `tune.combat.mana_regen_ooc_pct` | fraction of max /s | 0.05 | 0.01–0.25 | 0.01 | realm / `immediate` |
| `tune.combat.stamina_regen_combat` | points /s (+ dex/50) | 10 | 0–40 | 1 | realm / `immediate` |
| `tune.combat.stamina_regen_ooc` | points /s (+ dex/25) | 25 | 5–100 | 1 | realm / `immediate` |
| `tune.combat.energy_cost_mult` (new) | multiplier on every `cost.*_pct` | 1.00 | 0.50–2.00 | 0.05 | realm / `immediate`; plain label "Ability energy cost" |

Pool formulas (`Health`, `Mana`, `Stamina`) are release-bound. Reference starters at creation (10 base + 40 creation points):

| Starter | Attributes | Health L1 / L10 / L20 | Mana L1 / L10 / L20 | Stamina (no level term) | Mana regen in combat | Stamina regen in combat |
| --- | --- | --- | --- | --- | --- | --- |
| Fighter | str 30, con 25, dex 15 | 500 / 725 / 975 | 310 / 400 / 500 | 270 | 1.0 /s (0.3 %) | 10.3 /s (3.8 %) |
| Archer | dex 40, con 20 | 425 / 650 / 900 | 310 / 400 / 500 | 320 | 1.0 /s | 10.8 /s (3.4 %) |
| Mage | int 40, wis 20 | 275 / 500 / 750 | 910 / 1,000 / 1,100 | 180 | 3.1 /s (0.34 %) | 10.2 /s |
| Healer | wis 40, int 20 | 275 / 500 / 750 | 510 / 600 / 700 | 180 | 2.0 /s (0.38 %) | 10.2 /s |

Reading: a Mage chain-casting the 5 % filler every 2 s spends 2.5 %/s against 0.34 %/s regen → about 45 s of continuous casting from full, refilled out of combat in 20 s. A Fighter's 5 % filler every GCD (3.3 %/s) is covered by in-combat Stamina regen until pools grow (L100 pool 979 → 1.3 %/s); Stamina pressure comes from cooldown abilities and grows with level. This follows Part 1's absolute Stamina regen; see Owner check 2.

## 2. Authoring bounds for starter content

Per-ability values are content (release-bound, validated at publish, Part 3 Throughput budget). Bounds for every ability in this part:

| Field | Bounds | Notes |
| --- | --- | --- |
| `cost.*_pct` | 0.03–0.25 | filler 0.05, cooldown 0.06–0.10, major 0.10–0.20 |
| `timing.cooldown_s` | 0–120 | `haste_scaled_cooldown` false above 30 s (Part 3) |
| `timing.cast_s` | 0–3.0 (Revive 10) | pre-haste |
| `targeting.range_m` | melee `tune.combat.melee_reach_m`; ranged ≤ 30 | `melee_reach_m` (new) 3.0 m, bounds 2.0–5.0, step 0.1, realm / `restart`; the pilot's 2.8 m is a development value until adopted |
| `power.modifier` | Part 3 authoring bounds | filler 0.6–1.0, cooldown ≤ 1 + cd/6, AoE per target ≤ 0.6 × single, DoT ≤ 1.2 × instant |
| Unlock | `requirements.level_min` (new field) 1–20 here; plus `skill_min` 0 for all starter abilities | taunt and interrupt fixed at 10 by owner decision |

Melee abilities need facing ≤ 90°, LOS and `melee_reach_m`; ranged and spells need LOS and the listed range. GCD is `standard` 1.5 s unless listed; off-GCD abilities carry `t_ref_s` 0.5. Costs and cooldowns start at ability start; interrupted casts refund 100 %, self-cancel 50 % (built in `SFCastingComponent`).

## 3. Starter kits (level 1–20)

Six kits, 38 abilities. Column E = energy pool (S Stamina, M Mana), cost in % of max pool, CD in seconds (h = haste scaled), cast in seconds (0 = instant), Rng in metres, Tgt = `targeting.mode`.

### 3.1 Weapon kit — governing skill: equipped weapon's skill (swords, axes, maces, polearms, daggers, unarmed)

| L | Ability (`ability/`) | E | Cost | CD | Cast | GCD | Rng | Tgt | Effect |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | `strike` | S | 5 | 0 | 0 | std | melee | enemy | physical, mod 0.7 |
| 4 | `heavy_blow` | S | 8 | 8 h | 0 | std | melee | enemy | physical, mod 1.5 |
| 7 | `cleave` | S | 7 | 0 | 0 | std | melee | cone 90°, ≤ 3 | physical, mod 0.42 per target |
| 10 | `pommel_strike` | S | 5 | 15 | 0 | none | melee | enemy, casting | interrupt, §4.2; physical, mod 0.3 |
| 14 | `second_wind` | S | 10 | 60 | 0 | short | self | self | `heal_over_time` 15 % max health over 10 s (5 ticks), physical school; usable in combat |
| 18 | `rend` | S | 8 | 12 h | 0 | std | melee | enemy | `damage_over_time` physical, total mod 1.8 over 12 s (6 ticks), `refresh`; not blockable |
| 20 | `charge` | S | 8 | 20 | 0 | none | 8–20 | enemy | `displace` charge to target, then physical mod 0.5; out of range < 8 m rejected |

### 3.2 Guardian kit — governing skill `shield_defense` (`tactics` for `challenge`)

| L | Ability | E | Cost | CD | Cast | GCD | Rng | Tgt | Effect |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | `defensive_stance` | S | 5 | 1 | 0 | short | self | self | toggle aura: `threat_mult` ×2.0, `damage_done_mod` all −10 %, `damage_taken_mod` all −10 %; exclusive with other stances; costs 5 % on entry, no upkeep |
| 4 | `shield_bash` | S | 6 | 8 h | 0 | std | melee | enemy | physical, mod 1.0, `threat.mult` 1.5; `requires_shield` |
| 7 | `shield_block` | S | 5 | 20 | 0 | short | self | self | `stat_mod` block chance +0.30 for 6 s (inside the 0.75 cap); `requires_shield` |
| 10 | `challenge` | S | 5 | 8 | 0 | none | 10 | enemy | taunt, §4.1 |
| 14 | `shield_wall` | S | 10 | 90 | 0 | short | self | self | `damage_taken_mod` all −30 % for 8 s; `requires_shield` |
| 18 | `bulwark` | S | 8 | 30 | 0 | std | 10 | ally | target `damage_taken_mod` all −20 % for 6 s; `threat_on_apply` 0 |

### 3.3 Archery kit — governing skill `archery`; all need a `ranged` weapon and LOS

| L | Ability | E | Cost | CD | Cast | GCD | Rng | Tgt | Effect |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | `quick_shot` | S | 5 | 0 | 0 | std | 30 | enemy | physical, mod 0.7 |
| 4 | `aimed_shot` | S | 7 | 0 | 2.0 | std | 30 | enemy | physical, mod 1.3 (`t_ref` 2.0); cancels on move; not interruptible (physical) |
| 7 | `crippling_shot` | S | 6 | 12 h | 0 | std | 30 | enemy | physical, mod 0.6; `move_speed` −40 % for 6 s (World `D_CAP`) |
| 10 | `disengage` | S | 6 | 20 | 0 | none | self | self | `displace` knockback self 8 m away from facing; usable in combat |
| 14 | `multi_shot` | S | 9 | 0 | 0 | std | 25 | cone 30°, ≤ 3 | physical, mod 0.42 per target |
| 18 | `serpent_sting` | S | 7 | 12 h | 0 | std | 30 | enemy | `damage_over_time` poison, total mod 1.6 over 15 s (5 ticks), `refresh`, dispel `poison` |
| 20 | `steady_focus` | S | 8 | 60 | 0 | none | self | self | `stat_mod` haste +0.20 for 10 s (inside `HASTE_MAX`) |

### 3.4 Magic kit — governing skill `magic`; spells are `interruptible` (school lockout 3 s)

| L | Ability | E | Cost | CD | Cast | GCD | Rng | Tgt | School | Effect |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | `fire_bolt` | M | 5 | 0 | 2.0 | std | 30 | enemy | fire | mod 1.4 (`t_ref` 2.0) |
| 4 | `frost_shard` | M | 6 | 0 | 0 | std | 30 | enemy | frost | mod 0.7; `move_speed` −30 % for 4 s |
| 7 | `arcane_ward` | M | 8 | 30 | 0 | std | self | self | arcane | `absorb` amount mod 2.0, all schools, 20 s |
| 10 | `counterspell` | M | 4 | 24 | 0 | none | 30 | enemy, casting | arcane | interrupt, §4.2; no damage |
| 14 | `flame_burst` | M | 10 | 15 h | 0 | std | 30 | around target 6 m, ≤ 8 | fire | mod 0.9 per target |
| 18 | `entangle` | M | 6 | 20 | 0 | std | 25 | enemy | nature | `cc` root 5 s (DR category `root`); dispel `magic` |
| 20 | `blink` | M | 8 | 30 | 0 | none | self | self | arcane | `displace` teleport_spell 15 m along facing; breaks `root` on self |

### 3.5 Healing kit — governing skill `healing`; heals are `interruptible` (holy lockout 3 s)

| L | Ability | E | Cost | CD | Cast | GCD | Rng | Tgt | Effect |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | `mend` | M | 6 | 0 | 2.0 | std | 30 | ally/self | holy heal, mod 1.5 (`t_ref` 2.0) |
| 4 | `smite` | M | 5 | 0 | 1.5 | std | 30 | enemy | holy damage, mod 1.0 |
| 7 | `renew` | M | 5 | 0 | 0 | std | 30 | ally/self | `heal_over_time` total mod 1.2 over 12 s (4 ticks), `refresh`, per caster |
| 10 | `purify` | M | 5 | 8 | 0 | std | 30 | ally/self | dispel one `magic`, `poison` or `disease` effect (newest first) |
| 14 | `revive` | M | 20 | 0 | 10.0 | std | 30 | dead ally | out of combat only; target returns with `tune.combat.res_health_pct` (new, 0.35, bounds 0.10–1.00) of health and mana after accepting (`res_accept_s`); adds no durability wear |
| 18 | `flash_mend` | M | 10 | 0 | 1.0 | std | 30 | ally/self | holy heal, mod 1.0 (`t_ref` 1.5): expensive, fast |
| 20 | `healing_circle` | M | 14 | 10 | 2.5 | std | 30 | ground 6 m, ≤ 5 | holy heal, mod 0.9 per target |

### 3.6 Shadow kit — governing skills `stealth` (openers need `daggers` or `unarmed`)

| L | Ability | E | Cost | CD | Cast | GCD | Rng | Tgt | Effect |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | `hide` | S | 5 | 6 | 0 | short | self | self | toggle `stealth` level 1; walk speed; breaks on any action except `backstab`; out of combat only |
| 7 | `backstab` | S | 10 | 20 h | 0 | std | melee | enemy, from behind | `requires_stealth`; physical, mod 2.4, `can_crit` |
| 14 | `poisoned_blade` | S | 6 | 0 | 0 | short | self | self | self buff 30 s: `proc` on_hit chance 0.3, `effect/weak_poison` (`damage_over_time` poison total mod 0.6 over 6 s, `stack` ≤ 3), icd 1 s |
| 20 | `vanish` | S | 12 | 90 | 0 | none | self | self | `threat_drop` on all tables, enter `stealth` level 2 for 6 s even in combat |

Kits share nothing but the rule in §1: a Fighter who learns `mend` and `counterspell` is the owner's paladin, paying Mana for both.

## 4. Taunt and interrupt

Both unlock at level 10 (owner). Published abilities replace the sandbox T/I development activations; the built server rules stay.

### 4.1 Taunt — `ability/challenge`

| Field | Value | Source |
| --- | --- | --- |
| Kind / skill / school | `utility`, `skill/tactics`, physical | Part 1: taunt threat × (1 + 0.05 × tactics/1000) |
| Target rule | one hostile NPC in LOS within range; no facing check; PvP target → `taunt` CC 1.5 s (`tune.combat.pvp_taunt_s`) with DR | Part 2 PvP |
| Effect | threat = `max(own, top × 1.10)` (`taunt_threat_mult`), forced target 3 s (`taunt_force_s`), DR 3 / 1.5 / 0.75 / 0 s, reset 18 s (`dr_window_s`) | built in `SFCombatPilot`, `SFTauntRules` |
| Cost / CD / GCD | 5 % Stamina, 8 s, off-GCD (`t_ref_s` 0.5), not haste scaled | this part |
| Range | 10 m | this part |
| Denials | out of range, no LOS, dead, friendly: rejected, no cost; `taunt_immune` NPC or DR-immune (4th application): cost and cooldown spent, nothing applied | policy |
| Threat flag | `threat.taunt` true; `threat.on_apply` 0 | Part 3 |

### 4.2 Interrupt — `ability/pommel_strike` (melee, Stamina) and `ability/counterspell` (ranged, Mana)

| Field | `pommel_strike` | `counterspell` |
| --- | --- | --- |
| Kind / skill / school | `attack`, weapon skill, physical | `spell`, `skill/magic`, arcane |
| Target rule | hostile in LOS, facing ≤ 90°, within `melee_reach_m`, **currently casting or channeling** | hostile in LOS within 30 m, **currently casting or channeling** |
| Effect | cancel the cast, lock its school for `lockout_s` 3 s (not haste scaled), full cost refund to the victim; plus physical mod 0.3 | cancel, 3 s lockout, refund; no damage |
| Cost / CD / GCD | 5 % Stamina, 15 s, off-GCD | 4 % Mana, 24 s, off-GCD |
| Not casting | rejected, no cost or cooldown | same |
| Uninterruptible cast (encounter `interruptible: false`, tier immunity) | cost and cooldown spent; damage lands | cost and cooldown spent |
| Interrupt itself | `interruptible` false; `counterspell` is instant so never locked out by its own school except after being interrupted while casting another arcane spell | |

Built values kept: 3 s lockout and full refund (`SFCastingComponent`), 1.5 s GCD, 400 ms queue window, 50 % self-cancel refund.

### 4.3 Enhanced ladder

Ranks are separate ability definitions (`ability/challenge_2` …) learned at the level; the lower rank is replaced on the bar. Rank levels are release content (`ABILITY_RANK_LEVELS` = 10, 30, 50, 70, 90). Force duration, threat multiplier, DR and refund never change with rank (they are realm tuning keys).

| Rank | Level | `challenge` range / CD / cost | `pommel_strike` CD / lockout / cost | `counterspell` CD / lockout / cost |
| --- | --- | --- | --- | --- |
| I | 10 | 10 m / 8 s / 5 % | 15 s / 3 s / 5 % | 24 s / 3 s / 4 % |
| II | 30 | 12 m / 8 s / 5 % | 14 s / 3 s / 5 % | 22 s / 3 s / 4 % |
| III | 50 | 14 m / 7 s / 4 % | 13 s / 4 s / 4 % | 20 s / 4 s / 4 % |
| IV | 70 | 16 m / 7 s / 4 % | 12 s / 4 s / 4 % | 18 s / 4 s / 3 % |
| V | 90 | 20 m / 6 s / 3 % | 10 s / 5 s / 3 % | 15 s / 5 s / 3 % |

Additional enhanced abilities, outside the starter range: `mass_challenge` at L40 (around_self 8 m, ≤ 5 NPCs, each taunted as rank III, 12 % Stamina, 30 s CD, off-GCD) and `silence` at L60 (`cc` silence 3 s on one target in 30 m, 6 % Mana, 45 s CD, standard GCD; a silence is not an interrupt and gives no refund). Lockout above 5 s and taunt range above 20 m are not authored.

## 5. Sprint

| Key | Type / unit | Default | Bounds | Step | Rule |
| --- | --- | --- | --- | --- | --- |
| `tune.combat.sprint_drain_pct_s` (new) | fraction of max Stamina /s | 0.04 | 0.01–0.20 | 0.005 | deducted on the 1 s simulation tick (prorated on the last tick); sprint ends at Stamina 0 |
| `tune.combat.sprint_regen_delay_s` (new) | float s | 2 | 0–10 | 0.5 | Stamina regeneration is suspended while sprinting and for this long after; the combat `regen_delay_s` is separate |
| `tune.combat.sprint_min_start_pct` (new) | fraction of max Stamina | 0.10 | 0–0.50 | 0.01 | below this, the sprint intent is rejected (`run` continues); prevents stutter sprinting |

Result: 25 s of sprint from full at any pool size (225 m at 9 m/s), then 2 s pause and a refill of `max / (25 + dex/25)` s (Fighter 10.5 s, Mage 7 s). Sprint remains out-of-combat only and combat entry forces `run` (World spec); every ability used while the pool is below its cost is rejected with `combat_resource`, so a sprinting character arrives with less opening Stamina. Scope realm, apply `immediate`, plain label "Sprint stamina drain" in percent of the bar per second.

## 6. Death and durability touch points

Owner decision: from level 10 each death wears 5 % durability off equipped gear (`tune.combat.death_durability_pct`, new, 0.05, bounds 0–0.25, step 0.01, realm / `immediate`; 0 below level 10, `tune.combat.death_penalty_min_level` 10, bounds 1–100). Ability interactions only: `revive`, spirit release and accepted resurrection add no further wear; a weapon at 0 % keeps weapon abilities usable but feeds `unarmed_dps(L)` into the damage formula (Part 2 "broken weapon"); armor at 0 % contributes no stats, so cooldown abilities such as `shield_wall` still apply their percentage on top of whatever remains.

## 7. Owner check

1. Melee reach 3.0 m for published abilities (pilot uses 2.8 m). Keep 3.0, or adopt 2.8?
2. Part 1's absolute Stamina regen makes low-level fillers nearly free and high-level fillers expensive. Accept, or convert `stamina_regen_combat` to a fraction of max (≈ 0.02 /s) in a Part 1 revision?
3. Two interrupts at level 10 (melee Stamina, ranged Mana) rather than one. Agreed?
4. Stealth kit `hide` at level 1 is out-of-combat only; `vanish` at 20 is the first in-combat stealth. Agreed?
5. Rank ladder 10/30/50/70/90 for every ability that gains ranks, not only taunt and interrupt. Agreed?

## Change log

- 0.1 (2026-10-25): first draft from the 2026-10-25 owner decisions: energy rule, 38 starter abilities in six kits, taunt and interrupt definitions and five-rank ladder, sprint drain, death/durability touch points.
