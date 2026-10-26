# Greenlands environment model batch — 2026-10-18

Finite nonstructural batch for the playable construction sandbox. The image packages remain owned by the image catalog. This batch links their selected clean views and manifests directly and leaves image-session state untouched.

## Queue

| Stable asset ID | Route | State |
| --- | --- | --- |
| `PROP-GreenlandsWaystone-01` | Reuse `GreenlandsWaystone.blend`, GLB and FBX | Needs review: all 11 GLB components classified; star attachment repaired in v003; four crown degeneracies, source compatibility and engine gates open; geometry cap 3/3 |
| `RESOURCE-GreenlandsOakNode-01` | Pixal3D image-to-3D; foliage motion not requested | Needs review: root sheet/detached canopy, scale and component dispositions unresolved |
| `RESOURCE-GreenlandsIronVein-01` | Pixal3D image-to-3D; material appearance reviewed separately | Needs review: grounding skirt/shards, vein shape and component dispositions unresolved |
| `PROP-GreenlandsWayfindingMarker-01` | Pixal3D image-to-3D; static prop | v003 removed 11 reviewed debris islands; major lean and fused sheet defects remain; geometry attempt cap 3/3 exhausted |

Authoritative job records are linked in `docs/art-templates/model-catalog.md`. Attempt folders are immutable under ignored `Artifacts/Art/GreenlandsModelBatch-20261018/<asset-id>/attempt-01/`.

## Pinned generation profile

`backend-lock.json` and `software-lock.json` were copied and hashed before the first inference. The profile files under `docs/art-templates/model-profiles/` pin Pixal3D revision `b0cb2e1b794cab9aa0ac38f93` (full revision in the lock), MoGe2 camera weights, DINOv2, NAF weights, PyTorch `2.14.0+rocm10.1.0`, HIP runtime `7.16.26385`, and the RX 7900 XTX device. The current local Pixal runner script and input-preparation script hashes are also recorded by each profile.

All generated assets use seed `42`, `1024_cascade`, 12 sampler steps, maximum 49,152 tokens, low-VRAM mode, locally estimated MoGe2 camera parameters, 2K texture, 300,000-triangle export target, remesh band `1`, and the runner's recorded Pixal coordinate conversion. The user selected an 18–22 m mature-oak height envelope; final proportions, tree width, and placement scale remain open. Other assets keep asset-specific scale unresolved; no Voidreign 2.6 m scale is applied. Output formats from inference are GLB and raw PLY; Blender delivery exports are staged separately.

The opaque catalog views were conditioned independently with `prepare_model_input.py`. Each image retains its first border-connected flood-fill input, a rejected tight-threshold candidate, and a five-iteration GrabCut repair input with mask preview/report. No Voidreign image, mask, dimensions, segmentation regions or rig are reused.

## Ownership and limits

This batch owns GPU generation while the Voidreign session is CPU-only. One Pixal inference or render job runs at a time. Each stage permits one initial attempt and up to two recorded repairs; exhausted stages retain their failed evidence and exact blocker. Structural geometry is excluded from this scenery batch. Future construction models must use approved GEO-01 manifests and applicable GEO-02 fit evidence before geometry generation.

Package image provenance does not provide explicit license/permission metadata. The jobs record that limitation without making a rights claim. No Unreal integration, gameplay/resource behavior, collision acceptance or engine validation is included.

## Current evidence

The three Pixal jobs completed two inference runs each. Their reference stages used three preprocessing candidates; the stages are exhausted, and mask/contact acceptance remains open. Oak and iron geometry are at attempt 2/3; marker geometry is at 3/3 after v003 removed 11 reviewed debris islands. Their attempt evidence records the remaining defects. The component analyzer reduces raw index splits using exact-position links but cannot classify semantic attachments without reviewed evidence. The existing Waystone's 11 components are now classified, and its star attachment is repaired in v003; its geometry stage is capped at 3/3. Its Blender 5.2.2 source could not be inspected by the local Blender 5.1.1 process, which crashed in `python313.dll`. See the per-asset job records for exact current defects and gates. Static-prop rest/support/attachment checks apply; animated deformation checks are not applicable to this static batch.

Material and engine-depth work follows [the pinned UE 5.8 surface-quality profile](../../../docs/art-templates/model-profiles/unreal58-surface-quality-20261018.json) and the material-stage contract in `docs/art-templates/model-pipeline.md`. It records PBR map/channel requirements, reviewed height maps, optional Nanite displacement or bounded parallax, foliage options, Lumen/VSM views, and platform scalability checks. A Blender node and pixel audit (`material-channel-audit-v001.json`) confirms `Image_0` sRGB base color and `Image_1` Non-Color Green-to-roughness / Blue-to-metallic wiring. Blue is zero for every texel, so metallic is currently black; its suitability remains asset-specific. Normal/height/AO maps, measured relief, engine import/render comparisons and project settings remain open. No engine settings were changed by the profile.


## Production readiness reconciliation — 2026-10-18T22:55:14Z

No asset is production-ready. The existing profiles are prototype-authored and do not specify approved physical scale, target platform/hardware, geometry/UV/texture budgets or performance budgets. The three generated geometry stages are now `repair-required` because their saved review identifies visible artifacts; inference completion is not an acceptance pass. All previous attempts and files remain unchanged.

For all three Pixal assets, attempt-03 input and reference bytes match their recorded hashes, and runner/profile hashes match saved provenance. The saved profile pins preparation-script hash `ddc6ac31ee3c7b214a84b15d74630cae40749de67dd7aae8669fce20a87f3196`, while attempt-03 records `188d70b7b449e363dc9b6a38b7994458afa489f9b4320f27626e285c4598ee5d`; reconcile this before any further inference. Each job has a production-readiness `gate-summary.json`; UE 5.8 import, surface/depth comparisons and target-hardware performance remain open. Waystone source inspection still requires Blender 5.2.2, unavailable in the installed Blender 5.1.1 runtime.

## Selected PC acceptance target — 2026-10-18

The user selected Windows PC, 2560×1440 output and 60 fps. The development machine inventory is recorded as a candidate baseline in [the hardware evidence](../../../docs/evidence/pc-graphics-baseline-20261018.md), and adaptive policy is in [the graphics scalability contract](../../../docs/art-templates/pc-graphics-scalability.md). This does not yet define minimum hardware or prove performance: UE's active adapter/RHI, mature-tree scale/density, scene workload, memory/pass budgets and lower-tier behavior remain unmeasured. The job records link both documents; their earlier readiness snapshots remain historical evidence.

### Oak and iron boundary review — 2026-10-19

[CPU boundary review](geometry-boundary-review-20261019.md) maps the oak root sheet and iron ribbons/skirt to their fused main components, but neither has a safe closed face boundary. Both jobs retain their final geometry repair attempt. The iron report records paired first-hit surface seeds and protected rock/ore controls; the oak report records root-preservation bounds. Do not remove by color, thickness, height, or component size.

Read-only correspondence evidence in each asset's `Artifacts/Art/GreenlandsModelBatch-20261018/<asset-id>/surface-correspondence-v001/` folder confirms source-to-GLB/FBX triangle positions, UVs and nondegenerate face normals. Maximum paired-corner drift is under 0.11 micrometers; UVs match exactly. Oak has 12 and iron has 2 inherited zero-area triangles, listed by index in `surface-correspondence-v002.json`. Their major geometry defects remain, and neither final geometry attempt has been spent.

[Material channel audit](material-channel-audit-v001.json) confirms the packed map wiring and pixel ranges for the three Pixal assets. Image_1 Green drives roughness; Blue drives metallic and is zero-valued. These are authoring facts, not proof that each material is semantically correct.

[Segmentation mask audit](segmentation-mask-audit-v002.json) measures the saved grayscale masks. Attempt-03 has one connected foreground region for oak and iron, and 11 for the marker, with no foreground touching the image border. Numeric connectivity improved over attempt-01 (36, 59 and 543 regions), but this does not establish semantic contour correctness. Visual reference review remains open; all preparation attempts are exhausted.


### Waystone reuse check

A CPU review classified all 11 coincident-position GLB components as intentional, and GeometryRepair-v003 repaired the floating compass star with a closed backing. A CPU BVH check confirms the emblem backing is separated from the body by 3.6 mm minimum and 22.9 mm median; intended mount or clearance requires author interpretation. See [the attachment review](waystone-attachment-review-20261019.md) and the authoritative [waystone job record](../../../docs/art-templates/model-jobs/greenlands-waystone-reuse-20261018.json). The candidate reimports as 1,188 triangles, but the Blender 5.2.2 source still cannot be opened by local Blender 5.1.1; four crown faces are geometrically degenerate; full FBX correspondence, materials and UE 5.8 checks remain open. Geometry attempts are capped at 3/3, so the waystone is not complete.


### Mature-oak scale selection — 2026-10-19

The user selected an 18–22 m ground-to-crown height envelope for this oak. production-readiness-v002/target-selection.json records the decision and notes that uniform scaling of the current normalized mesh to 20 m implies an approximately 28 m crown width; no transform is applied until geometry defects and reference proportions are resolved. This scale belongs to this asset only.


### Marker geometry repair v003

A source-hash-pinned cleanup removed the 11 agent-classified isolated reconstruction-debris islands (2,108 triangles) while preserving all retained vertex positions. Fresh Blender 5.1.1 source, GLB and FBX checks pass bounds, triangles, UV/material/image dependencies and zero loose vertices; seam-aware inventory is now 15 components. This is not an accepted asset: major lean, fused underside/side sheets, unresolved components, scale/support and UE 5.8 gates remain. Geometry attempts are exhausted at 3/3. Evidence is under ignored `Artifacts/Art/GreenlandsModelBatch-20261018/PROP-GreenlandsWayfindingMarker-01/GeometryRepair-v003/`; see the authoritative job record.
