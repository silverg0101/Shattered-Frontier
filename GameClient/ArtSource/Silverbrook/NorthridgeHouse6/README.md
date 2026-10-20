# Silverbrook Northridge House 6 — concept prototype

Reference: `NorthridgeHouse6-reference.png` is a copy of `docs/Game-Design/concept-art/Silverbrook/Northridge House-6.png`; its SHA-256 is in `asset-manifest.json`. Asset ID `BLD-GreenlandsNorthridgeHouse-01`, variant 6. The model is original procedural geometry that interprets the elevation; it does not reconstruct the image.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_northridgehouse6.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookNorthridgeHouse6.blend -P review_northridgehouse6.py
```

The builder writes `SilverbrookNorthridgeHouse6.{blend,fbx,glb}` and `asset-manifest.json`. In a fresh process, the review script renders `NorthridgeHouse6-{Beauty,Front,Side,Rear}.png` from the saved .blend. It then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Built with Blender 5.1.1. Units are metres, Z is up, the front faces -Y and the origin is at ground centre.

## Interpretation

- **Size and scale:** The walls measure 9.7 × 5.2 m. The whole model, including the lean-to, porch, finials and chimney pots, is 10.93 × 7.35 × 9.63 m. That fits the 12 × 10 × 10 m slot (`SLOT`), and the build asserts it. The concept is dimensioned at 14 m wide, but it is about 16 m across its end roofs and about 14 m to the tower finial. It was scaled by about 0.7 to fit, while the door keeps 2.4 m of clear height.
- **Main roof:** a steep navy slate hip roof with 4.3 m eaves, a 7.4 m ridge and steep hip ends, as drawn. Iron finials stand at the ridge ends.
- **Tower:** an octagonal corner tower in coursed pale ashlar. It has a dressed plinth and leaded lancets; the upper lancet has a planter. A machicolated corbel table carries a crenellated parapet with a mitred coping. Four corbelled bartizans have slate cones. The main slate spire rises to an iron finial at 9.6 m. A navy stag banner hangs on the front face.
- **Central cross gable:** it rises to 8.1 m, above the main ridge, and has studded, crossed bargeboards and an iron finial. In the gable are two tall lancets flanked by two short ones over a sill course, and a quatrefoil roundel in a voussoir ring. Oak posts, braces, a collar and a king post are fixed over the stone. Corner piers carry pinnacles and bracket lanterns.
- **Porch and door:** a gabled slate porch stands on clustered stone colonnettes, with an open timber truss and a hanging lantern. It shelters a pointed, ledged double door with iron straps and ring handles. The door sits in a dressed hood ring above a stone landing and a step.
- **Right cross gable:** a canted oriel sits on carved oak brackets. It has four timber-framed lancets, a planter and a hipped slate roof. Below are three ground lancets. Above it are a gable lancet and oak framing.
- **Dormers:** two gabled stone dormers with lancets: a wall dormer between the tower and the central gable, and a smaller one higher on the slope.
- **Left lean-to:** a lean-to under a hipped slate pent roof.
- **Chimney stack:** a stack with a dressed cap and two copper pots, hung with a stag banner.
- **Right wall:** a second banner on the narrow right wall section.
- **Downpipe:** an iron downpipe beside the tower.
- **Inferred (not in the concept):**
  - The central gable runs the full depth to a rear gable with three lancets, because the drawn gable is taller than the main ridge.
  - The sides and rear, their lancets and the side timbering, the depths of the tower, stack and lean-to, and the position of the upper dormer on the slope.

## Materials

Every scanned surface uses a CC0 Poly Haven scan at its measured physical size. Sources and hashes are in `../materials-provenance.json`, and the maps are in `../Textures`. No new scans were downloaded.

| Surface | Material | Scan |
|---|---|---|
| Slates, spires, ridge caps | `SB_NorthridgeHouse6_Slate` | `grey_roof_tiles_02` darkened to navy. Each modelled slate samples one slate interior. No lichen, as drawn. |
| Walls, tower, stack, dormers, piers | `SB_NorthridgeHouse6_Ashlar` | `sandstone_blocks_05` (pale coursed blocks) |
| Window surrounds, sills, roundel, plinth, landing, colonnettes, coping | `SB_NorthridgeHouse6_Dressed` | `sandstone_blocks_08` (smooth dressed stone). Each part samples a single stone. |
| Bargeboards, framing, brackets, oriel frames, planters, roof boarding | `SB_OakTimber` | `weathered_brown_planks` |
| Door | `SB_DoorBoards` | `old_planks_02` |
| Banners | `SB_NorthridgeHouse6_Banner` | `denim_fabric_03` darkened to navy |

These are plain parametric materials:

- Forged iron (studs, finials, straps, rods, downpipe)
- Lead came
- Lit glass
- Lantern glow
- Planter soil
- Copper chimney pots (`SB_NorthridgeHouse6_Copper`)
- Banner gilt for the stag, fleur-de-lis and border (`SB_NorthridgeHouse6_Gilt`)

Damp at the base uses the exported `Weathering` vertex colour.

## Limits

- **Unreal:** not imported yet; `../import_silverbrook.py` owns the import. There is no interior, and the door is closed.
- **Simplified:**
  - The roundel's quatrefoil is four stone cusp arcs over lit glass, not carved tracery.
  - The porch's carved gable tracery is a plain king post and struts.
  - The drawn stone pinnacles near the ridge are iron ridge-end finials.
  - The concept's slightly drop-pointed door is an equilateral pointed arch.
- **Overlapping geometry:** individual slates overlap where the gable, dormer, porch, oriel and lean-to roofs meet other roofs or walls. Hidden slates are culled, but the meshes are not watertight. The build asserts that every cheek and side-wall top stays under the roof that covers it.
- **Shader round-trip:** the Blender shader graph does not round-trip to FBX/GLB. The exports carry UVs, the `UVMap`/`WorldUV` layers, material slots and the `Weathering` vertex colours. The review checks that the FBX texture references resolve. The GLB is exported without images.
- **Not modelled:** the concept's set dressing belongs to settlement dressing. That covers the window-box flowers, the potted topiaries and the hanging greenery on the tower.
