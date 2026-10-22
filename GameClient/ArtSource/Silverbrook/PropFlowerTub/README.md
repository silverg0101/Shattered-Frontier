# Silverbrook PropFlowerTub

Reference: `concept/flower-tub-sheet.png` (Codex). Original procedural geometry interpreting it (WP6c review fix 7).

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propflowertub.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookPropFlowerTub.blend -P review_propflowertub.py
```

Writes `SilverbrookPropFlowerTub.{blend,fbx,glb}` and `asset-manifest.json`; the review writes `PropFlowerTub-{Beauty,Front,Side,Rear}.png` and `exchange-review.json`. Unreal mesh `SM_SilverbrookPropFlowerTub` (`import_silverbrook.py PropFlowerTub`).

Metres, Z up, front -Y. **Origin = bottom centre.** 0.5 m oak half-barrel, 0.45 m tall: 20 staves (weathered plank scan, two facets each on a bulged profile), three iron hoops, damp soil 5 cm below the rim, six geraniums (leaf discs, stems, five-floret umbels about 0.25-0.3 m above the rim) and ten trailing thyme strands over the rim. 0.56 x 0.55 x 0.75 m, 5.6k triangles.

## Variants

The mesh is red. Flowers are slot `SB_GeraniumBloom` (shared with PropSillPlanter); `MI_SB_GeraniumBloom_Pink` is the pink tub. `silverbrook_props.dress_house_details` picks one per house.

## Limits

- Shader graph does not round-trip to FBX/GLB; Unreal rebuilds materials from `asset-manifest.json`.
- No rivets on the hoops; the thyme's white flowers are not modelled.
