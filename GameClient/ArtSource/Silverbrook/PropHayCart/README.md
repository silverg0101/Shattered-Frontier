# Silverbrook Hay Cart

Reference: `PropHayCart-reference.png` (crop of `docs/Game-Design/concept-art/Silverbrook/Street Props.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry interpreting that sheet, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_prophaycart.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookPropHayCart.blend -P review_prophaycart.py
```

Writes `SilverbrookPropHayCart.{blend,fbx,glb}` and `asset-manifest.json` (includes `placement`); the review renders `PropHayCart-{Beauty,Front,Side,Rear}.png` and checks GLB/FBX reimport. Shared helpers: `../hay_props_common.py`. Metres, Z up, front -Y, origin ground centre.

## Interpretation

- 2.1 x 3.5 x 2.1 m (slot 2.2 x 3.6 x 2.2, asserted), about 10.8k triangles. Sheet target 3.5 m long, 2 m tall.
- Open-topped plank box (three plank courses, battens, iron straps, top rails) on an oak floor and sills, over an axle-tree with two 1.3 m spoked wheels (12 spokes, 12 felloes, nave with iron bands, nail-studded iron tyre). Shafts run under the bed and rest on a trestle prop leg at the front.
- Heaped loose hay mound over the box with scattered stalks. **Inferred:** wheel placement mid-box, trestle prop shape, rear of the cart.

## Materials

| Surface | Material | Scan |
|---|---|---|
| Wood | `SB_OakTimber` (Cottage1 arguments) | `weathered_brown_planks` |
| Iron | `SB_ForgedIron` (Cottage1 arguments) | parametric |
| Hay | `SB_PropHayCart_Straw` | `reed_roof_04` (straw area, mapped as a window to avoid the moss band) |

## Limits

- The hay is a smooth lumpy mound plus thin box stalks, not strands; the end faces show a fan in the UVs.
- The wheels are fixed (not split into parts).
- Unreal: not imported yet; `../import_silverbrook.py` owns the import. The shader graph does not round-trip to FBX/GLB (UVs, `UVMap`/`WorldUV`, slots and `Weathering` colours do).
