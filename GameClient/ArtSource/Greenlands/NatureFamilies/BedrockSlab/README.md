# BedrockSlab (ROCK-GreenlandsBedrockSlab-01)

Look-profile role `bedrock_slab`; flat fractured granite slabs, 2.7-3.3 m across and 0.45-0.5 m high at scale 1.0. Reference: `docs/Game-Design/concept-art/Greenlands-Forest-Look-v1.webp`.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_slab.py
```

Needs `../Sources` (`py -3 ../acquire_sources.py`, hash-verified CC0 Poly Haven downloads, recorded in `../Sources/provenance.json` and copied into `asset-manifest.json`). Writes the four FBX, `BedrockSlab.blend`, the renders and `asset-manifest.json`.
Metres, Z up, origin at the base centre, no ground in any mesh. Import: `../import_nature.py` (see `../README.md`).

| Mesh | Size m (W x D x H) | Triangles | Source |
|---|---|---|---|
| SM_BedrockSlab_00 | 2.741 x 2.389 x 0.438 | 10,324 | marble_rock_01 |
| SM_BedrockSlab_01 | 3.256 x 2.473 x 0.519 | 12,789 | rock_08 |
| SM_BedrockSlab_02 | 2.746 x 1.982 x 0.454 | 9,406 | marble_rock_01 |
| SM_BedrockSlab_03 | 2.909 x 2.413 x 0.512 | 11,873 | rock_surface |

How: Original geometry (height grid cut to an angular outline, Voronoi fracture plates, V cracks, steep rim to the soil line); surface is Poly Haven marble_rock_01 / rock_08 / rock_surface grey-graded with box-projected UVs; moss collects in cracks and patches.

Budget: Nanite, convex collision. Budget: 30k triangles each; actual ~9-13k.

Placement: look profile place.scale is [1.5, 2.6]: meshes are 2.4-3.4 m across at 1.0, so placed slabs span 3.6-8.8 m (spec 3-8 m); sink 0.35.

Renders: `BedrockSlab-Variants.png`, `BedrockSlab-Variants-Top.png` (red 1.8 m figure is render only).

Sources:
- [marble_rock_01](https://polyhaven.com/a/marble_rock_01) (CC0-1.0, Amal Kumar)
- [mossy_rock](https://polyhaven.com/a/mossy_rock) (CC0-1.0, Rob Tuytel)
- [rock_08](https://polyhaven.com/a/rock_08) (CC0-1.0, Rob Tuytel)
- [rock_surface](https://polyhaven.com/a/rock_surface) (CC0-1.0, Amal Kumar)
