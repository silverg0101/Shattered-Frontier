# Buildings blueprint: houses and other structures

Scope: one Silverbrook building package (Blender -> FBX -> `/Game/Frontier/Environment/Greenlands/Silverbrook/`), its foundation, surface materials, relief, signs and per-house variation; the single-building dev-map path for a worktree session; registration in the town. Dressing around the house: [props-and-dressing.md](props-and-dressing.md). Depth: [GFX-03](../decisions/GFX-03-surface-detail-and-buildings.md). Shared rules, worktree setup, capture and commit: [README.md](README.md).

## 1. Pipeline (in order)
| # | Step | Command / file | Done when |
|---|---|---|---|
| 1 | Concept | §2 prompt -> `docs/Game-Design/concept-art/Silverbrook/<Display Name>.png` (display name with spaces, as the existing files: `The Golden Stag Inn.png`, `Northridge House-1.png`); add a row to that folder's README table (Image, Building, Asset ID `BLD-Greenlands<PascalName>-01`, Fits w x d x h, package, status) | file exists; copied to `GameClient/ArtSource/Silverbrook/<PascalName>/<PascalName>-reference.png` (SHA-256 goes into `asset-manifest.json` by the build) |
| 2 | Scans | `py -3 GameClient/ArtSource/Silverbrook/acquire_materials.py <polyhaven id> ...` only for a scan the kit does not have (`materials-provenance.json`); realistic scans only | provenance rows present |
| 3 | Build | from the package folder in PowerShell: `& 'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe' -b --factory-startup -P build_<name>.py` | `Silverbrook<PascalName>.{blend,fbx,glb}`, `_Door1.fbx` per leaf, `asset-manifest.json`; last line `<NAME>_BUILT [w, d, h] <tris>`; the script asserts `SLOT` and the triangle budget |
| 4 | Fresh-process review | `& '...\blender.exe' -b --factory-startup Silverbrook<PascalName>.blend -P review_<name>.py` | `<PascalName>-{Beauty,Front,Side,Rear}.png`, `exchange-review.json` (FBX textures resolve) |
| 5 | Render review | you spawn the reviewer: `Agent` with `subagent_type` general-purpose, `model: fable`, prompt = the brief in [docs/visual-review.md](../visual-review.md) with the four renders, the concept path and this package folder. Never judge your own renders | reviewer writes `<pkg>/review/REVIEW-blender.md`, all PASS |
| 6 | Import (open editor of this checkout) | `py -3 tools/Run-InEditor.py GameClient/ArtSource/Silverbrook/import_silverbrook.py <PascalName>` | `SM_Silverbrook<PascalName>`, `SM_Silverbrook<PascalName>_Door1`, `ue-import-report.json`; then revert the shared-asset side effects (README "Assets") |
| 7 | Place for review | single building: §6 dev map; town: §7 registration + `build_silverbrook.py --discard-unsaved` | `SILVERBROOK_BUILT` or `DEV_MAP_READY` |
| 8 | Gates | §6 single-building values or the README table for the town | pass lines printed |
| 9 | Capture + review | §6 views (dev map) or `house-frontage`/`foundation-close` (town); pairs; fresh Fable agent as in step 5 | `REVIEW.md` all PASS |

Package conventions (`silverbrook_kit.py`): metres, Z up, **front faces -Y**, origin at ground centre; `kit.scan(name, polyhaven_id, dark=, lichen=, normal=, damp=, aged=)` at measured physical size, `kit.solid(name, rgb, rough, metal)` for flat palettes (GFX-03 §2.4 retires them), `kit.model()` for Poly Haven props; `wall(G, mat, c, o, half, height, openings, th=.3, arch=)`, `window(G, M, c, o, w, h, shutters, planter, wall_th, band)`, `door(G, M, c, o, w, spring, stone_rects, ...)`, `lantern(G, M, top, arm=)`, `tile_plane(...)` size (.3,.42,.022) exposure .2, `deck(...)`; single-element `rect` sampling for one slate / stone / beam; weathering in vertex R (damp below 0.65 m) and G (lichen). Door leaves are split by `kit.split_doors(str(OUT), 'SM_Silverbrook<PascalName>')` (own FBX, origin at the hinge) and attached by `place_parts.attach` after spawn.
Import facts: the FBX import **flips Y** (front becomes +Y; `build_silverbrook.model()` turns the actor a further 180 deg) and **recentres each mesh on its bounds** (Townhouse1 +26 cm x, -18 cm y). Trace attach coordinates (sills, sockets, stacks) on the **imported** mesh, never from the Blender file.

## 2. Concept sheet prompt (Codex; fill the <> fields)
Write `GameClient/ArtSource/Silverbrook/<PascalName>/concept/prompt.txt`:
```
Generate one image with your image generation tool: Medieval <building type> "<Display Name>" for the town of Silverbrook, <storeys and materials: e.g. two storeys of dark oak timber framing over an ashlar stone ground floor, slate roof>, <signature elements: hanging sign, door type, windows, chimney, lean-to>. Asset sheet style: front elevation as the main view, a 3/4 view beside it, neutral light grey background, no people, no scene, no ground shadow beyond a soft contact shadow, realistic weathered materials, no text labels. Match the weathered timber, stone and slate look and the colour palette of the Silverbrook design board; reference the <TOWN STREETS | TOWN OVERVIEW | LAMPS & STREETSIGNS> tile of docs/Game-Design/concept-art/Silverbrook-Design-Board-v1.webp and the style of docs/Game-Design/concept-art/Silverbrook/<nearest existing concept>.png. Save it as <PascalName>.png in the current directory.
```
Run from that `concept/` folder: `codex exec --skip-git-repo-check -s workspace-write -o codex-last-message.txt - < prompt.txt` (stdin redirect; the `cat | codex` form is refused by the command guard). Codex writes the image under `~/.codex/generated_images/` and, with the "Save it as" sentence, copies it into the cwd. If `<PascalName>.png` is missing: `Get-ChildItem ~/.codex/generated_images -Recurse -Filter *.png | Sort-Object LastWriteTime | Select-Object -Last 1 | Copy-Item -Destination .\<PascalName>.png`. Then copy it to `docs/Game-Design/concept-art/Silverbrook/<Display Name>.png` and to `../<PascalName>-reference.png`; commit `prompt.txt` and `codex-last-message.txt` with the package.

## 3. Dimensions
- **SLOT (w, d, h in m)** = `silverbrook-town-layout.json` `building_types[<kind>]`. For a new kind add the row there and use the class table: cottage / workshop 8x6x6; townhouse 8x7x9; farmhouse / stables 12x8x7; Northridge house 12x10x10; inn / tavern / blacksmith / shop 14x12x14; guild 16x13x18; temple 14x24x20; town hall 22x18x28; watchtower 8x8x9; market stall 4x4x4. The whole model including eaves, sign, finial and pots must fit (`assert` in the build).
- Read the concept by proportion: the door head is 2.1-2.3 m; measure every other height in door units on the front elevation. Keep the concept's width; compress storeys to fit `SLOT` height (Townhouse1: concept drawn 10 m, built 8.97 m).
| Value | Default range | Passing examples |
|---|---|---|
| Wall half-extents `W2, D2` | footprint 0.6-0.8 of SLOT w/d; wall thickness 0.3 | Townhouse1 2.85/2.9; Blacksmith 5.2/3.8; Tankard 4.5/3.2 |
| Ground storey | 3.0-3.8 m (stone or ashlar) | 3.3; 3.8 |
| Upper storey | 2.6-3.0 m (timber/plaster, jetty 0.3-0.5 m optional) | 2.6 |
| `EAVE` | storeys + 0.2-0.4 m plate; overhang 0.35-0.4 m (`RX = W2 + .4`) | 5.75; 5.4; 6.4 |
| `RIDGE` | roof pitch 40-55 deg: `EAVE + (W2 + .4) * tan(pitch)` | 8.3; 11.6 (hip); 10.0 (gable) |
| Door | single `w` 0.9, spring (`DS`) 1.55 above the step (`STEP` 0.3); double `w` 2.6, spring 2.0; round-headed via `arch=` | Townhouse1 0.9/1.55/0.3; Tankard 2.6/2.0/0.2 |
| Windows | 0.7-0.8 wide x 1.0-1.2 tall, sill `WZ` 1.0-1.3 m; shutters on street windows | Townhouse1 0.7x1.2 at 1.0; Blacksmith 0.55x0.65 |
| Lanterns | `lantern(G, M, top, arm=)` one each side of the door, top at 2.5-2.6 m, 0.4 m off the wall | Townhouse1 (-2.42, -D2-.42, 2.55) |
| Chimney | stack 0.9x0.6 m, cap 12 cm, two pots; soot on the top 0.4 m | GFX-03 §3.1 |

## 4. Copy-this-building recipe
Reference packages: **Blacksmith** (public building: inn, tavern, shop, with a sign), **Townhouse1** (street house with bay, stoop, lantern), **Cottage1** (small house). Copy `build_<ref>.py` -> `build_<name>.py`, `review_<ref>.py` -> `review_<name>.py`, `README.md`; delete the reference's `.blend/.fbx/.glb/.png` from the copy. Then change, and nothing else:
1. Docstring: concept path, slot, one-paragraph interpretation.
2. `kit.reset(<new 4-digit seed>)`; `REF = HERE.parents[3] / 'docs/Game-Design/concept-art/Silverbrook/<Display Name>.png'` (`parents[3]` is the repo root from `GameClient/ArtSource/Silverbrook/<Pkg>/`; `parents[2]` is `GameClient/` and fails the copy).
3. Collection `Silverbrook<PascalName>`; every `Geo('SM_SB<PascalName>_<Part>', asset)`; `SLOT`; the measured layout block (§3).
4. Materials dict `M`: keep the kit names (`SB_Slate`, `SB_Ashlar`, `SB_LimePlaster`, `SB_OakTimber`, `SB_DoorBoards`, `SB_ForgedIron`, `SB_LeadCame`, `SB_LitGlass`, `SB_LanternGlow`); a package-specific material is `SB_<PascalName>_<Thing>` (banner, sign board, gilt emblem).
5. Export block: `kit.split_doors(str(OUT), 'SM_Silverbrook<PascalName>')`, manifest `asset='Silverbrook<PascalName>'`, print tag `<NAME>_BUILT`; triangle assert stays `tris <= 120000`.
6. Review script: blend file name, collection name, render prefix `<PascalName>-`, print tag.
7. README: reference, commands, interpretation, "Not modelled" list.
Triangle budget: **120 k per house/shop package** (the script assert; Townhouse1 71 k, Blacksmith 80 k, Tankard 34 k). GFX-03's 400 k is the ceiling for large civic packages after K1 relief; do not raise the assert for a house.

## 5. Signs and hanging signs
| Rule | Value |
|---|---|
| Who gets a baked sign | inns, taverns, shops, guild, smithy: the package bakes its sign in `G['detail']`. Houses get none; the dressing pass hangs `SM_SilverbrookPropShopSign` on 40 % of townhouses (never both on one building) |
| Bracket | iron arm `beam` 5x5 cm from the wall face, 1.2-1.5 m long, at z 3.45-3.75 m (above the door head, below the upper sills); diagonal brace 3.5x3.5 cm from 0.85 m below; wall plate box 6x10x100 cm; 8-segment iron scroll r 0.17 m under the arm (Golden Stag Inn lines 218-227) |
| Board | 0.8 m wide x 0.8-1.15 m tall, 5 cm thick, pentagon or shield outline, hung on two 2 cm iron rods 0.64 m apart; bottom >= 2.3 m above the step; board material `kit.scan('SB_<Pkg>_SignBoard', 'old_planks_02', dark=(.10,.10,.11), damp=False, aged=.8)`; emblem both faces, gilt `kit.solid('SB_<Pkg>_Gilt', (.62,.45,.18), .4, .85)` (stag, anvil, tankard, loaf) |
| Banner variant | navy linen `kit.scan('SB_<Pkg>_Banner', 'denim_fabric_03', dark=(.12,.14,.30), damp=False, aged=.75)` 1.2 x 1.6 m on an iron rod with crossed straps (Blacksmith 223-231, Townhouse1 277-297) |
| Position | on the street face beside the door or on the left side wall at the corner; clear of the door swing and of the lantern arms; projecting <= 1.5 m so it stays inside `SLOT` |
| Text | none; the emblem is the sign (no readable lettering in textures) |

## 6. Single-building path (worktree dev map)
Rule from the dependency map: a feature worktree works in its own map under `/Game/Frontier/Maps/Dev/<Feature>/` and never edits the Silverbrook map or shared art; Silverbrook wiring happens later in the main editor lane (§7).
1. Worktree setup, build and editor: README "Worktrees".
2. Dev map `/Game/Frontier/Maps/Dev/<PascalName>/<PascalName>_Review`: copy `tools/Create-BuildModeDevMap.py` to `tools/Create-BuildingDevMap.py` and run `py -3 tools/Run-InEditor.py tools/Create-BuildingDevMap.py <PascalName>` -> `DEV_MAP_READY`. Contents: 60x60 m flat floor labelled `Terrain` (cube scale (60,60,1), top at z 0, `BlockAll`, `MI_GN_Ground`), `PlayerStart` at (0, -1500, 110) facing +Y, `lighting.apply()` (sun, sky, fog, post from `silverbrook-lighting.json`), the building `SM_Silverbrook<PascalName>` at (0,0,0) yaw 0 in folder `Districts/Dev` (so the footing, gate and detail scripts select it), parts via `place_parts.attach`.
3. Footing and details: `py -3 tools/Run-InEditor.py GameClient/ArtSource/Silverbrook/silverbrook_props.py --folder Districts/Dev --terrain Terrain` -> `HOUSE_FOUNDATIONS 1`, `HOUSE_DETAILS {...}`.
4. Gates (single building): `check_foundations.py --terrain Terrain` -> `FOUNDATION_CHECK 1 failures 0 min_visible_cm 65.0 max_abs_gap_cm 0.0 proud_cm 4.0 4.0` (visible 50-70, buried >= 100, proud 0-8, gap 0); `check_details.py` -> `SILL_PLANTERS n expected n GROUP_DETAIL_OVERLAPS 0 []` with n = the package's bay count from its manifest (0 if none); `check_hidden_in_game.py --all` -> `HIDDEN_IN_GAME_CHECK checked N visible_in_game 0 []` and `DEFAULT_MATERIAL_CHECK visible_in_town N default_or_graybox 0 []`; `check_ground.py` -> `'floating': 0`.
5. Capture (`-game`, editor closed, README capture line with the dev map path and `tools/capture_building.py <PascalName> <out>`). Views from the imported actor's bounds (W, D = footprint, front = +Y after import, origin at the floor): `frontage` eye (0.55 W, D/2 + 5.3, 2.2) -> look (0, D/2, 2.4) FOV 72.4; `foundation-close` eye (0.55 W, D/2 + 5.3, 1.0) -> (0, D/2, 0.65) FOV 54.1; `three-quarter` eye (W + 6, D/2 + 12, 3.0) -> (0, 0, 0.45 h) FOV 60; `rear` eye (-0.4 W, -(D/2 + 12), 2.0) -> (0, -D/2, 3.0) FOV 60; `sign-close` eye 3 m from the sign board at its height, FOV 45. Sight-line assert as in the town script.
6. Pairs: concept left, live right (`make_pairs.py` copy); evidence folder `docs/evidence/graphics/<date>-<pascalname>/` with README (commands, gate lines, deviations); fresh Fable agent reviews; fix only its list.
7. Commit only the package folder, the concept PNG + README row, the dev-map asset, the evidence folder and `tools/` additions; revert resaved shared assets (README "Assets").

### Building tooling (implemented)
1. `tools/Create-BuildingDevMap.py <PascalName>` (copy of `tools/Create-BuildModeDevMap.py`): map path `/Game/Frontier/Maps/Dev/<PascalName>/<PascalName>_Review`; floor actor labelled `Terrain` with `MI_GN_Ground`; `PlayerStart`; `import lighting; lighting.apply()`; spawn `SM_Silverbrook<PascalName>` at the origin in folder `Districts/Dev` and call `place_parts.attach(actor)`; refuse to overwrite; print `DEV_MAP_READY <map>`.
2. `GameClient/ArtSource/Greenlands/pcg_dressing.py` `_terrain()`: when no `SM_Silverbrook_Terrain` actor exists, return the StaticMeshActor labelled `Terrain`. `GameClient/ArtSource/Silverbrook/silverbrook_props.py`: `dress_footings(c, folder='Districts/')`, `dress_house_details(folder='Districts/')`, `dress_all(folder='Districts/')` plus `--folder` / `--terrain` argv handling in `__main__`; `dress_house_details` reads the `sills` list from each package's `asset-manifest.json` (`sills: [[x, y, z], ...]` mesh-local cm, traced on the imported mesh) instead of the hard-coded three-package table.
3. `docs/evidence/graphics/2026-10-21-wp6-town-f/check_foundations.py` -> `tools/check_foundations.py --terrain <label> --expect <n>` (terrain by label, default `SM_Silverbrook_Terrain`; expected house count); `check_details.py --expect <n>` with `per` built from the manifests' `sills`; `check_hidden_in_game.py --all` to run `DEFAULT_MATERIAL_CHECK` over every visible StaticMeshActor when the map has no wall polygon; `tools/capture_building.py <PascalName> <out dir>` with the §6.5 views computed from `get_actor_bounds`, the sight-line assert and `HighResShot 1920x1080`.
4. `GameClient/ArtSource/Silverbrook/import_silverbrook.py --package-only`: skip `save_loaded_asset` for `M_SB_Scan`, `M_SB_Solid`, the shared textures and every `MI_SB_*` not named in the given package's manifest, and print `RESAVED <asset>` for anything it does save.

## 7. Town registration (replaces "runbook §8"; that section is only the table of Silverbrook-only hard-coded values)
1. `GameClient/ArtSource/Greenlands/silverbrook-town-layout.json`: `building_types.<kind>` = `[w, d, h]` and at least one lot or place of that `kind`.
2. `GameClient/ArtSource/Greenlands/greenlands-look-profile.json` `buildings["<Display Name>"].have = ["SM_Silverbrook<PascalName>"]`.
3. `GameClient/ArtSource/Greenlands/build_silverbrook.py` `KIND_BUILDING[<kind>] = "<Display Name>"` (line 47 table); `python GameClient/ArtSource/Greenlands/missing_assets.py` reports no stand-in for it.
4. Main editor lane only: `build_silverbrook.py --discard-unsaved`, `paint_street_edges.py`, the README gate table, the five town captures, fresh Fable review.

## 8. Foundations (owner: "every house foundation is too short")
`silverbrook-props.json` `footing`: `visible_cm` **65** (50-70), `burial_cm` **100** (100-150), `proud_cm` **4** (0-8), `base_band_cm` 25; `M_SB_FoundationStone` (`dress_footings`) roughness **0.86**, matte dark rubble, world-aligned; plinth sized from the mesh's own wall base (`_base_box`), top on the highest terrain under the footprint, bottom below the lowest; stoop steps `<House>_Stoop0..k` from the doorstep to the street. Gate thresholds: `|gap| <= 0.1 cm`, `min_visible >= 50`, `min_buried >= 100`, `-0.5 <= proud <= 8`. GFX-03 K1 moves the plinth into the kit (`plinth(height=0.6, proud=0.06, bevel=0.03)`).

## 9. Surfaces, relief, variation (GFX-03; K0-K2 not yet run: `import_silverbrook.py` `DISPLACEMENT_CM=0`)
| Rule | Value |
|---|---|
| Technique | geometry for anything touched or in silhouette (slates, quoins, beams, plinth, ridge, chimney); Nanite tessellation for ashlar/rubble/plaster/thatch; normal + AO only for cloth, iron, lead, glass, props < 40 cm |
| Edge mask before any displacement | displaceable quads inset 3 cm (`Geo.poly`), vertex B = 1 inside / 0 on the rim, A = 1; `disp = 0.5 + (Height - HeightMean) * DisplacementScale * B`, magnitude 3 cm. **Never** enable tessellation without it (`Cottage1/review/displacement-on6cm.png` cracks) |
| `relief` | ashlar 0.8, rubble 1.0, plaster 0.3, slate 0, thatch 1.0, timber 0, paving 0.7 |
| Texel density | >= 512 px/m within 1.5 m of the player, >= 256 elsewhere; replace, never stretch, a scan |
| PBR | `Diffuse, nor_gl, arm (AO R, rough G, metal B), Displacement`; `M_SB_Scan` v2 macro grunge 7.3 m, detail normal 0.12 m, world-aligned moss/streaks/edge wear; no `kit.solid()` without a normal map after K2 |
| Glass | `M_SB_Glass` opaque, roughness 0.06, emissive (1.0,0.5,0.14) x 2 x `MPC_Silverbrook.NightLit` |
| Tones | `vary_house_finishes`: five plaster `[(1.00,.93,.80),(.84,.86,.88),(1.00,.86,.66),(.93,.95,.84),(.78,.74,.70)]` and slate `[(.78,.86,1.02),(1.06,.90,.74),(.86,.98,.88),(.66,.68,.72),(1.00,1.00,1.00)]` tones by `_h(label+'tone')`; **>= 20 % luminance spread** (0.96-1.04 tints are invisible) |
| Packages | `model()` picks `crc32(label) % len(pool)` and skips a repeat of `_last_mesh[kind]`: no two adjacent lots share a mesh |
| Per-house custom data (spec) | six floats: `InstanceTint` RGB, `InstanceWear` by district 0.3-0.8, `InstanceTone`; set on parts too |
| Budget | +2.0 ms median at bookmarks 06/07 |

## 10. Known failure -> rule
| Failure | Rule |
|---|---|
| 20 cm glossy plank skirt, slit under the wall | §8 values; matte stone; proud on all sides; steps at the door |
| Smeared pale pier block beside the door | every kit box gets box-projected UVs at the scan tile or the plinth material |
| Sill planters floating in front of the bay | trace on the imported mesh; gate trace from 1.5 m in front at sill + 15 cm must not hit the house |
| "only Townhouse6 has bay sills" | read every `build_*.py` for bays; put `sills` in the manifest |
| Side window reads as a void | glass 13 cm behind the face is invisible at 80 deg; trace at local y -100 (import flips Y) before changing the mesh |
| White box in the plaza | authoring boxes hidden in game; `DEFAULT_MATERIAL_CHECK ... 0` |
| Same two meshes, same yaw, tints < 10 % apart | §9 tones and package rule |
| Import dirtied 49 shared `MI_SB_*`/`M_SB_*` assets | revert them (README "Assets"); commit only the package's own assets |
| Build script copied with `parents[2]` | `REF = HERE.parents[3] / ...` |
| 400 k used as the budget | the assert is 120 k for houses and shops |
| Ran the import against another checkout's editor | `Run-InEditor.py` targets only the editor opened from its own checkout; open that editor (README "Worktrees") |
| Ashlar repeats, flat plaster, windows glow at noon | GFX-03 §2, deferred in every WP6 review; not per-house patches |

## 11. Concept refs
`concept-art/Silverbrook/<Display Name>.png` per package, `concept-art/Silverbrook-Design-Board-v1.webp` (TOWN STREETS tile, townhouse elevation, LAMPS & STREETSIGNS), `concept-art/UI/v5/gameplay-hud.png`; owner wall shot = GFX-03 bookmark 06 `inn-wall`, street shot = 07 `market-street`.
