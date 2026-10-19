# Construction material quality revision B — 2026-10-19

## Scope and pinned build

Finite batch: `BLD-RT-CellFull-01`, `BLD-SM-CellFull-01`, and `BLD-WT-CellFull-01`. Revision B is materials-stage attempt 2 for each asset, following `Model-v007` attempt 1. Direct image-catalog packages are appearance references only; all three reuse the exact 0.5 m `cell-full` prototype carrier. No reference pixels are projected onto the meshes.

Blender 5.1.1 / Cycles CPU, 1280×960, 24 samples, denoise; 1024×1024 basecolor, roughness, tangent normal (+Y), and height sidecar. Pinned inputs and generator hashes are in [Model-v008-source-lock.json](../../GameClient/ArtSource/ConstructionGeometryPilot-20261018/Model-v008-source-lock.json). v007 and v008 outputs and hashes are retained.

## Checks

- Fresh Blender source reopen passes for all three: exact 0.5 m envelope, one asset mesh, UV0 spans 0–1, one position-welded component, no loose vertices, and required basecolor/roughness/normal image nodes present.
- Fresh GLB and FBX import checks pass for all three: expected dimensions, UV0 and material slots present, one connected component, no loose vertices.
- The solid-cell carrier has no intentional separate parts. No disconnected islands were found by the position-welded component check.
- Cycles CPU previews and all maps are saved under `Model-v008/<asset-id>/`; per-file hashes are in `batch-manifest.json`.

## Findings and state

v007 had false course segmentation on timber and weak/flat material appearance. v008 removes the timber course seams and adds directional grain. The repair improves continuity but the current wood still reads too smooth against the catalog image; the stone courses also remain too uniform. Edge-wrap metrics are recorded for investigation, but no approved pixel-continuity tolerance exists, so tileability is not accepted. These are material-stage defects, with one repair attempt remaining under the three-attempt cap.

All three jobs remain `needs-review`, not complete. Their geometry carrier is only `prototype-subset`, with no approved GEO-01 manifest/hash or applicable GEO-02 evidence. Unreal 5.8 import, normal-map convention, assembled seams, collision/LOD and performance are also open. See the authoritative records in [model-catalog.md](../art-templates/model-catalog.md).
