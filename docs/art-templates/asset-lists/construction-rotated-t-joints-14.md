# Construction rotated and T joints — batch 14

- Phase / scope revision: Construction kits / 1 (2026-10-18)
- Sources: `docs/art-templates/player-construction-kit-contract.md` — Structural joints; canonical reference frame; Construction Architecture §§3.1–3.2, 8–9.
- Owner: Codex
- Dependencies: KIT-RT / KIT-SM `cell-full/v1`; fixed six contact faces and authored 90° rotation set; no reflected or arbitrary rotations.

| Asset ID | Category / main concept | Source path / heading | Priority / dependencies | Owner / job state | Concept / clean-view status | Selected package / next action |
| --- | --- | --- | --- | --- | --- | --- |
| BLD-RT-JointRotatedCellFull-01 | Rough timber / authored 90° rotated join | Contract Structural joints; Construction §3.2 | P1; KIT-RT; full-cell contact/orientation | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-RT-JointRotatedCellFull-01/References/v001/); hollow-frame sheet rejected and retained |
| BLD-SM-JointRotatedCellFull-01 | Stone masonry / authored 90° rotated join | Contract Structural joints; Construction §3.2 | P1; KIT-SM; full-cell contact/orientation | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-SM-JointRotatedCellFull-01/References/v001/); course relief noted |
| BLD-RT-JointTCellFull-01 | Rough timber / T-intersection contact/joint | Contract Structural joints; Construction §3.2 | P1; KIT-RT; named contact faces | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-RT-JointTCellFull-01/References/v001/); three-cell T study reviewed |
| BLD-SM-JointTCellFull-01 | Stone masonry / T-intersection contact/joint | Contract Structural joints; Construction §3.2 | P1; KIT-SM; named contact faces | Codex / image-complete | proposed / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-SM-JointTCellFull-01/References/v001/); two confusing clean views retained as rejected |

## Component briefs

Each row receives its own component package with concept sheet, separate clean view, exact prompts, output hashes, actual image metadata and visual findings. Sheets include one isolated reference component plus a clearly separated assembly panel. The rotation study shows only an authored 90-degree grid rotation; the T study labels mating faces visually and preserves the third-party branch contact. No extra joinery changes occupancy, collision, support or gameplay material properties. The assembly panels are studies and do not count as separate components. Geometry remains proposed.

## Progress

- Planned: 0; complete: 4; blocked/deferred: 0. Concepts and component views reviewed. Assembly studies do not establish contact geometry/support or complete broader QA.
- Next asset: `BLD-RT-JointCrossCellFull-01` in batch 15.
- Cross joints, wall-floor/ceiling joints, beam-post, openings, roof details, curves and per-kit assemblies remain separately queued in the matrix.
