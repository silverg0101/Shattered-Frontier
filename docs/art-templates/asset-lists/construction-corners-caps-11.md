# Construction corners and end caps — batch 11

- Phase / scope revision: Construction kits / 1 (2026-10-18)
- Sources: `docs/art-templates/player-construction-kit-contract.md` — Corners and exposed ends; canonical reference frame; Construction Architecture §§3.1–3.2, 8–9.
- Owner: Codex
- Dependencies: proposed geometry `corner-inner/v1`, `corner-outer/v1`, and `cell-full/v1`; prior face/end/return appearance references. Preserve shape masks, contacts, openings, clearances and gameplay profiles.

| Asset ID | Category / main concept | Source path / heading | Priority / dependencies | Owner / job state | Concept / clean-view status | Selected package / next action |
| --- | --- | --- | --- | --- | --- | --- |
| BLD-RT-CornerInsideCellFull-01 | Rough timber / concave full-cell corner | Contract Corners and exposed ends; Construction §3.2 | P1; KIT-RT; `corner-inner/v1` proposal; matching slope contours | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-RT-CornerInsideCellFull-01/References/v001/); sheet/view reviewed; exact mask open |
| BLD-RT-CornerOutsideCellFull-01 | Rough timber / convex full-cell corner | Contract Corners and exposed ends; Construction §3.2 | P1; KIT-RT; `corner-outer/v1` proposal; matching slope contours | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-RT-CornerOutsideCellFull-01/References/v001/); wedge views reviewed; fit open |
| BLD-SM-CornerInsideCellFull-01 | Stone masonry / concave full-cell corner | Contract Corners and exposed ends; Construction §3.2 | P1; KIT-SM; `corner-inner/v1` proposal; matching slope contours | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-SM-CornerInsideCellFull-01/References/v001/); relief ambiguity recorded |
| BLD-SM-CornerOutsideCellFull-01 | Stone masonry / convex full-cell corner | Contract Corners and exposed ends; Construction §3.2 | P1; KIT-SM; `corner-outer/v1` proposal; matching slope contours | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-SM-CornerOutsideCellFull-01/References/v001/); wedge views reviewed; fit open |

## Component briefs

Each package is a single proposed corner appearance on the exact named occupied shape; label the empty/solid silhouette and mating slope/contact planes. Concept sheet includes orthographic faces, top, underside, neutral silhouette, and a small contact diagram; clean view shows only one corner component. The reference 0.5 m cell envelope is retained, but `corner-inner` is the union of matching wedges and `corner-outer` is the convex wedge, not a full cube. Do not fill the inner void, soften the mating contours, or add supports. Timber grain and masonry courses remain inside the target envelope. Images cannot verify occupied masks, fit, collision, support or clearances.

## Progress

- Planned: 0; complete: 4; blocked/deferred: 0. Selected sheets and separate clean views reviewed; image gate passed.
- Next asset: `BLD-RT-EndCapCellFull-01` in batch 12.
- End caps remain a separate queued role in the matrix; this batch prioritizes the distinctly shaped inside/outside geometry pairs.
