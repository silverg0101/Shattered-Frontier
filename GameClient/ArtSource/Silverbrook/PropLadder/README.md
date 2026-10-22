# Silverbrook PropLadder

Reference: `concept/ladder-sheet.png`. Original procedural geometry interpreting it.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propladder.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookPropLadder.blend -P review_propladder.py
```

Writes `SilverbrookPropLadder.{blend,fbx,glb}` and `asset-manifest.json`; the review writes `PropLadder-{Beauty,Front,Side,Rear}.png` and `exchange-review.json`. Unreal mesh `SM_SilverbrookPropLadder`.

Metres, Z up. **Origin = ground centre between the feet.** Modelled straight along +Z, rails along X (0.4 m centres, 0.5 m overall, 0.09 m deep with the lashing); the placer leans it (about 75 degrees, noted as `lean_deg` in the manifest placement). Two squared rails and ten round rungs passing through them (ends 3.5 cm proud), a hemp loop at each of the 20 joints. Rails and rungs: `old_planks_02` scan. 0.50 x 0.09 x 3.2 m, 2.8k triangles.

## Limits

- Shader graph does not round-trip to FBX/GLB; Unreal rebuilds materials from `asset-manifest.json`.
- Lashing is a single torus per joint, not a crossed wrap.
