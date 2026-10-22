# GFX-02 J2-pre hex-tiled ground: independent review, round 10 (Fable, 2026-10-22)

Judged against `docs/decisions/GFX-02-photoreal-ground-pipeline.md` section 2a ("J2-pre as shipped (r10)", Acceptance: gate `peak <= 0.25` on nine crops plus the human check "two or more copies of one motif inside a crop is a FAIL"), section 7 row J2-pre, `docs/blueprints/landscape-and-ground.md` section 2a, and the REVIEW-r9.md fix list with its stated acceptance. Domain: repeating motifs on cobble, earth or grass at A1-A3, plus anything the hex-tiling introduced (cells, seams, brightness steps, double exposure, loss of relief, regressions). Round 10 is commit 0c11f176; round 9 is d4581f87 and its `after/` and `after-measure/` frames were extracted with `git show` (byte sizes match the diff stat) and read beside the r10 frames, so every r9 number below is reproduced on frames, not quoted. Every frame in `before/`, `after/`, `before-measure/`, `after-measure/` and the six compare sheets was read: gate rectangles cut before | r9 | r10 at 3-14x from the measure frames, ground-level windows at 2-6x, sun views as full frames and 2-4x crops, context views at 0.5x and 2-5x, the two concepts beside their views. Compare sheets were checked against the committed `after/` frames (panel vs LANCZOS-fitted frame: mean abs diff 1.9-3.6, JPEG and resampling only). `Measure-GroundRepeat.py` re-run into scratch: `after-measure/` `GROUND_REPEAT PASS crops 9 max_peak 0.1972`, identical to the committed JSON; `before-measure/` `FAIL max_peak 0.665`. Metrics as in r4-r9: `sharp.py` (mean |dx|+|dy| of luminance, window std), band ratio = std of a Gaussian difference-of-scales band (sigma s minus 2s) r10/before at 0.7, 1.4, 2.8, 5.6, 11 px, rib-angle `R` with `rib_angle.py`, moss = fraction with G - R > 8 below y 500, plus a 40 x 40 px sliding-window sharpness map (stride 10) over the 04 ground to find relief-less areas anywhere in the frame, not only in the r9 rectangles. Every README round-10 number reproduces on the committed files (04 16.16 / 28.2, 04-sun20 19.98, ctx-ground-1 9.34, ctx-ground-2 7.85, ctx-ground-3 9.12, ctx-ground-4 mud 9.79, ctx-ground-4-sun20 10.31; moss 0.090 / 0.039 / 0.000; ctx-ground-1 blob (142.8, 128.6, 60.2) vs (145.4, 130.4, 62.3); A2 earth `R` 0.18, A3 strip 0.41, A3 cobble 0.61; sun twins 56.8 / 38.2 / 44.7).

Tally: PASS 15 (A1, A2, A3, 01, 04-forest-floor, 04-forest-floor-sun20, ctx-overview-south, ctx-overview-east, ctx-ground-1, ctx-ground-2, ctx-ground-3, ctx-ground-4, ctx-ground-4-sun20, A1-sun20, A1-margin-sun20). FAIL 0.

Verdict: PASSED. Both r9 fixes did what they were specified to do and nothing regressed. Fix 1 (moss weight from a mip-6 `MaskLow`, `InvRange` 256, `Low` 0.415): the translucent patches at 04 [1430,310,150,70] and [1720,175,130,50], the 04-sun20 blotch and the ctx-ground-1 blob are gone on the frames; the 1.3 m slab at 04 [1250,250,450,220] is back with its outline and dark underside; moss coverage stays inside the bounds (ctx-ground-2 0.090, ctx-ground-3 0.039, 04 0.000) and the patch outlines at A1-A3 are still unequal with no honeycomb. Fix 2 (T `Rot` = `lerp(0.5, 1.0, saturate((PixelDepth - 2000) / 6000))`): the three rib rows on the A3 top strip are gone at 10x and 14x, strip `R` 0.41 against the same frame's cobble floor 0.61 (condition `<= 0.66`), the 1-5 m mud keeps 9.79 (`>= 9.5`), the sun-20 mud 10.31 (`>= 10`), and there is no rotation step or swirl where the ramp crosses the margin at 20-80 m in A1, A1-sun20, A1-margin-sun20 or ctx-ground-4. One residual is reported under 04 (a 60 x 70 px partial-moss zone at the frame's right edge, half the r9 size and below the compare-sheet threshold); it does not fail the view and has an optional tune in the fix list. The owner's complaint (the tile grid of cobble, packed earth and grass at 20-200 m from the south approach) is answered on all three A views and the overviews.

## A1-south-approach-30m — PASS

- Gate (after-measure): cobble [876,850,256,222] 0.114 at (11,-30), earth [705,730,110,96] 0.074 at (-20,-10), grass [175,440,128,128] 0.197 at (17,-1); before 0.225 / 0.544 / 0.244. Cobble 3x: sand, no motif, no course change. Earth 6x at [700,725,120,100] (before | r9 | r10): the before's diamond lattice is gone; mottle with a one-direction diagonal grain (`R` 0.93, before 0.95, r9 0.93; carried forward), no two identical runs. Grass 4x at [150,420,200,170]: moss patches of unequal outline, none sharing shape; the before's identical cross-shaped patches are gone. Moss coverage 0.017 unchanged.
- Against `gameplay-hud.png` far range on `A1-hud-before-after-compare.jpg`: same layout, road, tree line, wall, river and town as r9; one continuous meadow; nothing clipped, floating or defaulted.
- Ramp check (fix 2): 2x at [600,500,400,500] on the after frame: the earth margin from 45 m to the horizon is granular throughout; no step, no swirl, no tone change along the margin where `Rot` goes 0.5 -> 1.0.

## A2-south-approach-100m — PASS

- Gate: cobble [885,790,110,256] 0.114, earth [815,600,64,256] 0.100 at (2,-24), grass [1230,800,256,256] 0.066.
- Earth margin 8x at [805,580,80,200] on the measure frame and on the shadowed after frame (before | r9 | r10): the before's diagonal dash rows are gone; granular, no run of one-angle dashes longer than one cell. `R` 0.18 on the measure frame (r9 0.27); on the after frame 0.55 (r9 0.06) with 16 blocks of 32 px, the same +-0.3 capture swing the r9 review recorded; the 8x crop shows no rows, which is the condition.
- Grass 3x at [1230,800,256,256]: moss patches of different outline; each carries the mossy_rock crack dashes at one angle (B `Rot` 1.0, texture choice, carried forward). Cobble 3x: sand. 2x at [700,500,500,580]: margins granular, same road, trees, river and wall as the before.

## A3-south-approach-250m — PASS

- Gate: cobble [925,600,55,215] 0.071, earth [884,600,32,215] 0.113 at (0,-27), grass [1075,520,190,190] 0.083.
- Earth margin 8x at [880,600,45,215], 10x at [884,600,32,100] and 14x at [884,600,32,50] (before | r9 | r10): the before carries a diamond lattice of dark dashes down the whole strip; r9 had three rows across the top 25-30 px; r10 is granular from top to bottom, no run of one-angle dashes across the full 32 px width longer than 15 px anywhere in the strip. `R` on [884,600,32,100] 0.41 (before 0.90, r9 0.76), on the full strip [884,600,32,215] 0.53 (r9 0.70), on the lower strip [884,700,32,115] 0.57 (r9 0.53, inside the capture swing), cobble crop 0.61, grass crop 0.62: the earth strip now reads at or below the view's own foreshortening floor. Condition `R` <= cobble + 0.05 met. Human check: no repeated motif inside the crop.
- Grass 4x at [1075,520,190,190]: patches of unequal outline; cleared. Cobble 4x: sand. Full frame: same town, bridge, river and fields as the before, mottle even, no lattice in the fields.

## 01-forest-path-town — PASS

Cobble 2x at [300,800,500,280]: continuous courses through every hex border, no doubled stone, no seam, no direction change; sharpness 5.26 -> 5.08, band 0.98-1.00 at every octave (identical to r9: the cobble layer did not change). Full frame: same town, wall, road and trees.

## 04-forest-floor — PASS

- Met: [600,450,400,250] sharpness 16.16 (before 16.95, r9 14.99; bar 15), std 28.2 (bar 26), band 1.00 / 1.04 / 1.07 / 1.03 / 0.96 (bar 0.8); 3x: 12+ flat slabs with crisp edges and dark undersides; [1300,300,300,200] 17.21 / 31.1, band 0.98 at 11 px (bar 0.8); gravel [1200,700,300,150] 15.78; 04 frame moss 0.000 (bar 0.05).
- r9 fix 1 acceptance, measured: 6x at [1380,290,220,110]: the pale speckled area is gone; every 40 x 40 window contains a stone edge (min 40 px sharpness 9.04, before 8.72, r9 8.55); mean RGB at [1430,310,150,70] (141.8, 127.9, 60.6) against (142.0, 124.8, 59.9) for [1250,250,450,220], i.e. -0.3 / +3.1 / +0.7 (r9 +11 / +17 / +12). 6x at [1600,150,260,90]: the green-grey smear is gone, pebbles and a pale slab with a hard edge in its place (min 40 px 9.58, r9 8.08); [1720,175,130,50] G -1.4 against its surround (r9 +0.3 over a greener surround). 3x at [1250,250,450,220]: the 1.3 m slab is back with its outline and dark underside; the r9 olive mush is gone. No straight crossfade line at 3x in [600,450,400,250], [1300,300,300,200], [100,500,500,300], [0,900,400,180].
- Residual, reported, not failed: the sliding 40 px map over the whole 04 ground (y 80-700, props excluded) finds two relief-less clusters in r10: [400,620,120,70] (min 5.79, G - R -18: the scan's smooth slab top, the same slab that sat at [1290,540,80,50] in the before; content) and [1850,220,60,70] at the frame's right edge (min 6.41, G - R -3.6 against -12 around; before 15.1 and -12.3 at the same pixels). The second is the moss ramp's remaining partial band: 4x at [1700,150,220,250] shows a smooth yellow-green zone about 60 x 70 px with soft edges and pebbles faintly through it, where r9 had 100 x 70 px at G - R -2.4 and the before has pebbles. In the corner [1760,200,160,120] the fraction of pixels greener than G - R -6 is 0.30 (r9 0.61, before 0.02). On the 0.5x compare sheet it is not legible; at 1x it is a slightly smoother patch at the edge beside the leaf. It is inside the r9 bound `InvRange` 128-512 to narrow further (optional fix 2 below).
- Near field [0,900,400,180] 10.65 -> 9.56: 3x shows the scan's large smooth slab at 1 m; content, carried forward.
- Against `Greenlands-Forest-Look-v1.webp`: stony floor with moss and leaf litter reads as the same kind of ground; the concept's needles, twigs and undergrowth variety are J2 work (carried forward).

## 04-forest-floor-sun20 — PASS

Sun view real (twin difference 56.8). Lit strip [1400,200,520,380] 19.98 (before 18.40, r9 18.36; bar 17), std 43.5, band 1.09 / 1.09 / 1.03 / 0.99 / 0.98; [1000,100,900,500] 22.78 (before 22.98). 3x at [1000,150,300,200]: crisp pebbles across the shadow edge, no brightness step along any hex border. 4x at [1380,300,260,130]: the r9 pale blotch is gone; a lit slab with a shadowed underside and pebbles around it, no pale area > 40 x 40 px (min 40 px 11.42 against the before's 12.01 at the same window). At 1x on the compare sheet the lit strip matches the before's slab layout with the hex-moved content.

## Context views

### ctx-overview-south, ctx-overview-east — PASS
Fields at 300 m: even mottle, no rows, no lattice, no honeycomb; moss sheets 5-40 m of varying outline; sharpness 3.27 -> 3.29 and 3.44 -> 3.33 on [0,300,1920,780], band 0.99-1.00 at every octave. 2x at [600,500,700,350] on the south view: field, shore and town identical to the before apart from the mottle. The world reads the same from above as from A1-A3; nothing clipped, floating or defaulted.

### ctx-ground-1 — PASS
4x at [500,900,300,150]: about 20 discrete pebbles and slabs with an edge (bar 15); sharpness 9.73 -> 9.34 (r9 9.25), band 0.95-1.05; [300,760,600,220] 8.38. 6x at [450,950,250,110]: the r9 green-grey blur at [503,1006,63,24] is gone; pebbles where the before has pebbles. Mean RGB (142.8, 128.6, 60.2) against (145.4, 130.4, 62.3) for [400,950,300,130]: -2.6 / -1.7 / -2.0, within 4/255 per channel (r9 R -6.7). Fields beyond: even, moss sheets of varying outline, no lattice.

### ctx-ground-2 — PASS
- Moss in the lower band 0.090 (before 0.053, r9 0.063; bar 0.10). 2x at [0,700,900,380]: no seam, no streak, no cell edge; the moss sheets have irregular outlines with a 2-6 px contour at 5-10 m (the expected result of the 28 cm field) and soft outlines beyond 20 m. 3x at [0,500,640,300]: the 10-40 m patches are larger than in r9 and of unequal shape; no straight edge > 60 px.
- Numbers reported, not used: [200,900,300,150] 7.85 (r9 7.67; bar 8.5), the lattice-dependent window the r9 review already set aside ([600,900,300,150] reads 8.52 against 8.20 before); lichen carpet [1100,780,400,200] 8.38 with band 1.61 at 11 px because the moss patch now fills more of the window (one patch, dashes at several angles).

### ctx-ground-3 — PASS
[100,850,400,200] 9.12 (bar 8.5); [80,880,300,110] 9.13; moss 0.039 (bar 0.10). 5x at [80,880,300,110]: pebbles; the same three or four dark sticks at one angle lower-centre as r9 (a texel of the scan, not a row across cells). 2x at [0,600,960,480]: the before's faint field hatch at 20-60 m is gone; moss sheets of unequal outline.

### ctx-ground-4 — PASS
- Cobble [1000,900,400,180] 4.77, 3x: continuous courses, no seam, no tone step. Far mud 3x at [0,600,600,130] 5.11: no one-angle ribbing at 10-20 m. [0,700,900,380] 8.53, band 0.90-0.98.
- Mud [100,850,300,150] 9.79 (r9 9.80; bar 9.5), band 0.92 / 0.95 / 0.99 / 1.02 / 0.82; 4x: clumpy granular mud with crack rings, no left-right drag, no cell, no step. 2x at [0,500,960,580]: the ramp from 20 m to the wall shows no rotation step along the margin.

## Sun-20 views

### A1-sun20 — PASS
Twin difference 38.2. 2x [700,500,500,300] 5.27, 4x [640,650,220,220] 7.94: no polygonal step, no seam, no cell edge on R, T or grass; band 0.98-1.01; no swirl or discontinuity where the ramp crosses the margin.

### A1-margin-sun20 — PASS
1x at [300,400,1000,680] (before | r9 | r10): earth margin from 1 m to the horizon granular, no cell, no step, no tone band at any distance. 3x at [500,820,400,250]: courses continuous through every hex border, no mortar-line seam, no tone step (5.74, band 0.96-1.09).

### ctx-ground-4-sun20 — PASS
Twin difference 44.7. Mud [0,700,900,380] 10.31 (bar 10), band 1.00 / 1.02 / 1.01 / 0.97 / 0.93; 2x: no directional drag. 4x at [100,850,300,150]: granular. Cobble 2x [900,700,600,300] 4.90, no steps.

## Measurement credibility and r9 fix items

1. Gate reproduces (`max_peak 0.1972`, A1 grass; before 0.665). All nine <= 0.25.
2. Perf: `perf-01.json` median 7.33 -> 7.26 ms (within +1), p95 9.67 -> 8.87; `ProfileGPU` BasePass 0.838 -> 1.259 ms (+0.42, within +0.6; r9 1.241). GPU frame 5.43 -> 6.23 includes `LumenSceneUpdate: N card captures` 0.038 -> 0.375 (a card capture landing in the profiled frame) and RenderDeferredLighting 0.83 -> 0.86; neither is the material. Met.
3. Sun views credible (twins 56.8 / 38.2 / 44.7 > 8).
4. r9 fix 1: `import_ground.py` lines 164-174 match the r9 text exactly (`MaskLow` sampler with `mip_value_mode` TMVM_MIP_LEVEL, `const_mip_value` 6, `Low` 0.415, `InvRange` 256, bounds in the comment; `sw` keeps the full-res `mk`). Acceptance: every stated point met (04 both rectangles, 04-sun20 rectangle, ctx-ground-1 blob within 4/255, moss coverage 0.090 / 0.039 / 0.000, A1 and A2 patch outlines unequal, 04 std 28.2 and band >= 0.96). The one zone outside the stated rectangles is the residual under 04.
5. r9 fix 2: `hexlayer()` line 123 accepts an expression; lines 216-223 build `depthT`, `rampT`, `rotT` and pass `rot=rotT` for T; the lines 137-138 comment carries "T 0.5 -> 1.0 over 20-80 m". Acceptance: A3 strip no run > 15 px, `R` 0.41 <= 0.66; A2 no rows; gate <= 0.25; mud 9.79 >= 9.5; sun mud 10.31 >= 10, no drag; far mud no ribbing; A1-margin-sun20 granular; A1-sun20 no step; A1 margin no discontinuity. All met.
6. r9 fix 3: recapture, measure frames, gate, perf, pairs, README Round 10 with the window table (before, r9, r10), moss coverage and `R` done. Minor omission: the README `R` list carries A2 earth, the A3 top strip and the A3 cobble floor but not A1 earth (0.93, unchanged) or the full A3 strip [884,600,32,215] (0.53); the r9 item asked for A1-A3 earth crops. Documentation only (fix 1).
7. r9 fix 4: spec "J2-pre as shipped (r10)" carries `MaskLow` (mip 6, `Low` 0.415, `InvRange` 256) with the one-sentence cause, the T ramp with both window numbers (9.8 / 8.6, 0.76 / 0.41) and the "baked shading turned against the sun, not the blend" sentence; blueprint 2a "Today (J2-pre)" row carries the ramp and `MaskLow`; dependency-map row says `REVIEW-r1..r9`. Done.

## Differences

1. Documentation: README Round 10 `R` list lacks A1 earth [705,730,110,96] 32 px and the full A3 strip [884,600,32,215] 16 px that the r9 fix 3 asked for.
   Fix type: CODE (docs). Exact: `docs/evidence/graphics/gfx02-j2pre-hextile/README.md`, Round 10 line "Rib-angle `R` (r10): A2 earth [815,600,64,256] 32 px 0.18; A3 strip [884,600,32,100] 16 px 0.41 vs the same frame's cobble crop [925,600,55,215] 0.61." becomes "Rib-angle `R` (r10, measure frames): A1 earth [705,730,110,96] 32 px 0.93 (unchanged); A2 earth [815,600,64,256] 32 px 0.18; A3 earth [884,600,32,215] 16 px 0.53 (r9 0.70); A3 top strip [884,600,32,100] 16 px 0.41 (r9 0.76) vs the same frame's cobble crop [925,600,55,215] 0.61 and grass crop [1075,520,190,190] 0.62."

2. Optional (not blocking): the moss ramp's remaining partial band at 04 [1850,220,60,70] (min 40 px sharpness 6.41, G - R -3.6 against -12 around; 0.30 of the corner [1760,200,160,120] greener than -6 against 0.02 in the before). Within the r9 bounds, `InvRange` 512 halves the band again.
   Fix type: CODE. Exact: `import_ground.py` line 174 `g.sc('InvRange', 256, -600, 2800)` -> `g.sc('InvRange', 512, -600, 2800)`; re-run, recapture `after/04-forest-floor.jpg`, `04-forest-floor-sun20.jpg`, `ctx-ground-1..3.jpg` only. Acceptance: 04 sliding 40 px map (stride 10, y 80-700, props excluded) finds no cluster with G - R > -8 and min sharpness < 8 larger than 40 x 40 px; moss coverage ctx-ground-2 and ctx-ground-3 within +-0.02 of 0.090 / 0.039; 04 [600,450,400,250] std >= 26 and band >= 0.8; patch outlines at A1 grass 4x and A2 grass 3x still unequal, contour <= 6 px at 1 m. If the corner zone is unchanged at 512, it is a plateau of the mip-6 field and the next step is `Low` 0.42 (coverage 0.173), not a higher `InvRange`.

## Fix list (job domain, deduplicated)

1. Codex — README Round 10 `R` line as in difference 1 (docs only; no capture).
2. Engineer (optional, not required for acceptance) — `import_ground.py` line 174 `InvRange` 256 -> 512, recapture the five ground-level views, measure per difference 2.

## Carried forward (not this job)

- Relief loss under full rotation is the scans' content, not the blend (`Rot` 1.0 reads 12.9-13.3 at 04 with wide bands, narrow bands and no warp alike; r9 control). Follow-up for J2: delight `forest_ground_04_Diffuse` and `brown_mud_02_Diffuse` (divide out an 8-16 px low-pass of the luminance, or use the `_arm` AO channel) before the landscape material ships `hex_rot` 1.0 on Mud / ForestFloor; confirm on import that `T_GN_*_nor_gl` has the green channel flipped.
- The scan's own pale fine-gravel zones and its 1.3 m smooth slab (04 near field [0,900,400,180], 04 [400,620,120,70] in r10): content of `forest_ground_04`; a second forest-floor scan for cell variety is J2 work.
- Aerial colour blend 25-60 m and macro luminance variation per layer (J2).
- Low-poly lime-green blob trees and bushes at A1-A3 and the overviews; identical house rows; bare plain with no fields, hedges or tracks between town and river.
- White blob mountains without rock or snow detail (A1, 01, ctx-ground-1/2/4).
- Cobbled carriageway where `gameplay-hud.png` far range shows a dirt track; beige cobble vs the concept's grey-brown stone.
- Bookmark 04 vs `Greenlands-Forest-Look-v1.webp`: no leaf litter variety, needles or undergrowth; the moss-patch contour at 1 m is 2-6 px (the 28 cm field).
- Hard-edged river bank and flat water colour (A2, A3).
- `mossy_rock` crack dashes as the moss-patch texture: a texture choice for layer B.
- The A1 earth margin's one-direction grain (`R` 0.93 at 30 m, reads as mottle): revisit with the landscape material's PackedEarth scan in J2.
