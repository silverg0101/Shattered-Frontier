# Legends of the Shattered Frontier — Itemization: Stat Budget, Affixes, Sets and Crafted Quality
### Version 1.0 — Expanded Technical Design Authority edition (2026-10-17)

Status: Design target; not implemented. Names per Shared Registry v1.0.

Entry file: [Itemization Architecture](../Legends_Shattered_Frontier_Itemization_Architecture_v1.0.md) · Registry: [Shared Registry](../Legends_Shattered_Frontier_Shared_Registry_v1.0.md)

---

## 1. Purpose

This part defines how much power an item may carry and how that power is turned into concrete stats: the item budget formula built on the registry power ladder, slot weights, rarity multipliers, the cost of each stat, base weapon damage and armor, the affix system (pools, weights, tiers, exclusivity, roll algorithm), item sets, and how crafted quality from `crafting-service` enters the same pipeline. All math here is implemented once in the shared deterministic library `lsf-itemmath` (see Agent implementation guide) and used by `inventory-service`, `loot-service`, the content validator, and tooling.

The Combat spec owns what stats *do* (attribute and rating conversions, damage, mitigation). This part owns only what stats an item *has*. Every stat named here is a Combat spec attribute or secondary statistic.

## 2. Item budget

### 2.1 Base budget curve `B(ilvl)`

`B` converts item level into "budget points". One budget point buys one primary attribute point (§4).

```text
B(ilvl) = BUDGET_BASE + BUDGET_LINEAR × ilvl                         for ilvl ≤ BUDGET_KNEE
B(ilvl) = B(BUDGET_KNEE) × BUDGET_ENDGAME_GROWTH ^ (ilvl − BUDGET_KNEE)  for ilvl > BUDGET_KNEE
```

| Variable | Meaning | Default | Tuning key |
| --- | --- | --- | --- |
| `ilvl` | item level of the instance, integer 0–475 at launch | — | — |
| `BUDGET_BASE` | budget at ilvl 0 | 1.0 | `tune.item.budget_base` |
| `BUDGET_LINEAR` | budget per ilvl while levelling | 0.075 | `tune.item.budget_linear` |
| `BUDGET_KNEE` | ilvl where the curve turns geometric (level-cap levelling ilvl, `4 × LEVEL_CAP`) | 400 | `tune.item.budget_knee` |
| `BUDGET_ENDGAME_GROWTH` | per-ilvl growth above the knee (+0.5 %/ilvl) | 1.005 | `tune.item.budget_endgame_growth` |

Rationale: levelling gear grows linearly with level (4 ilvl per level), so the level-scaling in Combat (`Level × 25` health, armor vs `100 × AttackerLevel`) stays aligned. At cap, each power-ladder step is a percentage step a player can feel (+5 ilvl ≈ +2.5 %).

Reference values (computed by `lsf-itemmath`, 3 decimals):

| Source (registry power ladder) | ilvl | `B(ilvl)` | Full kit, rare (Σw = 11.25) | Full kit, epic (×1.15) |
| --- | --- | --- | --- | --- |
| Level 5 quest | 20 | 2.500 | 28.1 | 32.3 |
| Level 20 quest | 80 | 7.000 | 78.8 | 90.6 |
| Level 50 quest | 200 | 16.000 | 180.0 | 207.0 |
| Level 100 quest | 400 | 31.000 | 348.8 | 401.1 |
| Dungeon `normal` | 405 | 31.783 | 357.6 | 411.2 |
| Dungeon `veteran` | 415 | 33.408 | 375.8 | 432.2 |
| Raid `normal` | 420 | 34.252 | 385.3 | 443.1 |
| Raid `veteran` | 430 | 36.003 | 405.0 | 465.8 |
| Raid `heroic` | 445 | 38.800 | 436.5 | 502.0 |
| Mythic dungeon M15+ / max crafted | 450 | 39.780 | 447.5 | 514.7 |
| Raid `mythic` | 460 | 41.814 | 470.4 | 541.0 |
| Legendary forged | 465 | 42.870 | — | — |
| Legendary max upgrade | 475 | 45.063 | — | — |

### 2.2 Item budget formula

```text
item_budget = B(ilvl) × slot_weight[slot] × rarity_mult[rarity] × set_factor
set_factor  = SET_ITEM_BUDGET_FACTOR if the definition belongs to an item_set, else 1.0
```

`item_budget` is stored as `numeric(10,3)` on the instance and never rounded; only final stat values are integers (§5.4). The roll pipeline guarantees `Σ cost(stat values) ≤ item_budget`.

### 2.3 Slot weights

Reference: `chest` = 1.0. Key: `tune.item.slot_weight.<equip_slot>`.

| `equip_slot` | Weight | Notes |
| --- | --- | --- |
| `main_hand` | 1.00 | one-hand weapons; stat budget only — weapon damage is separate (§3) |
| `off_hand` | 0.60 | shields, off-hand weapons, foci |
| `two_hand` | 1.60 | equals `main_hand + off_hand`; bows and staves are two-hand |
| `ranged` | 0.35 | thrown weapons, wands, relics used alongside melee or two-hand |
| `head`, `chest`, `legs` | 1.00 each | |
| `shoulders`, `hands`, `waist`, `feet` | 0.75 each | |
| `back` | 0.55 | always cloth for armor value |
| `neck`, `ring_1`, `ring_2` | 0.55 each | no armor, no durability |
| `trinket_1`, `trinket_2` | 0.55 each | usually carry `utility` affixes |
| `tool` | 0.50 | profession budget only (§4.3); never combat stats |

A full combat kit (main + off, or two-hand; plus ranged and 13 armor/accessory slots) sums to Σw = 11.25. Ring and trinket definitions are slot-family items: the definition says `ring` or `trinket`; the instance may sit in either numbered slot.

### 2.4 Rarity multipliers and affix shape

| `rarity` | `rarity_mult` | Primary affixes / share | Secondary affixes / share | Utility affixes / share | Tuning key prefix |
| --- | --- | --- | --- | --- | --- |
| `common` | 0.70 | 1 / 1.00 | 0 / 0 | 0 / 0 | `tune.item.rarity.common.*` |
| `uncommon` | 0.85 | 1 / 0.70 | 1 / 0.30 | 0 / 0 | `tune.item.rarity.uncommon.*` |
| `rare` | 1.00 | 2 / 0.60 | 2 / 0.40 | 0 / 0 (see note) | `tune.item.rarity.rare.*` |
| `epic` | 1.15 | 2 / 0.55 | 2 / 0.35 | 1 / 0.10 | `tune.item.rarity.epic.*` |
| `legendary` | 1.30 | 2 / 0.50 | 3 / 0.35 | 1 / 0.15 | `tune.item.rarity.legendary.*` |
| `mythic` | 1.45 | reserved — not obtainable at launch; validator rejects definitions | | | `tune.item.rarity.mythic.*` |

Note (rare utility): with probability `RARE_UTILITY_CHANCE` (0.20, `tune.item.rare_utility_chance`) a rare item replaces its second secondary affix with one utility affix that takes 0.10 of the budget; the remaining secondary takes 0.30.

Legendary items additionally carry one authored **legendary power** (a fixed utility affix from the `legendary_path` definition) that is *outside* the budget — its strength is balanced by hand and reviewed per path. At most `LEGENDARY_EQUIPPED_MAX` = 1 legendary may be equipped (`tune.item.legendary_equipped_max`).

`rarity_ceiling` from the power ladder clamps any rarity produced by loot or crafting (Loot and Rewards part, Loot tables).

## 3. Weapon damage and armor (outside the budget)

Base weapon damage and armor are *implicit* properties that scale with ilvl but do not consume budget, because every item of that type has them.

### 3.1 Weapon damage

```text
weapon_dps   = WEAPON_DPS_K × B(ilvl) × rarity_mult × hand_factor
avg_hit      = weapon_dps × speed_s
min_hit      = round(avg_hit × (1 − WEAPON_SPREAD))
max_hit      = round(avg_hit × (1 + WEAPON_SPREAD))
```

| Variable | Default | Tuning key |
| --- | --- | --- |
| `WEAPON_DPS_K` | 4.0 | `tune.item.weapon_dps_k` |
| `hand_factor` | `main_hand` 1.00, `two_hand` 1.35, `off_hand` weapon 0.50, `ranged` 0.60 | `tune.item.hand_factor.<slot>` |
| `speed_s` | authored on the definition, 1.4–3.8 s | — |
| `WEAPON_SPREAD` | 0.15 | `tune.item.weapon_spread` |

`WeaponDamage` in the Combat spec damage formula is one sample from `[min_hit, max_hit]` drawn by the simulating server.

### 3.2 Armor

```text
armor = round(ARMOR_K × B(ilvl) × armor_slot_weight[slot] × material_factor[material] × rarity_mult)
```

`armor_slot_weight` equals `slot_weight` for armor slots and 0 for `neck`, rings, trinkets. Shields use `off_hand` weight × 2.0 with material `plate`.

| Variable | Default | Tuning key |
| --- | --- | --- |
| `ARMOR_K` | 30 | `tune.item.armor_k` |
| `material_factor` | `cloth` 0.25, `leather` 0.40, `mail` 0.60, `plate` 1.00 | `tune.item.material_factor.<material>` |

Check against Combat targets (`DamageReduction = Armor / (Armor + 100 × AttackerLevel)`, attacker level 100): full epic ilvl 460 plate (armor slots Σ 6.0) = 8,656 armor → 46.4 % before shields, block and talents, inside the Combat "Tank: 50–70 %" band once block is added; full epic leather = 3,462 → 25.7 %; cloth = 2,164 → 17.8 %, inside the "DPS 15–25 %" band.

## 4. Stat cost weights

`cost_weight` is budget points per one unit of the stat. Stats are stored as integers in the unit shown. Key: `tune.item.stat_cost.<stat>`.

### 4.1 Combat stats

| Stat key | Unit | Cost weight | Derivation / note |
| --- | --- | --- | --- |
| `str`, `dex`, `con`, `int`, `wis`, `wil`, `cha` | attribute point | 1.00 | reference unit |
| `luck` | attribute point | 2.00 | scarce; drop and crafting influence capped by `LUCK_MAX_INFLUENCE` |
| `physical_power` | point | 0.50 | Combat: `PhysicalPower = Strength × 2` → 2 PP ≙ 1 str |
| `spell_power` | point | 0.50 | parity with physical power |
| `health` | point | 0.0667 (1/15) | Combat: 1 con ≙ 15 health |
| `mana` | point | 0.05 (1/20) | Combat: 1 int ≙ 20 mana |
| `stamina` | point | 0.20 | Combat: 1 con ≙ 5 stamina |
| `bonus_armor` | point | 0.10 | added to implicit armor |
| `crit_rating`, `haste_rating`, `dodge_rating`, `parry_rating`, `block_rating`, `armor_pen_rating`, `magic_pen_rating` | rating point | 0.25 | 4 rating per budget point; Combat converts rating with `Rating / (Rating + ScalingConstant)` |
| `crit_damage_rating` | rating point | 0.30 | Combat caps crit damage at 300 % |
| `resist_<school>` (fire, frost, nature, shadow, arcane, holy, poison, disease) | point | 0.50 | |
| `resist_all` | point | 3.00 | all eight schools; cheaper than 8 × 0.5 = 4.0 by design |

### 4.2 Utility stats

Utility stats are stored in basis points (bp, 1 bp = 0.01 %) or seconds.

| Stat key | Unit | Cost weight | Hard cap per item | Global cap (Combat or Itemization) |
| --- | --- | --- | --- | --- |
| `movement_speed_bp` | bp | 0.020 | 300 bp | 1,000 bp from items (`tune.item.cap.movement_speed_bp`) |
| `cc_duration_reduction_bp` | bp | 0.025 | 400 bp | 2,500 bp |
| `threat_mod_bp` | bp | 0.015 | 500 bp | ±2,000 bp |
| `lifesteal_bp` | bp | 0.040 | 200 bp | 600 bp |
| `gold_find_bp` | bp | 0.030 | 300 bp | 1,000 bp; not on raid gear |
| `durability_loss_reduction_bp` | bp | 0.010 | 1,500 bp | 3,000 bp |
| `proc_<effect>` | authored effect | authored `budget_cost` | one per item | Combat validates effect |

### 4.3 Profession stats (tools)

Tools have `tool` budget only and may roll only profession stats. Tools never carry combat stats; combat items never carry profession stats except `utility` affixes flagged `profession_allowed`.

| Stat key | Unit | Cost weight |
| --- | --- | --- |
| `skill_bonus_<skill>` (gathering or crafting skill) | skill point (0–1000 scale) | 0.40 |
| `harvest_speed_bp` | bp | 0.020 |
| `yield_bonus_bp` | bp | 0.050 |
| `craft_quality_bonus` | quality point (0–100 scale, see §8) | 1.50 |

Skill bonuses from items are counted *after* Character spec skill caps only as an effective-skill bonus; they never raise stored skill values and never count toward `GLOBAL_SKILL_CAP`.

## 5. Affixes

### 5.1 Affix definition (content)

Affix kinds are the registry `affix_kind`: `primary`, `secondary`, `utility`, `set`. Content kind `affix`, ID `affix/<slug>`.

```json
{
  "id": "affix/of_the_bear",
  "kind": "primary",
  "display_name_key": "affix.of_the_bear.name",
  "position": "suffix",
  "stats": [ { "stat": "con", "split": 0.6 }, { "stat": "str", "split": 0.4 } ],
  "allowed_categories": ["weapon", "armor", "accessory"],
  "allowed_slots": ["*"],
  "slot_weight_mods": { "neck": 0.5 },
  "min_ilvl": 1,
  "min_rarity": "common",
  "base_weight": 100,
  "exclusivity_group": "primary_con",
  "tags": ["tank", "melee"],
  "tiers": [
    { "tier": 1, "min_ilvl": 1,   "name_key": "affix.of_the_bear.t1" },
    { "tier": 2, "min_ilvl": 120, "name_key": "affix.of_the_bear.t2" },
    { "tier": 3, "min_ilvl": 240, "name_key": "affix.of_the_bear.t3" },
    { "tier": 4, "min_ilvl": 360, "name_key": "affix.of_the_bear.t4" },
    { "tier": 5, "min_ilvl": 420, "name_key": "affix.of_the_bear.t5" }
  ]
}
```

Rules:

- `split` values sum to 1.0. Multi-stat affixes share one budget slice.
- **Tiers** are presentation and gating, not extra power: the tier shown is the highest tier whose `min_ilvl ≤ ilvl`. A tier may set `min_rarity` and `extra_weight_mult` to gate or favour specific variants (e.g., a proc variant only from tier 4 epic). Power always comes from `item_budget`.
- `utility` affixes may carry `budget_fraction_override` (0.05–0.20) when a fixed effect needs a fixed cost; the validator checks the effect's `budget_cost` against it.
- `set` affixes are never rolled; they are attached by the item definition's `item_set_id` (§7).

### 5.2 Pools and weights

An **affix pool** is the set of candidate affixes for one roll step, computed, not authored:

```text
pool(kind, item) = { a ∈ affixes(content_version) |
    a.kind = kind
    ∧ item.category ∈ a.allowed_categories ∧ (item.slot ∈ a.allowed_slots ∨ "*" ∈ a.allowed_slots)
    ∧ a.min_ilvl ≤ item.ilvl ∧ rarity_rank(item.rarity) ≥ rarity_rank(a.min_rarity)
    ∧ a.exclusivity_group ∉ groups_already_on(item)
    ∧ a ∉ item.definition.excluded_affixes }

weight(a, item) = a.base_weight × slot_weight_mods[a][item.slot] (default 1.0)
                  × affinity(a, item.definition.stat_affinity) × tier.extra_weight_mult (default 1.0)
affinity = AFFINITY_BOOST (3.0, tune.item.affix_affinity_boost) if a.tags ∩ definition.stat_affinity ≠ ∅ else 1.0
```

Definitions may also declare `affix_pool_override` (explicit list) for curated items such as raid gear, which skips the computed pool but keeps exclusivity and budget checks.

### 5.3 Exclusivity groups

At most one affix per `exclusivity_group` per item. Canonical groups at launch (extensible by content): `primary_<attribute>` per attribute, `secondary_<rating>` per rating, `movement`, `on_hit_proc`, `on_kill_proc`, `lifesteal`, `threat`, `find_bonus`, `cc_reduction`. Implicit affixes (§5.4 step 2) reserve their groups first.

### 5.4 Roll algorithm

Input: `definition`, `ilvl`, `rarity` (already clamped to ceiling), `roll_floor` (default `ROLL_FLOOR` = 0.92, `tune.item.roll_floor`; crafted items raise it, §8), and an RNG stream (Loot and Rewards part, Deterministic seeded rolls). Output: ordered list of affix rolls with integer stat values.

```text
1. budget ← item_budget(definition, ilvl, rarity)
2. implicit ← definition.implicit_affixes (fixed list, each with fixed budget_fraction f_i)
   reserve budget_impl = Σ f_i × budget; remaining_budget ← budget − budget_impl
   mark their exclusivity groups as used
3. shape ← rarity shape (§2.4), apply RARE_UTILITY_CHANCE with one RNG draw if rarity = rare
   subtract implicit affixes of each kind from that kind's count (never below 0)
4. for kind in [primary, secondary, utility]:
     share_k ← shape.share[kind] × remaining_budget
     n_k ← shape.count[kind]
     if pool(kind) has fewer than n_k candidates: move the unused share to the next kind
         (utility → secondary → primary); record "pool_exhausted" diagnostic
     for j in 1..n_k:
       a ← weighted_pick_without_replacement(pool(kind), weights)   # 1 RNG draw
       slice ← share_k / n_k   (or definition.split_override)
       r ← uniform(roll_floor, 1.0)                                  # 1 RNG draw
       for each (stat, split) in a.stats:
         value ← floor(slice × split × r / cost_weight(stat))
         value ← min(value, per_item_cap(stat))
       mark a.exclusivity_group used
5. conformance check: Σ value × cost_weight ≤ budget (always true by construction; assert)
6. persist: affix id, tier, stat values, r (as integer per-mille), draw indexes
```

Properties:

- **Deterministic:** given `(definition, content_version, ilvl, rarity, roll_floor, seed)` the output is identical on every service and tool. The draw order above is normative; adding draws breaks replay and requires a new `roll_algorithm_version` (stored on the instance, starting at 1).
- **Never over budget:** floors and `r ≤ 1.0` guarantee it. Average utilisation ≈ 96 % at `ROLL_FLOOR` 0.92 minus rounding loss.
- **Weighted pick:** cumulative-weight scan over pool sorted by affix ID (stable order), one uniform draw in `[0, Σw)`.

### 5.5 Rerolls and modification

Affix reroll services (e.g., an inscription "reforge" recipe) are owned by Crafting for the cost; `inventory-service` executes the reroll with a new RNG stream keyed by the crafting request ID, replaces exactly one non-implicit affix from the same `kind` pool (excluding the current affix), keeps all others, and emits `item.instance.modified`. The original `item_budget`, `ilvl`, and `rarity` never change in a reroll.

## 6. Worked examples

All examples use defaults. Example item IDs are illustrative.

**Example A — raid mythic epic plate chest (`item/ashen_warplate_chest`, ilvl 460).**
`B(460) = 31 × 1.005^60 = 41.814`. `item_budget = 41.814 × 1.0 × 1.15 = 48.087`.
Primary share 0.55 → 26.447, two slices of 13.224. Secondary 0.35 → 16.830, two slices of 8.415. Utility 0.10 → 4.809.

| Affix | Stat | r | Value | Cost |
| --- | --- | --- | --- | --- |
| `affix/mighty` | str | 0.97 | floor(13.224 × 0.97 / 1.0) = 12 | 12.00 |
| `affix/of_the_bear`* | con | 0.94 | floor(13.224 × 0.94) = 12 | 12.00 |
| `affix/keen` | crit_rating | 0.99 | floor(8.415 × 0.99 / 0.25) = 33 | 8.25 |
| `affix/of_parrying` | parry_rating | 0.93 | floor(8.415 × 0.93 / 0.25) = 31 | 7.75 |
| `affix/fleet` | movement_speed_bp | 1.00 | floor(4.809 / 0.02) = 240 | 4.80 |
| implicit | armor | — | round(30 × 41.814 × 1.0 × 1.0 × 1.15) = 1,443 | outside budget |

\*single-stat variant for clarity. Total cost 44.80 ≤ 48.087 (93.2 % utilisation).

**Example B — dungeon normal rare ring (ilvl 405).** `item_budget = 31.783 × 0.55 × 1.0 = 17.481`. Primary 0.60 → slices 5.244; secondary 0.40 → slices 3.496. Rolls: dex r=0.96 → 5; wil r=0.92 → 4; haste_rating r=0.98 → floor(3.496 × 0.98 / 0.25) = 13; health r=0.95 → floor(3.496 × 0.95 × 15) = 49. Cost 5 + 4 + 3.25 + 3.267 = 15.52.

**Example C — level 5 common one-hand sword (ilvl 20, speed 2.6 s).** `item_budget = 2.5 × 1.0 × 0.70 = 1.75`; one primary at r=0.95 → str 1. `weapon_dps = 4 × 2.5 × 0.70 × 1.0 = 7.0`, `avg_hit = 18.2`, hit range 15–21.

**Example D — crafted epic two-hand greatsword.** Crafter smithing 950, crafting-service reports `quality_bonus` 38. `ilvl = floor(950 / 1000 × 400) + 38 = 418`, `B(418) = 33.912`, `item_budget = 33.912 × 1.6 × 1.15 = 62.398`. `roll_floor = 0.92 + 0.08 × 38/50 = 0.9808`. Slices: primary 17.159, secondary 10.920, utility 6.240. Rolls: str r=0.99 → 16, con r=0.985 → 16, crit_rating r=0.995 → 43, armor_pen_rating r=0.982 → 42, utility as rolled. `weapon_dps = 4 × 33.912 × 1.15 × 1.35 = 210.6`.

**Example E — legendary vs best epic two-hand.** Epic ilvl 460: `41.814 × 1.6 × 1.15 = 76.94`. Legendary 465: `42.870 × 1.6 × 1.30 = 89.17` (+15.9 %, plus legendary power). Upgraded 475: `45.063 × 1.6 × 1.30 = 93.73` (+5.1 % over 465).

## 7. Item sets

Content kind `item_set`, ID `item_set/<slug>`.

```json
{
  "id": "item_set/frostforged_regalia",
  "pieces": ["item/frostforged_helm", "item/frostforged_pauldrons", "item/frostforged_hauberk",
             "item/frostforged_gauntlets", "item/frostforged_greaves", "item/frostforged_sabatons"],
  "reference_ilvl": 445,
  "reference_rarity": "epic",
  "bonuses": [
    { "pieces": 2, "affix": "affix/set_frostforged_2" },
    { "pieces": 4, "affix": "affix/set_frostforged_4" },
    { "pieces": 6, "affix": "affix/set_frostforged_6" }
  ],
  "max_ilvl_spread": 30
}
```

Set math:

```text
set item budget factor:  SET_ITEM_BUDGET_FACTOR = 0.92            (tune.item.set_item_budget_factor)
set bonus budget at k pieces:
  SB(k) = SET_BONUS_EFFICIENCY × (1 − SET_ITEM_BUDGET_FACTOR) × k × B(reference_ilvl) × SET_AVG_SLOT_WEIGHT × rarity_mult(reference_rarity)
  SET_BONUS_EFFICIENCY = 1.25 (tune.item.set_bonus_efficiency), SET_AVG_SLOT_WEIGHT = 0.75 (tune.item.set_avg_slot_weight)
incremental bonus at threshold k_i: SB(k_i) − SB(k_{i−1})
```

Worked example (`reference_ilvl` 445 epic): `SB(2) = 1.25 × 0.08 × 2 × 38.800 × 0.75 × 1.15 = 6.693`, `SB(4) = 13.386`, `SB(6) = 20.079`. The 2-piece bonus could be +26 crit_rating (6.693 / 0.25 = 26.8 → 26). A full set returns 25 % more than the 8 % it taxed — sets are worth wearing but never mandatory.

Rules: set bonus affixes are authored with fixed values; the validator checks `Σ cost ≤ SB(k_i) − SB(k_{i−1})` or, for proc effects, the authored `budget_cost`. Pieces whose ilvl differs from the highest equipped piece by more than `max_ilvl_spread` still count (no punishment for mixed upgrades). Set counting is computed by `inventory-service` `ComputeEquipmentStats` and sent to the simulating server; the client never asserts set counts.

## 8. Crafted-item quality integration

Crafting owns recipes, quality factors (skill, tool, materials, workshop tier), and the quality result. Itemization owns what the result means for the item.

### 8.1 Contract with `crafting-service`

`crafting-service` calls `inventory-service.CreateCraftedItem` (Item model part, gRPC) with:

| Field | Type | Range / rule |
| --- | --- | --- |
| `recipe_id` | content ID | must output `item_definition_id` |
| `item_definition_id` | content ID | category in `weapon`, `armor`, `accessory`, `tool`, `consumable`, `siege_equipment`, `container`, `material` |
| `crafter_character_id` | `chr_` | |
| `crafter_skill_value` | int | `S_item = min(S, D + MARGIN_FULL)` (Crafting spec, Quality → item level; `MARGIN_FULL = 200`); effective skill at craft time from progression-service, including tool bonus |
| `quality_bonus` | int | 0–50 (registry crafted ilvl formula) |
| `quality_score` | int | 0–100, crafting-service raw score (`quality_bonus = floor(quality_score / 2)` is the Crafting default; inventory validates `quality_bonus ≤ 50`) |
| `rarity` | rarity | ≤ `epic`; `legendary` only when called by a legendary path forging stage |
| `locked_affixes` | list of affix IDs | 0–1 secondary from catalyst reagents; must be in pool |
| `material_lots` | list of `{item_id, quantity}` | provenance only; inventory consumes them in the same transaction |
| `beneficiary_character_id` | `chr_` | optional; legendary path beneficiary (Crafting spec, Legendary forge) |
| `legendary_path_id` | `lgp_` | optional; required for legendary-path forging (Crafting spec, Legendary forge) |
| `idempotency_key` | string | crafting request ID |

### 8.2 Crafted ilvl and roll floor

```text
crafted_ilvl = min(CRAFTED_ILVL_MAX, floor(crafter_skill_value / 1000 × 400) + quality_bonus)   CRAFTED_ILVL_MAX = 450
roll_floor   = ROLL_FLOOR + (1 − ROLL_FLOOR) × quality_bonus / 50
```

`CRAFTED_ILVL_MAX` mirrors the registry ladder (0–450) and is read from `tune.item.crafted_ilvl_max`. Crafted items record `crafted_by_character_id` and `quality_score` in provenance; the crafter's name is shown in the tooltip (privacy class `p1_pseudonymous`, opt-out via character setting).

Crafted vs dropped at the same ilvl: identical budget; crafted items have a higher `roll_floor` and optional locked secondary, dropped items have access to curated pools and sets. This keeps crafting competitive up to ilvl 450 per the Economy principle "most items crafted by players".

Worked example: see §6 Example D. A journeyman smith (skill 400, `quality_bonus` 10) makes ilvl 170 gear with `roll_floor` 0.936 — on par with level-42 quest rewards.

### 8.3 Consumables and non-budget categories

`consumable`, `material`, `token`, `quest_item`, `cosmetic`, and `container` items have no stat budget. Consumable potency is defined by Crafting (recipe) and Combat (effect). Containers define `capacity` only. `siege_equipment` uses Building spec stats; its durability follows the Item model part with category base values.

## 9. Validator rules (content publish)

`content-catalog-service` runs the `lsf-itemmath` validator before publishing a pack. It rejects:

1. Any item definition with rarity `mythic`, or with `max_rarity` above the source's ladder ceiling where the definition declares a source.
2. Affix stats not in §4, `split` not summing to 1.0 ± 0.0001, or `min_ilvl` above 475.
3. Exclusivity conflicts within implicit affixes.
4. Set bonus cost above its incremental `SB` slice.
5. Pools that cannot fill the rarity shape for any declared `(slot, ilvl, rarity)` combination (warning, not error, if the definition uses `affix_pool_override`).
6. Combat items with profession stats, or tools with combat stats.
7. Legendary powers without a `budget_review` sign-off field.
