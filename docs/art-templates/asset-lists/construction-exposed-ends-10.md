# Construction exposed ends and returns — batch 10

- Phase / scope revision: Construction kits / 1 (2026-10-18)
- Sources: `docs/art-templates/player-construction-kit-contract.md` — Corners and exposed ends; Canonical reference frame; Construction Architecture §§3.1–3.2, 8–9.
- Owner: Codex
- Dependencies: proposed `cell-full/v1`; prior KIT-RT/KIT-SM face packages; preserve the 0.5 m envelope, contacts and gameplay profile.

| Asset ID | Category / main concept | Source path / heading | Priority / dependencies | Owner / job state | Concept / clean-view status | Selected package / next action |
| --- | --- | --- | --- | --- | --- | --- |
| BLD-RT-EndExposedCellFull-01 | Rough timber / exposed full-cell end faces, both ends | Contract Corners and exposed ends; Construction §3.2 | P1; KIT-RT `cell-full/v1`; match exterior/interior/top/underside | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-RT-EndExposedCellFull-01/References/v001/); sheet/view reviewed, rejected people silhouette retained |
| BLD-SM-EndExposedCellFull-01 | Stone masonry / exposed full-cell end faces, both ends | Contract Corners and exposed ends; Construction §3.2 | P1; KIT-SM `cell-full/v1`; match exterior/interior/top/underside | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-SM-EndExposedCellFull-01/References/v001/); sheet/view reviewed; course relief is visual only |
| BLD-RT-ReturnEdgeCellFull-01 | Rough timber / full-cell edge return | Contract Corners and exposed ends; Construction §3.2 | P1; KIT-RT; return surfaces stay within envelope | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-RT-ReturnEdgeCellFull-01/References/v001/); reviewed; rejected stacked scene retained |
| BLD-SM-ReturnEdgeCellFull-01 | Stone masonry / full-cell edge return | Contract Corners and exposed ends; Construction §3.2 | P1; KIT-SM; return surfaces stay within envelope | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-SM-ReturnEdgeCellFull-01/References/v001/); reviewed; block relief visually ambiguous |

## Component briefs

Each image package is a distinct appearance role on the proposed 0.5 m full cube. Show front, back, side, top and underside orientation/contact target plus a neutral silhouette; no chamfers, grain, mortar, pegs or trim may move the outer plane. Clean view isolates one component. Keep appearance proposals distinct from authored geometry. Images do not establish dimensions, collision or runtime fit.

- `BLD-RT-EndExposedCellFull-01`: two end faces with end-grain timber and restrained joinery coherent with rough-timber face packages. Keep the full cube and six contacts.
- `BLD-SM-EndExposedCellFull-01`: two exposed masonry ends with matching stone courses and mortar; no projecting stones.
- `BLD-RT-ReturnEdgeCellFull-01`: visual turn from exterior face onto a perpendicular adjoining surface; show the return edge and both face directions within the same cell envelope.
- `BLD-SM-ReturnEdgeCellFull-01`: stone-course return at a perpendicular surface; courses align across the corner without protruding beyond the envelope.

## Progress

- Planned: 0; in-progress: 0; complete: 4; blocked/deferred: 0. All selected concept sheets and separate clean views visually reviewed; image gate passed.
- Next asset: `BLD-RT-CornerInsideCellFull-01` in batch 11.
- Geometry-fit, runtime collision/support and dimensional acceptance remain open. Next batch: inside/outside corners and end caps; see the 38-role [matrix](construction-kit-matrix-07.md). The matrix counts individual components separately from assembly studies.
