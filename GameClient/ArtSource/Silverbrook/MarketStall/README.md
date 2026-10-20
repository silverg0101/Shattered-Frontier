# Silverbrook Market Stall — concept prototype

Reference: `MarketStall-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Market Stall.png`; SHA-256 in `asset-manifest.json`). Asset ID `BLD-GreenlandsMarketStall-01`. Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_marketstall.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookMarketStall.blend -P review_marketstall.py
```

The builder writes `SilverbrookMarketStall.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `MarketStall-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The whole model, including the lantern bracket, banner arm and post finials, is 3.8 × 1.81 × 4.0 m, which fits the 4 × 4 × 4 m slot (`SLOT`; the build asserts it). The origin is the slot's ground centre. The triangle count is about 12.6k, under the 40k small-prop budget.
- Four oak posts stand 2.8 m apart. The front posts rise to 4 m and end in carved finials, with iron bands at the foot, the counter and the top. Jute rope lashes the rails to the posts.
- The canvas awning has five stripes (red, cream, red, cream, red). It runs from the rear top rail at 3.5 m down to a front rail at 2.6 m, sagging between the rails and between the sewn seams. It then folds over the front rail into a ragged valance that ends at about 2.0–2.1 m.
- The counter is 0.9 m high, with a plank top on bearers, a front apron, X-braced end bays and a low shelf. A red cloth is draped over the middle of the counter, with a torn hem.
- A forged lantern hangs from a timber arm on the left post. On the right post, a braced timber arm carries a navy banner with a gilded stag and border on both faces.
- **Inferred (not in the concept):**
  - The stall is 1.6 m deep. The awning is a shed roof that falls toward the front, because the elevation shows it rising to the back.
  - Also inferred: the rear posts and rails, the red side drops under the sloping awning edges, and the counter's back legs and shelf.
- **Compressed:** in the drawing, the lantern and banner reach about 4.8 m in total. Here the posts are closer together and the brackets shorter, so they fit the 4 m slot.

## Materials

Every scanned surface uses a CC0 Poly Haven scan at its measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. New scans: `hessian_380` and `velour_velvet`.

| Surface | Material | Scan |
|---|---|---|
| Posts, rails, counter, brackets, finials | `SB_OakTimber` | `weathered_brown_planks`, each beam confined to one plank's grain band |
| Red awning stripes, side drops, counter cloth | `SB_MarketStall_AwningRed` | `velour_velvet` (worn crimson), lightly aged |
| Cream awning stripes | `SB_MarketStall_AwningCream` | `hessian_380` (fine jute canvas), with soft grime toward the hem |
| Rope lashings | `SB_MarketStall_Rope` | `hessian_380`, darkened |
| Banner | `SB_MarketStall_Banner` | `denim_fabric_03`, darkened to navy |

Forged iron (`SB_ForgedIron`), the lantern glow (`SB_LanternGlow`) and the stag gilt (`SB_MarketStall_Gilt`) are plain parametric materials.

## Limits

- **Unreal:** not imported yet; `../import_silverbrook.py` owns the import.
- **Red stripes:** no Poly Haven canvas scan comes in red, and the kit's tint cannot turn cream jute crimson. The red stripes therefore use a red velvet scan, whose weave differs from the jute in the cream stripes.
- **Cloth:** the awning is two-sided, with its two layers 6 mm apart and smooth-shaded. The torn hems are a jagged edge line, not cut-out holes.
- **Shader round-trip:** the Blender shader graph does not round-trip to FBX/GLB. The exports carry UVs, the `UVMap`/`WorldUV` layers, material slots and the `Weathering` vertex colours, and the review checks that the FBX texture references resolve. The GLB is exported without images.
- **Not modelled:** the concept's set dressing belongs to settlement dressing: produce crates, sacks, barrels, bottles, jugs, bread, hanging herb bundles, the price sign and the under-counter crates.
