# Silverbrook Townhouse 4 — concept prototype

Reference: `Townhouse4-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Townhouse-4.png`; SHA-256 in `asset-manifest.json`). Asset `BLD-GreenlandsTownhouse-01`, variant 4. Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_townhouse4.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookTownhouse4.blend -P review_townhouse4.py
```

The builder writes `SilverbrookTownhouse4.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `Townhouse4-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The concept is about 6 m wide and 10 m to the gable (about 11 m to the chimney pots). Heights are compressed by about 0.8 to fit the slot. The whole model is 6.78 × 6.93 × 8.88 m, inside the 8 × 7 × 9 m town slot (`SLOT`; the build asserts it). Widths keep the concept's proportions.
- The ground storey is 5.2 × 5.35 m of squared sandstone, 3.25 m tall. It has a pointed (equilateral) arch door with jamb quoins, voussoirs and a keystone. The ledged plank door has iron strap hinges, a ring knocker and a small pointed leaded light, and stands on three stone steps. Beside it is a three-light leaded window with flanking oak posts, a planter and a pent slate roof on brackets.
- The upper storey is jettied 0.35 m at the front and 0.15 m at the sides, on a bressumer and six corbel brackets. It is lime plaster with oak posts, rails and braces, and a three-light window with a planter.
- An off-centre front gable has crossed bargeboards, a two-light window, studs, a collar and a king post. It sits on a slate hip roof (eaves 5.5 m, ridge 8.05 m).
- On the right front corner is an octagonal oriel turret on a timber corbel, with leaded lights to the front and side, a slated octagonal spire and an iron finial.
- The left stack is squared stone with two cap courses and three clay pots. Also modelled: a forged wall lantern, an iron bracket with a blue banner (gilt trim, pale stag on both faces), and downpipes at the left jetty corner and the right front corner.
- **Inferred (not in the concept):** the sides and rear; one upper window on each side; one upper and one ground window at the back; the depth position of the stack and turret; the turret's side window.

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. No new scans were downloaded.

| Surface | Material | Scan |
|---|---|---|
| Ground storey, door surround, steps, stack | `SB_Townhouse4_Stone` | `sandstone_blocks_05` (pale rock-faced squared blocks); quoins, voussoirs and caps sample single blocks |
| Slates (main roof, gable, pent, spire) | `SB_Slate` | `grey_roof_tiles_02`, with `lichen_rock` lichen |
| Plaster infill | `SB_LimePlaster` | `patterned_clay_plaster` |
| Timber framing, brackets, bargeboards | `SB_OakTimber` | `weathered_brown_planks` |
| Door | `SB_DoorBoards` | `old_planks_02` |
| Banner | `SB_Townhouse4_Banner` | `denim_fabric_03`, darkened to navy |

Iron, lead came, lit glass, lantern glow, soil and chimney pots reuse Cottage1's parametric materials. Banner trim (`SB_Townhouse4_Gilt`) and the stag (`SB_Townhouse4_Emblem`) are plain parametric materials.

## Limits

- **Unreal:** not imported yet. Use `../import_silverbrook.py` to import it as `SM_SilverbrookTownhouse4`. There is no interior, and the door is closed.
- **Triangles:** 55,484, within the 120k house budget. The roof slates are about 37k of them.
- **Overlapping geometry:** individual slates overlap where the gable, the turret spire and the main roof meet. Hidden slates are culled, but the meshes are not watertight.
- **Simplified:** the carved fleur-de-lis on the stack is a plain raised panel. The X carving on the jetty brackets and the bolt heads on the bressumer are not modelled. The turret corbel is a solid timber frustum, not separate brackets.
- **Shader round-trip and GLB textures:** same as Cottage1. The exports carry UVs, `UVMap`/`WorldUV`, material slots and `Weathering` vertex colours. The GLB has no embedded images, and the FBX references `../Textures` relatively; the review asserts that they resolve.
- **Not modelled:** set dressing (window-box flowers, potted shrubs, barrel and crates).
