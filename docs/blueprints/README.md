# Build blueprints (index)

One doc per asset kind. **Load only the doc for your asset**, plus this page. Each doc is a recipe a small model can follow without judgement calls: commands in order, exact values with file:key citations, gate commands with their expected output line, a copy-this-asset recipe for new work, and plain do/never rules from past failures. Depth lives in the specs they link; do not copy specs into the blueprints.

| Asset kind | Doc | Spec / decision for depth |
|---|---|---|
| Trees, shrubs, saplings, logs, stumps, boulders, forest density | [vegetation.md](vegetation.md) | `GameClient/ArtSource/Greenlands/NatureFamilies/README.md`, GFX-01 WP4 |
| Houses and other structures: kit, foundations, relief, materials, per-house variation | [buildings.md](buildings.md) | [GFX-03](../decisions/GFX-03-surface-detail-and-buildings.md) |
| Props from Codex sheets; town dressing placement | [props-and-dressing.md](props-and-dressing.md) | WP6 evidence `docs/evidence/graphics/2026-10-21-wp6-town-{b..f}/` |
| Landscape, mountains, ground layers and paint, grass and ground cover, roads, plazas, water, density targets | [landscape-and-ground.md](landscape-and-ground.md) | [GFX-02](../decisions/GFX-02-photoreal-ground-pipeline.md) |
| Sun, sky, fog, exposure, post, renderer tiers, shadows | [lighting-and-atmosphere.md](lighting-and-atmosphere.md) | [GFX-01](../decisions/GFX-01-graphics-quality-plan.md), GFX-02 §5 |
| Lamps, flames, smoke, fire | [vfx.md](vfx.md) | `GameClient/ArtSource/VFX/StreetLamp/README.md`, `docs/evidence/graphics/lamp-fx-3/REVIEW.md` |
| HUD and screens: kit, nine-slice, typography, layout | [ui-hud.md](ui-hud.md) | `docs/Game-Design/concept-art/UI/v5/{REVIEW,IMPLEMENTATION-FIX}.md` |
| Building-mode claim grid: state mask, decal material, HUD claim row, captures | [building-mode.md](building-mode.md) | [CLM-01](../decisions/CLM-01-claim-points-and-build-grid.md) §5 |
| Menus and panels: canonical controls, pixel-perfect acceptance, new-menu recipe, passing menus, known failures | [menus.md](menus.md) | `tools/ui_pixel_diff.py`, `docs/evidence/ui-v5/elements/` |
| Player and NPC characters | [characters.md](characters.md) | `GameClient/ArtSource/PlayerCharacter/README.md` |

## Shared rules (every visual job)

### Routing
1. **GPT-6.1 Sol builds** (editor passes, Blender, materials, scripts, C++ widgets) and handles design and difficult debugging; use high reasoning effort for those decisions. GPT-6 Luna may handle simple, bounded mechanical tasks at medium effort. This routing replaces the former Sonnet, Fable and Opus assignments in category recipes.
2. **Raster assets follow their approved asset pipeline and tool**: concept sheets, textures, masks and UI PNGs. Preserve the existing Asset Forge/local-model authority and source/licensing rules in the asset recipe. Use the current imagegen skill and image-generation tool only for session raster jobs the owner has authorized to use that workflow; it is not an implicit provider fallback. Keep the prompt and the exact returned output path or identifier beside the resulting asset; copy only the output associated with that request into its named package path. If it is missing, recover that exact output or report the missing result; never substitute the newest image from a shared output folder. Images do not grant their builder review authority.
3. **A fresh GPT-6.1 Sol reviewer judges, spawned by the builder** at high reasoning effort with no prior context: after each capture or render batch give it the brief in [docs/visual-review.md](../visual-review.md), the `*-compare.png` or render paths, the concept paths and, from round 2, the previous open list. It writes `REVIEW.md` in the evidence folder. The builder never grades its own work and implements only the reviewer's numbered fix list.
4. **At most 3 review rounds, then the owner.** After round 3 (or a PASS) the work goes to the owner with the latest compare images and open list; the owner's sign-off is what makes it done.
5. Delegate only useful parallel work, with at most three subagents active at once. Run one Unreal-heavy job at a time across all lanes; follow the ownership rules in [hand-offs](../hand-offs/README.md#how-we-work).

### Capture rules
1. World captures are `-game` HighResShots at **1920x1080**, timed from a Python capture script, with the editor closed:
   `& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' 'C:/Projects/Shattered-Frontier/GameClient/ShatteredFrontier.uproject' '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook' -game -RenderOffscreen -unattended -nosound -NoSplash -ResX=1920 -ResY=1080 '-ExecCmds=py C:/Projects/Shattered-Frontier/<capture script>'`
   The script sets `r.EyeAdaptationQuality 0`, `r.DefaultFeature.AutoExposure 0`, `r.MotionBlurQuality 0`, `r.ScreenPercentage 100`, hides the pawn, and runs `HighResShot 1920x1080 filename=<abs>.png` per view (template: `docs/evidence/graphics/2026-10-21-wp6-town-f/capture_game.py`).
2. UI captures use the console line `Shot showui` (HighResShot omits UMG), from the E1 session, never the construction sandbox (see [ui-hud.md](ui-hud.md)).
3. **Never** use `ShowFlag.*` overrides to hide anything in a capture: labels and markers must be absent because `bHiddenInGame` is set (gate below). Review-only sun/exposure overrides are applied in `-game` only and never saved to the map.
4. **Never ask the owner for screen captures.** Never capture from the editor viewport or PIE.
5. Pair every view: concept left, live right, one `<view>-<concept>-compare.png` per view (`make_pairs.py` in the evidence folder). Concepts: `docs/Game-Design/concept-art/UI/v5/gameplay-hud.png` (town, forest, HUD), `concept-art/Silverbrook-Design-Board-v1.webp` (town board), `concept-art/Greenlands-Forest-Look-v1.webp` (forest floor), `concept-art/Silverbrook/<Name>.png` (one per building, lamp, prop sheet).
6. Before each shot the capture script asserts no dressing prop or lamp within 1.5 m of the sight line in the first 10 m (bounds sphere, not pivot). A blocked frame is a bug, not a result.
7. Evidence folder `docs/evidence/graphics/<date>-<pass>/`: the capture script, the check scripts as run, their JSON outputs, the `*-compare.png`, and a README with run order, gate output lines and every deviation from the fix list. No verdict in the README; the reviewer writes `REVIEW.md`.

### Gates (run after every map pass; copy the scripts from `docs/evidence/graphics/2026-10-21-wp6-town-f/`)
| Gate | Command (open editor) | Expected output |
|---|---|---|
| Debug actors hidden, no default/graybox material | `py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/check_hidden_in_game.py` | `HIDDEN_IN_GAME_CHECK checked N visible_in_game 0 []` and `DEFAULT_MATERIAL_CHECK visible_in_town N default_or_graybox 0 []` |
| Foundations | `py -3 tools/Run-InEditor.py <evidence>/check_foundations.py` | `FOUNDATION_CHECK 312 failures 0 min_visible_cm 65.0 max_abs_gap_cm 0.0 proud_cm 4.0 4.0` |
| Placed bases on the ground | `py -3 tools/Run-InEditor.py <evidence>/check_ground.py` | `DRESSING_GROUND {... 'floating': 0 ...}` (nothing over 5 cm) |
| PCG dressing grounded | `py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/check_dressing_grounded.py` | `DRESSING_GROUNDED {"floating": 0, ...}` |
| House details | `py -3 tools/Run-InEditor.py <evidence>/check_details.py` | `SILL_PLANTERS 140 expected 140 GROUP_DETAIL_OVERLAPS 0 []` |
| Frame time | `perf_bookmark_game.py <view>` (docstring has the launch line) | `perf-<view>.json` p95 <= 16.7 ms, median <= 13 ms at 1440p Standard |
| Visual | independent review under the routing above (max 3 rounds), then owner sign-off | `REVIEW.md` verdict plus owner sign-off |
The counts above are the whole-town values; a single building in a dev map uses the per-doc values (for example [buildings.md](buildings.md) §6: `FOUNDATION_CHECK 1 failures 0 ...`).

### Worktrees (a feature session in its own checkout)
1. Sync first: in the worktree `git fetch origin && git merge --ff-only origin/main` (worktrees branch from a HEAD that may lack later config such as the SM6 `DefaultEngine.ini`; never autostash, never reset).
2. `pwsh tools/Link-WorktreeDeps.ps1` once: junctions `Artifacts/ThirdParty` from the main checkout and gives this worktree's editor its own MCP port (written to `GameClient/Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini` and this checkout's `.mcp.json`, skip-worktree); restart the Claude session so `.mcp.json` is re-read.
3. Build (no editor of this checkout open): `& 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' ShatteredFrontierEditor Win64 Development "-Project=$PWD\GameClient\ShatteredFrontier.uproject" -WaitMutex -NoHotReloadFromIDE` -> `Result: Succeeded`.
4. Launch this checkout's editor yourself: `$p = Start-Process 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList "`"$PWD\GameClient\ShatteredFrontier.uproject`"" -PassThru`; record `$p.Id`; stop only that PID. `tools/Run-InEditor.py` talks only to the editor opened from its own checkout (`project_root` match, commit `b123aaf`): another checkout's editor is ignored, and a `-game` run is skipped.
5. Work in a dev map under `/Game/Frontier/Maps/Dev/<Feature>/`; never edit the Silverbrook map or shared art from a worktree. Wiring into Silverbrook happens later in the main editor lane.

For a single building, use `tools/Create-BuildingDevMap.py <PascalName>` and `import_silverbrook.py --package-only <PascalName>` in this worktree. The package-only importer requires shared masters already installed and prints each saved asset as `RESAVED <asset>`. Run `silverbrook_props.py --folder Districts/Dev --terrain Terrain` and the single-building gates in [buildings.md](buildings.md) §6.

### Assets: shared side effects of an import
`import_silverbrook.py` rebuilds and resaves `M_SB_Scan`, `M_SB_Solid`, the shared `T_SB_*` textures and **every** `MI_SB_*` on each run (~49 assets go `M` after importing one package). Before committing: `git status --short GameClient/Content/Frontier/Environment/Greenlands/Silverbrook/`; revert everything that is not the new package's own files (`Meshes/SM_Silverbrook<Name>*`, `Materials/MI_SB_<Name>_*`, new `Textures/T_SB_<scan>_*`) with `git checkout -- <paths>`; never commit a shared `MI_SB_*`, `M_SB_*` or `T_SB_*` change from a package import. The same applies to `import_nature.py` (`M_GN_*`, `MI_GN_*`) and `Import-HudKit.py` (`DA_HudKit`, `T_Kit_*`): commit only what your task changed on purpose.

### Engine and repo rules
- Editor scripts run in the open editor through `py -3 tools/Run-InEditor.py <script> [args]` (needs Project Settings > Plugins > Python > Enable Remote Execution). With the editor open, never run headless commandlets that rewrite maps or assets. Blender scripts run headless from the package folder in PowerShell: `& 'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe' -b --factory-startup -P build_<x>.py` (a Bash call with a quoted absolute path is refused by the command guard).
- Serialize the editor and build lane: one editor session per checkout; close it before `/build-editor` (the Build.bat line above) or use Live Coding. Close it before `-game` captures.
- Kill only your own PIDs (`Start-Process -PassThru`, `Stop-Process -Id`), never by process name.
- Write only under `/Game/Frontier/`. Megascans and Fab content is licensed, git-ignored and read-only; nothing committed may reference it. Sources are Poly Haven CC0 (hash-verified via `acquire_sources.py` / `acquire_materials.py`) or Codex-generated.
- Every tunable is typed `{value, min, max, unit}` JSON with shipped defaults; scripts refuse out-of-bounds values. No console-only overrides.
- Any change to the painted ground goes in **both** `build_silverbrook.py` and `paint_street_edges.py` so a rebuild reproduces it.
- UE 5.8: `UTextBlock::SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis)` (not `SetOverflowPolicy`); C4458 (declaration shadows a class member) is an error, rename locals; creating a Landscape needs editor C++ (`ALandscape::Import`), there is no Python path; positional `unreal.Color(r,g,b,a)` maps BGRA, use keyword arguments.

### Commit method (shared checkout, private index)
```bash
base=$(git rev-parse HEAD)
export GIT_INDEX_FILE=.git/index.bp
git read-tree $base
git add <your files>
c=$(git commit-tree $(git write-tree) -p $base -m "<message>

Co-Authored-By: <your session's attribution line>")
git update-ref HEAD $c $base
git push
```
The trailer is **your own model's line** as given in your session's attribution reminder (for example `Co-Authored-By: Claude Haiku 5.5 <noreply@anthropic.com>`), never another model's. Never touch the shared `.git/index`, never autostash, never reset. For files other sessions also edit (`docs/dependency-map/<area>.md`), build the blob from HEAD plus your lines and keep LF endings. In a worktree, use its own branch and a normal `git add`/`git commit` with the same trailer.

### Done means
A pass is done only when the fresh reviewer's `REVIEW.md` has no FAIL in the pass's domain and every gate above printed its pass line. Report anything else as not done, with the open fix items.
