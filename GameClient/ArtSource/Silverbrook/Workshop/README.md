# Silverbrook Workshop — concept prototype

Reference: `Workshop-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Workshop.png`; SHA-256 in `asset-manifest.json`). Asset `BLD-GreenlandsWorkshop-01`. Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_workshop.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookWorkshop.blend -P review_workshop.py
```

The builder writes `SilverbrookWorkshop.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `Workshop-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The walls are 5.5 × 4.6 m. The whole model, including the sign bracket, lean-to, finial and chimney pot, is 7.71 × 5.49 × 5.94 m, which fits the town slot of 8 × 6 × 6 m (`SLOT`; the build asserts it). About 44.6k triangles (house budget 120k).
- Steep-hipped slate main roof: 2.7 m eaves, 5.3 m ridge. A half-timbered cross gable on the left half of the front shares the ridge height. It has crossed bargeboards, a finial, a bressumer, a leaded window between posts, braces, a collar and a king post. A small dormer sits on the right slope.
- The concept's open workshop bay is modelled closed: two ledged-and-braced plank leaves with strap hinges and a cover batten, under a heavy lintel with knee braces. Stone piers stand under the jamb and corner posts, and there is rubble infill above the lintel.
- The right bay has rubble walling in an oak frame with braces and a shuttered leaded casement. On the right side is a slate lean-to on two posts with a ledger, rafters and knee braces.
- A trade sign (crossed hammers) hangs from a forged bracket on the left, and a forged lantern hangs on a bracket on the left jamb post.
- The ridge stack stands behind the ridge at the right hip, with one clay pot.
- **Changed to fit the slot:** the concept's chimney and gable peak rise to about 6.7 m. Here the gable peak equals the main ridge, and the stack stops 0.2 m above it so that the pot ends under 6 m.
- **Inferred (not in the concept):** the sides and rear (one window on the left side, two at the back, a plain right wall under the lean-to), the depth of the lean-to and the stack position, the piers under the side and rear posts, and the closed bay doors.

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. `../silverbrook_kit.py` owns the mapping. No new scans were downloaded.

| Surface | Material | Scan |
|---|---|---|
| Slates (main, gable, dormer, lean-to) | `SB_Slate` | `grey_roof_tiles_02`, one slate interior per modelled slate, with lichen from `lichen_rock` |
| Rubble walling (grey-beige fieldstone) | `SB_Workshop_Rubble` | `stone_wall` |
| Piers, chimney stack | `SB_Workshop_BlockStone` | `rough_block_wall` |
| Gable and dormer infill | `SB_LimePlaster` | `patterned_clay_plaster` |
| Timber frame, bargeboards, roof deck | `SB_OakTimber` | `weathered_brown_planks`, each beam confined to one plank band |
| Bay doors, sign board | `SB_DoorBoards` | `old_planks_02` |
| Shutters (brown, not painted) | `SB_Workshop_Shutter` | `old_planks_02`, aged darker |

`SB_ForgedIron`, `SB_LeadCame`, `SB_LitGlass`, `SB_LanternGlow` and `SB_ChimneyPot` are plain parametric materials, built with the same arguments Cottage1 uses. Damp at the base and soot on the stack use the exported `Weathering` vertex colour.

## Limits

- **Unreal:** not imported yet (`../import_silverbrook.py` takes it as `SM_SilverbrookWorkshop`). There is no interior, and the doors are closed.
- **Overlapping geometry:** where the gable and dormer roofs meet the main roof, individual slates overlap. Hidden slates are culled one at a time, but the meshes are not watertight.
- **Shader round-trip:** same as Cottage1. The exports carry UVs, `UVMap`/`WorldUV`, material slots and `Weathering`, and the review checks that the FBX texture references resolve. The GLB is exported without images.
- **Not modelled:** the concept's set dressing: the workshop interior (anvil, benches, tools, wheel), the barrels, crates, cart wheel, leaning timber, the pulley under the lean-to, and the chain hook on the lintel.
