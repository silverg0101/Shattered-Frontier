# Construction end caps and inner slope corners — batch 12

- Phase / scope revision: Construction kits / 1 (2026-10-18)
- Sources: `docs/art-templates/player-construction-kit-contract.md` — Corners and exposed ends; Roof and slope transitions; Construction Architecture §§3.1–3.2, 8–9.
- Owner: Codex
- Dependencies: proposed `cell-full/v1` end treatment and `corner-inner/v1` slope contour; KIT-RT and KIT-SM references; all contact faces and gameplay profiles remain fixed.

| Asset ID | Category / main concept | Source path / heading | Priority / dependencies | Owner / job state | Concept / clean-view status | Selected package / next action |
| --- | --- | --- | --- | --- | --- | --- |
| BLD-RT-EndCapCellFull-01 | Rough timber / full-cell exposed end cap treatment | Contract Corners and exposed ends; Construction §3.2 | P1; KIT-RT; `cell-full/v1`; no envelope growth | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-RT-EndCapCellFull-01/References/v001/); end grain reviewed; cap is appearance only |
| BLD-SM-EndCapCellFull-01 | Stone masonry / full-cell exposed end cap treatment | Contract Corners and exposed ends; Construction §3.2 | P1; KIT-SM; `cell-full/v1`; no envelope growth | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-SM-EndCapCellFull-01/References/v001/); cut-face treatment reviewed |
| BLD-RT-SlopeCornerInner-01 | Rough timber / inner 1:1 slope corner | Contract Roof and slope transitions; Construction §3.2 | P1; KIT-RT; `corner-inner/v1` slope contact contours | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-RT-SlopeCornerInner-01/References/v001/); corrected unlabeled sheet selected |
| BLD-SM-SlopeCornerInner-01 | Stone masonry / inner 1:1 slope corner | Contract Roof and slope transitions; Construction §3.2 | P1; KIT-SM; `corner-inner/v1` slope contact contours | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-SM-SlopeCornerInner-01/References/v001/); stacked assembly rejected and retained |

## Component briefs

Each role gets one separately indexed v001 package with a reviewed concept sheet and isolated clean view. End caps describe only a material/edge treatment on the existing cell solid; no plug, cap geometry or protrusion is introduced. Inner slope-corner components use the named `corner-inner/v1` formula and retain its empty concave region and mating slope contours. Use neutral occupied/empty silhouettes, front/side/top/underside and one isometric view. Keep timber and stone appearance within the declared envelope. Images do not verify exact masks, dimensions, collision or fit.

## Progress

- Planned: 0; complete: 4; blocked/deferred: 0. Selected sheets and separate clean views reviewed; image gate passed.
- Next asset: `BLD-RT-SlopeCornerOuter-01` in batch 13.
- The outer slope-corner pair remains queued; end caps are an appearance treatment rather than a geometry attachment.
