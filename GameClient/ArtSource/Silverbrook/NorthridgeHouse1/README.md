# Silverbrook Northridge House 1 — concept prototype

Reference: `NorthridgeHouse1-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Northridge House-1.png`; SHA-256 in `asset-manifest.json`). Variant 1 of `BLD-GreenlandsNorthridgeHouse-01`. Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_northridgehouse1.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookNorthridgeHouse1.blend -P review_northridgehouse1.py
```

The builder writes `SilverbrookNorthridgeHouse1.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `NorthridgeHouse1-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The walls are 10.6 × 5.2 m. The whole model, including eaves, porch steps, gutters, finials, spire and chimney pots, is 11.49 × 7.83 × 9.95 m, which fits the 12 × 10 × 10 m slot (`SLOT`; the build asserts it). About 102k triangles.
- The concept is drawn 14 m wide and about 12 m to the spire tip. The width is read at ~85 px/m. The storeys keep that scale (ground storey 3.2–3.5 m); the roofs and spire are compressed so the spire finial stops at 9.95 m. Every eave is at 5.65 m and every ridge at 8.5 m.
- **Left cross wing:** a pale squared-stone ground storey with a canted oak bay (stone base, leaded lights, hipped slate roof) under a jetty. The jetty is a bressumer on corbels. The upper storey is stone in an oak frame, with a three-light leaded window, a planter box and two flanking lancets. A half-timbered plaster gable carries a lancet, collar, king post, crossed bargeboards and an iron-tipped finial.
- **Centre:** a stone gable in an oak frame with a three-light pointed window, two flanking lancets and a planter. In front is a gabled stone porch with a dressed coping, a cross finial and a quatrefoil. It shelters a pointed double plank door with iron straps and ring handles in two dressed orders, up three steps, flanked by two bracket lanterns and two navy stag banners.
- **Tower:** a round stone tower, 2 m across, with a plinth, string courses and a corbel table of oak brackets. It carries a coursed conical slate spire with a gabled spire dormer and an iron finial. Its pointed windows are on the front: a two-light at the bottom and single lights above.
- **Right wing:** under a steep hip, with a timber-framed dormer, a lancet, oak posts and rails, and a large navy banner with a gilt stag.
- There are two stone stacks with clay pots, plus gutters and downpipes.
- **Inferred (not in the concept):**
  - The roof plan: the left wing runs the full depth with a rear gable. The centre gable runs back to the main ridge. The right end is hipped.
  - The tower is half-engaged in the front wall.
  - The stack positions in depth.
  - The sides and rear, and their windows.
- **Simplified:** the centre window has three lights, without the concept's quatrefoil head tracery. The bay window lights are square-headed, not pointed. The concept's buttress at the left corner is not modelled.

## Materials

Every scanned surface uses a CC0 Poly Haven scan at its measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. No new scans were needed.

| Surface | Material | Scan |
|---|---|---|
| Slates, spire, ridge caps | `SB_Slate` | `grey_roof_tiles_02` (+ `lichen_rock`) |
| Walls, tower, stacks, steps | `SB_NorthridgeHouse1_Ashlar` | `sandstone_blocks_05` |
| Hoods, sills, mullions, door orders, coping, plinth, string courses | `SB_NorthridgeHouse1_Dressed` | `sandstone_blocks_08`, single-stone patches |
| Gable and dormer infill | `SB_LimePlaster` | `patterned_clay_plaster` |
| Framing, bargeboards, corbels, roof boarding | `SB_OakTimber` | `weathered_brown_planks`, one plank band per beam |
| Door | `SB_DoorBoards` | `old_planks_02` |
| Banners | `SB_NorthridgeHouse1_Banner` | `denim_fabric_03`, darkened navy |

Iron, lead came, lit glass, lantern glow, planter soil, chimney pots and the gilt stag and banner trim (`SB_NorthridgeHouse1_Gilt`) are plain parametric materials. Damp at the base and slate tone use the exported `Weathering` vertex colour.

## Limits

- **Overlapping geometry:** slates overlap where the gable, dormer and spire meet the main roof. Hidden slates are culled, but the meshes are not watertight. The spire is a coursed surface of revolution, not individual slates. The tower windows sit on flat stone panels proud of the round wall; they are not cut into it.
- **Shader round-trip:** as for Cottage 1, the shader graph does not round-trip to FBX/GLB. The exports carry UVs, `UVMap`/`WorldUV`, material slots and `Weathering`. The review checks that the FBX texture references resolve. The GLB is exported without images.
- **Not imported to Unreal yet.** There is no interior, and the door is closed. Set dressing (flowers, shrubs, tubs) is out of scope.
