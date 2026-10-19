# Silverbrook Northridge House 4 — concept prototype

Reference: `NorthridgeHouse4-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Northridge House-4.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry interpreting that elevation, not image reconstruction. Town asset `BLD-GreenlandsNorthridgeHouse-01`, variant 4.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_northridgehouse4.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookNorthridgeHouse4.blend -P review_northridgehouse4.py
```

The builder writes `SilverbrookNorthridgeHouse4.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `NorthridgeHouse4-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- **Scaled to the slot.** The concept is 14 m wide and about 14 m to the finials. Widths are ×0.8 and heights about ×0.7. The ground storey keeps a usable height: 4.2 m to the first floor, and a 1.5 m double door whose sill is 0.75 m up a flight of steps. The whole model, including bargeboards, gutters, steps and finial spikes, is 11.9 × 8.67 × 9.95 m. It fits the 12 × 10 × 10 m slot (`SLOT`; the build asserts it).
- **Plan.** The house is an H-plan in pale rough-faced ashlar. Two cross wings (4.1 m wide, 6.6 m deep) carry very steep slate gable roofs (51°, ridge 9.1 m). Between them is a centre block, recessed 0.6 m, with a lower slate roof (ridge 8.4 m). That roof runs into the wing roofs and has a small stone dormer with a pointed light and an iron spike.
- **Front gables.** Each has a heavy timber frame laid over the stonework: a jettied bressumer on corbels, corner posts, a split tie beam, a king post, a collar, struts and knee braces. It also has thick bargeboards with blocky feet and a timber finial with an iron fleur-de-lis spike. In the gable is a four-light traceried lancet (sub-arches and oculus) with a timber window box. It is flanked by two single lancets.
- **Ground floor of the wings.** Each wing has a canted bay window: a stone base with a window box, five pointed timber-framed lights, and a hipped slate roof with hip caps.
- **Centre.** A deep pointed-arch stone porch with a moulded hood leads to a pointed plank double door with iron straps and ring handles. On top of the porch is a stone balcony on corbels, with a colonnette balustrade and newels. Behind it is a four-light traceried lancet.
- **Steps.** A five-riser flight runs between pedestals with stone urns and cheek walls.
- **Fittings.** There are two lanterns on the porch face. Navy banners with gilt edging and stags hang on iron brackets beside the porch and from the balustrade. Iron gutters and downpipes run beside the porch and at the wing sides.
- **Stacks.** Four stone stacks carry clay pots: the outer pair on the outer wing slopes, the inner pair at the roof junctions.
- **Inferred (not in the concept):** the H-plan depth and the rear gables, the sides and rear with their lancets and timber, the corner buttresses' depth, and the stack positions in depth.

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. `../silverbrook_kit.py` owns the mapping. No new scans were downloaded.

| Material | Scan | Use |
|---|---|---|
| `SB_NorthridgeHouse4_Ashlar` | `sandstone_blocks_05` | Walls, porch, balcony, steps, stacks. Mullions, tracery, hoods and sills sample single blocks. |
| `SB_NorthridgeHouse4_Slate` | `grey_roof_tiles_02`, darkened toward navy, sparse `lichen_rock` | Slates, caps, dormer slabs |
| `SB_OakTimber` | `weathered_brown_planks` (Cottage1 arguments) | Gable framing, bargeboards, bay frames, roof decking, window boxes |
| `SB_DoorBoards` | `old_planks_02` (Cottage1 arguments) | Double door |
| `SB_NorthridgeHouse4_Banner` | `denim_fabric_03`, darkened navy | Banners |

`SB_ForgedIron`, `SB_LeadCame`, `SB_LitGlass`, `SB_LanternGlow`, `SB_PlanterSoil` and `SB_ChimneyPot` are plain parametric materials with Cottage1's arguments. `SB_NorthridgeHouse4_Gilt` (banner edging, stags) is parametric too. Damp at the base uses the exported `Weathering` vertex colour.

## Limits

- **Size:** 104k triangles, within the 120k house budget. Slates are 60k of that, and lead cames are unbevelled to save triangles.
- **Unreal:** not imported here. `../import_silverbrook.py` is expected to produce `SM_SilverbrookNorthridgeHouse4`. There is no interior; the door is closed.
- **Overlapping geometry:** slates overlap at the valleys where the centre roof meets the wings. Slates hidden under the other roof are culled per slate, but the meshes are not watertight.
- **Simplified:** the bay lights are rectangular frames with pointed glazing heads. The porch has a single moulded order rather than the concept's tympanum quatrefoil. The balustrade is a plain colonnette arcade, and the fleur-de-lis spikes are schematic.
- **Shader round-trip and GLB:** as in Cottage1. The exports carry UVs, `UVMap`/`WorldUV` and `Weathering`. The GLB has no embedded images, and the FBX references `../Textures` relatively; the review asserts that those references resolve.
- **Not modelled:** set dressing. That covers the shrubs, topiary, flowers in the window boxes and urns, and the ivy.
