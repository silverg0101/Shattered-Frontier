# Props and dressing blueprint

Scope: (A) one street or house prop from a Codex sheet to `SM_SilverbrookProp<Name>`; (B) the Silverbrook town dressing pass that places footings, verges, street groups and house details. Buildings themselves: [buildings.md](buildings.md). Shared rules: [README.md](README.md). Evidence of the passing pass: `docs/evidence/graphics/2026-10-21-wp6-town-{e,f}/` (main-street, house-frontage, foundation-close PASS).

## A. A prop from a Codex sheet (in order)
| # | Step | Command / value | Done when |
|---|---|---|---|
| 1 | Sheet | Codex (README routing): "Medieval <prop>, <materials>, 3/4 view asset sheet with front/side/top, neutral grey background, reference the <TILE> tile of the Silverbrook design board" -> `GameClient/ArtSource/Silverbrook/Prop<Name>/concept/<name>-sheet.png` | one sheet, no scene |
| 2 | Build | copy `PropFlowerTub/build_propflowertub.py` (or `PropRainBarrel` for a Poly Haven model base); `blender.exe -b --factory-startup -P build_prop<name>.py` | `SilverbrookProp<Name>.{blend,fbx,glb}`, `asset-manifest.json` with `placement` and `slot_m` (bounds rounded up to 0.1 m, asserted) |
| 3 | Render review | `blender.exe -b --factory-startup SilverbrookProp<Name>.blend -P review_prop<name>.py` -> `Prop<Name>-{Beauty,Front,Side,Rear}.png`; fresh Fable judges vs the sheet at the distance it is designed for (3 m street props) | PASS |
| 4 | Import | `py -3 tools/Run-InEditor.py GameClient/ArtSource/Silverbrook/import_silverbrook.py Prop<Name>` | `SM_SilverbrookProp<Name>`, no default slot |
| 5 | Place | add the mesh to `silverbrook-props.json` (`street_groups` meshes, a `props` rule with `spacing_m/offset_m/clear_m`, or the `verge` list) or to `silverbrook_props.dress_house_details` | `apply_dressing.py` run (§B) and gates pass |

Prop values that passed: metres, Z up, front -Y, **origin bottom centre** (mount point for wall props); 1k-2k albedo/normal/ORM from kit scans; triangles <= 8 k (FlowerTub 5.6 k, RainBarrel 11 k with a Poly Haven model); materials `kit.scan` / `kit.solid` / `kit.model` only (FlowerTub: `SB_OakTimber` = `weathered_brown_planks` dark (.32,.30,.28), `SB_ForgedIron` (.03,.026,.022) rough .6 metal .9, `SB_GeraniumBloom` (.5,.02,.02) with a `Pink` variant via `sheet.add_variants`). Delivered: Bench, FenceSection, FirewoodStack, FlowerTub, Grindstone, HayBales, HayCart, HitchingPost, IvyClump, Ladder, MarketGoods, NoticeBoard, RainBarrel, SackPile, ShopSign, Signpost, SillPlanter, WashingLine, WaterTrough, Well, Wheelbarrow.

Codex texture for a prop (hay bale straw, WP6e PASS): Codex 2048x2048 tileable albedo + height ("cut face of a tightly packed golden straw bale, stalks 2-4 mm, dark gaps, orthographic top-down tileable, no twine") -> `PropHayBales/textures/straw-face{,-height}.png` -> `py -3 PropHayBales/make_straw_scan.py` (normal + roughness from height, 0.6 m tile, provenance row) -> `kit.scan('SB_PropHayBales_Straw','sb_straw_face', normal=1.0)`; twine `(.30,.22,.12)` 12 mm; 330 loose stalks 0.12-0.25 m; bales inset 2 cm so the stack has shadowed gaps.

## B. Town dressing pass (open Silverbrook editor; run order of the passing pass)
1. `py -3 tools/Run-InEditor.py GameClient/ArtSource/Silverbrook/import_silverbrook.py <new Prop packages>`
2. `py -3 tools/Run-InEditor.py docs/evidence/graphics/<pass>/apply_dressing.py` = `NatureFamilies/import_ground.py` (ground master), `VFX/StreetLamp/build_materials.build()` (smoke material + `T_ChimneySmoke` reimport), `silverbrook_props.dress_all()`, save level, `Greenlands/paint_street_edges.py`.
   `dress_all` order (fixed): `dress_footings` -> `hide_debug_actors` -> `vary_house_finishes` -> `dress_roads` -> `gate_windows` -> `dress_verge` -> `dress_props` -> **`dress_house_details` -> `dress_street_groups`** (details first so groups keep clear of tubs).
3. Gates: `check_foundations.py`, `GameClient/ArtSource/Greenlands/check_hidden_in_game.py`, `check_ground.py`, `check_details.py` (README table). Passing counts: 312 houses, 5,850 bases 0 floating, 1,500 groups, 158 door tubs, 158 stoop tubs, 162 washing lines, 140 sill planters, 85 smoking chimneys.
4. Capture `capture_game.py` (five views: `main-street` eye (29.0, 336, 1.7) look (4.5, -30, 4) FOV 70; `market` (30, -62, 3.0) -> (0, -30, 1.2) 70; `house-frontage` (22.23, 311.63, 2.2) -> (17, 315.9, 2.4) 72.4; `foundation-close` (22.23, 311.63, 1.0) -> (17, 315.9, 0.65) 54.1; `chimney-smoke` 14 m from the nearest card at card height, 1.5 m down, 55) then `make_pairs.py`; fresh Fable review.
A full rebuild (`build_silverbrook.py --discard-unsaved`) runs the same `dress_all` and paints the same fields; use it when a rule in `build_silverbrook.py` changed (row packages, earth width), `apply_dressing.py` otherwise.

### Placement values (`silverbrook-props.json`, `silverbrook_props.py`)
| Item | Value | Rule |
|---|---|---|
| Street groups | `spacing_m` 5 (3-9), `offset_m` 1.2 beyond the painted road edge, `clear_m` 0.55, `max_groups` 1500 (100-2000) | both sides of every station, each group a cluster of 2-3 meshes (main + 1-2 satellites 0.6-1.0 m along); gate `STREET_GROUPS total GATE need` with need = 6 per 50 m of street inside the wall (`assert total >= need`); every street with stations inside the wall gets groups (`assert min(road_counts.values()) > 0`); **cap shared per street by station count**, never first-come |
| Density target | 6-10 groups per 50 m, both margins, from the first metre of the view | a 20 m empty margin is a FAIL |
| House details | gardens on `h < .75` (L of three `FenceSection`, 10 crops in two rows at 0.6 m), firewood stack at the rear corner on `h < .5`, door tub 45 cm in front of the traced street wall beside the door, stoop tub at scale 0.75 on the top step, wall dressing on `_h(label+'wall') < .4` (shop sign at 2.6 m on the street face, ladder at 75 deg on a side wall, ivy clump at a plinth corner), sill planter on **every** bay (Townhouse1 (140,330,95), Townhouse3 (0,276,92), Townhouse6 (-120,260,84),(-90,280,435) mesh-local cm, traced on the imported mesh), chimney smoke on townhouses with `h < .4` | the planter loop runs outside the 40 % branch; every detail prop is registered in `_obstacles` (`FlowerTub, StoopTub, GardenFence, GardenFirewood, WashingLine, SillPlanter`, `_Stoop*`) |
| Washing lines | on `_h(label+'line') < .5`; between same-row neighbours (yaw within 15 deg, |oy| <= 2 m) when the free wall-base gap is **3.0-8.0 m** (rows are 5.1 m apart by construction: `2*r + 2.5`), line scaled `clamp((gap-100)/370, 0.8, 1.3)`; else rear yard on its own posts; posts and mid-point tested against every house's wall-base box + 50 cm | a "< 4 m" rule places zero lines; derive gap rules from the real lot pitch |
| Verge | `verge.meshes` without `SM_MossPatch_*` | moss discs on packed earth read as lawn |
| Tones / packages | [buildings.md](buildings.md) §4 | >= 20 % luminance spread; no adjacent repeat |
| Sight line | `capture_game.py` asserts no dressing prop or `StreetLamp*` (bounds sphere) within 1.5 m of the ray in the first 10 m; the framed house's own details are exempt | a lamp post or barrel filling a frame is a capture bug |
| Ground paint | [landscape-and-ground.md](landscape-and-ground.md) §2 | earth to the plinth line, cobble to both kerbs, plaza full weight |

## C. Known failure -> rule
| Failure | Rule |
|---|---|
| ~3 groups per 50 m, one mesh per group, one side only (WP6b) | §B values; clusters; both sides |
| 700-group cap spent on four primary streets, secondaries bare (WP6c) | share `max_groups` per street by its station count inside the wall |
| Camera inside a prop group / behind a lamp post / 0.5 m from a rain barrel (WP6b, c, d) | sight-line assert on every prop's bounds; move the eye, never delete the prop |
| Tub geometry inside a barrel (WP6d) | details before groups + obstacle rects |
| 0 washing lines placed (WP6c) | gap rule from measured row spacing (3-8 m) |
| Flower tub plant = brown twig clump at 0.35 scale (WP6c) | a dedicated prop with blooms (`PropFlowerTub`), never a scaled foliage clump |
| Hay bales = tan cardboard boxes (WP6d) | §A straw recipe: real stalk texture with height-derived normal, dark twine, loose stalks, gaps between bales |
| Ivy cards read as flat sheets at 1.5 m | accepted at 3 m; polish item, not a FAIL |
| Blooms are flat five-petal discs under 2 m (WP6e note) | next prop polish: umbels as 12-16 florets on a dome |
| Lawn between cobble and houses; firewood stores "in a field" (WP6b-d) | ground paint, not more props: landscape doc §2 |
| `ShowFlag.TextRender 0` in the capture script (WP6b) | never; `hide_debug_actors` + `check_hidden_in_game.py` |

## D. Concept refs
`concept-art/Silverbrook-Design-Board-v1.webp` tiles: TOWN STREETS, CARTS & SUPPLIES, FLOWERS & FOLIAGE, SEATING & DECOR, LAMPS & STREETSIGNS; `concept-art/Silverbrook/Street Props.png` (owner prop sheet); `concept-art/UI/v5/gameplay-hud.png` (lived-in street, smoke).
