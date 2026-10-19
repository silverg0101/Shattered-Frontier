# Silverbrook Cottage 4 — concept prototype

Reference: `Cottage4-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Cottage-4.png`; SHA-256 in `asset-manifest.json`). Catalogue asset `BLD-GreenlandsCottage-01`, variant 4. Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_cottage4.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookCottage4.blend -P review_cottage4.py
```

The builder writes `SilverbrookCottage4.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `Cottage4-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The walls are 6.6 × 4.4 m. The whole model, including eaves, door hood, finial and chimney pot, is 7.91 × 5.91 × 5.98 m, which fits the town slot of 8 × 6 × 6 m (`SLOT`; the build asserts it and the 120k house triangle budget). It has 31.6k triangles.
- The main roof is a steep-hipped reed thatch: 2.35 m eaves, 5.0 m ridge. The thatch is laid in thick, stepped, overlapping courses with the reeds running down the slope, as the concept draws it, and the ridges and hips carry thatch rolls.
- A tall half-timbered cross gable rises above the main ridge, to 5.5 m. Its jettied bressumer carries a timber frame with rubble infill, a leaded window with green board shutters and a planter, crossed bargeboards and a finial. The concept's 7 m finial is cut down to fit the 6 m slot.
- Below it, a thatched door hood on knee braces shelters a round-headed plank door. The door has iron straps and a ring, and sits in a grey dressed-stone surround of jamb quoins and voussoirs, above a two-step stone landing.
- There are two small thatched dormers with leaded lights, and a grey squared-stone stack on the right with a clay pot. Two forged lanterns hang from a bracket at the left corner and in the hood.
- The ground storey is squared rubble on a plinth of larger stones, with oak corner and door posts, a wall plate and braces. The two front windows are leaded casements with green shutters and planter boxes.
- **Inferred (not in the concept):** the sides and rear; the cross gable running through to a matching rear gable; one window on each side wall and two at the back; the stack's depth position; the dormers made symmetric.

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. `../silverbrook_kit.py` owns the mapping.

| Surface | Material | Scan |
|---|---|---|
| Thatch courses, ridge rolls | `SB_Cottage4_Thatch` | `reed_roof_04` (new): one course samples one patch below the scan's mossy ridge strip, with per-course tone variation |
| Walls, plinth, gable and dormer infill | `SB_Cottage4_Rubble` | `rustic_stone_wall`, squared tan rubble |
| Door surround, steps, stack | `SB_Ashlar` | `stone_wall_04`, grey squared stone; quoins, voussoirs and the stack cap sample single stones |
| Timber frame, roof boarding, planters | `SB_OakTimber` | `weathered_brown_planks` |
| Door | `SB_DoorBoards` | `old_planks_02` |
| Shutters | `SB_Cottage4_Shutter` | `green_rough_planks` (new), aged |

Iron, lead came, lit glass, lantern glow, soil and the chimney pot reuse Cottage 1's parametric materials with the same arguments. Damp at the base uses the exported `Weathering` vertex colour.

## Limits

- **Not matched:** the green moss on the thatch. The kit's lichen layer is ochre and slate-specific, so the thatch is unmossed. The dormer hoods are steep gables, not the concept's rounded eyebrows, and the left dormer's light is square-headed, not arched. The carved gable boards and pierced brackets are not modelled.
- **Overlapping geometry:** thatch courses overlap where the gable, dormer and hood roofs meet the main roof. Hidden courses are culled per course, but the meshes are not watertight.
- **Shader round-trip:** the Blender shader graph does not round-trip to FBX/GLB. The exports carry UVs, the `UVMap`/`WorldUV` layers, material slots and the `Weathering` vertex colours; Unreal rebuilds the materials from the table in `asset-manifest.json`. The review checks that the FBX texture references resolve. The GLB is exported without images.
- **Not modelled:** there is no interior, and the door is closed. The concept's set dressing (ivy, flowers, fence, barrel, pots, rocks) belongs to settlement dressing. It has not been imported into Unreal.
