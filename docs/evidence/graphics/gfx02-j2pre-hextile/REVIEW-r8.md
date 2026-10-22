# GFX-02 J2-pre hex-tiled ground: independent review, round 8 (Fable, 2026-10-22)

Judged against `docs/decisions/GFX-02-photoreal-ground-pipeline.md` section 2a (Interim J2-pre, "J2-pre as shipped (r8)", Acceptance: gate `peak <= 0.25` on nine crops plus the human check "two or more copies of one motif inside a crop is a FAIL"), section 7 row J2-pre, `docs/blueprints/landscape-and-ground.md` section 2a, and the REVIEW-r7.md fix-3 pass condition. Domain: repeating motifs on cobble, earth or grass at A1-A3, plus anything the hex-tiling introduced (cells, seams, brightness steps, double exposure, loss of relief, regressions). Every frame in `before/`, `after/`, `before-measure/`, `after-measure/`, the six compare sheets and the r8 bisect folders (`bisect-bs24rot1`, `bisect-bs32rot1`, `bisect-bs32rot1-measure`, `bisect-T075`, `bisect-T075-measure`) were read; gate rectangles cut before|after at 3-10x from the measure frames, ground-level windows at 2-6x, sun views as full frames and 2-4x crops, context views at 0.5x and 2-4x. `Measure-GroundRepeat.py` re-run on `after-measure/` into scratch: `GROUND_REPEAT PASS crops 9 max_peak 0.1666`, identical to the committed JSON; `before-measure/` `FAIL max_peak 0.665`. The spec's HLSL block equals `hex_tile.py` `HLSL` byte for byte. Metrics as in r4-r7: `sharp.py` (mean |dx|+|dy| of luminance), window std, band ratio = std of a Gaussian difference-of-scales band (sigma s minus sigma 2s, separable NumPy Gaussian) after/before at 0.7, 1.4, 2.8, 5.6, 11 px; rib-angle `R` with the builder's `rib_angle.py`; moss = fraction with G - R > 8. Every README round-8 number reproduces on the committed files (04 16.22, 04-sun20 18.24, ctx-ground-1 9.40, ctx-ground-2 7.69, ctx-ground-3 8.93, ctx-ground-4 mud 9.93, ctx-ground-4-sun20 10.15; A2 earth `R` 0.03, A3 0.57; bisect 04 at `Rot` 1.0 13.17 / 13.18, mud 9.03 / 9.72, T075 mud 9.37 / 9.94).

Tally: PASS 9 (A1, A2, 01, ctx-overview-south, ctx-overview-east, ctx-ground-3, ctx-ground-4-sun20, A1-sun20, A1-margin-sun20). FAIL 6 (A3, 04-forest-floor, 04-forest-floor-sun20, ctx-ground-1, ctx-ground-2, ctx-ground-4).

Verdict: NOT PASSED. Round 8 clears A2 (the mud lattice is gone at 100 m: `R` 0.92 -> 0.03, no two cells share a rib angle at 8x) and keeps every cobble, grass, overview and sun-border check. It does not clear the two defects that have failed since r5: the translucent patches at 04 are still there at 1x in both 04 frames (the same spot, the same size, with `BlendSharp` 7, 24 and 32 and with `Rot` 0.15 and 1.0), the ground-level windows lost contrast again instead of regaining it (04 slab window std 27.4 before, 27.8 in r7, 23.8 now; ctx-ground-2 7.86 -> 7.69; ctx-ground-4 mud 10.76 -> 9.93), and the A3 margin keeps one-angle rib rows in the upper 100 px of its crop. The cause of the first two is the r7 fix-1 lattice warp as built: +-0.25 tile over 2.5 m features with two octaves has a slope of the same order as the lattice itself (up to ~1.3 lattice units per lattice unit), so the warped lattice coordinate stalls and folds, and the blend band, which `BlendSharp` 24 should have narrowed to ~3 % of a cell, becomes a wide patch wherever the warp runs against the lattice. That is why the r8 rule run (`Rot` 1.0 at `BlendSharp` 24 / 32: 13.17 / 13.18, std 19.4, band 0.49 at 11 px, worse than `bisect-base` 20.6 / 0.60) could not test the band hypothesis: the bands were never narrow during the test. The per-layer `Rot` (A 0.15, T 0.5) is therefore an interim read on a confounded measurement, and the rule has to be run again once the warp is gentle.

## A1-south-approach-30m — PASS

- Gate (after-measure): cobble [876,850,256,222] 0.119 at (12,-30), earth [705,730,110,96] 0.124 at (-15,-6), grass [175,440,128,128] 0.167 at (-15,0); before 0.225 / 0.544 / 0.244. Cobble 3x: sand, no motif. Earth 6x at [700,725,120,100]: mottle with a diagonal grain, no diamond lattice (the before has it), no two identical runs; `R` 0.93 (before 0.95) is the same one-direction grain as in r7, below texel size at 30 m. Grass 4x and 3x at [150,570,250,140], [1300,520,250,140]: moss patches of unequal outline, no honeycomb.
- Against `gameplay-hud.png` far range: same layout, road, tree line, wall and river as r7; one continuous meadow.

## A2-south-approach-100m — PASS

- Gate: cobble [885,790,110,256] 0.096, earth [815,600,64,256] 0.095, grass [1230,800,256,256] 0.062.
- Earth margin 8x at [805,580,80,200] and 6x at [815,600,64,256]: granular mottle, the diagonal dash rows of the before are gone; no two adjacent cells carry the same angle; `R` 0.03 (before 0.92; 16 blocks of 32 px). The r7 difference 2 is cleared here. The r7 condition "no one-angle run longer than one cell (30 px)" is met.
- Grass 3x at [1230,800,256,256] and 2x at [0,250,500,350]: moss patches of different outline and dash angle.
- Cobble 3x: sand, no motif.

## A3-south-approach-250m — FAIL

- Gate: cobble [925,600,55,215] 0.070, earth [884,600,32,215] 0.122 at (-2,-18), grass [1075,520,190,190] 0.083.
- Earth margin 10x at [880,600,45,150] and [884,600,32,100] (before | after | `bisect-bs32rot1-measure`): the after keeps six rows of dark diagonal dashes at one angle (upper-left to lower-right) across the full 32 px width over the top ~100 px of the crop, i.e. 70-80 px of one-angle run against cells of 6-12 px; the lower half is granular. `R` on [884,600,32,215] 0.57 (before 0.76), on the top [884,600,32,100] 0.61; the `Rot` 1.0 capture reads 0.33 on the same top strip, so the rows are layer T and `Rot` 0.5 does not turn them at 250 m. Two or more copies of one motif inside the crop: FAIL on the human check.
- The r7 number condition "`R` <= 0.4 on the whole crop" is ill-posed at A3: the A3 cobble crop (sand, no motif) reads 0.66 and the A3 grass crop 0.63 at 16 px blocks, so the view's foreshortening alone gives ~0.6 and the whole-crop figure cannot reach 0.4. The condition is restated in difference 2 (top strip [884,600,32,100] <= 0.40, where `Rot` 1.0 already measured 0.33, plus the run-length check at 10x).
- Grass 4x and 2x at [1300,650,500,400], [780,500,520,330]: patches vary; cleared. Cobble 4x: sand.
- Fix type: CODE. Difference 2 (after difference 1).

## 01-forest-path-town — PASS

Cobble 2x at [300,800,500,280]: continuous courses through every hex border, no doubled stone, no seam, no direction change; sharpness 5.26 -> 5.09, band 0.98-0.99 at every octave. Full frame: same town, road and trees.

## 04-forest-floor — FAIL

- Met: [600,450,400,250] sharpness 16.95 -> 16.22 (>= 15); 3x: 10+ flat slabs with crisp edges and dark undersides; gravel [1200,700,300,150] 14.79 -> 15.10; no straight crossfade line at 3x in [600,450,400,250], [1300,300,300,200], [100,500,500,300], [0,900,400,180].
- What's wrong (most visible): translucent double-image patches where two taps blend, visible at 1x in the upper right of the after on `04-forest-floor-before-after-compare.jpg`. 6x at [1380,290,220,110]: a pale, speckled, relief-less area about 150 x 70 px at full-frame [1430,310,150,70], mean RGB (160,152,78) against (146,132,64) for [1250,250,450,220]; the before has a crisp slab with a dark underside there. 6x at [1600,150,260,90]: a green-grey smear about 130 x 50 px at [1720,175,130,50] with no texel structure. 3x at [1250,250,450,220]: the before's 1.3 m slab is replaced by soft olive mush over about 250 x 120 px. The same patch, same place, same size, is in `bisect-bs32rot1/04-forest-floor.jpg` (`BlendSharp` 32, `Rot` 1.0) and was in r7 (`BlendSharp` 7, no warp): the blend exponent did not touch it.
- Window numbers: [600,450,400,250] std 27.4 -> 23.8 (r7 after 27.8), band ratio 1.00 / 1.00 / 0.91 / 0.74 / 0.66 (r7 condition >= 0.8 at every octave: not met at 5.6 and 11 px); [1300,300,300,200] 17.35 -> 16.78, std 31.4 -> 27.3, band 0.95 / 0.97 / 0.97 / 0.85 / 0.69 (condition >= 0.8 at 11 px: not met, r7 read 0.71); [1250,250,450,220] band 0.89 / 0.89 / 0.85 / 0.75 / 0.57. Fine octaves are intact (the texels are sharp); the coarse octaves, the slab shapes, are what the wide blend zones average away.
- Near field [0,900,400,180] 10.65 -> 8.98 (band 0.86 at 0.7 px, 1.29 at 11 px): 3x shows larger slabs with softer edges; a different texel, not a defect by itself.
- Fix type: CODE. Difference 1.

## 04-forest-floor-sun20 — FAIL

Sun view real (twin difference 57.1; before|after sun frames differ by 20.6). Lit strip [1400,200,520,380] 18.40 -> 18.24 (>= 17 met), [1000,100,900,500] 22.98 -> 21.32; 3x at [1000,150,300,200]: crisp pebbles across the shadow edge, no brightness step along any hex border. What's wrong: the same translucent patch as the default view, at 1x on `04-forest-floor-sun20-before-after-compare.jpg` as a pale blotch at the left of the lit strip; 4x at [1380,300,260,130]: a pale speckled area about 150 x 80 px with no slab relief where the before has one lit slab with a shadowed underside. Judged under difference 1; failed here because it is visible at 1x in this frame too.

## Context views

### ctx-overview-south, ctx-overview-east — PASS
Fields at 300 m: even mottle, no rows, no lattice, no honeycomb, moss sheets 5-40 m; sharpness 3.27 -> 3.28 and 3.44 -> 3.31 on [0,300,1920,780], band 0.99-1.00 at every octave. The world reads the same from above as from A1-A3; nothing clipped, floating or defaulted.

### ctx-ground-1 — FAIL
- 4x at [500,900,300,150]: about 20 discrete pebbles and slabs with an edge (>= 15 met); sharpness 9.73 -> 9.40, band 0.99-1.02 to 5.6 px, 0.87 at 11 px; [300,760,600,220] 8.67 -> 8.29.
- What's wrong: the dark green-grey double-image blob at [503,1006,63,24] is still there (6x at [500,960,200,90]: a smear about 60 x 30 px with no texel structure, the before has pebbles). Mean RGB (139,132,67) against (145,131,63) for [400,950,300,130]: R differs by 6.2 (r7 condition: within 4/255 per channel; not met). Same defect as 04, below 1x visibility; failed on the stated condition.
- Fix type: CODE. Difference 1.

### ctx-ground-2 — FAIL
- Moss in the lower band 0.053 -> 0.063 (<= 0.10 met). Lichen carpet [1100,780,400,200] 7.69 -> 7.52, one patch, dashes at several angles. 2x at [0,700,900,380]: no seam, no streak.
- What's wrong: [200,900,300,150] sharpness 8.74 -> 7.69 (condition >= 8.5; r7 read 7.86), std 10.7 -> 10.2, band 0.87 / 0.88 / 0.90 / 0.85 / 0.90, flat across octaves; [200,900,300,120] 8.47 -> 7.73. 4x: the same fine olive mottle as the before, uniformly softer. The contrast loss of difference 1 over the band fraction of the surface, 2 % worse than r7.
- Fix type: CODE. Difference 1.

### ctx-ground-3 — PASS
[100,850,400,200] 9.15 -> 8.93 (>= 8.5 met); [80,880,300,110] 8.46 -> 8.94; moss 0.011 -> 0.032 (<= 0.10 met). 5x at [80,880,300,110]: pebbles; three or four dark sticks at one angle in the lower right of the window (a texel of the mud scan, not a row that continues across cells). The before's faint field hatch at 20-60 m (visible at 0.5x) is gone in the after.

### ctx-ground-4 — FAIL
- Cobble [1000,900,400,180] 4.93 -> 4.78, 3x: continuous courses, no seam. Far mud 3x at [0,600,600,130] 5.55 -> 5.04: the before's faint one-angle ribbing at 10-20 m is gone; `R` 0.99 -> 0.98 is the view's own anisotropy (the same on the grass half of the window).
- What's wrong: mud [100,850,300,150] 11.15 -> 9.93 (condition >= 10; r7 read 10.76, so r8 lost 0.8 on this window), [500,750,300,150] 8.76 -> 8.27, [0,700,900,380] 8.95 -> 8.46, band 0.91 / 0.96 / 0.97 / 1.03 / 0.93. 4x: clumpy granular mud with crack rings, no left-right drag, softer than the before. Failed on the number; the loss is the same band contrast loss as ctx-ground-2 (difference 1), with T's `Rot` 0.5 contributing (the `bisect-T075` window reads 9.37).
- Fix type: CODE. Difference 1.

## Sun-20 views

### A1-sun20 — PASS
Twin difference 38.3. 2x [700,500,500,300] 5.53 -> 5.27, 4x [640,650,220,220] 8.27 -> 7.94: no polygonal step, no seam, no cell edge on R, T or grass; band 0.98-1.00.

### A1-margin-sun20 — PASS
Before|after 5.0 (only the ground differs). 3x at [500,820,400,250]: courses continuous through every hex border, no mortar-line seam, no tone step (6.10 -> 5.79, band 0.96-1.13). Earth margin at 1-5 m granular, no cell, no step.

### ctx-ground-4-sun20 — PASS
Twin difference 44.7. Mud [0,700,900,380] 10.49 -> 10.15 (>= 10 met), band 0.97 / 1.00 / 1.00 / 0.94 / 0.92; 2x: no directional drag. Cobble 2x [900,700,600,300] 5.03 -> 4.91, no steps.

## Measurement credibility, r7 fix items and the deviations

1. Gate reproduces (`max_peak 0.1666`). As in r5-r7 it passes the A3 earth crop (0.122) that fails the human check: broken rows kill the one-lag autocorrelation peak while the motif stays legible.
2. Perf: `perf-01.json` median 7.33 -> 7.39 ms (+0.06, within +1), p95 9.67 -> 8.80; `ProfileGPU` BasePass 0.838 -> 1.264 ms (+0.43, within +0.6; r7 1.302, so the two Simplex noises cost nothing measurable); every other pass within 0.11 ms. Met.
3. Sun views credible (twins 57.1 / 38.3 / 44.7 > 8).
4. r7 fix 1 (`BlendSharp`, `Warp`, lattice from the warped coordinate, `hwarp`, 24 on every hex layer): implemented exactly as written; `hex_tile.py` and `import_ground.py` match the r7 text, the committed `MF_GN_HexTile.uasset` and the spec block carry it. Acceptance not met on five of its eight points: 04 patches > 40 x 40 px at both rectangles; [1300,300,300,200] band 0.69 at 11 px; ctx-ground-2 7.69; ctx-ground-1 blob R +6.2; and the new numbers 04 std 23.8 (r7 27.8) and ctx-ground-4 mud 9.93 (r7 10.76) say the fix moved the frames the wrong way. Met: no straight crossfade line; 04 >= 15 and 04-sun20 >= 17; cobble continuous at 01, ctx-ground-4, A1-margin-sun20. The reason is in the instruction itself, which this review inherits: `scale` 0.004 (2.5 m) with `levels` 2 and `HexWarp` 0.25 tile gives the warp a slope of amplitude x 2 pi / period per octave = 0.25 x 6.28 / 1.25 tiles (2.5 m over the 2 m tile) = 1.26 lattice units per lattice unit for the first octave and the same again for the second; the lattice gradient is 1. Where the warp runs with the lattice the band is narrow; where it runs against it the lattice coordinate stalls and reverses, and the band (w near 0.5 over the whole stalled stretch, so `pow(w, 24)` does nothing there) becomes a patch of tens of centimetres. The r7 instruction said "above 0.4 adjacent borders cross"; the number that matters is the slope, and 0.25 at 2.5 m is already over it. Evidence: the 04 patch is unchanged between `BlendSharp` 7 (r7), 24 and 32 (`bisect-bs32rot1`) at the same world spot; 04 std fell from 27.8 (r7, no warp) to 23.8 with `Rot` unchanged; `bisect-bs32rot1` at `Rot` 1.0 reads std 19.4 / band 0.49 against `bisect-base` (no warp, `BlendSharp` 7) 20.6 / 0.60. The first fix below makes the warp gentle (slope ~0.2) and adds a `HexWarp` 0 control capture so the attribution is measured, not argued.
5. r7 fix 2 (`Rot` 1.0 rule): run as specified (`bisect-bs24rot1` 13.17, below 14, stop, `BlendSharp` 32 one more capture, 13.18) and the bisect frames reproduce. The builder's reading "bands ruled out as the cause of the `Rot` 1.0 loss" does not follow: the test ran with the folded lattice (item 4), so the bands were wide in every variant and the exponent could not act. The hypothesis is untested, not refuted; the spec paragraph's "Neither `BlendSharp` 24 nor 32 recovers 04 at full rotation" is true of the r8 build and must not be read as a property of narrow bands. Deviation (A 0.15, T 0.5, S/B 1.0, R 0.0): accepted as the interim that keeps 04 >= 15 and clears A2, not as the shipped value; the rule is re-run after fix 1 (fix 2 below). T at 0.5: A2 cleared (`R` 0.03), A3 not (0.57; top strip 0.61 against 0.33 at `Rot` 1.0), and the 1-5 m mud objection (9.03 at `Rot` 1.0) is itself a band-loss number from the folded build.
6. r7 fix 3 (recapture, pairs, gate, perf, README table with `R`): done; README round 8 reports the window table and `R` for A1-A3 earth. Pass condition not met (items above).
7. r7 fix 4 (docs): spec "J2-pre as shipped (r8)" paragraph carries the frame sentence as instructed, the new inputs and the regenerated HLSL (byte-identical to `hex_tile.py`); blueprint 2a "Today (J2-pre)" row updated. `docs/dependency-map.md` row not updated: still "layers A, R, T, S" and "(REVIEW-r1/REVIEW: not passed ...)". Not done.
8. Evidence chain: `git log` on `after/04-forest-floor.jpg` returns 85aa6728 (r2); rounds 3-8 (REVIEW-r3..r7, bisect folders, the r3-r8 frames, `hex_tile.py` / `import_ground.py` / the two `.uasset` changes) are uncommitted in this worktree, and the r3-r7 `after/` frames are overwritten and unrecoverable. The r7 numbers in this review are quoted from REVIEW-r7.md, not reproduced on frames. Fix 5.

## Differences, most visible first

1. Translucent double-image patches at 1-5 m and a coarse-octave contrast loss on every ground-level window (04 [1430,310,150,70] and [1720,175,130,50], both at 1x on the compare sheets; 04-sun20 [1380,300,260,130]; ctx-ground-1 [503,1006,63,24] R +6.2; ctx-ground-2 7.69 < 8.5; ctx-ground-4 mud 9.93 < 10; 04 slab window std 23.8 against 27.4 before and 27.8 in r7; band 0.66 / 0.69 at 11 px on the two 04 windows).
   What's wrong: the r7 lattice warp is too steep for a 2 m cell (item 4 above): +-0.25 tile over 2.5 m with two octaves folds the lattice, and in a fold the three weights sit near 0.5 / 0.5 over tens of centimetres whatever `BlendSharp` is. The 2.5 m feature size was chosen to bend a 2 m border; a 10 m feature at 0.15 tile still displaces a border by 30 cm and bends a 2 m segment by ~14 cm, which is what hides a straight line at 1-5 m, while its slope is 0.15 x 6.28 / 5 = 0.19 lattice units per lattice unit: band widths vary by +-19 % and never fold.
   Fix type: CODE. Exact: `GameClient/ArtSource/Greenlands/NatureFamilies/import_ground.py`, replace lines 108-112
   ```python
       def hex_noise(y, zoff):  # r7 review fix 1: hex lattice warp, 2.5 m features, +-HexWarp tile
           nz = node(m, u.MaterialExpressionNoise, -1700, y, noise_function=u.NoiseFunction.NOISEFUNCTION_SIMPLEX_TEX, scale=0.004, levels=2, output_min=-1.0, output_max=1.0, quality=2, turbulence=False)
           link(g.op(u.MaterialExpressionAdd, hwp, node(m, u.MaterialExpressionConstant3Vector, -1900, y, constant=u.LinearColor(0.0, 0.0, zoff, 0.0)), -1800, y), '', nz, 'World Position'); return nz
       hxy = node(m, u.MaterialExpressionAppendVector, -1500, 11000); link(hex_noise(11000, 0.0), '', hxy, 'A'); link(hex_noise(11150, 8192.0), '', hxy, 'B')
       hwarp = g.mul(hxy, g.sc('HexWarp', 0.25, -1500, 11100), -1350, 11000)  # tiles; bounds 0-0.4 (above 0.4 adjacent borders cross)
   ```
   with
   ```python
       def hex_noise(y, zoff):  # r8 review fix 1: hex lattice warp, 10 m features, one octave, +-HexWarp tile
           nz = node(m, u.MaterialExpressionNoise, -1700, y, noise_function=u.NoiseFunction.NOISEFUNCTION_SIMPLEX_TEX, scale=0.001, levels=1, output_min=-1.0, output_max=1.0, quality=2, turbulence=False)
           link(g.op(u.MaterialExpressionAdd, hwp, node(m, u.MaterialExpressionConstant3Vector, -1900, y, constant=u.LinearColor(0.0, 0.0, zoff, 0.0)), -1800, y), '', nz, 'World Position'); return nz
       hxy = node(m, u.MaterialExpressionAppendVector, -1500, 11000); link(hex_noise(11000, 0.0), '', hxy, 'A'); link(hex_noise(11150, 8192.0), '', hxy, 'B')
       # tiles; bounds 0-0.2. The warp's slope (HexWarp x 2 pi / period in tiles, per octave) must stay well under the lattice slope of 1: 0.15 x 6.28 / 5 = 0.19.
       # r7's 0.25 at 2.5 m with two octaves was 1.26 + 1.26: the lattice folded and the blend band became 20-40 cm patches that no BlendSharp narrows (r8 review).
       hwarp = g.mul(hxy, g.sc('HexWarp', 0.15, -1500, 11100), -1350, 11000)
   ```
   `hex_tile.py` unchanged. Before the recapture, one control: `GFX_ONLY=04-forest-floor` capture with `HexWarp` 0 into `bisect-warp0/` (the `Rot` and `BlendSharp` of the shipped build), then the new warp. Record std and band of [600,450,400,250] and [1300,300,300,200] for both in the README.
   Acceptance: 04 6x at [1380,290,220,110] and [1600,150,260,90]: no translucent patch larger than 40 x 40 px (every 40 x 40 window contains a stone edge); [600,450,400,250] std >= 26 and band >= 0.8 at 5.6 and 11 px; [1300,300,300,200] band >= 0.8 at 11 px; 04-sun20 4x at [1380,300,260,130]: slab relief present, no pale area > 40 x 40; ctx-ground-1 [503,1006,63,24] within 4/255 per channel of [400,950,300,130]; ctx-ground-2 [200,900,300,150] >= 8.5; ctx-ground-4 mud [100,850,300,150] >= 10 (at the `Rot` fix 2 settles on); no straight crossfade line longer than 60 px at 3x in any 04 or ctx-ground window (if one shows, `HexWarp` 0.15 -> 0.2 before anything else); cobble courses continuous at 01 [300,800,500,280], ctx-ground-4 [1000,900,400,180], A1-margin-sun20 [500,820,400,250].

2. Earth margin rib rows at A3 [884,600,32,100] (`R` 0.61, six one-angle rows over ~75 px against 6-12 px cells; 0.33 at `Rot` 1.0 on the same strip): T at `Rot` 0.5 turns the lattice enough at 100 m and not at 250 m. A at 0.15 is an interim read from the folded build (item 5).
   What's wrong: the r8 rule run could not measure what full rotation does with narrow bands, and the 1-5 m mud smear that justified 0.5 (9.03 at `Rot` 1.0) is a band number from the same build.
   Fix type: CODE. Exact: after fix 1 is built, `import_ground.py` line 149 `rot=0.15` -> `rot=1.0` and line 211 `rot=0.5` -> `rot=1.0`; comment on lines 137-138 rewritten to the measured facts once the run is done ("warp slope 0.19; 04 at Rot 1.0 <value>, std <value>"). One capture set at `Rot` 1.0 on A and T, then the rule: 04 [600,450,400,250] >= 15 with std >= 26 keeps 1.0 everywhere; 14-15 keeps T and S at 1.0 and sets A to the largest of 1.0, 0.5, 0.15 that reads >= 15 with std >= 26; below 14 stop, record the row with the `bisect-warp0` control beside it, and report (no further exponent captures). T stays 1.0 unless ctx-ground-4 mud [100,850,300,150] reads below 9.5 AND a directional drag is visible at 4x, in which case T 0.75; the A3 strip is the deciding view for T (the owner's complaint is the 20-200 m grid, section 2a), the mud window the secondary.
   Acceptance: A3 [884,600,32,100] `R` <= 0.40 at 16 px blocks (the strip where `Rot` 1.0 measured 0.33) and A2 [815,600,64,256] `R` <= 0.10 at 32 px; at 10x no run of one-angle dashes longer than 20 px anywhere in [884,600,32,215] or [815,600,64,256]; gate <= 0.25 on all nine; ctx-ground-4 mud [100,850,300,150] >= 10 and ctx-ground-4-sun20 [0,700,900,380] >= 10 with no directional drag at 4x; A1-margin-sun20 earth at 1-5 m granular, no cell, no step. (The whole-crop A3 `R` is not a condition: sand and grass at A3 read 0.63-0.66 at 16 px from the view's foreshortening.)

3. ctx-ground-2 7.69, ctx-ground-4 mud 9.93, ctx-ground-1 blob: the band contrast loss of difference 1. No separate fix; acceptance listed under 1.

4. Documentation and evidence chain.
   What's wrong: `docs/dependency-map.md` row (line 136) still says hex layers "A, R, T, S" and "(REVIEW-r1/REVIEW: not passed, open fixes listed there)" (r7 fix 4 asked for "A, B, R, T, S" and "REVIEW-r1..r8"); the spec paragraph states `HexWarp` 0.25 and the "neither 24 nor 32 recovers 04" sentence as if bands were tested; nothing after r2 is committed.
   Fix type: CODE (docs). Exact: dependency-map row: "uses it for layers A, B, R, T, S" and "reviews REVIEW-r1..r9 in `docs/evidence/graphics/gfx02-j2pre-hextile/`"; spec "J2-pre as shipped (r9)": `HexWarp` 0.15 at 10 m one octave with the slope rule, the fix-2 `Rot` values and the 04 number at `Rot` 1.0 with the gentle warp, the sentence "Neither `BlendSharp` 24 nor 32 recovers 04 at full rotation (13.2)" replaced by the r9 measurement with the `bisect-warp0` control; blueprint 2a row: the r9 values; README Round 9 with the window table (before, r7 from REVIEW-r7.md, r8, r9), the `bisect-warp0` control row and `R` for A1-A3 earth plus the A3 top strip.

## Fix list (job domain, deduplicated)

1. Engineer — `import_ground.py` lines 108-112: lattice warp `scale` 0.004 -> 0.001, `levels` 2 -> 1, `HexWarp` 0.25 -> 0.15 (bounds 0-0.2, slope comment); `bisect-warp0/` control capture of 04 (difference 1).
2. Engineer — after fix 1: `layer()` A `rot=1.0` (line 149) and T `rot=1.0` (line 211), one capture set, the 04 / mud / A3-strip decision rule in difference 2; lines 137-138 comment rewritten to the measured values (difference 2).
3. Codex — re-run `import_ground.py`; recapture `after/` (all views, four sun views) and `after-measure/`; gate on both measure folders; `perf_gpu.py` and the 500-frame timing; rebuild pairs; README Round 9 with the window table, the `bisect-warp0` row and rib-angle `R` (A1-A3 earth crops at 32 / 16 px plus A3 [884,600,32,100] at 16 px); fresh review. Pass condition: nine crops <= 0.25; A3 [884,600,32,100] `R` <= 0.40 and A2 [815,600,64,256] `R` <= 0.10, no one-angle dash run longer than 20 px at 10x in either earth crop; A2 [1230,800,256,256] and A3 [1075,520,190,190] at 3-4x with no two moss patches sharing angle and outline; after/04 [600,450,400,250] >= 15 with std >= 26, slabs and dark undersides at 3x, band >= 0.8 at every octave; [1300,300,300,200] band >= 0.8 at 11 px; no translucent patch > 40 x 40 px at 6x in [1380,290,220,110] or [1600,150,260,90] and none at 4x in 04-sun20 [1380,300,260,130]; no straight crossfade line > 60 px at 3x in any 04 or ctx-ground window; 04-sun20 [1400,200,520,380] >= 17, no brightness step along a hex border; ctx-ground-1 [500,900,300,150] >= 15 pebbles with an edge and [503,1006,63,24] within 4/255 of its surround; ctx-ground-2 [200,900,300,150] >= 8.5 and ctx-ground-3 [100,850,400,200] >= 8.5 with no one-angle dash row in [80,880,300,110]; ctx-ground-4 mud [100,850,300,150] >= 10 and ctx-ground-4-sun20 [0,700,900,380] >= 10, no directional drag at 4x; cobble courses continuous at 01 [300,800,500,280], ctx-ground-4 [1000,900,400,180], A1-margin-sun20 [500,820,400,250]; moss coverage ctx-ground-2 and ctx-ground-3 lower band <= 0.10; sun views differ from their twins by > 8; perf median at 01 within +1 ms of the before run and BasePass within +0.6 ms.
4. Codex — dependency-map row, GFX-02 "J2-pre as shipped (r9)" paragraph (`HexWarp`, `Rot`, the band sentence, HLSL block regenerated if `hex_tile.py` changes), blueprint 2a row (difference 4).
5. Codex — commit the r3-r8 state of this worktree now (code, the two `.uasset`, REVIEW-r3..r8, bisect folders, frames, README), then r9 as its own commit, so the next review can reproduce the previous round's numbers on frames (item 8).

## Carried forward (not this job)

- Aerial colour blend 25-60 m and macro luminance variation per layer (J2).
- Low-poly lime-green blob trees and bushes at A1-A3 and the overviews; identical house rows; bare plain with no fields, hedges or tracks between town and river.
- White blob mountains without rock or snow detail (A1, 01, ctx-ground-1/2/4).
- Cobbled carriageway where `gameplay-hud.png` far range shows a dirt track; beige cobble vs the concept's grey-brown stone.
- Bookmark 04 vs `Greenlands-Forest-Look-v1.webp`: no leaf litter, needles or undergrowth variety; hard 1 px moss-patch edges at 1 m from `Low 0.4 / InvRange 32` (present in the before).
- Hard-edged river bank and flat water colour (A2, A3).
- `mossy_rock` crack dashes as the moss-patch texture: a texture choice for layer B.
- The A1 earth margin's one-direction grain (`R` 0.93 at 30 m, reads as mottle): revisit with the landscape material's PackedEarth scan in J2.
