# Silverbrook Cottage 6 — concept prototype

Reference: `Cottage6-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Cottage-6.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry interpreting that elevation, not image reconstruction. Variant 6 of `BLD-GreenlandsCottage-01`.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_cottage6.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookCottage6.blend -P review_cottage6.py
```

The builder writes `SilverbrookCottage6.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `Cottage6-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The walls are 6.7 × 4.4 m. The whole model, including eaves, porch, wall lanterns, finials and chimney pots, is 7.86 × 5.85 × 5.96 m, which fits the town slot of 8 × 6 × 6 m (`SLOT`; the build asserts it). The origin is the slot's ground centre.
- The main roof is a slate roof with very steep hips, as drawn: 2.5 m eaves, 5.1 m ridge, small timber blocks at both ridge ends.
- Over the front wall is a central half-timbered cross gable on a jettied bressumer: a leaded window with board shutters and a planter, braces, a collar and king post, crossed bargeboards and a finial.
- Two matching gabled dormers sit either side of it, each with a leaded window, a planter box under the sill, bargeboards and a finial.
- Below the gable is a small gabled porch on two coursed stone piers with timber caps, over a two-step stone landing. It shelters a round-headed plank door with iron straps and a ring handle in a dressed-stone surround (jamb quoins and voussoirs). A lantern hangs in the porch; two more hang on brackets at the front corners.
- The ground storey is pale coursed sandstone on a plinth course, framed with oak corner posts, plate, braces and window lintels. The two front windows are leaded casements with sills, board shutters and planters.
- A stone stack with two clay pots rises at the right end of the ridge. It stops under 6 m, so it stands lower above the ridge than in the concept.
- **Inferred (not in the concept):** the sides and rear, one window on each side wall, two at the back, the hip-end plaster, and the stack's depth position. The cross-gable ridge is kept level with the main ridge (the concept draws it slightly higher).

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. `../silverbrook_kit.py` owns the mapping. No new scans were downloaded.

| Material | Surface | Scan |
|---|---|---|
| `SB_Slate` | Slates, ridge and hip caps | `grey_roof_tiles_02`, one slate interior per modelled slate, lichen from `lichen_rock` |
| `SB_Cottage6_Ashlar` | Walls, plinth, piers, landing, stack, quoins, voussoirs | `sandstone_blocks_05` (pale rough-faced coursed blocks); single-stone parts sample one block |
| `SB_LimePlaster` | Gable, dormer and hip-end infill | `patterned_clay_plaster` |
| `SB_OakTimber` | Framing, bargeboards, planters, roof boarding | `weathered_brown_planks`, each beam confined to one plank's grain band |
| `SB_DoorBoards` | Door | `old_planks_02` |
| `SB_Cottage6_Shutter` | Shutters | `wood_planks_grey`, darkened to the concept's slate grey |

Iron, lead came, lit glass, lantern glow, soil and chimney pots are plain parametric materials (Cottage 1's names and arguments). Damp at the base and soot on the stack use the exported `Weathering` vertex colour.

## Limits

- **Unreal:** not imported yet; `../import_silverbrook.py` would import it as `SM_SilverbrookCottage6` with Nanite. There is no interior, and the door is closed.
- **Facing:** `kit.prism` and `kit.wall(arch=...)` orient by their first three points, which flips concave outlines (the door spandrels came out inside out). The build uses its own `prism` (Newell normal) for all fills and asserts each has positive volume.
- **Overlapping geometry:** individual slates overlap where the gable, dormer and porch roofs meet the main roof. Hidden slates are culled, but the meshes are not watertight.
- **Shader round-trip:** the Blender shader graph does not round-trip to FBX/GLB; the exports carry UVs, `UVMap`/`WorldUV`, material slots and the `Weathering` colours. The review checks that the FBX texture references resolve. The GLB is exported without images.
- **Not modelled:** set dressing (flowers, shrubs, trees, fence, barrel, pots, path stones) belongs to settlement dressing.
