# Silverbrook Townhouse 2 — concept prototype

Reference: `Townhouse2-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Townhouse-2.png`; SHA-256 in `asset-manifest.json`). Original procedural geometry interpreting that elevation, not image reconstruction. Concept README asset `BLD-GreenlandsTownhouse-01`, variant 2.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_townhouse2.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookTownhouse2.blend -P review_townhouse2.py
```

The builder writes `SilverbrookTownhouse2.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `Townhouse2-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The walls are 5.6 × 4.8 m. The whole model, including eaves, gutters, steps, the bracket sign and the spire, is 6.96 × 6.32 × 8.97 m, which fits the town slot of 8 × 7 × 9 m (`SLOT`; the build asserts it). The concept is drawn 6 m wide and 10 m tall; heights are compressed by about 0.85 so the spire stays under 9 m.
- The ground storey is pale squared stone on a plinth. A pointed (equilateral) arch plank door with strap hinges and a ring sits in dressed jambs and voussoirs, up three stone steps. A wall lantern hangs beside it.
- The left bay's upper storey is jettied 0.3 m on a bressumer and corbels. It is half-timbered lime plaster with a leaded window and planter box.
- The right half is a front-gabled wing. Its steep (60°) slate roof runs front to back above the main ridge, with crossed bargeboards and a timber-and-iron spire finial. The jettied plaster gable has a king post, a collar and a leaded window with a planter.
- Under the gable is a two-storey canted oriel: an oak frame, leaded lights on three faces in two tiers, raised panels below each tier, a stone plinth and its own hipped slate roof.
- The main roof is slate, hipped on the left, with a dormer and a stone stack carrying two clay pots. Iron gutters and downpipes run at both front corners.
- An iron scroll bracket on the left wall carries a navy banner with a gilt stag on both faces.
- **Inferred (not in the concept):** the sides and rear (stone ground storey, half-timbered upper, two windows each side, four at the back, a rear wing gable), the roof depth, and the stack position on the hip.

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. `../silverbrook_kit.py` owns the mapping. No new scans were downloaded.

| Material | Surface | Scan |
|---|---|---|
| `SB_Slate` | Slates, with per-slate tone and lichen | `grey_roof_tiles_02`, `lichen_rock` |
| `SB_Townhouse2_Ashlar` | Ground storey, plinths, steps, stack; jambs and voussoirs sample single blocks | `sandstone_blocks_05` (pale rough squared blocks, as drawn) |
| `SB_LimePlaster` | Upper-storey and gable infill, dormer | `patterned_clay_plaster` |
| `SB_OakTimber` | Framing, oriel, corbels, bargeboards, roof boarding | `weathered_brown_planks`, each beam confined to one plank's grain band |
| `SB_DoorBoards` | Door | `old_planks_02` |
| `SB_Townhouse2_Banner` | Sign banner | `denim_fabric_03`, aged navy |

`SB_ForgedIron`, `SB_LeadCame`, `SB_LitGlass`, `SB_LanternGlow`, `SB_PlanterSoil`, `SB_ChimneyPot` (Cottage 1's arguments) and `SB_Townhouse2_Gilt` (the stag) are plain parametric materials. Damp at the base uses the exported `Weathering` vertex colour.

## Limits

- **Unreal:** not imported yet; `../import_silverbrook.py` would import it as `SM_SilverbrookTownhouse2`. There is no interior, and the door is closed.
- **Simplified:** the concept's carved Gothic tracery on the oriel panels is reduced to raised panels with a moulding. The stag is a stylised cut-out, and the pots are terracotta rather than the drawn copper tone.
- **Overlapping geometry:** individual slates overlap where the dormer and main roof meet the wing roof. Hidden slates are culled per slate, but the meshes are not watertight. The build asserts positive volume for every box and prism.
- **Shader round-trip and GLB:** same as Cottage 1. The exports carry UVs, `UVMap`/`WorldUV` and `Weathering`. The GLB is exported without images, and the FBX references `../Textures` relatively (the review asserts that they resolve).
- **Not modelled:** set dressing (flowers in the planters, potted shrubs, the barrel, crates).
