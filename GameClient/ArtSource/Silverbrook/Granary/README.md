# Silverbrook Granary — concept prototype

Reference: `Granary-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Granary.png`; SHA-256 in `asset-manifest.json`). Asset `BLD-GreenlandsGranary-01`. Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_granary.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookGranary.blend -P review_granary.py
```

The builder writes `SilverbrookGranary.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `Granary-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The concept is drawn 12 m wide and 9 m high, but the town slot is 8 × 8 × 10 m. The width is narrowed to fit the slot, and storey heights stay at full size. The walls are 6.8 × 6.0 m. The whole model, including stair, gallery, eaves and finials, is 7.84 × 7.62 × 9.74 m and fits the slot (`SLOT`; the build asserts it). The origin is the slot's ground centre.
- **Undercroft (0–2.5 m):** rubble-stone walls with pale ashlar corner quoins. At the front are three pale ashlar piers and a round-headed ledged double door with iron straps and two ring pulls, set in a dressed-stone surround of jamb quoins and voussoirs.
- **Gallery:** a timber gallery 0.9 m deep at 2.62 m runs across the front. Posts on the piers carry it, with knee braces, joists and boards. An open stair rises to it from the left: closed strings, 12 risers and a two-rail handrail.
- **Loft storey:** oak framing with lime-plaster panels, rails and braces. In the middle is a great pair of ledged-and-braced loft doors with long strap hinges and ring pulls. Each side has a wide and a narrow louvred vent and a bracket lantern.
- **Roof:** a steep-hipped slate main roof with 5.3 m eaves and an 8.4 m ridge. A taller loading gable crosses it, running front to back, with a 9.0 m apex. Each gable face has half-timbering, a large louvred vent, a collar and a king post. At the front, a hoist beam carries a pulley, rope and hook. There are two gabled louvred dormers, oak finials with forged-iron spikes, eaves gutters, and swan-necked downpipes at the front corners.
- **Inferred (not in the concept):** the sides and rear. These are solid rubble undercroft walls, one louvre on each side, two louvres and a louvred gable at the back, and the loading gable running through to the rear. The stair is steeper (about 45°) than drawn, so that it fits the narrowed front.

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. `../silverbrook_kit.py` owns the mapping.

| Surface | Material | Scan |
|---|---|---|
| Slates and ridge caps | `SB_Slate` | `grey_roof_tiles_02`, one slate interior per modelled slate; lichen from `lichen_rock` |
| Undercroft rubble walls | `SB_Granary_Rubble` | `rock_wall_08` (dark grey irregular rubble) |
| Quoins, piers, door surround | `SB_Ashlar` | `stone_wall_04`, single pale stones |
| Loft and gable panels | `SB_LimePlaster` | `patterned_clay_plaster` |
| Framing, louvres, dormers, stair strings | `SB_OakTimber` | `weathered_brown_planks`, each beam confined to one plank's grain band |
| Doors, gallery boards, treads, louvre backs | `SB_DoorBoards` | `old_planks_02` |

Iron (straps, rings, spikes, gutters, downpipes, pulley), lantern glow and the hoist rope (`SB_Granary_Rope`) are plain parametric materials.

## Limits

- **Unreal:** not imported yet. `../import_silverbrook.py` would import it as `SM_SilverbrookGranary`. There is no interior, and the doors are closed.
- **Overlapping geometry:** individual slates (most of the 70.8k triangles) overlap where the loading gable and the dormers meet the main roof. Hidden slates are culled, but the meshes are not watertight. The build asserts that no masonry or plaster vertex rises through the hip or gable roof planes.
- **Shader round-trip and GLB:** the same as Cottage1. The exports carry UVs, `UVMap`/`WorldUV` and the `Weathering` vertex colours. The GLB has no embedded images, and the FBX references `../Textures` relatively (the review checks this).
- **Not modelled:** the sack on the hoist, the barrels, crates and grain sacks, and the tools. These are settlement dressing.
