# Silverbrook Townhouse 6 — concept prototype

Reference: `Townhouse6-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Townhouse-6.png`; SHA-256 in `asset-manifest.json`). Asset ID `BLD-GreenlandsTownhouse-01`, variant 6. Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_townhouse6.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookTownhouse6.blend -P review_townhouse6.py
```

The builder writes `SilverbrookTownhouse6.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `Townhouse6-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The walls are 5.5 × 5 m (the ground storey is 4.65 m deep under the jetty). The whole model, including eaves, banner bracket, finial and chimney pots, is 6.94 × 6.13 × 8.97 m, which fits the 8 × 7 × 9 m slot (`SLOT`; the build asserts it). The origin is the slot's ground centre.
- The concept is drawn about 11 m to the finial. To fit 9 m, widths are kept near the drawing (×0.92) and heights compressed (×~0.75). The door keeps a usable 2 m clear height.
- The roof is a steep-hipped slate roof with 5.6 m eaves, a 7.8 m ridge and a timber ridge-end finial. A half-timbered front cross gable, set 0.6 m left of centre as drawn, rises above the ridge to 8.4 m. It has crossed bargeboards, an iron-tipped finial, and a tie beam, collar, king post and struts.
- The upper storey is half-timbered lime plaster, jettied 0.35 m on a bressumer carried by eight stepped oak corbels. The gable carries a canted oriel on three braced corbels, with a planter and its own hipped slate roof. Narrow leaded lights flank it. On the right is a leaded window with a planter.
- The ground storey is coursed pale ashlar. A canted bay window has four leaded lights on a stone base, a planter and a hipped slate pent roof. A round-headed plank door sits in a quoin-and-voussoir surround, up two stone steps, with a bracket lantern and a stone planter pier.
- On the right slope is a dormer with a spike finial. A left stack carries a two-course cap and two clay pots. Iron downpipes run from hoppers at both front corners. A forged bracket with a scroll carries a navy banner with a gilded stag and border on both faces.
- **Inferred (not in the concept):**
  - The cross gable runs the full depth, with a rear gable and window, because the drawn gable is taller than the main ridge.
  - Also inferred: the sides and rear, their windows, and the depth position of the stack.

## Materials

Every scanned surface uses a CC0 Poly Haven scan at its measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. No new scans were downloaded.

| Surface | Material | Scan |
|---|---|---|
| Slates | `SB_Slate` | `grey_roof_tiles_02`, one slate interior per modelled slate; `lichen_rock` lichen |
| Ground storey, stack, steps, door surround | `SB_Townhouse6_Ashlar` | `sandstone_blocks_05` (pale, rough-faced coursed blocks); quoins, voussoirs and caps sample single stones |
| Upper storey and gable infill | `SB_LimePlaster` | `patterned_clay_plaster` |
| Timber framing, corbels, oriel, planters | `SB_OakTimber` | `weathered_brown_planks`, each beam confined to one plank's grain band |
| Door | `SB_DoorBoards` | `old_planks_02` |
| Banner | `SB_Townhouse6_Banner` | `denim_fabric_03`, darkened to navy |

Forged iron, lead came, lit glass, lantern glow, planter soil, chimney pots and the stag gilt (`SB_Townhouse6_Gilt`) are plain parametric materials. Damp at the base uses the exported `Weathering` vertex colour.

## Limits

- **Unreal:** not imported yet; `../import_silverbrook.py` owns the import. There is no interior, and the door is closed.
- **Simplified:** the concept's Gothic tracery heads on the bay and oriel lights are drawn as the kit's rectangular diamond-leaded casements. The door is round-headed, not slightly pointed. The banner's fleur-de-lis is omitted.
- **Overlapping geometry:** individual slates overlap where the gable, dormer and bay roofs meet other roofs or walls. Hidden slates are culled, but the meshes are not watertight.
- **Shader round-trip:** the Blender shader graph does not round-trip to FBX/GLB. The exports carry UVs, the `UVMap`/`WorldUV` layers, material slots and the `Weathering` vertex colours, and the review checks that the FBX texture references resolve. The GLB is exported without images.
- **Not modelled:** the concept's set dressing (window-box flowers, potted topiaries, the dormer flower box) belongs to settlement dressing.
