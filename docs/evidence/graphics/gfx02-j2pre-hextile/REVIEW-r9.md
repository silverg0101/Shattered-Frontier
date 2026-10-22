# GFX-02 J2-pre hex-tiled ground: independent review, round 9 (Fable, 2026-10-22)

Judged against `docs/decisions/GFX-02-photoreal-ground-pipeline.md` section 2a ("J2-pre as shipped (r9)", Acceptance: gate `peak <= 0.25` on nine crops plus the human check "two or more copies of one motif inside a crop is a FAIL"), section 7 row J2-pre, `docs/blueprints/landscape-and-ground.md` section 2a, and the REVIEW-r8.md fix list with its stated acceptance. Domain: repeating motifs on cobble, earth or grass at A1-A3, plus anything the hex-tiling introduced (cells, seams, brightness steps, double exposure, loss of relief, regressions). Round 9 is commit d4581f87; round 8 is fccc5c62 and its `after/` and `after-measure/` frames were extracted with `git show` and read beside the r9 frames, so every r8 number below is reproduced on frames, not quoted. Every frame in `before/`, `after/`, `before-measure/`, `after-measure/`, the six compare sheets, `bisect-warp0/`, `bisect-r9rot1/`, `bisect-T075r9/`, `bisect-base/`, `bisect-bs32rot1/` and the r6 GBuffer dumps (`dump-after/`, `dump-before/`) were read; gate rectangles cut before | r8 | r9 at 3-10x from the measure frames, ground-level windows at 2-6x, sun views as full frames and 2-4x crops, context views at 0.5x and 2-5x; the two scans (`Sources/forest_ground_04`, `Sources/brown_mud_02`, Poly Haven) and the Mask's source map were read as well. `Measure-GroundRepeat.py` re-run into scratch: `after-measure/` `GROUND_REPEAT PASS crops 9 max_peak 0.1882`, identical to the committed JSON; `before-measure/` `FAIL max_peak 0.665`. Metrics as in r4-r8: `sharp.py` (mean |dx|+|dy| of luminance, window std), band ratio = std of a Gaussian difference-of-scales band (sigma s minus 2s) after/before at 0.7, 1.4, 2.8, 5.6, 11 px, rib-angle `R` with `rib_angle.py`, moss = fraction with G - R > 8. Every README round-9 number reproduces on the committed files (04 14.99 / 26.2, 04-sun20 18.36, ctx-ground-1 9.25, ctx-ground-2 7.67, ctx-ground-3 9.00, ctx-ground-4 mud 9.80, ctx-ground-4-sun20 10.31; `bisect-warp0` 13.29 / 20.6, `bisect-r9rot1` 12.89 / 19.9, mud 8.61, `bisect-T075r9` mud 9.03; A1 earth `R` 0.93, A2 0.27, A3 0.70, A3 top strip 0.76; sun twins 57.1 / 38.3 / 44.7).

Tally: PASS 11 (A1, A2, 01, ctx-overview-south, ctx-overview-east, ctx-ground-2, ctx-ground-3, ctx-ground-4, ctx-ground-4-sun20, A1-sun20, A1-margin-sun20). FAIL 4 (A3, 04-forest-floor, 04-forest-floor-sun20, ctx-ground-1).

Verdict: NOT PASSED. Round 9 did what the r8 fix 1 asked and the bands are now narrow: the coarse octaves at 04 came back (band 0.74 -> 0.95 at 5.6 px and 0.85 -> 1.03 at 11 px on [600,450,400,250], std 23.8 -> 26.2; 0.79 -> 0.91 at 11 px on [1300,300,300,200]; 04-sun20 0.84 -> 0.94), every cobble, grass, overview and sun-border check holds, and the gate passes with margin. The translucent patches did not move, and the `bisect-warp0` control proves why: they are not the hex blend at all. They are the moss-weight threshold. `wm = saturate((Mask - Low 0.4) x InvRange 32)` with `Mask` = `forest_ground_04_Displacement` sampled at full resolution on a 9 m tile; that map's median is 0.380 (p5 0.329, p95 0.459), so `Low` 0.4 sits in the middle of its histogram, 15 % of its texels are partial and at 7-28 cm scale 10-14 % of the ground is a texel-dithered 20-80 % blend of forest floor and mossy rock. The r6 GBuffer dump shows it: WorldNormal is flat over the patch (a lerp of two normals), BaseColor is greener and lower in contrast. r5-r8 chased this patch through `NEG_ANGLE`, `HeightSharp`, `BlendSharp` 7 / 24 / 32, the world-space frame and two lattice warps; none of those touch `wm`, which is why the patch sat at the same spot and size in every build, and why it survives `HexWarp` 0 unchanged. The r8 review, which this one inherits, misattributed it. The second open defect, the one-angle rib rows on the A3 earth margin, is real and is layer T's rotation: full rotation turns them (`R` 0.48 on the top strip, below the view's own sand baseline 0.65) but costs the 1-5 m mud 23 % of its relief (8.61 against 11.15), and the r8 rule's two acceptance numbers (`R` <= 0.40, mud >= 10) cannot both be met by one scalar. The rule's own deciding view (the A3 strip) said T 1.0; the builder shipped 0.5 on the secondary number. The fix is not a better scalar: `Rot` on T becomes a depth ramp (0.5 inside 20 m, 1.0 beyond 80 m), so each view gets the value that passes it.

## A1-south-approach-30m — PASS

- Gate (after-measure): cobble [876,850,256,222] 0.114 at (11,-30), earth [705,730,110,96] 0.093 at (22,-13), grass [175,440,128,128] 0.188 at (17,-1); before 0.225 / 0.544 / 0.244. Cobble 3x: sand, no motif, no course change. Earth 6x at [700,725,120,100]: the before's diamond lattice is gone; mottle with a one-direction diagonal grain (`R` 0.93, before 0.95, r8 0.93), no two identical runs. Grass 4x at [150,420,200,170]: moss patches of unequal outline; the before's identical cross-shaped patches are gone.
- Against `gameplay-hud.png` far range on `A1-hud-before-after-compare.jpg`: same layout, road, tree line, wall and river as r8; one continuous meadow; nothing clipped, floating or defaulted.

## A2-south-approach-100m — PASS

- Gate: cobble [885,790,110,256] 0.113, earth [815,600,64,256] 0.110 at (-10,-43), grass [1230,800,256,256] 0.067.
- Earth margin 8x at [805,580,80,200] (before | r8 | r9): the before's diagonal dash rows are gone in both afters; granular, no run of one-angle dashes longer than one cell. `R` on the measure frame reads 0.27 (r8 0.03), on the `after/` frame 0.06 (r8 0.47): 16 blocks of 32 px, and T did not change between r8 and r9 (only the lattice borders moved), so the number swings by 0.25-0.4 between captures of the same layer. The r8 condition "`R` <= 0.10" is therefore not met on the measure frame and not reliable at this sample count; the view is passed on the 8x crop, where the r7 condition (no one-angle run longer than 30 px) holds.
- Grass 3x at [1230,800,256,256]: moss patches of different outline and dash angle. Cobble 3x: sand. 2x at [600,450,700,400]: same road, margins, trees and river as before.

## A3-south-approach-250m — FAIL

- Gate: cobble [925,600,55,215] 0.068, earth [884,600,32,215] 0.131 at (-2,-17), grass [1075,520,190,190] 0.082.
- Earth margin 8x at [880,600,45,215] and 10x at [884,600,32,100] (before | r8 | r9): the before carries a diamond lattice of dark dashes down the whole strip. r9 is granular over the lower three quarters but the top 25-30 px of the strip carries three rows of dark diagonal dashes (upper-left to lower-right) across the full 32 px width, i.e. ~30 px of one-angle run against 10 px cells. `R` on [884,600,32,100] 0.76 (before 0.90, r8 0.61), on the lower strip [884,700,32,115] 0.53 (r8 0.71); the `Rot` 1.0 build reads 0.48 on the top strip (r8 0.33). The view's foreshortening alone gives `R` 0.65 on the sand crop and 0.62 on the grass crop at 16 px blocks, so the r8 number "`R` <= 0.40" cannot be reached by any rotation; the deciding evidence is the 10x crop, and it shows two or more copies of one motif inside the crop. FAIL on the human check.
- Grass 4x at [1075,520,190,190]: patches of unequal outline; cleared. Cobble 4x: sand. Full frame: same town, bridge, river and fields as before, mottle even, no lattice in the fields.
- Fix type: CODE. Difference 2.

## 01-forest-path-town — PASS

Cobble 2x at [300,800,500,280]: continuous courses through every hex border, no doubled stone, no seam, no direction change; sharpness 5.26 -> 5.08, band 1.00-1.05 at every octave. Full frame: same town, road and trees.

## 04-forest-floor — FAIL

- Met: [600,450,400,250] std 26.2 (>= 26), band 0.93 / 0.97 / 0.99 / 0.95 / 1.03 (>= 0.8 at every octave; r8 0.74 / 0.85 at 5.6 / 11 px); 3x: 10+ flat slabs with crisp edges and dark undersides; [1300,300,300,200] band 0.91 at 11 px (>= 0.8; r8 0.79); gravel [1200,700,300,150] 14.79 -> 15.14; no straight crossfade line at 3x in [600,450,400,250], [1300,300,300,200], [100,500,500,300], [0,900,400,180]. Sharpness 14.99 against the bar 15: a 0.01 miss inside the capture spread (the window's cell content moved with the lattice), judged met on the 3x crop.
- What's wrong (most visible, at 1x and at 0.5x on `04-forest-floor-before-after-compare.jpg`): the translucent patch in the upper right is still there and slightly paler than in r8. 6x at [1380,290,220,110]: a pale speckled relief-less area about 150 x 70 px at [1430,310,150,70], mean RGB (159.6, 150.8, 77.9) against (148.3, 133.4, 65.8) for [1250,250,450,220] (+11 / +17 / +12; r8 +9 / +15 / +11; before +15 / +17 / +9 for the scan's own pale zone there). 6x at [1600,150,260,90]: the green-grey smear about 130 x 50 px at [1720,175,130,50], G +10 and B +9 against the before at the same pixels, no texel structure. 3x at [1250,250,450,220]: the before's 1.3 m slab is soft olive mush over ~200 x 100 px with a ghost of the slab outline.
- Cause, measured: the same patch, same place, same size, is in `bisect-warp0/04-forest-floor.jpg` (`HexWarp` 0, `BlendSharp` 24, `Rot` 1.0), in `bisect-r9rot1/`, in `bisect-base/` (`BlendSharp` 7, no warp) and in `bisect-bs32rot1/`. Nothing inside `MF_GN_HexTile` moves it. `import_ground.py` line 164-170: `wm = saturate((forest_ground_04_Displacement at 9 m + nzA x 0.02 - 0.4) x 32)` and `forest = lerp(fa, fb, wm)`, `fn = lerp(na, nb, wm)`. The displacement map's values are p5 0.329 / p50 0.380 / p95 0.459; at `Low` 0.4 and `InvRange` 32, 15.3 % of the map is full moss, 15.1 % is partial, and in 14-28 cm blocks 12-14 % of the area averages to a 20-80 % blend: a texel-scale dither of forest floor and mossy rock, which the mip chain and TAA turn into pale speckle with a flat normal. The r6 `dump-after/04-forest-floor_WorldNormal.jpg` shows the flat normal over exactly this area and `_BaseColor.jpg` the green shift; `dump-before` has neither at this spot. The before has the same mechanism elsewhere (its Mask was unwarped); the r2 Mask warp and r3 `MaskNoise` moved these zones into the 04, 04-sun20 and ctx-ground-1 views. It is in this job's domain (double exposure and loss of relief introduced by this job's Mask change) and it is cheap to fix.
- Near field [0,900,400,180] 10.65 -> 8.81: 3x shows the scan's large smooth slab at 1 m (bottom-centre of `forest_ground_04_Diffuse`); content, not a defect.
- Fix type: CODE. Difference 1.

## 04-forest-floor-sun20 — FAIL

Sun view real (twin difference 57.1; before | after sun frames differ by 20.9). Lit strip [1400,200,520,380] 18.40 -> 18.36 (>= 17 met), std 43.6 -> 43.5, band 1.00 / 1.02 / 0.99 / 0.96 / 0.94 (r8 0.84 at 11 px: the warp fix recovered this strip); [1000,100,900,500] 22.98 -> 21.45; 3x at [1000,150,300,200]: crisp pebbles across the shadow edge, no brightness step along any hex border. What's wrong: the same moss-weight patch as the default view, visible at 1x on `04-forest-floor-sun20-before-after-compare.jpg` as a pale blotch left of the lit strip; 4x at [1380,300,260,130]: a pale speckled area about 150 x 80 px with no slab relief where the before has one lit slab with a shadowed underside. Judged under difference 1; failed here because it is visible at 1x in this frame too.

## Context views

### ctx-overview-south, ctx-overview-east — PASS
Fields at 300 m: even mottle, no rows, no lattice, no honeycomb; moss sheets 5-40 m of varying outline; sharpness 3.27 -> 3.28 and 3.44 -> 3.31 on [0,300,1920,780], band 0.99-1.00 at every octave. 2x at [600,500,700,350] on the south view: field, shore and town identical to the before apart from the mottle. The world reads the same from above as from A1-A3; nothing clipped, floating or defaulted.

### ctx-ground-1 — FAIL
- 4x at [500,900,300,150]: about 20 discrete pebbles and slabs with an edge (>= 15 met); sharpness 9.73 -> 9.25, band 0.98-1.03 at every octave; [300,760,600,220] 8.67 -> 8.24.
- What's wrong: the green-grey smear at [503,1006,63,24] is unchanged from r8 (6x at [450,950,250,110]: a 60 x 30 px blur with no texel structure where the before has pebbles). Mean RGB (138.4, 131.4, 66.2) against (145.1, 130.9, 63.0) for [400,950,300,130]: R differs by 6.7 (condition 4/255: not met); G - R is -7.0 inside against -14.2 around, the moss-rock green shift. Same cause as 04 (difference 1); invisible at 0.5x, visible at 1x as a greener smudge; failed on the stated condition and because fix 1 clears it.
- Fix type: CODE. Difference 1.

### ctx-ground-2 — PASS
- Moss in the lower band 0.053 -> 0.063 (<= 0.10 met). Lichen carpet [1100,780,400,200] 7.69 -> 7.61, one patch, dashes at several angles. 2x at [0,700,900,380]: no seam, no streak, no cell edge; the moss sheet at the top has a soft irregular outline.
- Number not met: [200,900,300,150] 8.74 -> 7.67 (r8 7.69; bar 8.5). The loss is local, not a property of the surface: [200,900,150,150] 8.96 -> 7.97 and [350,900,150,150] 8.53 -> 7.37, but [600,900,300,150] 8.20 -> 8.44 and [900,850,300,150] 8.04 -> 8.09, with 0 % moss in all of them. The window's cells now hold a stick-poor region of the scan where the before held sticks (4x: the before's dark dashes are the scan's twigs, the after has fewer of them). A fixed-window number whose content moves with the lattice is reported, not used to fail a view that shows no defect at 4x.

### ctx-ground-3 — PASS
[100,850,400,200] 9.15 -> 9.00 (>= 8.5 met); [80,880,300,110] 8.46 -> 9.04; moss 0.011 -> 0.032 (<= 0.10 met). 5x at [80,880,300,110]: pebbles; three or four dark sticks at one angle lower-centre (a texel of the scan, the same as r8, not a row across cells). The before's faint field hatch at 20-60 m is gone.

### ctx-ground-4 — PASS
- Cobble [1000,900,400,180] 4.93 -> 4.77, 3x: continuous courses, no seam, no tone step. Far mud 3x at [0,600,600,130] 5.55 -> 5.10: the before's one-angle ribbing at 10-20 m is gone. [0,700,900,380] 8.95 -> 8.53, band 0.97-1.01.
- Mud [100,850,300,150] 11.15 -> 9.80 (r8 9.93), band 0.90 / 0.95 / 0.99 / 1.01 / 0.88; 4x: clumpy granular mud with crack rings, no left-right drag, no cell, no step. The r8 bar of 10 is not met; the r8 rule's own floor for keeping T's rotation was 9.5, and the three captures of T at `Rot` 0.5 read 9.93 / 9.80 / 9.85. The loss is rotation itself (`Rot` 1.0 8.61, 0.75 9.03, 0.5 9.80; see "Rule outcome"), and difference 2 keeps 0.5 at this distance, so this window will not change. Passed on the 4x crop.

## Sun-20 views

### A1-sun20 — PASS
Twin difference 38.3. 2x [700,500,500,300] 5.53 -> 5.27, 4x [640,650,220,220] 8.27 -> 7.95: no polygonal step, no seam, no cell edge on R, T or grass; band 0.98-1.01.

### A1-margin-sun20 — PASS
Before | after 6.8 (only the ground differs). 3x at [500,820,400,250]: courses continuous through every hex border, no mortar-line seam, no tone step (6.10 -> 5.74, band 0.95-1.06). Earth margin at 1-5 m granular, no cell, no step.

### ctx-ground-4-sun20 — PASS
Twin difference 44.7. Mud [0,700,900,380] 10.49 -> 10.31 (>= 10 met), band 0.99 / 1.02 / 1.01 / 0.97 / 1.01; 2x: no directional drag. Cobble 2x [900,700,600,300] 5.03 -> 4.90, no steps.

## Measurement credibility, r8 fix items and the rule outcome

1. Gate reproduces (`max_peak 0.1882`; before 0.665). As in r5-r8 it passes the A3 earth crop (0.131) that fails the human check: broken rows kill the one-lag autocorrelation peak while three rows stay legible at 10x.
2. Perf: `perf-01.json` median 7.33 -> 7.33 ms (+0.00, within +1), p95 9.67 -> 8.63; `ProfileGPU` BasePass 0.838 -> 1.241 ms (+0.40, within +0.6; r8 1.264). GPU frame 5.43 -> 6.40 includes a 0.504 ms `CaptureConvolveSkyEnvMap` pass that is absent from the before frame (a sky capture that happened to land in the profiled frame) and RenderDeferredLighting 0.83 -> 0.92; neither is the material. Met.
3. Sun views credible (twins 57.1 / 38.3 / 44.7 > 8).
4. r8 fix 1 (`scale` 0.001, `levels` 1, `HexWarp` 0.15, bounds 0-0.2, slope comment, `bisect-warp0/` control): implemented exactly as written (`import_ground.py` lines 108-112 match the r8 text; spec paragraph and blueprint row carry 0.15 / 0.001). Acceptance: met on the band and std points (04 std 26.2, band >= 0.8 at every octave on both 04 windows, 04-sun20 strip >= 17 with no border step, no straight crossfade line, cobble continuous at 01, ctx-ground-4 and A1-margin-sun20); not met on the patches (04 at both rectangles, 04-sun20 at [1380,300,260,130], ctx-ground-1 blob R +6.7) and on the two fixed-window numbers (ctx-ground-2 7.67, mud 9.80). The warp fix did its job on the bands and could never have done the rest: the control the r8 review asked for shows the patch at `HexWarp` 0, and item 6 below says what it is.
5. r8 fix 2 (rule run at `Rot` 1.0 on A and T with the gentle warp): run as specified. `bisect-r9rot1/` 04 [600,450,400,250] 12.89, std 19.9, band 0.77 / 0.80 / 0.76 / 0.61 / 0.80; `bisect-warp0/` (same `Rot`, no warp) 13.29 / 20.6 / 0.79 / 0.83 / 0.81 / 0.67 / 0.78; `bisect-base/` (`BlendSharp` 7, no warp) 13.26 / 20.6 / 0.79 / 0.83 / 0.81 / 0.66 / 0.78. Below 14: stop and report, done correctly. The three numbers agree to 0.4, so the r8 hypothesis (the bands hid what narrow bands would do at full rotation) is refuted by the control it asked for, and the builder's conclusion "not the bands" stands. What remains at `Rot` 1.0 is the scans' own content under rotation (the diffuse maps carry baked directional shading; a cell turned against the sun loses edge contrast: `Rot` 0 15.4-15.8, 0.15 15.0-16.1, 1.0 12.9-13.3 across r6-r9), which is a scan property, routed to Carried forward. The A value 0.15 therefore stands as a measured deviation, not an interim.
   T: the rule said "T stays 1.0 unless mud < 9.5 AND a directional drag is visible at 4x, in which case 0.75; the A3 strip is the deciding view, the mud window secondary". Measured: `Rot` 1.0 mud 8.61 (below 9.5; 4x at [100,850,300,150] in `bisect-r9rot1/ctx-ground-4.jpg` shows softening, not a left-right drag), A3 strip `R` 0.48; 0.75: mud 9.03, strip 0.61; 0.5: mud 9.80-9.93, strip 0.61-0.76. On the deciding view 1.0 wins and 0.75 ties 0.5; the builder shipped 0.5 on the secondary number alone, which the rule did not allow. The deviation is noted and not held against the round: no scalar satisfies both windows, and the rule's acceptance (`R` <= 0.40 at A3) is below the view's foreshortening floor of 0.62-0.65 and unreachable. Difference 2 replaces the scalar.
6. The patches: `import_ground.py` line 164 samples `forest_ground_04_Displacement` as `Mask` on a 9 m tile at full resolution and line 168-169 threshold it at `Low` 0.4 with `InvRange` 32. On that map 0.4 is the median; 15 % of texels fall inside the 1/32 band and the blurred-at-distance result is a 20-80 % crossfade over 10-14 % of the ground in 7-28 cm blocks (measured on the source map). `wm` drives `lerp(fa, fb)` and `lerp(na, nb)`, so the crossfade has a flat normal and a mixed albedo: the "translucent double image" of r5-r9. Evidence: the patch is invariant to every `MF_GN_HexTile` input changed since r5; the r6 WorldNormal dump is flat over it; the 04 patch is greener than its surround by 6 (G - R) and the ctx-ground-1 blob by 7, the mossy-rock shift. Blurring the Mask alone does not fix it (a smoother field still spends 17 % of the tile inside the 1/32 band); the band must be narrow in space: a mip-6 sample (28 cm) with `InvRange` 256 cuts the partial area to 2.0 % of the tile with `Low` 0.415 keeping today's effective coverage (mean `wm` 0.196 against 0.221 today; 0.42 gives 0.173, 0.41 gives 0.226).
7. r8 fix 3 (recapture, pairs, gate, perf, README table with `R`): done; README Round 9 has the window table, the `bisect-warp0` row and `R` for A1-A3 earth plus the A3 top strip.
8. r8 fix 4 (docs): spec "J2-pre as shipped (r9)" carries `HexWarp` 0.15 / `scale` 0.001 / one level, the slope bound, the control sentence ("whether the bands are wide (`BlendSharp` 7) or narrow (24, unwarped control 13.29)") and the per-layer `Rot` with the r9 numbers; blueprint 2a row updated; dependency-map row now says "A, B, R, T, S", per-layer `rot`, world-space normals and lists the helpers, but says "`REVIEW-r1..r8` not passed, `REVIEW.md` the latest" where r8 asked for r1..r9 (minor, fix 4).
9. r8 fix 5 (commits): r3-r8 are fccc5c62 and r9 is d4581f87; the r8 frames were recovered from fccc5c62 for this review. Done.

## Differences, most visible first

1. Translucent, relief-less patches at 1-5 m: 04 [1430,310,150,70] and [1720,175,130,50] at 1x on the compare sheet; 04-sun20 [1380,300,260,130] at 1x; ctx-ground-1 [503,1006,63,24] (R +6.7).
   What's wrong: the moss weight `wm` thresholds a full-resolution 9 m displacement sample at its median value with a 1/32 band, so 10-14 % of the ground is a texel-dithered partial blend of layers A and B (item 6). It is outside `MF_GN_HexTile`; the r2 Mask warp moved these zones into the review views.
   Fix type: CODE. Exact: `GameClient/ArtSource/Greenlands/NatureFamilies/import_ground.py`, replace lines 164-169
   ```python
       mk = g.mask(g.smp('Mask', tex(T + 'forest_ground_04_Displacement'), S.SAMPLERTYPE_MASKS, muv, -1100, 2600), True, False, False, -800, 2600)
       wm = node(m, u.MaterialExpressionSaturate, 0, 2600)
       # J2-pre r2 fix 2: the first warp noise also moves the threshold, so moss patches range from absent to twice the size over ~25 m (MaskNoise 0-0.03: coverage at ctx-ground-2 is 0.053 + 2.0 x MaskNoise; 0.15 flooded 04 and the plain with layer B)
       mkv = g.op(u.MaterialExpressionAdd, mk, g.mul(nzA, g.sc('MaskNoise', 0.02, -900, 2750), -850, 2700), -700, 2600)
       sub = g.op(u.MaterialExpressionSubtract, mkv, g.sc('Low', 0.4, -600, 2700), -500, 2600)
       link(g.mul(sub, g.sc('InvRange', 32, -600, 2800), -300, 2600), '', wm, '')
   ```
   with
   ```python
       mk = g.mask(g.smp('Mask', tex(T + 'forest_ground_04_Displacement'), S.SAMPLERTYPE_MASKS, muv, -1100, 2600), True, False, False, -800, 2600)  # full-res: trail stones (sw) only
       # r9 review fix 1: the moss weight thresholds a 28 cm-blurred copy of the Mask (mip 6 of 2048 over 9 m) with a narrow band, so the moss edge is a
       # clean contour instead of texel dither. At full res with Low 0.4 / InvRange 32 the map's median (0.380) sits on the threshold and 10-14 % of the
       # ground was a 20-80 % A/B crossfade with a flat normal (the r5-r9 "translucent patches"). Mip 6 + InvRange 256 + Low 0.415: partial area 2.0 % of
       # the tile, mean wm 0.196 (today 0.221). Low bounds 0.40-0.43 (0.42 = 0.173 coverage, 0.41 = 0.226); InvRange bounds 128-512.
       mkl_s = g.smp('MaskLow', tex(T + 'forest_ground_04_Displacement'), S.SAMPLERTYPE_MASKS, muv, -1100, 2800)
       mkl_s.set_editor_property('mip_value_mode', u.TextureMipValueMode.TMVM_MIP_LEVEL); mkl_s.set_editor_property('const_mip_value', 6)
       mkl = g.mask(mkl_s, True, False, False, -800, 2800)
       wm = node(m, u.MaterialExpressionSaturate, 0, 2600)
       mkv = g.op(u.MaterialExpressionAdd, mkl, g.mul(nzA, g.sc('MaskNoise', 0.02, -900, 2750), -850, 2700), -700, 2600)  # J2-pre r2 fix 2: the first warp noise moves the threshold (MaskNoise 0-0.03)
       sub = g.op(u.MaterialExpressionSubtract, mkv, g.sc('Low', 0.415, -600, 2700), -500, 2600)
       link(g.mul(sub, g.sc('InvRange', 256, -600, 2800), -300, 2600), '', wm, '')
   ```
   `sw` (line 217) keeps reading `mk` at full resolution (the trail stones are texel-scale by design). `const_mip_value` is the `TextureSample` property used when the mip pin is unlinked; if the editor build exposes only the pin, link `node(m, u.MaterialExpressionConstant, -1250, 2850, r=6.0)` to the sampler's `Level` input after setting `mip_value_mode` (the pin only exists in that mode). `hex_tile.py` unchanged.
   Acceptance: 04 6x at [1380,290,220,110] and [1600,150,260,90]: no relief-less area larger than 40 x 40 px (every 40 x 40 window contains a stone edge or a moss-patch edge); 04-sun20 4x at [1380,300,260,130]: slab relief present, no pale area > 40 x 40; ctx-ground-1 [503,1006,63,24] within 4/255 per channel of [400,950,300,130]; moss coverage (G - R > 8, y >= 500) ctx-ground-2 and ctx-ground-3 <= 0.10 and 04 full frame <= 0.05 (today 0.063 / 0.032 / 0.01); moss-patch outlines at A1 grass [150,420,200,170] 4x and A2 grass [1230,800,256,256] 3x still of unequal shape with no honeycomb; no 1 px hard edge regression beyond the one already carried forward (an edge of 2-6 px at 1 m is the expected result of the 28 cm field); 04 [600,450,400,250] std >= 26 and band >= 0.8 at every octave unchanged.

2. One-angle rib rows on the A3 earth margin, top strip [884,600,32,100]: three rows across the full 32 px width over ~30 px, `R` 0.76 (r8 0.61, `Rot` 1.0 0.48); A2 cleared at 100 m.
   What's wrong: layer T's `Rot` is one scalar for every distance. 1.0 turns the rows at 250 m and takes 23 % off the 1-5 m mud (8.61); 0.5 keeps the mud (9.80) and leaves the rows. The rule's two acceptance windows are at 3 m and 270 m.
   Fix type: CODE. Exact: `import_ground.py`, `hexlayer()` line 123
   ```python
           link(node(m, u.MaterialExpressionConstant, -1300, y + 300, r=rot), '', call, 'Rot')
   ```
   becomes
   ```python
           link(rot if isinstance(rot, u.MaterialExpression) else node(m, u.MaterialExpressionConstant, -1300, y + 300, r=rot), '', call, 'Rot')  # r9 review fix 2: a layer may pass an expression (depth-ramped Rot)
   ```
   and line 211
   ```python
       dc, dn, dr, _ = layer('T', T + 'brown_mud_02_Diffuse', T + 'brown_mud_02_nor_gl', T + 'brown_mud_02_Rough', 200, (0.85, 0.75, 0.65), 8000, hex=True, rot=0.5)  # r9: mud [100,850,300,150] / A3 top-strip rib R: Rot 1.0 8.61 / 0.48, 0.75 9.03 / 0.61, 0.5 9.93 / 0.61 (0.5 dominates 0.75)
   ```
   becomes
   ```python
       # r9 review fix 2: T's rotation ramps with depth. Inside 20 m the mud keeps Rot 0.5 (1-5 m relief: 9.8 at 0.5, 8.6 at 1.0); beyond 80 m it is fully
       # rotated (the A3 margin's one-angle rib rows at 250 m: strip R 0.76 at 0.5, 0.48 at 1.0). Rot = lerp(RotTNear, RotTFar, saturate((PixelDepth - 2000) / 6000)).
       # The angle varies by <= 3 deg across one 2 m cell on the ramp, below the texel size at 20-80 m. Bounds: RotTNear 0-1, RotTFar 0-1, ramp 2000-8000 cm.
       depthT = node(m, u.MaterialExpressionPixelDepth, -2000, 7800)
       rampT = node(m, u.MaterialExpressionSaturate, -1450, 7800)
       link(g.op(u.MaterialExpressionDivide, g.op(u.MaterialExpressionSubtract, depthT, node(m, u.MaterialExpressionConstant, -1900, 7850, r=2000.0), -1800, 7800),
                 node(m, u.MaterialExpressionConstant, -1750, 7850, r=6000.0), -1600, 7800), '', rampT, '')
       rotT = g.lerp(g.sc('RotTNear', 0.5, -1500, 7700), g.sc('RotTFar', 1.0, -1500, 7900), rampT, -1300, 7800)
       dc, dn, dr, _ = layer('T', T + 'brown_mud_02_Diffuse', T + 'brown_mud_02_nor_gl', T + 'brown_mud_02_Rough', 200, (0.85, 0.75, 0.65), 8000, hex=True, rot=rotT)
   ```
   Lines 137-138 comment: "Rot is per layer (A 0.15, T 0.5 -> 1.0 over 20-80 m, S/B 1.0, R 0.0)". `hex_tile.py` unchanged (`Rot` is already a per-pixel float input).
   Acceptance: A3 10x at [884,600,32,100] and [884,600,32,215]: no run of one-angle dashes across the full 32 px width longer than 15 px (1.5 cells) anywhere in the strip; `R` on [884,600,32,100] at 16 px blocks <= the same frame's cobble crop [925,600,55,215] + 0.05 (the view's foreshortening floor, 0.65 in r9; the absolute 0.40 is withdrawn); A2 8x at [805,580,80,200]: no rows; gate <= 0.25 on all nine; ctx-ground-4 mud [100,850,300,150] >= 9.5 (today 9.80, unchanged at 3 m) and ctx-ground-4-sun20 [0,700,900,380] >= 10 with no directional drag at 4x; ctx-ground-4 far mud [0,600,600,130] 3x: no one-angle ribbing at 10-20 m; A1-margin-sun20 earth at 1-5 m granular, no cell, no step; `A1-sun20` 4x at [640,650,220,220]: no step or swirl where the ramp crosses the margin (20-80 m); no visible rotation discontinuity in a 2x crop of the A1 margin between y 500 and 800.

3. Window numbers that move with the lattice (ctx-ground-2 7.67, 04 sharpness 14.99, A2 earth `R` 0.27) are reported above and not used as pass/fail on their own; the pass conditions in fix 3 restate them as visual checks with the number as a tie-break. No separate fix.

4. Documentation.
   What's wrong: the spec's "J2-pre as shipped (r9)" paragraph and the blueprint row describe the patches as a hex-blend matter by omission (the r8 band sentence was replaced correctly, but the moss-weight cause is nowhere); the dependency-map row says `REVIEW-r1..r8`.
   Fix type: CODE (docs). Exact: spec "J2-pre as shipped (r10)": `MaskLow` (mip 6, `InvRange` 256, `Low` 0.415, with the one-sentence cause: the full-res threshold at the map's median dithered 10-14 % of the ground into an A/B crossfade), T `Rot` ramp 0.5 -> 1.0 over 20-80 m with the two window numbers, and the sentence "At full rotation 04 reads 12.9-13.3 whether the bands are wide (`BlendSharp` 7), narrow (24) or unwarped (control 13.29): the loss is the scans' baked shading under rotation, not the blend" in place of the current control sentence; blueprint 2a "Today (J2-pre)" row: the r10 values; dependency-map row: "`REVIEW-r1..r9` not passed, `REVIEW.md` the latest".

## Fix list (job domain, deduplicated)

1. Engineer — `import_ground.py` lines 164-169: moss weight from a mip-6 `MaskLow` sample, `InvRange` 32 -> 256, `Low` 0.4 -> 0.415 (bounds in the comment); `sw` keeps the full-res `mk` (difference 1).
2. Engineer — `import_ground.py` line 123 (`hexlayer` accepts an expression for `Rot`) and line 211 (T `Rot` = `lerp(RotTNear 0.5, RotTFar 1.0, saturate((PixelDepth - 2000) / 6000))`), lines 137-138 comment (difference 2).
3. Codex — re-run `import_ground.py`; recapture `after/` (all views, four sun views) and `after-measure/`; gate on both measure folders; `perf_gpu.py` and the 500-frame timing; rebuild pairs; README Round 10 with the window table (before, r9, r10), moss coverage for ctx-ground-2 / ctx-ground-3 / 04, rib-angle `R` (A1-A3 earth crops at 32 / 16 px, the A3 top strip, and the same frame's A3 cobble crop as the floor); fresh review. Pass condition: nine crops <= 0.25; A3 10x [884,600,32,215]: no one-angle dash run across the strip longer than 15 px, strip `R` <= cobble-crop `R` + 0.05; A2 8x [805,580,80,200]: no rows; A2 [1230,800,256,256] and A3 [1075,520,190,190] at 3-4x with no two moss patches sharing angle and outline; after/04 [600,450,400,250]: >= 10 slabs with dark undersides at 3x, std >= 26, band >= 0.8 at every octave; [1300,300,300,200] band >= 0.8 at 11 px; no relief-less area > 40 x 40 px at 6x in [1380,290,220,110] or [1600,150,260,90] and none at 4x in 04-sun20 [1380,300,260,130]; no straight crossfade line > 60 px at 3x in any 04 or ctx-ground window; 04-sun20 [1400,200,520,380] >= 17, no brightness step along a hex border; ctx-ground-1 [500,900,300,150] >= 15 pebbles with an edge and [503,1006,63,24] within 4/255 of its surround; ctx-ground-2 and ctx-ground-3 lower-band moss <= 0.10, 04 frame moss <= 0.05; ctx-ground-3 [80,880,300,110] no one-angle dash row; ctx-ground-4 mud [100,850,300,150] >= 9.5 and ctx-ground-4-sun20 [0,700,900,380] >= 10, no directional drag at 4x, no ribbing at [0,600,600,130]; A1-sun20 and the A1 margin at 20-80 m: no rotation step or swirl; cobble courses continuous at 01 [300,800,500,280], ctx-ground-4 [1000,900,400,180], A1-margin-sun20 [500,820,400,250]; sun views differ from their twins by > 8; perf median at 01 within +1 ms of the before run and BasePass within +0.6 ms.
4. Codex — spec "J2-pre as shipped (r10)" paragraph, blueprint 2a row, dependency-map row `REVIEW-r1..r9` (difference 4).

## Carried forward (not this job)

- Relief loss under full rotation is the scans' content, not the blend: `Rot` 1.0 reads 12.9-13.3 at 04 with wide bands, narrow bands and no warp alike (r9 control), against 15.4-15.8 at `Rot` 0 and 16.95 plain. The diffuse maps carry baked directional shading that fights the sun in cells turned against it. Follow-up for J2: delight `forest_ground_04_Diffuse` and `brown_mud_02_Diffuse` (divide out an 8-16 px low-pass of the luminance, or use the `_arm` AO channel instead of the baked shading) before the landscape material ships `hex_rot` 1.0 on Mud / ForestFloor; and confirm on import that `T_GN_*_nor_gl` has the green channel flipped (Poly Haven `nor_gl` is +Y; UE expects -Y), since an unflipped map also fights the baked shading at `Rot` 0.
- The scan's own pale fine-gravel zones and its 1.3 m smooth slab (04 near field [0,900,400,180], 04 top strip): content of `forest_ground_04`, present in the before; a second forest-floor scan for cell variety is J2 work.
- Aerial colour blend 25-60 m and macro luminance variation per layer (J2).
- Low-poly lime-green blob trees and bushes at A1-A3 and the overviews; identical house rows; bare plain with no fields, hedges or tracks between town and river.
- White blob mountains without rock or snow detail (A1, 01, ctx-ground-1/2/4).
- Cobbled carriageway where `gameplay-hud.png` far range shows a dirt track; beige cobble vs the concept's grey-brown stone.
- Bookmark 04 vs `Greenlands-Forest-Look-v1.webp`: no leaf litter, needles or undergrowth variety; the moss-patch edge hardness at 1 m becomes a 2-6 px contour with fix 1 (was 1 px).
- Hard-edged river bank and flat water colour (A2, A3).
- `mossy_rock` crack dashes as the moss-patch texture: a texture choice for layer B.
- The A1 earth margin's one-direction grain (`R` 0.93 at 30 m, reads as mottle): revisit with the landscape material's PackedEarth scan in J2.
