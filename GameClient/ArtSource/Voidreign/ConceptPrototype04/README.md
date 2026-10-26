# Voidreign concept prototype

This is an editable procedural Blender concept prototype based on `Voidreign.png`. It is a rough blockout with a skeletal flank motif, horn, dark barding, skull charms, mane and tail strands. The reference shows a death unicorn at about 17 hands; the current model is approximately 2.5 m tall and 4.0 m long including horn and tail, with ground at Z=0 and its muzzle toward -Y.

The model is organized into body/anatomy, tack, optional supernatural effects, studio presentation and a separate 1.8 m seated rider guide. The structural front, side and rear previews omit the rider and effects. The GLB and FBX include only body and tack. The source image is preserved beside this README.

## Reproduce

From the repository root in PowerShell:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe' --background --factory-startup --python-exit-code 1 --python tools/Generate-Voidreign.py -- --output "$PWD\Artifacts\Art\Voidreign-Review" --reference 'C:\Users\grego\Downloads\Voidreign.png'
```

Generation used Blender 5.1.1. The generator refuses to write into a non-empty output folder.

## Review and limitations

See `previews/Beauty.png`, `Front.png`, `Side.png`, `Rear.png` and `Rider-scale.png`. The model preserves the overall four-legged mount silhouette, horn, dark palette, exposed rib motif, tack and skull accents. Anatomy is assembled from editable primitive meshes; mane/tail are stylized curves. It is a **concept blockout**, not production topology, a sculpted reconstruction, a rigged or animated asset, or Unreal-validated content. The flank ribs are external arches over a solid body, not a modeled open thoracic cavity. Hidden surfaces and far-side ornaments are inferred from the reference. No authored UVs or image textures are included; materials are procedural flat colors.

`asset-manifest.json` records source hash, tool version, bounds, counts, outputs and checks. `validation-report.json` records a fresh-process .blend render and exchange-format reimports. GLB and FBX retain model geometry, scale and material assignments. No gameplay, collision, map or Unreal content was changed.
