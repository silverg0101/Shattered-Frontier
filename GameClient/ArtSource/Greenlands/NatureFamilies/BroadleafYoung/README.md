# BroadleafYoung (VEG-GreenlandsBroadleafYoung-01)

Look-profile role `tree_broadleaf_young`; birch/alder-like young broadleaf, 9-10.8 m at scale 1.0, 1-3 pale stems. Reference: `docs/Game-Design/concept-art/Greenlands-Forest-Look-v1.webp`.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_broadleaf.py
```

Needs `../Sources` (`py -3 ../acquire_sources.py`, hash-verified CC0 Poly Haven downloads, recorded in `../Sources/provenance.json` and copied into `asset-manifest.json`). Writes the four FBX, `BroadleafYoung.blend`, the renders and `asset-manifest.json`.
Metres, Z up, origin at the base centre, no ground in any mesh. Import: `../import_nature.py` (see `../README.md`).

| Mesh | Size m (W x D x H) | Triangles | Source |
|---|---|---|---|
| SM_BroadleafYoung_00 | 5.678 x 6.312 x 9.882 | 246,370 | procedural + tree_small_02 leaves |
| SM_BroadleafYoung_01 | 6.918 x 5.851 x 10.205 | 279,055 | procedural + tree_small_02 leaves |
| SM_BroadleafYoung_02 | 7.725 x 7.176 x 9.604 | 290,156 | procedural + tree_small_02 leaves |
| SM_BroadleafYoung_03 | 6.887 x 6.352 x 10.577 | 271,144 | procedural + tree_small_02 leaves |

How: Procedural stems/branches/twigs (no Poly Haven birch or alder exists), drooping upper crown, leaf sprays clustered at twig and branch tips (about 2x the earlier density); leaf pieces harvested from CC0 tree_small_02 with its alpha map; bark is Poly Haven japanese_hackberry_bark pushed to near-white with black lenticel bands in vertex colour R. One trunk collision prism (UCX) per stem.

Budget: Nanite with masked leaves, no LOD chain. LOD0 about 245-290k triangles (wood ~20-60k, leaves ~220k).

Placement: look profile place.scale is [0.6, 0.85]: meshes are 9-10.5 m tall at 1.0, so placed trees span 5.5-9 m (spec 6-10 m); sink 0.

Renders: `BroadleafYoung-Variants.png`, `BroadleafYoung-Detail-Crown.png`, `BroadleafYoung-Detail-Trunk.png` (red 1.8 m figure is render only).

Sources:
- [japanese_hackberry_bark](https://polyhaven.com/a/japanese_hackberry_bark) (CC0-1.0, Charlotte Baglioni)
- [tree_small_02](https://polyhaven.com/a/tree_small_02) (CC0-1.0, Rico Cilliers)
