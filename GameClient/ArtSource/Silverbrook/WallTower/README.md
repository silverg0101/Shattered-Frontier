# Silverbrook Wall Tower — tower in the town wall

Reference: `WallTower-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Wall Tower.png`; SHA-256 in `asset-manifest.json`). Asset `BLD-GreenlandsWallTower-01`. Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_walltower.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookWallTower.blend -P review_walltower.py
```

The builder writes `SilverbrookWallTower.{blend,fbx,glb}` and `asset-manifest.json`. The review renders `WallTower-{Beauty,Front,Side,Rear,Walk}.png`, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. The preview places a `WallSection` segment on each side. Blender 5.1.1. Metres, Z up, front (field side) faces -Y, origin at ground centre.

## Interpretation

- The slot comes from `wall.tower` in `../../Greenlands/silverbrook-town-layout.json`: 8 × 8 × 12 m. The model is exactly 8.0 × 8.0 × 12.0 m, and the build asserts it. The concept is read at about 84 px/m.
- **Plan:** a chamfered square (octagon). It stands on a battered plinth 8 m across at the ground, with a dressed cap at 2.1–2.3 m. Above that the shaft draws in by 0.24 m per side, up to the corbel table at 8.55 m. Quoins run up the four corners either side of the front and rear faces.
- **Front:** a round-headed loop in a dressed surround low down, three more above the string course, and one low loop on each chamfer, as in the concept. A corbelled string course at 5.28–5.46 m lines up with the wall-walk.
- **Head:** two-stage stone corbels stand on all eight corners, with iron-strapped oak brackets between them. They carry an overhanging course 0.6 m proud of the shaft. Above it is a 0.5 m parapet with crenel sills, three merlons on each flat face and a tall capped merlon on each chamfer (to 12.0 m). A flagged roof sits behind the parapet.
- **Stag banners:** two navy banners hang from forged arms either side of the front, kept inside the 8 m slot. The concept's arms reach a little wider. The stag and trim are copied from the Watchtower.
- **Wall joint:** the wall runs along X. Each side carries the last 1.1 m of a `WallSection` segment end: curtain, half wall-turret, wall-walk and rear parapet. Their inner end is buried in the tower. The joint is built from WallSection's own dimensions, so a segment placed with its end at x = ±4 m butts flush:
  - The joint face is asserted vertex-for-vertex against the end section in `../WallSection/SilverbrookWallSection.blend`.
  - The two halves close a complete wall-turret over the joint, as between two segments.
  - The masonry keeps the segment's UV phase, so the stonework continues across the joint.
- **Wall-walk:** the walk (z = 5.40, y −0.45 to 1.15) runs on to a closed plank door in each side face. The door is round-headed in a dressed surround, and its threshold is the walk surface.
- **Inferred (not in the concept):** the rear and the side faces, the doors from the walk, the roof flags, and all depths. The loops are dark recesses behind proud surrounds, not through-openings.

## Materials

Poly Haven CC0 scans from `../Textures` (provenance in `../materials-provenance.json`). No new scans were downloaded.

| Material | Surface | Scan / args |
|---|---|---|
| `SB_WallSection_Masonry` | All walling. Quoins, courses, corbels, surrounds and caps are dressed blocks that each sample one stone | `rough_block_wall`, WallSection's exact args `dark=(.70,.74,.86), aged=.4`, with the same 20/6 m repeat |
| `SB_WallSection_Paving` | Wall-walk and roof flags | `medieval_blocks_03`, WallSection's exact args |
| `SB_WallSection_LoopVoid` | Dark back of the loops | WallSection's exact `kit.solid` args |
| `SB_OakTimber` | Head brackets | Cottage1 args |
| `SB_DoorBoards` | Walk doors | Cottage1 args |
| `SB_ForgedIron` | Straps, ring handles, banner arms | Cottage1 args |
| `SB_WallTower_Banner` | Banners | `denim_fabric_03`, darkened navy (same tint as the Watchtower's banner) |
| `SB_WallTower_Gilt` | Stag and banner trim | plain parametric gilt |

## Limits

- **Not yet imported to Unreal.** The expected mesh is `SM_SilverbrookWallTower`. Collision for the walk needs a flat top hull.
- **Geometry:** the tower is solid, with no interior or stair, and the doors are closed. The shaft taper is gentler than the concept's so the joint stays inside the side faces. The masonry comes from the texture, apart from the dressed parts. 13.1k triangles.
- **Shader round-trip:** same as Cottage1. The exports carry UVs, `UVMap`/`WorldUV` layers, material slots and the `Weathering` vertex colour. The GLB carries no images; the FBX references the textures relatively, and the review checks that they resolve.
