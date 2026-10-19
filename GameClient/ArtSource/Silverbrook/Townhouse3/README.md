# Silverbrook Townhouse 3 — concept prototype

Reference: `Townhouse3-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Townhouse-3.png`; SHA-256 in `asset-manifest.json`). Concept asset `BLD-GreenlandsTownhouse-01`, variant 3. Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_townhouse3.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookTownhouse3.blend -P review_townhouse3.py
```

The builder writes `SilverbrookTownhouse3.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `Townhouse3-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The concept is drawn 6 m wide and about 10 m high (10.9 m to the finial), but the slot is 8 × 7 × 9 m. The width is kept: the stone storey is 6.2 m across. The height is compressed, mostly in the roof. The whole model is 7.65 × 6.31 × 8.98 m, which fits the slot (`SLOT`; the build asserts it). The origin is the slot's ground centre.
- The ground storey is coursed pale sandstone, 3.3 m high. It has two round-headed ledged plank doors with iron straps and ring handles, in dressed-stone surrounds (jamb quoins and voussoirs). Each door has two stone steps, and the door starts at the top step. Between the doors is a canted bay window with an oak frame and a panelled oak base on a stone plinth: two leaded lights at the front, one on each side. It has a small hipped slate roof. Two forged lanterns hang on brackets beside it.
- The upper storey is jettied 0.4 m at the front and 0.25 m at the sides. It sits on a deep bressumer with joist ends and six braced oak brackets. It is half-timbered lime plaster with posts, rails and braces. Two leaded two-light windows have planter boxes below them.
- The front gable spans almost the full width. It has a tie beam, a three-light leaded window, studs, a collar, a king post, heavy bargeboards, and a timber finial with an iron spike. Its eaves are level with the eaves of the steep-hipped blue-grey slate main roof. There is a sandstone stack with two clay pots on the right of the front slope, and iron gutters and round downpipes at both front corners.
- **Inferred (not in the concept):** the sides and rear, roof depth, side and rear windows, and the stack's depth position. The gable apex sits at the main ridge (the concept draws it slightly higher), so that the gable roof dies into the main roof inside the 9 m height. The doors are round-headed; the concept's heads are faintly pointed.

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. No new scans were needed.

| Surface | Material | Scan |
|---|---|---|
| Slates, bay roof, ridge and hip caps | `SB_Townhouse3_Slate` | `grey_roof_tiles_02`, darkened blue-grey; one slate interior per modelled slate; sparse `lichen_rock` |
| Ground storey, steps, stack, door surrounds | `SB_Townhouse3_Ashlar` | `sandstone_blocks_05` (pale, rough-faced, coursed); quoins, voussoirs and stack caps sample single blocks |
| Upper storey and gable infill | `SB_LimePlaster` | `patterned_clay_plaster` |
| Timber framing, brackets, bay frame, planters | `SB_OakTimber` | `weathered_brown_planks`, each beam confined to one plank's grain band |
| Doors, bay base panels | `SB_DoorBoards` | `old_planks_02` |

Iron (lanterns, straps, gutters, downpipes, finial spike), lead came, lit glass, lantern glow, planter soil and chimney pots are plain parametric materials, shared with Cottage1 (`SB_ForgedIron`, `SB_LeadCame`, `SB_LitGlass`, `SB_LanternGlow`, `SB_PlanterSoil`, `SB_ChimneyPot`). Damp at the base uses the exported `Weathering` vertex colour.

## Limits

- **Unreal:** not imported yet (`../import_silverbrook.py` handles it). There is no interior, and the doors are closed.
- **Custom geometry:** `kit.wall(arch=...)` and `kit.prism` orient faces by their first three points, which turns concave outlines inside out. This build's own `prism` (with a Newell normal and a volume assert) closes the door-arch spandrels and the gable and side-wall profiles.
- **Overlapping geometry:** individual slates (~64k triangles in total, including everything else) overlap where the gable roof meets the main roof. A few main-roof slates show under the gable bargeboards at the eave, and hip edges are stepped under the hip caps. The meshes are not watertight.
- **Downpipes:** they run straight down from the jettied corner, so they stand 0.3 m off the stone storey on iron stays.
- **Shader round-trip:** this works as for Cottage1. The exports carry UVs, the `UVMap`/`WorldUV` layers, material slots and the `Weathering` colours. The GLB has no images, and the FBX references `../Textures` relatively. The review asserts that the FBX references resolve.
- **Not modelled:** window-box flowers, tubs, crates, barrels and shrubs belong to settlement dressing.
