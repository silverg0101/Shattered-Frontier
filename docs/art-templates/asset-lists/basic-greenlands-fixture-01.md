# Basic Greenlands fixture assets — batch 01

- Phase / scope revision: Basic / 1 (2026-10-18)
- Sources: `docs/e1-implementation-packet.md` — “Greenlands fixture and acceptance cards”, E1-B; `docs/evidence/e1-foundation.md` — visual/manual movement notes; `GameClient/ArtSource/GreenlandsWaystone/README.md` — existing optional landmark study.
- Owner: Codex
- Dependency: retain the existing `GreenlandsWaystone` design as the arrival landmark; all other objects below are visual proposals for the graybox fixture, not gameplay requirements or approved art direction.

| Asset ID | Category / main concept | Source path / heading | Priority / dependencies | Owner / job state | Concept / clean-view status | Selected package / next action |
|---|---|---|---|---|---|---|
| TERR-GreenlandsArrivalPatch-01 | Terrain / compact walkable forest-floor arrival patch with gentle surface transition and safe landing shelf | `docs/e1-implementation-packet.md` — Greenlands fixture and acceptance cards, E1-B | P1; composition independent; proposal | Codex / image-complete | agent-selected / ready-for-model-test | [v001](../../GameClient/ArtSource/TERR-GreenlandsArrivalPatch-01/References/v001/); inspected sheet and clean view; review notes in package |
| BLD-GreenlandsWallCorner-01 | Building / short joined timber-and-stone wall corner module with clear inner/outer faces | same E1-B section: solid wall/corner and camera recovery | P2; component boundary is one corner module; proposal | Codex / image-complete | agent-selected / ready-for-model-test | [v001](../../GameClient/ArtSource/BLD-GreenlandsWallCorner-01/References/v001/); two failed views corrected, see manifest |
| PROP-GreenlandsWayfindingMarker-01 | Prop / small ground-level trail marker with a simple directional notch, no text or emblem | same E1-B section: stable landmark and clear return navigation | P3; distinct from reused waystone and not a gameplay beacon; proposal | Codex / image-complete | agent-selected / ready-for-model-test | [v001](../../GameClient/ArtSource/PROP-GreenlandsWayfindingMarker-01/References/v001/); corrected timber foot to match sheet |

## Briefs and component boundaries

### TERR-GreenlandsArrivalPatch-01
One small, bounded forest-floor terrain patch only: packed earth, short grass, a modest rise and stable landing shelf. It is a modeling study, not a runtime terrain system or biome. Exclude the waystone, wall module, vegetation assets and surrounding landscape; those are separate assets. Required: concept study, separate clean patch view; additional views only if the patch edges/topology are unreadable. Approximate scale is inferred/proposed.

### BLD-GreenlandsWallCorner-01
One short straight wall segment joined to one return at a right angle, showing a plausible stone footing and restrained timber construction. One component only; no complete building, gate, floor, or gameplay collision dimensions. Required: module sheet and separate clean corner view. Height, materials, and joining details are proposals.

### PROP-GreenlandsWayfindingMarker-01
One small freestanding wooden trail marker with a broad stable foot and a carved directional notch, no writing, logo, faction mark, or extra prop. It is separate from the already existing waystone and is a proposal, not a new navigation mechanic. Required: prop sheet and separate clean view.

## Progress

- Batch 01 gate: all 3 planned concepts image-complete after visual review. Reused `GreenlandsWaystone` remains an existing optional, non-imported study (not counted as newly generated). This closes only the E1 arrival-fixture concept batch; it does not certify all game-wide basic assets.
- Next asset: review the documented vertical-slice scope and create the next finite basic batch (resources, crafting, equipment or ordinary creatures only where those are needed by the earliest playable scope).
- Blockers: none for concept proposals. User/design approval remains separate from image completion.
