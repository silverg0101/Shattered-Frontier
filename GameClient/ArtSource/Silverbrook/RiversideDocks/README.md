# Silverbrook Riverside Docks — concept prototype

Reference: `RiversideDocks-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Riverside Docks.png`; SHA-256 in `asset-manifest.json`). Asset `BLD-GreenlandsRiversideDocks-01`. Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_riversidedocks.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookRiversideDocks.blend -P review_riversidedocks.py
```

The builder writes `SilverbrookRiversideDocks.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `RiversideDocks-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used.

## Orientation and levels

- **Front (-Y) is the water side**, as the concept is drawn. The quay runs along X. Z is up, units are metres.
- **The origin is the slot centre at riverbank ground level.** `silverbrook-town-layout.json` gives the docks a 30 × 10 × 1 m slot on the Riverside Harbour basin. `heightfield.py` puts the bank 1.5 m above the water. So z=0 is the bank, the water is at z=-1.5, and the piles and quay footings stand on a bed at z=-3.5.
- **The deck is at z=+0.5: 0.5 m above the bank and 2.0 m above the water.** The 2.0 m matches the concept's deck height above the water, measured on its 6 m scale.
- **The walking surface is continuous at z=0.5.** It is made of three parts that meet edge to edge with no step:
  - the timber wharf: boards from y=-4.95 to -1.2 and x=-10.8 to 15;
  - the paved stone quay: y=-1.2 to 3.5, the full 30 m;
  - the west stone pier: x=-15 to -10.8.
- A paved ramp at the back takes the deck down to the bank: from z=0.5 at y=3.5 to z=0 at y=5, a 0.33 grade.
- The water stairs drop from the stair head at the pier's front corner along the pier face, down to a tread 0.2 m above the water. There are 10 risers of 0.18 m with 0.38 m goings.
- The build casts rays down on a 0.25 m grid over the whole walking surface, 4,683 samples. It asserts that every hit is at the walking height, within 0.04 m. The only places skipped are the bollard piles and the stair flight.
- **Height decision:** the layout's 1 m is the deck; it does not cover the structures above it. In the concept the crane post rises 6 m above the water and the jib head reaches 7.4 m. The slot is therefore 30 × 10 × 10 m (`SLOT`; the build asserts it). The bounds are 30.0 × 10.0 × 9.42 m, with z running from -3.5 at the pile feet to 5.92 at the jib head.

## Interpretation

- **Stone quay:** pale squared stone with a single-stone dressed coping along the wharf edge and the stair edge. The rest of the top is flagged.
- **West pier and stairs:** the pier carries the water stairs, a timber railing along the stair edge and across the wharf end, and iron mooring rings on the stair wall.
- **Timber wharf:** two rows of nine piles. The front piles are octagonal, banded with iron and rise 1.1 m above the deck as bollards. Each has a rope coil and an iron cap, and slack guard ropes hang between them.
- **Wharf frame:** a front fascia with iron straps, bearers, a ledger on the quay wall, joists, and boards. Below the deck, crossed braces with iron plates at the crossings and a waling run between the front piles; diagonal braces run back to the middle row.
- **Jib crane:** an iron-banded post on a dressed stone pedestal, with knee braces. It has a raking jib with a strut and a knee, iron straps, a tip sheave between iron cheeks, and a rope from the windlass over the post head and along the jib. A hook block, hook and four slings carry a battened crate, 1.95 m above the deck.
- **Windlass:** an A-frame windlass with a rope drum and a spoked hand wheel with a crank.
- **Banner post:** a post with an arm and knee brace, carrying a navy swallow-tail banner with gilt edging and a gilt stag.
- **Inferred (not in the concept):** the plan depth, the middle pile row, the joists and ledger, the rear ramp, the guard ropes on the bollards, the windlass frame, and everything below the water.
- **Changed from the concept:**
  - The concept's stone pier stands a little above its deck. Here the pier and quay are level with the deck, so the walking surface stays continuous.
  - The concept's stairs climb the pier from the ground. Here they are water stairs down to the river.
  - The jib is swung 35° out over the water (-Y) instead of lying flat in the elevation plane. From the front it still reads to the right, as drawn.
  - The concept spans 16 m; it is stretched to 30 m by repeating the pile bays.

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. No new scans were downloaded.

| Material | Surface | Scan |
|---|---|---|
| `SB_RiversideDocks_Masonry` | Quay, pier and stair walls | `rough_block_wall`, cooled to the concept's pale grey |
| `SB_RiversideDocks_Paving` | Flags, coping, treads, ramp, crane pedestal | `medieval_blocks_03`, cooled; coping and pedestal sample single blocks |
| `SB_OakTimber` (Cottage1's arguments) | Piles, bearers, braces, crane, windlass, railing, banner post, crate | `weathered_brown_planks`, each beam confined to one plank's grain band |
| `SB_RiversideDocks_Decking` | Deck boards | `weathered_brown_planks`, one plank band per board, lighter ageing |
| `SB_RiversideDocks_Rope` | Coils, guard ropes, crane rope, slings | `hessian_380` (jute), darkened; Poly Haven has no rope scan |
| `SB_RiversideDocks_Banner` | Banner | `denim_fabric_03`, navy |

`SB_ForgedIron` (Cottage1's arguments) and `SB_RiversideDocks_Gilt` are plain parametric materials. Stone and timber are damp in a band 0.65 m above the waterline. The build writes this band into the `Weathering` vertex colour itself (`rewet`, as in StoneBridge), because the kit measures damp from z=0, which here is the bank.

## Limits

- **Not modelled:** the concept's crates, barrels and sacks on the deck are set dressing. The crate hanging from the crane is kept, because it is part of the crane's rigging.
- **Unreal:** it has not been imported yet; the intended mesh is `SM_SilverbrookRiversideDocks`.
  - **Placement:** the town build should place the origin at bank height, with -Y toward the water. The quay face (y=-1.2) goes on the shoreline, so the wharf stands over the water.
  - **Collision:** convex hulls around the crane may enclose the hanging crate's headroom. Check walkability under the jib after import.
- **Overlapping geometry:** piles, braces, bands and boards are separate solids that intersect. Faces where parts touch sit back to back and are hidden. The meshes are not watertight unions.
- **Shader round-trip and GLB textures:** same as Cottage1. The exports carry UVs, `UVMap`/`WorldUV`, material slots and `Weathering`. The GLB is written without images, and the FBX references `../Textures` relatively; the review asserts that those references resolve.
