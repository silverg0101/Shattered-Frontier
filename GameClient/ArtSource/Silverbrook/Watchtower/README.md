# Silverbrook Watchtower — concept prototype

Reference: `Watchtower-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Watchtower.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry interpreting that elevation, not image reconstruction. Asset `BLD-GreenlandsWatchtower-01`.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_watchtower.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookWatchtower.blend -P review_watchtower.py
```

The builder writes `SilverbrookWatchtower.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `Watchtower-{Beauty,Front,Side,Rear}.png` from the saved .blend, reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The concept is read at about 120 px/m. The whole model, including banner arms and flagpole, is 5.9 × 4.9 × 8.96 m and fits the 8 × 8 × 9 m slot (`SLOT`; the build asserts it). The flagpole is shorter than in the concept so the spear tip stays under 9 m.
- The shaft is a chamfered square in plan (3.4 m across, 0.45 m walls), 5.25 m tall, with battered buttresses on the four chamfers below a string course at 2.6 m.
- The front has a round-headed ledged plank door with iron straps and ring, set in a dressed quoin-and-voussoir surround. It sits on a stone step and has a forged lantern on each side.
- Each flat face has a pointed arrow slit in a dressed surround. A solid stair core inside keeps the slits dark.
- The oak hoarding has joists, hanging posts, knee braces and long corner braces. They carry a ring beam and a deck 4.7 m square.
- On the deck is a crenellated parapet with timber balustrades in the crenels. Four square corner turrets have slits and iron fire baskets with flames.
- The oak lookout has four posts, arched braces and plates under a pyramid roof of individual wooden shingles, with hip boards and a finial.
- The iron flagpole carries a navy swallowtail pennant with a gilt stag.
- Two navy stag banners with gilt trim hang from oak arms on the left and right faces. The stag cut-out is copied from the Golden Stag Inn.
- **Inferred (not in the concept):** the rear and side faces (slits, buttresses), the plan shape and depth, the stair core, and the turrets at the rear corners.

## Materials

| Surface | Material | Scan |
|---|---|---|
| Walls, buttresses, parapet, turrets, quoins, slit surrounds, step | `SB_Watchtower_Stone` | `seaworn_stone_tiles` (coursed squared rough-faced blocks), cool grey `dark` tint; dressed stones sample single blocks |
| Roof shingles | `SB_Watchtower_Shingle` | `wood_planks_grey`, one plank interior per shingle |
| Hoarding, lookout, roof boarding, arms | `SB_OakTimber` | `weathered_brown_planks` (Cottage1 args) |
| Door, deck | `SB_DoorBoards` | `old_planks_02` (Cottage1 args) |
| Banners, pennant | `SB_Watchtower_Banner` | `denim_fabric_03`, darkened navy |

These are plain parametric materials: `SB_ForgedIron` and `SB_LanternGlow` (Cottage1 args), `SB_Watchtower_Flame` and `SB_Watchtower_Gilt`. `seaworn_stone_tiles` was added to the palette with `acquire_materials.py`. Sources are listed in `../materials-provenance.json`.

## Limits

- **Stone colour:** `seaworn_stone_tiles` replaces the warm, irregular `rough_block_wall`. Its regular courses match the concept's squared stone. Weathering R is left at 0 except the damp band at the foot, so Unreal's `M_SB_Scan` (Diffuse lerped to Diffuse x DarkTint by R) shows the scan as rendered here. Sampled sunlit wall in `Watchtower-Front.png` is about (0.57, 0.52, 0.46) sRGB. The concept's stone is about (0.54, 0.47, 0.41), warmed by the parchment. No moss or ivy is modelled.
- **Not modelled:** iron plates on the ring beam, the slit in the middle merlon, and the concept's set dressing (rocks, shrubs, ivy).
- **Flat cloth:** the banners and pennant are flat; there is no cloth sim or wave.
- **Shader round-trip:** this is the same as Cottage1. The exports carry UVs, `UVMap`/`WorldUV`, material slots and `Weathering` vertex colours, and the GLB is exported without images. The review checks that the FBX texture references resolve.
- **Unreal:** not imported yet. The target mesh is `SM_SilverbrookWatchtower`. There is no interior, and the door is closed.
