# Silverbrook Fence Section (street prop)

Reference: `PropFenceSection-reference.png`, cropped from `docs/Game-Design/concept-art/Silverbrook/Street Props.png` (SHA-256s in `asset-manifest.json`). Original procedural geometry interpreting the sheet.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propfencesection.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookPropFenceSection.blend -P review_propfencesection.py
```

Writes `SilverbrookPropFenceSection.{blend,fbx,glb}` and `asset-manifest.json` (includes `placement`); the review renders `PropFenceSection-{Beauty,Front,Side,Rear}.png` and checks GLB/FBX reimport. Metres, Z up, front -Y, origin ground centre. Blender 5.1.1.

## Interpretation

- 3.0 x 0.25 x 1.2 m (slot 3.0 x 0.3 x 1.2). Two identical square oak posts with chamfered caps; three split rails against the post fronts, lashed with two diagonal rope turns at each joint, crossing on the front, as on the sheet.
- **Tiling:** `tiling.length_m` = 3.0 along X. Place sections 3.0 m apart to butt end to end (two posts per joint), or 2.86 m apart so the end posts overlap. The rope bands reach exactly the 3.0 m bounds.
- **Inferred:** rail jitter and the post cap shape.

## Materials

All CC0 Poly Haven scans; sources in `../materials-provenance.json`.

| Surface | Scan |
|---|---|
| Posts, rails | `weathered_brown_planks` (`SB_OakTimber`) |
| Rope | `hessian_380` fibre (`SB_PropFenceSection_Rope`), tinted grey hemp; Poly Haven has no rope scan |

## Limits

- Posts stop at ground level (no buried part). Rope is round 6-sided strands in rounded-rectangle turns (about 4,900 triangles in all); the knot is not modelled. Slot y is 0.3 m for 0.25 m of geometry (rounded up).
