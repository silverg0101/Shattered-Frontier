# Silverbrook Cottage 1 — concept prototype

Reference: `Cottage-1-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Cottage-1.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_cottage1.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookCottage1.blend -P review_cottage1.py
```

The builder writes `SilverbrookCottage1.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `Cottage1-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The walls are 6.6 × 4.6 m. The whole model, including eaves, porch, finial and chimney pots, is 7.65 × 5.94 × 5.99 m, which fits the town slot of 8 × 6 × 6 m (`SLOT`; the build asserts it). The origin is the slot's ground centre.
- The main roof is a steep-hipped slate roof: 2.4 m eaves, 5.35 m ridge, 0.3 m overhang. The stack and pots stop under 6 m, a little lower than in the concept.
- Over the front wall is a central half-timbered gable whose eaves land on the main slope at 3.6 m. Above the bressumer are a leaded window, shutters, a collar and a king post, with crossed bargeboards and a finial.
- Below it is a gabled timber porch on a two-step stone landing. It shelters a round-headed plank door with iron straps and a ring handle, in a dressed-stone surround of jamb quoins and voussoirs.
- On the right slope is a dormer; a ridge stack carries two clay pots. There are two forged lanterns: one on a bracket, one hanging in the porch.
- The ground storey is ashlar on a plinth, with oak posts, rails and braces. The six ground-floor windows are leaded casements with sills and board shutters; the two front ones have planter boxes.
- **Inferred (not in the concept):** the sides and rear, one window on each side wall, two at the back, and the chimney's depth position.

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. `../silverbrook_kit.py` owns the mapping.

| Surface | Scan |
|---|---|
| Slates | `grey_roof_tiles_02`, one slate interior per modelled slate, with per-slate tone variation |
| Lichen on slate | `lichen_rock` pattern warmed to ochre, masked per slate |
| Ashlar, plinth, stack, steps | `stone_wall_04`; quoins, voussoirs and the stack cap sample single stones |
| Gable and dormer infill | `patterned_clay_plaster` |
| Timber framing | `weathered_brown_planks`, each beam confined to one plank's grain band |
| Door | `old_planks_02` |
| Shutters | `blue_painted_planks`, aged darker |

Iron, lead came, lit glass, lantern glow, soil and chimney pots are plain parametric materials. Damp at the base and soot on the stack use the exported `Weathering` vertex colour.

## Limits

- **Unreal:** `../import_silverbrook.py` imports it as `/Game/Frontier/Environment/Greenlands/Silverbrook/Meshes/SM_SilverbrookCottage1`: Nanite, 12 convex collision hulls, and shared `MI_SB_*` material instances. `ue-import-report.json` records the size check. It is listed under `Cottage.have` in the look profile (1 of 6 cottage variants). There is no interior, and the door is closed.
- **Overlapping geometry:** individual slates (~60k triangles in total) overlap where the gable and dormer roofs meet the main roof. Hidden parts are culled per slate, but the meshes are not watertight.
- **Shader round-trip:** the Blender shader graph does not round-trip to FBX/GLB. The exports carry UVs, the `UVMap`/`WorldUV` layers, material slots and the `Weathering` vertex colours. In Unreal, `M_SB_Scan` rebuilds the weathering (vertex colour R) and lichen (G on UV1) from the shared textures, using the material table in `asset-manifest.json`. The review checks that the FBX texture references resolve.
- **GLB textures:** the GLB is exported without images (`export_image_format='NONE'`) to keep it at 6 MB; the FBX references the textures relatively.
- **Not modelled:** the concept's set dressing (window-box flowers, shrubs, fence, barrel, pots, path) belongs to settlement dressing.
