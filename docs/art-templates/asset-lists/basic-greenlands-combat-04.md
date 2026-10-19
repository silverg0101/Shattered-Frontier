# Basic Greenlands combat concepts — batch 04

- Phase / scope revision: Basic / 2 (2026-10-18)
- Sources: `docs/Game-Design/Legends_Shattered_Frontier_Vertical_Slice_Specification_v1.0.md` — Scope and exclusions, Region layout and encounter envelope, Content inventory; `docs/Game-Design/Legends_Shattered_Frontier_Combat_System_v0.1.md` — creature-tier rule (visual source context only).
- Owner: Codex
- Dependencies: none for concept generation. The weapon and enemy are separate concepts. Their exact identity is not provided, so both designs remain explicitly proposed.

| Asset ID | Category / main concept | Source path / heading | Priority / dependencies | Owner / job state | Concept / clean-view status | Selected package / next action |
|---|---|---|---|---|---|---|
| WEAPON-GreenlandsStarterMelee-01 | Weapon / practical, low-complexity starter melee weapon with readable grip and safe construction | Vertical Slice / Scope and exclusions, Combat; Content inventory | P1; none; proposed identity/materials | Codex / image-complete | agent-selected / ready-for-model-test | [v001](../../GameClient/ArtSource/WEAPON-GreenlandsStarterMelee-01/References/v001/); sword identity proposed; corrected curved guard |
| CREATURE-GreenlandsStarterEnemy-01 | Creature / one readable, modest-threat enemy silhouette for the Greenlands clearing | Vertical Slice / Scope and exclusions, Region layout and encounter envelope, Content inventory | P2; no extra enemy family; proposed species and anatomy | Codex / image-complete | agent-selected / ready-for-model-test | [v001](../../GameClient/ArtSource/CREATURE-GreenlandsStarterEnemy-01/References/v001/); bramble boar species is proposed |

## Component boundaries

The weapon is a single item only; exclude wielder, attack effects and balance claims. The enemy is one standalone creature concept; exclude gear variants, additional enemies, encounter scene and attack VFX. Art does not define damage, tier, behavior, spawn rules or progression.

## Progress

- Batch 04 gate: 2/2 concepts image-complete and visually reviewed.

## Progress

- Basic scope completion gate (2026-10-18): all 12 scoped concepts are image-complete and reviewed across batches 01–04. Existing optional Greenlands Waystone art was reused and not regenerated. Scope covers only physical image assets in the vertical-slice minimum content inventory plus three E1 arrival-fixture proposals; it excludes UI/audio/animation and does not claim production approval, runtime integration, alpha segmentation or every future-game asset.
- Next phase: Intermediate kit assembly and proposals; detailed signature scope follows in its own indexed batch.
- Blockers: none for visual proposals. Species, weapon identity and exact encounter content need later owner selection before production approval.
