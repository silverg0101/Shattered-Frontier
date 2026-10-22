# GFX-02 J2-pre: hex-tiled ground on today's mesh terrain (evidence)

Spec: `docs/decisions/GFX-02-photoreal-ground-pipeline.md` section 2a (Interim J2-pre). No verdict here; the reviewer writes `REVIEW.md`.

## Round 2 (after REVIEW-r1.md: 3 PASS, 8 FAIL)
Implemented from the r1 fix list:
1. `import_ground.py`: Mask is a plain 9 m sample again; its UV is domain-warped by two world-space Simplex `Noise` nodes (`scale` 0.0004, 2 levels, +-0.3 tile via `MaskWarp`). `Low`, `InvRange`, `StoneLow`, `StoneSharp` unchanged.
2. `CellTiles` 1.0 for layers A, T, S (and cobble R).
3. `MF_GN_HexTile` gains a `Snap` input (offsets snap to 1/Snap tile); cobble R: `Rot` 0.0, `CellTiles` 1.0, `Snap` 14 (14 stone courses per 2.4 m tile, counted on `cobblestone_pavement_Diffuse`).
4. `tools/Measure-GroundRepeat.py`: 6 px Gaussian high-pass, NaN (fail) when the band has no local maximum, per-view `repeat_min_radius_px` (A1 12, others 6). Measurement captures are separate (`before-measure/`, `after-measure/`: A1-A3 only, `ShowFlag.DynamicShadows 0`, capture-only), crops re-fixed on the shadow-free frames, sizes 32-256 px where 256 px of pure surface does not exist (earth margin 32-128 px, A3 road 85 px). PCG shrubs were not hidden; crops avoid them.
5. Sun-20 ground-level captures: `after/ctx-ground-4-sun20`, `after/A1-margin-sun20` (no `before/` pair for these).
Rebuilt with `import_ground.py`; no compile errors for `M_GN_Ground` / `MF_GN_HexTile`. All `after/` captures are from the round-2 material; `before/` is the original material (before-measure also).

## Changed (cumulative)
- `NatureFamilies/hex_tile.py` builds `MF_GN_HexTile` (spec HLSL + `Snap`). Rebuild deletes each expression explicitly (`delete_all_material_expressions_in_function` leaves stale ones) and gives every texture input a Preview (the function's own compile needs it).
- `import_ground.py`: `layer(..., hex=True)` for A forest floor, R cobble, T mud, S rocky trail; B and bedrock C plain; macro and EdgeNoise plain; Mask plain + warp (above).
- `Measure-GroundRepeat.py`, views `A1/A2/A3` with `repeat_crops` in `silverbrook-reference-bookmarks.json`.
- Assets: `MF_GN_HexTile.uasset` (new), `M_GN_Ground.uasset`; `MI_GN_Ground` untouched. Side-effect re-saves of `M_GN_Surface`, `MI_GN_Road`, `MI_GN_Trail`, `M_GN_River` reverted (`M_GN_River` logs "No inputs to Single Layer Water Material" on the untouched master too).

## Run order
1. Build.bat against this worktree: `Result: Succeeded`. Editor opened from this checkout, `import_ground.py` via `Run-InEditor.py`.
2. Editor closed. `-game` 1920x1080 shots: `capture_game.py` (`GFX_TAG`, optional `GFX_MEASURE=1`).
3. `py -3.12 tools/Measure-GroundRepeat.py <dir> <json>`; `make_pairs.py`; `perf_gpu.py`.

## Repeat gate (round 2; `repeat-measure.json` after, `repeat-measure-before.json` before; both on the `*-measure` frames)
`GROUND_REPEAT FAIL crops 9 max_peak 0.2946 limit 0.25` (after); before `max_peak 0.665`.

| crop (rect x,y,w,h) | before | after |
|---|---|---|
| A1 cobble [876,816,256,256] | 0.194 | 0.096 |
| A1 earth [700,715,128,112] | 0.559 | 0.078 |
| A1 grass [175,440,128,128] | 0.244 | 0.280 FAIL |
| A2 cobble [885,790,160,256] | 0.436 | 0.110 |
| A2 earth [815,600,64,256] | 0.665 | 0.118 |
| A2 grass [1230,800,256,256] | 0.342 | 0.236 |
| A3 cobble [915,600,85,256] | 0.592 | 0.204 |
| A3 earth [884,600,32,256] | 0.243 | 0.098 |
| A3 grass [1075,520,190,190] | 0.433 | 0.295 FAIL |

Local-maximum rule kept (the plain maximum returns the zero-lag shoulder). The two grass failures are the small cross-shaped moss patch of the Mask texture, now scattered but still repeated; no change outside the r1 fix list was made for it.
(The round-1 table from the earlier frames is superseded by `REVIEW-r1.md`'s findings.)

## Captures (`before/`, `after/`)
A1-A3 PNG, everything else JPG q90: `01-forest-path-town`, `04-forest-floor`, `04-forest-floor-sun20`, `A1-sun20`, `A1-margin-sun20`, `ctx-overview-south/east`, `ctx-ground-1..4`, `ctx-ground-4-sun20` (sun pitch -20, -game only). Pairs: `A1-hud-before-after-compare.jpg` (gameplay-hud far range | before | after), `A2-`, `A3-`, `01-`, `04-forest-floor-sun20-before-after-compare.jpg`.

## Timing at bookmark 01 (`perf-01.json` per folder, 500 frames, 2560x1440 windowed, other editors running on the machine)
| run | median / p95 ms |
|---|---|
| before | 10.31 / 13.10 (first launch, shader compile in warm-up), 8.20 / 13.9, 7.65 / 9.36 |
| after | 7.66 / 10.34, 7.87 / 10.40, 8.02 / 10.35 |
Committed pre-change `GameClient/ArtSource/Greenlands/perf-01-forest-path-town.json` (older state): 6.93 / 9.23. The `stat gpu` overlay is not drawn by `HighResShot` (`perf-01-gpu.png` shows no rows), so no per-pass GPU numbers; frame time is the measure and the difference is inside run-to-run noise (budget: within +1 ms of today).
