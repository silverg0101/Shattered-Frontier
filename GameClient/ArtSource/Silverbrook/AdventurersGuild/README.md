# Silverbrook Adventurers' Guild — concept prototype

Reference: `AdventurersGuild-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Adventurers Guild.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry interpreting that elevation, not image reconstruction. Town asset `BLD-GreenlandsAdventurersGuild-01`.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_adventurersguild.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookAdventurersGuild.blend -P review_adventurersguild.py
```

The builder writes `SilverbrookAdventurersGuild.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `AdventurersGuild-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The elevation is read at 59.4 px/m, so the cross finial lands under 18 m. The concept's 16 m and 18 m dimension lines disagree with each other at that scale. The whole model, including eaves, sign, lean-to and finial, is 15.61 × 12.07 × 17.68 m, which fits the slot of 16 × 13 × 18 m (`SLOT`; the build asserts it). The origin is the slot's ground centre, so the door sits 0.4 m left of the origin.
- The main body has 12.6 × 9.6 m walls: an ashlar ground storey to 4 m with an oak-framed lime-plaster band above it (chevron braces, eave brackets). It carries a steep slate hip roof with 5.3 m eaves and an 11.4 m ridge, with finials at the ridge ends.
- A central cross wing projects 0.8 m and rises to a 14.5 m half-timbered gable. On the ground floor, a round-headed oak double door with iron straps and two ring handles sits in a dressed-stone surround with voussoirs. It is reached by two stone steps with cheek blocks; the step top is the door sill. Above it are a bressumer with gilded compass bosses, two tall leaded lights, and the blue guild banner hanging from an iron rod. The banner carries a gilded compass star.
- On the wing ridge stands a louvred oak belfry with an octagonal slate spire and an iron cross.
- Two gabled dormers with finials sit on the front slope. A stone stack with two pots stands on the right hip. There are three wall lanterns at the door and one under the lean-to.
- Ground-floor windows are leaded casements with brown board shutters under dressed voussoir hoods. The two front windows have planter boxes.
- **Inferred (not in the concept):** the plan depth, the wing running the full depth with a rear gable, the sides and rear (2 windows per side, 4 at the back, 1 in the rear gable), the belfry and stack depth positions, and the lean-to's depth. The lean-to's notice board and parchment notices are also inferred.
- **Simplified:** the concept's ground windows have arched glazing. Here they are rectangular casements under an arched stone hood with a solid tympanum. The concept's bell-cast eave flare is a straight pitch. The two small lights behind the banner are omitted.

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. `../silverbrook_kit.py` owns the mapping. `SB_*` rows reuse Cottage1's exact arguments.

| Surface | Material | Scan |
|---|---|---|
| Slates, belfry spire | `SB_Slate` | `grey_roof_tiles_02`, one slate interior per modelled slate; `lichen_rock` lichen |
| Ashlar, steps, stack, voussoirs | `SB_Ashlar` | `stone_wall_04` |
| Plaster infill | `SB_LimePlaster` | `patterned_clay_plaster` |
| Timber framing, belfry, lean-to | `SB_OakTimber` | `weathered_brown_planks` |
| Door, shutters, sign, notice board | `SB_DoorBoards` | `old_planks_02` |
| Guild banner | `SB_AdventurersGuild_Banner` | `denim_fabric_03`, aged to navy |

Iron, lead came, lit glass, lantern glow, soil and chimney pots are plain parametric materials shared with Cottage1. So are `SB_AdventurersGuild_Gilt` (compass stars) and `SB_AdventurersGuild_Parchment` (notices).

## Limits

- **Unreal:** this package is not imported yet; `../import_silverbrook.py` is the path. The intended mesh is `SM_SilverbrookAdventurersGuild`. There is no interior; the doors are closed.
- **Triangles:** 196k against the landmark budget of 400k. Slates are about 155k of that.
- **Overlapping geometry:** individual slates overlap where the wing, dormer and main roofs meet. The main ridge runs into the wing slope. Hidden slates are culled per slate, but the meshes are not watertight.
- **Kit workarounds (local to this build):**
  - `kit.wall(arch=...)` builds inside-out door spandrels: its prism takes its winding from points on the concave arc. Cottage1's voussoirs hide this; this build makes its own spandrels.
  - `Geo.box`'s float32 volume check misfires on 9 mm lead cames this far from the origin. A failing box is re-checked at the origin before it is rejected.
- **Shader round-trip:** as for Cottage1. The exports carry UVs, `UVMap`/`WorldUV` and the `Weathering` vertex colour. The GLB has no embedded images; the FBX references `../Textures` relatively, and the review asserts that those references resolve.
- **Not modelled:** set dressing (shrubs, potted trees, barrels, benches, window-box flowers).
