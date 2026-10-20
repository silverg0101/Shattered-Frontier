# Silverbrook River Bridge

`SM_SilverbrookRiverBridge`, asset `BLD-SilverbrookRiverBridge-01`. Designed to fit Silverbrook crossings; no owner concept (`reference` is null in the manifest). Context for the job: `GameClient/ArtSource/Greenlands/Silverbrook-Crossing-InGame.png` (flat graybox slab, narrowed raised banks, loose boulders). Pairs with `../BridgeAbutment/`.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_riverbridge.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookRiverBridge.blend -P review_riverbridge.py
```

The builder writes `SilverbrookRiverBridge.{blend,fbx,glb}` and `asset-manifest.json`. The review renders `RiverBridge-{Beauty,Front,Side,Rear}.png`, the stretch pair `RiverBridge-Stretch085*.png` / `RiverBridge-Stretch115*.png` (bridge, ground and water scaled in X), reimports the GLB and FBX into empty scenes, and writes `exchange-review.json`. Blender 5.1.1 was used.

## Orientation and levels

- **Span along X, front (one parapet face) at -Y, Z up, metres.** Origin: mid-span (x=0), centred in Y, at deck-end ground level.
- **Deck surface z:** 0.000 at both ends (x=+-11, flush with the bank, flat there so no step), **1.600 at mid-span (x=0, over the pier; the cambered road centre is 1.660)**, 0.955 at the arch crowns (x=+-5). Profile `z = 1.6*(0.5*(1-u^2) + 0.25*(1+cos(pi*u)))`, u=|x|/11. Steepest grade 0.20 (11 deg). The build ray-casts three lines down the deck and asserts every hit is the paving at that height.
- **Width:** 5.4 m over the masonry, 4.5 m walkable between parapets. Parapets are 0.85 m above the deck plus 0.14 m coping; end posts 1.35 m with pyramid caps.
- **Size:** 22.0 x 7.4 x 6.6 m (z -4.0 to 2.6), inside the slot 22 x 7.6 x 7 m (asserted). 7.4 m in Y is the cutwaters (1.0 m each side); the deck body is 5.4 m.
- **Below ground:** masonry runs to z=-4.0 (water assumed at -1.5, as StoneBridge); end masses at |x|=9..11 go to the bed and sit inside the abutments.
- **Placer notes:** the placer sizes the deck as `max(bridge_width, road width)`; this mesh is 5.4 m wide, so scale Y by about width/5.4 only if a wider deck is needed (cutwaters and posts scale with it). Pitch the actor about Y over the run exactly as for the old slab; the hump is symmetric about mid-span, so a tilt keeps both ends on the ground when the ends differ by up to roughly 0.5 m. Use the X scale (0.85 to 1.15) to fit 19 to 25 m.

## Interpretation

- Two low segmental arches, 8 m clear span, 1.5 m rise (radius 6.08 m), spring 1.2 m below the end-of-deck ground, meeting on one central pier 2 m thick (|x|<1). Arch faces carry dressed voussoir rings with a projecting keystone.
- The pier has pointed cutwaters on both faces, vertical to z=0.3 and capped with a ramp rising to the parapet plinth. Nothing is round in plan, so an X stretch only changes pier thickness and span proportions; checked in the 0.85 and 1.15 renders.
- Parapets are coursed dressed blocks (a first course 4 cm proud as a plinth) with coping, laid parallel to the deck slope over a masonry core. The deck is cobbled with a 0.06 m camber (crown at y=0, flush at the edges and at both deck ends).
- **Inferred:** everything; there is no concept image. Proportions follow small English and French late-medieval road bridges.

## Materials

CC0 Poly Haven scans at physical size; `../materials-provenance.json`. One new scan: `cobblestone_floor_01` (the deck). Parapet blocks, coping and voussoirs each sample one undistorted block interior of `medieval_blocks_03` (block length picked from 0.45/0.55/0.65 m to match the scan's 1:1 scale).

| Material | Surface | Scan |
|---|---|---|
| `SB_RiverBridge_Masonry` | Spandrels, soffits, piers, parapet core | `stone_brick_wall_001`, cooled like StoneBridge |
| `SB_RiverBridge_Dressed` | Voussoirs, cutwaters, parapet blocks, coping, posts | `medieval_blocks_03` (blocks sample single stones) |
| `SB_RiverBridge_Paving` | Deck cobbles/setts, true scale (1 m tile) | `cobblestone_floor_01` (new) |

Damp band: 0.65 m above the waterline (z=-1.5), written into `Weathering` by the build.

## Limits

- Overlapping solids (rings, parapet blocks, core) are not unioned; touching faces are hidden.
- Stretch beyond 0.85 to 1.15 flattens or lengthens the arches and stretches the cutwater points; stone texture density follows the stretch.
- No ivy, railings, lamps or waterway details. Not imported into Unreal.
