# Silverbrook PropWheelbarrow

Reference: `PropWheelbarrow-reference.png` (crop of `docs/Game-Design/concept-art/Silverbrook/Street Props.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry following that sheet, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propwheelbarrow.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookPropWheelbarrow.blend -P review_propwheelbarrow.py
```

Writes `SilverbrookPropWheelbarrow.{blend,fbx,glb}` and `asset-manifest.json` (includes `placement`); the review renders `PropWheelbarrow-{Beauty,Front,Side,Rear}.png` and `exchange-review.json`. Metres, Z up, front -Y, origin ground centre. Unreal mesh `SM_SilverbrookPropWheelbarrow`.

## Interpretation

Oak rails carry a plank tray (floor and low splayed sides, front and back boards); two splayed legs at the rear; an 8-spoke oak wheel with twelve felloes and an iron tyre at the front on an iron axle.
Size 0.56 x 1.53 x 0.69 m (the build sets `slot_m` to this rounded up to 0.1 m and asserts the fit); 1.8k triangles (budget 40k).
**Inferred (not in the concept):** tray proportions and rear legs; the concept shows a V-section tray.

## Materials

Oak: `weathered_brown_planks` (`SB_OakTimber`); tray boards: `old_planks_02` (`SB_PropWheelbarrow_Boards`); iron: plain `SB_ForgedIron`.

## Limits

- Shader graph does not round-trip to FBX/GLB; Unreal rebuilds it from `asset-manifest.json` materials (see Cottage1 README).
- No interiors, no moving parts exported.
