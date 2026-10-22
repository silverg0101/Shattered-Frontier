# Gilded Tankard: Unreal stage (single-building dev map)

Worktree `agent-a257612538b402df2`, rebased on `origin/main`. Package: Blender review r5 PASS (`../REVIEW-blender-r5.md`). Round 1 review: `REVIEW-r1.md` (3 PASS / 8 FAIL). Round 2: `REVIEW-r2.md` (8 PASS / 3 FAIL); its fixes 1 (doorstep 1.5 m wide, `build_gilded_tankard.py` doorstep box) and 2 (gilt `(.90,.66,.22)`, roughness .3, metal .6) applied, map recreated and redressed, gates rerun with the same lines. Round 3: `REVIEW-r3.md` (4 PASS / 7 FAIL); fixes applied: opt-in manifest `footings_cm` read by `silverbrook_props.dress_footings` (`<House>_Footing<k>` under the external chimney), baked doorstep removed (`STEP` 0, `DS` 1.7, stile to z 0.02), round gilt tankard `vessel` through the board, right-wall ground-storey light. Map recreated; gates as above except `DEFAULT_MATERIAL_CHECK visible_in_town 26` (the footing). Round 4: `REVIEW-r4.md` (9 PASS / 2 FAIL); fixes applied: tankard `vessel(..., segments=96)` with a domed lid, gilt `(.60,.38,.09)`; re-imported, gates unchanged, recaptured. Round 5: `REVIEW.md`.

Run order (PowerShell, worktree root):
1. `pwsh tools/Link-WorktreeDeps.ps1` -> `linked: Artifacts\ThirdParty`, editor MCP port 8747.
2. `Build.bat ShatteredFrontierEditor Win64 Development -Project=<worktree>\GameClient\ShatteredFrontier.uproject -WaitMutex -NoHotReloadFromIDE` -> `Result: Succeeded`.
3. Editor of this checkout started with `Start-Process -PassThru`, stopped by its own PID.
4. `py -3 tools/Run-InEditor.py GameClient/ArtSource/Silverbrook/import_silverbrook.py --package-only GildedTankard` -> `SILVERBROOK_IMPORT` 10.59 x 8.93 x 12.07 m, 12 slots, Nanite, convex collision, `nanite_fallback_triangles 50274`; parts Door1 (swing), Barrels and Bench (static). Shared `MI_SB_*` resaves reverted with `git checkout`.
5. `py -3 tools/Run-InEditor.py tools/Create-BuildingDevMap.py GildedTankard` -> `DEV_MAP_READY /Game/Frontier/Maps/Dev/GildedTankard/GildedTankard_Review` (dev-map sun yaw -135, r1 fix 3).
6. `py -3 tools/Run-InEditor.py GameClient/ArtSource/Silverbrook/silverbrook_props.py --folder Districts/Dev --terrain Terrain` -> `HOUSE_FOUNDATIONS 1 CREATED 1`; `HOUSE_DETAILS {'gardens': 1, 'crops': 10, 'pots': 0, 'stoop_pots': 0, 'firewood': 0, 'smoking': 0, 'signs': 0, 'washing_lines': 1, 'ladders': 1, 'sill_planters': 0, 'ivy': 1}`; then the level is saved explicitly.
7. Gates:
   - `tools/check_foundations.py --terrain Terrain --expect 1` -> `FOUNDATION_CHECK 1 failures 0 min_visible_cm 65.0 max_abs_gap_cm 0.0 proud_cm 4.0 4.0`
   - `tools/check_details.py --expect 0` -> `SILL_PLANTERS 0 expected 0 GROUP_DETAIL_OVERLAPS 0 []`
   - `GameClient/ArtSource/Greenlands/check_hidden_in_game.py --all` -> `HIDDEN_IN_GAME_CHECK checked 0 visible_in_game 0 []`, `DEFAULT_MATERIAL_CHECK visible_in_town 25 default_or_graybox 0 []`
   - `tools/check_ground.py --terrain Terrain` -> `DRESSING_GROUND {'checked': 18, 'floating': 0, 'floating_over_5_cm': [], 'max_positive_gap_cm': 0}` (16 dressing props + the Barrels and Bench parts)
8. Capture (editor closed): `UnrealEditor.exe <uproject> /Game/Frontier/Maps/Dev/GildedTankard/GildedTankard_Review -game -RenderOffscreen -unattended -nosound -NoSplash -ResX=1920 -ResY=1080 "-ExecCmds=py <worktree>/tools/capture_building.py GildedTankard <this folder>"`: five building views plus six context views (two overviews, four seeded ground spots).
9. `py -3 make_pairs.py` -> `<view>-compare.png` (concept left, live right).

## r1 fix list: what was done
1. Foundation apron: **not as written.** The proposed `_base_box` filter left the Tankard's box unchanged (the barrels sit under the roof overhang, so "mesh above" is always true) and moved four other packages (Adventurers' Guild x-min -670 -> +740 cm, Armor/Weapons vendors, Granary); reverted. Cause: the footing pass lifts the house 65 cm onto its plinth, so barrels and bench baked into the house mesh needed the apron to stand on. Done instead: barrels and bench are separate `static` parts (`build_gilded_tankard.py`), placed by `place_parts.attach` and set on the terrain by `silverbrook_props.dress_house_details`; the house-filter loops skip `static` parts; the package declares its ashlar footprint as manifest `base_cm` (opt-in, used by `_base_box`; traced on the imported mesh to 0.1 cm); `check_foundations.py` uses `_base_box` and reloads `silverbrook_props` (the editor kept a stale module between runs).
2. Second sign: done (`baked_sign` from a `*SignBoard` slot skips `ShopSign_`).
3. Dev-map sun yaw -135: done in `tools/Create-BuildingDevMap.py`.
4. `sign_m` in the manifest, read by `capture_building.py`: done.
5. Lantern cage: done as an **opt-in** `kit.lantern(..., cage=True)` (default unchanged; Blacksmith rebuild identical), glow strength 1.5 on a package-local `SB_GildedTankard_LanternGlow`.
6. Glass strength 1.2: done on a package-local `SB_GildedTankard_LitGlass`, so the shared `MI_SB_LitGlass` is not rewritten.
7. Washing line: it was counted but the level had not been saved after it spawned; the pass now saves the level explicitly, and the line shows in `rear`.
8. Gilt colour: conditional item, not applied.
9. Crops inside the fence: done (`silverbrook_props.py`).
Also found: the `-game -RenderOffscreen` capture draws the Nanite **fallback** mesh; at the Auto setting (15 k of 51 k triangles) it erased the gilt emblem. Opt-in manifest `nanite_fallback_percent: 1.0`, read by `import_silverbrook.import_mesh`, gives a full-detail fallback for this package.

No verdict here; the reviewer writes `REVIEW.md`.
