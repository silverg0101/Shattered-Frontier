# Construction roof slope faces and hip/valley joints — batch 23

- Phase / scope revision: Construction kits / 1 (2026-10-18)
- Sources: `docs/art-templates/player-construction-kit-contract.md` — partial shapes and roof/slope transitions; Construction §§3.1–3.2, 8–9.
- Owner: Codex
- Dependencies: KIT-RT/KIT-SM `slope-1to1` and matching contour contacts; preserve one-cell wedge and no walkability/support claims.

| Asset ID | Category / main concept | Source path / heading | Priority / dependencies | Owner / job state | Concept / clean-view status | Selected package / next action |
| --- | --- | --- | --- | --- | --- | --- |
| BLD-RT-RoofSlopeFace-01 | Rough timber / one-to-one roof slope face | Contract partial shapes; Construction §3.2 | P1; KIT-RT; `slope-1to1` | Codex / image-complete | proposed / needs-segmentation | [BLD-RT-RoofSlopeFace-01 v001](../../GameClient/ArtSource/BLD-RT-RoofSlopeFace-01/References/v001/); Timber slope sheet has decorative projection concern; review further. |
| BLD-SM-RoofSlopeFace-01 | Stone masonry / one-to-one roof slope face | Contract partial shapes; Construction §3.2 | P1; KIT-SM; `slope-1to1` | Codex / image-complete | proposed / ready-for-model-test | [BLD-SM-RoofSlopeFace-01 v001](../../GameClient/ArtSource/BLD-SM-RoofSlopeFace-01/References/v001/); Wedge faces readable; geometry unvalidated. |
| BLD-RT-RoofHipValley-01 | Rough timber / hip/valley contour joint | Contract slope transitions; Construction §3.2 | P1; KIT-RT; matching wedges | Codex / image-complete | proposed / needs-segmentation | [BLD-RT-RoofHipValley-01 v001](../../GameClient/ArtSource/BLD-RT-RoofHipValley-01/References/v001/); Clean view contains assembly context; later segmentation needed. |
| BLD-SM-RoofHipValley-01 | Stone masonry / hip/valley contour joint | Contract slope transitions; Construction §3.2 | P1; KIT-SM; matching wedges | Codex / image-complete | proposed / needs-segmentation | [BLD-SM-RoofHipValley-01 v001](../../GameClient/ArtSource/BLD-SM-RoofHipValley-01/References/v001/); Clean view contains assembly context; later segmentation needed. |

## Component briefs

Index each wedge skin and roof contour joint separately. Show contact faces, underside and a small assembly. Keep gameplay/navigation unchanged. Images are not numerical geometry, collision or walkability evidence.

## Progress

- Planned: 4; image-complete: 4; blocked/deferred: 0.
- All four sheets and clean views reviewed; RT slope sheet has decorative projection concern, hip/valley clean views need segmentation.
- Next asset: `BLD-RT-RoofRidge-01` in batch 24.
