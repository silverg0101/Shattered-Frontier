# Silverbrook Farmhouse — concept prototype

Reference: `Farmhouse-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Farmhouse.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry interpreting that elevation, not image reconstruction. Concept asset id `BLD-GreenlandsFarmhouse-01`.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_farmhouse.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookFarmhouse.blend -P review_farmhouse.py
```

The builder writes `SilverbrookFarmhouse.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `Farmhouse-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The house walls are 8.8 × 5.4 m. The whole model, including eaves, porch, lean-to, finial and chimney pots, is 11.81 × 7.05 × 6.89 m, which fits the town slot of 12 × 8 × 7 m (`SLOT`; the build asserts it). It has 74,276 triangles, under the 120k house budget (also asserted).
- The main roof is a long, steep-hipped slate roof: 2.75 m eaves, 5.85 m ridge, 0.4 m overhang. The hip ends are steeper than the long slopes, as in the concept. The build also asserts that no stone, plaster or timber vertex rises through the roof boarding over the wall footprint. This is the check against wall tops showing through the hips.
- Right of centre is a half-timbered cross gable with crossed bargeboards and a finial. It has a shuttered leaded window, and below that a boarded hatch (**inferred** loft hatch).
- On the left slope is a plastered dormer with a leaded casement. A squared-rubble ridge stack at the right end carries two clay pots.
- Left of the gable, a gabled timber porch on a stone landing shelters a round-headed plank door. The door sits in a dressed-stone surround and has iron straps and a ring handle. The porch posts stand on stone pads.
- There are four forged lanterns: one hanging in the porch, two on wall brackets beside the porch, one on the right wall. A fifth hangs from a tie in the lean-to.
- The ground storey is coursed squared rubble with oak corner posts, a mid rail, short studs to the wall plate, and corner braces. The front window is a wide leaded casement with board shutters and a planter box.
- On the right end is an open lean-to: a slated pent roof on three oak posts, with a wall ledger, rafter-ties and braces.
- **Inferred (not in the concept):** the sides and rear, one window on each side wall, two at the back, the chimney's depth position, and the lean-to's depth (4.35 m).

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. `../silverbrook_kit.py` owns the mapping. No new scans were downloaded.

| Surface | Material | Scan |
|---|---|---|
| Slates (main, gable, dormer, porch, lean-to) | `SB_Slate` | `grey_roof_tiles_02`, one slate interior per slate, with lichen from `lichen_rock` |
| Walls, stack, landing, door surround, post pads | `SB_Farmhouse_Rubble` | `rough_block_wall` (pale grey-beige squared rubble); quoins, voussoirs and the stack cap sample single blocks |
| Gable and dormer infill | `SB_LimePlaster` | `patterned_clay_plaster` |
| Timber framing, roof boarding | `SB_OakTimber` | `weathered_brown_planks` |
| Door, loft hatch | `SB_DoorBoards` | `old_planks_02` |
| Shutters (brown boards, as the concept) | `SB_Farmhouse_Shutter` | `old_planks_02`, aged |

Iron (`SB_ForgedIron`), lead came, lit glass, lantern glow, planter soil and chimney pots are plain parametric materials, with the same arguments as Cottage 1.

## Limits

- **Unreal:** not imported yet (`../import_silverbrook.py` handles it, as `SM_SilverbrookFarmhouse`). There is no interior, and the door is closed.
- **Overlapping geometry:** slates overlap where the gable, dormer and porch roofs meet the main roof. The lean-to slates and the side-wall posts also overlap where they meet. Hidden slates are culled per slate, but the meshes are not watertight.
- **Shader round-trip and GLB:** same as Cottage 1. The exports carry UVs, `Weathering` vertex colours and material slots. The GLB has no images; the FBX references the textures relatively (the review asserts they resolve).
- **Not modelled (set dressing):** the log stack, barrels, sacks, wagon wheel, tools, fencing, hoist pulley, climbing plant, window-box flowers and shrubs. The concept's recessed log alcove right of the porch is plain wall here.
