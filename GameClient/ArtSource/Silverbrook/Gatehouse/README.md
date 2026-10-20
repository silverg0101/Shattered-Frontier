# Silverbrook Gatehouse — town-wall gate

Reference: `Gatehouse-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Gatehouse.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry interpreting that elevation, not image reconstruction. Asset `BLD-GreenlandsGatehouse-01`.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_gatehouse.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookGatehouse.blend -P review_gatehouse.py
```

The builder writes `SilverbrookGatehouse.{blend,fbx,glb}` and `asset-manifest.json`. The review renders `Gatehouse-{Beauty,Front,Side,Rear,Passage,Joint}.png`. The preview places a WallSection segment against each tower, so the renders show the joint. It then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1. Metres, Z up, front (field side) faces -Y, origin at ground centre.

## Interpretation

- **Slot:** `wall.gate_gap` (10 m) plus two `wall.tower` (8 × 8 × 12 m) from `../../Greenlands/silverbrook-town-layout.json` gives `slot_m` 26 × 8 × 12. The gate block fills the 10 m gap between the towers (x ±5), and the towers stand at x ±9. The model is 26.0 × 8.0 × 11.98 m; the build asserts that it fits.
- **Height:** the concept is 14 m to the roofs, but here everything, finials included, is fitted under 12 m. The tower shafts rise to 7.6 m, the machicolated heads have merlons to 9.4 m, and the slate pyramids peak at 11.3 m with an iron finial. The gate block's parapet tops out at 8.0 m, level with the wall turrets.
- **Gate passage:** a 5 m round-arched barrel vault, 6 m deep, with its spring at 3.0 m and its crown at 5.5 m. There are dressed jamb quoins and voussoir rings on both faces. The concept's arch is narrower; it is widened here for traffic. The two plank gates are shown **open**, swung back flat against the passage walls on iron pintles. The iron portcullis is **raised** in its groove, with its spike tips at 4.0 m. The build asserts that a clear way 4.4 m wide and 3.9 m high runs through the full depth.
- **Wall joint:** the towers' outer faces are flat at x ±13 m, so a WallSection end face butts against them. The two pieces share their y origin, so the curtain (−0.70), parapet (−0.95) and walk (−0.45…1.15) line up. Each outer face has a closed door whose sill sits at the wall-walk height, z 5.40 (asserted). Its flush dressed surround stays inside the walk's y span, so nothing intrudes into the wall.
- Following the concept: corbel table, crenellated parapet with a slit in every merlon, and a carved stag shield over the keystone. Twin wall lanterns flank the arch. Each tower has stepped corner buttresses, a plinth, tall slits, and a navy stag-and-fleur banner on an iron rod. Each tower head is crenellated and corbelled out to the full 8 m depth, and carries a slate pyramid roof with a lit, half-timbered dormer.
- **Inferred (not in the concept):** all depths; the rear (town) face, with a plain parapet and a ground door into each tower; the wall-walk doors; the paved walks on the block and the tower heads; the plain inner tower faces; the leaf ledges and braces; slits that are 0.3 m recesses with a dark back.

## Materials

Poly Haven CC0 scans from `../Textures` (provenance in `../materials-provenance.json`). No new scans were downloaded.

| Material | Surface | Scan / args |
|---|---|---|
| `SB_WallSection_Masonry` | All masonry; quoins, voussoirs, corbels, copings and the shield face sample single stones | `rough_block_wall`, `dark=(.70,.74,.86), aged=.4`, UV repeat 20/6 m — identical to WallSection |
| `SB_WallSection_Paving` | Gate-block and tower-head walks | `medieval_blocks_03`, `dark=(.55,.58,.62), damp=False, aged=.25` — identical |
| `SB_WallSection_LoopVoid` | Dark back of the slits | `kit.solid((.012,.012,.014), .95)` — identical |
| `SB_Slate`, `SB_OakTimber`, `SB_LimePlaster`, `SB_DoorBoards` | Roof slates and deck, dormer frame and infill, gates and doors | Cottage1's exact arguments |
| `SB_Gatehouse_Banner` | Banners | `denim_fabric_03`, `dark=(.12,.14,.30), damp=False, aged=.75` (navy) |

The parametric materials are `SB_ForgedIron`, `SB_LeadCame`, `SB_LitGlass` and `SB_LanternGlow` (Cottage1's arguments), plus `SB_Gatehouse_Gilt` for the banner charges and trim.

## Limits

- **Not yet imported to Unreal.** The expected mesh is `SM_SilverbrookGatehouse`. Collision needs the passage left open, and flat hulls for the walks.
- The concept's closed gates and partly lowered portcullis are shown open and raised (static), so the gate can be walked through. There are no interiors, so the doors are closed.
- The portcullis and the overlapping slates run into hidden masonry and are not watertight. The casement is built at the origin and moved into place, because `kit.box`'s float32 winding check fails for 9 mm cames that far from the origin.
- **Shader round-trip:** same as Cottage1. The GLB carries no images, and the FBX references the textures relatively; the review checks that they resolve. 64.4k triangles, 43.5k of them in the slates.
