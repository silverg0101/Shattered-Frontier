# Silverbrook Stables — concept prototype

Reference: `Stables-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Stables.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_stables.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookStables.blend -P review_stables.py
```

The builder writes `SilverbrookStables.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `Stables-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The walls are 10 × 6.4 m. The whole model, including eaves, hoist, lanterns, finials and chimney pots, is 10.96 × 7.63 × 6.97 m, which fits the town slot of 12 × 8 × 7 m (`SLOT`; the build asserts it). About 79k triangles. The concept's "8 m" width label disagrees with its own 7 m height scale; the slot and the drawn proportions were followed instead.
- The main roof is a steep-hipped slate roof: 2.95 m eaves, 6.2 m ridge, 0.35 m overhang, with post-and-spike finials at both ridge ends. The build asserts that no masonry or plaster vertex rises through the roof planes.
- In the centre is a half-timbered hay-loft gable flush with the front wall. It has a loft door opening with strap hinges, a collar, a king post and braces, plaster infill, bargeboards, a finial, and a hoist beam with an iron pulley and chain.
- On each side of the gable is a boarded, gabled dormer with a lit four-light window and a finial. A stone stack with two clay pots rises from the front slope, right of the right dormer.
- The ground storey is stone. It has a segmental-arched double plank carriage door with iron straps and ring handles, and two arched stall bays with X-braced half gates, each in a voussoir ring. Oak posts stand on stone bases and carry a plate beam with knee braces. Five forged lanterns hang there: four on brackets and one under the plate above the carriage door.
- **Inferred (not in the concept):** the sides and rear, with one leaded window on each side and two at the back; the stall bays as shallow boarded niches (0.7 m deep, manger rail, a hanging lantern) instead of interiors; the loft opening backed by boards; and the stack's position in depth.

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. `../silverbrook_kit.py` owns the mapping. No new scans were needed, and every material reuses Cottage 1's names and arguments.

| Surface | Scan | Material |
|---|---|---|
| Slates | `grey_roof_tiles_02`, one slate interior per modelled slate, with lichen from `lichen_rock` | `SB_Slate` |
| Walls, stack, post bases, voussoirs | `stone_wall_04`; voussoirs, bases and the stack cap sample single stones | `SB_Ashlar` |
| Gable infill and cheeks | `patterned_clay_plaster` | `SB_LimePlaster` |
| Frame, dormers, fascias, finials | `weathered_brown_planks`, each beam confined to one plank's grain band | `SB_OakTimber` |
| Carriage doors, gates, niche and loft boards | `old_planks_02` | `SB_DoorBoards` |

Iron, lead came, lit glass, lantern glow and chimney pots are plain parametric materials (`SB_ForgedIron`, `SB_LeadCame`, `SB_LitGlass`, `SB_LanternGlow`, `SB_ChimneyPot`).

## Limits

- **Unreal:** not imported yet. The intended mesh is `SM_SilverbrookStables` (`unreal_mesh` in the manifest). There is no interior, and the doors and gates are closed.
- **Overlapping geometry:** individual slates (~66k triangles in total) overlap where the gable and dormer roofs meet the main roof. The meshes are not watertight.
- **Shader round-trip:** this is the same as Cottage 1. The exports carry the UVs, the `Weathering` vertex colours and the material slots, and the review checks that the FBX texture references resolve. The GLB has no images.
- **Not modelled:** the concept's hay in the loft and stalls, the barrel, fences, shrubs and tools belong to settlement dressing. The loft doors are shown as hinge straps only, not as open leaves.
