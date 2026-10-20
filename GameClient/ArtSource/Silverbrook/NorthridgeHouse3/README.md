# Silverbrook Northridge House 3 — concept prototype

Reference: `NorthridgeHouse3-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Northridge House-3.png`; SHA-256 in `asset-manifest.json`). Asset `BLD-GreenlandsNorthridgeHouse-01`, variant 3. Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_northridgehouse3.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookNorthridgeHouse3.blend -P review_northridgehouse3.py
```

The builder writes `SilverbrookNorthridgeHouse3.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `NorthridgeHouse3-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The concept is drawn 14 × 12 m; it is fitted to the 12 × 10 × 10 m slot (`SLOT`; the build asserts it). The whole model, including eaves, steps, downpipes and finials, is 11.40 × 8.95 × 9.92 m. The walls are 10.6 × 6.0 m.
- The main roof is a hipped slate roof: 5.0 m eaves, 8.2 m ridge.
- A cross wing runs the full depth under a steeper gable, with its ridge at 9.0 m. The front is half-timbered over ashlar infill: a tie beam, king post and struts, crossed bargeboards, and a forged fleur finial. It frames a four-light Gothic window with dressed mullions, two sub-arches and a quatrefoil oculus over a window box.
- Below it is a stone porch with a pointed archivolt under a coped gable. It has two buttresses with pinnacles, navy banners on gilt rods, and two bracket lanterns. Three dressed steps lead to a pointed, ledged double door with iron straps and ring pulls.
- The round corner tower has two leaded lancets, a moulded corbel ring with timber brackets, and a 16-facet slate cone with a finial. It rises through a half-polygonal bay window: a stone base, oak-framed pointed leaded lights, and a hipped slate roof with a finial.
- The right wing is jettied on a bressumer and brackets. It has two lancets with a window box, an oak-framed twin light with a window box, and its own half-timbered gable and gable light.
- There is a gabled dormer on the left slope. Two ashlar stacks carry clay pots, and five cast-iron downpipes have hopper heads.
- **Inferred (not in the concept):** the sides and rear (four lancets on each side, six on the rear, a rear gable on the cross wing with a small lancet), the depth of the roofs, the tower's left-side lancet, and the positions of the stacks in depth. The concept's main eave is higher on the left (about 5.4 m) than on the right (about 4.6 m); here it is a single 5.0 m eave.

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. `../silverbrook_kit.py` owns the mapping. No new scans were downloaded.

| Surface | Material | Scan |
|---|---|---|
| Slates, ridge and hip caps | `SB_Slate` | `grey_roof_tiles_02` (one slate interior per modelled slate) + `lichen_rock` |
| Walls, tower, gable infill, stacks | `SB_NorthridgeHouse3_Ashlar` | `sandstone_blocks_05` (pale coursed, rock-faced squared blocks) |
| Mullions, tracery, hoods, sills, porch, pinnacles, steps | `SB_NorthridgeHouse3_Dressed` | `sandstone_blocks_08`, sampled one stone per piece |
| Timber framing, bargeboards, bay frame, window boxes | `SB_OakTimber` | `weathered_brown_planks` |
| Door | `SB_DoorBoards` | `old_planks_02` |
| Banners | `SB_NorthridgeHouse3_Banner` | `denim_fabric_04`, navy |

Iron, lead came, lit glass, lantern glow, soil, chimney pots and gilt banner trim (`SB_NorthridgeHouse3_Gilt`) are plain parametric materials. Damp at the base and soot on the stacks use the exported `Weathering` vertex colour.

## Limits

- **Triangles:** 108,756, within the 120k house budget. The roof accounts for about 69k (individual slates, using the larger slate size the Town Hall uses).
- **Overlapping geometry:** slates overlap where the cross-wing, right-gable, dormer and bay roofs meet the main roof and where the tower passes through. Hidden slates are culled per slate, but the meshes are not watertight.
- **Simplified:** the banners' stag device is a gilt roundel and fleur, not a stag. The porch hood is a plain coped gable without crockets, and the tower's arrow slit is omitted.
- **Shader round-trip:** this is the same as Cottage 1. The exports carry the UVs, the `UVMap`/`WorldUV` layers, material slots and `Weathering` colours; in Unreal, `M_SB_Scan` rebuilds the materials from the `asset-manifest.json` table. The GLB has no embedded images. The FBX references `../Textures` relatively, and the review asserts that those references resolve.
- **Not modelled:** the concept's set dressing (flowers, shrubs, urns and pots) belongs to settlement dressing. There is no interior, and the doors are closed.
