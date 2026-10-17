# Legends of the Shattered Frontier — Crafting: Salvage and Economic Balancing
Version 1.0 — Expanded Technical Design Authority edition (2026-10-17)

Design target; not implemented. Names per Shared Registry v1.0.

Entry file: [Resource and Crafting Architecture](../Legends_Shattered_Frontier_Resource_and_Crafting_Architecture_v1.0.md) · Registry: [Shared Registry](../Legends_Shattered_Frontier_Shared_Registry_v1.0.md)

This part makes crafting computable as an economy: static reference values for every material, the crafted-value function `C_equiv` that prices an item of any ilvl/slot/rarity in materials, the recipe cost validator, salvage profiles that meet Itemization's return targets, dungeon material tables, crafting gold sinks, time-to-craft, and the balancing targets with metrics and alerts. Economy owns currency balance and the inflation index; Itemization owns vendor value and repair cost; this part owns the item-side material economy and how crafted goods relate to those numbers.

---

## 1. Reference values

### 1.1 Why static values

Every validation, salvage computation and fee uses **reference values** (`ref_value_copper`) published with content, never market prices. Market prices are manipulable, regional and lagging; a roll or payout that depends on them would turn market manipulation into an item faucet (entry file C7 #8). Market prices are compared against reference values only in analytics (§9), and reference values are retuned between content versions when they drift.

### 1.2 Raw resources

| `resource_tier` | `ref_value_copper` | Tuning key |
| --- | --- | --- |
| `common` | 120 | `tune.crafting.ref_value.common` |
| `uncommon` | 600 | `tune.crafting.ref_value.uncommon` |
| `rare` | 2,000 | `tune.crafting.ref_value.rare` |
| `epic` | 6,000 | `tune.crafting.ref_value.epic` |
| `legendary` | 18,000 | `tune.crafting.ref_value.legendary` |

Calibration: tier value × focused units per hour ≈ the combat gold faucet at the tier's typical level (Gathering part, Gathering balancing targets). The common value matches the Analytics/Economy illustrative basket price of iron ore (120 copper).

### 1.3 Derived values

```text
ref_value(output) = round_half_up( Σ_i qty_i × ref_value(input_i) × REFINE_VALUE_MULT / output_quantity )     refine, component, structure_material
```

`REFINE_VALUE_MULT` 1.15 (`tune.crafting.refine_value_mult`) is the labor premium; it is a valuation, not a faucet (no currency or item is created by it). Derived values are computed bottom-up by the content validator in topological order of the material graph (cycles are rejected) and stored per `content_version` in `crafting.material_values` (Data part). For a material produced by several recipes, the **minimum** derived value is used (the cheapest way to make it bounds what it is worth).

### 1.4 Authored reagents

Reagents that are not made from gathered inputs (salvage-only essences, dungeon shards, gems from bonus outputs, vendor reagents) carry authored values:

| Material | Tier | Sources | `ref_value_copper` |
| --- | --- | --- | --- |
| `item/flux`, `item/vial`, `item/thread_spool`, `item/salt` | common | `vendor_stock` | 20–60 (equals vendor price) |
| `item/rough_quartz` | uncommon | mining bonus output | 1,500 |
| `item/prismatic_dust` | rare | salvage (jewelry, ilvl 301–475) | 3,000 |
| `item/arcane_ink` | rare | inscription component | derived (≈ 3,000) |
| `item/sapphire` | rare | mining bonus | 5,000 |
| `item/tempered_flux` | epic | smithing component from salvage scraps | derived (≈ 9,000) |
| `item/flawless_ruby` | epic | mining bonus, salvage | 15,000 |
| `item/ember_essence` | epic | salvage of ilvl > 400, world events | 40,000 |
| `item/abyssal_shard` | epic | dungeon material tables at cap | 60,000 |
| `item/prismatic_core` | legendary | world events, raid salvage | 250,000 |

Validator: an authored value must lie within 0.5–3.0 × its tier's raw value × `REFINE_VALUE_MULT^k` for the shortest crafting depth `k` at which an equivalent material appears, or carry a `value_review` sign-off (like Itemization's legendary `budget_review`).

---

## 2. Crafted-value function `C_equiv`

### 2.1 Definition

```text
C_equiv(ilvl, slot, rarity) = max( VENDOR_FLOOR_MULT × vendor_value(ilvl, slot, rarity),
                                   CRAFT_VALUE_W × gold_reward(L_eq, 1) × slot_weight[slot] × rarity_mult[rarity] × E(ilvl_rel) )

L_eq      = clamp(ceil(ilvl / 4), 1, LEVEL_CAP)
ilvl_rel  = ilvl − ilvl_tier_offset(t_current)                         (Registry, Power ladder seasonal offset)
E(x)      = 1 + ENDGAME_COST_RAMP × clamp((x − 400) / 5, 0, 1) + ENDGAME_COST_SLOPE × max(0, x − 405)
```

| Variable | Default | Tuning key / owner |
| --- | --- | --- |
| `vendor_value` | Itemization `vendor_value(item)` at full durability (`VENDOR_K × B(ilvl) × slot_weight × rarity_mult`) | Itemization (`tune.item.vendor_k`) |
| `VENDOR_FLOOR_MULT` | 2.5 | `tune.crafting.vendor_floor_mult` |
| `CRAFT_VALUE_W` | 0.25 | `tune.crafting.craft_value_weight` |
| `gold_reward(L, 1)` | `round(100 × (1 + L)^1.5)` | Registry, Shared reward scaling functions |
| `slot_weight`, `rarity_mult` | Itemization, Stat budget (chest 1.0, two_hand 1.6, ring 0.55, tool 0.50; common 0.70 … epic 1.15) | Itemization |
| `ENDGAME_COST_RAMP` | 30 | `tune.crafting.endgame_cost_ramp` |
| `ENDGAME_COST_SLOPE` | 1.0 per ilvl above 405 | `tune.crafting.endgame_cost_slope` |
| `ilvl_tier_offset(t)` | `15 × (t − 1)` | Registry (`tune.item.tier_ilvl_step`) |

Result is floored to an integer copper value. `C_equiv` is computed by `lsf-craftmath`, which imports `B(ilvl)`, `vendor_value`, slot weights and rarity multipliers from `lsf-itemmath` (no formula implemented twice).

Design intent:

- Levelling: a full rare crafted kit (Σ slot weight 11.25) at level L costs `11.25 × 0.25 × gold_reward(L,1) = 2.81 × gold_reward(L,1)` in materials, ≈ 0.375 h of the Itemization faucet model (`7.5 × gold_reward(L,1)` per hour). Levelling gear is replaced every ≈ 1.2 h (Itemization gear pace), so crafted levelling gear is affordable but not free.
- Endgame: the ramp makes ilvl 405+ items cost real effort (≈ 12 h of faucet for a 405 rare kit, ≈ 19 h for a 418 epic kit), matching Itemization's "Dungeon veteran / crafted +20–25 h; time, gold" pace. The seasonal offset keeps the ramp anchored at each tier's entry, so last tier's top crafted gear becomes cheap when the new tier opens.
- The vendor floor guarantees `vendor_value ≤ 0.4 × C_equiv`: crafting-and-vendoring is never a gold faucet.

### 2.2 Reference values of `C_equiv` (chest, slot weight 1.0)

| ilvl | Rarity | `L_eq` | `gold_reward(L_eq,1)` | `E` | Formula term | Vendor floor (2.5 × vendor) | `C_equiv` (copper) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 40 | uncommon | 10 | 3,648 | 1 | 775 | 1,020 | 1,020 |
| 80 | uncommon | 20 | 9,623 | 1 | 2,044 | 1,785 | 2,044 |
| 160 | rare | 40 | 26,253 | 1 | 6,563 | 3,900 | 6,563 |
| 240 | rare | 60 | 47,643 | 1 | 11,910 | 5,700 | 11,910 |
| 320 | rare | 80 | 72,900 | 1 | 18,225 | 7,500 | 18,225 |
| 400 | rare | 100 | 101,504 | 1 | 25,376 | 9,300 | 25,376 |
| 405 | rare | 100 | 101,504 | 31 | 786,656 | 9,532 | 786,656 |
| 418 | epic | 100 | 101,504 | 44 | 1,284,025 | 11,697 | 1,284,025 |
| 450 | epic | 100 | 101,504 | 76 | 2,217,862 | 13,722 | 2,217,862 |
| 460 | epic | 100 | 101,504 | 86 | 2,509,686 | 14,425 | 2,509,686 |

Full-kit equivalents (× 11.25): ilvl 405 rare ≈ 885 g (11.6 h of the 76.13 g/h cap faucet), ilvl 418 epic ≈ 1,445 g (19.0 h), ilvl 450 epic ≈ 2,495 g (32.8 h). In gathering terms (epic gathering ≈ 66 g/h of reference value), the 418 kit is ≈ 22 h of gathering.

### 2.3 Worked example

Greatsword of entry file C2.5 at its produced ilvl 418, epic, two-hand: formula term `25,376 × 1.6 × 1.15 × 44 = 2,054,441` (`0.25 × 101,504 = 25,376`); vendor value `floor(120 × 33.912 × 1.6 × 1.15) = 7,487`, floor term 18,717 → `C_equiv = 2,054,441` copper (205 g).

---

## 3. Recipe cost validator

### 3.1 Rules

For `gear` and `tool` recipes, at content publish:

```text
S_ref    = min(SKILL_MAX, D + 100)                                 (an on-band crafter past the gain optimum)
ilvl_ref = min(CRAFTED_ILVL_MAX, floor(S_ref / 1000 × 400) + 25)   (median quality bonus)
C_mat    = Σ_i qty_i × ref_value(input_i)                          (all slots, including non-quality inputs)
require  0.75 ≤ C_mat / C_equiv(ilvl_ref, output.slot, output.base_rarity) ≤ 1.25     (tune.crafting.recipe_cost_band)
require  vendor_value(output at CRAFTED_ILVL_MAX of the recipe, rarity_max) ≤ C_mat / VENDOR_FLOOR_MULT
```

The second check uses the recipe's best possible output (top `S_item`, `quality_bonus` 50, `rarity_max`) so no outcome can be vendored for more than 40 % of its inputs. `consumable` and `structure_material` recipes are value-checked only for `ref_value(output) × output_quantity ≥ C_mat` (no value created) and the vendor rule on their explicit vendor values. Repair kits additionally satisfy `C_mat ∈ [0.35, 0.55] × KIT_CAPACITY(tier)` (Recipes part, Repair kits).

### 3.2 Examples

**Iron longsword** (`D` 120, base `common`, main hand): `S_ref = 220` → `ilvl_ref = 88 + 25 = 113` → `L_eq = 29`, `gold_reward = 100 × 30^1.5 = 16,432`; formula term `0.25 × 16,432 × 1.0 × 0.70 = 2,875`; vendor floor `2.5 × floor(120 × 9.475 × 0.70) = 2.5 × 795 = 1,987` → `C_equiv = 2,875`. A first draft with 14 iron ingots (138) + 2 leather strips (138) = 2,208 passes the band (0.768) but **fails** the vendor check: the best output (ilvl 178, `B(178) = 14.35`) with `rarity_max = rare` vends for `floor(120 × 14.35 × 1.0) = 1,722 > 2,208 / 2.5 = 883`. The published recipe therefore sets `rarity_max = common` (best vendor value `floor(120 × 14.35 × 0.70) = 1,205`) and uses 20 iron ingots + 2 strips: `C_mat = 2,760 + 276 = 3,036`, band ratio `3,036 / 2,875 = 1.056` ✓, vendor `1,205 ≤ 3,036 / 2.5 = 1,214` ✓. Lesson encoded by the validator: low-difficulty gear recipes have a wide `S_item` window, so they stay `common` or become material-heavy.

**Emberforged greatsword** (`D` 850, base `rare`, two-hand): `S_ref = 950` → `ilvl_ref = 380 + 25 = 405` → `C_equiv = 25,376 × 1.6 × 1.0 × 31 = 1,258,649`. Recipe (Recipes part §1.1): 6 steel ingots (2,714) + 15 ember essence (40,000) + 12 abyssal shards (60,000) + 2 flawless rubies (15,000) = `16,284 + 600,000 + 720,000 + 30,000 = 1,366,284` → ratio 1.086 ✓. Vendor check: best output ilvl 450 epic → `120 × 39.780 × 1.6 × 1.15 = 8,783` ≤ `1,366,284 / 2.5` ✓.

The Recipes part catalogue row for the iron longsword reflects the published version; the validator is the authority, catalogue rows are illustrative.

---

## 4. Salvage profiles

### 4.1 Targets (Itemization spec, Item Model and Lifecycle part, Salvage)

Expected material value returned = 25–40 % of the recipe input value for crafted items, 15–25 % for dropped items, × 0.5 when `broken`. Legendary items cannot be salvaged. Salvage outputs use source `salvage_output`; the salvaged item uses sink `salvage_input` (inventory-service, one transaction).

### 4.2 `salvage` profile (content kind registered in Shared Registry, Content ID kinds)

```json
{
  "id": "salvage/metal_weapon",
  "skill_id": "skill/smithing",
  "bands": [
    { "ilvl_rel_max": 100, "outputs": [ { "definition_id": "item/iron_ingot", "share": 0.85 },
                                        { "definition_id": "item/rough_quartz", "share": 0.15 } ] },
    { "ilvl_rel_max": 200, "outputs": [ { "definition_id": "item/silver_steel_ingot", "share": 0.85 },
                                        { "definition_id": "item/rough_quartz", "share": 0.15 } ] },
    { "ilvl_rel_max": 300, "outputs": [ { "definition_id": "item/steel_ingot", "share": 0.80 },
                                        { "definition_id": "item/sapphire", "share": 0.20 } ] },
    { "ilvl_rel_max": 400, "outputs": [ { "definition_id": "item/mithril_ingot", "share": 0.80 },
                                        { "definition_id": "item/sapphire", "share": 0.20 } ] },
    { "ilvl_rel_max": 475, "outputs": [ { "definition_id": "item/ember_essence", "share": 0.85 },
                                        { "definition_id": "item/flawless_ruby", "share": 0.15 } ] }
  ]
}
```

Rules: shares per band sum to 1.0 ± 0.0001; outputs are material items with reference values; `skill_id` is the salvager skill for the band position `t`. Launch profiles:

| Profile | Skill | Item families | Endgame band outputs |
| --- | --- | --- | --- |
| `salvage/metal_weapon` | smithing | swords, axes, maces, polearms, daggers | ember essence, flawless ruby |
| `salvage/metal_armor` | smithing | mail, plate, metal shields | ember essence, tempered flux scraps |
| `salvage/leather_armor` | tailoring | leather | ember essence, frost wyrm scale |
| `salvage/cloth_armor` | tailoring | cloth, back | ember essence, ember lotus fiber |
| `salvage/wood_weapon` | carpentry | bows, staves, wooden shields | ember essence, frostbark resin |
| `salvage/jewelry` | jewelcrafting | rings, necklaces, trinkets | prismatic dust, ember essence |
| `salvage/tool` | smithing | tools | band-matching ingots |
| `salvage/siege` | engineering | siege equipment | construction materials (Building may override) |

Itemization item definitions reference these IDs (`salvage_profile`); an item definition without a profile cannot be salvaged (`SALVAGE_NOT_ALLOWED`).

### 4.3 `ComputeSalvage` algorithm (normative)

```text
input: item (definition, ilvl, rarity, slot, status, provenance{recipe_id?, quality_score?}), character_id, idempotency_key
1. reject if rarity = legendary, category = material, or no salvage_profile               → SALVAGE_NOT_ALLOWED
2. crafted = provenance.recipe_id present and recipe published in any retained content version
   V_in    = crafted ? C_mat(recipe at the content_version recorded on the item) : C_equiv(ilvl, slot, rarity)
3. S       = salvager effective skill in profile.skill_id (progression-service GetProgression)
   D_salv  = min(SKILL_MAX, 2 × ilvl_rel)
   t       = clamp((S − D_salv + MARGIN_LOW) / (MARGIN_LOW + MARGIN_FULL), 0, 1)
4. [r_lo, r_hi] = crafted ? SALVAGE_RETURN_CRAFTED (0.25, 0.40) : SALVAGE_RETURN_DROPPED (0.15, 0.25)
   r       = (r_lo + (r_hi − r_lo) × t) × (status = broken ? SALVAGE_BROKEN_MULT (0.5) : 1)
5. V_out   = r × V_in
6. band    = first profile band with ilvl_rel ≤ ilvl_rel_max
   for each output j in band order:
       x_j = V_out × share_j / ref_value(output_j)
       q_j = floor(x_j) + bernoulli(frac(x_j))          RNG seeded by hash(idempotency_key, server_secret), draws in order
7. lot_quality = crafted ? floor(provenance.quality_score / 2) : SALVAGE_DROPPED_LOT_QUALITY (25)
8. return [{definition_id, quantity = q_j, lot_quality}] omitting q_j = 0
```

Properties (tested, Data part): `E[Σ q_j × ref_value_j] = V_out` exactly; `r` is always inside the Itemization band; the call is pure (no writes), so inventory-service can retry it inside its own transaction and get identical outputs for the same `idempotency_key`; no practice record is produced (salvage never trains skill, closing craft → salvage skill loops).

The skill term uses the same margins as crafting quality: a salvager at `S = D_salv + 200` gets the top of the band, one at `D_salv − 100` or below gets the bottom. `D_salv = 2 × ilvl_rel` makes levelling items (ilvl ≤ 400) top-band for a crafter of skill `2 × ilvl + 200`, and endgame items (ilvl 405+, `D_salv` 810+) top-band only for specialized crafters (≥ 1000 at ilvl 400+ is capped by `SKILL_MAX`; `D_salv` 1000 at ilvl ≥ 500 only).

### 4.4 Worked examples

**Example 1 — crafted greatsword.** The entry file's greatsword (ilvl 418, `quality_score` 77), salvaged by its smith (`S` 950).

```text
V_in   = C_mat(recipe/emberforged_greatsword) = 1,366,284
D_salv = min(1000, 836) = 836 ; t = (950 − 836 + 100) / 300 = 0.7133
r      = 0.25 + 0.15 × 0.7133 = 0.3570 ; V_out = 487,763
band 401–475: ember essence x = 487,763 × 0.85 / 40,000 = 10.365 → 10 + bernoulli(0.365)
              flawless ruby x = 487,763 × 0.15 / 15,000 = 4.878  → 4 + bernoulli(0.878)
lot_quality = floor(77 / 2) = 38
```

Return 35.7 % of input value (inside 25–40 %); the craft → salvage loop loses 64.3 %.

**Example 2 — dropped rare ring.** Ring ilvl 405 rare (Itemization vendor example, vendor value 2,097), salvaged by a jewelcrafter at `S` 500.

```text
V_in   = C_equiv(405, ring 0.55, rare) = max(2.5 × 2,097, 25,376 × 0.55 × 1.0 × 31) = 432,660
D_salv = 810 ; t = clamp((500 − 810 + 100)/300, 0, 1) = 0 ; r = 0.15 ; V_out = 64,899
band 401–475 (salvage/jewelry): prismatic dust 0.7 → 15.143 ; ember essence 0.3 → 0.487
lot_quality = 25
```

Salvage yields ≈ 31 × the vendor value: at endgame, players salvage replaced drops (Itemization target: salvage 35–55 % of exits, epic ≥ 40 % salvaged or vendored within 60 days).

**Example 3 — broken levelling drop.** Uncommon chest ilvl 80 (`C_equiv` 2,044), broken, salvaged by a tailor at `S` 300: `D_salv` 160, `t = (300 − 160 + 100)/300 = 0.8`, `r = (0.15 + 0.10 × 0.8) × 0.5 = 0.115`, `V_out = 235` → linen cloth (derived 276) `x = 235 × 0.85 / 276 = 0.72` → 0 or 1 cloth; thread `x = 235 × 0.15 / 40 = 0.88`. Vendor value of the broken chest is 0 for durability-scaled vendor value (Itemization: × durability fraction) — salvage is the only exit for broken low-level gear, as intended.

### 4.5 Vendor vs salvage by level (dropped, `t` = 0.5)

| Item | `C_equiv` | Salvage value (`r` 0.20) | Vendor value | Better exit |
| --- | --- | --- | --- | --- |
| ilvl 40 uncommon chest | 1,020 | 204 | 408 | vendor |
| ilvl 160 rare chest | 6,563 | 1,313 | 1,560 | vendor (close) |
| ilvl 320 rare chest | 18,225 | 3,645 | 3,000 | salvage |
| ilvl 400 rare chest | 25,376 | 5,075 | 3,720 | salvage |
| ilvl 460 epic chest | 2,509,686 | 501,937 | 5,770 | salvage |

Expected blended salvage share of exits: 25–35 % while levelling, 70–90 % at cap; population-weighted 40–55 % (inside Itemization's 35–55 % target). If `m_salvage_share` (Itemization) leaves the band, tune `SALVAGE_RETURN_DROPPED` within its 15–25 % range first, then the vendor floor.

---

## 5. Dungeon and raid material tables

The Dungeon spec assigns "1 roll of `loot/dungeon_mats_<biome>_<band>` per boss" with "Crafting spec owns tables". Crafting authors these tables in Itemization's loot-table format (Itemization owns the format and rolls them in loot-service):

```text
target_value_per_roll = DUNGEON_MATS_W × gold_reward(L_band, 1) × diff_mult
L_band    = band upper level (band b → 10 × b)
diff_mult = story 0.5, normal 1.0, veteran 1.5, mythic 1.5 + 0.1 × min(M, 15)
```

`DUNGEON_MATS_W` 0.4 (`tune.crafting.dungeon_mats_weight`).

| Band | Tier composition (by value share) | Target value per roll (normal) |
| --- | --- | --- |
| 1–2 | 100 % `common` biome resources | 1,459 (b1) – 3,849 (b2) |
| 3–4 | 70 % `uncommon`, 30 % `common` | 6,904 – 10,501 |
| 5–7 | 70 % `rare`, 30 % `uncommon` | 14,569 – 23,930 |
| 8–9 | 70 % `epic`, 30 % `rare` | 29,160 – 34,723 |
| 10 (cap) | 40 % `epic` resources, 60 % epic reagents (`abyssal_shard`, `ember_essence`) | 40,602 (normal) / 60,902 (veteran) / up to 121,804 (mythic 15) |

Table structure: one weighted pick among entries `{definition_id, quantity: {min, max}}` where each entry's expected value `E[qty] × ref_value ≈ target_value_per_roll`; e.g. band 10 normal: `abyssal_shard` qty 0–1 at 60 % weight (`E` ≈ 0.68 shards ≈ 40,600). Luck applies per Itemization loot rules. Example: a 4-boss veteran run at cap yields ≈ `4 × 60,902 = 243,608` copper of materials per player (≈ 24 g), and ≈ 2.4 abyssal shards — so an emberforged greatsword's 12 shards are ≈ 5 veteran runs, linking endgame crafting to group content without making crafting a raid-only activity.

Raid material tables are Raid-owned (`loot/raid_mats_<raid>`); recommended target 1.5 × the dungeon veteran value at the raid's difficulty, plus `prismatic_core` at ≤ 2 % per final boss kill per player.

---

## 6. Crafting gold sinks

| Sink | Code | Formula | Example |
| --- | --- | --- | --- |
| Recipe training | `training_cost` (economy-service `Debit` by crafting-service) | `TRAINER_COST_W × gold_reward(L_eq(D), 1) × npc_price_mult` (Recipes part, Trainer price) | `D` 450 → 31,199 copper |
| Public workshop fee | `npc_service` | `min(PUBLIC_FEE_CAP, ceil(PUBLIC_FEE_RATE × C_mat))` for `D ≥ PUBLIC_FEE_MIN_D` at public workshops; per unit in a batch | `C_mat` 18,000 → 180 copper |

Defaults: `PUBLIC_FEE_RATE` 0.01, `PUBLIC_FEE_MIN_D` 300, `PUBLIC_FEE_CAP` 50,000 copper (`tune.crafting.public_fee_rate`, `public_fee_min_d`, `public_fee_cap_copper`). Player workshops charge no crafting-service fee (settlement upkeep is Building's sink).

Target: crafting gold sinks are 2–5 % of gold created at cap (`m_crafting_gold_sink_share`), alongside Itemization's 18–25 % (repair, legendary upgrade, destroy) and Economy's remaining sinks. Crafting's primary sink is item-side (materials consumed), not gold.

Item-side sinks owned by Crafting: `crafting_consumed`, `crafting_failure`, `salvage_input` (the salvaged item), and `consumable_used` (kits, scrolls; Shared Registry, Currency sources and sinks). Target: material sink value ≥ 90 % of material faucet value per realm per week (§9).

---

## 7. Time-to-craft and time-to-gear

| Activity | Target | Derivation |
| --- | --- | --- |
| Refine a 20-unit batch | 48 s | `20 × 3 × 0.8` |
| Levelling gear piece (craft) | 11–13 s | `10 + D/100` |
| Levelling gear piece (materials, self-gathered) | 3–10 min of gathering | `C_equiv(ilvl, chest, rare) / tier gathering income` (e.g. ilvl 160: 6,563 / 132,000 per h ≈ 3 min) |
| Endgame gear piece (craft) | 18–20 s | `10 + D/100`, D 800–1000 |
| Endgame gear piece (materials) | 1.5–3.5 h gathering-equivalent | ilvl 418 two-hand epic `C_mat` ≈ 1.37–2.05 M / 660,000 per h |
| Full crafted kit ilvl 405 rare | ≈ 12 h of faucet / 13 h of gathering | §2.2 |
| Full crafted kit ilvl 418 epic | ≈ 19 h of faucet / 22 h of gathering | §2.2; Itemization "+20–25 h" |
| Crafting skill 0 → 700 | 40–60 active hours | Character spec, Skill balancing targets |
| Crafting skill 0 → 900 | 75–110 active hours | Character spec |
| Net material cost 0 → 900 in one crafting skill, after selling or salvaging outputs at median prices | 800–1,500 g | validated by `crafting-sim` (Data part); ≈ 10–20 % of gold earned over the same play time |

Craft channel time is deliberately short compared with material acquisition: crafting is gated by materials, recipes and workshops, not by waiting. `m_craft_time_share` (share of a crafter's active time in craft channels) should stay below 35 %.

---

## 8. Market value targets for crafted goods

| Relation | Target | Rationale |
| --- | --- | --- |
| Median market price of crafted gear / `C_mat` at reference values (`m_crafted_margin_ratio`) | 1.2–1.8 | crafters earn a margin for skill, workshop access and time |
| Vendor value / `C_mat` (any crafted output) | ≤ 0.40 (validator) | crafting-to-vendor is never a gold faucet |
| Salvage value / `C_mat` (crafted) | 0.25–0.40 | Itemization target; loop loss ≥ 60 % |
| Median market price of a raw resource / its reference value | 0.7–1.5 per tier | reference values stay meaningful; outside 0.5–2.0 for 14 days → retune `ref_value.<tier>` next content version |
| Crafted share of equipped items, levelling characters | 35–55 % | Economy principle "most items crafted by players" |
| Crafted share of equipped items ilvl ≥ 405 (`m_crafted_ilvl_share`) | weeks 1–4 of a raid tier 30–45 %, week 12: 15–30 % | crafted gear bridges into raids and is then superseded |
| Share of crafted endgame items that are epic | 40–60 % | `RARITY_STEP_Q1` 60 reachable with good setups |

---

## 9. Balancing targets, metrics and alerts

All metrics are defined in the Analytics warehouse from `modeled.fact_crafting`, `modeled.fact_gathering`, `modeled.fact_item_events`, `fact_market_sales` and the reference-value snapshot exported per content version.

| Metric | Definition | Target | Alert |
| --- | --- | --- | --- |
| `m_craft_success_rate` | succeeded / (succeeded + failed) by recipe and `d` bucket | on-band (`d` 0…+100) 0.80–0.95 | < 0.70 for a recipe with ≥ 500 attempts |
| `m_quality_score_distribution` | percentiles of `quality_score` by skill band and recipe tier | on-band median 45–65; share ≥ 90 ≤ 3 % | share ≥ 90 > 6 % (stacking exploit or tuning error) |
| `m_crafted_ilvl_share` | §8 | §8 | outside band 14 days |
| `m_crafted_margin_ratio` | §8 | 1.2–1.8 | < 1.0 or > 2.5 for 7 days |
| `m_salvage_return_ratio` | Σ ref value of salvage outputs / Σ `V_in`, by origin | crafted 0.25–0.40, dropped 0.15–0.25 (× 0.5 broken) | any realm outside → bug (sev2) |
| `m_crafting_material_sink_value` | ref value of `crafting_consumed` + `crafting_failure` + `salvage_input` + `consumable_used` (materials) per week | ≥ 0.90 × material faucet value | < 0.80 for 2 weeks |
| `m_material_stock_growth` | weekly change of Σ ref value of material items held per realm | −2 % … +3 % | > +5 % for 2 weeks (oversupply) or < −5 % (shortage) |
| `m_gathering_yield_per_hour` | Gathering part | Gathering part | Gathering part |
| `m_node_depletion_rate` | Gathering part | 40–80 % | Gathering part |
| `m_recipe_learn_rate` | `crafting.recipe.learned` per active crafter-week by source | trainer ≥ 50 % of learns in bands ≤ expert | discovery > 10 % of learns (chance too high) |
| `m_workshop_tier_usage` | share of attempts by `workshop_tier`, by `D` band | `D` ≥ 800 at tier 5: 30–60 % | < 15 % (guild workshops not built — Building/Crafting review) |
| `m_repair_kit_share` | kit-repaired value / all repair value (copper-equivalent) | ≤ 35 % | > 45 % for 7 days (gold repair sink eroding; Economy reviews `repair_scaling`) |
| `m_crafting_gold_sink_share` | (`training_cost` from crafting-service + `npc_service` workshop fees) / gold created | 2–5 % at cap | < 1 % or > 8 % |
| `m_craft_time_share` | craft-channel seconds / active seconds of crafters | < 35 % | > 45 % |

Tuning order of operations (prefer the most local lever): content quantities in recipes → `PUBLIC_FEE_RATE`, `TRAINER_COST_W` → salvage return bands (inside Itemization's ranges) → `ENDGAME_COST_*` → `CRAFT_VALUE_W` → `ref_value.<tier>` (last; re-derives every material). Economy's controls (`tune.economy.repair_scaling`, market fees) are not changed by Crafting; Crafting raises a review when a crafting metric is the cause of an Economy alert.
