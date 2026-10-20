# Silverbrook Hay Bales

Reference: `PropHayBales-reference.png` (crop of `docs/Game-Design/concept-art/Silverbrook/Street Props.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry interpreting that sheet, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_prophaybales.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookPropHayBales.blend -P review_prophaybales.py
```

Writes `SilverbrookPropHayBales.{blend,fbx,glb}` and `asset-manifest.json` (includes `placement`); the review renders `PropHayBales-{Beauty,Front,Side,Rear}.png` and checks GLB/FBX reimport. Shared helpers: `../hay_props_common.py`. Metres, Z up, front -Y, origin ground centre.

## Interpretation

- 2.1 x 0.8 x 1.3 m (slot 2.1 x 0.8 x 1.3, asserted), about 4.3k triangles. Three 0.95 x 0.6 x 0.56 m bales: two on the ground end to end, one on top, each with two twine loops and loose stalks at the cut ends. (The count is 3, matching the sheet.)

## Materials

| Surface | Material | Scan |
|---|---|---|
| Bales | `SB_PropHayBales_Straw` | `reed_roof_04` straw window per bale |
| Twine | `SB_PropHayBales_Twine` | parametric pale hemp-grey, 14 mm bevelled cord |

## Limits

- Bales are rounded boxes; the straw is texture only, with a few loose stalk boxes.
- Unreal: not imported yet; `../import_silverbrook.py` owns the import. The shader graph does not round-trip to FBX/GLB (UVs, `UVMap`/`WorldUV`, slots and `Weathering` colours do).
