# Construction outer slope corners and stacked joints — batch 13

- Phase / scope revision: Construction kits / 1 (2026-10-18)
- Sources: `docs/art-templates/player-construction-kit-contract.md` — Structural joints; Roof and slope transitions; Construction Architecture §§3.1–3.2, 8–9.
- Owner: Codex
- Dependencies: proposed `corner-outer/v1`, `cell-full/v1`; baseline KIT-RT/KIT-SM face and end packages. Stacking is a surface/joint study, not altered support mechanics.

| Asset ID | Category / main concept | Source path / heading | Priority / dependencies | Owner / job state | Concept / clean-view status | Selected package / next action |
| --- | --- | --- | --- | --- | --- | --- |
| BLD-RT-SlopeCornerOuter-01 | Rough timber / convex 1:1 slope corner | Contract Roof and slope transitions; Construction §3.2 | P1; KIT-RT; `corner-outer/v1` contacts | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-RT-SlopeCornerOuter-01/References/v001/); corrected pyramidal views reviewed |
| BLD-SM-SlopeCornerOuter-01 | Stone masonry / convex 1:1 slope corner | Contract Roof and slope transitions; Construction §3.2 | P1; KIT-SM; `corner-outer/v1` contacts | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-SM-SlopeCornerOuter-01/References/v001/); hollow-view sheet rejected and retained |
| BLD-RT-JointStackCellFull-01 | Rough timber / repeated and stacked full-cell joint | Contract Structural joints; Construction §3.2 | P1; KIT-RT; face/top/underside/end packages | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-RT-JointStackCellFull-01/References/v001/); repeated/stacked/rotated sheet reviewed |
| BLD-SM-JointStackCellFull-01 | Stone masonry / repeated and stacked full-cell joint | Contract Structural joints; Construction §3.2 | P1; KIT-SM; face/top/underside/end packages | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-SM-JointStackCellFull-01/References/v001/); one-cell view corrected; course depth ambiguous |

## Component briefs

Slope outer roles use one separately indexed proposed `corner-outer/v1` wedge, solid `w ≤ min(u,v)` in a 0.5 m envelope, with occupied silhouette, two slope contact faces, top and underside views. Stacked-joint packages show one separately indexed contact/joint treatment plus an assembly panel demonstrating repeated horizontal and vertical joins; stacked cells in the scene do not count as additional components. Keep full-cell envelope/contact planes fixed, label the assembly as appearance-only, and do not imply new support behavior. Include a separated clean component view per row. Images do not establish fit or runtime mechanics.

## Progress

- Planned: 0; complete: 4; blocked/deferred: 0. Concept sheets and separate clean views visually reviewed; no geometry/runtime validation implied.
- Next asset: `BLD-RT-JointRotatedCellFull-01` in batch 14.
- Next structural rows include rotated, T/cross, wall/floor, wall/ceiling and beam/post joints; all remain separately tracked in the matrix.
