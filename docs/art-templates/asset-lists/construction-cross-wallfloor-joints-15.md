# Construction cross joints and wall/floor interfaces — batch 15

- Phase / scope revision: Construction kits / 1 (2026-10-18)
- Sources: `docs/art-templates/player-construction-kit-contract.md` — Structural joints; Required kit QA; Construction Architecture §§3.1–3.2, 8–9.
- Owner: Codex
- Dependencies: KIT-RT/KIT-SM `cell-full/v1`, six contact planes; distinct wall/floor members; no material-profile blending or support-rule implication.

| Asset ID | Category / main concept | Source path / heading | Priority / dependencies | Owner / job state | Concept / clean-view status | Selected package / next action |
| --- | --- | --- | --- | --- | --- | --- |
| BLD-RT-JointCrossCellFull-01 | Rough timber / four-way cross joint | Contract Structural joints; Construction §3.2 | P1; KIT-RT; full-face cross contacts | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-RT-JointCrossCellFull-01/References/v001/); reviewed appearance, fit unresolved |
| BLD-SM-JointCrossCellFull-01 | Stone masonry / four-way cross joint | Contract Structural joints; Construction §3.2 | P1; KIT-SM; full-face cross contacts | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-SM-JointCrossCellFull-01/References/v001/); reviewed appearance, fit unresolved |
| BLD-RT-JointWallFloorCellFull-01 | Rough timber / wall-to-floor seam | Contract Structural joints; Construction §3.2 | P1; KIT-RT; named full-cell contacts | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-RT-JointWallFloorCellFull-01/References/v001/); multi-course seam study, scale unresolved |
| BLD-SM-JointWallFloorCellFull-01 | Stone masonry / wall-to-floor seam | Contract Structural joints; Construction §3.2 | P1; KIT-SM; named full-cell contacts | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-SM-JointWallFloorCellFull-01/References/v001/); multi-block seam study, scale unresolved |

## Component briefs

Each row packages one separately indexed surface/joint appearance and its supporting assembly study. The cross-joint panel shows four arms meeting in the grid; wall/floor panel labels the horizontal floor contact and vertical wall contact through clear views. Include a separate full-cell source component. Preserve the existing 0.5 m cube envelope, planar contacts and style. Do not add brackets, floor overhangs, joists or implied support capacity. Images cannot define exact contacts or runtime rules.

## Progress

- Planned: 0; complete: 4; blocked/deferred: 0. Reviewed as image references; seam-scale, numeric fit, collision and runtime compatibility remain unverified.
- Next asset: `BLD-RT-RectOpeningJamb-01` in batch 17.
- Wall/ceiling and beam/post joins, then openings, roof, curve and assembly-study gaps remain in the matrix.
