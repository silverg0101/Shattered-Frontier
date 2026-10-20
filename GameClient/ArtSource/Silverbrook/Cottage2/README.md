# Silverbrook Cottage 2 — concept prototype

Reference: `Cottage2-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Cottage-2.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry interpreting that elevation, not image reconstruction. Variant 2 of `BLD-GreenlandsCottage-01`.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_cottage2.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookCottage2.blend -P review_cottage2.py
```

The builder writes `SilverbrookCottage2.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `Cottage2-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The whole model, including eaves, steps, finial, side lantern and chimney pots, is 7.86 × 5.79 × 5.99 m. It fits the town slot of 8 × 6 × 6 m (`SLOT`; the build asserts it). About 48k triangles.
- On the left is a cross wing in rubble stone. It runs the full depth, 4.4 × 4.4 m, under a steep front gable: 3.2 m eaves, 5.65 m ridge. Crossed bargeboards meet under a finial, and knee braces sit under the eave ends. The gable holds a leaded window with shutters and a window box over a floor beam at 2.9 m. Below are a shuttered ground window and the door.
- The door is round-headed, with plank boards, iron straps and a ring, in a dressed-stone surround. It stands on three stone steps (0.42 m), under a slated door hood carried on wall posts and knee braces. A lantern hangs in the hood.
- On the right is a lower range, 2.5 × 4 m of walls, under a slate roof: 2.6 m eaves, 5.35 m ridge. Its right end is hipped, and its ridge runs into the wing roof. It has a timber-framed dormer, a rubble stack with two clay pots at the ridge end, a shuttered window with a window box, and a bracket lantern at the right front corner.
- The shutters are grey boards with dark diamond insets.
- **Inferred (not in the concept):** the rear and side walls, with one shuttered window on each of the wing's back and left walls, the range's back wall and the range's right end. Also inferred: the wing's plain rear gable, the depths of the wing and the range, and the stack's depth position. The stack and pots stop under 6 m; they are a little lower than in the concept.

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. `../silverbrook_kit.py` owns the mapping.

| Surface | Material | Scan |
|---|---|---|
| Walls, gables, stack, steps, door surround | `SB_Cottage2_Rubble` | `stone_wall` (pale rounded fieldstone with light mortar, as Cottage 5); the quoins, voussoirs and stack caps sample single stones |
| Slates, hood, ridge caps | `SB_Slate` | `grey_roof_tiles_02` plus ochre `lichen_rock`, as in Cottage 1 |
| Timber framing, bargeboards, dormer, window boxes, roof boarding | `SB_OakTimber` | `weathered_brown_planks` |
| Door | `SB_DoorBoards` | `old_planks_02` |
| Shutters | `SB_Cottage2_Shutter` | `wood_planks_grey` (new); the boards run vertically |

Iron (including the diamond insets), lead came, lit glass, lantern glow, soil and chimney pots use Cottage 1's plain parametric materials, with the same names and arguments. Damp at the base uses the exported `Weathering` vertex colour.

## Limits

- **Unreal:** not imported yet. `../import_silverbrook.py` would bring it in as `SM_SilverbrookCottage2`. There is no interior, and the door is closed.
- **Overlapping geometry:** individual slates overlap where the range roof, dormer and stack meet the wing roof. Hidden slates are culled one by one, but the meshes are not watertight.
- **Stone tone:** the walls sample mean RGB (142, 127, 109) against the concept stone (133, 112, 89): slightly greyer and lighter than the concept, and the scanned stones are smaller than its cushion stones.
- **Shader round-trip and GLB textures:** the same as Cottage 1. The exports carry UVs, `UVMap`/`WorldUV`, the material slots and `Weathering`. The GLB has no images; the FBX references `../Textures` relatively, and the review asserts that those references resolve.
- **Not modelled:** the concept's set dressing (ivy, flowers in the window boxes, potted plants, shrubs, fence, barrel) belongs to settlement dressing.
