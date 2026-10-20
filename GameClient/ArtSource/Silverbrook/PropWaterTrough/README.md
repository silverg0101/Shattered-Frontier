# Silverbrook Water Trough

Reference: `PropWaterTrough-reference.png` (crop of `docs/Game-Design/concept-art/Silverbrook/Street Props.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry interpreting that sheet, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propwatertrough.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookPropWaterTrough.blend -P review_propwatertrough.py
```

Writes `SilverbrookPropWaterTrough.{blend,fbx,glb}` and `asset-manifest.json` (includes `placement`); the review renders `PropWaterTrough-{Beauty,Front,Side,Rear}.png` and checks GLB/FBX reimport. Shared helpers: `../hay_props_common.py`. Metres, Z up, front -Y, origin ground centre.

## Interpretation

- 2.1 x 0.93 x 0.72 m (slot 2.2 x 1.0 x 0.8, asserted), about 1.8k triangles. Two courses of staggered rough-hewn grey stone blocks (long side walls, end blocks, floor slab), still water 9 cm below the rim (own mesh and material slot `SB_PropWaterTrough_Water`), flat moss tufts at the base.

## Materials

| Surface | Material | Scan |
|---|---|---|
| Blocks | `SB_PropWaterTrough_Granite` | `rock_wall_08` (dark weathered grey stone, one clean interior per block) |
| Moss | `SB_PropWaterTrough_Moss` | `mossy_rock` |
| Water | `SB_PropWaterTrough_Water` | parametric dark water, opaque

## Limits

- The water is opaque; Unreal should replace it with a translucent water material.
- Unreal: not imported yet; `../import_silverbrook.py` owns the import. The shader graph does not round-trip to FBX/GLB (UVs, `UVMap`/`WorldUV`, slots and `Weathering` colours do).
