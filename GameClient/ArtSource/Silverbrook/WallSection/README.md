# Silverbrook Wall Section — modular town-wall segment

Reference: `WallSection-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Wall Section.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_wallsection.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookWallSection.blend -P review_wallsection.py
```

The builder writes `SilverbrookWallSection.{blend,fbx,glb}` and `asset-manifest.json`. The review renders `WallSection-{Beauty,Front,Side,Rear,Walk}.png` (the preview places neighbouring segments either side to show the joint), then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1. Metres, Z up, front (field side) faces -Y, origin at ground centre.

## Interpretation

- Size comes from the `wall` entry in `../../Greenlands/silverbrook-town-layout.json`: 20 m segment along X, 3 m thick, 8 m high. The model is exactly 20.0 × 3.0 × 8.0 m (the build asserts it).
- **Tiling:** the segment ends flat at x = ±10 m and is mirror-symmetric, so both end faces have the same section (the build asserts this vertex-for-vertex). Square battered wall turrets stand every 6.67 m: full ones at x = ±3.33 m, and half ones at each end. When two segments are butted together, the two halves form a complete turret, including its loop, ring course and corner merlons. The masonry UVs repeat every 3.33 m (6 per segment), so the stonework is continuous across the joint.
- **Wall-walk:** one continuous flagstone surface at z = 5.40 m, 1.6 m wide (y −0.45 to 1.15), running the full 20 m. The turret heads sit in front of the walk line, so nothing blocks it.
- The rest follows the concept: a curtain with three cross arrow-loops per bay in dressed surrounds, and a lower string course at 2.25 m. A four-bracket corbel table carries the overhanging parapet: string course, crenel sills, five coped merlons per bay. The turrets have a battered base to 2.3 m, a shaft with a cross loop, and a head that overhangs on corbels, with a plain loop and corner merlons up to 8.0 m.
- Heights are the concept's 7 m elevation scaled to the 8 m slot height. The 3 m slot depth limits how far the turrets project: 0.6 m from the curtain, with the batter reaching the 1.5 m slot face.
- **Inferred (not in the concept):** the rear (town-side) face and the low 0.85 m rear parapet with coping; the wall-walk and its paving; all depths; the loops are 0.3 m-deep recesses with a dark back, not through-openings.

## Materials

Poly Haven CC0 scans from `../Textures` (provenance in `../materials-provenance.json`). No new scans were downloaded.

| Material | Surface | Scan / args |
|---|---|---|
| `SB_WallSection_Masonry` | All walling, turrets, merlons. Copings, string courses, corbels, loop surrounds and ring courses are separate dressed blocks that each sample one stone (`kit.patches('rough_block_wall',(.06,.035),dark=.7,frac=.01,std=.25)`) | `rough_block_wall`, `kit.scan(..., dark=(.70,.74,.86), aged=.4)`. The tint cools the scan toward the concept's pale grey. UV repeat is overridden to 20/6 m (`kit.SCALE`) |
| `SB_WallSection_Paving` | Wall-walk flagstones | `medieval_blocks_03`, `dark=(.55,.58,.62), damp=False, aged=.25` |
| `SB_WallSection_LoopVoid` | Dark back of each arrow loop | `kit.solid((.012,.012,.014), .95)` |

**For the wall tower and gatehouse:** to match this masonry, use `rough_block_wall` with the same `dark=(.70,.74,.86), aged=.4`. Size blocks with a repeat close to 3.33 m (UV scale `kit.SCALE[...] = 20/6`), and take dressed elements from `rough_block_wall` single-stone patches. The wall-walk is at z = 5.40, running from y −0.45 to 1.15. The parapet front is at y −0.95 (string course −1.00) and the curtain face at y −0.70.

## Limits

- **Not yet imported to Unreal**. The expected mesh is `SM_SilverbrookWallSection`. Collision for the walk needs a flat top hull.
- The end faces are closed, so a segment can end against a tower. Where two segments butt, the coincident end faces face each other and are hidden.
- **Shader round-trip:** same as Cottage1. The exports carry UVs, `UVMap`/`WorldUV` layers, material slots and the `Weathering` vertex colour; Unreal rebuilds the shading from the material table in `asset-manifest.json`. The GLB carries no images, and the FBX references the textures relatively; the review checks that they resolve.
- **Not modelled:** no stairs or access to the walk (those belong with towers and gatehouses), no hoardings, and no set dressing. The masonry blocks come from the texture, apart from the dressed parts, so there are no individually modelled wall stones. 9.4k triangles.
