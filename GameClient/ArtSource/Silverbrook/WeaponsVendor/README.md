# Silverbrook Weapons Vendor — concept prototype

Reference: `WeaponsVendor-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Weapons Vendor.png`; SHA-256 in `asset-manifest.json`). Asset ID `BLD-GreenlandsWeaponsVendor-01`. Original procedural geometry interpreting that elevation, not image reconstruction, plus placed CC0 Poly Haven models.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_weaponsvendor.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookWeaponsVendor.blend -P review_weaponsvendor.py
```

The builder writes `SilverbrookWeaponsVendor.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `WeaponsVendor-{Beauty,Front,Side,Rear}.png` and four close-ups (`CloseCounter`, `CloseWall`, `CloseLeft`, `CloseRight`) from the saved .blend in a fresh process. It then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The whole model is 5.81 × 2.51 × 4.60 m, which fits the 6 × 3.5 × 5 m slot (`SLOT`; the build asserts it). That includes the banner arm, the lantern arm, the end barrels and the finials. It has about 237k triangles, under the 350k budget set for this stall; Poly Haven models make up about 200k of them.
- **Frame:** two oak posts stand 4 m apart (centre to centre), with pointed finials at 4.6 m. They carry iron bands and rope lashings.
- **Awning:** seven red/cream canvas stripes run over side rafters. The awning slopes from a rear top rail at 3.8 m to a front batten at 2.8 m and sags between them. A ragged valance hangs to about 2.4 m, with deeper tears in places. Knee braces from the posts carry the rafters.
- **Banner and lantern:** on the left, a red banner hangs on chains from a braced arm. It has gold crossed swords, a fleur-de-lis and a gilt border on both faces. On the right, a hexagonal iron lantern hangs on a chain from a second arm.
- **Back wall:** vertical boards carry the following stock:
  - a sword rack with six swords hanging point-down on iron pegs, and one more lying across the top;
  - a shelf with two spiked morning stars and a flanged mace;
  - a round board shield with an iron rim, boss and straps;
  - three tall crescent double-bit axes and a spear leaning against the wall.
- **Counter:** 0.95 m high, with braced end bays and a red drape with gilt crossed swords. On it stand two open display cases and a tray of daggers and knives, a standing lantern, a mace, a saber and a war hammer.
- **Left end:** a polearm rack holds two halberds, a spear and a glaive. A crate holds swords and an axe, and a crate of red- and cream-fletched arrows is tied with rope. A barrel has a hatchet lying on its lid.
- **Right end:** a crate holds a halberd, a spear, a glaive, a double-bit axe and two bearded axes. Beside it is an open stave barrel holding a bearded axe, a double-bit axe, a war hammer, an axe and a mace, and a chest with an axe and a hammer on it.
- **Lights:** `light_socket_m` lists the two glass centres: the counter lantern first, then the hanging one. The glass uses `SB_WeaponsVendor_Glow`.
- **Inferred (not in the concept):**
  - the stall's depth (2.5 m in all);
  - the rear posts, top rail, rafters and front batten;
  - the back wall's rear battens and diagonal brace;
  - the counter's back legs and low shelf;
  - the case lids;
  - the chest behind the right crate.

## Materials

Scanned surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`. No new scans or models were downloaded.

| Surface | Material | Scan |
|---|---|---|
| Posts, rafters, counter, crates, racks, shield boards, open barrel | `SB_OakTimber` | `weathered_brown_planks`, one plank band per beam |
| Back wall boards | `SB_WeaponsVendor_WallBoards` | `old_planks_02` |
| Weapon hafts, knife handles, arrow shafts | `SB_WeaponsVendor_Haft` | `weathered_brown_planks`, warmed |
| Sword grips | `SB_WeaponsVendor_Leather` | `brown_leather` |
| Red stripes, side drops, counter drape, banner, case linings, fletching | `SB_WeaponsVendor_ClothRed` | `velour_velvet`, darkened |
| Cream stripes, fletching | `SB_WeaponsVendor_ClothCream` | `hessian_380` |
| Rope lashings | `SB_WeaponsVendor_Rope` | `hessian_380`, darkened |

Plain parametric materials:

- `SB_ForgedIron` (bands, chains, straps, lantern frames);
- `SB_WeaponsVendor_Steel` (blades and ground edges);
- `SB_WeaponsVendor_SteelWorn` (forged axe bodies, guards, pommels, sockets);
- `SB_WeaponsVendor_Gilt` (emblems);
- `SB_WeaponsVendor_Glow` (lantern glass).

**Poly Haven models** (via `kit.model`, with shared `SB_PH_*` materials; counts in `asset-manifest.json`):

| Model | Count |
|---|---|
| `antique_estoc` | 6 |
| `ornate_medieval_dagger` | 10 |
| `ornate_medieval_mace` | 3 |
| `ornate_war_hammer` | 3 |
| `wooden_handle_saber` | 3 |
| `wooden_axe` | 2 |
| `wooden_axe_02` | 1 |
| `wooden_axe_03` | 1 |
| `wine_barrel_01` | 1 |
| `wooden_crate_01` | 1 |
| `wooden_crate_02` | 1 |

Modelled here: the arming swords, knives, halberds, glaives, spears, double-bit and bearded axes, morning stars, arrows, shield, open crates, stave barrel and lanterns.

## Limits

- **Unreal:** not imported yet; `../import_silverbrook.py` owns the import. The stock is part of the mesh (the owner wants the stall shown well stocked), so there are no separate dressing props.
- **Shapes:** the modelled weapons are faceted lofts: diamond or wedge sections, with no fullers and no engraving.
- **Morning stars:** the concept's spiked "helmets" on the shelf are read as morning-star heads.
- **Not used:** `kite_shield` was tried and removed, because the concept shows only a round shield. The other Poly Haven barrels are modern, and `hatchet` is a modern hatchet, so neither was used.
- **Red cloth:** same as the Market Stall: the red stripes use a velvet scan, whose weave differs from the jute in the cream stripes. The torn hems are a jagged edge line, not cut-out holes.
- **Shader round-trip:** the Blender shader graph does not round-trip to FBX/GLB. The exports carry UVs, `UVMap`/`WorldUV`, material slots and `Weathering` vertex colours. The review checks that the FBX texture references resolve. The GLB has no images.
- **Overlapping geometry:** weapons stand in crates, barrels and racks with small interpenetrations where hafts lean together. Every piece rests on a floor, a peg or a surface.
