# Legends of the Shattered Frontier — Combat: Damage Pipeline, Mitigation, NPCs and Threat
### Version 1.0 — Expanded Technical Design Authority edition (2026-10-17)

Status: Design target; not implemented. Names per [Shared Registry v1.0](../Legends_Shattered_Frontier_Shared_Registry_v1.0.md).
Entry file: [Combat spec](../Legends_Shattered_Frontier_Combat_System_v0.1.md). Part 2 of 4.

This part owns how a hit becomes a number: the damage and healing formulas, the per-hit resolution pipeline, armor and resistances, NPC mitigation, NPC attribute generation and the creature-tier rule, NPC combat profiles, threat and aggro, kill credit and death, PvP rules, and combat randomness. Inputs come from Part 1 ([Attributes, ratings and reference build](Legends_Shattered_Frontier_Combat_Attributes_and_Ratings_v1.0.md)); ability and effect data come from Part 3 ([Abilities, effects and runtime](Legends_Shattered_Frontier_Combat_Abilities_Effects_and_Runtime_v1.0.md)).

---

## Damage formula

### Character-sourced damage and healing

Characters, their pets and summons share one formula. `t_ref` is the ability's reference time, which makes every ability's value proportional to the time it occupies.

```text
Raw = (weapon_dps × t_ref × spread + Power × t_ref / POWER_SPEED_REF) × AbilityModifier
Out = Raw × σ(L) × skill_mult × Π (1 + Σ category_mods) × crit_mult
```

| Symbol | Definition |
| --- | --- |
| `weapon_dps` | Itemization `weapon_dps` of the governing weapon (main hand; off-hand for off-hand abilities; ranged for ranged). Spells use the equipped weapon's `weapon_dps` as the *implement* value (staves are `two_hand`). No weapon or broken weapon: `unarmed_dps(L) = UNARMED_DPS_FACTOR × WEAPON_DPS_K × B(4L)`, `UNARMED_DPS_FACTOR` 0.40 (`tune.combat.unarmed_dps_factor`), `speed_s` 2.0. |
| `spread` | `WeaponDamage / avg_hit`, where `WeaponDamage` is one sample from Itemization's `[min_hit, max_hit]` (`WEAPON_SPREAD` 0.15). Spells and heals sample the same spread from their implement. |
| `Power` | `PhysicalPower` for `physical` school, `SpellPower` for every magic school and for heals. |
| `t_ref` | auto-attack: weapon `speed_s`; instant ability: `GCD_BASE` 1.5 s (1.0 s for `gcd_class: short`); cast: `max(cast_s, GCD_BASE)`; channel: `channel_s` split evenly over ticks; DoT/HoT: authored `t_ref` split evenly over ticks; off-GCD instant: authored `t_ref` (default 0.5 s). All pre-haste. |
| `POWER_SPEED_REF` | 2.4 s (`tune.combat.power_speed_ref_s`): a 2.4 s ability adds exactly `Power` once, preserving the v0.1 form `(WeaponDamage + PhysicalPower) × AbilityModifier`. |
| `AbilityModifier` | authored per ability (Part 3, Throughput budget). Auto-attack 1.0 (off-hand auto 1.0 of the off-hand weapon; Itemization already halves off-hand `weapon_dps`). |
| `σ(L)` | throughput scale (Part 1, Reference throughput), attacker's synced level. |
| `skill_mult` | Part 1, Skill effects on combat. |
| `category_mods` | Modifier categories (below). |
| `crit_mult` | `crit_damage` on a crit, else 1. |

v0.1 forms, kept as the special case `t_ref = POWER_SPEED_REF`, `σ = 1`, `skill_mult = 1`:

```text
PhysicalDamage = (WeaponDamage + PhysicalPower) × AbilityModifier
SpellDamage    = (AbilityBase + SpellPower) × AbilityModifier       AbilityBase = implement weapon_dps × t_ref × spread
Healing        = (AbilityBase + SpellPower) × AbilityModifier × (1 + Wisdom / 1000)
```

Healing additionally multiplies by `heal_wis_mult = 1 + wis_total / 1000` (Part 1, Wisdom) and by the **target's** healing-taken modifiers, and is never mitigated.

### NPC-sourced damage and healing

NPC damage is sized against the reference character, not computed from NPC attributes (NPC combat profile):

```text
DMG_npc(ability)   = H_ref(L_npc) × dmg_coef × T_dmg(t) × context_dmg
DMG_npc(auto swing) = H_ref(L_npc) × npc_dps_coef[archetype] × swing_s × T_dmg(t) × context_dmg
HEAL_npc           = H_ref(L_npc) × heal_coef × context_dmg
```

`context_dmg` is 1.0 in the open world and `G_dmg(n) × D_dmg(d) × Y(M) × Π affix_dmg × E_soft` inside dungeons (Dungeon spec, Content scaling and rewards part, Damage) or the Raid equivalents. NPC damage enters the pipeline at step 4 (crit) with `σ` and `skill_mult` not applied; accuracy still uses skill (Part 1).

### Modifier categories

Percent modifiers on damage and healing done are **additive within a category and multiplicative across categories**, so a new category cannot double-dip with existing ones.

| Category | Examples (stat modifier keys, Part 3) | Clamp of Σ |
| --- | --- | --- |
| `ability` | "Riposte +10 %", set bonus on one ability | [−0.5, +1.0] |
| `school` | `weapon_damage`, `spell_damage`, `fire_damage` … | [−0.5, +0.5] |
| `target_shape` | `single_target_damage`, `cleave_damage` (mastery capstones) | [0, +0.25] |
| `global` | `damage_done` buffs (raid buffs, leadership auras) | [−0.5, +0.30] |
| `healing` | `healing_done` (heals only, replaces `school`) | [−0.5, +0.5] |

Damage-taken modifiers on the defender (`damage_taken`, `<school>_damage_taken`) multiply **individually** at step 6 and their product is clamped to [0.10, 2.0]: no stack of player effects makes a player immune ("No Hard Immunities"). Authored NPC phase immunity is an encounter state (`targetable: false` or `immune: true`), not a modifier.

### Area damage

```text
per_target = Out × min(1, sqrt(AOE_SOFT_TARGETS / n_hit))      n_hit ≤ ability.max_targets (default 8)
AOE_SOFT_TARGETS = 5 (tune.combat.aoe_soft_targets)
```

Targets are chosen in ascending distance, ties by ascending entity ID. Cleave (`shape: cone`, ≤ 3 targets) is exempt from the soft cap. Example: 8 targets hit → each takes `sqrt(5/8)` = 0.79 of single-target damage.

### Over-time effects

DoT and HoT ticks are resolved independently through the pipeline (each tick rolls crit, applies current modifiers and mitigation). Tick count is fixed at application; haste does not change DoT/HoT tick count or duration (Part 1, Haste). Re-application refreshes to `min(remaining + duration, 1.3 × duration)` ("pandemic" window `tune.combat.dot_refresh_cap` 1.3).

### Lifesteal, absorbs and overkill

- **Lifesteal**: `heal = landed × min(lifesteal_bp, 600) / 10000` after step 7; it is healing for threat but cannot crit and has no skill or `σ` scaling (it already derives from scaled damage).
- **Absorbs** (`absorb` effects, Part 3) consume damage after mitigation and block, oldest first; ties by effect instance ID. Absorbed damage counts as damage dealt for threat and kill credit, and as healing done by the absorb's caster for threat and `combat.damage.summarized`.
- **Overkill** (damage beyond remaining health) counts for nothing: not for threat, credit or summaries.

### Environmental damage

Falling, drowning, lava and trap volumes deal `environment` damage: `MaxHealth × env_coef` per event or tick, school `physical` (falling, traps) or the volume's school. It ignores armor, avoidance and block but honors resistances for magic schools. `env_coef` is content data on World volumes; falling uses World's impact speed (`fall_coef = clamp((v_impact − 12) / 20, 0, 1.2)`, `tune.combat.fall_speed_threshold_mps` 12).

---

## Resolution pipeline

Every step runs on the simulating server (`zone-server` or `instance-server`) at the simulation tick where the ability resolves.

1. **Validate** the action (Security spec, Combat validation). Rejection → `security.action.rejected` with the registry `reject_reason`; nothing below runs.
2. **Hit roll** (`miss_chance`, Part 1, Skill effects on combat). Weapon attacks: weapon skill + tactics vs defense skill. Spells: `magic` vs `resist_magic` → outcome `resist` (full miss). Heals and authored `cannot_miss` abilities skip this step.
3. **Avoidance roll** (physical, attacker in the defender's front arc): dodge, then parry, after the `AVOIDANCE_CAP` scaling. Spells cannot be dodged or parried.
4. **Crit roll**: `crit_chance` (Part 1, Critical Strike Formula); damage × `crit_damage` (NPC archetype attacks: `NPC_CRIT_DAMAGE` 1.5).
5. **Mitigation**: armor after armor penetration (physical) or resistance after magic penetration (spell school). Environmental physical damage skips armor.
6. **Block roll** (physical, frontal, shield equipped, or `blockable: true` ability): blocked damage × (1 − `block_reduction`). Then defender damage-taken modifiers (product clamped [0.10, 2.0]).
7. **Absorbs**, then `round()` half up to an integer ≥ 0. A hit that rounds to 0 after a non-zero raw value is reported as 1 unless fully absorbed.
8. **Apply**: health change, death check (Death), lifesteal, procs (Part 3, Procs), resource refunds.
9. **Threat** (Threat & Aggro System), kill-credit contribution (Kill credit), practice records (Part 3, Practice reporting), wear counters (Itemization `ApplyWear`), damage summary counters.

Outcome enum (proposed `hit_outcome`): `hit`, `crit`, `miss`, `resist`, `dodge`, `parry`, `block`, `crit_block`, `absorb`, `immune`. A blocked crit is `crit_block`; a hit fully absorbed is `absorb`.

### Random draws

The pipeline consumes exactly **five** uniform draws per hit from the attacker's combat stream, in this order, whether or not each draw is needed: `d_hit`, `d_avoid`, `d_crit`, `d_block`, `d_spread`. Fixed consumption makes decision replays reproducible (Determinism and combat randomness). A roll succeeds when `d < chance`. Dodge and parry share `d_avoid`: dodge if `d_avoid < dodge`, parry if `dodge ≤ d_avoid < dodge + parry`.

### Worked example — auto-attack

Reference damage dealer, L100, rare ilvl 400 two-hand (`weapon_dps` 167.4, `speed_s` 3.4 → `avg_hit` 569.2, range 484–655), `str` 324 (PhysicalPower 648.8), vs a common L100 NPC (`medium` armor 5,400).

```text
WeaponDamage sample = 600            spread = 600 / 569.2 = 1.054
Raw = 167.4 × 3.4 × 1.054 + 648.8 × 3.4 / 2.4 = 600.0 + 919.1 = 1,519.1   (modifier 1.0)
Out = 1,519.1 × σ(100) 0.3098 × skill 1.00 = 470.6           (no crit: d_crit 0.41 ≥ 0.207)
Armor: 5,400 / (5,400 + 100 × 100) = 0.3506 → 470.6 × 0.6494 = 305.6 → 306
```

Over a rotation the auto-attack averages `(569.2 + 919.1) × 0.3098 × 0.6494 / (3.4 × 0.878)` ≈ 100 DPS, 1.0 of the 1.6 reference modifier-per-second; the GCD abilities supply the rest of `DPS_ref(100, 400)` = 194.

### Worked example — instant ability, crit

Same character, "Sundering Blow" (`AbilityModifier` 1.2, instant, `t_ref` 1.5), same sample spread 1.054:

```text
Raw = (167.4 × 1.5 × 1.054 + 648.8 × 1.5 / 2.4) × 1.2 = (264.7 + 405.5) × 1.2 = 804.2
Out = 804.2 × 0.3098 = 249.2;  crit (d_crit 0.11 < 0.207): × 2.0 = 498.3
Armor 0.3506 → 323.6 → 324
```

### Worked example — NPC tank hit through a tank

Dungeon `abl_crushing_tide` (frost, `dmg_coef` 0.30, veteran, 5 players): 1,852 pre-mitigation (Dungeon spec worked example 1). The tank (L100, Wisdom 43, `resist_magic` 300, no frost gear):

```text
Resistance(frost) = 0 + 0 + floor(43/2) + floor(300/10) = 21 + 30 = 51
ResistMitigation  = 51 / (51 + 5 × 102) = 0.0909      (attacker level L_npc 102)
Not blockable (spell) → 1,852 × 0.9091 = 1,683.6 → 1,684
```

Authoring note: magic tank hits bypass armor, avoidance and block. The Dungeon example's "60 % tank mitigation" applies only to **physical** `tank_hit` abilities; magic tank hits should use `dmg_coef` ≈ 0.6 × the physical value, or be paired with a resist cooldown mechanic.

---

## Armor System

```text
DamageReduction = EffectiveArmor / (EffectiveArmor + ARMOR_K_LEVEL × AttackerLevel)
ARMOR_K_LEVEL = 100 (tune.combat.armor_k_level);  DamageReduction ≤ ARMOR_DR_CAP = 0.85 (tune.combat.armor_dr_cap)
Armor = Σ implicit item armor (non-broken) + bonus_armor + armor effects
```

Example: Armor 5,000 vs attacker level 50 → 5,000 / 10,000 = **50 %**.

| Armor at L100 attacker | Reduction |
| --- | --- |
| 2,232 (rare 400 leather) | 18.2 % |
| 5,400 (NPC `medium`) | 35.1 % |
| 8,656 (epic 460 plate) | 46.4 % |
| 10,000 | 50.0 % |
| 56,667 | 85.0 % (cap) |

---

## Magic Resistance System

```text
ResistMitigation   = EffectiveResistance / (EffectiveResistance + RESIST_K × AttackerLevel)
RESIST_K = 5 (tune.combat.resist_k);  ResistMitigation ≤ RESIST_CAP = 0.75 (tune.combat.resist_cap)
Resistance(school) = resist_<school> + resist_all + floor(wis / 2) + floor(resist_magic / 10) + school_bonus
school_bonus       = floor(con / 4) for poison and disease, else 0
```

Schools (`damage_school`, registered in Shared Registry, Phase-2 enums): `physical` plus the eight magic schools `fire`, `frost`, `nature`, `shadow`, `arcane`, `holy`, `poison`, `disease` (Itemization `resist_<school>` keys). `physical` uses armor; every other school uses resistance.

Example: level-100 attacker, 108 `resist_fire` + Wisdom 150 (75) → 183 / (183 + 500) = **26.8 %**.

Attribute and skill resistance terms apply to **characters only**; NPC resistances come from NPC mitigation so that `d_ref` holds.

---

## NPC mitigation

NPCs use the same reduction formulas. Armor and resistance come from an archetype coefficient on the NPC definition:

```text
Armor_npc      = armor_coef  × 100 × L_npc
Resistance_npc = resist_coef × RESIST_K × L_npc
```

`armor_coef`: `light` 0.25 (20 % vs equal level), `medium` 0.54 (35 %, default, `d_ref`), `heavy` 1.00 (50 %). `resist_coef` uses the same three values, default `medium`. Keys `tune.combat.npc_armor_coef.<class>`, `tune.combat.npc_resist_coef.<class>`. Per-school overrides on the definition (`resist_class_by_school`, e.g. fire elementals `heavy` fire, `light` frost) replace the default class for that school only.

| Class (`npc_armor_class`, registered in Shared Registry, Phase-2 enums) | vs equal level | vs attacker 5 levels lower |
| --- | --- | --- |
| `light` | 20.0 % | 20.8 % (L100 NPC vs L95) |
| `medium` | 35.1 % | 36.2 % |
| `heavy` | 50.0 % | 51.3 % |

Penetration applies to NPC armor/resistance exactly as to characters. Debuffs that reduce armor (`stat_mod` on `armor`, e.g. "Sunder −10 %") are effects; their sum is clamped to −30 % per target (`tune.combat.armor_debuff_cap`) before penetration.

---

## NPC Attribute Generation

All NPCs use identical formulas. Attributes and skills come from the Character spec's NPC progression templates (Character spec, NPC parity: `attr_ref(L)`, archetype weights, ±5 % deterministic spawn variance; `skill_ref(L)` + `off(tier)`). NPCs do not gain skills or attributes at runtime.

Creature Tiers (registry `npc_tier`, multiplier `T`):

```text
Tier 1: Common Creatures (1.0x)   common
Tier 2: Elite Creatures (2.5x)    elite
Tier 3: Mini Bosses (5.0x)        mini_boss
Tier 4: World Bosses (20.0x)      world_boss
```

### Creature-tier rule

**Ratified 2026-10-17.** `T` multiplies **health**; archetype damage is multiplied by `√T` (`tune.dungeon.tier_damage_exponent` 0.5 → 1.0 / 1.58 / 2.24 / 4.47). Encounter-authored abilities use `T_dmg = 1.0` (their `dmg_coef` is final). Tiers never multiply NPC attributes, ratings, armor or resistances (Character spec, NPC parity). Health keys `tune.combat.tier_health_mult.<npc_tier>`. Dungeon spec, Scaling, implements the rule.

Tier effects outside health and damage are fixed here:

| Effect | `common` | `elite` | `mini_boss` | `world_boss` |
| --- | --- | --- | --- | --- |
| Health × `T` | 1.0 | 2.5 | 5.0 | 20.0 |
| Archetype damage × `√T` | 1.00 | 1.58 | 2.24 | 4.47 |
| Skill offset `off(tier)` (Character) | 0 | +50 | +100 | +150 |
| Default `level_offset` (Dungeon) | 0 | +1 | +2 | authored |
| Crowd-control immunity (Part 3) | none | `charm` | `stun`, `fear`, `charm`, `incapacitate` | all except authored `interruptible` casts; slows capped 0.30 |
| Kill XP multiplier (Character `T(tier)`) | 1.0 | 2.5 | 5.0 | 20.0 |

**Raid bosses.** Raid bosses carry `npc_tier = world_boss` for classification, CC immunity and kill XP; their health is the Raid spec's `H` (the tier health multiplier does not apply), their abilities are encounter-authored (`T_dmg` 1.0), and their armor and resistances come from NPC mitigation with default class `heavy`. "Tier 4 baseline" (Raid spec, Raid boss health) therefore means classification and `heavy` mitigation, never multiplied armor or ratings.

### What NPC attributes do

Because NPC health and damage are sized from `H_ref` (NPC combat profile), NPC attributes affect only:

| Attribute | NPC use |
| --- | --- |
| `dex` | archetype attack crit chance `min(0.20, CRIT_BASE + dex / 5000)` |
| `wil` | crowd-control duration reduction (Part 1, Willpower) |
| `str`, `int` | none in damage (`dmg_coef` is final); exported for AI behavior tags |
| `con` | none in health; exported for AI behavior (flee thresholds) |
| `wis` | none (NPC resistances are NPC mitigation) |
| `luck` | none |
| `cha` | `commander` archetype aura magnitude (`cha_aura_mult`) |

Skills (Character's NPC skills) set NPC **accuracy** (offense skill vs player defense skill) and NPC **defense** (player miss chance), Part 1.

### NPC combat profile

```text
HP_npc = round(H_ref(L_npc) × T_hp(t) × A_hp × context_hp)
```

`context_hp` = 1.0 open world; Dungeon `G_hp(n) × D_hp(d) × Y(M) × Π affix_hp`; raid bosses: Raid spec `H`. NPC Constitution does **not** set NPC health.

| Field (NPC definition `combat` block) | Default | Notes |
| --- | --- | --- |
| `archetype` | — | registry enum `npc_archetype` (`brute`, `skirmisher`, `caster`, `healer`, `guardian`, `beast`, `commander`, `shaman`, `champion`, `guard`, `civilian`); attribute weights: Character spec; behavior: AI spec |
| `npc_dps_coef` | by archetype (table below) | fraction of `H_ref(L_npc)` per second from auto-attacks, pre-mitigation |
| `swing_s` | 2.0 (casters: `cast_s` 2.5 bolt) | |
| `school` | `physical` (casters: authored) | |
| `armor_class`, `resist_class` | `medium` | NPC mitigation |
| `A_hp` | 1.0 | boss `boss_hp_factor` (Dungeon) |
| `dodge`, `parry`, `block` | 0.03 / 0.03 (melee archetypes, front arc) / 0.05 (`guardian` with shield) | fixed; never from ratings |
| `can_crit` | true for archetype attacks, false for encounter abilities | crit damage 1.5 |
| `cc_profile` | from tier (Creature-tier rule) | may add immunities, never remove tier immunities |
| `leash` | Dungeon/AI defaults | evade behavior (Dungeon spec, Leashing and evade) |
| `threat_profile` | `standard` | Threat profiles |

Archetype auto-attack defaults (`tune.combat.npc_dps_coef.<archetype>`):

| Archetype | `npc_dps_coef` | Role in fights |
| --- | --- | --- |
| `brute` | 0.030 | melee pressure |
| `beast` | 0.030 | melee, fast swings (`swing_s` 1.6) |
| `skirmisher` | 0.028 | mobile melee/ranged |
| `caster` | 0.034 | spell bolts, interruptible |
| `guardian` | 0.020 | shields allies |
| `healer` | 0.015 | heals `heal_coef` 0.15 per 10 s (Dungeon shaman) |
| `commander` | 0.024 | buffs pack (`leadership_aura`) |
| `shaman` | 0.015 | support caster; same coefficients as `healer` (AI shaman behavior) |
| `champion` | 0.030 | mini-boss melee; encounter abilities add the rest |
| `guard` | 0.026 | settlement defender (melee or ranged) |
| `civilian` | 0.008 | non-combatant; flees (AI spec, Civilians: cower and flee) |

Balance target (Part 1, Endgame Balance Targets): a same-level `common` NPC loses to a reference damage dealer, leaving 35–45 % health; an `elite` beats a solo reference player.

### Worked example — open-world fights at level 30

Reference damage dealer L30: `H_ref(30)` = 1,735, `DPS_ref(30, 120)` = 69.4, mitigation vs physical 20 % (leather + avoidance).

| Opponent | HP | Player TTK | NPC damage to player | Result |
| --- | --- | --- | --- | --- |
| Common brute L30 | 1,735 × 1.0 = 1,735 | 1,735 / 69.4 = 25.0 s | 1,735 × 0.030 = 52.1/s pre → 41.6/s → 1,041 over 25 s | player wins at 40 % health |
| Elite brute L31 | `H_ref(31)` 1,779.5 × 2.5 = 4,449 | armor vs L30: 1,674/(1,674+3,000) = 35.8 % → 68.6 DPS → 64.9 s | 1,779.5 × 0.030 × 1.581 = 84.4/s pre → 67.5/s → player dies at 25.7 s | elite wins; 3 players kill it in ≈ 22 s |

Elite brute swing: `1,779.5 × 0.030 × 2.0 × 1.5811` = 168.8 → **169** per swing before mitigation; crit (brute `dex` = round(`attr_ref(31)` 244 × 0.10) = 24 → 0.05 + 0.0048 = 5.5 %) → 169 × 1.5 = 253.

---

## Threat & Aggro System

Threat decides NPC target selection after engagement. Aggro radius only triggers engagement (Dungeon spec, Perception and aggro; AI spec for world NPCs). Players and player-owned pets are never threat-driven.

### Threat & Aggro

```text
1 Damage  = 1 Threat
1 Healing = 0.5 Threat        (split evenly across engaged NPCs)
Taunt     = Forced Top Threat (threat = top × 1.10, forced target 3 s)
threat_final = threat_base × (1 + threat_mod_bp / 10000) × Π threat_mult_effects
```

| Source | `threat_base` | Recipients |
| --- | --- | --- |
| Damage landed on the NPC (incl. absorbed by the NPC's shields; excl. overkill) | `1.0 × amount` | that NPC |
| Effective healing (excl. overheal) on a character | `HEAL_THREAT` 0.5 × amount, divided by `k` | every NPC (count `k`) whose threat table contains the healed target |
| Absorb consumed on a character | 0.5 × absorbed, divided by `k` | as healing; credited to the absorb's caster |
| Hostile effect applied without damage | authored `threat_on_apply` (default 0) | the target NPC |
| Beneficial effect on an engaged ally | authored `threat_on_apply` (default 0), split as healing | as healing |
| Resource gain, regeneration | 0 | — |

`threat_mod_bp` sums item, mastery and effect basis points, clamped to ±2,000 (Itemization global cap). `threat_mult_effects` are effect multipliers (tank stances e.g. ×2.0, threat-reduction cloaks ×0.7); their product is clamped to [0.10, 5.0] (`tune.combat.threat_mult_min`, `tune.combat.threat_mult_max`). Taunt-flagged abilities also multiply by `1 + 0.05 × tactics / 1000`.

### Threat table rules

1. **Entry.** A character enters an NPC's table when it damages the NPC, applies a hostile effect to it, heals or buffs a character already on the table, or is detected by the NPC's aggro check (initial threat 1, Dungeon spec, Initial threat). Pack-linked NPCs share engagement but keep separate tables.
2. **Target selection.** An NPC attacks the highest-threat character it can reach. It switches only when a challenger's threat exceeds the current target's by **110 %** within 5 m or **130 %** beyond (`tune.dungeon.threat_switch_melee` 1.10, `tune.dungeon.threat_switch_ranged` 1.30 — Combat adopts the Dungeon keys realm-wide).
3. **Taunt.** Sets the taunter's threat to `max(own, top × TAUNT_THREAT_MULT)`, `TAUNT_THREAT_MULT` 1.10 (`tune.combat.taunt_threat_mult`), and forces the NPC to attack the taunter for `TAUNT_FORCE_S` 3 s (`tune.combat.taunt_force_s`). Taunts have their own diminishing returns per NPC (`cc_category` `taunt`, Part 3). NPCs with `taunt_immune` (encounter-authored) ignore taunts. This replaces the Dungeon draft's "top × 1.0" so that the taunter keeps the target after the forced window.
4. **Utility targeting.** Encounter specials and elites with utility profiles read the threat rank as one input (`threat_rank = 1 − rank/(n−1)`, Dungeon spec, Encounter scripting); utility choices never modify threat.
5. **Removal.** Death, leaving the NPC's leash range for 5 s, `threat_drop` effects (vanish, feign) and evade remove the character from the table. Resurrection does not restore threat.
6. **Reductions.** `threat_reduce` effects multiply the caster's threat on all tables by `(1 − pct)`; immediate.
7. **Phase reset.** Encounter phases with `threat_reset_on_phase` (Raid spec) set every entry to 0 and re-run the initial-threat rule at the next decision tick.
8. **Evaluation cadence.** Threat accumulates on the simulation tick; NPC target decisions read a snapshot at the 10 Hz decision tick (Dungeon spec, Determinism and seeding). Ties break by ascending `character_id`.
9. **Table size.** At most 40 entries (`tune.combat.threat_table_max`); the lowest entry is evicted when full.

### Worked example

Tank with stance ×2.0 and `threat_mod_bp` +500 deals 1,000 damage: `1,000 × 1.05 × 2.0` = 2,100 threat. Healer heals the tank for 1,200 effective (overheal 300 excluded) while 3 NPCs have the tank on their tables: `1,200 × 0.5 / 3` = 200 threat on each. A damage dealer with 2,400 threat at 20 m on NPC A (tank at 2,100): 2,400 / 2,100 = 1.14 < 1.30 → no switch; at 2,800 (1.33) the NPC switches. If the tank then taunts: tank threat = 2,800 × 1.10 = 3,080, forced 3 s.

### Pets and summons

Pets and summons have their own threat entries (their damage generates threat for themselves, not their owner). Healing done **by** a pet credits the pet. Owner commands (`pet_attack`, `pet_passive`) never change threat directly.

[Trained creature gameplay](../Companions/Legends_Shattered_Frontier_Companion_Gameplay_Details_v0.1.md) §§5–9 specializes persistent captured pets: character-sourced math with a virtual natural implement, no wild encounter-tier multipliers, single/dual output profiles, owner-shared interrupt recovery and canonical target DR. Both creatures' damage/healing attributes to their owner once for reward credit; each retains its own threat entry. They add no party/reward/scaling slot. Slot withdrawal/storage cannot clear threat, refresh conditions or adopt wild evade immunity/healing. Gameplay Details §11 owns the proposed bounded Combat controls; coefficients and combination balance need targeted evidence before acceptance. Temporary magical summons retain their existing rules and do not become Stable creatures implicitly.

### PvP

Characters do not use threat. Taunt-flagged abilities applied to a character instead apply `taunt` crowd control (forces target selection for 1.5 s, `tune.combat.pvp_taunt_s`), subject to diminishing returns.

### Threat profiles

| `threat_profile` | Behavior |
| --- | --- |
| `standard` | rules above |
| `random_target` | ignores threat for target choice (uses stream `target`), threat still tracked for reset/credit |
| `fixate` | fixates one character chosen by the encounter script; threat ignored until fixate ends |
| `no_threat` | totems, objects; never attack |

---

## Kill credit and death

### Kill credit

For every NPC with `xp_eligible` or loot, the server keeps per-character contribution counters from engagement until death or evade (evade clears them):

```text
dmg_i  = damage landed on the NPC by character i (incl. pets/summons, absorbed by the NPC, excl. overkill)
heal_i = effective healing + absorbs by i on characters present on the NPC's threat table during the engagement
credited(i) ⇔ dmg_i ≥ KILL_CREDIT_SHARE × HP_npc_max
           ∨ heal_i ≥ KILL_CREDIT_SHARE × Σ_j heal_j   (and Σ heal > 0)
           ∨ (i in the same party or raid group as a credited character ∧ alive ∧ within KILL_CREDIT_RADIUS_M of the NPC at death)
KILL_CREDIT_SHARE = 0.10 (tune.combat.kill_credit_share); KILL_CREDIT_RADIUS_M = 60 (tune.combat.kill_credit_radius_m)
```

These implement the Character spec's Kill XP eligibility; `credited_character_ids` in `combat.npc.killed` lists the result in ascending ID order. `killing_character_id` is the character (or pet/summon owner) whose hit reduced health to 0; absent for NPC or environment kills. The server then calls `GrantKillXp` once per party (1–5 credited `xp_eligible` characters, idempotency key `<srvi_id>:<ent_id>:<character_id>`) and requests open-world loot through `reward-service` (Itemization spec, open-world kills).

Anti-tagging: a character whose `dmg_i` comes ≥ 90 % from a single hit while the NPC was below 10 % health does not satisfy the damage clause (`tune.combat.kill_steal_guard`); it may still be credited through party membership.

### Death

1. Health ≤ 0 → combat state `dead` (Part 3, Combat state machine); all effects without `persists_through_death` are removed; the character leaves every threat table; casts are cancelled.
2. The server emits `combat.character.died` (entry file, Combat events) with `killer_kind` `npc`, `player` or `environment`.
3. Death wear is reported through Itemization `ApplyWear` (`deaths` count; Itemization owns `DEATH_WEAR_FRACTION`).
4. Resurrection: `spirit_healer` (respawn point, 50 % health/mana, `tune.combat.spirit_res_fraction`), `player_ability` (ability-authored fraction, default 35 %), `encounter_reset` (full), `gm`. Each emits `combat.character.resurrected`. A resurrection request must be accepted by the dead player within 60 s (`tune.combat.res_accept_s`).
5. Displacement on respawn uses World teleport tokens with cause `respawn` / `resurrect`.

---

## PvP rules

| Rule | Value | Key |
| --- | --- | --- |
| Where PvP is allowed | World spec / Building spec territory rules decide; Security validates `combat_state` | — |
| Damage multiplier character → character | 1.0 | `tune.combat.pvp_damage_mult` |
| Healing multiplier on characters flagged `in_pvp_combat` | 0.85 | `tune.combat.pvp_healing_mult` |
| Crowd-control diminishing returns | shared with PvE (Part 3) | — |
| Level difference | no artificial scaling; natural formulas | — |
| Assist credit window | damage or hostile effect on the victim within 30 s before death | `tune.combat.pvp_assist_window_s` |
| `combat.pvp_kill.recorded` | one per character death in PvP; `killer_character_id` = killing blow (pet owner) | — |
| Honorless kills | victim killed by the same killer > 3 times in 10 min → event still recorded, `level_delta` unchanged, consumers (LiveOps, Analytics) dedupe | `tune.combat.pvp_repeat_kill_window_s` |

`in_pvp_combat` lasts 10 s after the last PvP damage or hostile effect (`tune.combat.pvp_combat_s`).

---

## Determinism and combat randomness

1. **Streams.** Each simulating server process seeds a PCG32 generator per attacking entity: seed = `SHA-256(server_seed | ent_id)` truncated, where `server_seed` comes from a CSPRNG at process boot and is never sent to clients. Inside encounters the instance-server instead uses the encounter's stream 4 `crit` (Dungeon spec, Determinism and seeding) for **NPC-side** rolls and per-character streams derived from `encounter_seed | character_id` for player rolls.
2. **Fixed draw count.** Five draws per hit (Random draws); DoT/HoT ticks are hits; procs draw from a separate `proc` sub-stream so that adding a proc never shifts hit rolls.
3. **Ordering.** Hits resolving on the same tick are processed in ascending `(ability_start_tick, attacker ent_id, ability sequence)` order.
4. **No client prediction of randomness.** Clients predict animations and estimated numbers only; displayed numbers come from server results.
5. **Replay scope.** Decision replays (Dungeon spec) compare outcomes (`hit_outcome`, target choices), not floating-point damage totals across builds; damage totals must match within 0.1 % on the same build.

---

## Balance checks by simulation

`lsf-combatmath` (Part 4) runs these checks on every tuning or content publish touching combat inputs; failures block the publish:

| Check | Pass condition |
| --- | --- |
| Reference throughput | `DPS_ref(L, 4L)` = `k_dps × H_ref(L)` ± 1 % for L 1–100 |
| Rotation budget | each published kit's sustained single-target MPS within ±5 % of `ROTATION_MPS_REF` (role-adjusted) |
| Rating level invariance | equal budget share → equal percentage ± 0.5 pp at L 20/50/100 |
| Common fight | reference damage dealer vs common same-level NPC ends with 35–45 % health at L 10/30/60/100 |
| Tank band | reference tank at the content's `ilvl_ref` in 50–70 % physical mitigation |
| Caps | no reachable build exceeds a Part 1 Soft Caps value before clamping by more than 2× (warns stacking content) |
| PvP TTK | 15–30 s for reference builds at L 30/60/100 |
