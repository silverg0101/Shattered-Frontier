# GFX-02 J2-pre hex-tiled ground: independent review, round 7 (Fable, 2026-10-22)

Judged against `docs/decisions/GFX-02-photoreal-ground-pipeline.md` section 2a (Interim J2-pre, "J2-pre as shipped (r7)", Acceptance: gate `peak <= 0.25` on nine crops plus the human check "two or more copies of one motif inside a crop is a FAIL"), section 7 row J2-pre, `docs/blueprints/landscape-and-ground.md` section 2a, and the REVIEW-r6.md fix-7 pass condition. Domain: repeating motifs on cobble, earth or grass at A1-A3, plus anything the hex-tiling introduced (cells, seams, brightness steps, double exposure, loss of relief, regressions). Every frame in `before/`, `after/`, `before-measure/`, `after-measure/`, the six compare sheets, the r7 `bisect-ws*`/`bisect-k1`/`bisect-sampleD` captures and the r6 dumps were read; gate rectangles and their neighbours cut before|after at 3-10x from the measure frames, ground-level windows at 3-6x, sun views as full frames and 2-4x crops. `Measure-GroundRepeat.py` re-run on `after-measure/` into scratch: `GROUND_REPEAT PASS crops 9 max_peak 0.1727`, identical to the committed JSON. The committed `MF_GN_HexTile.uasset` carries the r7 code (`rsqrt(dot(W, W))`, three `SampleLevel`, `Tw * nb.x`, `VN` found in the asset). Metrics as in r4-r6 (`sharp.py`; band ratio = std of a Gaussian difference-of-scales band after/before at 0.7, 1.4, 2.8, 5.6, 11 px; moss = fraction with G - R > 8). Every README round-7 window number reproduces on the committed files (04 15.44, 04-sun20 18.35, ctx-ground-1 9.41, ctx-ground-2 7.86, ctx-ground-3 9.16, ctx-ground-4 mud 10.76, ctx-ground-4-sun20 10.48). New metric for the earth margins: structure-tensor dominant orientation per 32 x 32 px block (16 px at A3) on a 3 px high-pass, axial mean resultant length `R` (1 = every block the same rib angle, 0 = angles spread evenly).

Tally: PASS 11 (A1, 01, 04-forest-floor-sun20, ctx-overview-south, ctx-overview-east, ctx-ground-1, ctx-ground-3, ctx-ground-4, ctx-ground-4-sun20, A1-sun20, A1-margin-sun20). FAIL 4 (A2, A3, 04-forest-floor, ctx-ground-2).

Verdict: NOT PASSED. Round 7 cleared the moss-patch motif (B at `Rot` 1.0: A2/A3 grass crops and ctx-ground-3 now pass the human check) and holds every ground-level number except ctx-ground-2, but the two defects that r6 named first and third are still in the frames: the earth margin at A2/A3 keeps its one-angle rib lattice (T at `Rot` 0.15, rib-angle `R` 0.78 / 0.61 against 0.91 / 0.76 before), and the hex overlap bands at 04 still read as translucent double images (two patches 145 x 63 px and 227 x 54 px, visible at 1x on the compare sheet; [1300,300,300,200] band ratio 0.71 at 11 px against the >= 0.8 condition). The r6 fix 4 (variance-preserving gain) shipped and measures as a 0-5 % change: it cannot un-double two different texels, only rescale their average, so the haze is unchanged (`bisect-k1` and `bisect-wsBflip` show the same patches). The r6 fix 2b (world-space normal frame) also shipped and measures neutral where it was meant to act (04 at `Rot` 1.0: 13.26 in the r6 tangent frame, 13.28 in the r7 world frame), which refutes the cause the spec paragraph now records. Reading the r7 table with that in mind, the `Rot` 1.0 sharpness loss is the overlap bands themselves: with rotation the three taps in a band are unrelated texels and their average is mush over 25-45 % of the surface; with offsets only, they are the same print shifted and the average keeps more edge. So one fix covers both open items: narrow the bands and bend their borders (fix 1), then restore the spec's `Rot` 1.0 on A, T, S (fix 2). The deviation to `Rot` 0.15 is a measured compromise, not a solution: it keeps 04 at 15.4 by giving up the rotation that the margin needs, and the margin fails.

## A1-south-approach-30m — PASS

- Gate (after-measure): cobble [876,850,256,222] 0.122 at (12,-30), earth [705,730,110,96] 0.106 at (-14,-7), grass [175,440,128,128] 0.173 at (18,-1); before 0.225 / 0.544 / 0.244. Cobble 3x: sand, no motif. Earth 6x at [700,725,120,100]: mottle with a diagonal grain, no lattice, no two identical runs; rib-angle `R` 0.94 (one grain direction across the crop, the same as the before's 0.90) but at 30 m the ribs are below the texel size and read as grain, not as a motif. Grass 4x and 3x at [150,570,250,140], [1300,520,250,140]: patches of unequal size and outline, no honeycomb, no two alike.
- Against `gameplay-hud.png` far range: same layout, road, tree line, wall and river as r6; one continuous meadow.

## A2-south-approach-100m — FAIL

- Gate: cobble [885,790,110,256] 0.127, earth [815,600,64,256] 0.110, grass [1230,800,256,256] 0.063; before 0.583 / 0.665 / 0.342.
- Grass 3x at [1230,800,256,256] and 2x at [0,250,500,350]: moss patches (B, `Rot` 1.0) of different outline, the crack dashes at a different angle in each; no two patches share angle and outline. The r6 difference 1 is cleared.
- Earth margin 6x at [805,600,80,150] and 8x at [805,580,80,200]: the `brown_mud_02` rib lattice is still there as rows of dark diagonal dashes, 12 rows in 200 px, upper-left to lower-right at one angle along the whole strip, broken only where a cell offset shifts a run. Rib-angle `R` 0.78 (before 0.91; 16 blocks, circular std 20 deg against 12.5 deg before): `Rot` 0.15 (+-27 deg) moves the angle by less than the dash length shows, so the motif is the same in every cell. Two or more copies of one motif inside the crop: FAIL on the human check. The r6 fix-7 condition "no one-angle rib run longer than one cell (30 px at A2)" is not met: runs span the strip.
- Cobble 3x: sand, no motif.
- Fix type: CODE. Differences 1, 2.

## A3-south-approach-250m — FAIL

- Gate: cobble [925,600,55,215] 0.072 at (5,-55), earth [884,600,32,215] 0.097 at (1,81), grass [1075,520,190,190] 0.085 at (-26,-26); before 0.628 / 0.500 / 0.433. The crop move (r6 fix 5) reads 0.097 and its neighbours ([876..932] x 8 px strips) carry the same mean luminance before and after: the rectangle is no longer the lowest-reading strip.
- Grass 4x at [1075,520,190,190], 2x at [1300,650,500,400] and [780,500,520,330]: moss patches vary in outline and dash angle; no two alike. Cleared.
- Earth margin 8x at [880,600,45,120] and 10x at [880,600,45,150]: diagonal rib rows at one angle across the full 32 px width, 10 rows in 150 px (12 px period = 1.8 m = the 2 m mud tile), the same lattice as the before with its nodes broken. Rib-angle `R` 0.61 (before 0.76; 26 blocks). At 3x on [860,560,140,260] both margins of the road show the diamond pattern at 1x scale. FAIL on the human check; the r6 condition "no one-angle rib run longer than one cell (13 px at A3)" is not met.
- Cobble 4x: sand, no motif; worn edge at 3x: no two identical bites.
- Fix type: CODE. Differences 1, 2.

## 01-forest-path-town — PASS

Cobble 2x at [300,800,500,280]: continuous courses, no doubled stone, no seam, no direction change; sharpness 5.26 -> 5.10, band ratio 0.98-1.01. Full frame: same town, road and trees.

## 04-forest-floor — FAIL

- Met (r6 fix-7 conditions): [600,450,400,250] sharpness 16.95 -> 15.44 (>= 15), std 27.4 -> 27.8, p5-p95 77-170 -> 79-169, band ratio 0.95 / 1.01 / 1.06 / 0.99 / 0.98 (>= 0.8 at every octave); 3x: 10+ flat slabs with crisp edges and dark undersides; no straight tone edge in [1300,300,300,200] or [100,800,400,250] at 3x. Gravel [1200,700,300,150] 14.79 -> 15.09. Against `Greenlands-Forest-Look-v1.webp`: hard-edged loose stones on an olive floor.
- What's wrong (most visible): translucent double-image patches where two hex taps blend. 6x at [1380,290,220,110]: a pale, speckled fog about 145 x 63 px at full-frame [1420,318,145,63], mean RGB (162,154,78) against (149,134,66) for [1250,250,450,220], no stone edge inside it, crisp slabs above and below; 6x at [1600,150,260,90]: a green-grey smear about 227 x 54 px at [1623,170,227,54] with no texel structure. Both are visible at 1x on `04-forest-floor-before-after-compare.jpg` as pale smudges in the upper right of the after. Window [1300,300,300,200]: sharpness 17.35 -> 15.55, std 31.4 -> 27.2, band ratio 0.89 / 0.93 / 0.96 / 0.83 / **0.71** (r6 condition: >= 0.8 at 11 px); [1250,250,450,220] band 0.89 / 0.90 / 0.87 / 0.78 / 0.64. The r6 fix 4 gain is in the shader and the patches are identical in `bisect-k1` (gain off) and `bisect-wsBflip` (gain on) at 3x: the gain rescales the average of two different texels, it does not pick one. The height tie-break does not decide these spots because the lichen crust and the soil have the same height.
- Near field [0,900,400,180] 10.65 -> 9.03 (band 0.85 at 0.7 px, 1.25 at 11 px): at 6x on [350,750,250,150] the after shows larger slabs with softer edges than the before's twigs and pebbles; not a defect by itself (a different texel), noted because it is the same band softness at the fine octave.
- Fix type: CODE. Difference 1.

## 04-forest-floor-sun20 — PASS

Sun view real (twin difference 65.3; before|after sun frames differ by 22.3). Lit strip [1400,200,520,380] 18.40 -> 18.35 (>= 17), [1000,100,900,500] 22.98 -> 21.57; 2x: slabs with raking light and shadowed undersides, relief on the albedo's slab shapes; 3x at [1000,150,300,200]: crisp pebbles across the shadow edge, no border step. The default view's pale patch is in this frame too (4x at [1380,300,260,130]: a pale speckled area with no slab relief where the before has one solid slab); judged under difference 1, not failed here, as in r6.

## Context views

### ctx-overview-south, ctx-overview-east — PASS
Fields at 300 m: even mottle, no rows, no lattice, no honeycomb; moss sheets 5-40 m; sharpness and band ratio within 3 % of the before (3.44 -> 3.40, 3.46 -> 3.36; band 1.00 at every octave). The world reads the same from above as from A1-A3.

### ctx-ground-1 — PASS
- 4x at [500,900,300,150]: about 20 discrete pebbles and slabs with an edge (>= 15 met); sharpness 9.73 -> 9.41, band 1.00-1.04; [300,760,600,220] 8.67 -> 8.32; mid field [900,700,500,250] 7.61 -> 7.52.
- 6x at [500,960,200,90]: one dark green-grey double-image blob about 63 x 24 px at [503,1006,63,24], mean RGB (139,132,66) against (145,130,63) for [400,950,300,130]; the before has (144,128,59) there. Same defect as 04, below 1x visibility; noted, not failed.

### ctx-ground-2 — FAIL
- Moss in the lower band [0,500,1920,580] 0.053 -> 0.062 (<= 0.10 met). Lichen carpet [1100,780,400,200] 7.69 -> 7.68, one patch, crack dashes at several angles, no ladder.
- [200,900,300,150] sharpness 8.74 -> **7.86**, condition >= 8.5 not met. The README's reason (a moss patch in the window) accounts for little: the moss rows are the bottom 30 px; [200,900,300,120] reads 8.47 -> 7.95. Band ratio 0.89 / 0.89 / 0.89 / 0.85 / 0.90, flat across octaves: a uniform contrast loss over the band fraction of the surface, 2-3 points lower than the r6 frame's 0.82-0.88 but inside noise. 4x: the same fine olive mottle as the before, softer; no streak, no smear, no seam.
- Fix type: CODE. Difference 1.

### ctx-ground-3 — PASS
[100,850,400,200] 9.15 -> 9.16 (>= 8.5 met); [600,900,300,150] 10.48 -> 9.37; moss 0.011 -> 0.031 (<= 0.10 met). 5x at [80,880,300,110]: pebbles, no row of parallel dashes (8.46 -> 9.20); 3x at [1500,650,400,100]: faint moss, no dash ladder. The r6 difference 1 is cleared here too.

### ctx-ground-4 — PASS
Mud [100,850,300,150] 11.15 -> 10.76 (>= 10 met), [500,750,300,150] 8.76 -> 8.43, [0,700,900,380] 8.95 -> 8.74, band 0.96-1.11; 4x: clumpy granular mud with crack rings, no left-right drag. Cobble [1000,900,400,180] 4.93 -> 4.76, continuous courses, no seam. Far mud 3x at [0,600,600,130] 5.55 -> 5.18: faint one-angle ribbing at 10-20 m as in the before (difference 2 covers it).

## Sun-20 views

### A1-sun20 — PASS
Twin difference 39.8. 2x [700,500,500,300] 5.53 -> 5.31, 4x [640,650,220,220] 8.27 -> 7.99: no polygonal step, no seam, no cell edge on R, T or grass.

### A1-margin-sun20 — PASS
Before|after 5.0 (the sun views share the material except the hex layers; the ground differs, the rest does not). 3x at [500,820,400,250]: courses continuous through every hex border, no mortar-line seam, no tone step (6.10 -> 5.87). Earth margin at 1-5 m granular, no cell, no step.

### ctx-ground-4-sun20 — PASS
Twin difference 44.6. Mud [0,700,900,380] 10.49 -> 10.48 (>= 10 met, measured on the committed JPEG), band 1.00 / 1.02 / 1.02 / 0.99 / 0.97; 2x: no directional drag. Cobble 2x [900,700,600,300] 5.03 -> 4.90, no steps.

## Measurement credibility, r6 fix items and the two deviations

1. Gate reproduces (`max_peak 0.1727`). It passes the A2/A3 earth crops (0.110, 0.097) that fail the human check, for the reason r6 gave: broken rows kill the one-lag autocorrelation peak while the motif stays legible. The rib-angle `R` above is the number that tracks the human check (0.9 before, 0.6-0.8 now, <= 0.4 expected with full rotation).
2. Perf: `perf-01.json` median 7.33 -> 7.89 ms (+0.56, within +1), p95 9.67 -> 10.04; `ProfileGPU` BasePass 0.838 -> 1.302 ms (+0.46, within +0.6); other passes within 0.04 except ComputeVolumetricFog +0.16 and Lumen card captures +0.13 (single-frame noise). Met.
3. Sun views credible (twins 65.3 / 39.8 / 44.6 > 8).
4. r6 fix 1 (`flatN` control): run (11.71 / 7.32) and not followed. The builder's objection is right: a flat normal removes the relief shading that the metric counts, so the control cannot reach 15 whatever the albedo does; the r6 decision rule was ill-posed. Deviation accepted. What it does not do is license fix 2b without a diagnosis.
5. r6 fix 2b (world-space frame, `tangent_space_normal` False, bedrock wrapped): implemented as written. Its own measurement refutes the stated cause: at `Rot` 1.0, 04 reads 13.26 in the r6 tangent frame (`bisect-base`) and 13.28 in the r7 world frame (`Bw` +y), and 17.14 / 13.28 for -y / +y is a frame-sign effect only at `Rot` 1.0 (the -y frame "scores high" because its mirrored normal lights the albedo's baked shadow side, which the metric counts as edge; the mud in the same frame drops to 7.7). The world frame is kept (it is at least not mirrored: `Rot` 0 +y 15.8 vs -y 11.2) but the spec's "rotated tangent-space normals fought the albedo" sentence is not supported by any number and goes (difference 4). The tangent-frame vs world-frame comparison at `Rot` 0 (r6 `rot0` 15.41 vs r7 `wsRot0` 15.80) is not like for like: the r6 variant ran with `HeightSharp` 0.
6. r6 fix 3 (B `rot=1.0`): implemented; A2/A3 grass and ctx-ground-3 pass. Met.
7. r6 fix 4 (variance-preserving gain): implemented as written. Measured effect: 04 unchanged (17.13 vs 17.14 in the k1/wsBflip pair, because the height tie-break already makes A's bands near-binary so `k` is 1 there), ground windows +4-6 % (ctx-ground-3 7.06 -> 7.55, mud 8.35 -> 8.88). The 04 haze patches are the same with the gain on or off. The acceptance ([1300,300,300,200] band >= 0.8 at 11 px, no washed patch > 40 x 40, ctx-ground-2 >= 8.5) is not met. The fix was the wrong tool for a structural double image; difference 1 replaces it with band narrowing (the gain stays, it costs nothing and helps the thin bands that remain).
8. r6 fix 5 (A3 earth crop) and fix 6 (README 9.87, spec HLSL block regenerated, "J2-pre as shipped" paragraph, blueprint row): done; the HLSL block matches `hex_tile.py` line for line. Fix 7 (recapture, pairs, perf, README): done; pass condition not met on A2/A3 earth, 04 bands, ctx-ground-2.
9. Deviation 2 (`Rot` 0.15 on A, T, S instead of 1.0): the measurement behind it (04: 1.0 -> 13.3, 0.25 -> 14.8, 0.15 -> 15.4, 0 -> 15.8) is real and reproduces, but the reading "the scans' baked shading turned against the sun" does not follow from it: a rotated albedo keeps its gradient energy, and the loss grows monotonically with how unrelated the three taps in a band are, which is what rotation does to a band. 0.15 is the point where bands blend near-identical prints again; it buys the 04 number by giving up the per-cell angle change the mud lattice needs (`R` 0.78 / 0.61 at A2 / A3). Not justified as the shipped value; justified only until fix 1 lands, after which the spec's 1.0 is re-measured (fix 2).
10. Docs: README round 7 and the spec paragraph state the shipped values accurately except the frame reason (difference 4). `docs/dependency-map.md` row still lists hex layers "A, R, T, S" (B is hex since r3) and "REVIEW-r1/REVIEW" (seven reviews now).

## Differences, most visible first

1. Hex overlap bands read as translucent double images at 1-5 m and lower contrast by ~10 % on every ground-level window (04 [1420,318,145,63] and [1623,170,227,54]; ctx-ground-1 [503,1006,63,24]; ctx-ground-2 7.86 < 8.5; [1300,300,300,200] band 0.71 at 11 px), and the same bands are what costs 04 its sharpness at `Rot` 1.0 (13.3 vs 15.8 at `Rot` 0).
   What's wrong: `pow(w, 7.0)` leaves a band about 10 % of the cell width (60 px at 04, 2 m cells) where two or three taps carry 20-50 % each; where the taps are different texels of equal height, the average is a fog that no gain restores. The paper's exponent is a tuning value, not a constant; its straight borders are hidden by the warp below.
   Fix type: CODE. Exact:
   (a) `GameClient/ArtSource/Greenlands/NatureFamilies/hex_tile.py`, `spec`: after `('VN', T.FUNCTION_INPUT_VECTOR3, None)` add
   ```python
   ('BlendSharp', T.FUNCTION_INPUT_SCALAR, 7.0), ('Warp', T.FUNCTION_INPUT_VECTOR2, 0.0)
   ```
   and in the input loop, after the `FUNCTION_INPUT_VECTOR3` branch, add
   ```python
   elif name == 'Warp':  # lattice warp in texture tiles; default none
       pv = u.Vector4f(); fi.set_editor_property('preview_value', pv); fi.set_editor_property('use_preview_value_as_default', True)
   ```
   (`UV` is also VECTOR2 and must keep no default; the `CustomInput` list is built from `spec`, nothing else changes.) In `HLSL` replace
   ```hlsl
   float2 p = UV / CellTiles;
   float2 gx = ddx(UV), gy = ddy(UV);
   float2 sk = float2(p.x - 0.57735027 * p.y, 1.15470054 * p.y);
   ```
   with
   ```hlsl
   float2 p = UV / CellTiles, pw = p + Warp / CellTiles;                       // lattice from the warped coordinate (borders bend by +-Warp tiles), texels from the unwarped p (r7 review)
   float2 gx = ddx(UV), gy = ddy(UV);
   float2 sk = float2(pw.x - 0.57735027 * pw.y, 1.15470054 * pw.y);
   ```
   (`cen`, `v`, `w` and `h` stay as they are: cell ids and weights come from `sk`; `uv = (mul(p - cen, R) + cen) * CellTiles + h` keeps sampling the unwarped `p`, so texels are not distorted; inside one cell the map is continuous.) Replace
   ```hlsl
   float3 W = lerp(1.0, L, 0.6) * pow(w, 7.0) * exp2(max(HeightSharp * (Hv - max(Hv.x, max(Hv.y, Hv.z))), -3.0)); W /= (W.x + W.y + W.z);   // height tie-break capped at 8x (r5 review)
   ```
   with
   ```hlsl
   float3 W = lerp(1.0, L, 0.6) * pow(w, BlendSharp) * exp2(max(HeightSharp * (Hv - max(Hv.x, max(Hv.y, Hv.z))), -3.0)); W /= (W.x + W.y + W.z);   // height tie-break capped at 8x (r5 review); BlendSharp 24: the 20-50 % band is ~3 % of the cell, not 10 % (r7 review)
   ```
   (b) `GameClient/ArtSource/Greenlands/NatureFamilies/import_ground.py`: directly after `HEX = hex_tile.build()` (line 105) add one shared lattice warp, two Simplex noises at 2.5 m features (the Mask's 25 m warp is too smooth to bend a 2 m cell border):
   ```python
   hwp = node(m, u.MaterialExpressionWorldPosition, -2000, 11000)

   def hex_noise(y, zoff):  # r7 review fix 1: hex lattice warp, 2.5 m features, +-HexWarp tile
       nz = node(m, u.MaterialExpressionNoise, -1700, y, noise_function=u.NoiseFunction.NOISEFUNCTION_SIMPLEX_TEX, scale=0.004, levels=2, output_min=-1.0, output_max=1.0, quality=2, turbulence=False)
       link(g.op(u.MaterialExpressionAdd, hwp, node(m, u.MaterialExpressionConstant3Vector, -1900, y, constant=u.LinearColor(0.0, 0.0, zoff, 0.0)), -1800, y), '', nz, 'World Position'); return nz
   hxy = node(m, u.MaterialExpressionAppendVector, -1500, 11000); link(hex_noise(11000, 0.0), '', hxy, 'A'); link(hex_noise(11150, 8192.0), '', hxy, 'B')
   hwarp = g.mul(hxy, g.sc('HexWarp', 0.25, -1500, 11100), -1350, 11000)  # tiles; bounds 0-0.4 (above 0.4 adjacent borders cross)
   ```
   In `hexlayer`, after the `VN` link (line 120) add
   ```python
   link(g.sc('BlendSharp' + p, bsharp, -1300, y + 500), '', call, 'BlendSharp')
   link(hwarp, '', call, 'Warp')
   ```
   add `bsharp` to `hexlayer`'s parameters and to `layer(..., hsharp=0.0, bsharp=24.0)` (bounds 7-32; 7 = the r1-r7 blend) and pass it through; every hex layer (A, B, T, S, R) takes the default 24. Cobble R: with `Snap` 14 its borders already fall on mortar lines; `HexWarp` moves the border, not the texels, so courses stay continuous (check 01 [300,800,500,280] at 2x, ctx-ground-4 [1000,900,400,180] at 3x, A1-margin-sun20 [500,820,400,250] at 3x). Re-time (two Simplex evaluations per pixel; BasePass must stay within +0.6 ms of the before).
   Acceptance: 04 6x at [1380,290,220,110] and [1600,150,260,90]: no translucent patch larger than 40 x 40 px (every 40 x 40 window contains at least one stone edge); [1300,300,300,200] band ratio >= 0.8 at 11 px; ctx-ground-2 [200,900,300,150] >= 8.5; ctx-ground-1 [503,1006,63,24] mean RGB within 4/255 per channel of [400,950,300,130]; no straight crossfade line longer than 60 px at 3x in any 04 or ctx-ground window (if one shows, `HexWarp` 0.25 -> 0.35 before touching `BlendSharp`); 04 [600,450,400,250] >= 15 and 04-sun20 strip >= 17 unchanged; cobble courses continuous at 01, ctx-ground-4, A1-margin-sun20.

2. Earth margin rib lattice at A2 [805,600,80,150] (`R` 0.78) and A3 [880,600,45,120] (`R` 0.61), and faint at ctx-ground-4 [0,600,600,130]: `Rot` 0.15 on T leaves the diamond ribbing at one angle in every cell; the spec's technique is per-cell offset **and rotation** (`Rot` 1.0 for A, T, S; cobble 0.15 or 0).
   What's wrong: the mud tile's lattice is invariant under offsets; only a rotation of tens of degrees per cell breaks it. 0.15 was chosen for 04's number, which fix 1 addresses at the source.
   Fix type: CODE. Exact: after fix 1 is built, `import_ground.py` line 131 `def layer(..., rot=0.15, ...)` -> `rot=1.0` (spec), comment lines 128-130 rewritten to the measured reason ("bands, not frame: 04 at Rot 1.0 13.3 with BlendSharp 7, <value> with 24"); R stays `rot=0.0, snap=14.0`, B stays 1.0. One capture set, then the rule: 04 [600,450,400,250] >= 15 at `Rot` 1.0 keeps 1.0 everywhere; 14-15 keeps T and S at 1.0 (their 1-5 m views are ctx-ground-4 and A1-margin-sun20, where the conditions are mud >= 10 and no step) and sets A to the largest of 1.0, 0.5 that reads >= 15; below 14 stop, record the row, and difference 1's `BlendSharp` goes to 32 for one more capture before anything else changes.
   Acceptance: A2 [815,600,64,256] and A3 [884,600,32,215] rib-angle `R` <= 0.4 (structure tensor, 32 / 16 px blocks, 3 px high-pass); at 6-8x no run of one-angle dashes longer than one cell (30 px at A2, 13 px at A3); gate still <= 0.25 on all nine; ctx-ground-4 mud [100,850,300,150] >= 10 and ctx-ground-4-sun20 [0,700,900,380] >= 10 with no directional drag at 4x.

3. ctx-ground-2 [200,900,300,150] 7.86 (< 8.5) and the flat 0.85-0.90 band ratio on every `Rot`-0.15 layer window: the band contrast loss of difference 1. No separate fix; its acceptance is listed under 1.

4. Documentation.
   What's wrong: `docs/decisions/GFX-02-photoreal-ground-pipeline.md` section 2a "J2-pre as shipped (r7)" states a cause ("the terrain mesh's UV tangents are not the world-x/y frame ... so rotated tangent-space normals fought the albedo") that the r7 table refutes (13.26 -> 13.28 at `Rot` 1.0), and a `Rot` 0.15 reason ("baked shading fights the sun") that no measurement separates from the band effect; `import_ground.py` lines 128-130 carry the same text; `docs/dependency-map.md` row lists hex layers "A, R, T, S" and "REVIEW-r1/REVIEW".
   Fix type: CODE (docs). Exact: in the "J2-pre as shipped" paragraph replace the sentence beginning "Reason: the terrain mesh's UV tangents" with "The world-space frame is kept because the +y / -y test at `Rot` 0 showed the frame is not mirrored (04 15.8 vs 11.2); it measured neutral at `Rot` 1.0 (13.26 tangent frame, 13.28 world frame), so the relief loss there was the overlap bands, narrowed in r8 (`BlendSharp` 24, `HexWarp` 0.25)."; replace "`Rot` 0.15 for A, T, S (full rotation lets the scans' baked shading fight the sun: ...)" with the r8 values and the 04 number at `Rot` 1.0 with `BlendSharp` 24; add `BlendSharp` and `Warp` to the input list and regenerate the HLSL block from `hex_tile.py`; blueprint 2a "Today (J2-pre)" row: `Rot` and the two new values; dependency-map row: "layers A, B, R, T, S" and "REVIEW-r1..r8"; README Round 8 with the r7-style table (before, r7, r8 per window) and the rib-angle `R` for A1-A3 earth.

## Fix list (job domain, deduplicated)

1. Engineer — `hex_tile.py`: `BlendSharp` and `Warp` inputs, lattice from the warped coordinate, `pow(w, BlendSharp)`; `import_ground.py`: shared 2.5 m Simplex lattice warp `hwarp` (`HexWarp` 0.25), `BlendSharp<p>` 24 on every hex layer (difference 1).
2. Engineer — `layer()` default `rot=1.0` after fix 1, with the 04 decision rule in difference 2; comment rewritten (difference 2).
3. Codex — re-run `import_ground.py`; recapture `after/` (all views, four sun views) and `after-measure/`; gate on both measure folders; `perf_gpu.py` and the 500-frame timing; rebuild pairs; README Round 8 with the window table and rib-angle `R` (A1-A3 earth, 32 / 16 px blocks); fresh review. Pass condition: nine crops <= 0.25; A2 [815,600,64,256] and A3 [884,600,32,215] rib-angle `R` <= 0.4 and at 6-8x no one-angle dash run longer than one cell; A2 [1230,800,256,256] and A3 [1075,520,190,190] at 3-4x with no two moss patches sharing angle and outline; after/04 [600,450,400,250] >= 15 with slabs and dark undersides at 3x, band ratio >= 0.8 at every octave; [1300,300,300,200] band >= 0.8 at 11 px; no translucent patch > 40 x 40 px at 6x in [1380,290,220,110] or [1600,150,260,90]; no straight crossfade line > 60 px at 3x in any 04 or ctx-ground window; 04-sun20 [1400,200,520,380] >= 17, no brightness step along a hex border; ctx-ground-1 [500,900,300,150] >= 15 pebbles with an edge and [503,1006,63,24] within 4/255 of its surround; ctx-ground-2 [200,900,300,150] >= 8.5 and ctx-ground-3 [100,850,400,200] >= 8.5 with no one-angle dash row in [80,880,300,110]; ctx-ground-4 mud [100,850,300,150] >= 10 and ctx-ground-4-sun20 [0,700,900,380] >= 10, no directional drag at 4x; cobble courses continuous at 01 [300,800,500,280], ctx-ground-4 [1000,900,400,180], A1-margin-sun20 [500,820,400,250]; moss coverage ctx-ground-2 and ctx-ground-3 lower band <= 0.10; sun views differ from their twins by > 8; perf median at 01 within +1 ms of the before run and BasePass within +0.6 ms.
4. Codex — GFX-02 section 2a "J2-pre as shipped" paragraph (frame sentence, `Rot`, inputs, HLSL block), blueprint 2a row, dependency-map row (difference 4).

## Carried forward (not this job)

- Aerial colour blend 25-60 m and macro luminance variation per layer (J2).
- Low-poly lime-green blob trees and bushes at A1-A3 and the overviews; identical house rows; bare plain with no fields, hedges or tracks between town and river.
- White blob mountains without rock or snow detail (A1, 01, ctx-ground-1/2/4).
- Cobbled carriageway where `gameplay-hud.png` far range shows a dirt track; beige cobble vs the concept's grey-brown stone.
- Bookmark 04 vs `Greenlands-Forest-Look-v1.webp`: no leaf litter, needles or undergrowth variety; hard 1 px moss-patch edges at 1 m from `Low 0.4 / InvRange 32` (present in the before).
- Hard-edged river bank and flat water colour (A2, A3).
- `mossy_rock` crack dashes as the moss-patch texture (they read as rock slab even when rotated): a texture choice for layer B.
- The A1 earth margin's one-direction grain (`R` 0.94 at 30 m, reads as mottle): revisit with the landscape material's PackedEarth scan in J2 if the same grain shows at 1-5 m there.
