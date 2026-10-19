# Construction geometry mesh prototype pilot

2026-10-18. This bounded asset step exports the three shapes emitted by the reviewed geometry pilot into isolated Blender/GLB/FBX prototypes. It is not an approval of production geometry, appearance, collision, or engine integration.

## Source and output

- Canonical pilot: [`tools/construction_geometry.py`](../../tools/construction_geometry.py), with copied payloads and manifest at [`ConstructionGeometryPilot-20261018/Model-v001`](../../GameClient/ArtSource/ConstructionGeometryPilot-20261018/Model-v001/README.md).
- Blender exporter and fresh-source validator: [`tools/Build-ConstructionGeometryModels.py`](../../tools/Build-ConstructionGeometryModels.py) and [`tools/Validate-ConstructionGeometryModel.py`](../../tools/Validate-ConstructionGeometryModel.py), Blender 5.1.1, CPU Eevee preview.
- Stable canonical shape keys and dimensions: `cell-full` 0.5 × 0.5 × 0.5 m; `slab-half` 0.5 × 0.5 × 0.25 m; `slope-1to1` 0.5 × 0.5 × 0.5 m. All origins are minimum grid corner (0,0,0); axes are local X/Y with Z up.
- Outputs: one `.blend`, `.glb`, `.fbx`, and `asset-view.png` per shape. Fresh-process source reopen passed for all three; fresh Blender reimports found one mesh in each exchange file and dimensions within 0.0001 m of the canonical mesh. Exact hashes and reports are in the package manifest.

Two Blender startup attempts failed before model output and are recorded in the local ignored Artifacts attempt folders and package attempt history. The third produced matching exports but its preview lighting clipped; the fourth corrected only the CPU preview illumination and completed the three models. No GPU inference/render job ran. No geometry repair was applied.

## Open gates

The geometry pilot itself declares `prototype-subset`, `acceptance_claim=false`. Production GEO-01 approved manifests/registry IDs and full applicable GEO-02 evidence remain absent. These neutral clay meshes have no timber/stone materials, UVs, LODs, collision, material seam binding, GEO-03 import validation, or platform performance measurements. Keep structural readiness blocked until those gates have their owning approvals and evidence.
