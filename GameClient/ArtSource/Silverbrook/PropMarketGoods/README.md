# Silverbrook PropMarketGoods

Reference: `PropMarketGoods-reference.png` (crop of `docs/Game-Design/concept-art/Silverbrook/Street Props.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry following that sheet, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propmarketgoods.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookPropMarketGoods.blend -P review_propmarketgoods.py
```

Writes `SilverbrookPropMarketGoods.{blend,fbx,glb}` and `asset-manifest.json` (includes `placement`); the review renders `PropMarketGoods-{Beauty,Front,Side,Rear}.png` and `exchange-review.json`. Metres, Z up, front -Y, origin ground centre. Unreal mesh `SM_SilverbrookPropMarketGoods`.

## Interpretation

Poly Haven CC0 models via `kit.model`: wine barrel (iron hoops) and a wooden barrel, plus the bodies of crate_01 and crate_02 with lids and latches dropped. The crates hold red apples/tomatoes and green cabbages (simple spheres). Five tied hessian sacks (hessian_380 at ~3x magnification, wrinkled, sagging bases) (one on the barrel top) and two small jugs.
Size 2.59 x 1.79 x 1.36 m (the build sets `slot_m` to this rounded up to 0.1 m and asserts the fit); 37.9k triangles (budget 40k).
**Inferred (not in the concept):** layout; the Poly Haven crates are rope-handled chests, not slatted market crates; produce is spheres only. Barrel_02/barrel_03 were rejected as modern steel drums.

## Materials

Barrels/crates: `SB_PH_*` Poly Haven model scans; sacks and ties: `hessian_380` (`SB_PropMarketGoods_Hessian`, `_Rope`); produce, jugs and bottle plain colours (`SB_PropMarketGoods_*`).

## Limits

- Shader graph does not round-trip to FBX/GLB; Unreal rebuilds it from `asset-manifest.json` materials (see Cottage1 README).
- No interiors, no moving parts exported.
