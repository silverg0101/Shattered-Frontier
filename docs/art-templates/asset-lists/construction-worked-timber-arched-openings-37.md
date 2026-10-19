# Worked timber arched-opening treatments — batch 37

- Phase / scope revision: Intermediate construction kits / 1 (2026-10-18)
- Sources: `docs/art-templates/player-construction-kit-contract.md` §2 `arch-2m` and §3 buildability route; `docs/decisions/BLD-01-voxel-geometry-interface.md` coordinate/orientation contract; `docs/art-templates/asset-lists/construction-kit-matrix-07.md`; batches 20–22 for baseline role ownership.
- Owner: Codex
- Dependencies: KIT-WT face appearance references; KIT-RT arch packages used only as visual references; no gameplay tier changes.
- Scope: one material/type only — worked-timber arched-opening treatments on the documented `arch-2m/v1` carrier. Not a new structural shape or assembly.

## Route and carrier contract

The contract defines `arch-2m` with a 2 × 0.5 × 3 m envelope, in metres, on the 0.5 m grid. Local X is the 2 m width, Y is the 0.5 m extrusion, and Z is the 3 m height. The minimum-grid-corner frame is the carrier origin. In XZ, the outer solid is `[0,2]×[0,2]` plus an upper semicircle centered `(1,2)` with radius 1 m; the opening subtracts `(0.5,1.5)×[0,2]` plus an upper semicircle at the same center with radius 0.5 m. No treatment may alter these boundaries or empty opening volume.

BLD-01 defines 24 possible proper cube rotations, but each shape must author its own allowlist; identity is valid unless more are authored. The `arch-2m` treatment is shown in identity orientation only. No production allowlist, approved GEO-01 payload/manifest/hash, or applicable GEO-02 opening/rotation evidence is supplied. Images remain appearance proposals and do not establish numerical/runtime compatibility.

| Asset ID | Intended route / component boundary | Exposed and mating faces; named counterpart interfaces | State / selected outputs |
| --- | --- | --- | --- |
| BLD-WT-ArchOpeningJamb-01 | Proposed surface treatment on the left/right straight jamb and reveal patches of one `arch-2m/v1` carrier; no new member | Exposed front/back jamb faces and reveal; treatment boundary meets `BLD-WT-ArchOpeningRingLintel-01` at the two spring points and `BLD-WT-ArchOpeningThreshold-01` at the base. Exact patch/contact masks and repeat phase remain unapproved. | Codex / image-complete; structural acceptance blocked | agent-selected / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-WT-ArchOpeningJamb-01/References/v001/); two selected images reviewed, no numerical fit claim |
| BLD-WT-ArchOpeningRingLintel-01 | Proposed surface treatment on the curved crown/ring patches of one `arch-2m/v1` carrier; not a separate arch member | Exposed front/back curved band and reveal; terminates at both spring points against `BLD-WT-ArchOpeningJamb-01`. Exact patch/contact masks and repeat phase remain unapproved. | Codex / image-complete; structural acceptance blocked | agent-selected / ready-for-model-test | [v001 package](../../GameClient/ArtSource/BLD-WT-ArchOpeningRingLintel-01/References/v001/); two selected images reviewed, no numerical fit claim |
| BLD-WT-ArchOpeningThreshold-01 | Proposed separately defined threshold object at the arch base; no approved solid/anchor exists, so blocked before images | Candidate contacts: arch host at `z=0`, floor plane and clear passage. Dimensions, pivot, exposed/underside/end faces and exact counterpart contour are unresolved; do not infer a board or step. | Blocked; 0/2 selected outputs |
| BLD-WT-ArchDoorInterface-01 | Proposed exposed-surface treatment at the inner reveal; no rebate, door leaf or hardware may be added. Patch ownership/contour unresolved, so blocked before images. | Candidate counterpart: jamb/reveal treatment and future door-frame contact; the functional door workflow and exact stop plane are unapproved. | Blocked; 0/2 selected outputs |
| BLD-WT-ArchWindowInterface-01 | Proposed separately defined window/frame interface object; applicability and geometry are unresolved for the floor-reaching `arch-2m` aperture, so blocked before images. | No approved sill/frame anchor, sash opening, collision or clearance. Do not invent a window or change the opening. | Blocked; 0/2 selected outputs |

For the three blocked roles, frame context remains metres, 0.5 m grid, local X/Y/Z as above, minimum-grid-corner host frame. Their independent pivots, member boundaries, allowed rotations, exposed/mating contours, and approved counterparts are intentionally unresolved. These gaps remain visible in the matrix and are not closed by the two generated surface-treatment concepts.

## Image briefs

The two planned treatments each use exactly two selected outputs: `concept-sheet.png` and `asset-view.png`; `additional_images: 0`. Put exact target dimensions, target opening contour, patch boundary, contacts, section and seam information on the concept sheet, with the complete treated arch carrier as the dominant view. The clean view shows only the complete named treatment on the neutral `arch-2m/v1` carrier. Use KIT-WT and KIT-RT artwork only for appearance cues. No threshold, door leaf, window, hardware, cutaway solid, extra trim or unsupported projection may appear.

## Progress

- Planned: 0; in-progress: 0; image-complete: 2; blocked: 3; deferred: 0.
- Image-ready roles are complete; structural blockers remain on the threshold, door and window interfaces.
- Remaining authority blockers: approved GEO-01 manifest/hash and applicable GEO-02 fit evidence for the shared carrier; a separately owned shape/interface definition for threshold; explicit door-stop/contact and window-applicability definitions.
