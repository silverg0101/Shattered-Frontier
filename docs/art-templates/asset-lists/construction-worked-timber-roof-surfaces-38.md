# Worked timber slope-roof surface and edge treatments — batch 38

- Phase / scope revision: Intermediate construction kits / 1 (2026-10-18)
- Sources: `docs/art-templates/player-construction-kit-contract.md` §2 `slope-1to1` and §3 buildability route; `docs/decisions/BLD-01-voxel-geometry-interface.md` coordinate/orientation contract; `docs/art-templates/asset-lists/construction-kit-matrix-07.md`.
- Owner: Codex
- Dependencies: KIT-WT surface appearance; baseline RT slope/eave images are appearance-only references and do not define this geometry.
- Scope: one material/type only — worked-timber slope-roof face, exposed-end and eave/underside treatments. No overhang, shingles as geometry, braces, roof assembly or walkability claim.

## Route and carrier contract

All three jobs are **proposed material/surface treatments** on the contract `slope-1to1` carrier, not new solids or assemblies. The carrier envelope is one 0.5 × 0.5 × 0.5 m grid cell; units are metres; local X is run, Y is depth, Z is rise; minimum-grid-corner origin/pivot. In normalized local coordinates the solid target is `w ≤ u` (equivalent to Z ≤ X over the 0.5 m cell). BLD-01 defines 24 possible proper cube rotations, but each shape must author its allowlist; identity only is valid unless more are authored. Show identity orientation only. Exact patch mapping, seam phase, production allowlist and approved GEO-01 payload/hash are absent; applicable GEO-02 evidence remains open.

| Asset ID | Category / main concept | Source path / heading | Priority / dependencies | Owner / job state | Concept / clean-view status | Selected package / next action |
|---|---|---|---|---|---|---|
| BLD-WT-RoofSlopeFace-01 | KIT-WT flush slope-face treatment | Contract §2 `slope-1to1`; BLD-01 orientation; batch 38 brief | P2; proposed carrier and KIT-WT face appearance | Codex / image-complete | proposed / ready-for-model-test; geometry acceptance open | [v001](../../../GameClient/ArtSource/BLD-WT-RoofSlopeFace-01/References/v001/); exactly 2 selected; GEO-01/GEO-02 open |
| BLD-WT-RoofEnd-01 | KIT-WT flush triangular y-end treatment | Contract §2 `slope-1to1`; BLD-01 orientation; batch 38 brief | P2; shares proposed carrier with slope-face role | Codex / image-complete | proposed / ready-for-model-test; geometry acceptance open | [v001](../../../GameClient/ArtSource/BLD-WT-RoofEnd-01/References/v001/); exactly 2 selected; GEO-01/GEO-02 open |
| BLD-WT-RoofEave-01 | KIT-WT flush underside/eave treatment | Contract §2 `slope-1to1`; BLD-01 orientation; batch 38 brief | P2; shares proposed carrier and named roof-wall/slope interfaces | Codex / image-complete | proposed / ready-for-model-test; geometry acceptance open | [v001](../../../GameClient/ArtSource/BLD-WT-RoofEave-01/References/v001/); exactly 2 selected; GEO-01/GEO-02 open |

## Route and interfaces

All three jobs are proposed surface treatments on the 0.5 × 0.5 × 0.5 m `slope-1to1` carrier, with local X run, Y depth, Z rise; minimum-grid-corner frame; `Z≤X`; identity orientation only until an authored shape allowlist exists. `RoofSlopeFace` treats z=x and meets repeated slope seams plus unvalidated hip/valley/ridge interfaces. `RoofEnd` treats only the triangular end faces y=0 and y=0.5 and meets the slope-face role. `RoofEave` treats z=0 through exact lower edge x=z=0 and names `BLD-WT-RoofWallSeam-01` and `BLD-WT-RoofSlopeFace-01`. No overhang, cap, thickness, extra members or walkability claim. Each row has exactly two selected outputs and zero additional images.

The concept sheet must show the full wedge as the dominant neutral carrier with the selected treatment flush on its named patch, plus front/side/top or section views, target 0.5 m envelope/grid, contact boundaries and named seam. The clean view is one complete treated wedge only. Keep worked-timber grain and color consistent with existing KIT-WT faces; do not imitate baseline overhangs, shingle thickness, braces or cap blocks. Images do not validate numerical fit or runtime behavior.

## Progress

- Planned: 0; in-progress: 0; image-complete: 3; blocked: 0; deferred: 0.
- Batch 38 image gate: passed for the three selected pairs. Structural readiness remains blocked for all three pending approved GEO-01 manifests/hashes and applicable GEO-02 seam/rotation evidence.
- Next asset: see catalog current handoff.
- Shared blocker: approved GEO-01 manifest/hash and applicable GEO-02 repeat/rotation/seam evidence remain absent; these do not prevent appearance-only concept work on the explicit proposed carrier.
