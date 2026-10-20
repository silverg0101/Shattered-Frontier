# Silverbrook Cottage 5 — concept prototype

Reference: `Cottage5-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Cottage-5.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_cottage5.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookCottage5.blend -P review_cottage5.py
```

The builder writes `SilverbrookCottage5.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `Cottage5-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The walls are 6.4 × 4.4 m. The whole model, including eaves, porch steps, woodshed, lantern bracket and chimney pots, is 7.86 × 5.78 × 5.98 m, which fits the town slot of 8 × 6 × 6 m (`SLOT`; the build asserts it). The origin is the slot's ground centre.
- The concept's pots reach about 6.8 m, so all heights are scaled by about 0.9: 2.45 m eaves, 5.0 m ridge, stack cap at 5.5 m, pots at 5.98 m.
- The main roof is a slate roof with very steep hips (0.6 m run). It is patched as drawn: board patches on the left slope and by the gable, and rust-red replacement tiles right of the gable and right of the dormer.
- The front gable is half-timbered with pale fieldstone infill. It sits 0.45 m left of the door, as drawn. It has a bressumer, corner posts, braces, a collar, a king post, a shuttered leaded window, bargeboards and a finial.
- Below the gable is a gabled timber porch hood on two posts and a stone landing. It shelters a round-headed plank door in a dressed-stone surround, with a hanging lantern.
- On the right slope is a dormer. At the right end, a fieldstone stack carries two clay pots. On the right wall is an open-fronted lean-to woodshed with a slate roof.
- The walls are pale fieldstone on a grey rubble plinth, with oak corner posts, wall plates, corner braces and window lintels. The two front windows are leaded casements with board shutters and planter boxes. A forged lantern hangs on a bracket at the front-left corner.
- **Inferred (not in the concept):** the sides and rear, the side and rear windows (the right one has no shutters, beside the shed), the stack's depth position, and the shed's depth and rear/side boarding.

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. `../silverbrook_kit.py` owns the mapping.

| Surface | Material | Scan |
|---|---|---|
| Slates | `SB_Slate` | `grey_roof_tiles_02`, one slate interior per slate, lichen from `lichen_rock` |
| Rust replacement tiles | `SB_Cottage5_RustTile` | `roof_tiles_14`; the warmest (R−G) tile interiors |
| Walls, gable and dormer infill, stack | `SB_Cottage5_Fieldstone` | `stone_wall` (pale rounded rubble) |
| Plinth | `SB_Cottage5_Plinth` | `rock_wall_08` (grey rubble) |
| Door surround, steps, stack cap | `SB_Ashlar` | `stone_wall_04`; single stones |
| Timber framing, roof boards, shed | `SB_OakTimber` | `weathered_brown_planks`, one plank band per beam |
| Door | `SB_DoorBoards` | `old_planks_02` |
| Shutters | `SB_PaintedShutter` | `blue_painted_planks`, aged darker |

Iron, lead came, lit glass, lantern glow, soil and chimney pots are plain parametric materials (Cottage 1's). Damp at the base uses the exported `Weathering` vertex colour.

## Limits

- **Unreal:** not imported yet. Import it with `../import_silverbrook.py` as `SM_SilverbrookCottage5`. There is no interior, and the door is closed.
- **Overlapping geometry:** individual slates overlap where the gable, dormer and porch roofs meet the main roof, and under the board patches. Hidden slates are culled per slate, but the meshes are not watertight.
- **Shader round-trip and GLB textures:** the same as Cottage 1. The exports carry UVs, `UVMap`/`WorldUV`, material slots and `Weathering`. The GLB has no images. The review checks that the FBX texture references resolve.
- **Not modelled:** the concept's set dressing (stacked logs in the shed, ivy, flowers, shrubs, fence, barrels, tub) belongs to settlement dressing.
