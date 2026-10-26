# Construction geometry mesh prototypes v001

This package exports three canonical BLD-01 pilot shapes as isolated 3D mesh prototypes: `cell-full`, `slab-half`, and `slope-1to1`. They use the exact normalized vertices and face loops from `geometry-source/`; the mesh builder applies only the fixed 0.5 m unit conversion. Each object has its origin at the minimum grid corner, construction-local X/Y axes, and Z up.

Each shape includes an editable Blender source, GLB, FBX, and neutral-clay CPU render. The export contains one mesh only; render lights, camera, and floor are staging objects in the Blender source and are not exported. Fresh-process source reopen checks and empty-scene GLB/FBX reimports passed in Blender 5.1.1. The FBX axis conversion changes bounds by less than 0.0001 m; the GLB importer triangulates the faces while preserving the measured envelope. Per-shape checks and hashes are in `manifest.json` and `source-reopen-check.json`.

## Status and limits

These are **prototype-subset models**, not production-approved building assets. The source geometry tool declares `prototype-subset` and `acceptance_claim=false`; the package does the same. No contacts or silhouette were invented, and no decorative geometry was added. Timber/stone surface treatments, UVs, LODs, collision, seam binding, full GEO-02 compatibility, GEO-03 engine checks, and performance evidence remain open. Use the meshes to begin construction-kit 3D integration review only; do not mark structural acceptance from these files.

The source and complete machine-readable results are in `manifest.json`. `provenance/attempt-history.json` retains the bounded build/preview attempt history. The CPU-only reproduction command from the repository root is:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe' --background --factory-startup --python tools/Build-ConstructionGeometryModels.py -- --payload-dir GameClient/ArtSource/ConstructionGeometryPilot-20261018/Model-v001/geometry-source --output-dir Artifacts/ConstructionGeometry/<new-output-folder>
```

The tool refuses an existing output folder. Blender 5.1.1 was used for this saved version.
