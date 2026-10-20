# Silverbrook Prop Notice Board

Reference: `PropNoticeBoard-reference.png` (copy of the whole `docs/Game-Design/concept-art/Silverbrook/Street Props.png` sheet; the NOTICE BOARD is one item; SHA-256 in `asset-manifest.json`). Asset ID in `asset-manifest.json`. Original procedural geometry; late-medieval northern-European village prop.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propnoticeboard.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookPropNoticeBoard.blend -P review_propnoticeboard.py
```

The builder writes `SilverbrookPropNoticeBoard.{blend,fbx,glb}` and `asset-manifest.json` (with `slot_m` and `placement`); the review renders `PropNoticeBoard-{Beauty,Front,Side,Rear}.png` and reimports GLB/FBX (`exchange-review.json`, asserts FBX textures resolve). Shared helpers: `../prop_common_wns.py`. Metres, Z up, front -Y, origin ground centre. Unreal mesh `SM_SilverbrookPropNoticeBoard`.

## Interpretation

- 1.85 x 1.0 x 2.45 m inside a 1.9 x 1.0 x 2.5 m slot, 2.5k triangles. Two oak posts set in the ground, a plank-backed oak-framed board with six pinned parchments (illegible ink strips, two wax seals), braced tie beams and a small slate gable roof. Inferred: roof framing and the rear.

## Materials

| Surface | Material | Scan |
|---|---|---|
| Footings | `SB_Ashlar` | `stone_wall_04` |
| Roof | `SB_Slate` | `grey_roof_tiles_02` |
| Posts, frame | `SB_OakTimber` | `weathered_brown_planks` |
| Board back | `SB_DoorBoards` | `old_planks_02` |
| Pins | `SB_ForgedIron` | parametric |
| Paper, ink, wax | `SB_PropNoticeBoard_{Parchment,Ink,Wax}` | parametric |

## Limits

- Not imported into Unreal yet. the shader graph does not round-trip to FBX/GLB (UVs, `Weathering` colour and slots do).
