# GFX-02 J2-pre hex-tiled ground: independent review, round 6 (Fable, 2026-10-22)

Judged against `docs/decisions/GFX-02-photoreal-ground-pipeline.md` section 2a (Interim J2-pre, Acceptance: gate `peak <= 0.25` on nine crops plus the human check "two or more copies of one motif inside a crop is a FAIL"), section 7 row J2-pre, `docs/blueprints/landscape-and-ground.md` section 2a, and the REVIEW-r5.md fix-6 pass condition. Domain: repeating motifs on cobble, earth or grass at A1-A3, plus anything the hex-tiling introduced (cells, seams, brightness steps, double exposure, loss of relief, regressions). Every frame in `before/`, `after/`, `before-measure/`, `after-measure/`, the six compare sheets, the five `bisect-*` captures and both GBuffer dumps were read; gate rectangles and neighbouring rectangles were cut before|after at 3-8x from the measure frames, ground-level windows at 3-6x, sun views as full frames and 2-4x crops. `Measure-GroundRepeat.py` was re-run on `after-measure/` into scratch: `GROUND_REPEAT PASS crops 9 max_peak 0.1602`, identical to the committed JSON. Metrics as in r4/r5 (`sharp.py`: mean |dx|+|dy| of luminance 0.2126/0.7152/0.0722; band ratio = std of a Gaussian difference-of-scales band after/before at 0.7, 1.4, 2.8, 5.6, 11 px; moss = fraction with G - R > 8). Bisect numbers in the README reproduce (base 13.26, rot0 15.41, plainN 13.99, cell4 10.47, nograd 12.93). One README number does not: ctx-ground-4-sun20 [0,700,900,380] after reads **9.87** with `sharp.py` on the committed `after/ctx-ground-4-sun20.jpg`, not 10.13.

Tally: PASS 9 (A1, 01, 04-forest-floor-sun20, ctx-overview-south, ctx-overview-east, ctx-ground-1, ctx-ground-4, A1-sun20, A1-margin-sun20). FAIL 6 (A2, A3, 04-forest-floor, ctx-ground-2, ctx-ground-3, ctx-ground-4-sun20).

Verdict: NOT PASSED. The gate passes and the forest-floor slabs are back (04 [600,450,400,250] 16.95 -> 15.76, slabs with a dark underside at 3x, no straight tone edge), but the way they were brought back, `Rot` 0.0 on every hex layer, is not the spec's technique (section 2a: "each hex samples the texture at its own random offset **and rotation**"; blueprint 2a `hex_rot` 1.0, cobble 0.15) and it regresses the two anisotropic scans: the moss patches (layer B, `mossy_rock`) now carry the same diagonal three-dash stripe at the same angle in every patch at A2 and A3 (the r5 review passed those crops with "no two alike"), and the earth margin (layer T, `brown_mud_02`) shows its diamond ribbing at one angle along the whole road at A2/A3 again. Two further in-domain defects: hazy double-exposure patches in the hex overlap bands at 04 and ctx-ground-1 (the blend's contrast loss, now visible because the taps are no longer rotated against each other), and the stated numeric conditions at ctx-ground-2 (7.57 < 8.5) and ctx-ground-4-sun20 (9.87 < 10) are not met. The rotation problem itself (relief loss at `Rot` 1.0) is still unlocated: the bisect's own decision rules did not fire (BaseColor spread -9 %, WorldNormal sharpness 1.5x the before, neither branch), and the fix applied, no rotation anywhere, is outside those rules and outside the spec.

## A1-south-approach-30m — PASS

- Gate (after-measure): cobble [876,850,256,222] 0.122 at (12,-30), earth [705,730,110,96] 0.123 at (-9,-24), grass [175,440,128,128] 0.094 at (21,-1); before 0.225 / 0.544 / 0.244. Cobble 3x: sand, no motif. Earth 6x at [700,725,120,100]: the before's wall-to-wall diagonal ribbing is broken into short runs of dashes at one angle; at 1x in the 110 x 96 crop it reads as mottle, no two identical runs countable. Grass 4x: blotches of unequal size, no honeycomb; 5x at [150,570,250,140] and [1300,520,250,140]: no two patches alike.
- Open field [0,560,400,300] 0.130 and [1250,520,400,300] 0.049 (gate `peak` at r_min 12). Against `gameplay-hud.png` far range: same layout, road, tree line and wall as r5; one continuous meadow.

## A2-south-approach-100m — FAIL

- Gate: cobble [885,790,110,256] 0.121, earth [815,600,64,256] 0.110, grass [1230,800,256,256] 0.082; before 0.583 / 0.665 / 0.342. The gate passes; the human check does not.
- Grass 3x at [1230,800,256,256]: every moss patch (layer B) carries the same motif, three parallel diagonal dark dashes running upper-left to lower-right, at the same angle; 8 copies in the crop (before: 14 identical dashed patches on a lattice; r5 after with B at `Rot` 1.0: "1-3 m lobed patches, no two alike"). Open field [0,250,500,350] at 2x: the same one-angle stripe inside every patch. That is two or more copies of one motif inside a crop. Regression against r5, caused by `layer()` default `rot=0.0` applied to B.
- Earth margin 6x at [805,600,80,150]: the `brown_mud_02` diamond ribbing is back as rows of dark diagonal dashes at one angle over the whole 64 px strip, broken only where a hex offset shifts a run by a fraction of a period (before: unbroken lattice; r5: "fine mottle, no lattice"). Gate 0.110 because the breaks kill the autocorrelation peak, not because the motif is gone.
- Fix type: CODE. Differences 1, 2.

## A3-south-approach-250m — FAIL

- Gate: cobble [925,600,55,215] 0.076 at (1,6), earth [884,600,32,160] 0.160 at (5,-12), grass [1075,520,190,190] 0.119 at (-26,-23); before 0.628 / 0.840 / 0.433.
- Grass 4x at [1075,520,190,190]: 12 copies of the same striped moss patch at the same angle (three dashes upper-left to lower-right) in the 190 x 190 crop; patch outlines differ, the motif does not. 2x at [1300,650,500,400] and [780,500,520,330]: the same. FAIL on the spec's human check. Regression against r5.
- Earth margin 8x at [880,600,45,120]: diagonal lattice at one angle across the full 32 px width, nine rows in 120 px; neighbouring gate rectangles on the same margin read 0.16-0.25 ([890,620,24,120] 0.210 at (5,-12); [888,700,26,100] 0.249 at (-6,-1)): the committed 32 x 160 rectangle is the one that reads lowest. 12 px vertical period at this range = 1.8 m = the 2 m mud tile's rib spacing.
- r5 fix 3 (EdgeNoise warp): the 23 px (3.5 m) edge period is gone: old crop [915,600,70,215] now peaks at (-1,-6) instead of (1,-23), the worn edge at 4x shows no two identical bites along 230 px. Its stated acceptance (old crop <= 0.15 before moving) reads 0.329: that number was ill-posed (a near-vertical edge inside a crop autocorrelates along its own direction at every vertical lag; [925,600,55,215] on the carriageway alone is 0.076). Intent met, number not.
- Fix type: CODE. Differences 1, 2.

## 01-forest-path-town — PASS

Cobble 2x at [300,800,500,280]: continuous courses, no doubled stone, no seam, no direction change; sharpness 5.26 -> 4.89, band ratio 0.95-0.97. Full frame: same town, road, trees.

## 04-forest-floor — FAIL

- Met (r5 fix-6 condition): [600,450,400,250] sharpness 16.95 -> 15.76 (>= 15), luminance std 27.0 -> 26.2, p5-p95 76-168 -> 81-167, band ratio 0.94 / 0.97 / 1.00 / 0.95 / 0.99 (>= 0.8 at every octave); 3x: 10+ flat slabs with crisp edges and dark undersides. Gravel [1200,700,300,150] 14.74 -> 15.22; near field [0,900,400,180] 10.59 -> 10.73. No straight tone edge in [1300,300,300,200] or [100,800,400,250] at 3-4x (the capped height term did its job). Against `Greenlands-Forest-Look-v1.webp`: the floor has hard-edged loose stones again.
- What's wrong (most visible): hazy double-exposure patches where two hex taps blend 50/50. 3x at [1250,250,450,220] after: a washed-out band about 220 x 60 px at full-frame [1330,300,220,60] with no stone edge in it, between crisp slabs above and below; 3x at [1400,80,450,200]: a second fog patch at about [1530,150,100,30]. Window [1300,300,300,200] sharpness 17.22 -> 14.70, std 31.1 -> 24.3, band ratio 0.86 / 0.88 / 0.89 / 0.78 / 0.63 (the coarse octaves lose most: a slab averaged with soil). Full frame: the pale smudges at upper right (x 1330-1700, y 100-360) are visible at 1x on the compare sheet. The before has none. With `Rot` 0 the two taps in a band are the same texels shifted by a fraction of a tile, so the average reads as a doubled, faded print rather than noise; the `lerp(1, L, 0.6) * pow(w, 7)` weights narrow the band but do not restore its contrast, and the height tie-break (now capped at 8x) only decides it where one texel is a stone.
- Fix type: CODE. Difference 3.

## 04-forest-floor-sun20 — PASS

Sun view real (twin difference 58.0; before|after sun frames differ by 21.0). Lit strip [1400,200,520,380] 18.33 -> 18.60 (>= 17), [1000,100,900,500] 22.89 -> 21.34; 2x: slabs with raking light and shadowed undersides, the relief sits on the albedo's slab shapes. 4x at [1300,300,300,200]: 25.25 -> 23.82, no brightness step along any hex border, no straight tone edge. Shaded floor [600,450,400,250] 7.70 -> 7.91. The overlap-band haze of the default view is in this frame too (lit strip upper left, [1000,150,150,100]) but reads as a lit slab face at this angle; judged under difference 3, not failed here.

## Context views

### ctx-overview-south, ctx-overview-east — PASS
Fields at 300 m: even mottle, no rows, no lattice, no honeycomb; moss sheets 5-40 m; sharpness and band ratio within 3 % of the before; the world reads the same as A1-A3 from above.

### ctx-ground-1 — PASS
- 4x at [500,900,300,150]: about 20 discrete pebbles and slabs with an edge (>= 15 met); sharpness 9.73 -> 8.73, band 0.90-0.98 with 0.80 at 11 px. 3x at [300,760,600,220] 8.67 -> 7.75; mid field [900,700,500,250] 7.61 -> 7.04.
- 6x at [500,960,200,90]: one hazy overlap-band patch about 45 x 25 px at full-frame [500,995,45,25] (same defect as 04, difference 3); below 1x visibility, noted not failed.

### ctx-ground-2 — FAIL
- Moss in the lower band [0,500,1920,580] 0.053 -> 0.063 (<= 0.10 met). Lichen carpet [1100,780,400,200] one patch, as r5.
- [200,900,300,150] sharpness 8.74 -> **7.57**, condition >= 8.5 not met; std 10.7 -> 10.2, band ratio 0.86 / 0.88 / 0.88 / 0.82 / 0.83, flat across octaves: the overlap bands' contrast loss over 25-48 % of the surface (the r5 "one-angle streaks" are gone; 4x shows the same fine olive mottle as the before, softer).
- Fix type: CODE. Difference 3.

### ctx-ground-3 — FAIL
- [100,850,400,200] 9.15 -> 8.87 (>= 8.5 met); [600,900,300,150] 10.48 -> 9.17; moss 0.011 -> 0.027 (met). No smear.
- 5x at [80,880,300,110]: a row of six parallel dark diagonal dashes at one angle, about [230,925,150,60] at 1x, and a second ladder of the same dashes at [1500,650,400,100] (full frame, right): the `mossy_rock` crack motif of layer B, now at one angle in every patch because B has `Rot` 0 (difference 1). The r5 condition "no one-angle streaks" is not met by these.
- Fix type: CODE. Difference 1.

### ctx-ground-4 — PASS
Mud [100,850,300,150] 11.15 -> 10.33 (>= 10), [500,750,300,150] 8.76 -> 7.78, [0,700,900,380] 8.95 -> 8.28, band 0.91-0.99; 4x: clumpy granular mud with crack rings, no left-right drag (r5's drag is gone). Cobble [1000,900,400,180] 4.93 -> 4.40, continuous courses, no seam. Far mud 3x at [0,600,600,130]: faint one-angle ribbing at 10-20 m, same as the before (difference 2 covers it).

## Sun-20 views

### A1-sun20 — PASS
Twin difference 38.2. 2x [700,500,500,300] 5.53 -> 5.20, 4x [640,650,220,220] 8.27 -> 7.86: no polygonal step, no seam, no cell edge on R, T or grass; the margin's ribbing reads as mottle at this light.

### A1-margin-sun20 — PASS
Before|after 6.6. 3x at [500,820,400,250]: courses continuous through every hex border, no mortar-line seam, no tone step (6.10 -> 5.37). Full frame: the earth margin at 1-5 m is granular, no cell, no step.

### ctx-ground-4-sun20 — FAIL
Twin difference 44.7. Cobble 2x [900,700,600,300] 5.03 -> 4.63, no steps. Mud [0,700,900,380] 10.49 -> **9.87** (condition >= 10 not met; README says 10.13, `sharp.py` on the committed JPEG says 9.87), band 0.95 / 0.97 / 0.97 / 0.95 / 0.91; 2x: no left-right drag, the loss is the flat overlap-band contrast loss of difference 3.
- Fix type: CODE. Difference 3; Codex corrects the README number (difference 5).

## Measurement credibility

1. Gate reproduces (`max_peak 0.1602`). It passes crops that fail the human check (A2/A3 grass: same striped motif, 8-12 copies; A3 margin: one-angle lattice), because the motif's copies sit at random offsets and the peak is the autocorrelation at one lag. The spec's human check is the binding one for those crops.
2. Bisect table reproduces to 0.01 on the committed `bisect-*` frames. Reading it: `rot0` 15.41 vs `plainN` 13.99 vs base 13.26 says most of the loss at `Rot` 1.0 follows the rotated **albedo** sampling, not the rotated normal (an unrotated plain normal under rotated albedo is itself a mismatch, so `plainN` is not a clean control). The GBuffer dumps say the opposite for the albedo (BaseColor spread -9 %, sharpness 5.15 -> 5.01, intact) and WorldNormal sharpness 11.74 -> 17.86 (noisier, not weaker). Neither r5 decision rule fired. The one control that separates the two paths cleanly, a flat normal under `Rot` 1.0 albedo, was not run; it is fix 1 below.
3. Perf: `perf-01.json` median 7.33 -> 7.25 ms, p95 9.67 -> 8.84 (within +1 ms); `ProfileGPU` BasePass 0.838 -> 1.389 ms (+0.55, within +0.6; the other passes within 0.02 except Lumen card captures +0.06 and PostProcessing -0.10, noise). Met.
4. Sun views credible (twins 58.0 / 38.2 / 44.7 > 8).
5. Spec drift: `import_ground.py` `layer()` default `rot=0.0` (comment "J2-pre r6") and the spec section 2a Interim paragraph / blueprint 2a table still say `Rot` 1.0 per layer. The committed material no longer implements the documented technique and nothing in the decision doc records that.

## Differences, most visible first

1. Layer B (`mossy_rock`) at `Rot` 0.0: the same three-dash stripe at one angle in every moss patch (A2 grass crop 8 copies, A3 grass crop 12 copies, ctx-ground-3 two dash ladders).
   What's wrong: spec human check fails (>= 2 copies of one motif in a crop); regression against r5, which passed these crops with B rotated.
   Fix type: CODE. Exact: `GameClient/ArtSource/Greenlands/NatureFamilies/import_ground.py` line 138, replace
   ```python
   fb, nb, rb, _ = layer('B', T + 'mossy_rock_Diffuse', T + 'mossy_rock_nor_gl', T + 'mossy_rock_Rough', 150, (0.34, 0.46, 0.20), 1200, hex=True)
   ```
   with
   ```python
   fb, nb, rb, _ = layer('B', T + 'mossy_rock_Diffuse', T + 'mossy_rock_nor_gl', T + 'mossy_rock_Rough', 150, (0.34, 0.46, 0.20), 1200, hex=True, rot=1.0)  # r6 review fix 2: B keeps its rotation (its crack dashes are the moss-patch motif at A2/A3; no relief loss was ever shown on B)
   ```
   B had no relief finding in r4/r5 (its patches are 1-3 m at 30-250 m, no 1-5 m view shows it lit). Acceptance: A2 [1230,800,256,256] and A3 [1075,520,190,190] at 3-4x: the stripe angle differs between patches, no two patches share angle and outline; ctx-ground-3 [80,880,300,110] at 5x: no row of parallel dashes; gate still <= 0.25.

2. Layer T (`brown_mud_02`) at `Rot` 0.0: one-angle diamond ribbing on the earth margin at A2 [805,600,80,150] and A3 [880,600,45,120], faint at ctx-ground-4 [0,600,600,130].
   What's wrong: the mud tile's rib lattice is invariant under offsets (and under 90° or mirror, so no cheap substitute for rotation exists); only per-cell rotation breaks it, and rotation is what r5 showed smearing the mud (ctx-ground-4 [100,850,300,150] 8.49 at `Rot` 1.0). So this is the same unlocated defect as the 04 relief loss, and it needs the real fix, not a workaround.
   Fix type: CODE. Exact, two steps, one capture between them:
   (a) Control capture. `import_ground.py`: extend the `BISECT` values with `'flatN'`; in `hexlayer` replace the `N` return expression
   ```python
   (g.mask(g.smp('Normal' + p, tex(nor), S.SAMPLERTYPE_NORMAL, uv, -1100, y + 250), True, True, True, -950, y + 250) if BISECT == 'plainN' and p == 'A' else pick('N', y + 250, **rgb))
   ```
   with
   ```python
   (node(m, u.MaterialExpressionConstant3Vector, -950, y + 250, constant=u.LinearColor(0.0, 0.0, 1.0, 0.0)) if BISECT == 'flatN' and p == 'A' else pick('N', y + 250, **rgb))
   ```
   and in the layer A call add `rot=1.0 if BISECT == 'flatN' else 0.0`. One capture of `04-forest-floor` and `04-forest-floor-sun20` (`GFX_ONLY`), `sharp.py` on [600,450,400,250] and [1400,200,520,380] into the README table as row `flatN (A Rot 1, normal (0,0,1))`. Decision: `flatN` >= 15 on the default view (the albedo at `Rot` 1.0 is sharp when no normal fights it) means the normal path is the cause and step (b) is the fix; `flatN` < 14 means the rotated albedo sampling itself is soft in the engine and step (b) is wrong: stop, record, and the fix becomes `D.SampleGrad` -> `D.SampleBias(Material.Wrap_WorldGroupSettings, uv, -0.5)` per tap as the next single-capture check (anisotropic filter under rotated gradients).
   (b) World-space normal (the UV is `WorldPosition.xy`, so the normal map's tangent frame is world X/Y, not the mesh's UV tangent frame; rotating `n.xy` in the mesh frame is only right if that frame is world-aligned, and the two transposition signs tried in r4/r5 both assume it is). `hex_tile.py`: add to `spec` after `HeightSharp`
   ```python
   ('VN', T.FUNCTION_INPUT_VECTOR3, None)
   ```
   with, in the input loop, for `FUNCTION_INPUT_VECTOR3`: `pv = u.Vector4f(); pv.set_editor_property('z', 1.0); fi.set_editor_property('preview_value', pv); fi.set_editor_property('use_preview_value_as_default', True)`; add `'VN'` to the `CustomInput` list (it is built from `spec`, so nothing else changes). In `HLSL` replace line 43
   ```hlsl
   Nrm = normalize(W.x * cN[0] + W.y * cN[1] + W.z * cN[2]);
   ```
   with
   ```hlsl
   float3 n = normalize(W.x * cN[0] + W.y * cN[1] + W.z * cN[2]);
   float3 Tw = normalize(cross(float3(0.0, 1.0, 0.0), VN)), Bw = cross(VN, Tw);   // world-aligned tangent frame: u = world +x, v = world +y (UV is WorldPosition.xy); the mesh's own UV tangents are not it
   Nrm = normalize(Tw * n.x + Bw * n.y + VN * n.z);                                 // world-space normal; the material sets tangent_space_normal False
   ```
   `import_ground.py` `hexlayer`: `link(node(m, u.MaterialExpressionVertexNormalWS, -1300, y + 460), '', call, 'VN')`. In `ground()` after `m = master('M_GN_Ground')`: `m.set_editor_property('tangent_space_normal', False)`. Bedrock `bn` (line 160, the only plain normal reaching `MP_NORMAL`): wrap it in a `MaterialExpressionCustom` with inputs `n` (= `bn`, pin `RGB`) and `VN` (= a `VertexNormalWS` node), `output_type` `CMOT_FLOAT3`, code `float3 Tw = normalize(cross(float3(0.0, 1.0, 0.0), VN)), Bw = cross(VN, Tw); return normalize(Tw * n.x + Bw * n.y + VN * n.z);`, and use its output in place of `bn` in the `nrm` lerp. Remove the `plainN`/`flatN` bisect branches once (a) is recorded. Then `layer()` default back to `rot=1.0` (spec), cobble `R` stays `rot=0.0` with `Snap` 14. If the sun-20 capture shows slab undersides lit on the sun side (frame mirrored in y), change `Bw = cross(VN, Tw)` to `Bw = -cross(VN, Tw)`; never the angle sign.
   Acceptance: 04 [600,450,400,250] >= 15 with slabs and undersides at `Rot` 1.0 on A; 04-sun20 strip >= 17, no border step; ctx-ground-4 mud [100,850,300,150] >= 10 and ctx-ground-4-sun20 [0,700,900,380] >= 10 with no directional drag at 4x; A2 [805,600,80,150] and A3 [880,600,45,120] at 6-8x: rib direction changes from cell to cell, no run of one-angle dashes longer than one cell (2 m = 13 px at A3).

3. Overlap-band contrast loss reads as double exposure at 1-5 m (04 [1330,300,220,60] and [1530,150,100,30] hazy patches; ctx-ground-1 [500,995,45,25]; ctx-ground-2 near field 7.57 < 8.5; ctx-ground-4-sun20 mud 9.87 < 10; band ratios 0.82-0.88 flat across octaves on every `Rot` 0 layer).
   What's wrong: a 50/50 band of two uncorrelated taps has 1/sqrt(2) of the texel contrast; with offsets only, the two taps are the same print shifted, so the band reads as a faded double image rather than noise. The paper's `lerp(1, L, 0.6) * pow(w, 7)` narrows bands, it does not restore their contrast.
   Fix type: CODE. Exact: `hex_tile.py`, replace lines 42-46
   ```hlsl
   float3 W = lerp(1.0, L, 0.6) * pow(w, 7.0) * exp2(max(HeightSharp * (Hv - max(Hv.x, max(Hv.y, Hv.z))), -3.0)); W /= (W.x + W.y + W.z);   // ...
   Nrm = normalize(W.x * cN[0] + W.y * cN[1] + W.z * cN[2]);
   Msk = W.x * cM[0] + W.y * cM[1] + W.z * cM[2];
   Hgt = W.x * cH[0] + W.y * cH[1] + W.z * cH[2];
   return W.x * cD[0] + W.y * cD[1] + W.z * cD[2];
   ```
   with
   ```hlsl
   float3 W = lerp(1.0, L, 0.6) * pow(w, 7.0) * exp2(max(HeightSharp * (Hv - max(Hv.x, max(Hv.y, Hv.z))), -3.0)); W /= (W.x + W.y + W.z);   // height tie-break capped at 8x (r5 review)
   float k = rsqrt(dot(W, W));                                                                 // variance-preserving blend (Heitz-Neyret 2018): a 50/50 band keeps the texel contrast instead of 1/sqrt(2) of it (r6 review: hazy double-exposure bands at 04)
   float4 mD = D.SampleLevel(Material.Wrap_WorldGroupSettings, float2(0.5, 0.5), 20.0);          // texture mean = its last mip
   float4 mM = M.SampleLevel(Material.Wrap_WorldGroupSettings, float2(0.5, 0.5), 20.0);
   float  mH = H.SampleLevel(Material.Wrap_WorldGroupSettings, float2(0.5, 0.5), 20.0).r;
   float3 n = float3((W.x * cN[0].xy + W.y * cN[1].xy + W.z * cN[2].xy) * k, W.x * cN[0].z + W.y * cN[1].z + W.z * cN[2].z);   // mean normal is (0,0,1): scale the xy deviation only
   Nrm = normalize(n);                                                                          // (fix 2b replaces this line with the world-frame lines; keep n as the input there)
   Msk = max(mM + (W.x * cM[0] + W.y * cM[1] + W.z * cM[2] - mM) * k, 0.0);
   Hgt = max(mH + (W.x * cH[0] + W.y * cH[1] + W.z * cH[2] - mH) * k, 0.0);
   return max(mD + (W.x * cD[0] + W.y * cD[1] + W.z * cD[2] - mD) * k, 0.0);
   ```
   `k` is 1.0 inside a cell (no change) and up to 1.73 at a three-way corner. Three extra `SampleLevel` taps per layer, all at the resident 1 x 1 mip (re-time; BasePass must stay within +0.6 ms of the before). Update the HLSL block in GFX-02 section 2a to match (Codex, difference 5). Acceptance: 04 3x at [1250,250,450,220] and [1400,80,450,200]: no washed patch larger than 40 x 40 px with 24 px local luminance std below half the ground's median; [1300,300,300,200] band ratio >= 0.8 at 11 px; ctx-ground-2 [200,900,300,150] >= 8.5; ctx-ground-4-sun20 [0,700,900,380] >= 10; no new seam or doubled stone at 3x in any 04 or ctx-ground window (a contrast gain inside a band must not read as a hard edge: if it does, cap `k` at 1.4).

4. Earth-margin gate rectangle at A3 measures the one 32 x 160 px strip that reads lowest (0.160) while neighbouring strips on the same margin read 0.21-0.25.
   Fix type: CODE (data). Exact: `silverbrook-reference-bookmarks.json` `A3-south-approach-250m` `repeat_crops` `earth` [884,600,32,160] -> [884,600,32,215] (the full margin run in frame, same x); re-run the gate on both measure folders. This is a reporting fix; difference 2 is what makes it pass on its merits.

5. Documentation and numbers.
   What's wrong: README "Window numbers" says ctx-ground-4-sun20 10.13 (committed JPEG reads 9.87 with `sharp.py`); the spec section 2a Interim paragraph and blueprint 2a table say `Rot` 1.0 while the shipped material has 0.0 on every layer with no decision recorded; the section 2a HLSL block will differ from `hex_tile.py` after fix 3.
   Fix type: CODE (docs). Exact: README round-6 number corrected; `docs/decisions/GFX-02-photoreal-ground-pipeline.md` section 2a: the Interim paragraph gets one sentence per the outcome of fix 2 (either "`Rot` 1.0 restored on A, B, T, S with the world-space normal frame (r6 review fix 2)" or, if 2a's control fails, the measured reason `Rot` is 0.0 and what replaces it), and the HLSL block updated to the fix-3 blend; blueprint 2a "Today (J2-pre)" row updated the same way; `docs/dependency-map.md` row unchanged unless file roles change.

## Fix list (job domain, deduplicated)

1. Engineer — `flatN` control capture at 04 (`BISECT = 'flatN'`, layer A `Rot` 1.0, normal (0,0,1)), numbers in the README (difference 2a). One capture; decides fix 2b.
2. Engineer — world-space normal frame in `MF_GN_HexTile` (`VN` input, `Tw`/`Bw`/`VN` frame), `M_GN_Ground` `tangent_space_normal` False, bedrock normal wrapped the same way, bisect branches removed, `layer()` default `rot=1.0` restored (difference 2b). Gate for the round: 04 >= 15 and mud >= 10 at `Rot` 1.0.
3. Engineer — layer B `rot=1.0` now, independent of fix 2 (difference 1).
4. Engineer — variance-preserving blend in `hex_tile.py` (`k = rsqrt(dot(W, W))`, last-mip means) (difference 3).
5. Codex — A3 `earth` crop [884,600,32,215]; gate re-run on both measure folders; README table (difference 4).
6. Codex — README number 10.13 -> 9.87; GFX-02 section 2a Interim sentence and HLSL block; blueprint 2a row (difference 5).
7. Codex — re-run `import_ground.py`; recapture `after/` (all views, four sun views) and `after-measure/`; `perf_gpu.py` and the 500-frame timing; rebuild pairs; README Round 7 with the `flatN` row; fresh review. Pass condition: nine crops <= 0.25; A2 [1230,800,256,256] and A3 [1075,520,190,190] at 3-4x with no two moss patches sharing stripe angle and outline; A2 [805,600,80,150] and A3 [880,600,45,120] at 6-8x with no one-angle rib run longer than one cell; after/04 [600,450,400,250] >= 15 with slabs and dark undersides at 3x, band ratio >= 0.8 at every octave, no straight tone edge and no washed patch > 40 x 40 px in [1250,250,450,220] or [1400,80,450,200]; 04-sun20 [1400,200,520,380] >= 17, relief on the albedo's slabs, no brightness step along a hex border; ctx-ground-1 [500,900,300,150] >= 15 pebbles with an edge; ctx-ground-2 [200,900,300,150] >= 8.5 and ctx-ground-3 [100,850,400,200] >= 8.5 with no one-angle dash row in [80,880,300,110]; ctx-ground-4 mud [100,850,300,150] >= 10 and ctx-ground-4-sun20 [0,700,900,380] >= 10 (measured with `sharp.py` on the committed file), no directional drag at 4x; moss coverage ctx-ground-2 and ctx-ground-3 lower band <= 0.10; sun views differ from their twins by > 8; perf median at 01 within +1 ms of the before run and BasePass within +0.6 ms.

## r5 fix items: status

- r5 fix 1 (bisect, GBuffer dumps, fix per the decision rules, `BISECT` back to `''`): the four variants and the dumps were run and recorded (numbers reproduce). Neither decision rule fired (BaseColor spread -9 %, WorldNormal sharpness 1.5x, not < 0.7x), and the fix applied (`Rot` 0.0 on every layer) is outside the rules and outside the spec. 04 >= 15 met (15.76); ctx-ground-1 pebbles met; ctx-ground-3 >= 8.5 met but one-angle dash rows present; ctx-ground-4-sun20 >= 10 not met (9.87). Partly met; the cause is still unlocated (fixes 1-2).
- r5 fix 2 (height term capped at -3): implemented as written; no straight tone step at 04 in either light. Met.
- r5 fix 3 (EdgeNoise domain warp x2.5): implemented; the 3.5 m edge period is gone (lag (1,-23) -> (-1,-6)). Its stated number (old crop <= 0.15) reads 0.329 because the edge line itself autocorrelates along the road; the intent is met.
- r5 fix 4 (A3 cobble crop [925,600,55,215]): implemented; 0.076. Met.
- r5 fix 5 (`NEG_ANGLE` False, comment rewritten): met; moot at `Rot` 0.0.
- r5 fix 6 (recapture, perf, pairs, pass condition): captures, perf row and pairs done; pass condition not met on A2/A3 motif copies (new), ctx-ground-2, ctx-ground-3 (dash rows), ctx-ground-4-sun20, and the 04 overlap-band haze (new).

## Carried forward (not this job)

- Aerial colour blend 25-60 m and macro luminance variation per layer (J2).
- Low-poly lime-green blob trees and bushes at A1-A3 and the overviews; identical house rows; bare plain with no fields, hedges or tracks between town and river.
- White blob mountains without rock or snow detail (A1, 01, ctx-ground-1/2/4).
- Cobbled carriageway where `gameplay-hud.png` far range shows a dirt track; beige cobble vs the concept's grey-brown stone.
- Bookmark 04 vs `Greenlands-Forest-Look-v1.webp`: no leaf litter, needles or undergrowth variety; hard 1 px moss-patch edges at 1 m from `Low 0.4 / InvRange 32` (present in the before).
- Hard-edged river bank and flat water colour (A2, A3).
- `mossy_rock` crack dashes as the moss-patch texture (they read as rock slab even when rotated): a texture choice for layer B.
- If fix 1's control shows the rotated albedo itself soft in the engine, the sampler/anisotropy question moves to J2 with the landscape material.
