# Silverbrook Townhouse 1 — concept prototype

Reference: `Townhouse1-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Townhouse-1.png`; SHA-256 in `asset-manifest.json`). Variant 1 of `BLD-GreenlandsTownhouse-01`. Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_townhouse1.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookTownhouse1.blend -P review_townhouse1.py
```

The builder writes `SilverbrookTownhouse1.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `Townhouse1-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The walls are 5.7 × 5.8 m. The whole model, including the bracket banner, eaves, gutters, finial and chimney pots, is 7.08 × 7.0 × 8.97 m, which fits the 8 × 7 × 9 m slot (`SLOT`; the build asserts it). About 71k triangles.
- The concept is drawn 10 m tall; the storeys are compressed to fit 9 m (ground storey 3.3 m, upper storey 2.6 m, both ridges at 8.3 m). The width (6 m walls) is kept.
- The ground storey is pale squared stone. A round-headed plank door with iron straps and a ring sits in a quoin-and-voussoir surround, up two stone steps, with a bracket lantern beside it. To the right is a canted oak bay: stone plinth, carved panels with lancet tracery, leaded lights on all three faces, and a hipped slate roof under the jetty.
- The upper storey is jettied 0.4 m on a bressumer with carved corbels. It is half-timbered (posts, rails, braces) with lime plaster. A three-light window with a planter box sits under the gable, and a single light to its right.
- A steep front cross-gable, offset left as drawn, carries a two-light window, studs, collar, king post, crossed bargeboards and an iron-tipped finial.
- On the left wall a forged bracket holds a navy banner with a gilded stag and border on both faces. Gutters and downpipes run on both sides.
- **Inferred (not in the concept):**
  - The roof behind the elevation: the cross wing runs the full depth with a plain rear gable; right of it are two parallel hipped slate ranges (double pile) so the slopes stay as steep as drawn in a 7 m deep slot.
  - The dormer sits on the front range; the stack (two clay pots) is on the rear ridge at the right hip.
  - The sides and rear, their windows, and the rear and side framing.
  - The door head is round, not the concept's slightly pointed arch.

## Materials

Every scanned surface uses a CC0 Poly Haven scan at its measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`.

| Surface | Material | Scan |
|---|---|---|
| Ground storey, steps, bay plinth, stack, door surround | `SB_Townhouse1_Ashlar` | `sandstone_blocks_05` (pale, rough-faced squared blocks); quoins, voussoirs and stack caps sample single stones |
| Slates, bay roof, ridge and hip caps | `SB_Slate` | `grey_roof_tiles_02` + `lichen_rock` |
| Infill panels | `SB_LimePlaster` | `patterned_clay_plaster` |
| Framing, corbels, bargeboards, window frames, roof boarding | `SB_OakTimber` | `weathered_brown_planks` |
| Door, bay panels | `SB_DoorBoards` | `old_planks_02` |
| Banner | `SB_Townhouse1_Banner` | `denim_fabric_03`, darkened to navy |

Iron (gutters, pipes, bracket, straps), lead came, lit glass, lantern glow, soil and chimney pots use Cottage 1's plain parametric materials with the same names and arguments. The stag and banner trim are `SB_Townhouse1_Gilt` (plain parametric). No new scans were downloaded.

## Limits

- **Unreal:** not imported yet. `../import_silverbrook.py` would bring it in as `SM_SilverbrookTownhouse1`. There is no interior, and the door is closed.
- **Scale:** the sandstone scan's blocks (about 0.6 × 0.3 m) are larger than the concept's; the scan is used at its physical size.
- **Overlapping geometry:** individual slates overlap where the ranges, dormer and stack meet the wing roof and in the central valley. Hidden slates are culled one by one, but the meshes are not watertight.
- **Shader round-trip and GLB textures:** the same as Cottage 1. The exports carry UVs, `UVMap`/`WorldUV`, the material slots and `Weathering`. The GLB has no images; the FBX references `../Textures` relatively, and the review asserts that those references resolve.
- **Not modelled:** the concept's set dressing (window-box flowers, barrel, potted plants, crate) belongs to settlement dressing; the wall plaque by the door is omitted.
