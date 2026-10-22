# GFX-02 J2-pre: hex-tiled ground on today's mesh terrain (evidence)

Spec: `docs/decisions/GFX-02-photoreal-ground-pipeline.md` section 2a (Interim J2-pre). No verdict here; the reviewer writes `REVIEW.md`.

## Changed
- `GameClient/ArtSource/Greenlands/NatureFamilies/hex_tile.py` builds `MF_GN_HexTile` (Mikkelsen 2022, one Custom HLSL node, spec code verbatim). Rebuild deletes each expression explicitly (`delete_all_material_expressions_in_function` leaves stale ones behind) and gives every texture input a Preview (the function's own compile needs it).
- `import_ground.py`: `layer(..., hex=True)` for A forest floor, R cobble (Rot 0.15), T mud, S rocky trail; B and bedrock C plain; macro and EdgeNoise plain. **Deviation:** the `Mask` sample (9 m, drives moss patches and trail stones) is also hex-tiled (`CellTiles` 0.25, bounds 0.25-1.0). With it plain the green moss patches stay a visible 9 m grid and the grass crops of A2 read 0.39 / A3 0.30 (before and after the layers alone).
- `Measure-GroundRepeat.py`, three views `A1/A2/A3` with `repeat_crops` in `silverbrook-reference-bookmarks.json`.
- Assets: `MF_GN_HexTile.uasset` (new), `M_GN_Ground.uasset`. `MI_GN_Ground` untouched. Side-effect re-saves of `M_GN_Surface`, `MI_GN_Road`, `MI_GN_Trail`, `M_GN_River` reverted.

## Run order
1. Build.bat against this worktree: `Result: Succeeded`. Editor opened from this checkout.
2. `py -3 tools/Run-InEditor.py .../NatureFamilies/import_ground.py` -> `GROUND_IMPORT [...]`; no `Failed to compile` for `M_GN_Ground` or `MF_GN_HexTile` (`M_GN_River` logs "No inputs to Single Layer Water Material", which it does on the untouched master too).
3. Editor closed. `-game` shots at 1920x1080, `capture_game.py` with `GFX_TAG=before` (old asset) and `after`.
4. `py -3.12 tools/Measure-GroundRepeat.py <dir> <json>`; `make_pairs.py`.
5. `perf_gpu.py` at bookmark 01, 2560x1440 windowed, alternating old and new `M_GN_Ground`.

## Repeat gate (`repeat-measure.json`, before in `repeat-measure-before.json`)
`GROUND_REPEAT PASS crops 9 max_peak 0.2414 limit 0.25`

| crop | before | after |
|---|---|---|
| A1 cobble / earth / grass | 0.782 / 0.082 / 0.270 | 0.000 / 0.083 / 0.236 |
| A2 cobble / earth / grass | 0.000 / 0.000 / 0.387 | 0.000 / 0.000 / 0.240 |
| A3 cobble / earth / grass | 0.151 / 0.104 / 0.077 | 0.155 / 0.105 / 0.066 |

Measurement notes (deviations a reviewer should know):
- Peak is taken over local maxima of the autocorrelation in 6 < r <= 120 px. Taking the plain maximum returns the zero-lag shoulder (0.7-0.9 for any smooth image, before or after), so it cannot show a repeat.
- A 256 px crop of pure ground is not possible in these views (the earth margin is 25-90 px wide, shadows and PCG shrubs cover the fields). Crops are chosen away from props where possible; A1 earth/grass and A2 cobble/earth still contain shrubs, tree shadows or road edges, whose repeats are unchanged by the material. "0.000" means no local maximum in the band.
- The numbers sit close to the limit (0.236, 0.240). A3 cobble and earth are mostly road edge.

## Captures (`before/`, `after/`)
A1-A3 PNG (lossless, the gate reads them), everything else JPG q90. `01-forest-path-town`, `04-forest-floor`, `04-forest-floor-sun20` and `A1-sun20` (sun pitch -20, -game only), context views: `ctx-overview-south`, `ctx-overview-east` (300 m up) and `ctx-ground-1..4` (seeded, Random(20261022)).
Pairs: `A1-hud-before-after-compare.jpg` (gameplay-hud far range | before | after), `A2-...`, `A3-...`, `01-...`, `04-forest-floor-sun20-before-after-compare.jpg`.

## Hex-border lighting self-check
`04-sun20-after-crop.png` and `A1-sun20-after-crop.png` (1:1 crops of the after shots with the sun at 20 deg elevation): no brightness step visible along a hex edge, so the rotation sign was left as in the spec (`NEG_ANGLE` False). The visible ground at bookmark 04 is mostly the plain mossy-rock layer B, so A1-sun20 (cobble, earth, grass) is the stronger check.

## Look change to expect
Bookmark 04 changed from stony olive forest floor to mostly green mossy rock: the hex-tiled `Mask` no longer lands on the same 9 m window there, so that spot now draws from the whole mask texture instead of one low-moss patch of it.

## Timing at bookmark 01 (`perf-01.json` per folder, 500 frames, 2560x1440 windowed, other editors running on the machine)
| run | median / p95 ms |
|---|---|
| before | 10.31 / 13.10 (first launch, shader compile in warm-up), 8.20 / 13.9, 7.65 / 9.36 |
| after | 7.66 / 10.34, 7.87 / 10.40, 8.02 / 10.35 |
Committed pre-change `GameClient/ArtSource/Greenlands/perf-01-forest-path-town.json` (older state): 6.93 / 9.23. The `stat gpu` overlay is not drawn by `HighResShot` (`perf-01-gpu.png` shows no rows), so no per-pass GPU numbers; frame time is the measure and the difference is inside run-to-run noise (budget: within +1 ms of today).
