# Rough-timber cell face components — batch 08

- Phase / scope revision: Construction kits / 1 (2026-10-18)
- Sources: `player-construction-kit-contract.md` — canonical frame and Main faces; Construction Architecture §§3.1–3.2, 8–9.
- Owner: Codex
- Dependencies: batch 07 selected `KIT-RT` rough timber cell style and `cell-full/v1` proposal. Its artwork is material guidance only; `cell-full` geometry is still proposed, not accepted.

| Asset ID | Category / main concept | Source path / heading | Priority / dependencies | Owner / job state | Concept / clean-view status | Selected package / next action |
|---|---|---|---|---|---|---|
| BLD-RT-FaceExteriorCellFull-01 | Rough timber kit / separately indexed exterior-face treatment on `cell-full` | Contract Main faces; Construction §3.2 | P1; KIT-RT, `cell-full/v1` proposed; preserve six contacts | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-RT-FaceExteriorCellFull-01/References/v001/); sheet/view reviewed; fit not established |
| BLD-RT-FaceInteriorCellFull-01 | Rough timber kit / separately indexed interior-face treatment on `cell-full` | Contract Main faces; Construction §3.2 | P1; KIT-RT, same `cell-full/v1`; interior and exterior differ only in finish | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-RT-FaceInteriorCellFull-01/References/v001/); sheet/view reviewed; fit not established |
| BLD-SM-FaceExteriorCellFull-01 | Stone masonry kit / separately indexed exterior-face treatment on `cell-full` | Contract Main faces; Construction §3.2 | P1; KIT-SM, same neutral `cell-full/v1` | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-SM-FaceExteriorCellFull-01/References/v001/); reviewed after four rejected iterations; fit not established |
| BLD-SM-FaceInteriorCellFull-01 | Stone masonry kit / separately indexed interior-face treatment on `cell-full` | Contract Main faces; Construction §3.2 | P1; KIT-SM, same neutral `cell-full/v1`; no tier/profile blending | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-SM-FaceInteriorCellFull-01/References/v001/); sheet/view reviewed; geometry-fit not established |

Each row is one appearance-component reference, not a whole shape definition. The separate clean view centers the target face in front while retaining the same complete solid cube envelope. Do not create a separate voxel, collision, occupancy mask or material profile per face. Geometry dimensions/contacts come from the contract, not these images. Matching top/underside/exposed ends remain separate roles. This batch does not complete the kit or replace its assembly studies.

## Progress

- Batch gate passed: 4/4 image-complete (`BLD-RT-FaceExteriorCellFull-01`, `BLD-RT-FaceInteriorCellFull-01`, `BLD-SM-FaceExteriorCellFull-01`, `BLD-SM-FaceInteriorCellFull-01`); selected sheets and clean views reviewed. Ambiguous/relieved iterations were rejected and retained in prompts/manifests.
- Next asset: `BLD-RT-FaceTopCellFull-01` in batch 09.
- Remaining roles are indexed in [batch 07's full matrix](construction-kit-matrix-07.md); the same finite matrix applies to all five kit IDs. Blockers: none for appearance references.
