# Greenlands waystone attachment review — 2026-10-19

## Result

The 11 disconnected GLB components are intentional: compass ring, dark emblem backing, body, collar, crown, lower base, upper step, three moss patches, and bronze star. Preserve all 11. Review evidence is the component inventory and annotated CPU views under ignored `Artifacts/Art/GreenlandsModelBatch-20261018/PROP-GreenlandsWaystone-01/GeometryRepair-v003/`.

The repaired v003 component report maps those roles to its own current inventory. These IDs are scoped to their listed mesh hashes; they are not reusable indices for later exports.

| Role | v003 component | Triangles |
| --- | --- | ---: |
| Compass ring | `SM_GreenlandsWaystone` `a940a3d10f7c:C0000` | 384 |
| Emblem backing | `SM_GreenlandsWaystone` `a940a3d10f7c:C0001` | 124 |
| Main body | `SM_GreenlandsWaystone` `a940a3d10f7c:C0002` | 92 |
| Collar | `SM_GreenlandsWaystone` `a940a3d10f7c:C0003` | 92 |
| Crown | `SM_GreenlandsWaystone` `a940a3d10f7c:C0004` | 92 |
| Lower base | `SM_GreenlandsWaystone` `a940a3d10f7c:C0005` | 68 |
| Upper step | `SM_GreenlandsWaystone` `a940a3d10f7c:C0006` | 68 |
| Moss patch 1 | `SM_GreenlandsWaystone` `a940a3d10f7c:C0007` | 68 |
| Moss patch 2 | `SM_GreenlandsWaystone` `a940a3d10f7c:C0008` | 68 |
| Moss patch 3 | `SM_GreenlandsWaystone` `a940a3d10f7c:C0009` | 68 |
| Bronze star | `SM_GreenlandsWaystone_CompassStar` `55bde0310b62:C0000` | 64 |

The star originally floated approximately 20 mm in front of the backing. GeometryRepair-v003 retained the visible 16-face star and added a closed backing that overlaps the emblem by 1 mm. The repair report records the operation. No other component was modified. The repaired candidate reimports as 1,188 triangles with zero loose vertices; bounds and triangle counts match in the fresh GLB and FBX checks. A separate fresh-source CPU comparison matched all triangle positions, UVs, material assignments, and normals in both formats at 0.1 mm coordinate-key precision. Its script and report are `Artifacts/Art/GreenlandsModelBatch-20261018/PROP-GreenlandsWaystone-01/GeometryRepair-v003/compare-waystone-exchange.py` and `surface-correspondence-v001.json`. This validates exchange correspondence, not UE material behavior or the missing texture images. The attempt manifest pins source, script, outputs, checks, and CPU preview settings/hashes.

## Open defects and gates

- The authoring source was made in Blender 5.2.2; the available Blender 5.1.1 crashes when opening it. The candidate derives from a GLB reimport, so original-source preservation and editability remain unverified.
- Crown triangles 356, 364, 395, and 403 have coincident vertices and zero UV area. The audit at `Artifacts/Art/GreenlandsModelBatch-20261018/PROP-GreenlandsWaystone-01/GeometryRepair-v003/uv-degenerate-face-audit-v001.json` confirms this is geometric degeneracy, not a UV packing issue.
- A CPU BVH vertex-to-surface check measures the backing 3.6 mm minimum and 22.9 mm median from the body. This confirms separation but not whether a mount or designed clearance was intended. Evidence: `Artifacts/Art/GreenlandsModelBatch-20261018/PROP-GreenlandsWaystone-01/GeometryRepair-v003/backing-body-distance-check-v001.json`.
- Unreal material behavior remains unverified even though the saved GLB/FBX correspondence check passes.
- The candidate has flat prototype materials and no production normal, height, or AO maps; no Unreal Engine 5.8 import, depth treatment, or performance comparison has been run.
- Geometry is at attempt 3/3 under the current job record. Do not perform another geometry mutation without recording a new attempt budget and compatible source runtime.

The waystone remains `needs-review`; it is not a completed or production-ready asset and must not be promoted or committed as accepted. The authoritative job record is `docs/art-templates/model-jobs/greenlands-waystone-reuse-20261018.json`.
