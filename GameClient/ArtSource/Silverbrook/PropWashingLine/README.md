# Silverbrook PropWashingLine

Reference: `concept/washing-line-sheet.png`. Original procedural geometry interpreting it.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propwashingline.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookPropWashingLine.blend -P review_propwashingline.py
```

Writes `SilverbrookPropWashingLine.{blend,fbx,glb}` and `asset-manifest.json`; the review writes `PropWashingLine-{Beauty,Front,Side,Rear}.png` and `exchange-review.json`. Unreal mesh `SM_SilverbrookPropWashingLine`.

Metres, Z up, front -Y. **Origin = ground centre between the posts**; the line runs along X, props lean towards +Y. Two 2.4 m log posts 3.5 m apart (bark scan), rope wraps and cross stubs, a leaning prop each, a 0.2 m sag rope, 12 pegs, six cloth pieces (two linen, two undyed wool incl. trousers, one indigo tunic, one linen smock; hessian/teddy/denim scans). Cloth is double-sided geometry (front + back faces), so it needs no two-sided material. 3.70 x 0.71 x 2.41 m, 2.7k triangles.

## Limits

- Shader graph does not round-trip to FBX/GLB; Unreal rebuilds materials from `asset-manifest.json`.
- No grass or stones at the post feet (the sheet has them); folds are a fixed sine, no simulation.
