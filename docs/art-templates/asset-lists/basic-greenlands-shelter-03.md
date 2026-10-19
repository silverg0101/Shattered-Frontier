# Basic Greenlands shelter components — batch 03

- Phase / scope revision: Basic / 2 (2026-10-18)
- Sources: `docs/Game-Design/Legends_Shattered_Frontier_Vertical_Slice_Specification_v1.0.md` — Scope and exclusions, Content inventory; `docs/Game-Design/Legends_Shattered_Frontier_Construction_Integration_Contracts_v0.1.md` — 9 Building service and domain contracts.
- Owner: Codex
- Dependencies: one coherent starter-shelter visual family. Match the material language of the completed `BLD-GreenlandsWallCorner-01` proposal where practical; that corner module is a visual guide, not a canonical design or a substitute for these four required pieces.

| Asset ID | Category / main concept | Source path / heading | Priority / dependencies | Owner / job state | Concept / clean-view status | Selected package / next action |
|---|---|---|---|---|---|---|
| BLD-GreenlandsFoundation-01 | Building / one modular stone-and-timber shelter foundation panel | Vertical Slice / Content inventory; Construction Integration / 9 | P1; starter family | Codex / image-complete | agent-selected / ready-for-model-test | [v001](../../GameClient/ArtSource/BLD-GreenlandsFoundation-01/References/v001/); reviewed; modular dimensions remain undefined |
| BLD-GreenlandsWallPanel-01 | Building / one plain framed wall panel for starter shelter | Vertical Slice / Content inventory; Construction Integration / 9 | P2; align with `BLD-GreenlandsWallCorner-01` style | Codex / image-complete | agent-selected / ready-for-model-test | [v001](../../GameClient/ArtSource/BLD-GreenlandsWallPanel-01/References/v001/); sheet/view checked |
| BLD-GreenlandsDoor-01 | Building / one simple hinged door panel with readable frame and hardware | Vertical Slice / Content inventory; Construction Integration / 9 | P3; align with wall panel style | Codex / image-complete | agent-selected / ready-for-model-test | [v001](../../GameClient/ArtSource/BLD-GreenlandsDoor-01/References/v001/); clean view corrected to match sheet |
| BLD-GreenlandsRoof-01 | Building / one simple pitched roof module with visible support and weather-shedding surface | Vertical Slice / Content inventory; Construction Integration / 9 | P4; align with wall panel style | Codex / image-complete | agent-selected / ready-for-model-test | [v001](../../GameClient/ArtSource/BLD-GreenlandsRoof-01/References/v001/); sheet/view checked |

## Component boundaries

Each row is one separately reconstructable shelter piece, not an assembled building. Connection geometry and exact sizes remain proposals pending the owning construction content definition; do not claim image dimensions as placement/collision dimensions. The required piece list comes from the vertical-slice minimum-content inventory.

## Progress

- Batch 03 gate: 4/4 shelter pieces image-complete and visually reviewed; connection sizes remain unresolved construction-content decisions.
- Next asset: `WEAPON-GreenlandsStarterMelee-01` in batch 04.
- Blockers: none for conceptual work; exact modular interface/placement standard is not specified by this visual brief and remains a later construction-content decision.
