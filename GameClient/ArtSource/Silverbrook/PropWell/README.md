# Silverbrook Prop Well

Reference: `PropWell-reference.png` (copy of the whole `docs/Game-Design/concept-art/Silverbrook/Street Props.png` sheet; the STONE WELL is one item; SHA-256 in `asset-manifest.json`). Asset ID in `asset-manifest.json`. Original procedural geometry; late-medieval northern-European village prop.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propwell.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookPropWell.blend -P review_propwell.py
```

The builder writes `SilverbrookPropWell.{blend,fbx,glb}` and `asset-manifest.json` (with `slot_m` and `placement`); the review renders `PropWell-{Beauty,Front,Side,Rear}.png` and reimports GLB/FBX (`exchange-review.json`, asserts FBX textures resolve). Shared helpers: `../prop_common_wns.py`. Metres, Z up, front -Y, origin ground centre. Unreal mesh `SM_SilverbrookPropWell`.

## Interpretation

- 1.98 x 1.98 x 2.99 m inside a 2.0 x 2.0 x 3.0 m slot, 8.0k triangles. Round grey rubble curb (four courses of irregular blocks plus coping, `SB_PropWell_Rubble`), four oak posts with iron straps and braces standing on the coping, plates, a steep slate hip roof with hip caps and a turned finial, an oak axle with rope drum and iron crank, and a Poly Haven wooden bucket on rope over the shaft. Inferred: the hidden king rafters, shaft water, and the drum details.

## Materials

| Surface | Material | Scan |
|---|---|---|
| Curb | `SB_PropWell_Rubble` | `stone_wall_04`, darkened grey |
| Roof | `SB_Slate` (Cottage1 args) | `grey_roof_tiles_02` |
| Frame, axle | `SB_OakTimber` (Cottage1 args) | `weathered_brown_planks` |
| Iron | `SB_ForgedIron` | parametric |
| Rope, water | `SB_PropWell_Rope`, `SB_PropWell_Water` | parametric |
| Bucket | `SB_PH_wooden_bucket_01` | Poly Haven model `wooden_bucket_01` (already in Models) |

## Limits

- Not imported into Unreal yet. the shader graph does not round-trip to FBX/GLB (UVs, `Weathering` colour and slots do).
