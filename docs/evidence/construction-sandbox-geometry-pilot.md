# Construction sandbox — first geometry pilot evidence

2026-10-18. [Sandbox milestone](../construction-sandbox-milestone.md), SBX-01/GEO-01 pilot. This is reviewed executable preparation for three simple shapes, not approved final-mesh manifests, GEO-02/GEO-03 completion or a playable construction tool.

`tools/construction_geometry.py` uses only Python's standard library. It emits the BLD-01 canonical payload for `cell-full`, `slab-half` and `slope-1to1`, derives rational volumes/contact contours from their outward solid faces, handles the 24 proper cell-centred cube rotations and floor-based negative address buckets. Validation admits only exact canonical instances of these fixed shapes; it is not a general polyhedron validator or a curved/multi-cell brush compiler.

Executed from repository root:

```powershell
py -3 tests/test_construction_geometry.py
py -3 tools/construction_geometry.py --output-dir Artifacts/ConstructionGeometry/sandbox-pilot-20261018-01
```

Six focused tests passed: analytic fractions and contact areas/contours, fixed payload SHA-256 goldens, exact volume preservation across all 24 rotations, negative address cases, malformed/noncanonical inputs and refusal to modify an existing output directory. The separate CLI run emitted three payloads and a manifest marked `prototype-subset`, `acceptance_claim=false`; its ignored output is reproducible and is not a committed production content package. Always choose a new output directory.

| Shape | Exact occupied fraction | Canonical payload SHA-256 |
| --- | --- | --- |
| `cell-full` | 1 | `9455bd04dab9df9f4643f175c95fb17cb7e25e6248c239975a8f1280f040061a` |
| `slab-half` | 1/2 | `e6236a7b600eccb0a2f558f74524e183ef57154a278938694629ed247ecfd154` |
| `slope-1to1` | 1/2 | `e55a54e6824119dffb929bf7de536eba786e7ce145efa732620da147cca4cdda` |

Luna/medium authored the bounded tool/tests; parent inspected the code, requested contact-winding preservation, pinned hashes and stronger rotation/input/output checks, then reran tests and CLI. A separate gpt-6.1-sol/medium reviewer executed the six tests and independently checked each geometry's planar outward convex faces, opposite directed edges and rational volumes under all rotations. Recheck passed with no remaining actionable finding within this subset. The milestone/implementation packet and art handoff were also independently reviewed.

Next: SBX-03/GEO-02 pilot must establish neighboring-solid intersection and exposed-face compatibility, including rotations, partial overlaps and bucket boundaries; SBX-04/GEO-03 then proves an isolated Unreal adapter. Compiler coverage still needs the other seven reference targets, multi-cell re-anchoring, convex decomposition and curve/arch error/clearance evidence. No Unreal compile, collision cook, imported art, live server mutation, save/reload or playable acceptance was performed in this cycle.
