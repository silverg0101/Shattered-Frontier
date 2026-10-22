# GFX-02 J2-pre hex-tiled ground: independent review (Fable, 2026-10-22)

Judged against `docs/decisions/GFX-02-photoreal-ground-pipeline.md` section 2a (Interim J2-pre, Acceptance) and section 7 row J2-pre. Domain that must be empty: repeating motifs on cobble, earth or grass at A1-A3. Every image in `before/`, `after/`, the five compare pairs, the two sun-20 crops and both measure JSONs were read at full size; the nine gate rectangles were cut 1:1 and at 3x from the PNGs, and the gate's own `peak()` was re-run on the gate rectangles plus nine extra open-field windows (numbers below).

Tally: PASS 3 (A1, ctx-overview-south, ctx-overview-east). FAIL 8 (A2, A3, 01, 04, ctx-ground-1, -2, -3, -4).

## A1-south-approach-30m — PASS

Human check (3x crops, 1:1 field):
- Cobble crop [876,816]: no repeating motif. The before showed the stone pitch as a 7 px lattice (gate 0.782 at lag (7,0)); the after reads as uniform beige sand. The crop is one third tree shadow.
- Earth crop [640,380]: not an earth crop. It holds grass, two lamp posts, the gate tower, wall and tree shadows; the margin is a 70 px diagonal strip in its lower right. No repeat visible in that strip.
- Grass crop [1170,470]: tree canopy, a low-poly bush, a tree shadow and four shrubs inside. In the grass itself five faint pale 2 m blobs of differing shape; not copies of one motif.
- Open field (whole frame): the before's identical "X" tuft clusters on a 2 m grid are gone. What replaced them is a loose scatter of pale angular patches ~2 m wide (the hex-tiled Mask's 2.25 m cells, see ctx-ground below); at 30 m they do not line up in rows, so no copies counted.
- Side effect visible here: the carriageway lost its stone read entirely (before: fine stone grid at 30 m; after: sand). Cause and fix under 01 / fix 3.

## A2-south-approach-100m — FAIL

- Cobble crop [840,620]: one third tree shadow; the rest uniform sand. No motif.
- Earth crop [730,600]: a third shadow, a third cobble, a third margin. The before's diagonal ribbing on the margin is gone. No motif.
- Grass crop [1228,800]: FAIL. Motif: a pale green blob ~20-27 px wide (2.0-2.5 m on the ground), same size and orientation each time, sitting on diagonal rows. Count in the crop: 14 copies on 4 rows (before: 19 identical striped tufts on the same rows). Gate reads 0.241 at lag (9,-9), the before read 0.387 at (10,-9): same lattice period, 60 % of the amplitude. The blobs are the hex-tiled `Mask`'s 2.25 m cells (CellTiles 0.25 x 9 m) whose random window crossed the moss threshold; the cells sit on the hex lattice, so the moss patches now sit on a lattice.
- Open field right of the road (window [1250,580]): same rows, 30+ blobs.

## A3-south-approach-250m — FAIL

- Cobble crop [840,740] and earth crop [840,600]: both are mostly road edge (the measured 0.155 / 0.105 are at lags along the road direction, (64,-21) and (99,-36)). No tile motif visible on either surface.
- Grass crop [1066,514]: FAIL. Same motif as A2 at 8-12 px: 25 pale blobs on diagonal rows (before: 30 identical striped tufts on the same rows). The gate reads 0.066; the before read 0.077. The gate passed the visibly tiled before image here, see "Measurement".
- Open field left of the road: rows of blobs across the whole field, strongest between the trees at [0-500, 300-700].

## 01-forest-path-town — FAIL

- Seams on cobble at ground level. 4x crop at [380-700, 830-1000]: a straight diagonal hex border runs through x ≈ 580, y 830-960 where the stone courses change direction by ~15° and the stones in the ~25 px blend band are doubled (ghosted). The same happens at every cell border, i.e. every 1.2 m (CellTiles 0.5 x 2.4 m), across the whole foreground; the before is one continuous stone grid. Random per-cell offset `h` shifts the stone grid by a fraction of a stone at every border; cobble is a regular grid, so the contrast-preserving blend cannot hide it.
- Overall the after cobble is softer than the before at 1-5 m (ghost bands cover ~30 % of each cell at pow 7).
- Earth margin and grass beyond: no seams, no repeat.

## 04-forest-floor (and 04-forest-floor-sun20) — FAIL

- Hex-border brightness step at sun 20: none visible in `04-sun20-after-crop.png`, but the visible ground is the plain mossy-rock layer B, so this capture tests nothing about the hex rotation of normals. `A1-sun20-after-crop.png` (margin + cobble at 30 m, hex cells ~20 px) shows no steps either; that is the only evidence, and it is at a distance where a step would be 1-2 px. Inconclusive, not a pass.
- Colour/content regression: the before was a stony olive forest floor (layer A forest_ground_04 with moss patches), the after is a flat lichen-green rock slab with one crack and no stones, litter or soil. Against `Greenlands-Forest-Look-v1.webp` (stones, needles, litter, grass between) the after is further away than the before. Cause: the hex-tiled Mask (`wm = saturate((Mask - 0.4) * 32)`) now samples random windows of the whole mask texture, so the moss-rock weight `wm` is high over most of this spot; also the trail stone switch `sw` on line 185 of `import_ground.py` reads the same `mk`, so trail stones moved to 2.25 m hex cells as well. The README calls this a look change to expect; it is a regression of the designed 9 m moss-patch field into 2.25 m hex chunks.

## Context views

### ctx-overview-south, ctx-overview-east — PASS
No repeating structure on the fields at 300 m in either; faint mottling only. The world reads as a place: a ring-walled town on a river plain, radial avenues, the south approach, bridge and mill island are consistent between the two overviews and with A1-A3. (The empty plain, blob mountains and identical house rows are carried forward.)

### ctx-ground-1, ctx-ground-2, ctx-ground-3 — FAIL
Visible hexagon cells. 3x crops of the moss patches: ctx-ground-2 at [800-1300, 780-1000] shows a moss patch bounded by two dead-straight edges meeting at a 120° corner, and a second patch cut by a straight diagonal; ctx-ground-1 [300-900, 780-960] and ctx-ground-3 [100-700, 750-1000] show the same straight-edged polygonal patches, ~2.25 m across, where the before had rounded patches (on a 9 m grid, which was its own problem). Mechanism: the hex blend of the mask is soft, but `Low 0.4 / InvRange 32` re-hardens it into a hard line exactly along the cell border. Any hex-tiling of a thresholded mask produces this.

### ctx-ground-4 — FAIL
Cobble at 1-5 m: patchwork of stone-course orientations with ghosted stones in the bands between (3x crop [500-1000, 780-1080], after panel, band from (1500,150) to (1750,350)); the before is one continuous grid. Earth margin: fine, no cells visible.

## Measurement credibility

The gate numbers are real but do not measure what the spec says, and the README's attributions are wrong in two places:
1. "0.000" is not a pass. The gate's `peak()` returns 0.0 when the band has no local maximum at all. Re-running it on the gate rectangles: A1 cobble, A2 cobble and A2 earth (after) and A1 road windows (before and after) all return `nmax 0`, i.e. the luminance autocorrelation is one smooth lobe because a tree shadow dominates the crop. A high-passed luminance (σ = 6 px Gaussian subtracted) of the same crops has 300-750 local maxima. Three of the nine passing numbers are "the shadow swallowed the texture".
2. The before A1 cobble 0.782 at lag (7,0) is the stone pitch (a cobble road is periodic at its stone pitch), not the tile grid; the after's 0.000 there comes from the stones being blurred out plus the shadow, not from a removed repeat. The spec's claim that 6 px is below every period in frame is false for cobble at 30 m.
3. The README says the Mask's 9 m grid is what A2/A3 grass measure. The measured lag is 12.7 px ≈ 1.1-1.3 m on the ground at A2; 9 m would be 80-100 px. The 0.387 before was layer A's 2 m tile content; the 0.241 after is the hex-cell lattice (1 m layer-A cells and 2.25 m Mask cells on the same lattice). The gate is sitting at 0.24 because it now measures the honeycomb instead of the tile.
4. The gate passed the before image at A3 (0.151 / 0.104 / 0.077) although the before A3 grass crop shows 30 identical tufts on a grid, and my extra open-field windows read 0.000-0.004 before and after at A3. A gate that cannot see the visibly tiled before is not a gate; it is not being gamed deliberately, it is meaningless for 6 of 9 crops.
5. Crops violate the spec's own rule ("no props or grass cards inside"): A1 earth holds walls, lamp posts and trees; A1 grass holds a canopy and a bush; A1/A2 cobble and A2 earth hold tree shadows; A3 cobble/earth are road edge. A 256 px pure-earth crop does not exist at A1 (margin 25-90 px wide).

Perf: the three after runs (7.66-8.02 ms median) are inside the +1 ms budget against the 7.65 ms before run; accepted as reported, noting other editors were running.

## Differences, most visible first

1. Diagonal rows of pale moss blobs across the open field at A2 and A3 (14 and 25 copies in the grass crops), hexagon cell edges with 120° corners on moss patches at ctx-ground-1/2/3, and the 04 regression are one defect: the Mask (and through `mk`, the trail stone switch) is hex-tiled at 0.25 cells.
   Fix type: CODE. Exact: `import_ground.py` lines 134-140: delete the `mcall` hex call and its `TextureObjectParameter`; restore the plain sample the spec prescribes but domain-warp its UV with world-space Simplex noise so the 9 m period does not line up on a grid:
   ```python
   wp = node(m, u.MaterialExpressionWorldPosition, -2000, 2600)
   def noise(x, y, off):  # non-periodic scalar in [-1,1], ~25 m features
       nz = node(m, u.MaterialExpressionNoise, x, y, noise_function=u.NoiseFunction.NOISEFUNCTION_SIMPLEX_TEX,
                 scale=0.0004, levels=2, output_min=-1.0, output_max=1.0, quality=2, turbulence=False)
       link(g.op(u.MaterialExpressionAdd, wp, g.vec('NoiseOff' + off, (0.0, 0.0, 4096.0) if off else (0.0, 0.0, 0.0), x - 150, y), x - 50, y), '', nz, 'Position'); return nz
   warp = g.mul(node(m, u.MaterialExpressionAppendVector, -1500, 2650, ...) , g.sc('MaskWarp', 0.3, -1500, 2750), -1400, 2650)   # append noise(.., '') and noise(.., 'B'); 0.3 tiles = ±2.7 m
   mk = g.mask(g.smp('Mask', tex(T + 'forest_ground_04_Displacement'), S.SAMPLERTYPE_MASKS,
               g.op(u.MaterialExpressionAdd, g.uv(xy, g.sc('TileCmMask', 900, -1800, 2700), -1650, 2600), warp, -1300, 2600), -1100, 2600), True, False, False, -800, 2600)
   ```
   Keep `Low 0.4`, `InvRange 32`, `StoneLow 0.6`, `StoneSharp 14` unchanged (coverage at 04 returns to the before's). Cost: 1 texture tap (was 12 inside the Custom node, which the compiler cannot dead-strip) plus two Noise evaluations. Re-measure A2/A3 grass; expected: the blob rows disappear because the moss patches are no longer on any lattice.

2. Residual honeycomb on layer A (1 m cells, blotchy margins at A1, part of the 0.24 at A2): with `CellTiles 0.5` every cell window is half a tile, so cell mean luminance differs cell to cell and the lattice shows.
   Fix type: CODE. Exact: `import_ground.py` line 114: `CellTiles` constant 0.5 → 1.0 for layers A, T, S (add `cell=1.0` to `hexlayer()` / `layer()`); a one-tile hex window has the same mean as every other, so the lattice carries no brightness. Spec bound 0.25-1.0 allows it. Rot stays 1.0.

3. Hex borders on cobble (01, ctx-ground-4, A1 stone read lost): random fractional-stone offset and ±27° rotation at every 1.2 m border on a regular stone grid.
   Fix type: CODE. Exact: `hex_tile.py`: add a scalar function input `Snap` (default 0.0, `use_preview_value_as_default` True) after `CellTiles` in `spec` and in the Custom inputs; HLSL after line 29: `if (Snap > 0.0) h = floor(h * Snap + 0.5) / Snap;`. `import_ground.py` line 176: cobble R with `rot=0.0`, `cell=1.0`, `snap=<stones per tile>` (count the courses across one 2.4 m tile of `T_GN_<SB>_Diffuse`; ~12 if stones are 20 cm). Offsets then move the grid by whole stones, borders fall on mortar lines, no doubled stones, courses continuous; the texel repeat stays broken (different stones per cell). `landscape_material.py` callers are unaffected (Snap defaults to 0).

4. Hex-border lighting self-check tests a plain layer (04 = layer B).
   Fix type: ART (capture). Exact: `capture_game.py`: add `ctx-ground-4-sun20` (same camera as ctx-ground-4, sun pitch -20) and `A1-margin-sun20` (`xy_m` [181,661] eye height 1.7 m looking down the margin, fov 70). Cells are 1-2.4 m at 1-5 m, i.e. 100+ px: a normal-rotation step is unmistakable there. Keep `NEG_ANGLE` as is unless that capture shows a step.

5. Gate rectangles and method.
   Fix type: CODE. Exact: `tools/Measure-GroundRepeat.py` `peak()`: (a) subtract a 6 px Gaussian blur of the luminance before windowing (`from PIL import ImageFilter`, or a numpy separable kernel) so shadows and gradients do not swallow the texture; (b) when `band` is empty return `float('nan')` and fail the crop ("no measurement"), never 0.0; (c) lower band radius per view: 12 px at A1 (cobble stone pitch is 7 px at 30 m), 6 px at A2/A3. `silverbrook-reference-bookmarks.json`: re-fix `repeat_crops` on pure ground after capturing the A views for measurement with `ShowFlag.DynamicShadows 0` and the PCG shrub actor hidden (measurement capture only; the review pairs keep shadows), and allow crop sizes 128-256 px where 256 of pure surface does not exist (A1 earth: 128 px at [700,960]). Validate the gate on the before captures first: it must fail A3-before grass.

## Fix list (job domain, deduplicated)

1. Engineer — Mask back to a plain sample, domain-warped by two Simplex Noise nodes (±0.3 tile); delete the hex Mask call (difference 1). Also restores 04.
2. Engineer — `CellTiles` 1.0 for layers A, T, S (difference 2).
3. Engineer — `Snap` input in `MF_GN_HexTile`; cobble R: Rot 0.0, CellTiles 1.0, Snap = stones per tile (difference 3).
4. Codex — `Measure-GroundRepeat.py`: high-pass, NaN-on-no-maximum, per-view lower band; shadow-free/shrub-free measurement captures; re-fixed pure-ground crops; validate against before (difference 5).
5. Codex — sun-20 captures on hex layers at ground level: `ctx-ground-4-sun20`, `A1-margin-sun20` (difference 4).
6. Codex — re-run `import_ground.py`, recapture A1-A3, 01, 04, ctx-ground-1..4 and the pairs; fresh review.

## Carried forward (not this job)

- Aerial colour blend 25-60 m and macro luminance variation per layer (J2): until then the hex cells of every layer are what A2/A3 show instead of the aerial scan.
- Low-poly lime-green blob trees and bushes at A1-A3 and ctx-overview; identical house rows; bare plain with no fields, hedges or tracks between town and river (world as a place).
- White blob mountains without rock or snow detail (A1, 01, ctx-ground-1/2/3).
- Cobbled carriageway where `gameplay-hud.png` far range shows a dirt track; beige cobble colour vs the concept's grey-brown stone.
- Bookmark 04 vs `Greenlands-Forest-Look-v1.webp`: no leaf litter, needles, loose stones or undergrowth variety even in the before; only the layer mix regression is in this job.
- Hard-edged river bank and flat water colour (A2, A3).
- Perf `stat gpu` overlay not captured by `HighResShot`; per-pass GPU numbers still missing.
