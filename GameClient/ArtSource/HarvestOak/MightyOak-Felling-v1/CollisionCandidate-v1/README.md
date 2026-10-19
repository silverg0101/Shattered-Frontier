# Mighty Oak felling collision candidate v1

**Status:** Imported into fresh `MightyOakFallV2` with 52 UCX hulls; the accepted upper-tree source hash is unchanged. The V2 runtime path remains experimental and disabled by its flag. Felling motion and collision quality review are pending.

The FBX contains base mesh `SM_MightyOak_FallCollision` and 52 closed convex objects named `UCX_SM_MightyOak_FallCollision_00`–`_51`. The source-derived proxy represents all 54 clean geodesic contour bands and all four local fork bands at or above a 0.06 m equivalent radius. At ambiguous forks, each piece uses a separate 0.18 m contour-geodesic arc near its linked clean band. Finer limbs remain excluded.

Hull fit: 0 nonmanifold edges, maximum 26 vertices per hull, 8,224 surface samples, none over 0.25 m from source wood, maximum distance 0.1955 m. Blender FBX round-trip passed with one base plus 52 UCX meshes and base bounds 13.6335 × 12.2571 × 8.9643 m at import scale 1.0. See [manifest.json](manifest.json) and [validation.json](validation.json). This does not replace in-game fall-motion and collision-quality review.

To reproduce into a separate fresh directory, run from the repository root and choose a new absolute output path with an existing parent:

```powershell
& 'C:/Program Files/Blender Foundation/Blender 5.2/blender.exe' --background --factory-startup --threads 4 --python 'GameClient/ArtSource/HarvestOak/MightyOak-Felling-v1/BranchContours/tools/Build-LocalHullPrototype.py' -- --output 'C:/path/to/new-output-directory'
```

The builder reads `BranchContours/CorrectedRootLinks/branch-contour-audit.json`. It requires `--output` and refuses an existing path, then writes previews/metrics and a new `CollisionCandidate-v1/` beneath that path. It does not overwrite this imported packet.

Source credit: **Mighty oak trees by Jagobo, CC BY 4.0** ([Sketchfab source](https://sketchfab.com/3d-models/mighty-oak-trees-4f6ab5594a8a415aba3f958682b9ced5)).
