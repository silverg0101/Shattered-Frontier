# Worked timber corners and slope transitions — batch 33

- Phase / scope revision: Intermediate construction kits / 1 (2026-10-18)
- Sources: `docs/art-templates/player-construction-kit-contract.md`; Construction §§3.1–3.2, 8–9; matrix 07.
- Owner: Codex
- Dependencies: KIT-WT cell-full appearance and contract corner/slope shape envelopes; gameplay properties unchanged.

| Asset ID | Category / main concept | Source path / heading | Priority / dependencies | Owner / job state | Concept / clean-view status | Selected package / next action |
| --- | --- | --- | --- | --- | --- | --- |
| BLD-WT-CornerInsideCellFull-01 | Worked timber / inside architectural corner treatment | Construction matrix; Construction §3.2; contract buildability route | P1; KIT-WT; exposed patches on two orthogonal cell-full faces | Codex / image-complete; structural route blocked | proposed / needs-segmentation | [v001 dated review](../../GameClient/ArtSource/BLD-WT-CornerInsideCellFull-01/References/v001/review-2026-10-18.md); appearance evidence only; no scale, ends or connection contour |
| BLD-WT-CornerOutsideCellFull-01 | Worked timber / outside architectural corner treatment | Construction matrix; Construction §3.2; contract buildability route | P1; KIT-WT; exposed patches on two orthogonal cell-full faces | Codex / image-complete; structural route blocked | proposed / needs-review | [v001 dated review](../../GameClient/ArtSource/BLD-WT-CornerOutsideCellFull-01/References/v001/review-2026-10-18.md); interlocking end blocks imply unapproved member assembly |
| BLD-WT-SlopeCornerInner-01 | Worked timber / canonical slope-corner component proposal; current V image is unapproved study evidence | Construction matrix; Construction §3.2; contract buildability route | P1; KIT-WT; `slope-1to1` geometry and named contacts required | Codex / image-complete; structural route blocked | proposed / needs-segmentation | [v001 dated review](../../GameClient/ArtSource/BLD-WT-SlopeCornerInner-01/References/v001/review-2026-10-18.md); multi-panel valley collage is not this component's clean view |
| BLD-WT-SlopeCornerOuter-01 | Worked timber / canonical slope-corner component proposal; current hip image is unapproved study evidence | Construction matrix; Construction §3.2; contract buildability route | P1; KIT-WT; `slope-1to1` geometry and named contacts required | Codex / image-complete; structural route blocked | proposed / needs-segmentation | [v001 dated review](../../GameClient/ArtSource/BLD-WT-SlopeCornerOuter-01/References/v001/review-2026-10-18.md); multi-panel roof assembly exceeds declared component boundary |

## Component briefs

## Buildability review (2026-10-18)

- `CornerInsideCellFull` and `CornerOutsideCellFull` are **surface-treatment proposals**, not member assemblies and not the sloped `corner-inner`/`corner-outer` solids. Intended carrier: adjacent exposed face patches of two orthogonal canonical `cell-full` members; units m; grid 0.5 m; BLD-01 X/Y/Z frame; minimum-grid-corner carrier frame; treatment inherits the carrier's approved rotation allowlist and has no independent pivot. Exact axis-to-face mapping, patch contours, end faces, repeat/phase, contact plane, dimensions and approved counterpart are unresolved. Candidate style references are `BLD-WT-FaceExteriorCellFull-01` and `BLD-WT-FaceInteriorCellFull-01`; these are not approved corner interfaces. Inside image is room-corner/wall context, useful for appearance only. Outside image exposes alternating end blocks and suggests member crossings without member definitions. Both v001 pairs stay historically intact; clean views are not accepted as isolated surface-treatment assets. GEO-01 approved payload/hash and applicable GEO-02 adjacent/rotated seam evidence are missing.
- `SlopeCornerInner` remains a **proposed canonical structural component** on `slope-1to1`, not an assembly-study job. Units m; 0.5 m grid; X/Y/Z and minimum-grid-corner frame required by BLD-01; exact envelope, pivot, allowed rotations, contacts, exposed faces and counterparts remain unresolved pending approved manifest/hash. Its current V-shaped roof-panel collage is retained as style/study evidence only; member boundaries and numeric angle/contact interfaces are undeclared. Its clean view depicts an undefined multi-panel roof composition, so it is not accepted as the one-component clean view. No substitute assembly ID is created. Any future assembly-study brief must declare separate member IDs, offsets, rotations and interfaces first.
- `SlopeCornerOuter` remains a proposed canonical `slope-1to1` component. The v001 images depict broad hip assemblies with support members extending beyond a demonstrated envelope; appearance evidence only. Dimensions, exposed/mating faces, member boundaries, rotation allowlist and counterparts remain unresolved; clean view is not accepted.
- GEO-01 remains only `prototype-subset` with `acceptance_claim=false`; approved manifests/hashes and relevant GEO-02 fit evidence are absent. Image review does not establish numeric mesh/runtime fit.

Index each shape separately. Show exterior and interior, top/underside and exposed edges where visible; maintain exact contract shape, cut/contact planes and clearances. No decorative feature may grow outside the source envelope. Each package requires concept sheet, separate clean view, exact prompts, hashes, actual metadata and visual review. Mesh/runtime compatibility remains unverified.

## Progress

- Planned: 4; image-complete: 4; blocked/deferred: 0.
- Slope-corner images retain explicit envelope/read cautions; geometry and compatibility are not established.
- Next asset: `BLD-WT-JointStackCellFull-01` in batch 34.
