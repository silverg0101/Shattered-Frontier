# Intermediate Greenlands kit and variants — batch 05

- Phase / scope revision: Intermediate / 1 (2026-10-18)
- Sources: `docs/Game-Design/Legends_Shattered_Frontier_Vertical_Slice_Specification_v1.0.md` — Scope and exclusions, Content inventory; `docs/Game-Design/Crafting/Legends_Shattered_Frontier_Crafting_Recipes_Workshops_and_Learning_v1.0.md` — 9.2 Builder's Cache; `docs/Game-Design/Legends_Shattered_Frontier_Construction_Integration_Contracts_v0.1.md` — 2 Container extension and 9 Building service and domain contracts.
- Owner: Codex
- Dependencies: `BLD-GreenlandsStarterShelter-01` assembles Basic batch 03 foundation/wall/door/roof proposals; window and Builder's Cache are separate proposed physical assets with defined content roles.

| Asset ID | Category / main concept | Source path / heading | Priority / dependencies | Owner / job state | Concept / clean-view status | Selected package / next action |
|---|---|---|---|---|---|---|
| BLD-GreenlandsStarterShelter-01 | Building / assembled small starter shelter using the four existing Basic component concepts | Vertical Slice / Housing, Content inventory; batch 03 package links | P1; depends on foundation, wall panel, door and roof concepts | Codex / image-complete | agent-selected / ready-for-model-test | [v001](../../GameClient/ArtSource/BLD-GreenlandsStarterShelter-01/References/v001/); clean view corrected to shingles, see manifest |
| BLD-GreenlandsWindowPanel-01 | Building / one separate timber-framed window wall panel matching the starter shelter kit | Vertical Slice / Settlement and Housing, Content inventory | P2; style follows Basic shelter kit; window is a proposal, not a required fifth piece | Codex / image-complete | agent-selected / ready-for-model-test | [v001](../../GameClient/ArtSource/BLD-GreenlandsWindowPanel-01/References/v001/); inspected, proposal |
| PROP-GreenlandsBuildersCache-01 | Prop / sturdy lockable construction-material cache with clear lid, hinges and stable footprint | Crafting Recipes / 9.2; Construction Integration / 2 and 9 | P3; visual reference to crafting/construction role only; capacity/security not inferred | Codex / image-complete | agent-selected / ready-for-model-test | [v001](../../GameClient/ArtSource/PROP-GreenlandsBuildersCache-01/References/v001/); stone plinth rejected; corrected to standalone timber chest |

## Component boundaries

The shelter is a single assembled building study, not a substitute for separately modeled pieces. It must visually reuse the Basic component designs and cannot redefine their interfaces. The window is a single wall-module variant, not an additional approved requirement. The cache is one physical container only; exclude inventory contents, access UI, locks with inferred security behavior, capacity or recipe rules.

## Progress

- Batch 05 gate: 3/3 assets image-complete and reviewed; the optional window remains a proposal.
- Blockers: none for visual concepts; module interfaces remain unspecified and are not claimed as resolved by images.
