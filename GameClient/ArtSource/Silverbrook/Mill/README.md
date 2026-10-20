# Silverbrook Mill — concept prototype

Reference: `Mill-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Mill.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry interpreting that elevation, not image reconstruction. Asset ID `BLD-GreenlandsMill-01`.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_mill.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookMill.blend -P review_mill.py
```

The builder writes `SilverbrookMill.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `Mill-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The mill house walls are 7.2 × 5.2 m. The whole model, from the jib hoist to the outer wheel pier and including the spire finial, is 11.5 × 6.99 × 8.93 m. That fits the town slot of 12 × 10 × 9 m (`SLOT`; the build asserts it). The concept's vertical proportions were compressed so the finial stays under 9 m. About 80k triangles; the build asserts the 120k budget.
- The ground storey is cream sandstone. It has a pointed (two-centred) arch door with jamb quoins, voussoirs and a keystone. The door is a ledged plank door with iron strap hinges and a ring handle, under a bracketed slate canopy with a king-post truss. A bracket lantern hangs beside it.
- On the right of the ground storey is an open work bay, 1.2 m deep, with a boarded back and ceiling, a hanging lantern and a stone floor. A slate pent roof on three braced posts covers it.
- The upper storey is jettied 0.35 m on the front. It is half-timbered plaster on a bressumer, with joist ends, soffit boards and curved brackets. The front windows match the concept: a single window, a triple window with one long planter box, another single, and a shuttered window. All are leaded casements.
- The main roof is a hipped slate roof, with a 4.95 m eave and a 7.25 m ridge. A tall cross wing crosses it, with its ridge at 8.1 m. Its front gable has crossed bargeboards, a king post, and an oak finial post with a forged-iron spike. It also holds an open loft hoist door: two board leaves swung out 90°, a 0.6 m boarded recess, a hoist beam, a pulley, a rope and a hook. On the right slope are a dormer and a stack with two clay pots.
- The water wheel is a breastshot wheel, 3.2 m across, with its axle along X. It has 24 buckets, sole boards, twin rims, 2 × 8 spokes, an iron-banded hub and an oak axle. The axle rests in oak bearings on two stone piers. A slate lean-to wheel house on braced posts covers the inner pier.
- A timber jib hoist with a pulley, rope and hook projects from the left wall. A cast-iron downpipe with a hopper runs down the front-left corner.
- **Inferred (not in the concept):** the sides and rear, the rear gable of the cross wing, the side and rear windows (shuttered), the depth of the wheel house and piers, the depth and boarded back of the work bay, the closed-off loft recess, and where the stack sits along the depth.

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`, and the maps are in `../Textures`. No new scans were downloaded.

| Surface | Material | Scan |
|---|---|---|
| Slates, ridge and hip caps | `SB_Slate` | `grey_roof_tiles_02`, one slate interior per modelled slate; `lichen_rock` lichen |
| Ground storey, piers, stack, quoins, voussoirs | `SB_Mill_Ashlar` | `sandstone_blocks_05` (cream rough-faced blocks, as in the concept) |
| Plaster infill | `SB_LimePlaster` | `patterned_clay_plaster` |
| Framing, wheel, bargeboards, roof deck | `SB_OakTimber` | `weathered_brown_planks`, each beam confined to one plank band |
| Door, bay boarding, soffit, loft recess | `SB_DoorBoards` | `old_planks_02` |
| Shutters, loft door leaves | `SB_Mill_Shutter` | `old_planks_02` aged (the concept shows bare oak boards, not blue paint) |
| Hoist ropes | `SB_Mill_Rope` | `hessian_380` |

Iron, lead came, lit glass, lantern glow, planter soil and chimney pots are plain parametric materials (`SB_ForgedIron`, `SB_LeadCame`, `SB_LitGlass`, `SB_LanternGlow`, `SB_PlanterSoil`, `SB_ChimneyPot`, all with Cottage1's arguments).

## Limits

- **Water wheel is static.** The wheel, hub, iron bands and axle are a separate object, `SM_SBMill_Wheel`, centred on the axle line (x = WX, z = 1.7 m before recentring). `import_silverbrook.py` still merges every object into one static mesh. A turning wheel would need a separate rotating part later: its own package or mesh, pivoted on the axle and rotating about X, with the bearings left static. No water, race or splash is modelled; that belongs to the river and terrain.
- **Pointed door:** the kit's `door()` is round-headed, so the build script draws its own two-centred arch.
- **Overlapping geometry:** individual slates overlap at the cross-wing valleys and around the dormer, and hidden slates are culled one at a time. The meshes are not watertight. The renders show no wall tops through the hip roofs: the plate is held 6 cm under the front slope deck, and the stack stays inside the wall line.
- **Shader round-trip:** this is the same as Cottage1. The exports carry `UVMap`/`WorldUV`, material slots and the `Weathering` vertex colours, and `M_SB_Scan` rebuilds the shading in Unreal. The review checks that the FBX texture references resolve. The GLB is exported without images.
- **Not modelled:** the concept's set dressing (sacks, barrels, crates, potted plants, window-box flowers, tools, the workbench in the bay). There is no interior, and the door is closed. The loft recess is a shallow boarded niche.
