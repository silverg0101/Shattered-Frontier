# Silverbrook PropGrindstone

Reference: `PropGrindstone-reference.png` (crop of `docs/Game-Design/concept-art/Silverbrook/Street Props.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry following that sheet, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propgrindstone.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookPropGrindstone.blend -P review_propgrindstone.py
```

Writes `SilverbrookPropGrindstone.{blend,fbx,glb}` and `asset-manifest.json` (includes `placement`); the review renders `PropGrindstone-{Beauty,Front,Side,Rear}.png` and `exchange-review.json`. Metres, Z up, front -Y, origin ground centre. Unreal mesh `SM_SilverbrookPropGrindstone`.

## Interpretation

A pale sandstone wheel (0.86 m, 72-segment, smooth-shaded rim, weathered grey-brown tint, single-stone interior of `sandstone_blocks_05`, damp low on the rim) on an iron axle through two oak A-frames with ties, a bent iron hand crank (collar, arm, offset) with a wooden handle on the right, and an oak water trough with iron straps and water surface under the wheel.
Size 1.26 x 1.04 x 1.19 m (the build sets `slot_m` to this rounded up to 0.1 m and asserts the fit); 1.1k triangles (budget 40k).
**Inferred (not in the concept):** the trough's walls and strapping; the wheel has no hole or flange detail beyond oak plates.

## Materials

Sandstone: `sandstone_blocks_05` (`SB_PropGrindstone_Sandstone`); oak `SB_OakTimber`; trough `old_planks_02` (`SB_DoorBoards`); iron `SB_ForgedIron`; water `SB_PropGrindstone_Water` (plain).

## Limits

- Shader graph does not round-trip to FBX/GLB; Unreal rebuilds it from `asset-manifest.json` materials (see Cottage1 README).
- No interiors, no moving parts exported.
