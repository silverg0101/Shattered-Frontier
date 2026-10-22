# Silverbrook PropIvyClump

Reference: `concept/ivy-clump-sheet.png`. Four alpha cards textured with `PropIvyClump-atlas.png`.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propivyclump.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookPropIvyClump.blend -P review_propivyclump.py
```

The build regenerates the atlas: the flat grey background of the sheet is keyed to alpha with a soft edge, the grey is un-mixed from edge pixels, transparent texels are filled with leaf colour (no grey mip bleed), and the clump is cropped and resized to 1024 x 1024 RGBA. Writes `SilverbrookPropIvyClump.{blend,fbx,glb}`, `asset-manifest.json`; the review writes `PropIvyClump-{Beauty,Front,Side,Rear}.png` and `exchange-review.json`. Unreal mesh `SM_SilverbrookPropIvyClump`.

Metres, Z up. **Origin = ground centre**; four 1.2 x 1.0 m cards at 0/45/90/135 degrees rise from z=0, each a 6 x 5 grid leaning 7 cm at the top (alternate directions), odd cards mirrored in U. 240 triangles. Unreal: the manifest row has an `image`, so `import_silverbrook.py` uses `M_SB_Card` (masked, two-sided, clip 0.5).

## Limits

- Cards only: no volume, and edge-on views thin out. Single atlas, no per-card variation beyond mirroring.
- Shader graph does not round-trip to FBX/GLB; Unreal rebuilds materials from `asset-manifest.json`.
