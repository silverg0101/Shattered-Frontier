# Silverbrook PropSillPlanter

Reference: `concept/sill-planter-sheet.png` (red and pink variants). Original procedural geometry interpreting it.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propsillplanter.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookPropSillPlanter.blend -P review_propsillplanter.py
```

Writes `SilverbrookPropSillPlanter.{blend,fbx,glb}` and `asset-manifest.json`; the review writes `PropSillPlanter-{Beauty,Front,Side,Rear}.png` and `exchange-review.json`. Unreal mesh `SM_SilverbrookPropSillPlanter`.

Metres, Z up, front -Y. **Origin = bottom centre of the trough's back edge**: the back face is vertical and flush with y=0 (the wall), the trough sits on a sill at z=0 and tapers towards the front. 0.9 m terracotta trough (solid material, no scan) with a lip band whose front has a chipped notch, damp soil, six geranium plants (leaf discs, stems, five-floret umbels) and ten trailing thyme strands over the front. 0.93 x 0.31 x 0.44 m, 5.8k triangles.

## Variants

The mesh is the red one. Flowers are the single slot `SB_GeraniumBloom`; `import_silverbrook.py` also makes `MI_SB_GeraniumBloom_Pink` (manifest `variants`), to be assigned to that slot for the pink planter.

## Limits

- Shader graph does not round-trip to FBX/GLB; Unreal rebuilds materials from `asset-manifest.json`.
- Terracotta is a flat solid: no scan was a fit for fired clay in `Textures`. Chips are modelled only on the front lip.
