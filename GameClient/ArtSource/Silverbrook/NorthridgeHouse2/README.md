# Silverbrook Northridge House 2 — concept prototype

Reference: `NorthridgeHouse2-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Northridge House-2.png`; SHA-256 in `asset-manifest.json`). Asset `BLD-GreenlandsNorthridgeHouse-01`, variant 2. Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_northridgehouse2.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookNorthridgeHouse2.blend -P review_northridgehouse2.py
```

The builder writes `SilverbrookNorthridgeHouse2.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `NorthridgeHouse2-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The concept is drawn 14 m wide and about 14 m to the spire tips. It is scaled uniformly by about 0.68 to fit the town slot of 12 × 10 × 10 m. The whole model is 10.86 × 8.17 × 9.53 m (`SLOT`; the build asserts it). The main block is 9.2 × 6.0 m, with a hipped slate roof: 5.1 m eaves and a 7.9 m ridge.
- Two round corner towers (ten-sided drums) have a battered plinth, a string course and a corbel table. Their slate cones rise to 8.75 m, with iron finials to 9.5 m. Each tower has two leaded lancets on the front and two on the front-outer face.
- The central gable is half-timbered above a stone lower part. It sits over a tall pointed window with a mullion and a quatrefoil head, flanked by two small lancets. The two smaller timber gables have paired lancets and stand over paired lancets with window boxes. All three gables have bargeboards with pendants and iron fleur-de-lis finials.
- Two canted timber bay windows stand on stone bases under hipped slate roofs, with diamond-leaded lights and a planter on each.
- The stone porch has a coped gable front with a carved quatrefoil. Its pointed two-order arch is reached by three steps. Inside, a pointed double plank door with iron straps and ring handles sits in a two-order dressed surround. There are two bracket lanterns and one hanging lantern, plus two stag banners on iron brackets.
- **Inferred (not in the concept):** the house depth, the sides and rear (pointed lancets on two floors), placing the end stacks outside the side walls, and the downpipe positions.

## Materials

All textured surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. No new scans were downloaded.

| Surface | Material | Scan |
|---|---|---|
| Walls, towers, stacks, steps | `SB_NorthridgeHouse2_Ashlar` | `sandstone_blocks_05` (pale rock-faced squared blocks) |
| Window and door surrounds, sills, tower bands, coping | `SB_NorthridgeHouse2_Dressed` | `sandstone_blocks_08`, one dressed stone per piece |
| Slates (main roof, gables, bays, porch, cones) | `SB_Slate` | `grey_roof_tiles_02` + `lichen_rock` |
| Gable infill | `SB_LimePlaster` | `patterned_clay_plaster` |
| Timber framing, bay frames, window boxes | `SB_OakTimber` | `weathered_brown_planks` |
| Door | `SB_DoorBoards` | `old_planks_02` |
| Banners | `SB_NorthridgeHouse2_Banner` | `denim_fabric_03` |

The following are plain parametric materials: iron, lead came, lit glass, lantern glow, soil, chimney pots, the gilt banner border (`SB_NorthridgeHouse2_Gilt`) and the stag emblem (`SB_NorthridgeHouse2_Emblem`).

## Limits

- **Triangles:** 111,432, within the 120k house budget. The main-roof and gable slates are laid larger (0.38 × 0.52 m) than Cottage1's to stay in budget.
- **Not modelled:** the fish-scale slate pattern on the tower cones (laid as plain slates); the small lucarnes in the cones; the slim pent roofs beside the towers; the carved tracery inside the lancet heads; and the set dressing (flowers, shrubs, topiary, planters by the steps).
- **Simplified:** the bay windows use square-headed leaded lights, where the concept has pointed ones.
- **Overlapping geometry:** individual slates overlap where the gable and tower roofs meet the main roof. Slates fully inside another volume are culled, but the meshes are not watertight. The side-gable bargeboards run into the towers.
- **Exports:** the shader graph does not round-trip. The exports carry UVs, the `UVMap`/`WorldUV` layers, material slots and the `Weathering` vertex colours (see the Cottage1 README). The GLB is exported without images; the FBX references the textures relatively, and the review checks that they resolve.
- **Not imported into Unreal** by this package build: there is no `ue-import-report.json` yet. There is no interior, and the doors are closed.
