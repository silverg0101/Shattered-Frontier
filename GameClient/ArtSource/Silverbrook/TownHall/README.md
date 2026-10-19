# Silverbrook Town Hall — concept prototype

Reference: `TownHall-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Town Hall.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry interpreting that elevation, not image reconstruction. Package for `BLD-GreenlandsTownHall-01`.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_townhall.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookTownHall.blend -P review_townhall.py
```

The builder writes `SilverbrookTownHall.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `TownHall-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The whole model, including eaves, steps, lanterns and finials, is 22.0 × 14.31 × 27.95 m. It fits the 22 × 18 × 28 m slot (`SLOT`; the build asserts it). Measured from the elevation at about 40 px/m.
- The main hall is 12 × 10 m with 8 m walls. Its hipped slate roof (ridge 15 m) carries two stone wall dormers with timber bargeboards.
- The entrance bay projects 2.5 m. It has a 15.6 m stone gable with raking coping, a gilt apex finial and a small roundel. Corner buttresses rise to slate pinnacles. In the bay are a four-light traceried window (sub-arches and an oculus) and a pointed portal: three archivolt orders, a glazed tympanum over a stone transom, and iron-strapped plank double doors. The doors start at the top of a three-step landing. Stone pedestals with lanterns stand at the foot of the steps, and bracket lanterns hang on the buttresses.
- The central clock and bell tower is 4 m square, with clasping buttresses and a gilt-starred clock roundel. In the belfry, a bronze bell hangs from an oak beam behind four pointed openings. Above are four corner pinnacles and a slate spire to 26.65 m, with a gilt finial reaching 27.95 m.
- Two corner towers (2.4 m square) carry broad pyramidal slate spires to 14.4 m. Two roof turrets rise from the main roof, with spires to 18.3 m. Both have small decorative lancets.
- The lower hipped wings have 5.5 m walls, half-timbered plaster gablets, oak corner posts and wall plates, and gilt ridge finials.
- On the main front are traceried lancets with stone balconettes and iron railings. All windows are leaded lit glass with moulded hoods and sills. Three navy banners have gilt edging and rods, on the gable and the corner towers.
- **Inferred (not in the concept):** the plan depth (14.3 m, which leaves 3.7 m of the slot free), the sides and rear, the rear and wing windows, the turret footprints, the tower's depth position, and the belfry interior.

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. `../silverbrook_kit.py` owns the mapping.

| Surface | Material | Scan |
|---|---|---|
| Ashlar walls, towers, tracery, archivolts, steps | `SB_TownHall_Ashlar` | `sandstone_blocks_08` (new); dressed parts sample single blocks |
| Slates, small spire caps, dormer roofs | `SB_Slate` | `grey_roof_tiles_02`, with `lichen_rock` lichen |
| Gablet infill | `SB_LimePlaster` | `patterned_clay_plaster` |
| Bargeboards, posts, bell beam, roof decking | `SB_OakTimber` | `weathered_brown_planks` |
| Doors | `SB_DoorBoards` | `old_planks_02` |
| Banners | `SB_TownHall_Banner` | `denim_fabric_04` (new), aged to navy |

Iron, lead came, lit glass and lantern glow reuse Cottage1's parametric materials. `SB_TownHall_Gilt` (finials, banner trim, clock star) and `SB_TownHall_Bronze` (bell) are plain parametric metals.

## Limits

- **Triangles:** there are 236k triangles, under the 400k landmark budget; the modelled slates account for 192k. The small pinnacle caps and dormer roofs are solid slabs that use the projected slate scan.
- **Not modelled:** the stag emblem on the banners (they are plain cloth), the crockets and small gablets on the spire and pinnacles, the corbel tables, and the moulded plinth. The concept's shrubs belong to settlement dressing. There is no interior, and the doors are closed.
- **Scale of the scan:** the ashlar scan is used at its measured size (blocks about 1 × 0.6 m), so the courses are larger than in the painted concept.
- **Overlapping geometry:** as in Cottage1, slates overlap where roofs meet (bay/main, dormers, gablets). Slates fully inside another volume are culled, but the meshes are not watertight.
- **Shader round-trip and GLB textures:** same as Cottage1. The exports carry UVs, `UVMap`/`WorldUV`, material slots and the `Weathering` vertex colour. The GLB has no embedded images, and the FBX references `../Textures` relatively; the review asserts that they resolve.
