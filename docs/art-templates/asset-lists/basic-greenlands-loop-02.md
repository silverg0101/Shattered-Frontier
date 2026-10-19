# Basic Greenlands playable-loop assets — batch 02

- Phase / scope revision: Basic / 2 (2026-10-18)
- Sources: `docs/Game-Design/Legends_Shattered_Frontier_Vertical_Slice_Specification_v1.0.md` — Scope and exclusions, Content inventory; `docs/Game-Design/Crafting/Legends_Shattered_Frontier_Crafting_Gathering_and_Resources_v1.0.md` — 1.4 Catalogue and 2.1 gathering_node definition; `docs/Game-Design/Crafting/Legends_Shattered_Frontier_Crafting_Recipes_Workshops_and_Learning_v1.0.md` — 3.1 Stations.
- Owner: Codex
- Dependencies: standalone assets; resource identities use common `oak_log` and `iron_ore` from the indexed illustrative catalogue. Shapes and exact material styling remain concept proposals.

| Asset ID | Category / main concept | Source path / heading | Priority / dependencies | Owner / job state | Concept / clean-view status | Selected package / next action |
|---|---|---|---|---|---|---|
| RESOURCE-GreenlandsOakNode-01 | Prop / one common oak logging node with readable trunk and harvestable branch/canopy mass | Vertical Slice / Content inventory; Crafting Gathering / 1.4 and 2.1 | P1; none; proposal for appearance | Codex / image-complete | agent-selected / ready-for-model-test | [v001](../../GameClient/ArtSource/RESOURCE-GreenlandsOakNode-01/References/v001/); inspected; manifest records hashes and review |
| RESOURCE-GreenlandsIronVein-01 | Prop / common iron-ore mining node, one exposed vein in a stable rock outcrop | Vertical Slice / Content inventory; Crafting Gathering / 1.4 and 2.1 | P2; none; proposal for appearance | Codex / image-complete | agent-selected / ready-for-model-test | [v001](../../GameClient/ArtSource/RESOURCE-GreenlandsIronVein-01/References/v001/); inspected; manifest records hashes and review |
| PROP-GreenlandsCraftStation-01 | Prop / compact communal refinement/crafting station with clear work surface and simple heat/tool cues | Vertical Slice / Content inventory; Crafting Recipes / 3.1 Stations | P3; resource nodes establish loop context but no visual dependency; specific recipe outputs unspecified | Codex / image-complete | agent-selected / ready-for-model-test | [v001](../../GameClient/ArtSource/PROP-GreenlandsCraftStation-01/References/v001/); corrected clean view to match open timber frame, see manifest |

## Component boundaries

Each node is an individual reusable resource opportunity, not a forest, mine, respawn system, tool or harvested item. The station is one physical station only, with no recipe UI or gameplay process implied. All unspecified design details are visual proposals; no dimensions or harvest rules are inferred.

## Progress

- Batch 02 gate: 3/3 assets image-complete and visually reviewed.
- Next asset: `BLD-GreenlandsFoundation-01` in batch 03.
- Blockers: none for image proposals. The broader scope calls for only wood and ore nodes; avoid expanding the resource catalog.
