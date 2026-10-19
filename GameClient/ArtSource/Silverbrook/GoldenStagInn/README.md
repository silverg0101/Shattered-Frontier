# Silverbrook Golden Stag Inn — concept prototype

Reference: `GoldenStagInn-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/The Golden Stag Inn.png`; SHA-256 in `asset-manifest.json`). Asset ID `BLD-GreenlandsTheGoldenStagInn-01`. Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_goldenstaginn.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookGoldenStagInn.blend -P review_goldenstaginn.py
```

The builder writes `SilverbrookGoldenStagInn.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `GoldenStagInn-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The ground storey is 11.2 × 8 m. The whole model, including the bracket sign, side lean-to, eaves, finial and chimney pots, is 13.68 × 10.08 × 13.78 m, which fits the 14 × 12 × 14 m slot (`SLOT`; the build asserts it). The origin is the slot's ground centre.
- The main roof is a steep-hipped slate roof with 5.3 m eaves and an 11 m ridge, and timber finials at the ridge ends.
- A half-timbered cross wing, set 0.85 m left of centre as drawn, rises above the main ridge to 12.2 m. It has crossed bargeboards and an iron-tipped finial. Its face carries two leaded windows with planters, flanking a hanging stag banner (gilded stag and border), with a tie beam, collar, king post and struts above.
- The front upper storey is jettied 0.45 m on a bressumer with joist ends, carried by five braced oak posts on dressed-stone pads. Above it is a plaster band with studs.
- The ground storey is ashlar on a plinth. A round-headed plank door sits in a quoin-and-voussoir surround, up three stone steps, flanked by two bracket lanterns. Three leaded windows have planters, and a lantern hangs from the jetty.
- On the right slope is a dormer with a finial. A tall stone stack carries a two-course cap and two clay pots.
- On the left wall is a forged bracket with a scroll and a lantern. From it hangs a dark shield-shaped board with a gilded stag on both faces.
- **Inferred (not in the concept):**
  - The cross wing runs the full depth, with a rear gable and window, because the drawn wing is taller than the main ridge.
  - The right-hand lean-to is read as a slate side porch over an arched side door.
  - Also inferred: the sides and rear, their windows, and the depth position of the stack.

## Materials

Every scanned surface uses a CC0 Poly Haven scan at its measured physical size. Sources and hashes are in `../materials-provenance.json`.

| Surface | Material | Scan |
|---|---|---|
| Slates, ridge and hip caps | SB_Slate | `grey_roof_tiles_02` + `lichen_rock` |
| Ashlar, plinth, pads, steps, stack | SB_Ashlar | `stone_wall_04` |
| Infill panels | SB_LimePlaster | `patterned_clay_plaster` |
| Framing, bargeboards, roof boarding | SB_OakTimber | `weathered_brown_planks` |
| Doors | SB_DoorBoards | `old_planks_02` |
| Hanging sign board | SB_GoldenStagInn_SignBoard | `old_planks_02`, aged near-black |
| Banner | SB_GoldenStagInn_Banner | `rough_linen` (new scan for this package), aged dark blue |

Iron, lead came, lit glass, lantern glow, soil, chimney pots and gilding (`SB_GoldenStagInn_Gilt`) are plain parametric materials. Cottage1's names are reused with identical arguments.

## Limits

- **Triangles:** 168,828, under the 400k landmark budget. Roof slates are about 136k of these.
- **Overlapping geometry:** where the cross wing meets the main roof, slates overlap per slate and are culled only where fully covered. The meshes are not watertight.
- **Banner colour:** the banner is a mid-dark blue rather than the concept's navy. The kit's ageing caps darkening at 0.8, and the linen scan is pale blue.
- **Arched windows:** the ground windows are rectangular leaded casements, not the concept's arched heads.
- **Not modelled:** window-box flowers, shrubs, barrels, crates, ladder, bench and the cart (settlement dressing). The door is closed, and there is no interior.
- **Exports:** the GLB is exported without images, and the FBX references `../Textures` relatively. The review asserts that the FBX textures resolve.
