# Silverbrook PropHitchingPost

Reference: `PropHitchingPost-reference.png` (copy of the owner sheet `docs/Game-Design/concept-art/Silverbrook/Street Props.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry following that sheet's item, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_prophitchingpost.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookPropHitchingPost.blend -P review_prophitchingpost.py
```

Writes `SilverbrookPropHitchingPost.{blend,fbx,glb}` and `asset-manifest.json` (includes `placement`); the review renders `PropHitchingPost-{Beauty,Front,Side,Rear}.png` and `exchange-review.json`. Metres, Z up, front -Y, origin ground centre. Unreal mesh `SM_SilverbrookPropHitchingPost`. `slot_m` is the bounds rounded up to 0.1 m (asserted).

## Interpretation

Two square oak posts on stone footings carry a mortised rail (iron straps at the joints); two forged iron tie rings hang from staples under the rail; a two-course grey ashlar mounting block (0.6 x 0.5 m, real-scale SB_Ashlar UVs) stands in front (-Y). 2.5 x 0.8 x 1.1 m, ~1k triangles.
**Inferred:** footing stones, staples, block placement and step (the sheet shows the block between the posts).

## Materials

Oak: `weathered_brown_planks` (`SB_OakTimber`); stone: `stone_wall_04` (`SB_Ashlar`, single-block patches); iron: `SB_ForgedIron`.

## Limits

- Shader graph does not round-trip to FBX/GLB; Unreal rebuilds it from `asset-manifest.json` materials (see Cottage1 README).
- No moving parts exported.
- The block reads as small coursed stones rather than one monolith (the scan is a rubble-ashlar wall).
