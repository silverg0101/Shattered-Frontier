# BoulderLargeMoss (ROCK-GreenlandsBoulderLargeMoss-01)

Look-profile role `boulder_large_moss`; rounded-to-blocky granite boulders, 1.6-2.6 m long at scale 1.0. Reference: `docs/Game-Design/concept-art/Greenlands-Forest-Look-v1.webp`.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_boulder.py
```

Needs `../Sources` (`py -3 ../acquire_sources.py`, hash-verified CC0 Poly Haven downloads, recorded in `../Sources/provenance.json` and copied into `asset-manifest.json`). Writes the four FBX, `BoulderLargeMoss.blend`, the renders and `asset-manifest.json`.
Metres, Z up, origin at the base centre, no ground in any mesh. Import: `../import_nature.py` (see `../README.md`).

| Mesh | Size m (W x D x H) | Triangles | Source |
|---|---|---|---|
| SM_BoulderLargeMoss_00 | 1.757 x 2.03 x 1.257 | 40,000 | boulder_01 |
| SM_BoulderLargeMoss_01 | 1.572 x 2.299 x 1.302 | 40,000 | namaqualand_boulder_06 |
| SM_BoulderLargeMoss_02 | 2.348 x 1.923 x 1.178 | 39,999 | namaqualand_boulder_03 |
| SM_BoulderLargeMoss_03 | 2.302 x 1.716 x 1.615 | 39,999 | namaqualand_boulder_04 |

How: Poly Haven scan meshes boulder_01, namaqualand_boulder_06/03/04, decimated to 40k triangles, re-graded from brown to grey granite in the material, moss (mossy_rock texture) blended by up-facing normals + noise in vertex colour R.

Budget: Nanite, convex collision. Budget: 40k triangles each (Nanite fallback is computed by the engine).

Placement: look profile place.scale is [1.0, 1.6]: meshes are 1.6-2.6 m long at 1.0, so placed boulders span 1.6-4.2 m (spec 1.5-4 m); sink 0.25.

Renders: `BoulderLargeMoss-Variants.png`, `BoulderLargeMoss-Variants-Top.png` (red 1.8 m figure is render only).

Sources:
- [boulder_01](https://polyhaven.com/a/boulder_01) (CC0-1.0, Rico Cilliers)
- [mossy_rock](https://polyhaven.com/a/mossy_rock) (CC0-1.0, Rob Tuytel)
- [namaqualand_boulder_03](https://polyhaven.com/a/namaqualand_boulder_03) (CC0-1.0, Jenelle van Heerden, Dario Barresi)
- [namaqualand_boulder_04](https://polyhaven.com/a/namaqualand_boulder_04) (CC0-1.0, Jenelle van Heerden)
- [namaqualand_boulder_06](https://polyhaven.com/a/namaqualand_boulder_06) (CC0-1.0, Greg Zaal, Jenelle van Heerden)
