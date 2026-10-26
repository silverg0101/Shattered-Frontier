# Voidreign loose-component review and targeted cleanup

CPU-only review, 2026-10-18. No AI inference or GPU rendering was launched; the GPU remains available to the separate scenery batch. [Before](Component-Review.png) and [after](AfterDiagram/Component-Review.png) are CPU triangle-projection diagrams. Boxes locate the user-marked pieces; selected faces in the before view are drawn through the surface for identification, not as an occlusion test.

## Result

The two circled pieces are disconnected in the rest mesh as well as visibly unsupported:

| Source component ID | Location | Triangles | Minimum sampled gap to main component | Disposition |
| --- | --- | --- | --- | --- |
| `cb6d4a55ea8d:C0080` | Below head | 74 | about6.3cm | Removed in cleanup candidate |
| `cb6d4a55ea8d:C0065` | Below body | 92 | about4.7cm | Removed in cleanup candidate |

Open [Cleanup-v001/Voidreign-HairPrototype.blend](Cleanup-v001/Voidreign-HairPrototype.blend) for the new revision. The original model remains in [HairMotion-20261018](../HairMotion-20261018/README.md). Removal affects only163 vertices/166 triangles, leaving284,685 vertices and296,693 triangles. This is a reversible prototype repair, not approval to remove other separate ornaments or hair strands.

The source has22,323 raw indexed islands because texture/normal seams duplicate vertex indices. Exact-coincident-position analysis reduces that to230 connected components. This analysis does **not** weld vertices or alter UVs. It can also join intentionally touching parts and does not bridge near-coincident gaps. Neither count equals a defect total.

Cleanup leaves228 components unclassified. Some may be intended separate ornaments/hair; others need review. `component-classifications.json` records only the two reviewed source components and their cleanup disposition. Mesh-scoped component IDs change after topology edits; do not reuse source numeric component IDs against the cleanup mesh.

## Evidence

- `Inspection-v002/component-report.json`: final source inventory, tool/source/mesh hashes, component bounds/centroids/counts, sampled gaps and capability limits. NPZ arrays preserve exact component selections in that folder.
- `CleanupInspection-v002/component-report.json`: independent read-only inventory after removal;228 components remain.
- `Inspection/` and `CleanupInspection/`: earlier inventories retained with the repair attempt's original selection provenance.
- `Cleanup-v001/repair-report.json`: exact removal IDs, source hashes, reason and vertex/triangle delta.
- `Cleanup-v001/cleanup-validation.json`: fresh source and export checks. Retained coordinates, faces, UVs and material assignments are exactly unchanged. Retained deformed vertex positions match the pre-cleanup mesh at seven sampled frames (1,60,120,180,240,300,330), with measured maximum error0m in local mesh coordinates. This is not continuous animation or collision acceptance.

The reimported GLB retains the20-bone rig,4K maps and296,693 triangles; sampled bone poses pass the existing5mm check. The original cleanup validator records its default-FBX failure and returns exit2. The later [FBX import correction](FbxImport-v003/README.md) identifies Blender's auto-connected joints as the cause: the default importer fails, while a hash-bound source connectivity contract restores animation and passes all330-frame bone/sampled-surface checks. Use the contract adapter or GLB; this does not establish other applications' import behavior. Full mane/tail coverage, weight boundaries, thin bridges, semantic classification, deformed-surface clearance and engine runtime validation remain open.

The author inspected the CPU diagrams; an independent visual reviewer found the intended two deletions and no additional visible silhouette change in those views. Exact delta/UV/deformation checks support the limited deletion. No new material-lit or GPU animation renders were produced during this CPU-only work.

## Reproduce and reuse

`tools/Inspect-MeshComponents.py` is the read-only inventory entry point. Run background Blender with `--factory-startup --threads 4 --python-exit-code 1 --python <tool> -- --source <model.blend> --collection <asset-collection> --output <new-folder>`. Default connectivity accounts for exact-coincident seam vertices. Optional `--assembly-root <name>` removes a rigid, unit-scale motion root. Distances apply scene unit scaling; the largest component is a heuristic reference, not an approved semantic body. Automatic semantic decisions, thin-bridge detection, general motion attachment checks and cross-object gap comparisons are not implemented.

`tools/Plot-MeshComponentReview.py` reads the report/NPZ with CPU NumPy/Pillow and explicit `--component` IDs. `tools/Repair-MeshComponents.py` requires explicit reviewed IDs, the unmodified source/hash, inspection and a reason, and writes a new source/export revision. It never deletes by size threshold. `tools/Validate-VoidreignComponentRepair.py` checks this specific rig/profile after removal; adapt and validate other rig profiles before reuse. Source and tools are hashed in `manifest.json`.

Workflow: [model-pipeline.md](../../../../docs/art-templates/model-pipeline.md); authoritative current state: [Voidreign job](../../../../docs/art-templates/model-jobs/voidreign-motion-20261018.json). Image and Greenlands queues were not claimed or changed here.
