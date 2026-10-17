# Legends of the Shattered Frontier — Dungeon Spec, Part 3: Dungeon Content, Scaling, Mythic, Affixes and Rewards
### Version 1.0 — Expanded Technical Design Authority edition (2026-10-17)

**Status:** Design target; not implemented. Names per Shared Registry v1.0.
**Entry file:** [Dungeon Architecture](../Legends_Shattered_Frontier_Dungeon_Architecture_v1.0.md) · **Registry:** [Shared Registry v1.0](../Legends_Shattered_Frontier_Shared_Registry_v1.0.md)

This part defines what a dungeon *is* as content: dungeon types, composition and the dungeon definition schema; the complete scaling model (group size 1–10, Character Level, difficulty, Mythic Levels 1–20); mythic timers and access; dungeon affixes and their weekly rotation; seeded procedural generation of packs, objectives and events; open-world and seasonal dungeons; reward assignment; and economic balancing targets.

---

## 1. Dungeon types (registry `dungeon_type`)

| Type | Hosting | Sizes | Difficulties | Lockout | Purpose |
| --- | --- | --- | --- | --- | --- |
| `story` | `instance-server` | 1–5 (`solo_party`) | `story` (optionally `normal`) | none | Narrative chapters tied to quest chains; solo-able with NPC companions. |
| `open_world` | `zone-server` (shared space) | any; scaling counts 1–40 | single tuning (`normal`) | soft (per-encounter cooldown) | Shared delves, ruins, caves within zones; no instance run. |
| `instanced` | `instance-server` | `solo_party` and/or `extended_party` | `normal`, `veteran`, `mythic` (if in season pool) | `veteran`: daily loot-only | The standard repeatable dungeon. |
| `mythic` | `instance-server` | `solo_party` (5) | `mythic` only | none (weekly cache rule) | Endgame-only dungeons designed around the timer (e.g. labyrinth vaults). |
| `seasonal` | `instance-server` | `solo_party` and/or `extended_party` | `normal`, `veteran` (+`mythic` when in pool) | daily loot-only | Exists only while its season is `active`; seasonal affix always on. |

`difficulty` values used by dungeons are `story`, `normal`, `veteran`, `mythic` (Registry §6). `veteran` and `mythic` require Character Level 100 (`LEVEL_CAP`). `story` and `normal` exist across the level range.

### 1.1 Story dungeons and NPC companions

`story` runs with fewer than 3 players spawn **companions** (`npc/companion_<role>`) to fill missing `tank` and `healer` roles (max 2 companions). Companions use the Part 2 NPC behavior stack with player-ally behavior trees, count **0.3** toward damage-equivalents and **do not count** toward the scaling group size `n`. They never receive rewards and cannot loot.

---

## 2. Composition and the dungeon definition

A dungeon is a directed graph of **segments** from entrance to final encounter. Node kinds: `entrance`, `pack_slot`, `objective_slot`, `event_slot`, `encounter` (boss or mini boss), `reward` (end chest), `exit`.

```json
{
  "id": "dungeon/sunken_vault",
  "schema_version": 1,
  "dungeon_type": "instanced",
  "biome": "greenlands",
  "map_id": "Map_SunkenVault",
  "entrance": { "zone_id": "zone/greenlands_01", "portal_actor": "portal_sunken_vault", "requirements": { "min_level": 18, "quests_completed": [] } },
  "level_range": { "min": 18, "max": 26, "cap_variant": true },
  "size_classes": ["solo_party"],
  "difficulties": ["normal", "veteran", "mythic"],
  "lockout_policy": { "veteran": { "period": "daily", "granularity": "loot_only", "extendable": false, "soft_cooldown_s": null, "scope": "character" } },
  "segments": [
    { "id": "seg_gate", "kind": "entrance", "next": ["ps_01"] },
    { "id": "ps_01", "kind": "pack_slot", "candidates": ["pack/sv_drowned_brutes", "pack/sv_tide_casters"], "next": ["ps_02", "os_01"] },
    { "id": "os_01", "kind": "objective_slot", "pick": 1, "candidates": ["objective/sv_free_prisoners", "objective/sv_seal_floodgates"], "next": ["enc_tidecaller"] },
    { "id": "ps_02", "kind": "pack_slot", "candidates": ["pack/sv_thrall_swarm", "pack/sv_drowned_brutes", "pack/sv_reef_guardian"], "patrol_paths": ["pp_hall_a"], "next": ["enc_tidecaller"] },
    { "id": "enc_tidecaller", "kind": "encounter", "encounter_id": "encounter/sunken_vault_tidecaller", "next": ["es_01"] },
    { "id": "es_01", "kind": "event_slot", "probability": 0.5, "candidates": ["dungeon_event/sv_collapsing_corridor", "dungeon_event/sv_ambush"], "next": ["ps_03"] },
    { "id": "ps_03", "kind": "pack_slot", "candidates": ["pack/sv_tide_casters", "pack/sv_reef_guardian"], "next": ["enc_drowned_king"] },
    { "id": "enc_drowned_king", "kind": "encounter", "encounter_id": "encounter/sunken_vault_drowned_king", "final": true, "next": ["rw_end"] },
    { "id": "rw_end", "kind": "reward", "next": ["exit"] }
  ],
  "generation": { "budget_points": 180, "budget_tolerance": 0.10, "required_tags": ["interrupt_test", "caster"], "max_elites_per_segment": 2 },
  "mythic": { "par_ms": 1920000, "enemy_forces_required": 260 },
  "run_time_target_ms": { "normal": 1500000, "veteran": 1800000 },
  "rewards": { "mythic_chest": "reward/sunken_vault_mythic_chest", "completion": { "normal": "reward/sunken_vault_completion_normal" } }
}
```

Pack template:

```json
{
  "id": "pack/sv_tide_casters",
  "members": [
    { "npc_id": "npc/tide_caster", "tier": "common", "archetype": "caster", "base_count": 3 },
    { "npc_id": "npc/reef_shaman", "tier": "elite", "archetype": "shaman", "base_count": 1 }
  ],
  "threat_points": 30,
  "enemy_forces": 22,
  "tags": ["caster", "interrupt_test"],
  "est_clear_ms_at_ref": 35000
}
```

Composition rules (validated at publish):

1. Exactly one `entrance`, at least one `encounter` with `final: true`, every node reachable from the entrance and able to reach `exit`.
2. 2–4 encounters per dungeon (`instanced`), 1–3 (`story`), 3–5 (`mythic`).
3. Every `encounter` node's prerequisites (Part 2) must reference encounters earlier on all paths.
4. `objective_slot.pick` objectives are required for completion unless flagged `optional`.
5. Reward bundles exist for every enabled difficulty.

**Completion condition** (Part 1 `in_progress` → `completed`): final encounter `defeated` **and** all required objectives done; for `mythic`, additionally `enemy_forces.current ≥ enemy_forces_required`. If the final boss dies before forces are complete, the run completes when forces are reached (the chest unlocks then; the timer stops then).

---

## 3. Scaling model

All scaling is computed once per run by `instance-service` (inputs) and applied by `instance-server` at spawn. Mid-run changes of group size (leave/backfill) re-scale **out-of-combat** NPCs only, at the next safe point (no engaged encounter), never during an engaged attempt.

### 3.1 Variables

| Symbol | Meaning | Source |
| --- | --- | --- |
| `n` | Scaling group size = connected real players at run start (companions excluded), 1–10. | Run members |
| `L_run` | Run level. Levelling content: `clamp(round(mean(member levels)), level_range.min, level_range.max)`; cap content (`veteran`, `mythic`, cap `normal`): 100. | Part 1 `run_level` |
| `L_npc` | `L_run + level_offset` (tier defaults: common 0, elite +1, mini_boss +2, encounter-authored for bosses). | Content |
| `t` | Creature tier: `common`, `elite`, `mini_boss` (Combat). | Content |
| `d` | Difficulty. | Run |
| `M` | Mythic Level 1–20 (`MYTHIC_LEVEL_MAX`); 0 if not mythic. | Run |
| `A_hp` | Archetype/boss health factor (default 1.0; bosses `boss_hp_factor`). | Content |
| `dmg_coef` | Ability damage as a fraction of `H_ref(L_npc)`. | Content |

### 3.2 Reference health

The Combat spec defines `Health = 100 + (Constitution × 15) + (Level × 25)`. Dungeons scale NPCs against a **reference player** at level `L`:

```
C_ref(L) = ref_con_base + ref_con_per_level × L
H_ref(L) = 100 + 15 × C_ref(L) + 25 × L
```

Defaults: `ref_con_base = 20` (`tune.dungeon.ref_con_base`), `ref_con_per_level = 1.3` (`tune.dungeon.ref_con_per_level`). Simplified: `H_ref(L) = 400 + 44.5 × L` → `H_ref(30) = 1,735`, `H_ref(100) = 4,850`, `H_ref(102) = 4,939`. These defaults are the reference build ratified by Combat and Character (Combat spec, Reference build): Constitution takes a fixed 29.17 % share of Character's median natural attribute total and reaches the natural cap 150 at level 100; the linear form stays within 5 % health of that curve at every checkpoint, so the values above and the worked examples below are unchanged.

### 3.3 Health

```
HP(npc) = round( H_ref(L_npc) × T_hp(t) × A_hp × G_hp(n) × D_hp(d) × Y(M) × Π affix_hp )
```

| Term | Definition | Defaults | Tuning key |
| --- | --- | --- | --- |
| `T_hp(t)` | Combat creature tier multiplier. | common 1.0, elite 2.5, mini_boss 5.0 | (Combat) |
| `G_hp(n)` | `(n / 5) ^ α_hp` | `α_hp = 0.9` | `tune.dungeon.group_hp_exponent` |
| `D_hp(d)` | Difficulty multiplier. | story 0.60, normal 1.00, veteran 1.35, mythic 1.30 | `tune.dungeon.difficulty_hp.<d>` |
| `Y(M)` | Mythic multiplier: `1` if `M = 0`, else `y1^(min(M,10) − 1) × y2^(max(M − 10, 0))` | `y1 = 1.07`, `y2 = 1.09` | `tune.dungeon.mythic_step_low`, `tune.dungeon.mythic_step_high` |
| `affix_hp` | Affix multipliers (§5). | — | per affix |

`G_hp(n)` values: n=1 0.235, 2 0.438, 3 0.631, 4 0.818, 5 1.000, 6 1.178, 7 1.354, 8 1.527, 9 1.697, 10 1.866.

`Y(M)` values: M=1 1.000, 2 1.070, 5 1.311, 10 1.838, 12 2.184, 15 2.828, 20 4.352.

### 3.4 Damage

```
DMG(ability) = round( H_ref(L_npc) × dmg_coef × T_dmg(t) × G_dmg(n) × D_dmg(d) × Y(M) × Π affix_dmg × E_soft )
```

| Term | Definition | Defaults | Tuning key |
| --- | --- | --- | --- |
| `T_dmg(t)` | `sqrt(T_hp(t))` for archetype NPCs (trash, elites, trash-area mini bosses); **1.0** for encounter-authored abilities (their `dmg_coef` is final). | common 1.0, elite 1.58, mini_boss 2.24 | `tune.dungeon.tier_damage_exponent` (0.5) |
| `G_dmg(n)` | `n ≤ 5: g0 + g1 × n`; `n > 5: 1 + g2 × (n − 5)` | `g0 = 0.55`, `g1 = 0.09`, `g2 = 0.04` | `tune.dungeon.group_dmg_base`, `…_per_player`, `…_per_extra` |
| `D_dmg(d)` | Difficulty damage. | story 0.50, normal 1.00, veteran 1.25, mythic 1.30 | `tune.dungeon.difficulty_dmg.<d>` |
| `E_soft` | Soft enrage (Part 2). | 1.0 before soft start | — |

The result is pre-mitigation damage, fed to the Combat damage pipeline (armor reduction with attacker level `L_npc`, resistances, block/dodge/parry). Applying the tier multiplier's square root to damage (rather than the full tier) keeps elite burst survivable; Combat ratified this rule (Combat spec, NPC Attribute Generation, Creature-tier rule). NPC armor and resistances come from Combat spec, NPC mitigation.

`G_dmg(n)` values: n=1 0.64, 2 0.73, 3 0.82, 4 0.91, 5 1.00, 6 1.04, 8 1.12, 10 1.20.

### 3.5 Count

Count scaling applies to `common`-tier pack members and to encounter adds with `scale_count: true`:

```
count(n) = max(1, round(base_count × (c0 + c1 × n)))      c0 = 0.6, c1 = 0.08
```

(`tune.dungeon.count_base`, `tune.dungeon.count_per_player`). Elites and mini bosses keep their authored count (only health and damage scale). For `base_count = 4`: n=1 → 3, n=3 → 3, n=5 → 4, n=8 → 5, n=10 → 6. Enemy forces per member are fixed, so the mythic forces requirement is computed with `n = 5`; mythic is fixed at 5 players.

### 3.6 Level sync (levelling content)

Characters above `L_run + 3` are synced down for the run: the `Level` term in Combat formulas uses `min(level, L_run + 3)` and each equipped item's stat budget is evaluated at `ilvl_eff = min(ilvl, 4 × (L_run + 3) + 10)` (Itemization, Stat budget). Characters below `level_range.min` cannot enter. XP is computed at the character's real level clamped to the content range (Registry §7.4).

### 3.7 Reference damage output and time-to-kill (TTK)

Used for authoring `boss_hp_factor`, for the publish-time simulation (Part 2, Content validation) and for the TTK floor (Part 2, Defeat validation).

```
ilvl_dps_factor(i, L) = DPS_ref(L, i) / DPS_ref(L, 4 × L)    (Combat canonical; scalar fallback k_ilvl ^ (i − 4 × L), k_ilvl = 1.0053, tune.dungeon.ilvl_dps_step)
dps_ref(L, i)         = k_dps × H_ref(L) × ilvl_dps_factor(i, L)   k_dps = 0.04  (tune.dungeon.ref_dps_fraction)
n_eq(group)           = Σ members: damage 1.0, support 0.6, tank 0.3, healer 0.3; solo player = 1.0; companion 0.3
TTK_ref               = HP / (n_eq × dps_ref(L_run, ilvl_ref))
```

Reference ilvl `ilvl_ref` (the gear a group is expected to bring): levelling `4 × L_run`; cap `normal` 400; `veteran` 405; `mythic` `min(450, 415 + 2 × min(M, 15))`. A standard 5-player group (1 tank, 1 healer, 3 damage) has `n_eq = 3.6`.

Resolved (2026-10-17): the gear factor is re-fitted to Combat spec, Reference throughput (canonical `DPS_ref(L, i)`, ≈ 0.53 % per ilvl above 400 plus the rare → epic step). `k_dps × H_ref(L)` equals `DPS_ref(L, 4L)` (e.g. 0.04 × 4,850 = 194.0 at L 100), so `dps_ref(L, i) = DPS_ref(L, i)`; `k_dps` 0.04 is unchanged. The old `1.015^(i − 4L)` overstated gear (epic 460: 2.44× vs Combat's 304.0 / 194.0 = 1.57×). The scalar fallback 1.0053 is valid only within one rarity (levelling gear at `4L` gives factor 1 either way); cap content uses the Combat table.

**Worked example 1 — veteran boss, 5 players.** Drowned King: `boss_hp_factor` 6.0, `level_offset` 2, `L_run` 100 → `L_npc` 102, `H_ref(102)` = 4,939.
HP = 4,939 × 5.0 × 6.0 × 1.000 × 1.35 × 1 = **200,030**.
`dps_ref(100, 405)` = 0.04 × 4,850 × (227.7 / 194.0) = 194 × 1.1737 = 227.7 (Combat epic 405); group = 3.6 × 227.7 = 819.7 → TTK = 200,030 / 819.7 ≈ **244 s** (target band 210–300 s ✓).
`abl_crushing_tide` (dmg_coef 0.30): 4,939 × 0.30 × 1.0 × 1.00 × 1.25 = **1,852** before mitigation; at 60 % tank mitigation (Combat endgame target) ≈ 741.

**Worked example 2 — same boss, solo `normal` levelling at L_run 22.** `L_npc` 24, `H_ref(24)` = 1,468. HP = 1,468 × 5 × 6 × 0.2349 × 1.0 = **10,345**. `dps_ref(22, 88)` = 0.04 × 1,379 × 1 = 55.2; n_eq 1.0 → TTK ≈ 187 s. Crushing Tide: 1,468 × 0.30 × 0.64 × 1.0 = 282 (≈ 20 % of the solo player's 1,379 reference health, before mitigation). `mech_grasp_interrupt` is disabled (`min_group_size` 2).

**Worked example 3 — Mythic 12, tyrant week.** `Y(12)` = 1.07^9 × 1.09^2 = 1.8385 × 1.1881 = 2.184. HP = 4,939 × 30 × 1.30 × 2.184 × 1.30 (tyrant) = **546,900** (rounded). `ilvl_ref` = 415 + 24 = 439 → `DPS_ref(100, 439)` = 258.8 (epic 430) × 1.0053^9 = 258.8 × 1.0487 = 271.4 → factor 271.4 / 194.0 = 1.399; group 3.6 × 271.4 = 977.0 → TTK = 546,900 / 977.0 ≈ 560 s. On entrenched weeks the boss is 420,700 HP → 420,700 / 977.0 ≈ 431 s. Both lie above the M ≤ 10 band by design: `Y(M)` outpaces the gear factor above M 10, so high keys are gated by gear beyond `ilvl_ref`, not only by execution.

**Worked example 4 — trash pack, extended party n = 8, veteran.** `pack/sv_tide_casters`: 3 × caster (common, base_count 3) + 1 × shaman (elite). count = round(3 × (0.6 + 0.64)) = round(3.72) = 4 casters. Caster HP = 4,850 × 1.0 × 1.0 × 1.527 × 1.35 = 9,998; shaman (`L_npc` 101, H_ref 4,894.5) HP = 4,894.5 × 2.5 × 1.527 × 1.35 = 25,224. Caster bolt (dmg_coef 0.06): 4,850 × 0.06 × 1.0 × 1.12 × 1.25 = 407.

### 3.8 TTK and run time targets

| Target | `story` | `normal` | `veteran` | `mythic` (at `ilvl_ref`, M ≤ 10) | Tuning key |
| --- | --- | --- | --- | --- | --- |
| Common pack | 15–25 s | 20–35 s | 25–45 s | 30–50 s | `tune.dungeon.ttk_pack_<d>` |
| Elite-heavy pack | 25–40 s | 30–50 s | 40–60 s | 45–70 s | — |
| Mini boss | 45–75 s | 60–100 s | 80–120 s | 90–140 s | — |
| Boss | 90–150 s | 150–240 s | 210–300 s | 240–360 s | `tune.dungeon.ttk_boss_<d>` |
| Full run | 15–25 min | 20–30 min | 25–35 min | par 28–36 min | `run_time_target_ms` (content) |

Open-world loop: 10–20 min. Seasonal: same as `instanced`.

---

## 4. Mythic dungeons

### 4.1 Access

There are no consumable keys. Each character has per-dungeon `best_timed_level` per season (`instance.mythic_progress`). The **access level** is:

```
unlock(dungeon) = best_timed_level(dungeon) + best_upgrade_at_that_level(dungeon)
access(character) = min(MYTHIC_LEVEL_MAX, max(1, max over season pool dungeons of unlock))
```

A run at Mythic Level `M` may be started if the **party leader**'s `access ≥ M`. Other members need only Character Level 100. Access resets each season (season start: everyone at 1; `tune.dungeon.mythic_season_carryover` = 0 levels).

### 4.2 Timer

```
T_limit = par_ms                                       (content; does not change with M)
elapsed = (t_complete − t_start) + deaths × death_penalty_ms
timed   = elapsed ≤ T_limit
upgrade = elapsed ≤ 0.6 × T_limit → 3 ; ≤ 0.8 × T_limit → 2 ; ≤ T_limit → 1 ; else 0
```

`death_penalty_ms` = 5,000 (`tune.dungeon.mythic_death_penalty_ms`); thresholds `tune.dungeon.mythic_upgrade_thresholds` = [0.6, 0.8, 1.0]. The timer starts when the first member crosses the entrance trigger after a 10 s countdown (doors open), pauses only for server recovery (Part 1), and stops at completion (§2 completion condition). There is no failure on overtime: the run continues and completes untimed.

Worked example: par 32:00 (1,920,000 ms); group finishes at 24:10 (1,450,000 ms) with 7 deaths → elapsed = 1,450,000 + 35,000 = 1,485,000 ms = 0.773 × par → timed, upgrade **+2**. Completed at M=9: `unlock` = 11.

### 4.3 Affix thresholds

| Mythic Level | Active affix slots |
| --- | --- |
| 1 | none |
| 2–3 | slot 1 (`entrenched` or `tyrant`) |
| 4–6 | slots 1–2 |
| 7–9 | slots 1–3 |
| 10–20 | slots 1–4 (slot 4 = season affix) |

### 4.4 Score (for listings and leaderboards)

```
dungeon_score = timed ? 20 × M + round(10 × (1 − elapsed / T_limit)) : 10 × M
mythic_rating = Σ over season pool dungeons of best dungeon_score
```

Example above: 20 × 9 + round(10 × 0.227) = 182. Leaderboards per (`dc_region`, `content_id`, `week_index`) list the top 100 timed runs by (M desc, elapsed asc); entries require all members to have no open `high`/`critical` detections (Security spec).

### 4.5 Timed-rate targets

| M | Target timed rate (all attempts) | Population reaching it (season end, of level-100 actives) |
| --- | --- | --- |
| 1–5 | 75–85 % | 70 % |
| 6–10 | 55–70 % | 40 % |
| 11–15 | 35–50 % | 15 % |
| 16–20 | 10–25 % (M20 < 5 %) | 2 % |

---

## 5. Dungeon affixes

### 5.1 Definition

```json
{
  "id": "dungeon_affix/volcanic",
  "schema_version": 1,
  "slot": 2,
  "min_mythic_level": 4,
  "name_key": "affix.volcanic.name",
  "modifiers": [],
  "behaviors": [
    { "trigger": "interval_in_combat", "interval_ms": 8000, "action": "spawn_hazard", "hazard": "hz_volcanic_plume",
      "placement": "under_random_ranged_player", "telegraph_ms": 2000, "dmg_coef": 0.25 }
  ],
  "banned_with": ["dungeon_affix/shattered_ground"],
  "tags": ["ground_hazard"],
  "also_in_normal_pool": false
}
```

`modifiers` are multiplicative stat changes with selectors, e.g. `{ "selector": "non_boss", "hp": 1.20, "dmg": 1.30 }`. `behaviors` use framework triggers: `interval_in_combat`, `on_npc_death`, `on_npc_health_pct`, `on_player_death`, `on_pack_engage`. Hazards and affix-spawned effects use the `placement` RNG stream (Part 2) so they are deterministic per attempt.

### 5.2 Launch pools (illustrative slugs)

| Slot | Affix | Effect summary |
| --- | --- | --- |
| 1 | `entrenched` | Non-boss NPCs: hp ×1.20, dmg ×1.30. |
| 1 | `tyrant` | Bosses: hp ×1.30, dmg ×1.15. |
| 2 | `volcanic` | Plumes under ranged players every 8 s in combat. |
| 2 | `bloodbond` | Dying non-boss NPCs leave a pool healing NPCs 4 % max hp/s for 6 s. |
| 2 | `rallying` | Dying non-boss NPCs grant nearby allies +20 % dmg and +20 % hp (max 3 stacks). |
| 2 | `frenzied` | Non-boss NPCs at ≤ 30 % hp: +50 % attack speed (dispellable `enrage`). |
| 2 | `shattered_ground` | Cracks spawn under tanks every 12 s; standing in them roots for 2 s. |
| 3 | `corrupting` | Player kills of non-boss NPCs add a stacking 1 % hp/s DoT for 6 s to the killer's group. |
| 3 | `stormcalled` | Every 30 s, chain lightning between players within 8 m. |
| 3 | `grievous_frost` | Players below 90 % hp gain stacking frost damage until healed to full. |
| 3 | `necrotic_echo` | Bosses' melee applies stacking −4 % healing received. |
| 4 | season affix | One per season (e.g. `rift_touched`), defined by LiveOps season content. |

### 5.3 Weekly rotation (generation algorithm)

Generated once per season at season publish by `instance-service`, stored in `instance.affix_rotations`, and announced weekly by `instance.affix.rotated` at each region's weekly reset (Registry §7.3). All regions use the same `week_index` schedule.

```
seed      = first 8 bytes of SHA-256("affix|" + season_id)
rng       = PCG32(seed, stream = 6 "affix")
weeks     = season_weeks (default 12, tune.dungeon.season_weeks)
slot1[w]  = (w even) ? entrenched : tyrant              -- alternation
for attempt in 1..1000:
    slot2 = rng-shuffled sequence over slot-2 pool, repeated to length weeks
    slot3 = rng-shuffled sequence over slot-3 pool, repeated to length weeks
    if constraints(slot2, slot3) hold: return rotation
fallback: authored rotation from season content; raise ops alert
```

Constraints:

1. The same slot-2 affix does not appear in 2 consecutive weeks; same for slot 3.
2. The pair (slot2, slot3) is unique within the season.
3. No `banned_with` pair is active in the same week.
4. Each slot-2 and slot-3 affix appears at least `floor(weeks / pool_size)` times.
5. No two affixes tagged `ground_hazard` together.

LiveOps can override a week (`AdminOverrideAffixWeek`, audited) up to the week's start; overrides must still pass constraints 3 and 5.

### 5.4 Non-mythic procedural modifiers

`normal` and `veteran` runs do not use affixes. Their variation comes from layout generation (§6). Seasonal dungeons always apply the season's slot-4 affix at reduced strength (`modifiers` scaled by `tune.dungeon.seasonal_affix_strength`, default 0.5).

---

## 6. Randomized packs, objectives and events (seeded generation)

### 6.1 Seed selection

- `story`, `normal`, `veteran`, `seasonal`: `layout_seed = run_seed` → each run differs.
- `mythic`: `layout_seed = mythic_layout_seed` (per dungeon per week) → every group faces the same layout that week; routes are learnable and leaderboards are fair.

### 6.2 Algorithm

```
rng = PCG32(layout_seed, stream = 7 "layout")
for attempt in 0..63:
    choices = {}
    for node in segments in topological order:
        pack_slot:      choices[node] = weighted_pick(candidates, rng), patrol = pick(patrol_paths, rng)
        objective_slot: choices[node] = pick_k(candidates, node.pick, rng)
        event_slot:     choices[node] = (rng.float() < probability) ? pick(candidates, rng) : none
    if validate(choices): return choices, layout_hash = SHA-256(canonical_json(choices))
    rng.advance(attempt + 1)
return authored default layout (flag default_layout = true, ops alert)
```

### 6.3 Validation constraints

| # | Constraint | Default |
| --- | --- | --- |
| V1 | Σ `threat_points` within `budget_points × (1 ± budget_tolerance)` | ±10 % |
| V2 | No identical pack template in adjacent pack slots | — |
| V3 | Every `required_tags` tag present at least once | e.g. `interrupt_test`, `caster` |
| V4 | Elites per segment ≤ `max_elites_per_segment` | 2 |
| V5 | No patrol path intersects an encounter arena volume or a mandatory objective area | — |
| V6 | Mythic: Σ `enemy_forces` of all packs ∈ [1.15, 1.35] × `enemy_forces_required` | route choice without full clear |
| V7 | Estimated clear time Σ `est_clear_ms_at_ref` + boss TTKs within `run_time_target_ms` ± 15 % | — |
| V8 | All required objectives reachable before the final encounter | graph check |

Generation is deterministic: the same `(layout_seed, content_version)` must produce the same `layout_hash` on every server build (property test, Part 4). The server reports the hash in `ReportServerReady`; `instance-service` stores it and checks it on every defeat.

### 6.4 Randomized objectives

Objective definitions (`objective/<slug>`, nested in the dungeon pack) use types `interact_count` (free 3 prisoners), `destroy` (seal 2 floodgates), `escort` (failure → `story` run `failed`; other difficulties: escort respawns after 30 s), `survive` (hold position for 60 s), `collect` (relic fragments from elites). Objectives report progress to the run checkpoint; `quest-service` consumes `encounter.attempt.defeated` and `instance.run.completed` for quest progress — dungeons never call quest-service directly.

Event slots (`dungeon_event/<slug>`) are short scripted set pieces: `ambush` (a pack spawns behind the group after a trigger), `gauntlet` (waves for 45–90 s, uses the encounter framework with `kind: event`), `collapsing_corridor` (timed traversal with hazards). `pack/`, `objective/` and `dungeon_event/` are registered content kinds owned by Dungeon.

---

## 7. Open-world dungeons

Open-world dungeons are shared areas inside a zone (World spec cells), hosted by `zone-server` with the same `LSFEncounter` module.

| Aspect | Rule |
| --- | --- |
| Instance run | None. Encounter attempts are stored with `zone_id`, `run_id` null. |
| Population | Shared; zone interest management applies (Network spec). |
| Bosses | Respawn after `respawn_ms` (default 900,000 ms) once defeated; `defeated` → `idle` on respawn. |
| Scaling | `n` = distinct engaged eligible players in the arena, re-evaluated every 5 s while `engaged`; health scales **up only** during an attempt (never down, to stop "leave to shrink" abuse). For `n > 10`: `G_hp(n) = G_hp(10) × (n / 10)`, capped at `n = 40`. |
| Level | `L_run` = zone content level (World spec). |
| Eligibility (tagging) | Contributed ≥ 2 % of total damage or healing on the primary NPC (`tune.dungeon.ow_tag_threshold`), or is in a party with a contributor and within 60 m at defeat. |
| Lockout | Soft: `period: cooldown`, 20 h per encounter per character. Trash never locked. |
| Rewards | `reward/<dungeon>_<encounter>_ow`; marks weight 0.3 (Level ≥ 60). |
| Exploit guard | Boss evades if pulled out of its arena volume; tagging requires presence inside arena when damage was dealt. |

---

## 8. Seasonal dungeons

- Content declares `season: "season/<slug>"`; it is queueable only while the matching runtime Season (`ssn_`) is `active` (`season_state`).
- Seasonal currency: rewards include `currency/season_tokens` via reward bundles (season caps owned by LiveOps).
- At `liveops.season.ended`, queues close; runs in progress continue until completion or max duration; rewards still issue and tokens convert per LiveOps rules.
- A seasonal dungeon may join the next season's mythic pool as an `instanced` dungeon (content republish with new `dungeon_type`; slug unchanged).

---

## 9. Rewards

### 9.1 Assignment conventions

| Reward point | Bundle ID | Loot table ID | `source` code |
| --- | --- | --- | --- |
| Boss / mini boss defeat | `reward/<dungeon>_<encounter>_<difficulty>` | `loot/<dungeon>_<encounter>_<difficulty>` | `dungeon_reward` |
| Run completion (non-mythic) | `reward/<dungeon>_completion_<difficulty>` | optional | `dungeon_reward` |
| Mythic end chest | `reward/<dungeon>_mythic_chest` (parameterized by `M`, `timed`, `upgrade`) | `loot/<dungeon>_mythic` | `dungeon_reward` |
| Mythic weekly cache | `reward/mythic_weekly_cache` | `loot/mythic_weekly_cache` | `dungeon_reward` |
| Open-world boss | `reward/<dungeon>_<encounter>_ow` | `loot/<dungeon>_<encounter>_ow` | `dungeon_reward` |
| LFG scarce-role bonus | `reward/lfg_role_bonus` | — | `dungeon_reward` |

Items dropping from NPC kills in the world simulation use `loot_drop`; all dungeon-authored rewards use `dungeon_reward`. `instance-service` passes `pity_key = "dungeon:<slug>:<difficulty>"` to `reward-service` (pity behavior owned by Itemization).

### 9.2 Item level and rarity (Registry §8 power ladder)

| Source | ilvl | Rarity ceiling |
| --- | --- | --- |
| Levelling dungeon at `L_run` | `4 × L_run` | `rare` (`epic` from final boss) |
| `normal` at cap | 405 | `rare` |
| `veteran` | 415 | `epic` |
| `mythic` end chest, timed | `420 + 2 × min(M, 15)` | `epic` |
| `mythic` end chest, untimed | `420 + 2 × min(max(M − 2, 1), 15)` | `epic` |
| Mythic weekly cache | `420 + 2 × min(M_best_week, 15)` | `epic` |

### 9.3 Loot modes (registry `loot_mode`)

| Group origin | Allowed modes | Default |
| --- | --- | --- |
| LFG-formed (`formed_by = lfg`) | `personal` only | `personal` |
| Premade `solo_party` | `personal`, `group_need_greed` | `personal` |
| Premade `extended_party` | `personal`, `group_need_greed`, `master_loot` | `personal` |

`council` is raid only. Mode is fixed at run creation. Group loot trading rules are in Part 4 (Security considerations).

### 9.4 Expected drops

| Reward point | Expected items per eligible player | Notes |
| --- | --- | --- |
| Non-final boss (`normal`, `veteran`) | 0.20 | `tune.dungeon.drop_rate_boss` |
| Final boss (`normal`, `veteran`) | 0.35 | `tune.dungeon.drop_rate_final` |
| Mythic chest, timed | 0.40 (+0.10 if upgrade ≥ 2) | `tune.dungeon.mythic_chest_rate` |
| Mythic chest, untimed | 0.25 | — |
| Mythic weekly cache | 1.00 (one per character per week) | requires ≥ 1 timed run that week |
| Crafting materials | 1 roll of `loot/dungeon_mats_<biome>_<band>` per boss | Crafting spec owns tables |

### 9.5 XP, gold and marks (Registry §7.4 functions)

Weights `w` per reward point:

| Reward point | `xp_reward(L, w)` w | `gold_reward(L, w)` w | `marks_reward(w)` w (Level ≥ 60) | Marks |
| --- | --- | --- | --- | --- |
| Levelling boss | 0.5 | 0.25 | — | — |
| Levelling final boss | 1.0 | 0.5 | — | — |
| Levelling completion | 1.5 | 0.5 | — | — |
| Cap `normal` boss / final | — | 0.25 / 0.5 | 0.2 / 0.4 | 10 / 20 |
| `veteran` boss / final | — | 0.3 / 0.6 | 0.5 / 1.0 | 25 / 50 |
| `mythic` chest | — | 0.8 | `(1.5 + 0.1 × min(M, 15)) × (timed ? 1 : 0.5)` | 40–150 |
| Open-world boss | 0.3 | 0.2 | 0.3 | 15 |
| LFG scarce-role bonus | — | 0.5 | 0.2 | 10 |

A levelling clear of a 3-boss dungeon gives `w_xp = 0.5 + 0.5 + 1.0 + 1.5 = 3.5` → `xp_reward(L, 3.5)` = 3.5/12 ≈ 29 % of a level, plus kill XP (Character spec). Example: Mythic 10 timed → w = 2.5 → 125 marks; at L=100 gold = `gold_reward(100, 0.8)` = round(0.8 × 100 × 101^1.5) = 81,204 copper ≈ 8.1 gold.

Marks are subject to `FRONTIER_MARKS_WEEKLY_CAP` (1,500) enforced by `economy-service`; reward bundles still issue items when the cap is hit (marks portion is truncated, logged as `capped`).

---

## 10. Economic and pacing targets

### 10.1 Frontier marks per week (Level 100, dungeon-focused player)

| Activity | Marks per run | Run length incl. overhead | Marks per hour | Hours to 1,000 from dungeons |
| --- | --- | --- | --- | --- |
| Cap `normal` (3 bosses) | 40 | 25 min | 96 | 10.4 |
| `veteran` (first daily clear per dungeon) | 100 | 32 min | 188 | 5.3 |
| `mythic` M5 timed | 100 | 38 min | 158 | 6.3 |
| `mythic` M10 timed | 125 | 38 min | 197 | 5.1 |
| `mythic` M15 timed | 150 | 40 min | 225 | 4.4 |

Targets: dungeons should supply **~1,000 of the 1,500** weekly cap for a dungeon-focused player in 5–7 hours; the remaining ~500 come from quests and world events (Quest and AI specs). Median Level-100 player: 600–900 marks/week from dungeons (`m_marks_from_dungeons_per_char_week`).

### 10.2 Gear pace

| Milestone (average equipped ilvl) | Main source | Target cumulative dungeon hours after reaching Level 100 |
| --- | --- | --- |
| 400 → 405 | cap `normal` | 4–6 h |
| 405 → 415 | `veteran` | 15–20 h |
| 415 → 430 | `mythic` M1–M5 + weekly cache | 35–45 h (≈ week 3–4) |
| 430 → 440 | `mythic` M6–M10 | 70–90 h (≈ week 6–8) |
| 440 → 450 | `mythic` M11–M15 | 120+ h (top 15 %, ≈ week 9–12) |

### 10.3 Session and run distribution targets

| Metric | Target |
| --- | --- |
| Share of runs completed (non-mythic) | ≥ 85 % |
| Median `veteran` run duration | 28–32 min |
| Median wipes per `veteran` run | ≤ 1.0 |
| Mythic runs per active mythic player per week | 6–12 |
| Share of runs that are `extended_party` | 10–20 % |
| Share of Level-100 actives running ≥ 1 dungeon per week | ≥ 60 % |
