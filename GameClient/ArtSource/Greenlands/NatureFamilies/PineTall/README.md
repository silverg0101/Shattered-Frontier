# PineTall (VEG-GreenlandsPineTall-01)

Look-profile role `tree_pine_tall`; Scots-pine-like, 21-25 m tall at scale 1.0. Reference: `docs/Game-Design/concept-art/Greenlands-Forest-Look-v1.webp`.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_pine.py
```

Needs `../Sources` (`py -3 ../acquire_sources.py`, hash-verified CC0 Poly Haven downloads, recorded in `../Sources/provenance.json` and copied into `asset-manifest.json`). Writes the four FBX, `PineTall.blend`, the renders and `asset-manifest.json`.
Metres, Z up, origin at the base centre, no ground in any mesh. Import: `../import_nature.py` (see `../README.md`).

| Mesh | Size m (W x D x H) | Triangles | Source |
|---|---|---|---|
| SM_PineTall_00 | 9.358 x 10.187 x 23.867 | 306,085 | pine_tree_01 |
| SM_PineTall_01 | 10.902 x 10.528 x 23.94 | 320,666 | pine_tree_01 |
| SM_PineTall_02 | 11.563 x 10.947 x 22.702 | 328,659 | pine_tree_01 |
| SM_PineTall_03 | 9.5 x 10.038 x 23.187 | 327,890 | pine_sapling_medium |

How: Poly Haven pine_tree_01 (a, b, c) and pine_sapling_medium (a) scans reshaped to a Scots habit: the lower trunk is stretched into a straight bare stem (56-62% of the height, root lump trimmed, ~20% of dead stubs kept), the crown is squeezed vertically into the top 40-45% as a flat dense umbrella, needle sprays thinned to ~300k triangles and grown so they clump. Bark: grey plated trunk scan blending gradually into Poly Haven pine_bark (orange flaky) through the crown zone (vertex colour R). Trunk-only collision prism (UCX) in each FBX.

Budget: Nanite (opaque geometry foliage, no alpha), no LOD chain. LOD0 about 305-330k triangles.

Placement: look profile place.scale is [0.95, 1.4]: meshes are 21-25 m tall at 1.0, so placed pines span 20-35 m (spec 20-30 m); sink 0.

Renders: `PineTall-Variants.png`, `PineTall-Detail-Trunk.png`, `PineTall-Detail-Crown.png` (red 1.8 m figure is render only).

Sources:
- [pine_sapling_medium](https://polyhaven.com/a/pine_sapling_medium) (CC0-1.0, Rob Tuytel, Rico Cilliers)
- [pine_tree_01](https://polyhaven.com/a/pine_tree_01) (CC0-1.0, Rob Tuytel, Rico Cilliers)
