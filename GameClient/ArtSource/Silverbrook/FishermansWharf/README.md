# Silverbrook Fisherman's Wharf — concept prototype

Reference: `FishermansWharf-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Fishermans Wharf.png`; SHA-256 in `asset-manifest.json`). Asset `BLD-GreenlandsFishermansWharf-01`. Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_fishermanswharf.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookFishermansWharf.blend -P review_fishermanswharf.py
```

The builder writes `SilverbrookFishermansWharf.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `FishermansWharf-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used.

## Orientation and levels

- **The front (-Y) is the water side.** The long side runs along X, parallel to the bank. The land is at +Y. Z is up, and units are metres.
- **The origin is at the deck centre, at riverbank ground level.** As for StoneBridge, `heightfield.py` puts the bank 1.5 m above the water and the channel bed 2 m below the water. So the bank is at z=0, the water at z=-1.5 and the bed at z=-3.5.
- **The deck top is at z=0, flush with the bank and 1.5 m above the water.** The concept shows about 1.9 m. The deck is one continuous plank surface, 19.9 × 7.75 m including the fascia, and the buildings stand on it. The land edge (+Y) meets the bank with no step. The water edge (-Y) has a fascia, mooring posts that rise about 1 m above the deck, and two ladders down to below the waterline. The build casts rays down on a 0.2 m grid over the whole deck, inside the buildings too (3,762 rays). It asserts that every ray reaches a walking surface at z=0 with no gap or step.
- **Slot height.** The layout gives the wharf 20 × 8 × 1 m, which covers the deck only. The concept has 8 m of shop, crane and chimney above the deck, and the piles reach the bed 3.5 m below the bank. So the slot is 20 × 8 × 11.5 m (`SLOT`). The bounds are 20.0 × 7.95 × 11.5 m, with z running from -3.5 to 8.0. The build asserts the fit.

## Interpretation

- **Substructure.** The deck is 80 grey planks running toward the land. They rest on four bearers along X, carried on rows of 0.3 m timber piles. There is X-bracing behind the front piles and at both ends. The front row is the concept's line of mooring posts.
- **Fish shop.** It is 6 × 5 m, with a gable facing the water.
  - The ground storey is stone, with an open shop front 3.8 m wide under a timber bressumer. Behind the opening is a 1.1 m recess with a fish counter and ceiling joists. It is a closed recess, not an interior.
  - Above the bressumer is a jettied gable of vertical boards with corner posts and a rail. It carries the navy banner with a gilt stag on an iron rod.
  - The roof is a steep slate roof: 3.7 m eaves, 7.6 m ridge, bargeboards and a finial.
  - A stone stack with two clay pots rises from the left slope.
- **Side wing.** The wing's slate roof runs into the shop roof with a proper valley. It has a dormer with a leaded window. Under it is an open porch on posts with knee braces, in front of a boarded body with a door and a window. The wing's slope is inferred to match the concept's front view.
- **Work shed and pavilion.** Next is an open work shed with a low slate roof, posts, ties, braces and a king-post ridge. After it comes a lean-to drying pavilion with a single slate slope falling to the right. It has drying rails and a boarded end where it rises above the shed.
- **Crane and awning.** A jib crane stands at the left front corner. It has braces, an iron pulley block, and a chain and bridle holding a hanging basket. Beside it, a cream canvas awning runs from the shop's side wall to a post-and-rail frame over a fish-drying rail.
- **Lanterns.** Five forged lanterns on brackets: two on the shop piers, and one each on the porch, the shed and the pavilion posts.
- **Inferred (not in the concept):** the depth layout, all rear and side faces, the rear arched door and the rear window, the side window, and everything below the deck.

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. One scan was added for this package: `cotton_jersey`.

| Material | Surface | Scan |
|---|---|---|
| `SB_Slate` (Cottage1) | Slates and ridge caps | `grey_roof_tiles_02`, lichen from `lichen_rock` |
| `SB_Ashlar` (Cottage1) | Shop ground storey, stack, door surround | `stone_wall_04` |
| `SB_OakTimber` (Cottage1) | Piles, bearers, framing, crane, roof boarding | `weathered_brown_planks` |
| `SB_DoorBoards` (Cottage1) | Doors | `old_planks_02` |
| `SB_FishermansWharf_Boards` | Vertical wall boarding, gables, dormer | `weathered_brown_planks` |
| `SB_FishermansWharf_Deck` | Deck planks | `wood_planks_grey` |
| `SB_FishermansWharf_Canvas` | Awning | `cotton_jersey`, warmed to cream |
| `SB_FishermansWharf_Banner` | Banner | `denim_fabric_03`, navy |

`SB_ForgedIron`, `SB_LeadCame`, `SB_LitGlass`, `SB_LanternGlow` and `SB_ChimneyPot` use Cottage1's arguments. `SB_FishermansWharf_Gilt` is a plain parametric material.

Damp is written into the `Weathering` vertex colour from the water upward: below the water, and in a 0.65 m band above it. The kit's usual damp band, measured from z=0, is switched off because z=0 here is the deck.

## Limits

- **Not modelled:** the set dressing. That covers the barrels, crates, lobster pots, hanging nets, hanging fish, ropes on the posts, and the goods on the counter.
- **Simplified:** the basket is a slatted crate, not wicker. The shop opening has a flat lintel instead of the concept's shallow arch. The pavilion's short left slope is a boarded end. The awning is flat instead of sagging.
- **Unreal:** it has not been imported yet; the intended mesh is `SM_SilverbrookFishermansWharf`. The town build should place the origin at bank height, with the front (-Y) facing the water.
- **Triangles:** 84,040, within the 120k house budget. The meshes are not watertight unions. Slates overlap where the roofs meet, and touching parts sit back to back.
- **Shader round-trip and GLB textures:** same as Cottage1. The GLB is written without images. The FBX references `../Textures` relatively, and the review asserts that those references resolve.
