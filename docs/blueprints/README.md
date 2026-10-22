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
| Player and NPC characters | [characters.md](characters.md) | `GameClient/ArtSource/PlayerCharacter/README.md` |

## Shared rules (every visual job)

### Routing
1. **Sonnet builds** (editor passes, Blender, materials, scripts, C++ widgets).
2. **Codex does image generation only**: concept sheets, textures, masks, UI PNGs. Working call: `cd <empty out dir> && timeout 600 codex exec --skip-git-repo-check -s workspace-write "Generate one image with your image generation tool: <prompt>. Save it as <name>.png in the current directory."` Codex may also run already-committed scripts in the background; it never makes judgement calls on art.
3. **Fable judges**: a fresh `Agent` on model `fable` with no prior context, given the `*-compare.png` pairs, the concept paths and the brief in [docs/visual-review.md](../visual-review.md). The builder never grades its own work and implements only the reviewer's numbered fix list.
4. **A failed Fable check goes to Opus** for the next pass; the Opus pass is re-judged by a fresh Fable reviewer.
5. One active Claude task at a time; everything scriptable goes to Codex.

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
| Visual | fresh Fable review of the pairs | `REVIEW.md` with every view PASS |

### Engine and repo rules
- Editor scripts run in the open editor through `py -3 tools/Run-InEditor.py <script> [args]` (needs Project Settings > Plugins > Python > Enable Remote Execution). With the editor open, never run headless commandlets that rewrite maps or assets. Blender scripts run headless: `"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P <build>.py`.
- Serialize the editor and build lane: one editor session at a time; close it before `/build-editor` or use Live Coding. Close it before `-game` captures.
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

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>")
git update-ref HEAD $c $base
git push
```
Never touch the shared `.git/index`, never autostash, never reset. For files other sessions also edit (`docs/dependency-map.md`), build the blob from HEAD plus your lines and keep LF endings.

### Done means
A pass is done only when the fresh reviewer's `REVIEW.md` has no FAIL in the pass's domain and every gate above printed its pass line. Report anything else as not done, with the open fix items.
