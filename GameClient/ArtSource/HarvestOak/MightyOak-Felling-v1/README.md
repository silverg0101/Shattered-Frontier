# Mighty Oak felling collision candidate v1

**Status:** `SM_MightyOak_FallCollision.fbx` was imported into fresh `MightyOakFallV2` with 52 UCX hulls; the accepted upper-tree source hash is unchanged. The V2 runtime path is experimental and remains disabled by its flag. Felling motion and collision quality review are pending.

The collision base duplicates the accepted upper-tree mesh. Its 52 closed convex hulls follow 54 clean geodesic contour bands and four local fork bands at or above a 0.06 m equivalent radius. The four ambiguous junctions use separate short contour-geodesic arcs to preserve branch gaps. Finer limbs remain outside this proxy. Provenance, dimensions, FBX hash and per-hull measurements are in [CollisionCandidate-v1/manifest.json](CollisionCandidate-v1/manifest.json); Blender FBX round-trip results are in [CollisionCandidate-v1/validation.json](CollisionCandidate-v1/validation.json).

The hull source fit has no nonmanifold edges, no hull vertex count above 26, and no sampled hull surface farther than 0.25 m from source wood (maximum 0.1955 m). The three-view overlays are in [BranchContours/CorrectedRootLinks/LocalHullPrototype-r06/previews](BranchContours/CorrectedRootLinks/LocalHullPrototype-r06/previews). This geometry has not passed fall-motion or in-game quality review.

To reproduce, choose a new absolute output directory whose parent already exists and whose directory does not yet exist:

```powershell
& 'C:/Program Files/Blender Foundation/Blender 5.2/blender.exe' --background --factory-startup --threads 4 --python 'GameClient/ArtSource/HarvestOak/MightyOak-Felling-v1/BranchContours/tools/Build-LocalHullPrototype.py' -- --output 'C:/path/to/new-output-directory'
```

The builder reads the corrected versioned contour audit at `BranchContours/CorrectedRootLinks/branch-contour-audit.json`, refuses a missing output argument or any existing output path, and writes previews/metrics plus `CollisionCandidate-v1/` only below the new directory. The imported packet above remains unchanged by reproduction.

Source credit: **Mighty oak trees by Jagobo, CC BY 4.0** ([Sketchfab source](https://sketchfab.com/3d-models/mighty-oak-trees-4f6ab5594a8a415aba3f958682b9ced5)).
