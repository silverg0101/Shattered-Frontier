# Silverbrook Northridge House 5 — concept prototype

Reference: `NorthridgeHouse5-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Northridge House-5.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry interpreting that elevation, not image reconstruction. Town asset `BLD-GreenlandsNorthridgeHouse-01`, variant 5.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_northridgehouse5.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookNorthridgeHouse5.blend -P review_northridgehouse5.py
```

The builder writes `SilverbrookNorthridgeHouse5.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `NorthridgeHouse5-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The walls are 10.8 × 5.4 m. The whole model, including eaves, tower parapet, steps and the spire finial, is 11.55 × 7.25 × 9.95 m, which fits the town slot of 12 × 10 × 10 m (`SLOT`; the build asserts it).
- **Heights are compressed.** The concept is 14 m wide and about 16 m to the spire finial. Widths keep roughly 90 px/m. The ground storey and door keep usable heights (3.05 m storey; a 1.5 m pointed double door springing 2.05 m above a 0.3 m step). The roofs, gables, tower and spire take the reduction. The spire is about 1.9 m tall, against roughly 3.5 m in the concept.
- The house is pale rock-faced ashlar. Two steep front gables share the 8 m main ridge. They are half-timbered over the stone, with tie beams, posts, braces, a collar, a king post with a pendant, iron-studded bargeboards and forged-iron finials.
- In the left gable: a slate-roofed three-sided bay window with five leaded lancets on the ground storey, and three upper lancets above it with window boxes, the centre one tall. In the centre gable: a two-light traceried window with an oculus, flanked by two lancets.
- The projecting stone portal has two archivolt orders, a carved quatrefoil in a steep gablet, and two pinnacle piers with bracket lanterns. Behind it is a plank double door with iron straps and ring handles, under a glazed tympanum on a stone transom, reached by two steps.
- The main slate roof is gable-ended on the left, with a stack and two clay pots, and hipped on the right. There are two small stone dormers: one in the valley between the gables and one beside the tower.
- The 2.7 m square corner tower projects 1.2 m. It has tall and short lancets, a corbel table carrying a crenellated parapet, a slate spire with an iron finial, and two pots on the rear merlons.
- Navy banners with a gilded stag hang from iron rods: three on the ground storey and a larger one on the tower. There are four iron downpipes.
- **Inferred (not in the concept):** the sides and rear, including three ground and three upper lancets at the back, three lancets in the left gable end and four on the right wall; the depth of the tower and of the stack.

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. `../silverbrook_kit.py` owns the mapping. No new scans were downloaded.

| Surface | Material | Scan |
|---|---|---|
| Slates, spire, ridge caps, bay roof, dormer roofs | `SB_NorthridgeHouse5_Slate` | `grey_roof_tiles_02`, darkened to blue-grey; one slate interior per modelled slate |
| Walls, tower, portal, tracery, hoods, sills, stack | `SB_NorthridgeHouse5_Ashlar` | `sandstone_blocks_05` (pale rock-faced blocks); dressed pieces sample single blocks |
| Half-timbering, bargeboards, rails, window boxes | `SB_OakTimber` | `weathered_brown_planks`, each beam confined to one plank's grain band |
| Door | `SB_DoorBoards` | `old_planks_02` |
| Banners | `SB_NorthridgeHouse5_Banner` | `denim_fabric_03`, darkened to navy |

Forged iron, lead came, lit glass, lantern glow, planter soil, chimney pots (`SB_ForgedIron`, `SB_LeadCame`, `SB_LitGlass`, `SB_LanternGlow`, `SB_PlanterSoil`, `SB_ChimneyPot`, same arguments as Cottage 1) and the gilded stags and banner edging (`SB_NorthridgeHouse5_Gilt`) are plain parametric materials. Damp at the base uses the exported `Weathering` vertex colour.

## Limits

- **Unreal:** not imported here; `../import_silverbrook.py` takes `SM_SilverbrookNorthridgeHouse5`. There is no interior, and the door is closed.
- **Simplified:** the portal's crockets and tympanum rose tracery, the fleur-de-lis under each stag, the spire's lucarne and the small lean-to at the left corner are not modelled. The tower's front pier is not modelled, and the small lancets beside the portal are left out for clearance.
- **Overlapping geometry:** individual slates (most of the 86k triangles) overlap where the gables, dormers, tower and stack meet the main roof. Hidden slates are culled per slate, but the meshes are not watertight.
- **Shader round-trip:** as for Cottage 1, the exports carry UVs, the `UVMap`/`WorldUV` layers, material slots and `Weathering` vertex colours; `M_SB_Scan` rebuilds the look from the material table in `asset-manifest.json`. The GLB carries no images; the FBX references `../Textures` relatively, and the review asserts that those references resolve.
- **Not modelled:** set dressing (flowers in the window boxes, potted conifers, shrubs).
