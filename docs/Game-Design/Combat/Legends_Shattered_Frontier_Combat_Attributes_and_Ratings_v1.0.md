# Legends of the Shattered Frontier — Combat: Attributes, Derived Stats, Ratings and Reference Build
### Version 1.0 — Expanded Technical Design Authority edition (2026-10-17)

Status: Design target; not implemented. Names per [Shared Registry v1.0](../Legends_Shattered_Frontier_Shared_Registry_v1.0.md).
Entry file: [Combat spec](../Legends_Shattered_Frontier_Combat_System_v0.1.md). Part 1 of 4.

This part defines what every attribute and rating *does*: the eight primary attributes, derived resources, skill effects on combat, the level-scaled rating system, haste, critical strikes, avoidance, penetration, luck, the movement modifier, the reference build, reference throughput, caps and balance targets. Damage resolution and NPCs are in Part 2 ([Damage, mitigation and NPCs](Legends_Shattered_Frontier_Combat_Damage_Mitigation_and_NPCs_v1.0.md)); abilities, effects and the runtime are in Part 3 ([Abilities, effects and runtime](Legends_Shattered_Frontier_Combat_Abilities_Effects_and_Runtime_v1.0.md)).

Conventions in this part:

- Fractions are written as decimals (0.05 = 5 %). Ratings, attributes and resources are integers; intermediate math is IEEE-754 double on the server, rounded only where a rule says so.
- `L` is the owner's level after level sync (Dungeon spec, Level sync), clamped to `LEVEL_CAP`.
- Every constant has a default and a tuning key in `config-service`; the key is authoritative at runtime (Registry, Constants).
- "Total" means natural (Character spec) + items (Itemization spec) + effects (Part 3), summed before any cap.

---

## Primary Attributes

Every entity (player, NPC, pet, summon, guard) carries the eight registry attributes `str`, `dex`, `con`, `int`, `wis`, `wil`, `cha`, `luck`.

```text
A_total = A_natural + A_items + A_effects
```

| Term | Owner | Notes |
| --- | --- | --- |
| `A_natural` | Character spec (growth, allocation, `ATTRIBUTE_MIN` 10, `ATTRIBUTE_NATURAL_CAP` 150, `ATTRIBUTE_TOTAL_NATURAL_CAP` 600) | NPCs: Character spec, NPC parity (generated profile). |
| `A_items` | Itemization spec (Stat budget; broken items contribute nothing) | Read from `ComputeEquipmentStats` snapshot (`stats_version`). |
| `A_effects` | Part 3 (effects with `stat_mod` operations) | Flat first, then percent (Attribute evaluation order). |

Totals are not capped. Each attribute-derived bonus that needs a ceiling states it. Totals below 0 are clamped to 0 for formula input.

### Strength

| Output | Formula | Cap | Consumer |
| --- | --- | --- | --- |
| Physical power | `PhysicalPower = 2 × str + physical_power (items/effects)` | — | Part 2, Damage formula |
| Block value bonus | `block_reduction = BLOCK_REDUCTION + min(0.10, max(0, str − 10) / 4000)` | 0.40 total | Part 2, block step |
| Gathering and construction efficiency | `str_eff = 1 + min(0.10, max(0, str − 10) / 1400)` (mining, logging, construction labor) | +10 % | Crafting spec, Building spec (exported value; consumer decides use) |

Itemization prices `physical_power` at 0.50 budget because 2 power ≙ 1 `str`.

### Dexterity

| Output | Formula | Cap | Consumer |
| --- | --- | --- | --- |
| Crit chance | `+ dex / 5000` (fraction; the v0.1 "Dexterity / 50 percentage points") | inside `CRIT_CAP` | Critical Strike Formula |
| Movement | Dexterity → movement speed | +0.05 | World spec |
| Harvest speed | `dex_eff = 1 + min(0.10, max(0, dex − 10) / 1400)` divides harvest channel time | +10 % | Crafting spec (exported value) |
| Stealth | exported as `dex` total; AI spec owns detection | — | AI spec |

Dexterity does **not** change attack speed directly; attack speed is haste (Haste). The v0.1 bullet "Attack Speed" is satisfied by mastery `percent_modifier` `attack_speed` (Part 3, Stat modifier keys), not by the attribute.

### Constitution

| Output | Formula | Consumer |
| --- | --- | --- |
| Max health | `Health = 100 + 15 × con + 25 × L + health (items/effects)` | Health Formula |
| Stamina | `+ 5 × con` | Stamina Formula |
| Health regeneration | Regeneration | — |
| Poison and disease resistance | `+ floor(con / 4)` to `poison` and `disease` | Part 2, Magic Resistance System |

### Intelligence

| Output | Formula | Consumer |
| --- | --- | --- |
| Mana | `Mana = 100 + 20 × int + 10 × L + mana (items/effects)` | Mana Formula |
| Spell power | `SpellPower = 2 × int + spell_power (items/effects)` | Part 2 |
| Crafting precision | exported `int` total | Crafting spec |

### Wisdom

| Output | Formula | Consumer |
| --- | --- | --- |
| Healing bonus | `heal_wis_mult = 1 + wis / 1000` | Part 2, healing formula |
| Mana regeneration | Regeneration | — |
| Magic resistance | `resist_all_from_wis = floor(wis / 2)` to every school | Part 2 |

### Willpower

| Output | Formula | Cap | Consumer |
| --- | --- | --- | --- |
| Crowd-control duration reduction | `wil_cc_red = min(0.15, max(0, wil − 10) / 1000)` | 0.15 (combined with items ≤ `CC_REDUCTION_CAP` 0.40) | Part 3, Crowd control and diminishing returns |
| Fear and charm duration | additional `× (1 − min(0.10, max(0, wil − 10) / 1400))` on `fear`, `charm` only | +10 % | Part 3 |
| Mental defense | Willpower is the defense attribute for `cc_category` `fear`, `charm`, `incapacitate` (no full-resist roll; "No Hard Immunities") | — | Part 3 |

### Charisma

Combat ratifies the AI spec's multiplier (AI spec, Faction service, Charisma effect) unchanged: up to +10 % on **positive** reputation deltas from `quest_reward`, `quest_choice`, `reward_grant` and `world_event`, computed from **natural** Charisma so gear swaps cannot farm reputation:

```text
cha_rep_mult = 1 + CHA_REP_MAX_BONUS × min(cha_natural, CHA_REP_REFERENCE) / CHA_REP_REFERENCE
CHA_REP_MAX_BONUS 0.10 (tune.faction.cha_rep_max_bonus); CHA_REP_REFERENCE 150 (tune.faction.cha_rep_reference)
```

Other Charisma outputs (exported values; the consuming spec decides whether and where to apply them):

| Output | Formula | Cap | Consumer |
| --- | --- | --- | --- |
| NPC vendor prices | `cha_price_mult = 1 − 0.05 × min(cha_natural, 150) / 150` on vendor buy prices only | −5 % | Economy spec (cross-spec request; never applies to player markets) |
| Leadership and guild buffs | `cha_aura_mult = 1 + 0.10 × min(cha_total, 150) / 150` on the **magnitude** of effects tagged `leadership_aura` | +10 % | Part 3, Party-scoped effects |

Example: natural Charisma 90 → `cha_rep_mult` 1.06 (AI spec example: delta 250 → 265), vendor prices × 0.97.

### Luck

Luck never changes combat crit, hit, dodge or damage. It changes rare drops, crafting and gathering critical success, crafting quality and treasure discovery through one curve:

```text
luck_factor(luck_total) = LUCK_MAX_INFLUENCE × min(luck_total, LUCK_REFERENCE) / LUCK_REFERENCE
LUCK_MAX_INFLUENCE = 0.10 (tune.combat.luck_max_influence)
LUCK_REFERENCE     = 150  (tune.loot.luck_reference)
```

`luck_total` is **effective** luck (natural + items + effects) for every consumer. `LUCK_REFERENCE` equals the natural attribute cap, so a naturally capped character reaches the full 10 % and luck from items only helps characters below the cap. Consumers apply `(1 + luck_factor)` as a multiplier on eligible probabilities or quality:

| Consumer | Applied to | Owner |
| --- | --- | --- |
| Loot | entries of rarity ≥ `rare` and `personal_extras` | Itemization spec, Loot tables |
| Crafting quality | `Q_base` (adds at most 10 % of earned quality) | Crafting spec, Quality |
| Crafting / gathering critical success | the authored crit-success chance | Crafting spec |
| Treasure discovery | discovery chance of hidden caches | AI / World specs |

Worked examples: luck 90 → 0.10 × 90/150 = **0.06**; luck 150 → 0.10; luck 210 → 0.10 (capped). A 2 % rare drop at luck 90 becomes 2.12 %.

### Attribute evaluation order

1. Natural values (progression snapshot) at the synced level.
2. Item flat attributes (only non-broken items, evaluated at `ilvl_eff` under level sync).
3. Effect flat modifiers (`stat_mod` with `op = add`).
4. Effect percent modifiers (`op = mul_pct`): `A = A_flat × (1 + Σ pct)`, Σ clamped to [−0.50, +0.50] per attribute.
5. Floor to integer, clamp at 0.

### Stat change propagation

Any change to an input (equip, effect apply/expire, level change, `progression.attribute.changed`) recomputes the dependent derived stats on the next simulation tick. Max health and max mana changes preserve the **current fraction** (`current = round(fraction × new_max)`), except that effects that grant max health as an absorb-style "temporary hit points" (Part 3) add current health equally.

---

## Derived resources

### Health Formula

```text
Health = 100 + 15 × con_total + 25 × L + health_items + health_effects
```

Level-100 example: `con` 150, no items → 100 + 2,250 + 2,500 = **4,850** (Character spec's level-100 example and `H_ref(100)`).

### Mana Formula

```text
Mana = 100 + 20 × int_total + 10 × L + mana_items + mana_effects
```

Example: `int` 150 at L100 → 100 + 3,000 + 1,000 = 4,100.

### Stamina Formula

```text
Stamina = 100 + 5 × con_total + 3 × dex_total + stamina_items
```

Stamina pays for physical abilities (authored `cost.stamina_pct`), `sprint` (World spec, Movement modes) and dodge-style mobility. Example: `con` 150, `dex` 43 → 100 + 750 + 129 = 979.

### Regeneration

| Resource | In combat (per second) | Out of combat (per second, after `REGEN_DELAY_S` 5 s) | Keys |
| --- | --- | --- | --- |
| Health | 0 (effects only) | `MaxHealth × 0.03 × (1 + con_total / 500)` | `tune.combat.health_regen_ooc_pct`, `tune.combat.regen_delay_s` |
| Mana | `Mana × (0.003 + wis_total × 0.00002)` | `Mana × 0.05` | `tune.combat.mana_regen_combat_base`, `tune.combat.mana_regen_wis_coef`, `tune.combat.mana_regen_ooc_pct` |
| Stamina | `10 + dex_total / 50` | `25 + dex_total / 25` | `tune.combat.stamina_regen_combat`, `tune.combat.stamina_regen_ooc` |

Regeneration ticks every 1 s on the simulation clock; haste never affects it. Example: Mana 4,100, Wisdom 150 → in combat 4,100 × 0.006 = 24.6 mana/s; out of combat 205 mana/s (full in 20 s).

Resting in a `rest_area` volume (World spec) doubles out-of-combat health and mana regeneration.

### Secondary statistics catalog

| Statistic | Source stat keys (Itemization stat-key catalog) | Formula | Part |
| --- | --- | --- | --- |
| Health, Mana, Stamina | `con`, `int`, `dex`, `health`, `mana`, `stamina` | Health/Mana/Stamina Formula | 1 |
| Armor | implicit armor + `bonus_armor` | Part 2, Armor System | 2 |
| Physical Power, Spell Power | `str`, `int`, `physical_power`, `spell_power` | Primary Attributes | 1 |
| Haste | `haste_rating` | Haste | 1 |
| Crit chance, Crit damage | `dex`, `crit_rating`, `crit_damage_rating` | Critical Strike Formula | 1 |
| Block, Parry, Dodge | `block_rating`, `parry_rating`, `dodge_rating` | Avoidance | 1 |
| Armor / Magic penetration | `armor_pen_rating`, `magic_pen_rating` | Armor Penetration | 1 |
| Resistances | `resist_<school>`, `resist_all`, `wis`, `con`, `skill/resist_magic` | Part 2 | 2 |
| Utility | `movement_speed_bp`, `cc_duration_reduction_bp`, `threat_mod_bp`, `lifesteal_bp` | Movement speed, Part 3, Part 2 Threat | 1–3 |

---

## Skill System

Inspired by Ultima Online. No classes; players improve by use. The canonical skill list is the Registry's (Canonical skills); growth, caps, locks, mastery and specializations are owned by the Character spec. This section owns how skill **values** change combat outcomes.

### Skill Progression

Range `0 → 1000` (`SKILL_MAX`). Bands: `novice` 0–99, `apprentice` 100–299, `journeyman` 300–499, `expert` 500–699, `master` 700–899, `legendary` 900–1000.

```text
SkillIncreaseChance = BaseRate / max(1, CurrentSkill / 100)
```

The `max(1, …)` guard removes the division by zero at skill 0 and the > 100 % chance below skill 100. The Character spec (Skills and Mastery part, Skill gain formula) owns the full gain model built on this term; zone-servers report practice through `SubmitPracticeBatch` (Part 3, Practice reporting).

### Skill effects on combat

Every combat ability names one **governing skill** (`ability.skill_id`, Part 3). For weapon abilities and auto-attacks it is the equipped weapon definition's `skill_id` (Itemization item definition); for spells `skill/magic`; for heals `skill/healing`; unarmed `skill/unarmed`.

```text
skill_ref(L)  = round(900 × (L / 100)^0.85)                       (Character spec, NPC parity; shared reference)
skill_mult    = SKILL_DAMAGE_FLOOR + (1 − SKILL_DAMAGE_FLOOR) × min(SKILL_RATIO_MAX, S / max(1, skill_ref(L)))
S_hit         = S + TACTICS_HIT_WEIGHT × S_tactics                (weapon attacks only; spells use S)
miss_chance   = clamp((D_def − S_hit) / MISS_DIVISOR, 0, MISS_MAX)
```

| Symbol | Meaning | Default | Tuning key |
| --- | --- | --- | --- |
| `S` | Effective value of the governing skill (Character spec effective value, after specialization caps) | — | — |
| `S_tactics` | Effective `skill/tactics` | — | — |
| `SKILL_DAMAGE_FLOOR` | Damage/healing multiplier at skill 0 | 0.80 | `tune.combat.skill_damage_floor` |
| `SKILL_RATIO_MAX` | Ratio cap; skill above 110 % of `skill_ref` gives nothing more | 1.10 | `tune.combat.skill_ratio_max` |
| `TACTICS_HIT_WEIGHT` | Tactics contribution to accuracy | 0.10 | `tune.combat.tactics_hit_weight` |
| `D_def` | Defender's defense skill: NPC = highest combat-track skill (Character spec, NPC parity, Derived difficulty values); player = max(skill of equipped weapon, `shield_defense` if a shield is equipped). Spells: target's `skill/resist_magic` (NPC: its offense skill ×0.75) | — | — |
| `MISS_DIVISOR` | Skill points per 1 % miss | 2,000 (→ 0.05 % per point) | `tune.combat.miss_divisor` |
| `MISS_MAX` | Miss ceiling | 0.30 | `tune.combat.miss_max` |

Additional skill effects:

| Skill | Effect |
| --- | --- |
| `shield_defense` | `block_chance += 0.10 × min(1, shield_defense / max(1, skill_ref(L)))` while a shield is equipped (inside the 0.75 block cap) |
| `resist_magic` | `+ floor(resist_magic / 10)` resistance to every magic school (0–100) |
| `tactics` | accuracy (above) and threat: `threat × (1 + 0.05 × tactics / 1000)` for tanks' taunt-flagged abilities only |
| `healing` | governing skill of heals: `skill_mult` on healing done |
| `magic` | governing skill of spells: `skill_mult`, spell hit (`miss_chance` vs `resist_magic`) |
| `stealth` | opener eligibility (`requires_stealth` abilities, Part 3); detection is AI spec |
| Any | ability unlock thresholds `ability.requirements.skill_min`, checked by the server (`combat_unknown_ability`) |

Worked examples:

1. Reference player at L30 (swordsmanship 323 = `skill_ref(30)`, tactics 242) vs a common L30 NPC (defense 323): `skill_mult` = 0.80 + 0.20 × 1.0 = **1.00**; `S_hit` = 323 + 24.2 = 347.2 → miss = clamp((323 − 347.2)/2000) = **0**.
2. Same player vs elite L31 (defense `skill_ref(31)` + 50 = 333 + 50 = 383): miss = (383 − 347.2)/2000 = **1.8 %**.
3. Fresh character (sword 50, tactics 0) at L10 (`skill_ref(10)` = 127) vs common L10 (defense 127): `skill_mult` = 0.80 + 0.20 × 0.394 = **0.879**; miss = (127 − 50)/2000 = **3.85 %**.
4. Legendary specialist at L100 (sword 1000, tactics 900) vs world boss (defense 900 + 150 = 1,050): `skill_mult` = 0.80 + 0.20 × min(1.10, 1.111) = **1.02**; `S_hit` = 1,090 → miss 0.

Skill can therefore move damage by −20 % … +2 % and accuracy by 0 … −30 %; specialization's main power comes from mastery nodes (Character spec, budget ≤ +12 % / +18 %), whose `rating` effects convert through the Combat Rating System, Haste, crit and avoidance sections.

---

## Combat Rating System

All ratings convert with diminishing returns:

```text
Percentage = Rating / (Rating + K_stat(L))
K_stat(L)  = K_stat_cap × s(L)
s(L)       = B(4 × L) / B(400)
```

| Variable | Meaning |
| --- | --- |
| `Rating` | total rating of that stat from items, mastery nodes and effects |
| `L` | rating owner's level after level sync, clamped to `LEVEL_CAP` at launch |
| `B(ilvl)` | Itemization base budget curve (Stat budget part, Base budget curve): `1 + 0.075 × ilvl` up to ilvl 400 |
| `s(L)` | level scale; `s(100) = 1`, `s(50) = 16/31 = 0.516`, `s(20) = 7/31 = 0.226`, `s(1) = 1.3/31 = 0.042` |

Because item budget grows with `B(4L)`, the same budget *share* yields the same percentage at every level. NPC ratings are 0 unless the NPC template authors them; NPC avoidance uses fixed values (Part 2, NPC combat profile).

### Pricing rule (consistency with Itemization)

Itemization prices every rating at **0.25 budget per point** (`crit_damage_rating` 0.30). The constants are chosen so equal **budget** buys equal value at the reference allocation: at level 100, 87.5 budget points (350 rating, about one secondary stat of a full epic kit) give **+20 % throughput or survival-equivalent** (`G_ref` = 0.20).

| Stat | Value of the percentage | Derivation of `K_stat_cap` |
| --- | --- | --- |
| Crit chance | +1 % damage per point (crit bonus 1.0 at 200 %) | `350 / (350 + K) = 0.20` → **1,400** |
| Haste | throughput `1 / (1 − h)`; with `h = R/(R+K)` throughput is exactly `1 + R/K` | `350 / K = 0.20` → **1,750** |
| Dodge, parry | 1 % fewer hits taken | same as crit → **1,400** |
| Block | blocked hits lose `BLOCK_REDUCTION` (30 %); shields also carry implicit armor outside the budget, so block is priced at half value | `350/(350+K) × 0.30 = 0.10` → **700** |
| Armor penetration | gain against reference target mitigation `d_ref` = 0.35 (Part 2, NPC mitigation): `(1 + k) / (k (1 − p) + 1)`, `k = d_ref / (1 − d_ref) = 0.5385` | `p = 0.476` gives 1.20 → `350/(350+K) = 0.476` → **385** |
| Magic penetration | same reference mitigation for resistances | **385** |
| Crit damage | gain `c_ref × Δmult`, `c_ref` = 0.30; 87.5 budget = 291.7 rating at 0.30 | `Δ = 0.667` → `291.7/(291.7+K) = 0.667` → **146** |

| Constant (at L 100) | Value | Tuning key |
| --- | --- | --- |
| `K_crit_cap` | 1,400 | `tune.combat.rating_k_crit` |
| `K_haste_cap` | 1,750 | `tune.combat.rating_k_haste` |
| `K_dodge_cap`, `K_parry_cap` | 1,400 | `tune.combat.rating_k_dodge`, `tune.combat.rating_k_parry` |
| `K_block_cap` | 700 | `tune.combat.rating_k_block` |
| `K_armor_pen_cap`, `K_magic_pen_cap` | 385 | `tune.combat.rating_k_armor_pen`, `tune.combat.rating_k_magic_pen` |
| `K_crit_damage_cap` | 146 | `tune.combat.rating_k_crit_damage` |

These replace the v0.1 dodge/block/parry constant of 500, which made avoidance 2.8× more valuable per budget point than crit.

### Level table of `K_stat(L)`

| L | `s(L)` | `K_crit`, `K_dodge`, `K_parry` | `K_haste` | `K_block` | `K_pen` | `K_crit_damage` |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 0.0419 | 58.7 | 73.4 | 29.4 | 16.1 | 6.1 |
| 10 | 0.1290 | 180.6 | 225.8 | 90.3 | 49.7 | 18.8 |
| 20 | 0.2258 | 316.1 | 395.2 | 158.1 | 86.9 | 33.0 |
| 30 | 0.3226 | 451.6 | 564.5 | 225.8 | 124.2 | 47.1 |
| 50 | 0.5161 | 722.6 | 903.2 | 361.3 | 198.7 | 75.4 |
| 75 | 0.7581 | 1,061.3 | 1,326.6 | 530.6 | 291.9 | 110.7 |
| 100 | 1.0000 | 1,400 | 1,750 | 700 | 385 | 146 |

Implementations compute `K_stat(L)` from the formula; the table is for checking.

### Worked examples

Level 100, full epic ilvl 460 kit, secondary budget ≈ 189 points = 757 rating: all-in crit → 757/2,157 = 35.1 %; all-in haste → h = 757/2,507 = 30.2 % (throughput +43 %); split 350 crit / 407 haste → crit 20.0 %, h = 18.9 % (+23 %). Level invariance holds against levelling gear: a level-100 ilvl-400 epic kit (secondary ≈ 140 budget = 560 rating) all-in crit gives 560/1,960 = 28.6 %; a level-50 ilvl-200 epic kit (≈ 72 budget = 289 rating) with `K_crit(50)` = 722 gives 289/1,011 = 28.6 %.

Level sync: a level-100 character synced to level 33 (`L_run` 30 + 3) evaluates items at `ilvl_eff = min(ilvl, 4 × 33 + 10)` = 142 **and** `K` at `s(33)` = 0.3516; the budget share, hence the percentage, stays near its level-appropriate value.

---

## Haste

```text
h_rating = haste_rating / (haste_rating + K_haste(L))
h        = min(HASTE_MAX, h_rating + Σ h_effects)
```

`h` is a **time-reduction fraction**. `attack_speed` modifiers (mastery `percent_modifier`, Part 3) add to `Σ h_effects` for the **auto-attack swing only**.

| Timer | Effective value |
| --- | --- |
| Cast time | `cast × (1 − h)` |
| Channel duration and tick interval | `× (1 − h)` (tick count unchanged) |
| Auto-attack swing | `speed_s × (1 − h_swing)`, `h_swing = min(HASTE_MAX, h + Σ attack_speed)` |
| Global cooldown | `gcd_eff = max(GCD_MIN, GCD_BASE × (1 − h))`; `GCD_BASE` 1.5 s (`tune.combat.gcd_base_s`), `GCD_MIN` 0.75 s (`tune.combat.gcd_min_s`); abilities may author a 1.0 s GCD (`gcd_class: short`), floored at the same `GCD_MIN` |
| Ability cooldown | `cd × (1 − h)` only when the ability's `haste_scaled_cooldown` is true. Default: true when base `cd ≤ 30 s`, false above (major defensives, interrupts' lockouts, resurrection, mobility). Content may set it false, never true for `cd > 30 s`. |
| Buff/debuff durations, DoT/HoT durations, resource regeneration, movement | not affected |

Security's validation formulas (Security spec, Threat model and server authority, Combat validation) use exactly this `h` for cooldowns and casts. The cooldown check applies `(1 − h)` only to abilities with `haste_scaled_cooldown = true`, and the GCD check must use `gcd_eff` (with the `GCD_MIN` floor), not `gcd × (1 − h)`.

### Haste maximum

`HASTE_MAX` = **0.75** (`tune.combat.haste_max`) caps `h` after summing ratings and effects; haste above the cap is discarded, never banked. With `GCD_MIN` the GCD reaches its floor at `h = 0.50` already, so haste above 0.50 benefits casts, channels, swings and haste-scaled cooldowns only.

Worked example: `h` = 0.30, ability `cd` 12 s (haste-scaled) → 8.4 s; GCD → max(0.75, 1.05) = 1.05 s; 2.5 s cast → 1.75 s. With `h` = 0.60 the GCD floors at 0.75 s; a 45 s defensive (`haste_scaled_cooldown` false) stays 45 s at any haste.

---

## Critical Strike Formula

```text
crit_chance = min(CRIT_CAP, CRIT_BASE + dex_total / 5000 + crit_rating / (crit_rating + K_crit(L)) + Σ crit_effects)
CRIT_BASE = 0.05 (tune.combat.crit_base);  CRIT_CAP = 0.60 (tune.combat.crit_cap)
```

Heals crit with the same chance. Damage-over-time and heal-over-time ticks roll crit per tick. Abilities flagged `can_crit: false` never crit (absorbs, environmental damage, encounter-authored NPC abilities by default).

### Critical Damage

```text
crit_damage = min(CRIT_DAMAGE_MAX, 2.0 + 1.0 × crit_damage_rating / (crit_damage_rating + K_crit_damage(L)) + Σ crit_damage_effects)
CRIT_DAMAGE_MAX = 3.0 (tune.combat.crit_damage_max)
```

Base 200 %, approaching 300 % asymptotically; effects may not push it above 3.0. NPC archetype attacks that can crit use `NPC_CRIT_DAMAGE` 1.5 (Part 2). PvP critical damage and healing use the same multiplier (no PvP crit reduction at launch).

Worked example: L100, `dex` 43, 350 crit rating, 100 crit damage rating → crit = 0.05 + 0.0086 + 0.200 = **0.2586**; crit damage = 2.0 + 100/246 = **2.407**; expected damage multiplier `1 + 0.2586 × 1.407` = **1.364**.

---

## Avoidance: Dodge, Parry and Block

### Dodge Formula

```text
DodgeChance = dodge_rating / (dodge_rating + K_dodge(L)) + Σ dodge_effects        hard cap 50 % (with parry)
```

### Parry Formula

```text
ParryChance = parry_rating / (parry_rating + K_parry(L)) + Σ parry_effects        (requires a melee weapon equipped)
dodge + parry ≤ AVOIDANCE_CAP = 0.50 (tune.combat.avoidance_cap)
```

When the sum exceeds the cap, both are scaled proportionally: `dodge' = dodge × cap / (dodge + parry)`.

### Block Formula

```text
BlockChance  = min(BLOCK_CAP, block_rating / (block_rating + K_block(L)) + shield_skill_bonus + Σ block_effects)   (shield equipped)
BLOCK_CAP    = 0.75 (tune.combat.block_cap)
block effect = blocked damage × (1 − block_reduction); BLOCK_REDUCTION = 0.30 (tune.combat.block_reduction), plus Strength bonus (Strength), total ≤ 0.40
```

Avoidance and block apply only to **physical** attacks from the defender's **front arc** (180°, `tune.combat.front_arc_deg`); spells are never dodged, parried or blocked unless the ability sets `blockable: true` (e.g., shield-reflectable projectiles). Rolls happen in pipeline order (Part 2, Resolution pipeline): miss → dodge → parry → crit → mitigation → block.

Worked example (tank, L100): 500 dodge, 600 parry, 700 block rating, `shield_defense` 900 (`skill_ref` 900), `str` 250 → dodge 500/1,900 = 0.263, parry 600/2,000 = 0.300 → sum 0.563 > 0.50 → scaled to 0.234 / 0.266; block = 700/1,400 + 0.10 = **0.60**; block reduction 0.30 + min(0.10, 240/4000 = 0.06) = **0.36**.

---

## Armor Penetration

```text
ArmorPen%      = min(ARMOR_PEN_CAP, armor_pen_rating / (armor_pen_rating + K_armor_pen(L)) + Σ pen_effects)
EffectiveArmor = Armor × (1 − ArmorPen%)
MagicPen%      = min(MAGIC_PEN_CAP, magic_pen_rating / (magic_pen_rating + K_magic_pen(L)) + Σ pen_effects)
EffectiveResistance = Resistance × (1 − MagicPen%)
ARMOR_PEN_CAP = MAGIC_PEN_CAP = 0.60 (tune.combat.armor_pen_cap, tune.combat.magic_pen_cap)
```

Penetration is a percentage of the target's armor/resistance (never flat), so it cannot reduce armor below 40 % of its value. Worked example: 350 armor pen rating at L100 → 350/735 = 47.6 % → target with 35 % reduction (`k` 0.5385) takes `1.5385 / (0.5385 × 0.524 + 1)` = **1.20×** damage.

---

## Movement speed

Combat owns the attribute and item term of movement; World owns base speeds, mounts, buff/debuff multipliers, hard caps and the authoritative `v_eff` (World spec, Authoritative movement).

### Dexterity → movement speed

```text
combat_move_mult = 1 + min(MOVE_DEX_CAP, max(0, dex_total − 10) / MOVE_DEX_DIVISOR)
                     + min(item movement_speed_bp, 1000) / 10000
```

`MOVE_DEX_CAP` 0.05 (`tune.combat.move_dex_cap`), `MOVE_DEX_DIVISOR` 3,000 (`tune.combat.move_dex_divisor`); the item cap is Itemization's `tune.item.cap.movement_speed_bp` (1,000 bp). Range [1.00, 1.15].

Rules:

1. Haste never affects movement.
2. `combat_move_mult` applies **on foot only**; mounted speed is World's mount speed and mount-flagged buffs. Item `movement_speed_bp` never applies to mounts.
3. Ability speed buffs and slows are **effects** (Part 3, `move_speed` operation) that World consumes as `u_i` (additive buffs) and `d_j` (strongest single slow) in `M_buff`; Combat never multiplies them into `combat_move_mult`.
4. The zone-server recomputes `combat_move_mult` whenever `dex_total` or the equipment snapshot changes and hands it to the World movement component (Part 3, Runtime contracts).

Example: Dexterity 130, items 300 bp → 1 + 0.04 + 0.03 = 1.07.

---

## Reference build

Balance, NPC scaling (Dungeon spec, Reference health; Raid spec) and the TTK checks (Security spec) use one reference character at level L. Agreed with the Character spec (Leveling, Attributes and Endgame part, Attribute balance targets):

- Natural attribute total = Character's median natural total `N_med(L)` (150 at L10, 210 at L25, 320 at L50, 450 at L75, 560 at L100; 120 at creation).
- Constitution receives a fixed share `s_con = (ATTRIBUTE_NATURAL_CAP − ATTRIBUTE_MIN) / (N_med(100) − 8 × ATTRIBUTE_MIN) = 140 / 480 = 0.2917` of points above the minimum, so the reference reaches the natural Constitution cap exactly at level 100: `C_med(L) = 10 + 0.2917 × (N_med(L) − 80)`.
- Health uses natural Constitution only; gear health is absorbed by content `dmg_coef` authoring and the reference DPS factor (Reference throughput).

Ratified closed form (used by every consumer):

```text
C_ref(L) = ref_con_base + ref_con_per_level × L      ref_con_base = 20, ref_con_per_level = 1.3
H_ref(L) = 100 + 15 × C_ref(L) + 25 × L  =  400 + 44.5 × L
```

Keys stay `tune.dungeon.ref_con_base` / `tune.dungeon.ref_con_per_level` (Dungeon reads them; Combat and Character co-own their values). Fit check against `C_med`:

| L | `N_med` | `C_med(L)` | `C_ref(L)` | Δ CON | Δ health (% of `H_ref`) |
| --- | --- | --- | --- | --- | --- |
| 1 | 120 | 21.7 | 21.3 | −0.4 | −0.1 % |
| 10 | 150 | 30.4 | 33.0 | +2.6 | +4.0 % |
| 25 | 210 | 47.9 | 52.5 | +4.6 | +4.6 % |
| 50 | 320 | 80.0 | 85.0 | +5.0 | +2.9 % |
| 75 | 450 | 117.9 | 117.5 | −0.4 | −0.2 % |
| 100 | 560 | 150.0 | 150.0 | 0 | 0 % (`H_ref(100)` = 4,850 = Character's level-100 example) |

The linear form slightly over-estimates mid-level health (≤ 5 CON), which makes NPC damage marginally harsher at levels 10–50; accepted because it keeps one continuous formula. If Character's median table changes, re-fit both keys so `|Δ health| ≤ 5 %` at every checkpoint.

### Reference offense profile

The reference build also fixes the offensive allocation used by Reference throughput (damage role; tank and healer variants scale by role factors):

| Input | Reference value |
| --- | --- |
| Main attribute (`str` melee, `int` caster) natural | `M_ref(L) = C_ref(L)` (reaches 150 at L100) |
| Other natural points | `(N_med(L) − 80 − 2 × (C_ref(L) − 10)) / 6` above the minimum each (Dexterity 43 at L100) |
| Gear | levelling `4 × L` rare full kit (Itemization `Σ slot_weight` 11.25); at cap the content's `ilvl_ref` (Dungeon spec) or `ilvl_entry` (Raid spec), epic above ilvl 400 |
| Gear budget split | 50 % main attribute, 15 % Constitution (not counted toward health, see above), 35 % secondary ratings split evenly between crit and haste |
| Weapon | two-hand of the gear ilvl and rarity (`hand_factor` 1.35), `speed_s` 3.4 |
| Skills | governing skill = `skill_ref(L)`, tactics = 0.75 × `skill_ref(L)` (`skill_mult` 1.0, miss 0 vs common) |
| Rotation | sustained single-target rotation at `ROTATION_MPS_REF` = 1.6 (Part 3, Throughput budget) |
| Target | equal-level NPC, armor class `medium` (35.06 % reduction), attacked from behind (no avoidance) |

Reference gear: levelling `4 × L` rare; at cap the Dungeon/Raid reference ilvl of the content.

---

## Reference throughput

Player damage and NPC health must share one scale. The Dungeon spec sizes NPC health from `H_ref` and a reference damage output `dps_ref(L, i) = k_dps × H_ref(L) × ilvl_dps_factor(i, L)` with `k_dps` = 0.04 (`tune.dungeon.ref_dps_fraction`): a reference damage dealer removes 4 % of a same-level reference character's health per second. Combat's raw formulas (Part 2, Damage formula) produce about 3× that at level 100, so every character-sourced damage and healing amount is multiplied by the **throughput scale** `σ(L)`:

```text
U_ref(L, i)  = (weapon_dps(i) + P_ref(L, i) / POWER_SPEED_REF) × ROTATION_MPS_REF × (1 + crit_ref × (2.0 − 1)) / (1 − h_ref) × (1 − d_ref)
σ(L)         = k_dps × H_ref(L) / U_ref(L, 4L)          (published per level as tuning table tune.combat.throughput_scale)
σ(L) ≈ 0.04 × (400 + 44.5 L) / (38.0 + 5.882 L)          (closed-form fit, error < 0.5 % for L 1–100)
DPS_ref(L, i) = σ(L) × U_ref(L, i)
```

| Symbol | Meaning | Default |
| --- | --- | --- |
| `weapon_dps(i)` | Itemization `WEAPON_DPS_K × B(i) × rarity_mult × 1.35` | — |
| `P_ref(L, i)` | `2 × (M_ref(L) + 0.5 × 11.25 × B(i) × rarity_mult)` | — |
| `POWER_SPEED_REF` | seconds of attack time that one point of power adds one damage to (Part 2) | 2.4 s (`tune.combat.power_speed_ref_s`) |
| `ROTATION_MPS_REF` | reference modifier-per-second of a sustained rotation | 1.6 (`tune.combat.rotation_mps_ref`) |
| `crit_ref`, `h_ref` | from the rating, haste and crit sections with the reference split | ≈ 0.20, 0.12 |
| `d_ref` | reference target mitigation | 0.35 (`medium`) |

Computed table (`lsf-combatmath`, rare `4L` gear):

| L | `H_ref` | `U_ref(L, 4L)` | `σ(L)` | `DPS_ref` | crit / h |
| --- | --- | --- | --- | --- | --- |
| 1 | 444.5 | 43.9 | 0.4047 | 17.8 | 0.201 / 0.122 |
| 10 | 845.0 | 96.6 | 0.3499 | 33.8 | 0.201 / 0.122 |
| 20 | 1,290.0 | 155.2 | 0.3326 | 51.6 | 0.202 / 0.122 |
| 30 | 1,735.0 | 213.8 | 0.3246 | 69.4 | 0.202 / 0.122 |
| 50 | 2,625.0 | 331.2 | 0.3170 | 105.0 | 0.203 / 0.122 |
| 75 | 3,737.5 | 478.6 | 0.3124 | 149.5 | 0.206 / 0.122 |
| 100 | 4,850.0 | 626.2 | 0.3098 | 194.0 | 0.207 / 0.122 |

### Gear factor at cap (canonical `DPS_ref(100, i)`)

| ilvl / rarity | `DPS_ref(100, i)` | Dungeon proxy `194 × 1.015^(i−400)` | Raid proxy `1500 × 1.006^(i−420)` |
| --- | --- | --- | --- |
| 400 rare | 194.0 | 194.0 | 1,330.9 |
| 405 epic | 227.7 | 209.0 | 1,371.3 |
| 415 epic | 239.5 | 242.5 | 1,455.8 |
| 420 epic | 245.7 | 261.3 | 1,500.0 |
| 430 epic | 258.8 | 303.2 | 1,592.5 |
| 445 epic | 280.2 | 379.1 | 1,742.0 |
| 450 epic | 287.9 | 408.4 | 1,794.9 |
| 460 epic | 304.0 | 474.0 | 1,905.5 |

Normative consequences (resolutions; the owning specs re-fit their keys):

1. **Combat's `DPS_ref(L, i)` is canonical** for every TTK, boss-health and TTK-floor calculation. Proxies in other specs must stay within ±10 % of it at every reference point.
2. Above ilvl 400 throughput grows ≈ **0.53 % per ilvl** (epic). The Dungeon factor `1.015^(i − 4L)` overstates gear: epic 460 relative to rare 400 is 304 / 194 = 1.57× in Combat's model but 2.44× in the proxy. Replace with `ilvl_dps_factor(i, L) = U_ref(L, i) / U_ref(L, 4L)` or the close approximation `(B(i) × rarity_mult) / B(4L)`; scalar fallback `tune.dungeon.ilvl_dps_step` = **1.0053**.
3. The Raid proxy `D0` = 1,500 is 6.1× Combat's value at ilvl 420. Re-fit `tune.raid.dps_ref_base` = **246** and `tune.raid.dps_ref_growth` = **1.0053**; raid boss `H_ref` values scale down by the same factor (e.g. the 20-player `normal` example 6.63 M → ≈ 1.09 M), keeping `T_target` unchanged.
4. Healing uses the same `σ(L)`. Reference healer sustained output `HPS_ref(L) = HPS_REF_FRACTION × H_ref(L)`, `HPS_REF_FRACTION` 0.06 (`tune.combat.hps_ref_fraction`, 291 HPS at L100) — the healer can sustain a target taking 6 % of `H_ref` per second.
5. PvP sanity (L100, two reference damage dealers in rare 400 leather, armor 2,232 → 18.2 %): `σ × U_pre-mit` = 298.5 → 244 DPS landed → **≈ 20 s** time-to-kill without healing (target band 15–30 s).

`σ(L)` is recomputed by `lsf-combatmath` whenever any input (`k_dps`, reference build keys, Itemization budget or weapon keys, rating constants) is republished; the publish pipeline rejects a tuning publish whose `σ` table is stale (Part 4, Invariants).

---

## Level Scaling

```text
Player levels 1–100 (LEVEL_CAP); expansion support 101+
```

Level enters combat through: Health (`25 × L`), Mana (`10 × L`), `s(L)` (ratings), `σ(L)`, `skill_ref(L)`, and attacker level in armor/resistance mitigation. Above `LEVEL_CAP`, `s(L)`, `C_ref(L)` and `σ(L)` are re-fitted by the expansion; formulas stay. Level sync replaces `L` everywhere in this list with the synced level.

Level difference effects (attacker level `L_a`, defender level `L_d`, `Δ = L_d − L_a`):

| Effect | Rule |
| --- | --- |
| Mitigation | automatic: armor/resistances of the defender scale with `L_d`, divisor with `L_a` |
| Accuracy | automatic through `skill_ref(L)` of NPC defense skill |
| Gray targets | `Δ ≤ −10`: no practice, no kill XP (Character spec), damage unchanged |
| Skull targets | `Δ ≥ +10` vs NPC: NPC `level_offset` cap; players cannot taunt skull bosses unless the ability is `taunt_ignores_level` |

---

## Soft Caps

| Cap | Value | Key |
| --- | --- | --- |
| Critical chance | 60 % | `tune.combat.crit_cap` |
| Critical damage | 300 % | `tune.combat.crit_damage_max` |
| Dodge + parry | 50 % | `tune.combat.avoidance_cap` |
| Block chance | 75 % | `tune.combat.block_cap` |
| Block reduction | 40 % | `tune.combat.block_reduction_max` |
| Haste | 75 % | `tune.combat.haste_max` |
| Armor / magic penetration | 60 % | `tune.combat.armor_pen_cap`, `tune.combat.magic_pen_cap` |
| Armor damage reduction | 85 % | `tune.combat.armor_dr_cap` |
| Resistance mitigation | 75 % | `tune.combat.resist_cap` |
| Crowd-control reduction | 40 % | `tune.combat.cc_reduction_cap` |
| Lifesteal | 6 % (600 bp, Itemization global cap) | `tune.item.cap.lifesteal_bp` |
| Threat modifier from items | ±20 % (±2,000 bp) | Itemization |
| Movement modifier | [1.00, 1.15] | Movement speed |
| Luck influence | 10 % | `tune.combat.luck_max_influence` |

Caps apply after summing **all** sources (natural, items, effects, mastery). A source that would exceed a cap is still applied to the sum; only the result is clamped.

---

## Endgame Balance Targets

| Role | Damage mitigation (armor + avoidance + block, vs equal-level physical) | Share of `DPS_ref` | Notes |
| --- | --- | --- | --- |
| Tank | 50–70 % | 0.3 (Dungeon `n_eq`) | plate, shield or parry builds |
| Damage | 15–25 % | 1.0 | |
| Support | 20–35 % | 0.6 | buffs counted at their throughput value |
| Healer | 10–20 % | 0.3 | `HPS_ref` 0.06 × `H_ref` |

| Target | Value | Measured by |
| --- | --- | --- |
| Same-level common NPC vs reference damage dealer, solo | player wins with 35–45 % health left; TTK 20–30 s | simulation, `m_ttk_vs_ref_ratio` |
| Same-level elite vs reference damage dealer, solo | player loses (needs 2–3 players) | simulation |
| Even PvP duel (reference builds, no healing) | TTK 15–30 s | `m_pvp_ttk_p50` |
| Share of characters at any stat cap | < 5 % at the start of a raid tier, < 25 % at its end | `m_stat_cap_saturation` |
| Crit chance p50 at cap in raid gear | 25–35 % | `m_crit_chance_p50` |
| Haste p50 at cap in raid gear | 15–25 % | `m_haste_p50` |

Worked mitigation check (Itemization data): full epic 460 plate = 8,656 armor → 46.4 % vs L100; with 0.234/0.266 avoidance and block 0.60 × 0.36 on remaining hits, a tank's expected physical damage taken per swing is `(1 − 0.50) × (1 − 0.464) × (1 − 0.60 × 0.36)` = 0.210 → **79 % mitigation**, above band: tanks reach the band without full avoidance stacking, which is the intended ceiling at `mythic` raid ilvl. At ilvl 420 with half the avoidance the same calculation gives ≈ 62 %.
