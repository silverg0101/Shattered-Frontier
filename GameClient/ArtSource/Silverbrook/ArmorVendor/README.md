# Silverbrook Armor Vendor — concept prototype

Reference: `ArmorVendor-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Armor Vendor.png`; SHA-256 in `asset-manifest.json`). Asset ID `BLD-GreenlandsArmorVendor-01`. Original procedural geometry interpreting that elevation, not image reconstruction, plus placed CC0 Poly Haven prop models.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_armorvendor.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookArmorVendor.blend -P review_armorvendor.py
```

The builder writes `SilverbrookArmorVendor.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `ArmorVendor-{Beauty,Front,Side,Rear}.png` plus three close-ups (`CloseLeft`, `CloseCounter`, `CloseRight`) from the saved .blend in a fresh process. It then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The whole model is 5.52 × 2.43 × 4.35 m, which fits the 6 × 3.5 × 5 m slot (`SLOT`; the build asserts it). That size includes the banner and lantern arms, the barrels, the polearm rack and the leaning shields. The posts stand 4 m apart. There are about 290k triangles; the budget for this package is 350k, and the build asserts it.
- **Structure:** two front oak posts with finials at 4.35 m, iron bands with rivet heads, and jute rope lashings. A blue/cream nine-stripe canvas shed awning runs from the 3.7 m rear rail to the 3.0 m front rail and ends in a tattered valance. On the left, a braced arm carries the stag-and-fleur banner on chains. On the right, a braced arm carries a hexagonal iron lantern on a chain; its glass is the `SB_ArmorVendor_Glow` pane and `light_socket_m` marks its centre.
- **Counter:** 1.2 m high, with a plank top, apron, iron corner plates and an X-brace. The blue counter drape is a stag pennant with a gilt border and fringe. Fur pelts hang over the counter edge on both sides.
- **Stock, modelled here as closed thin shells:**
  - Three plate harnesses on wooden stands. Each has a cuirass with a medial ridge, three fauld lames, laminated pauldrons, arms, a gorget, a belt and brass rivets, and wears a helmet. One has mail sleeves, one has a mail skirt and one has a diagonal baldric. Two have blue cloth skirts under the tassets.
  - 17 helmets (great helms, pig-faced bascinets, sallets and barbutes), each with visor slits and breaths. They sit on a top shelf, a stepped riser, the counter and two crates.
  - A mail hauberk with pauldrons on a stand.
  - Three gauntlets and two vambraces.
  - Hanging belts with buckles and a coiled belt.
  - A painted heater shield with a gilt stag and fleur, and a round plank shield with a blue segment pattern, an iron rim and a boss.
  - A rack of a halberd, two spears, a glaive and a bardiche.
  - A steel bucket of greaves, plus greaves stacked in a chest.
- **Poly Haven models** (kit.model, own UVs and `SB_PH_*` materials):
  - `wine_barrel_01`: four barrels, slimmed to 0.55 × 0.92 m.
  - `wooden_crate_01` and `wooden_crate_02`: crates.
  - `wooden_bucket_01`, holding `wooden_handle_saber` and an `ornate_medieval_dagger`.
  - In the rack: `antique_estoc`, `wooden_axe_02`, `ornate_war_hammer` and `ornate_medieval_mace`.
  - A second `ornate_medieval_dagger` lies on the counter.
- **Inferred (not in the concept):**
  - The 1.6 m depth, the rear posts and the boarded back wall.
  - The short side drops and the top shelf on brackets.
  - The arrangement of everything behind the front plane.
  - The armour's back plates.

## Materials

Every scanned surface uses a CC0 Poly Haven scan at its measured physical size (`../materials-provenance.json`, maps in `../Textures`). New scans downloaded for this package: `brown_leather`, `curly_teddy_natural` and `terlenka`.

| Surface | Material | Scan |
|---|---|---|
| Posts, rails, counter, shelves, stands, back boards, rack, shafts | `SB_OakTimber` | `weathered_brown_planks` |
| Blue stripes, side drops | `SB_ArmorVendor_AwningBlue` | `denim_fabric_04`, darkened |
| Cream stripes | `SB_ArmorVendor_AwningCream` | `terlenka`, warmed and aged |
| Banner, drape, heater shield face, cloth skirts | `SB_ArmorVendor_Banner` | `denim_fabric_03`, darkened to navy |
| Rope lashings | `SB_ArmorVendor_Rope` | `hessian_380`, darkened |
| Belts, baldric, straps | `SB_ArmorVendor_Leather` | `brown_leather` |
| Pelts | `SB_ArmorVendor_Fur` | `curly_teddy_natural`, darkened to grey-brown |
| Round shield segments | `SB_ArmorVendor_PaintedPlanks` | `blue_painted_planks` |

Plain parametric materials:

- Plate armour: `SB_ArmorVendor_Steel` (bright) and `SB_ArmorVendor_SteelWorn` (dark).
- Mail: `SB_ArmorVendor_Mail` and `SB_ArmorVendor_MailShadow`.
- Rivets and trim: `SB_ArmorVendor_Brass`.
- Heraldry: `SB_ArmorVendor_Gilt`.
- Bands, chains and the lantern frame: `SB_ForgedIron` (Cottage1's arguments).
- Lantern glass: `SB_ArmorVendor_Glow` (emissive).

The prop models carry their own `SB_PH_*` scan materials.

## Limits

- **Unreal:** not imported yet; `../import_silverbrook.py` owns the import. Put a point light at `light_socket_m`.
- **Mail:** there is no Poly Haven chain-mail scan. Mail is a displaced grid with alternating ring courses, and its faces alternate between `Mail` and `MailShadow`. It reads as mail at stall distance, but close up it shows a regular checker.
- **Steel:** steel is parametric (metal 1, roughness 0.34–0.5), not a scan. The concept's etched decoration on the breastplates is not modelled; it is suggested by brass rivets and gilt edges.
- **Prop placement:** the four rack props (estoc, axe, war hammer, mace) stand on the ground leaning into the rack by eye, not by contact solving. The saber and dagger stand inside the bucket.
- **Cloth:** the cloth, pelts and the hauberk are two-sided closed thin solids (6–14 mm). Torn hems are jagged edge lines, not holes.
- **Shader round-trip:** the Blender shader graph does not round-trip to FBX/GLB. The exports carry UVs, the `UVMap`/`WorldUV` layers, material slots and the `Weathering` vertex colours, and the review checks that the FBX texture references resolve. The GLB is exported without images.
