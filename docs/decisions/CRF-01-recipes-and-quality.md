# CRF-01 — Learned recipes and crafted quality

Status: accepted by the owner 2026-10-22 ([owner decisions](OWNER-2026-10-22-decisions.md), "Crafting depth"): recipes are unlocked from trainers, books or practice; crafted items come in quality grades from skill and materials. Not implemented. One engineering session. Backlog rows [CRF-01/CRF-03](../Game-Design/Legends_Shattered_Frontier_Gameplay_Feature_Backlog_v1.0.md); design [Crafting entry C2 quality formula](../Game-Design/Legends_Shattered_Frontier_Resource_and_Crafting_Architecture_v1.0.md) and [Recipes, Workshops and Learning](../Game-Design/Crafting/Legends_Shattered_Frontier_Crafting_Recipes_Workshops_and_Learning_v1.0.md) §4 learning, §5 discovery; [Itemization](../Game-Design/Legends_Shattered_Frontier_Itemization_Architecture_v1.0.md) `quality_bonus` 0–50.

## What exists (build on, do not fork)

| Piece | Today | This spec |
| --- | --- | --- |
| `content/crafting/recipes_greenlands_starter.json` (9 recipes: station, duration, inputs, outputs) + `BackendServices/CraftingContent` validator | No skill, difficulty or learning data | Adds `skill`, `difficulty`, `learn`, `discovers`, `quality_inputs` |
| `Gameplay.Host` `Crafting.Craft` (ledger `Convert`, count 1..20, durable receipt by operation id) | Fixed outputs | Rolls quality, consumes chosen input grades, emits the graded output |
| `Gathering/SFStationRecipes` + station channel (`SFGatheringPilot`, [refining pilot](../gameplay-refining-pilot.md)) | C selects any shipped recipe | Lists known recipes; carries input grades |
| Progression pilot ([skills](../gameplay-gathering-progression-pilot.md)): server skill values, practice receipts, `CharacterState.Postgres` progression snapshot | Six gathering skills | Adds crafting skills and `known_recipes` to the snapshot |
| `InventoryLedger` quantities per `definition_id`, no item instances | No provenance | Grades are definition ids (below) |
| UI v5 `crafting-station.png`, `inventory.png` | Demo | Known/unknown recipes, grade preview, graded item names |

## Player-facing behaviour

1. **Starter recipes.** Tam, a new character, uses the sawmill: Rough Timber is known from the start. The panel shows "Expected: Common–Fine" from his Carpentry 20 and the oak logs' grade. He crafts; the result is 4 × Rough Timber (Fine). Fine timber stacks apart from Common timber and is named "Rough Timber (Fine)".
2. **Material quality carries.** Crafting iron nails from a Superior ingot gives better odds of Superior nails than from a Crude one. Grade also raises the next step's quality (M in the formula), nothing else for materials.
3. **Trainer.** Tam talks to the miller; a dialogue choice "Learn: Worked Timber (120 copper)" appears because his Carpentry is high enough. Accepting debits the coins and the recipe shows at the sawmill from then on, across logouts.
4. **Book.** A Recipe Scroll: Iron Brackets drops from the boar table or is sold on the market. Using it from the backpack consumes it and teaches the recipe. Using one for a recipe already known is refused and keeps the scroll.
5. **Practice.** After enough successful Iron Ingot crafts, Tam discovers "Iron Ingot, Tempered" (a variant with the same inputs and higher difficulty); a toast says what he learned.
6. **Unknown recipes.** The station lists recipes he cannot craft yet, greyed, with the reason: "Learn from the miller", "Needs a scroll", "Carpentry 150 to practise".

## Grades

Five grades bucket the design's `quality_score` 0–100. Material lot quality `M` for an input is its grade midpoint.

| Grade | `quality_score` | `M` | Definition id | Display |
| --- | --- | --- | --- | --- |
| Crude | 0–24 | 12 | `item/<slug>/crude` | "Rough Timber (Crude)" |
| Common | 25–49 | 37 | `item/<slug>` (unchanged) | "Rough Timber" |
| Fine | 50–74 | 62 | `item/<slug>/fine` | "Rough Timber (Fine)" |
| Superior | 75–89 | 82 | `item/<slug>/superior` | "Rough Timber (Superior)" |
| Masterwork | 90–100 | 95 | `item/<slug>/masterwork` | "Rough Timber (Masterwork)" |

**Conflict resolved.** The design keeps `quality_score` per item instance (`provenance.quality_score`, C2.3). The ledger has no instances; it counts quantities per definition. For MVP a grade is a sibling definition id, generated from the base item (same icon, name suffix, stack size). Existing content, quests, the Exchange allowlist and tests keep working because the ungraded id is the Common grade. Gathered materials (oak log, iron ore, boar hide, rough stone) stay ungraded = Common until the gathering lot quality (C5.2) is built. Construction bundles accept any grade; grade changes nothing structural (design §9, appearance only). Revisit when Itemization instances arrive (gear needs them anyway).

## Quality formula (MVP terms of C2.1)

```text
d          = S − D                                  S: crafter's skill 0..1000, D: recipe difficulty
Q_skill    = Q_SKILL_MAX × clamp((d + MARGIN_LOW) / (MARGIN_LOW + MARGIN_FULL), 0, 1)
Q_material = Q_MATERIAL_MAX × M / 100               M: quantity-weighted grade midpoint of quality_inputs
Q_tool     = Q_TOOL_MAX × DEFAULT_TOOL_QUALITY / 100 (fixed until tool quality exists)
Q_base     = Q_skill + Q_material + Q_tool          (workshop, mastery and luck terms are 0 in MVP)
v          = uniform integer in [−QUALITY_VARIANCE, +QUALITY_VARIANCE], seeded by hash(operation_id, host secret)
quality_score = clamp(round_half_up(Q_base + v), 0, 100)   → grade by the table above
```

Example: Carpentry 20, Rough Timber D 20, Common oak (M 37): `Q_skill = 50 × 100/300 = 16.7`, `Q_material = 7.4`, `Q_tool = 5` → 29 ± 5 → Common (Crude if the roll lands under 25). With Carpentry 220: `Q_skill = 50`, 62 ± 5 → Fine. Same seed, same result, so retries never re-roll. No success/failure roll in MVP (Owner question 1).

Practice discovery (design §5): on every successful craft of a recipe with `discovers[]`, `p = DISCOVERY_BASE × quality_score/100`; on hit learn one unknown candidate with `S ≥ learn_skill_min`, weighted draw, drawn from the same seeded stream after `v`.

## Content

Recipe fields added to `content/crafting/*.json` (validator: `CraftingContent.Tool`):

```json
{ "id": "recipe/worked_timber", "station": "sawmill", "duration_seconds": 6,
  "skill": "carpentry", "difficulty": 250,
  "inputs":  [ { "definition_id": "item/rough_timber", "quantity": 4, "quality_input": true },
               { "definition_id": "item/iron_nails",  "quantity": 2 } ],
  "outputs": [ { "definition_id": "item/worked_timber", "quantity": 2 } ],
  "learn": { "sources": ["trainer"], "learn_skill_min": 150, "trainers": ["npc/miller_hale"] },
  "discovers": [ { "recipe_id": "recipe/worked_timber_seasoned", "weight": 1 } ] }
```

- `skill` ∈ MVP crafting skills `carpentry`, `smithing`, `leatherworking` (registry names per Character Skills/Mastery; the progression adapter adds them beside the six gathering skills). `difficulty` 0..1100. `learn.sources` ⊆ `default | trainer | scroll | discovery`; `learn_skill_min` default `max(0, difficulty − 100)`. `trainers[]` NPC dialogue stems (required for `trainer`). A `scroll` source implies the item `item/recipe_scroll_<slug>` (category consumable, generated like graded items). `discovers[]` names recipes whose own `learn.sources` contain `discovery`.
- `quality_input` marks inputs that count in `M`; inputs without it (nails, hide strips as fasteners) do not dilute quality. A recipe accepts **any grade** of a listed input; the base id in content means "any grade".
- Starter content: the nine existing recipes gain `skill`/`difficulty` (`rough_timber` carpentry 20, `iron_ingot` smithing 40, `iron_nails` smithing 80, `hide_strips` leatherworking 30, bundles carpentry 60–120, `starter_tool_repair_kit` carpentry 100) and `learn.sources: ["default"]`. New: `recipe/worked_timber` (trainer, miller), `recipe/iron_brackets` (scroll; the scroll joins the Silverbrook boar loot table at low weight and the MKT-01 allowlist), `recipe/iron_ingot_tempered` (discovery from `iron_ingot`, D 140). Dialogue: miller and smith trees gain a `"action": "learn:<recipe id>"` choice; the dialogue tool checks the recipe lists that NPC.
- Graded definitions: `tools/Create-GradedItems.py` (run in the open editor via `tools/Run-InEditor.py`) creates `DA_<Name>_<Grade>` under `/Game/Frontier/Gameplay/Pilot/` for every crafted output and the scrolls, refusing overwrite, like `Create-EconomyPilotItems.py`. `tools/Check-ItemIds.py` keeps the UE, ledger and content id sets equal.

## Typed configuration

Backend `CraftingControls` (same shape as `ExchangeControls`: key, label, unit, default, min, max, permission `crafting.tuning.publish`, effective "new attempts"), snapshot captured per host start:

| Key | Default | Bounds | Unit |
| --- | --- | --- | --- |
| `tune.crafting.q_skill_max` | 50 | 0..100 | points |
| `tune.crafting.skill_margin_low` | 100 | 0..500 | skill |
| `tune.crafting.skill_margin_full` | 200 | 1..1000 | skill |
| `tune.crafting.q_material_max` | 20 | 0..100 | points |
| `tune.crafting.q_tool_max` / `default_tool_quality` | 10 / 50 | 0..100 | points / quality |
| `tune.crafting.quality_variance` | 5 | 0..25 | points |
| `tune.crafting.discovery_base` | 0.02 | 0..0.5 | probability |
| `tune.crafting.trainer_cost_per_difficulty_copper` | 0.5 | 0..50 | copper per difficulty point (120 copper at D 250, rounded) |
| `tune.crafting.max_known_recipes` | 500 | 50..5000 | recipes |

Cross-field check: `q_skill_max + q_material_max + q_tool_max ≤ 100`. Grade thresholds (25/50/75/90) are versioned code, not tuning (they change item identity). UE `USFGatheringPilotSettings` keeps station timing; add `CraftGradePreviewEnabled` (bool, default on) only.

## Server authority and durable owner

Per [DEC-02](DEC-02-durable-actions.md): local transactions per domain, replay by operation id, **InventoryLedger is the only item writer**.

- **Zone server** validates station reach/lock/channel as today and that the recipe is known to the character (cached from the last snapshot; the host re-checks). It sends `Craft{ operation_id, character_id, container_id, recipe_id, expected_container_revision, count, input_grades[] }` through the backpack FIFO (`SFBackpackComponent`, single writer, one `:r1` stale retry).
- **Gameplay.Host `Craft`**: reads the character's progression snapshot (skill `S`, `known_recipes`), refuses `recipe_unknown`; computes `M` from `input_grades`; rolls `quality_score` from the seeded stream; ledger `Convert` consumes the graded inputs and grants the graded outputs in one receipt whose fingerprint includes recipe, grades, count and `quality_score`; reply adds `quality_score`, `grade`, `discovered_recipe_id`. Replay returns the stored receipt; a changed payload with the same id is rejected. Practice gain stays the existing follow-up `ApplyReceipt` (skill `carpentry` etc., `difficulty` from the recipe, `success` true).
- **Known recipes** live in the CharacterState progression snapshot (`state.known_recipes[]: { recipe_id, source, learned_at }`), written under the character lock with CAS revision. `LearnRecipe{ operation_id, character_id, recipe_id, source, trainer_npc | scroll_container }` on the Progression service: `trainer` debits the Exchange wallet (sink `training_cost`, same operation id; refused `insufficient_gold`), `scroll` consumes one scroll via ledger `Consume` first, then records knowledge; both idempotent. If the wallet or ledger step committed and the knowledge write fails, the recovery sweep retries the knowledge write from the pending intent (never a second debit). `discovery` is recorded inside the craft transaction's follow-up intent.
- Restore: known recipes arrive with `GetCharacterState`; the station panel reads the replicated owner-only set.

## UI surfaces

[menus.md](../blueprints/menus.md) and [ui-hud.md](../blueprints/ui-hud.md) rules; concept `docs/Game-Design/concept-art/UI/v5/crafting-station.png`.

| Surface | Behaviour |
| --- | --- |
| Station panel `USFStationPanel` (C at a station, replaces the recipe cycle) | Left: known recipes for this station, then unknown ones greyed with the learn hint; right: inputs with a grade picker per `quality_input` (defaults to the lowest grade owned), count, duration, "Expected: <grade>–<grade>" from `Q_base ± variance`, Craft button |
| Result toast | "Rough Timber (Fine) × 4"; discovery toast "Learned: Iron Ingot, Tempered" |
| Dialogue choice | `learn:<recipe>` rows show the price and are disabled with the reason below `learn_skill_min` or without coins |
| Inventory (`SFInventoryPanel`) | Graded stacks show the suffix and a grade pip in the slot; scroll has a "Use" action |
| Skills panel (M) | Adds the three crafting skills rows |

## Acceptance tests

1. **Deterministic quality.** `CraftingContent.Tests`: formula examples above, boundary scores 24/25, 89/90; same operation id → same score; different id → different `v` within bounds.
2. **Grade conservation.** Convert consumes exactly the chosen grades and grants the graded output; a retry with the same id adds nothing; a changed `input_grades` with the same id is rejected (`Test-GameplayHosts.ps1` crafting leg).
3. **Unknown recipe refused** by the host even when the client claims it; known after `LearnRecipe`; survives host restart and reconnect (`Test-CharacterStateDurable.ps1` snapshot case).
4. **Trainer.** Below `learn_skill_min` → `skill_too_low`, no debit. With coins → debit once, recipe known; replay → same receipt. Insufficient coins → refused, nothing written.
5. **Scroll.** Use consumes one scroll and teaches; using a second scroll for a known recipe → `already_known`, scroll kept. Kill the host between consume and knowledge write → recovery completes knowledge, scroll count unchanged.
6. **Discovery.** With `discovery_base` forced to 1.0 in the fixture, the first successful craft learns the variant exactly once; with 0 never.
7. **Content.** Tool rejects: unknown skill, `trainer` without trainers, `discovers` naming a recipe without `discovery`, `quality_input` on a non-input, graded id used in content inputs, weights exceeding 100.
8. **UE.** `ShatteredFrontier.Crafting.*`: station lists known/unknown correctly from the replicated set; preview range matches the backend formula for fixed inputs; `tools/Test-GatheringPilot.ps1 -Refining` extended with grade assertions (timber Fine from a high-skill fixture character).
9. **Visual.** Station panel and inventory captures vs concepts, fresh reviewer.

## Jobs (in order)

| # | Job | Size |
| --- | --- | --- |
| 1 | Content + validator: recipe fields, starter values, three new recipes, scroll items, dialogue `learn:` action check; `Create-GradedItems.py`, `Check-ItemIds.py` | M |
| 2 | Backend: `CraftingControls`, pure quality/grade/discovery functions with tests, `Craft` input grades + graded outputs + reply fields, fingerprint | M |
| 3 | Backend: `known_recipes` in the progression snapshot, `LearnRecipe` RPC (trainer debit, scroll consume, recovery intent), crafting skills in the progression adapter | L |
| 4 | UE server: craft intent with grades, known-set replication, `learn:` dialogue action, scroll use from inventory | M |
| 5 | UE UI: station panel, grade preview, graded names/pips, toasts, skills rows | M |
| 6 | Tests and runners: content/backend tests, GameplayHosts crafting leg, `Test-GatheringPilot.ps1 -Refining` grades, captures + fresh review | M |
| 7 | `docs/dependency-map.md` rows; refining pilot doc note | S |

## Owner questions

1. **Can a craft fail?** (a) Never: a crafter below the recipe's skill just gets a Crude result. (b) It can fail below the recipe's skill and lose half the materials. (c) No failure for simple materials, failure only for advanced items later. Spec assumes (a).
2. **Showing recipes you do not know yet.** (a) Show them greyed with where to learn them. (b) Hide them until learned, so discovery is a surprise. Spec assumes (a).
3. **Trainer prices.** (a) A small fee that grows with the recipe's difficulty (about 120 copper for the first trainer recipe). (b) Free during alpha. (c) A short favour quest instead of coins. Spec assumes (a).
