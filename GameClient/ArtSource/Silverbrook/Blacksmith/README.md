# Silverbrook Blacksmith — concept prototype

Reference: `Blacksmith-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Blacksmith.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_blacksmith.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookBlacksmith.blend -P review_blacksmith.py
```

The builder writes `SilverbrookBlacksmith.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `Blacksmith-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The walls are 10.4 × 7.6 m. The whole model, including eaves, lean-to, lanterns, finial and chimney pots, is 12.39 × 9.06 × 13.39 m, which fits the town slot of 14 × 12 × 14 m (`SLOT`; the build asserts it). About 80k triangles.
- The main roof is a steep-hipped slate roof: 5.4 m eaves, 11.6 m ridge, 0.4 m overhang.
- Left of centre is a jettied half-timbered gable (jetty 0.5 m) over the open forge bay. It has a shuttered leaded window, a navy banner with a gilt anvil hung from an iron rod, crossed iron straps, crossed bargeboards and a tall spire finial.
- The open forge bay is 6.1 m wide and 3 m deep, with a flagged floor, a boarded back wall, a boarded and joisted ceiling, and a bressumer on two posts with knee braces and stone piers. Inside are a brick hearth with a glowing fire bed, a tapered brick hood and flue, and an anvil on an oak stump. Two lanterns hang from the ceiling.
- To the right is an ashlar ground storey with a round-headed plank door in a quoined surround on a stone step, a wall lantern, and a shuttered leaded window with a planter box. Above it is a timber and plaster band with braces under the eaves. There is a dormer on the right slope.
- There are two stacks. A ridge stack carries two clay pots; a smaller stone flue on the left slope carries an iron stovepipe with a hat. On the left corner is a bracket lantern; on the right is a slated lean-to on two posts with a lantern.
- **Inferred (not in the concept):** the sides and rear, one window on each side wall, two at the back, the depth of the forge bay, the stacks' positions in depth, and the lean-to's depth.

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. `../silverbrook_kit.py` owns the mapping.

| Surface | Scan | Material |
|---|---|---|
| Slates | `grey_roof_tiles_02`, one slate interior per modelled slate, with lichen from `lichen_rock` | `SB_Slate` |
| Ashlar walls, stacks, piers, step, bay floor | `stone_wall_04`; quoins, voussoirs and caps sample single stones | `SB_Ashlar` |
| Gable, dormer and band infill | `patterned_clay_plaster` | `SB_LimePlaster` |
| Timber framing, ceiling, stump | `weathered_brown_planks` | `SB_OakTimber` |
| Door, shutters, bay back wall | `old_planks_02` (the concept's shutters are bare boards) | `SB_DoorBoards` |
| Forge hearth, hood and flue | `medieval_red_brick` (new), sooted | `SB_Blacksmith_ForgeBrick` |
| Banner | `denim_fabric_03` (new), darkened to navy | `SB_Blacksmith_Banner` |

Iron, lead came, lit glass, lantern glow, soil, chimney pots, the forge fire (`SB_Blacksmith_ForgeFire`) and the gilt anvil emblem (`SB_Blacksmith_Gilt`) are plain parametric materials. Damp at the base and soot use the exported `Weathering` vertex colour.

## Limits

- **Unreal:** not imported yet; `../import_silverbrook.py` is the path. There is no interior beyond the open forge bay, and the door is closed.
- **Overlapping geometry:** individual slates overlap where the gable and dormer roofs meet the main roof. Hidden parts are culled per slate, but the meshes are not watertight.
- **Shader round-trip:** the same as Cottage1. The exports carry UVs, `UVMap`/`WorldUV`, material slots and `Weathering`. The review checks that the FBX texture references resolve, and the GLB has no embedded images.
- **Not modelled:** the concept's set dressing belongs to settlement dressing. That covers the tool racks and hanging tools, barrels, the cart wheel, the log pile, the grindstone, flowers and the left wood rack.
