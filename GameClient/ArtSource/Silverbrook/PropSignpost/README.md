# Silverbrook Prop Signpost

Reference: `PropSignpost-reference.png` (copy of the whole `docs/Game-Design/concept-art/Silverbrook/Street Props.png` sheet; the SIGNPOST is one item; SHA-256 in `asset-manifest.json`). Asset ID in `asset-manifest.json`. Original procedural geometry; late-medieval northern-European village prop.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propsignpost.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookPropSignpost.blend -P review_propsignpost.py
```

The builder writes `SilverbrookPropSignpost.{blend,fbx,glb}` and `asset-manifest.json` (with `slot_m` and `placement`); the review renders `PropSignpost-{Beauty,Front,Side,Rear}.png` and reimports GLB/FBX (`exchange-review.json`, asserts FBX textures resolve). Shared helpers: `../prop_common_wns.py`. Metres, Z up, front -Y, origin ground centre. Unreal mesh `SM_SilverbrookPropSignpost`.

## Interpretation

- 1.09 x 0.36 x 2.78 m inside a 1.1 x 0.4 x 2.8 m slot, 1.0k triangles. Square oak post (0.12 m) with three iron bands on a small two-step stone base, a tall pointed finial, and three blank arrow-shaped arms (routed border, boss, bolts). Inferred: arm yaws and the rear.

## Materials

| Surface | Material | Scan |
|---|---|---|
| Footing | `SB_Ashlar` | `stone_wall_04` |
| Post, arms | `SB_OakTimber` | `weathered_brown_planks` |
| Bands, bolts | `SB_ForgedIron` | parametric |
| Carving | `SB_PropSignpost_Carving` | parametric |

## Limits

- Not imported into Unreal yet. the shader graph does not round-trip to FBX/GLB (UVs, `Weathering` colour and slots do).
