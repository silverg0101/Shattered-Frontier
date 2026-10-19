# Worked timber curve-to-straight interface study — batch 40

- Phase / scope revision: Intermediate construction kits / 1 (2026-10-18)
- Sources: `docs/art-templates/player-construction-kit-contract.md` §§2 `curve-quarter-r2` and `cell-full`; `docs/decisions/BLD-01-voxel-geometry-interface.md`; `docs/art-templates/asset-lists/construction-kit-matrix-07.md`.
- Owner: Codex
- Dependencies: KIT-WT curved-face appearance pair in batch 39; canonical proposal shapes only.
- Scope: one KIT-WT straight-to-curve interface shown as an explicit two-member assembly study. It is not a new adapter solid or proof of fit.

| Asset ID | Category / main concept | Source path / heading | Priority / dependencies | Owner / job state | Concept / clean-view status | Selected package / next action |
|---|---|---|---|---|---|---|
| BLD-WT-CurveStraightJoin-01 | Explicit cell-member assembly study: one `curve-quarter-r2` course meets one straight `cell-full` wall cell | Contract §§2 `curve-quarter-r2` / `cell-full`; BLD-01 grid/pivot/orientation; batch 40 brief | P2; KIT-WT curve-face treatments; both members use contract geometry proposals | Codex / image-complete | proposed / ready-for-model-test (study only; fit open) | [v001](../../../GameClient/ArtSource/BLD-WT-CurveStraightJoin-01/References/v001/); exactly 2 selected; not component coverage; GEO-01/GEO-02 open |

## Route and member boundaries

Route: `explicit cell-member assembly study`. The curve member is exactly the first-quadrant quarter-annulus `1.5²≤x²+y²≤2²`, `0≤z≤0.5 m`, envelope 2.0×2.0×0.5 m, minimum-grid-corner origin/pivot `(0,0,0)`, identity orientation. The straight member is one `cell-full` cube of 0.5×0.5×0.5 m, minimum-grid-corner pivot `(1.5,-0.5,0)`, identity orientation. In the shared assembly frame, the curve radial end at `y=0`, `1.5≤x≤2.0`, `0≤z≤0.5`, contacts the cube face at `y=0` over the same rectangle. Both member boundaries remain individually visible and separately named; no adapter, overlap, gap, fastener, trim or extra member is introduced. Units metres, grid 0.5 m, axes local/global X/Y plan and Z height. The cube's authored rotation must remain identity for this study; no arbitrary rotation is shown.

Exposed surfaces: the curve retains concave and convex arc faces, top, underside and its other radial end; the straight cube retains five exposed faces. Mating interface: the shared `y=0`, X `[1.5,2.0]`, Z `[0,0.5]` face, area 0.25 m² by contract targets. Named counterparts: `BLD-WT-CurveInnerCourse-01`, `BLD-WT-CurveOuterCourse-01`, and `BLD-WT-FaceExteriorCellFull-01`. The study is appearance-only. Approved GEO-01 manifests/hashes and applicable GEO-02 evidence are absent; image review cannot validate numerical fit, collision or runtime behavior.

The concept sheet must show one dominant assembled interface, an exploded or separated two-member view with clear boundaries, and an orthographic contact diagram with dimensions. The clean view shows one intact two-member interface assembly only. No repeated wall scene, additional courses, corner, roof, opening, fastener, beam or new geometry. Images cannot count as the separate per-kit architectural assembly study.

## Progress

- Planned: 0; in-progress: 0; image-complete: 1; blocked: 0; deferred: 0.
- Batch 40 image gate: passed for the two selected images. This interface study does not close a separate component role. Structural/runtime fit remains blocked pending approved GEO-01/GEO-02 evidence.
- Next asset: `BLD-WT-RoofHipValley-01` (route blocked; see catalog and matrix notes).
- Shared blocker: geometry remains proposed; GEO-01 approval and GEO-02 fit/contact evidence are open.
