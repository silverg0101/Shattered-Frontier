# Legends of the Shattered Frontier — Economy Part 3: Balancing Targets, Inflation and Events
### Version 1.0 — Expanded Technical Design Authority edition (2026-10-17)

Status: Design target; not implemented. Names per Shared Registry v1.0.

Entry file: [Economy Architecture](../Legends_Shattered_Frontier_Economy_Architecture_v1.0.md) · Registry: [Shared Registry](../Legends_Shattered_Frontier_Shared_Registry_v1.0.md)

---

## Economic balancing targets

Economy owns total gold flow and the realm-level targets; authoring domains own their **share** (Quest spec, Economic balancing targets; Dungeon spec, Economic and pacing targets; Raid spec, Faucets per boss kill; Itemization spec, Gold faucet vs repair sink). All reward amounts use the Registry shared reward scaling functions; Economy owns `tune.economy.gold_reward_scale` (100) and `tune.economy.marks_reward_scale` (50) and no other domain changes them.

### Reference faucet per active hour

The reference model (adopted from Itemization, Gold faucet vs repair sink) is `6 × gold_reward(L, 1)` per hour from quests, bounties and content rewards plus 25 % from vendoring loot:

```text
F_ref(L) = 7.5 × gold_reward(L, 1) = 7.5 × round(100 × (1 + L)^1.5)   copper per focused active hour
```

Worked example: `F_ref(60) = 7.5 × 47,643 = 357,323` copper = 35.73 gold/h.

| Band (levels) | F_ref at band start → end (gold/h) | Band mean (gold/h) | Focused hours in band (Character spec, H(L)) | Gross gold earned in band | Target band (gold/h, typical player = 0.7 × focused) |
| --- | --- | --- | --- | --- | --- |
| 1 (1–10) | 0.21 → 2.74 | 1.41 | 3.4 | 5 g | 0.8–2.0 |
| 2 (11–20) | 3.12 → 7.22 | 5.21 | 4.8 | 25 g | 3.0–7.0 |
| 3 (21–30) | 7.74 → 12.95 | 10.41 | 6.6 | 69 g | 6.0–13.0 |
| 4 (31–40) | 13.58 → 19.69 | 16.71 | 8.6 | 144 g | 10–20 |
| 5 (41–50) | 20.41 → 27.32 | 23.95 | 10.8 | 258 g | 14–28 |
| 6 (51–60) | 28.12 → 35.73 | 32.01 | 13.1 | 420 g | 19–37 |
| 7 (61–70) | 36.61 → 44.87 | 40.82 | 15.6 | 637 g | 24–46 |
| 8 (71–80) | 45.82 → 54.67 | 50.33 | 18.2 | 916 g | 30–56 |
| 9 (81–90) | 55.69 → 65.11 | 60.48 | 20.9 | 1,265 g | 36–66 |
| 10 (91–100) | 66.18 → 76.13 | 71.23 | 23.7 | 1,689 g | 42–78 |
| Cap (100) | 76.13 | — | — | — | 55–85 (all activities incl. raids) |

Cumulative gross gold to reach level 100 for a focused leveler ≈ 5,240 gold over ≈ 123 focused hours. Measured as `m_gold_created_per_active_hour` segmented by `level_band`; alert if a band's 7-day median leaves its target band by more than 20 %.

### Faucet composition

| Source | Levelling share (L < 100) | At-cap share | Owner of the share |
| --- | --- | --- | --- |
| `quest_reward` | 25–35 % | 10–20 % | Quest |
| `npc_bounty` + `loot_drop` (coin) | 25–35 % | 15–25 % | Combat/Itemization (bounty tables), Economy (total) |
| `dungeon_reward` | 10–20 % | 15–25 % | Dungeon |
| `raid_reward` | — | 5–12 % | Raid |
| `world_event_reward` | 5–10 % | 8–15 % | AI |
| `vendor_sale` | 15–22 % | 15–22 % | Itemization (vendor value), Economy |
| `achievement_reward`, `season_conversion`, `liveops_compensation`, `gm_grant` | ≤ 5 % outside season end | ≤ 5 % outside season end | Character, LiveOps |
| LiveOps event-attributable (any source with `liveops_event_id`) | ≤ 15 % per realm-day | ≤ 15 % | LiveOps (`B_gold`) |

### Sink targets per band

Sinks as a share of gross gold created by that band (`S/F`). Levelling players must net-save (mounts, bank rows, training); capped players must run near balance.

| Band | Target `S/F` | Implied FSR | Main sinks |
| --- | --- | --- | --- |
| 1–2 | 25–45 % | 2.2–4.0 | training, repairs (newbie ×0.5), consumables |
| 3–5 | 45–65 % | 1.5–2.2 | training, repairs, fast travel, bank rows |
| 6–8 | 65–85 % | 1.2–1.5 | repairs, market fees, housing tax, fast travel |
| 9 | 80–95 % | 1.05–1.25 | as above + guild upkeep |
| 10 / cap | 90–110 % | 0.91–1.11 | full composition below |
| Realm, weekly (`m_gold_faucet_sink_ratio`) | — | **0.90–1.15** | — |

Realm FSR is population-weighted; because most gold is created at cap, realm FSR tracks the cap band. Money supply targets: `m_gold_money_supply_growth_wow` ≤ +5 % in a non-launch week, ≤ +15 % in launch weeks; long-run per active account liquid gold at cap median 2,000–6,000 gold.

### Sink composition at cap

Share of gold destroyed (`flow_class = sink`) per realm-week:

| Sink group | Codes | Target share | Owner of amounts |
| --- | --- | --- | --- |
| Repairs | `repair_cost` | 12–20 % | Itemization (`REPAIR_K`), Economy (`repair_scaling`) |
| Market fees | `market_listing_fee`, `market_transaction_fee`, `market_expiry_fee` | 15–25 % | Economy |
| Property and guild upkeep | `housing_tax`, `territory_upkeep`, `guild_upkeep`, `siege_declaration` | 15–25 % | Building |
| Travel | `fast_travel` | 8–14 % | World |
| Training and services | `training_cost`, `npc_service` | 8–14 % | Character, Economy |
| Vendor purchases | `vendor_purchase` (gold) | 10–18 % | Economy (catalogs) |
| Progression sinks | `legendary_upgrade_cost`, `quest_turn_in`, `campaign_contribution` | 5–12 % | Itemization, Quest |

Analytics' illustrative realm week (repairs 14 %, market fees 21 %, housing tax 18 %, fast travel 12 %, training 9 %, other 26 %) lies inside these bands.

**Consistency check (market fees).** Blended market fee rate `φ` ≈ 6.3 % of market gold volume (Part 2, Market fees). For market fees to be 20 % of sinks `S`: market volume = `0.20 × S / φ` ≈ 3.2 × S per week. With velocity `V = 0.25` per week and `S ≈ F`, the money supply that supports it is `M = volume / V` ≈ 12.7 × weekly F. For a realm creating 400,000 gold/day (LiveOps' `G_base` example, ≈ 2.8 M gold/week), that is M ≈ 35 M gold — consistent with ≈ 5,500 active hours per day at ≈ 72 gold/h and ≈ 9,000 active accounts × ≈ 4,000 gold median liquid wealth.

### Distribution and velocity targets

| Metric | Target | Alert |
| --- | --- | --- |
| `m_wealth_gini` | 0.55–0.75 per realm | > 0.80 for 14 days |
| `m_market_velocity` | 0.15–0.40 per week | < 0.10 for 14 days (illiquid market) |
| `m_gold_inflation_index_wow` | within ±1 % steady state; ±2 % during events | outside ±3 % |
| `m_index_reconciliation_delta` | ≤ 0.5 index points | > 1.0 |
| Top 1 % account share of liquid gold | ≤ 30 % | > 40 % |
| `m_market_fee_sink_share` | 15–25 % | outside 10–30 % for 14 days |

### Price bands (market design targets)

Target market prices for gathered resources at season start are anchored on a **gatherer wage**: gathering at a tier should earn 60–100 % of the combat faucet of a character at that tier's reference level.

```text
P_band(tier) = [W_LOW, W_HIGH] × F_ref(L_ref(tier)) / Y(tier)       copper per unit
W_LOW = 0.6, W_HIGH = 1.0                                          tune.economy.gatherer_wage_band
Y(tier) = units per focused gathering hour at the tier's skill band (Crafting spec, gathering yields)
```

| `resource_tier` | `L_ref` | `F_ref(L_ref)` copper/h | `Y` (illustrative, Crafting-owned) | Target band (copper/unit) |
| --- | --- | --- | --- | --- |
| `common` | 10 | 27,360 | 180 | 91–152 |
| `uncommon` | 35 | 162,000 | 120 | 810–1,350 |
| `rare` | 60 | 357,323 | 60 | 3,573–5,955 |
| `epic` | 85 | 598,148 | 15 | 23,926–39,877 |
| `legendary` | 100 | 761,280 | 2 | 228,384–380,640 |

Worked example: common ore with `Y` = 180 → `0.6 × 27,360 / 180 = 91.2` to `152` copper; the index example's iron ore at 120 copper sits mid-band. Refined materials target `P_input × units_in / units_out × 1.15–1.40` (crafter margin); crafted gear targets 1.5–3.0 × the sum of material band midpoints. These are monitoring targets, not price controls: Economy acts on them only through tuning (yields via Crafting, vendor reagent prices, fees), never by setting market prices. When `Y` is published by the Crafting spec, the table is recomputed by `tools/economy-sim` and these illustrative rows are replaced.

NPC anchors: every vendor reagent sold for gold must price at ≥ the upper band of the equivalent gathered resource (vendors are a ceiling-side backstop, not a competitor for gatherers).

### Marks and token pacing

| Target | Value | Owner |
| --- | --- | --- |
| Weekly cap | 1,500 per character (`FRONTIER_MARKS_WEEKLY_CAP`) | Economy |
| Dungeon share (dungeon-focused player) | ~1,000 in 5–7 h | Dungeon |
| Quest share | ≤ 525 per week | Quest |
| Raid (heroic full clear) | 600 (40 % of cap) | Raid |
| Median Level-100 player marks/week | 900–1,300 | Economy (`m_marks_time_to_cap_hours` median 10–14 h for players who cap) |
| Marks sinks | vendors, `raid_bonus_roll` (250 each, 2 per week), `npc_service` | Raid, Economy |
| Marks balance at cap, median | ≤ 3 weeks of cap (4,500) | alert if > 6,000 for 28 days (sinks too weak) |
| Season tokens expired ratio | 30–50 % of season-end balances | LiveOps (`m_season_tokens_expired_ratio`) |

### Time-to-X targets (gold)

| Purchase | Price | Target hours of play at the band | Check |
| --- | --- | --- | --- |
| First bank row (L 20) | 2 × gold_reward(20,1) = 19,246 c ≈ 1.9 g | ≈ 0.3–0.6 h at band 2 | ok |
| Third bank row (L 60) | 285,858 c ≈ 28.6 g | 0.8–1.5 h at band 6 | ok |
| Full repair, epic kit at cap | ≈ 60 g (Itemization) | 4.5 combat hours of wear | 18 % share |
| Season conversion at L 100 (max) | 203 g one-off | — | ≤ 5 % of season gold created |

---

## Inflation index

### Decision

The **official** index is computed by economy-service and published as `economy.index.computed`. It uses **the same method, basket rules, weights and base period as Analytics' reference Laspeyres index** (Analytics, Economy health, Inflation index reference computation). The only difference is who selects the basket and the data cut-off: Economy selects and publishes the basket (`basket_version` with item list, weights and base prices in the event and in `GetIndexPrices`), and Analytics computes its reference index **from Economy's published basket** rather than selecting its own. This makes the two values differ only by data timing (Reconciliation with Analytics).

### Index data model

```sql
CREATE TABLE economy.price_index_daily (
  dc_region       text NOT NULL,
  index_day       date NOT NULL,                  -- day starting at the dc_region daily reset
  basket_version  int  NOT NULL,
  method_version  int  NOT NULL,
  revision        int  NOT NULL DEFAULT 0,
  index_value     numeric(10,4) NOT NULL,
  index_chain_value numeric(12,4) NOT NULL,
  items_imputed   int  NOT NULL,
  dynamic_fee_factor numeric(6,4),
  computed_at     timestamptz NOT NULL,
  PRIMARY KEY (dc_region, index_day, revision)
);
CREATE TABLE economy.index_baskets (
  dc_region text NOT NULL, basket_version int NOT NULL, season_id text NOT NULL,
  base_start timestamptz NOT NULL, base_end timestamptz NOT NULL,
  items jsonb NOT NULL,                           -- [{definition_id, group, weight, base_price_copper, base_units}]
  PRIMARY KEY (dc_region, basket_version)
);
CREATE TABLE economy.item_prices_daily (
  dc_region text NOT NULL, price_day date NOT NULL, item_definition_id text NOT NULL,
  vw_median_unit_price_copper bigint, units_sold bigint NOT NULL, sales int NOT NULL, imputed boolean NOT NULL,
  PRIMARY KEY (dc_region, price_day, item_definition_id)
);
```

### Basket (per dc_region, fixed for a season)

1. Candidates: `item/` and `resource/` definitions sold on the `global_exchange`, `binding = none`, not event-limited, not `currency/*`.
2. Base period: the first `B` = 14 days of the season in that dc_region, excluding the launch week of a new realm.
3. Eligibility: sold on ≥ `E_days` = 12 of 14 base days and ≥ `E_units` = 1,000 units per week across the dc_region's realms.
4. Composition target `N` = 24: 8 raw resources (gathering outputs across `common`/`uncommon`/`rare`), 6 refined crafting materials, 6 consumables, 4 crafted gear pieces (ilvl 200–400, `uncommon`/`rare`). Within a group, highest base-period value traded first. A group short of eligible items has its weight redistributed proportionally to the other groups.
5. Weights: `w_i = p_{i,0} q_{i,0} / Σ_j p_{j,0} q_{j,0}`; any `w_i > W_cap` = 0.15 is capped and the excess redistributed proportionally over uncapped items (repeat until none exceed). The cap applies only when `N × W_cap ≥ 1` (true for the 24-item basket: 3.6); smaller baskets are uncapped.
6. Before the first basket of a season exists (base period running), the index for those days is computed with the previous season's basket and chained; at launch, base = first 14 days and `I = 100` for those days.

### Formula

```text
p_{i,0}  = volume-weighted median unit price of item i over the base period
q_{i,0}  = units of item i sold in the base period
p_{i,d}  = volume-weighted median unit price of item i on index day d
w_i      = capped value weight (Basket)
I(d)       = 100 × Σ_i w_i × (p_{i,d} / p_{i,0})                         within a season
I_chain(d) = I_chain(s_start) × I(d) / 100                             s_start = last day of previous season; I_chain(launch) = 100
```

- Index day `d` = the 24 h interval starting at the dc_region daily reset (Registry, Resets), aggregated over all realms of the dc_region.
- Volume-weighted median: sort the day's fills by unit price; the price at which cumulative quantity first reaches ≥ 50 % of the day's quantity.
- Exclusions before the median: fills with `flagged = true` (a `security.trade.flagged` with `transfer_ref` = the listing), cancelled held fills, and fills whose unit price lies outside `[O_low, O_high] ×` the item's trailing 7-day median (`O_low` = 0.2, `O_high` = 5.0).
- Missing day: carry forward the last price for ≤ `K` = 3 days; then impute `p_{i,d} = p_{i,0} × G_d`, where `G_d` = mean of `p_{j,d}/p_{j,0}` over non-imputed items in the same group (if none, over all non-imputed items), and mark `imputed = true`.
- Published value rounded to 2 decimals; `index_value` = `I(d)`, `index_chain_value` = `I_chain(d)`.

| Variable | Default | Tuning key |
| --- | --- | --- |
| `B` base period days | 14 | `tune.economy.index_base_period_days` |
| `E_days` | 12 | `tune.economy.index_min_days_sold` |
| `E_units` per week | 1,000 | `tune.economy.index_min_weekly_units` |
| `N` basket size | 24 (8/6/6/4) | `tune.economy.index_basket_size`, `tune.economy.index_basket_groups` |
| `W_cap` | 0.15 | `tune.economy.index_weight_cap` (Analytics' `tune.analytics.basket_weight_cap` must equal it) |
| `O_low`, `O_high` | 0.2, 5.0 | `tune.economy.index_outlier_low`, `tune.economy.index_outlier_high` |
| `K` carry-forward days | 3 | `tune.economy.index_carry_forward_days` |
| reconciliation tolerance | 0.5 index points (target), 1.0 (alert) | `tune.economy.index_reconcile_tolerance` |

Tuning changes create a new `method_version` and take effect at the next season's basket; mid-season changes require `RecomputeIndex` backfill and are audited (`config_change`).

### Worked example (identical to Analytics' example, eu)

Base period: iron ore p₀ = 120, q₀ = 50,000 → 6.0 M copper; oak log 80 × 60,000 = 4.8 M; minor healing potion 900 × 8,000 = 7.2 M; linen 200 × 10,000 = 2.0 M; total 20.0 M → weights 0.30, 0.24, 0.36, 0.10. The cap is not applied because `N × W_cap` = 4 × 0.15 = 0.6 < 1 (Basket rule 5), which is why Analytics' example also shows uncapped weights.

Day d prices 138, 82, 945, 190 → relatives 1.150, 1.025, 1.050, 0.950.
`I(d) = 100 × (0.30×1.150 + 0.24×1.025 + 0.36×1.050 + 0.10×0.950) = 100 × (0.345 + 0.246 + 0.378 + 0.095) = 106.4`.
If the season started with `I_chain(s_start) = 112.0`, then `I_chain(d) = 112.0 × 1.064 = 119.17`.

Imputation variant: if linen had no sales for 4 days, its day-4 price is `200 × mean(1.150, 1.025, 1.050) = 200 × 1.075 = 215` (all other items are in different groups here, so the all-items fallback applies), giving `I(d) = 100 × (0.345 + 0.246 + 0.378 + 0.1075) = 107.65` with `items_imputed = 1`.

Weight-cap example (24-item basket): if a consumable has raw weight 0.21 and the other 23 items share 0.79, it is capped to 0.15 and the excess 0.06 is spread over the 23 in proportion to their weights (each × `1 + 0.06/0.79`); repeat if any now exceeds 0.15.

### Reconciliation with Analytics

| Aspect | Economy (official) | Analytics (reference) | Expected difference |
| --- | --- | --- | --- |
| Basket, weights, base prices | selects and publishes | uses Economy's published basket | none |
| Formula, exclusions, imputation | Formula | same | none |
| Day boundary | dc_region daily reset | must use the same reset-aligned day (Analytics daily jobs are reset-aligned) | none if aligned |
| Data source | OLTP `economy.market_fills` at reset + 15 min | `modeled.fact_market_sales` at reset + 2–3 h | late flags or late events only |
| Flag exclusion | flags known at computation time | flags known at warehouse load | flags raised in between |

Tolerance: `|I_econ(d) − I_ref(d)| ≤ 0.5` points expected; Analytics' existing alert at > 1 point stays. Economy recomputes day d once at reset + 24 h (`economy.index.computed` with `revision = 1`) to absorb late flags; Analytics compares against the latest revision.

---

## Inflation controls

Preserved controls: dynamic taxes, repair scaling, territory maintenance, seasonal economic audits. All controls are tuning values in config-service; every automatic adjustment is a config publish with `actor = economy-service`, audited (`config_change`), and bounded. Automatic controls are off at launch and enabled per realm by `economy_analyst` after two weeks of stable index data.

### Dynamic fee factor

Dynamic fee factor (flag `ff.economy.dynamic_fees_enabled`, default off at launch):

```text
x = I_chain(d) / I_target − 1
D = clamp(1 + k × x, D_min, D_max)        applied to r_txn from the next dc_region reset, recomputed daily
```

Defaults: `I_target` = trailing 28-day mean of `I_chain` at season start (frozen per season), `k` = 2.0, `D_min` = 0.8, `D_max` = 2.0 (`tune.economy.dynamic_fee_k`, `tune.economy.dynamic_fee_min`, `tune.economy.dynamic_fee_max`). Example: I_chain 106.0 vs target 100.0 → x = 0.06, D = 1.12, r_txn_eff = 5.6 %. Repair scaling and territory maintenance factors are owned by Itemization and Building as formulas; Economy publishes `I_chain` for them via `GetIndexPrices`.

Daily change limit: `|D(d) − D(d−1)| ≤ 0.05` (`tune.economy.dynamic_fee_max_step`), so fees never jump.

### Repair scaling

`economy_repair_mult` in Itemization's repair formula is the Economy-owned key `tune.economy.repair_scaling` (default 1.0, clamp 0.8–1.5). Economy adjusts it weekly (flag `ff.economy.auto_repair_scaling`, default off; manual otherwise):

```text
share = m_gold_repair_sink_share over the last 7 days (repair sink / gold created, cap band)
if share < 0.10: repair_scaling += 0.05
if share > 0.20: repair_scaling −= 0.05
also: if I_chain/I_target − 1 > 0.05 and share ≤ 0.18: repair_scaling += 0.05
clamp to [0.8, 1.5]; at most one step per week; Itemization changes REPAIR_K only if scaling sits at a clamp for 4 weeks
```

Worked example: share 8.7 % for 7 days → 1.00 → 1.05; the epic chest repair of 42,400 copper becomes ≈ 44,520.

### Vendor price scaling

`v_econ` (`tune.economy.vendor_price_scaling`, default 1.0, clamp 0.8–1.3) multiplies gold vendor buy prices only (never sell prices, which would add a faucet lever in the wrong direction). Manual control; recommended rule: raise by 0.05 per week while `I_chain/I_target − 1 > 0.08` and vendor purchases are < 10 % of sinks.

### Event multipliers

LiveOps overlays on reward functions are Economy-owned keys applied **upstream by reward-service** when it evaluates a bundle (LiveOps spec, Event modifiers): `tune.economy.event_gold_reward_multiplier` (default 1.0) multiplies `gold_reward(L, w)`; `tune.economy.event_marks_multiplier` (default 1.0) multiplies `marks_reward(w)`. Rules:

1. Bounds: gold multiplier ≤ 2.0, marks multiplier ≤ 2.0 (`tune.economy.event_multiplier_max`); `FRONTIER_MARKS_WEEKLY_CAP` is never raised by events.
2. Credits made under a multiplier carry `liveops_event_id`; LiveOps' `B_gold(r) = 0.15 × G_base(r)` budget is measured from those credits (`m_event_gold_created`).
3. economy-service rejects credits whose `liveops_event_id` refers to an event not `active` at `occurred_at` (cached from `liveops.event.started/ended`), reported as `economy_invariant`.

Worked example: bonus gold weekend, multiplier 1.25 on a level-70 standard quest: `round(1.25 × gold_reward(70, 1)) = round(1.25 × 59,826) = 74,783` copper, credit carries `liveops_event_id = lop_…`.

### Territory maintenance

Building owns territory upkeep amounts. Economy's contract: Building may scale upkeep by `I_chain(d) / I_target` clamped to [0.9, 1.3], read through `GetIndexPrices.index_chain_value`; no other index-linked control exists outside Economy.

### Control summary

| Control | Lever | Default | Bounds | Mode at launch |
| --- | --- | --- | --- | --- |
| Dynamic fee factor `D` | `r_txn_eff` | 1.0 | 0.8–2.0, ±0.05/day | off |
| Repair scaling | `tune.economy.repair_scaling` | 1.0 | 0.8–1.5, ±0.05/week | manual |
| Vendor price scaling `v_econ` | gold vendor buy prices | 1.0 | 0.8–1.3 | manual |
| Event multipliers | reward functions | 1.0 | ≤ 2.0 | LiveOps-scheduled |
| Territory maintenance | Building upkeep | 1.0 | 0.9–1.3 | Building |
| Reward scale constants | `gold_reward_scale`, `marks_reward_scale` | 100, 50 | ±10 % per season | seasonal audit only |

### Seasonal economic audit

At each season end, economy_team reviews per realm: FSR by week, money supply growth, Gini, lien collections and write-offs, top 1 % wealth share, and fee sink share; outcomes are tuning proposals through config-service (LiveOps release process). No automated action.

Audit inputs (generated by `tools/economy-sim report`): per-band faucet per active hour vs Economic balancing targets; sink composition vs targets; marks balances; price band adherence per resource tier; index path and control actions; lien and hold statistics; list of sources whose share left its band for ≥ 2 weeks, with the owning spec named for follow-up.

---

## Event payloads

Envelope per Registry, Event taxonomy, Envelope (not redefined). Envelope `privacy_class` = maximum of payload field classes (`p1_pseudonymous` for every event below except `economy.index.computed` and `economy.snapshot.exported`, which are `p0_public`). `account_id` and `character_id` in the envelope are the wallet owner's (credited/debited), the seller's (`market.*`), side A's (trade), absent for `economy.index.computed`. `causation_id` and `correlation_id` in the envelope equal the transaction's values. All amounts are integer minor units. `schema_version` = 1. Topics and keys: entry file, Service.

### `economy.currency.credited`

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `txn_id` | string | yes | `txn_` | p1_pseudonymous |
| `currency_id` | string | yes | `currency/*` | p0_public |
| `amount` | int64 | yes | applied amount (after marks cap) | p1_pseudonymous |
| `requested_amount` | int64 | yes | amount before cap | p1_pseudonymous |
| `capped_amount` | int64 | no | marks cap truncation (absent if 0) | p1_pseudonymous |
| `garnished_amount` | int64 | no | lien collection triggered by this credit (absent if 0) | p1_pseudonymous |
| `balance_after` | int64 | yes | balance after this txn (before any garnish debit) | p1_pseudonymous |
| `wallet_seq` | int64 | yes | per-wallet ledger sequence | p1_pseudonymous |
| `source` | string | yes | Registry source code | p0_public |
| `flow_class` | string | yes | `faucet`, `transfer`, `correction` | p0_public |
| `owner_kind` | string | yes | `character`, `account`, `guild` | p0_public |
| `owner_id` | string | yes | `chr_` / `acc_` / `gld_` | p1_pseudonymous |
| `counterparty_owner_kind` | string | no | transfers only | p0_public |
| `counterparty_owner_id` | string | no | transfers only | p1_pseudonymous |
| `transfer_kind` | string | no | `trade`, `market`, `mail` | p0_public |
| `transfer_ref` | string | no | `trd_`, `lst_`, mail ref | p1_pseudonymous |
| `reward_grant_id` | string | no | `rwg_` when via reward-service | p1_pseudonymous |
| `liveops_event_id` | string | no | `lop_` when an event modifier applied | p1_pseudonymous |
| `hold_id` | string | no | `thd_` if executed after a hold | p1_pseudonymous |
| `vendor_id` | string | no | `vendor/*` for `vendor_sale` | p0_public |
| `reason_code` | string | no | caller reason enum (e.g. `season_token_conversion`) | p0_public |
| `config_version` | int64 | yes | config-service tuning version used | p0_public |

### `economy.currency.debited`

Same fields as `economy.currency.credited` except: `sink` (string, required, Registry sink code, p0_public) replaces `source`; `flow_class` ∈ `sink`, `transfer`, `correction`; no `requested_amount`/`capped_amount`/`garnished_amount`; adds:

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `shortfall` | int64 | no | partial debit shortfall (`allow_partial`) | p1_pseudonymous |
| `lien_id` | string | no | garnish debits: oldest lien | p1_pseudonymous |
| `lien_allocations` | object[] | no | `[{lien_id, amount}]` when spanning liens | p1_pseudonymous |
| `rollback_id` | string | no | `rbk_` for enforcement debits | p1_pseudonymous |
| `npc_price_mult` | number | no | applied modifier for NPC-priced sinks | p0_public |

```json
{
  "event_name": "economy.currency.credited",
  "account_id": "acc_01J9ZK7W0A1B2C3D4E5F6G7H8J",
  "character_id": "chr_01J9ZK3Q8M4V6T2N5X7B1C0D9E",
  "causation_id": "rwg_01JA0B1C2D3E4F5G6H7J8K9M0N",
  "privacy_class": "p1_pseudonymous",
  "payload": {
    "txn_id": "txn_01JA0B1C9Q8R7S6T5V4W3X2Y1Z",
    "currency_id": "currency/gold",
    "amount": 36000,
    "requested_amount": 36000,
    "balance_after": 1254000,
    "wallet_seq": 4182,
    "source": "dungeon_reward",
    "flow_class": "faucet",
    "owner_kind": "character",
    "owner_id": "chr_01J9ZK3Q8M4V6T2N5X7B1C0D9E",
    "reward_grant_id": "rwg_01JA0B1C2D3E4F5G6H7J8K9M0N",
    "config_version": 1931
  }
}
```

### `economy.trade.completed`

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `trade_id` | string | yes | `trd_` | p1_pseudonymous |
| `transfer_kind` | string | yes | `trade` (mail transfers also use this event with `mail`) | p0_public |
| `sides` | object[2] | yes | per side: `account_id`, `character_id`, `gold_copper`, `items[]` (`item_definition_id`, `item_instance_id`, `quantity`), `value_copper` (fair value of what the side gave) | p1_pseudonymous |
| `value_ratio` | number | yes | `max(value_a, value_b) / max(min(value_a, value_b), 1)` | p1_pseudonymous |
| `hold_id` | string | no | `thd_` if held | p1_pseudonymous |
| `held_seconds` | int | no | duration of hold | p1_pseudonymous |
| `lien_active` | bool | yes | either side had an active lien | p1_pseudonymous |

For mail, `trade_id` carries the mail `transfer_ref` (`txn_`), side A is the sender, side B the recipient with empty offer.

```json
{
  "event_name": "economy.trade.completed",
  "payload": {
    "trade_id": "trd_01JA0C3D4E5F6G7H8J9K0M1N2P",
    "transfer_kind": "trade",
    "sides": [
      { "account_id": "acc_01J9ZK7W0A1B2C3D4E5F6G7H8J", "character_id": "chr_01J9ZK3Q8M4V6T2N5X7B1C0D9E",
        "gold_copper": 150000, "items": [], "value_copper": 150000 },
      { "account_id": "acc_01J9ZM2V5B6C7D8E9F0G1H2J3K", "character_id": "chr_01J9ZM4X6C7D8E9F0G1H2J3K4M",
        "gold_copper": 0, "items": [ { "item_definition_id": "item/iron_longsword", "item_instance_id": "itm_01JA0C5F6G7H8J9K0M1N2P3Q4R", "quantity": 1 } ],
        "value_copper": 30000 }
    ],
    "value_ratio": 5.0,
    "hold_id": "thd_01JA0C6G7H8J9K0M1N2P3Q4R5S",
    "held_seconds": 86400,
    "lien_active": false
  }
}
```

### `economy.index.computed`

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `dc_region` | string | yes | repeated from envelope (Analytics requirement) | p0_public |
| `index_day` | date | yes | reset-aligned day | p0_public |
| `basket_version` | int | yes | | p0_public |
| `method_version` | int | yes | Formula tuning set | p0_public |
| `base_period` | object | yes | `{start, end}` RFC 3339 | p0_public |
| `index_value` | number | yes | `I(d)` | p0_public |
| `index_chain_value` | number | yes | `I_chain(d)` | p0_public |
| `items_imputed` | int | yes | | p0_public |
| `revision` | int | yes | 0 at reset + 15 min, 1 at reset + 24 h | p0_public |
| `items` | object[] | yes | `[{definition_id, group, weight, base_price_copper, price_copper, imputed}]` | p0_public |
| `dynamic_fee_factor` | number | no | `D` if enabled | p0_public |
| `repair_scaling` | number | no | current `tune.economy.repair_scaling` | p0_public |

```json
{
  "event_name": "economy.index.computed",
  "dc_region": "eu",
  "privacy_class": "p0_public",
  "payload": {
    "dc_region": "eu", "index_day": "2026-10-17", "basket_version": 3, "method_version": 1,
    "base_period": { "start": "2026-09-01T04:00:00.000Z", "end": "2026-09-15T04:00:00.000Z" },
    "index_value": 106.40, "index_chain_value": 119.17, "items_imputed": 0, "revision": 0,
    "items": [ { "definition_id": "resource/iron_ore", "group": "raw", "weight": 0.30,
                 "base_price_copper": 120, "price_copper": 138, "imputed": false } ]
  }
}
```

### `economy.lien.placed`

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `lien_id` | string | yes | `wln_` | p1_pseudonymous |
| `account_id` | string | yes | debtor account | p1_pseudonymous |
| `currency_id` | string | yes | `currency/gold`, marks or tokens (never crowns) | p0_public |
| `amount` | int64 | yes | original amount (= rollback shortfall) | p1_pseudonymous |
| `rollback_id` | string | yes | `rbk_` | p1_pseudonymous |
| `case_id` | string | yes | `cas_` | p1_pseudonymous |
| `expires_at` | timestamp | yes | write-off deadline | p0_public |
| `active_liens_total` | int64 | yes | Σ outstanding of the account's active liens in this currency after placement | p1_pseudonymous |

### `economy.lien.closed`

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `lien_id` | string | yes | `wln_` | p1_pseudonymous |
| `account_id` | string | yes | | p1_pseudonymous |
| `currency_id` | string | yes | | p0_public |
| `final_state` | string | yes | `settled`, `released`, `written_off` | p0_public |
| `original_amount` | int64 | yes | | p1_pseudonymous |
| `collected` | int64 | yes | total garnished | p1_pseudonymous |
| `written_off_amount` | int64 | yes | outstanding forgiven (0 unless `written_off`) | p1_pseudonymous |
| `close_reason` | string | yes | `settled`, `appeal_overturned`, `staff_release`, `expired`, `permanent_ban`, `account_deleted` | p0_public |
| `rollback_id` | string | yes | `rbk_` | p1_pseudonymous |
| `lien_age_seconds` | int64 | yes | created → closed | p0_public |

### `economy.hold.placed`

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `hold_id` | string | yes | `thd_` | p1_pseudonymous |
| `transfer_kind` | string | yes | `trade`, `market`, `mail` | p0_public |
| `transfer_ref` | string | yes | `trd_`, `lst_…:fill_no`, mail `txn_` | p1_pseudonymous |
| `held_account_id` | string | yes | elevated party | p1_pseudonymous |
| `counterparty_account_id` | string | yes | other party | p1_pseudonymous |
| `value_copper` | int64 | yes | held party's outgoing value | p1_pseudonymous |
| `risk_band` | string | no | Security band at hold; absent if `GetRiskScore` unavailable | p1_pseudonymous |
| `release_at` | timestamp | yes | first re-check | p0_public |
| `max_release_at` | timestamp | yes | | p0_public |

### `economy.hold.released`

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `hold_id` | string | yes | | p1_pseudonymous |
| `transfer_kind` | string | yes | | p0_public |
| `transfer_ref` | string | yes | | p1_pseudonymous |
| `outcome` | string | yes | `released`, `cancelled` (the `transfer_hold_state` reached) | p0_public |
| `outcome_reason` | string | yes | `clean_recheck`, `max_hold_elapsed`, `restriction`, `rollback_freeze`, `banned`, `staff_release`, `staff_cancel`, `account_erasure` | p0_public |
| `rechecks` | int | yes | | p0_public |
| `held_seconds` | int | yes | | p0_public |

`risk_band` is placed only on `domain.economy` (internal consumers); it is never exposed to players and is stripped from any warehouse table readable by non-security roles (Analytics privacy rules for detection-adjacent data). Its class is raised from the earlier p0 marking to `p1_pseudonymous` because it is linked to an account.

### `economy.snapshot.exported`

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `dc_region` | string | yes | | p0_public |
| `snapshot_day` | date | yes | | p0_public |
| `as_of` | timestamp | yes | reset instant | p0_public |
| `iceberg_snapshot_id` | int64 | yes | realm table commit | p0_public |
| `account_iceberg_snapshot_id` | int64 | no | account-wallet table commit | p0_public |
| `row_count` | int64 | yes | | p0_public |
| `totals` | object | yes | `{realm_id: {currency_id: Σ balance}}` | p0_public |
| `account_totals` | object | no | `{currency_id: Σ balance}` for account-owned wallets | p0_public |

### `market.listing.created`

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `listing_id` | string | yes | `lst_` | p1_pseudonymous |
| `market_scope` | string | yes | `global_exchange`, `regional` | p0_public |
| `world_region_id` | string | no | regional only | p0_public |
| `item_definition_id` | string | yes | | p0_public |
| `item_instance_id` | string | yes | escrowed `itm_` | p1_pseudonymous |
| `quantity` | int | yes | listed quantity | p1_pseudonymous |
| `unit_price_copper` | int64 | yes | | p1_pseudonymous |
| `listing_fee_copper` | int64 | yes | | p1_pseudonymous |
| `duration_hours` | int | yes | 12, 24, 48 | p0_public |
| `expires_at` | timestamp | yes | | p0_public |
| `seller_account_id` | string | yes | | p1_pseudonymous |
| `seller_character_id` | string | yes | | p1_pseudonymous |
| `fair_value_copper` | int64 | no | unit fair value at listing (spoofing features) | p1_pseudonymous |

### `market.listing.sold`

One event per fill.

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `listing_id` | string | yes | `lst_` | p1_pseudonymous |
| `fill_no` | int | yes | 1-based | p1_pseudonymous |
| `item_definition_id` | string | yes | | p0_public |
| `quantity` | int | yes | units in this fill | p1_pseudonymous |
| `quantity_remaining` | int | yes | after this fill | p1_pseudonymous |
| `unit_price_copper` | int64 | yes | | p1_pseudonymous |
| `total_copper` | int64 | yes | `quantity × unit_price_copper` | p1_pseudonymous |
| `fee_copper` | int64 | yes | transaction fee | p1_pseudonymous |
| `seller_net_copper` | int64 | yes | `total − fee` | p1_pseudonymous |
| `market_scope` | string | yes | | p0_public |
| `world_region_id` | string | no | regional only | p0_public |
| `buyer_account_id` | string | yes | | p1_pseudonymous |
| `buyer_character_id` | string | yes | | p1_pseudonymous |
| `seller_account_id` | string | yes | | p1_pseudonymous |
| `seller_character_id` | string | yes | | p1_pseudonymous |
| `buyer_txn_id` / `seller_txn_id` | string | yes | `market_purchase` / `market_sale` txns | p1_pseudonymous |
| `hold_id` | string | no | `thd_` if executed after hold | p1_pseudonymous |
| `fair_value_copper` | int64 | no | unit fair value at fill | p1_pseudonymous |

```json
{
  "event_name": "market.listing.sold",
  "account_id": "acc_01J9ZK7W0A1B2C3D4E5F6G7H8J",
  "dc_region": "na",
  "realm_id": "na-1",
  "payload": {
    "listing_id": "lst_01JA0D1E2F3G4H5J6K7M8N9P0Q", "fill_no": 1,
    "item_definition_id": "item/iron_ingot", "quantity": 12, "quantity_remaining": 8,
    "unit_price_copper": 1500, "total_copper": 18000, "fee_copper": 900, "seller_net_copper": 17100,
    "market_scope": "global_exchange",
    "buyer_account_id": "acc_01J9ZM2V5B6C7D8E9F0G1H2J3K", "buyer_character_id": "chr_01J9ZM4X6C7D8E9F0G1H2J3K4M",
    "seller_account_id": "acc_01J9ZK7W0A1B2C3D4E5F6G7H8J", "seller_character_id": "chr_01J9ZK3Q8M4V6T2N5X7B1C0D9E",
    "buyer_txn_id": "txn_01JA0D2F3G4H5J6K7M8N9P0Q1R", "seller_txn_id": "txn_01JA0D2F3G4H5J6K7M8N9P0Q1S",
    "fair_value_copper": 1480
  }
}
```

### `market.listing.expired`

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `listing_id` | string | yes | | p1_pseudonymous |
| `item_definition_id` | string | yes | | p0_public |
| `market_scope` | string | yes | | p0_public |
| `quantity_unsold` | int | yes | returned to seller | p1_pseudonymous |
| `expiry_fee_copper` | int64 | yes | collected | p1_pseudonymous |
| `expiry_fee_waived_copper` | int64 | yes | uncollected (insufficient balance) | p1_pseudonymous |
| `seller_account_id` | string | yes | | p1_pseudonymous |
| `listing_age_seconds` | int | yes | | p0_public |

### `market.listing.cancelled`

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `listing_id` | string | yes | | p1_pseudonymous |
| `item_definition_id` | string | yes | | p0_public |
| `market_scope` | string | yes | | p0_public |
| `quantity_unsold` | int | yes | | p1_pseudonymous |
| `cancelled_by` | string | yes | `seller`, `enforcement`, `system` | p0_public |
| `seller_account_id` | string | yes | | p1_pseudonymous |
| `listing_age_seconds` | int | yes | for spoofing detectors | p0_public |
| `unit_price_copper` | int64 | yes | | p1_pseudonymous |

### Coverage of Analytics interim requirements

| Analytics required field | Provided by |
| --- | --- |
| `txn_id`, `currency_id`, `amount`, `balance_after`, `source`/`sink`, `owner_kind`, `owner_id`, `counterparty_owner_id`, `reward_grant_id`, `config_version` | `economy.currency.credited` / `economy.currency.debited` |
| `liveops_event_id`, `compensation_id` (from `causation_id` = `cmpg_`) | credited field + envelope |
| `listing_id`, `item_definition_id`, `quantity`, `unit_price_copper`, `total_copper`, `fee_copper`, `market_scope`, `buyer_account_id`, `seller_account_id` | `market.listing.sold` |
| `trade_id`, per side account, gold, item definition IDs and quantities | `economy.trade.completed` `sides[]` |
| `dc_region`, `basket_version`, `index_value`, `base_period` | `economy.index.computed` |
| nightly wallet export `(realm_id, owner_kind, owner_id, currency_id, balance)` | Part 1, Wallet snapshot export |
| `flow_class` | now published (Analytics may stop deriving it) |
| buyer-side market debit code | `market_purchase` (Part 1, Faucet/sink taxonomy) |

All economy metrics marked `provisional` in Analytics can be promoted once these payloads are registered in the schema registry.
