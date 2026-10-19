# Worked timber cell-full cosmetic pilot — batch 41

- Phase / scope revision: Intermediate construction kits / revision 1 (2026-10-18)
- Sources: `docs/art-templates/player-construction-kit-contract.md` §§2, 4, 7, 8; `docs/art-templates/asset-lists/construction-kit-matrix-07.md` KIT-WT scope; Construction Architecture §3.2.
- Owner: Codex
- Dependencies: same proposed `cell-full/v1` carrier and KIT-WT material profile for both variants; plain style is the sole master for the ornamented variant; gameplay tier properties unchanged.
- Scope: contract-required KIT-WT cosmetic pilot pair only. This is two jobs (the exact finite pilot); each has two selected images, `concept-sheet.png` and `asset-view.png`, `selected_image_count: 2`, `additional_images: 0`.

## Shared route and component boundary

Route: appearance-only KIT-WT skin on the canonical proposed `cell-full/v1` full cube. Target units are metres; envelope is 0.5 × 0.5 × 0.5 m on a 0.5 m grid. Construction-local axes: X = width/run, Y = depth, Z = up. Origin/pivot is the minimum grid corner `(0,0,0)`. Identity orientation is shown; no unapproved rotations or mirroring are implied. The solid remains the complete cube. Six full-square mating/contact faces remain at x=0/0.5, y=0/0.5 and z=0/0.5; each target face area is 0.25 m². The skin has no added thickness, parts, relief, trim, cuts, openings or altered contact/collision/clearance. Geometry is `geometry-proposed`; no approved GEO-01 payload/hash or applicable GEO-02 evidence is supplied, so image review cannot accept numerical or runtime fit.

| Asset ID | Category / main concept | Source path / heading | Priority / dependencies | Owner / job state | Concept / clean-view status | Selected package / next action |
| --- | --- | --- | --- | --- | --- | --- |
| BLD-WT-CellFull-01 | KIT-WT plain, unornamented full-cell skin | Contract §2 `cell-full`; §§4, 7–8; Construction Architecture §3.2; WT surface face references | P1; proposed `cell-full/v1`; KIT-WT plain style master | Codex / image-complete; structural readiness blocked | agent-selected / ready-for-model-test (image presentation only) | [v001 package](../../GameClient/ArtSource/BLD-WT-CellFull-01/References/v001/); selected pair reviewed; GEO-01/GEO-02 remain open |
| BLD-WT-CellFull-Ornamented-01 | KIT-WT flush ornamented appearance variant of the same full-cell skin | Contract §7 cosmetic pilot; same `cell-full/v1` and material profile as plain master | P1; depends on selected `BLD-WT-CellFull-01` master; appearance only | Codex / image-complete; structural readiness blocked | agent-selected / ready-for-model-test (image presentation only) | [v001 package](../../GameClient/ArtSource/BLD-WT-CellFull-Ornamented-01/References/v001/); same carrier/profile; GEO-01/GEO-02 remain open |

## Per-job brief

Both concept sheets show the complete cube as the dominant view, with front/exterior, back/interior, +X/-X ends, top, underside, dimensioned neutral silhouette, axes/origin and all six contact squares. Diagram measurements are labelled design targets. Each clean view shows exactly one complete isolated cube with the same design, full silhouette and no labels or extra assets. Use existing KIT-WT faces and rough-timber/stone `cell-full` packages as appearance references only; do not copy their boundaries or treat image pixels as authoritative geometry.

The plain variant uses dressed worked-timber grain and restrained flush plank/board seams, with no ornament or hardware. The ornamented variant inherits that selected base appearance and adds restrained, flat color-only stained linework/patterns on exposed vertical faces. It adds no relief, thickness, trim, parts or different material/gameplay tier; mating faces and their full contact areas stay unobstructed and geometrically unchanged. No alternate design, assembly, collage or separate swatch is authorized.

## Progress

- Planned: 0; in-progress: 0; image-complete: 2; blocked: 0; deferred: 0. Image completion does not accept geometry/runtime fit.
- `BLD-WT-CellFull-01` selected pair reviewed; initial clean-view candidate was replaced only to correct tight framing and preserved in package provenance. Ornamented variant shares the plain master carrier/profile and adds only flat surface color.
- Cosmetic pilot complete; structural acceptance remains blocked on approved GEO-01 manifest/hash and applicable GEO-02 evidence.
- Next indexed asset: `BLD-WT-RoofHipValley-01` (blocked pending an approved KIT-WT member/treatment/attachment route and contact/member boundaries). Rectangular door/window interfaces also lack an approved aperture carrier/contour; do not image those until authority supplies it.
