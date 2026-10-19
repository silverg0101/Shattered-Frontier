# Worked timber curved-course faces — batch 39

- Phase / scope revision: Intermediate construction kits / 1 (2026-10-18)
- Sources: `docs/art-templates/player-construction-kit-contract.md` §2 `curve-quarter-r2` and §3 buildability route; `docs/decisions/BLD-01-voxel-geometry-interface.md` coordinate/orientation contract; `docs/art-templates/asset-lists/construction-kit-matrix-07.md`.
- Owner: Codex
- Dependencies: KIT-WT worked-timber appearance; the RT/SM curve packages are appearance references only and do not establish runtime fit.
- Scope: one KIT-WT component type — concave/convex face treatments on the existing `curve-quarter-r2` reference carrier. The two jobs are separately indexed surface treatments of the same proposed carrier, not two physical modules or a complete assembled curve kit.

## Route and carrier contract

Contract envelope is 2.0 × 2.0 × 0.5 m. The solid is a first-quadrant quarter-annulus `1.5² ≤ x²+y² ≤ 2²`, `0 ≤ z ≤ 0.5 m`; inner radius 1.5 m; outer radius 2.0 m. Units are metres; grid is 0.5 m; local X/Y are plan axes and Z is height; origin/pivot is the minimum-grid-corner carrier frame. Identity orientation only until this shape authors its own BLD-01 rotation allowlist. Both jobs use the same full carrier and no geometry changes. Numerical mesh/runtime compatibility remains unproven.

| Asset ID | Category / main concept | Source path / heading | Priority / dependencies | Owner / job state | Concept / clean-view status | Selected package / next action |
|---|---|---|---|---|---|---|
| BLD-WT-CurveInnerCourse-01 | KIT-WT concave inner-face treatment on `curve-quarter-r2` | Contract §2 `curve-quarter-r2`; BLD-01 orientation; batch 39 brief | P2; contract carrier proposal and KIT-WT face appearance | Codex / image-complete | proposed / ready-for-model-test; geometry acceptance open | [v001](../../../GameClient/ArtSource/BLD-WT-CurveInnerCourse-01/References/v001/); exactly 2 selected; GEO-01/GEO-02 open |
| BLD-WT-CurveOuterCourse-01 | KIT-WT convex outer-face treatment on the same carrier | Contract §2 `curve-quarter-r2`; BLD-01 orientation; batch 39 brief | P2; same carrier as inner-face role; KIT-WT face appearance | Codex / image-complete | proposed / ready-for-model-test; geometry acceptance open | [v001](../../../GameClient/ArtSource/BLD-WT-CurveOuterCourse-01/References/v001/); exactly 2 selected; GEO-01/GEO-02 open |

## Route and interfaces

Both jobs use the same proposed 2.0 × 2.0 × 0.5 m first-quadrant quarter-annulus `1.5²≤x²+y²≤2²`, 0≤z≤0.5 m; 0.5 m grid; local X/Y plan and Z height; minimum-grid-corner frame; identity orientation only until the curve authors a rotation allowlist. Inner treatment is the concave r=1.5 m face; outer treatment is convex r=2.0 m face. Each separate image shows one entire carrier with only its named face treated; they are complementary surface roles on one carrier, not separate physical solids. Repeated arc boundaries share their matching same-role profile; radial ends target `BLD-WT-CurveStraightJoin-01`, with contact mask/phase unresolved. Each role has exactly two selected outputs and zero additional images. Geometry/runtime fit remains blocked by absent GEO-01/GEO-02 evidence.

The concept sheet must show the full annular-quarter carrier as the dominant view, target boundary and radii, a plan view and an end/contact view. Clean view is one intact complete quarter-annulus carrier only, with the named concave or convex patch treated. No stacked scene, multi-piece collage, extended wall, corner cap, extra timber member, or unsupported joint shape. These packages do not cover `CurveStraightJoin` or the separate assembly study.

## Progress

- Planned: 0; in-progress: 0; image-complete: 2; blocked: 0; deferred: 0.
- Batch 39 image gate: passed for both selected pairs. Structural readiness remains blocked for both pending approved GEO-01 manifests/hashes and applicable GEO-02 curve/contact evidence.
- Next asset: `BLD-WT-CurveStraightJoin-01` (route review required before generation).
- Shared blocker: GEO-01 approved geometry manifest/hash and applicable GEO-02 curve repetition/rotation/contact evidence are absent; appearance-only concept work may proceed on the explicit contract proposal.
