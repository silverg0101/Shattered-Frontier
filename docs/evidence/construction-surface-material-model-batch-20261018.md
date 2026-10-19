# Construction surface material model batch 2026-10-18

Six 3D surface studies apply reference-guided material appearances to the canonical `cell-full` and `slope-1to1` mesh prototypes. The references are the image catalog packages `BLD-RT-CellFull-01`, `BLD-SM-CellFull-01`, `BLD-WT-CellFull-01`, `BLD-WT-CellFull-Ornamented-01`, `BLD-RT-RoofSlopeFace-01`, and `BLD-SM-RoofSlopeFace-01`. Each model records the source image path and SHA-256. Selected image regions guide the surface; no full scene or collage is projected onto the mesh.

## Outputs and checks

[Model-v006](../../GameClient/ArtSource/ConstructionGeometryPilot-20261018/Model-v006/) contains one editable Blender source, GLB, FBX, 1024×1024 base-color/roughness/height maps, and rendered preview per role. The height map drives a shallow shader bump in Blender and does not change structural geometry. GLB/FBX fresh-process reimports passed dimension, UV, material-slot, connected-component (position welded), and loose-vertex checks. Per-asset logs and hashes are in `batch-manifest.json` and `export-reimport-checks.json`.

The bounded build and fresh-import checkers are [Build-ConstructionMaterialModels.py](../../tools/Build-ConstructionMaterialModels.py) and [Validate-ConstructionMaterialModel.py](../../tools/Validate-ConstructionMaterialModel.py). Run only with Blender 5.1.1 and Pillow installed:

```powershell
py tools/Build-ConstructionMaterialModels.py --output-dir GameClient/ArtSource/ConstructionGeometryPilot-20261018/Model-vNNN
```

The tool refuses an existing output directory. Run the import checker for each export using its declared `--kind` and expected dimensions before accepting the batch.

## Review status

All six remain `prototype-subset`, `acceptance_claim=false`, and `engine_ready=false`. Current procedural color extraction and face projections have visible repetition and perspective-lighting artifacts; these are useful material studies, not production-ready textures. The batch does not close structural GEO-01 approval, applicable GEO-02 compatibility, assembled seam checks, Unreal material import, LOD/collision, or platform/performance gates. It does not certify the appearance against 1440p/60 FPS or an Unreal Engine version.
