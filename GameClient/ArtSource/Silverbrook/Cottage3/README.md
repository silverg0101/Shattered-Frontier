# Silverbrook Cottage 3 — concept prototype

Reference: `Cottage3-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Cottage-3.png`; SHA-256 in `asset-manifest.json`). Asset `BLD-GreenlandsCottage-01`, variant 3. Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_cottage3.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookCottage3.blend -P review_cottage3.py
```

The builder writes `SilverbrookCottage3.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `Cottage3-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The walls are 6.6 × 4.6 m. The whole model, including eaves, porch, steps, finial, pots and the side lantern, is 7.76 × 5.91 × 5.98 m, which fits the town slot of 8 × 6 × 6 m (`SLOT`; the build asserts it). The origin is the slot's ground centre.
- Unlike Cottage 1, the whole ground storey is half-timbered: cream lime-plaster panels between oak posts, rails and braces, on a 0.58 m squared-rubble plinth.
- The main roof is a steep-hipped slate roof: 2.45 m eaves, 5.35 m ridge, short ridge with timber stubs at both ends.
- The front cross gable sits 0.4 m left of the wall centre, as in the concept, and its ridge (5.65 m) rises 0.3 m above the main ridge. It has a bargeboarded, half-timbered face with a shuttered leaded window and planter, collar, king post, struts and finial.
- Below the gable is a gabled timber porch on two rubble piers and a two-step rubble landing. It shelters a round-headed plank door with iron strap hinges and a ring handle, in a timber jamb-and-arch surround (no stone dressing in the concept).
- On the right slope is a dormer with a leaded window and planter. Behind it a rubble stack with a projecting cap course carries two clay pots. There are two forged lanterns: one hanging in the porch, one on a bracket at the right front corner.
- The four front windows are leaded casements with charcoal board shutters (none on the dormer); all have planter boxes.
- **Inferred (not in the concept):** the sides and rear, one window on each side wall and two at the back; the small steep hip that closes the back of the raised cross gable; the chimney's depth position; the plinth depth.

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. `../silverbrook_kit.py` owns the mapping.

| Surface | Material | Scan |
|---|---|---|
| Slates | `SB_Slate` | `grey_roof_tiles_02`, one slate interior per modelled slate, with per-slate tone variation |
| Lichen on slate | (in `SB_Slate`) | `lichen_rock` pattern warmed to ochre, masked per slate |
| Plinth, porch piers, landing, stack | `SB_Cottage3_Rubble` | `rough_block_wall`: grey-taupe squared rubble with rough rounded faces (mean RGB 130/119/96 vs. the concept's 133/114/92). Downloaded for this package. |
| Plaster panels and gable infill | `SB_LimePlaster` | `patterned_clay_plaster` |
| Timber framing, door surround | `SB_OakTimber` | `weathered_brown_planks`, each beam confined to one plank's grain band |
| Door | `SB_DoorBoards` | `old_planks_02` |
| Shutters | `SB_Cottage3_Shutter` | `wood_planks_grey` (the concept's shutters are charcoal, not Cottage 1's blue) |

Iron, lead came, lit glass, lantern glow, soil and chimney pots are the plain parametric `SB_ForgedIron`, `SB_LeadCame`, `SB_LitGlass`, `SB_LanternGlow`, `SB_PlanterSoil` and `SB_ChimneyPot` (same arguments as Cottage 1). Damp at the base and soot on the stack use the exported `Weathering` vertex colour.

## Limits

- **Unreal:** not imported yet. `../import_silverbrook.py` should import it as `SM_SilverbrookCottage3` (Nanite). There is no interior, and the door is closed.
- **Overlapping geometry:** individual slates overlap where the gable, dormer and porch roofs meet the main roof. Hidden parts are culled per slate, but the meshes are not watertight.
- **Shader round-trip:** the Blender shader graph does not round-trip to FBX/GLB. The exports carry UVs, the `UVMap`/`WorldUV` layers, material slots and the `Weathering` vertex colours; `M_SB_Scan` rebuilds the look in Unreal from the material table in `asset-manifest.json`. The review checks that the FBX texture references resolve.
- **GLB textures:** the GLB is exported without images; the FBX references the textures relatively.
- **Simplified:** the shutters have no diamond cut-outs, and the chimney stops below 6 m (the concept's pots reach about 6.6 m).
- **Not modelled:** the concept's set dressing (climbing rose, window-box flowers, shrubs, fence, barrel) belongs to settlement dressing.
