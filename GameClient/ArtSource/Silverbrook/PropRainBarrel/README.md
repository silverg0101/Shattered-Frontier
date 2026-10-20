# Silverbrook PropRainBarrel

Reference: `PropRainBarrel-reference.png` (copy of the owner sheet `docs/Game-Design/concept-art/Silverbrook/Street Props.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry following that sheet's item, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_proprainbarrel.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookPropRainBarrel.blend -P review_proprainbarrel.py
```

Writes `SilverbrookPropRainBarrel.{blend,fbx,glb}` and `asset-manifest.json` (includes `placement`); the review renders `PropRainBarrel-{Beauty,Front,Side,Rear}.png` and `exchange-review.json`. Metres, Z up, front -Y, origin ground centre. Unreal mesh `SM_SilverbrookPropRainBarrel`. `slot_m` is the bounds rounded up to 0.1 m (asserted).

## Interpretation

Poly Haven `wine_barrel_01` (iron-hooped oak, scaled 1.07 via `kit.model`) on four flat stones; a 1.4 m oak post behind it carries a cantilever arm, brace and a sloped wooden gutter whose iron funnel and pipe pour into the open barrel (dark water disc under the rim). 0.8 x 0.9 x 1.4 m, 11k triangles.
**Inferred:** brace, post straps, stones, pipe length. The model's bung faces front.

## Materials

Barrel: `ph_wine_barrel_01` (model set, `SB_PH_wine_barrel_01`); oak: `SB_OakTimber`; stones: `SB_Ashlar`; iron: `SB_ForgedIron`; water: `SB_PropRainBarrel_Water` (parametric).

## Limits

- Shader graph does not round-trip to FBX/GLB; Unreal rebuilds it from `asset-manifest.json` materials (see Cottage1 README).
- No moving parts exported.
- Water is a flat disc, no ripple.
