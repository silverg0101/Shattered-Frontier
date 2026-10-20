# Silverbrook PropSackPile

Reference: `PropSackPile-reference.png` (copy of the owner sheet `docs/Game-Design/concept-art/Silverbrook/Street Props.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry following that sheet's item, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propsackpile.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookPropSackPile.blend -P review_propsackpile.py
```

Writes `SilverbrookPropSackPile.{blend,fbx,glb}` and `asset-manifest.json` (includes `placement`); the review renders `PropSackPile-{Beauty,Front,Side,Rear}.png` and `exchange-review.json`. Metres, Z up, front -Y, origin ground centre. Unreal mesh `SM_SilverbrookPropSackPile`. `slot_m` is the bounds rounded up to 0.1 m (asserted).

## Interpretation

Ten tied hessian grain sacks (procedural soft bodies: sagging flat undersides, rounded tops, pinched necks with two cord wraps and a gathered frill, necks cocked upward) in four layers (4, 3, 2, 1) on a low oak pallet (two rough bearers, six loose planks); each sack is dropped onto a heightfield of those below and draped onto them. 1.58 x 1.07 x 1.02 m, 11k triangles.
**Inferred:** layering pattern, per-sack jitter (seeded).

## Materials

Sacks: `hessian_380` (`SB_PropSackPile_Hessian`); pallet: `SB_OakTimber`; cord: `SB_PropSackPile_Cord` (parametric).

## Limits

- Shader graph does not round-trip to FBX/GLB; Unreal rebuilds it from `asset-manifest.json` materials (see Cottage1 README).
- No moving parts exported.
- Hessian UVs are deliberately enlarged (0.9 m tile, about 6 mm threads) so the weave survives mipmapping at 2-3 m.
