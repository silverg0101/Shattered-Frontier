# Silverbrook Townhouse 5 — concept prototype

Reference: `Townhouse5-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Townhouse-5.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry interpreting that elevation, not image reconstruction. Town asset `BLD-GreenlandsTownhouse-01`, variant 5.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_townhouse5.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookTownhouse5.blend -P review_townhouse5.py
```

The builder writes `SilverbrookTownhouse5.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `Townhouse5-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The walls are 5.8 × 4.6 m, matching the concept's 6 m width. The whole model, including eaves, sign bracket, finial and chimney pots, is 7.27 × 5.98 × 8.95 m, which fits the town slot of 8 × 7 × 9 m (`SLOT`; the build asserts it).
- **Heights are compressed about 0.8×.** The concept is about 11 m to the finial. The ground storey and door keep a usable height (3.3 m stone storey, 1.6 m door spring on a 0.39 m stoop). The upper storey, gable and roof take the reduction.
- The ground storey is pale rough-faced ashlar on a plinth. It has a pointed (two-centred) arched plank door with iron straps and a ring, set in jamb quoins and voussoirs, with three steps and a bracket lantern.
- The shop front has a glazed plank door on one step, a narrow side light, and a two-light leaded shop window over a stallboard. It sits under a slate lean-to awning with hipped ends, carried on two braced posts on stone pads, with a hanging lantern.
- The jettied half-timbered upper storey has a moulded bressumer on eight corbels, posts, rails and braces. It has a three-light window with a planter and a single tall light on the right.
- The cross gable runs front to back, with its apex above the main ridge. It has a balustraded balcony on four brackets at its foot, a two-light gable window, a collar, a king post and struts, bargeboards, and a timber finial with an iron spike.
- The main slate roof is hipped at the right end and dies into the cross gable on the left. It has a small gabled dormer and a stack with two clay pots behind the ridge.
- There are iron gutters and downpipes on both sides. A navy banner with a gilded stag head hangs on a scrolled iron bracket from the left wall.
- **Inferred (not in the concept):** the sides and rear, including one ground and one upper window on each side, two ground and two upper windows at the back, and the rear gable; the stack's depth position. The concept's dormer planter is omitted.

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. `../silverbrook_kit.py` owns the mapping. No new scans were downloaded.

| Surface | Material | Scan |
|---|---|---|
| Slates, ridge caps | `SB_Slate` | `grey_roof_tiles_02` (one slate interior per modelled slate) with `lichen_rock` |
| Ground storey, plinth, steps, stack, quoins, voussoirs | `SB_Townhouse5_Ashlar` | `sandstone_blocks_05` (pale rock-faced blocks); dressed stones sample single blocks |
| Upper-storey and gable infill | `SB_LimePlaster` | `patterned_clay_plaster` |
| Timber framing, balcony, awning, planter | `SB_OakTimber` | `weathered_brown_planks`, each beam confined to one plank's grain band |
| Doors | `SB_DoorBoards` | `old_planks_02` |
| Sign banner | `SB_Townhouse5_Banner` | `denim_fabric_03`, darkened to navy |

Iron (`SB_ForgedIron`), lead came, lit glass, lantern glow, planter soil, chimney pots and the stag's gilding (`SB_Townhouse5_Gilt`) are plain parametric materials. Damp at the base uses the exported `Weathering` vertex colour.

## Limits

- **Unreal:** not imported yet. `../import_silverbrook.py` is shared and was not edited here. There is no interior, and the doors are closed.
- **Pointed door:** `kit.door` and `kit.wall(arch=...)` only make round heads. The two-centred arch, its spandrel fills (convex strips, so the faces cannot invert) and its voussoirs are built in this package's script.
- **Overlapping geometry:** individual slates overlap where the cross-gable and dormer roofs meet the main roof, and at the awning hips. Hidden slates are culled per slate, but the meshes are not watertight.
- **Shader round-trip:** same as Cottage 1. The exports carry UVs, `UVMap`/`WorldUV`, material slots and `Weathering`. The review checks that the FBX texture references resolve. The GLB has no embedded images.
- **Not modelled:** the concept's set dressing (flowers, hanging baskets, barrel, crates, shop-window wares, the small shop shield sign) belongs to settlement dressing.
