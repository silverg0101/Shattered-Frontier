# GFX-02 J2-pre: hex-tiled ground on today's mesh terrain (evidence)

Spec: `docs/decisions/GFX-02-photoreal-ground-pipeline.md` section 2a (Interim J2-pre). No verdict here; the reviewer writes `REVIEW.md`.

## Round 8 (after REVIEW-r7.md: 11 PASS, 4 FAIL)
Implemented from the r7 fix list; `rib_angle.py` (structure-tensor rib-angle coherence `R`, 3 px high-pass) added for fix 2's acceptance.
1. `hex_tile.py` inputs `BlendSharp` (default 7) and `Warp` (lattice warp, tiles); lattice from `pw = p + Warp / CellTiles`, texels from `p`; `pow(w, BlendSharp)`. `import_ground.py`: shared lattice warp `hwarp` (two Simplex noises, `scale` 0.004 = 2.5 m, `HexWarp` 0.25) and `BlendSharp<p>` 24 on every hex layer.
2. Rule run at `Rot` 1.0 on every layer: 04 13.17 with `BlendSharp` 24 (`bisect-bs24rot1/`), below 14, so `BlendSharp` 32 for one more capture: 13.18 (`bisect-bs32rot1/`; mud 9.03 / 9.72, rib `R` A2 0.02, A3 0.44). The bands are ruled out as the cause of the `Rot` 1.0 loss. Deviation (measured, per layer): A `Rot` 0.15 (04 16.08), T `Rot` 0.5 (`bisect-T075/`: at 0.75 mud 9.37 / 9.94 and A3 `R` 0.58, no better than 0.5), S and B 1.0, cobble R 0.0 + `Snap` 14. `BlendSharp` stays 24 (no measured gain at 32).
3. Recaptured all views and the measure frames; pairs (`SUN_TWIN_DIFF` 57.1 / 38.3 / 44.7); gate; re-timed.
4. Docs: GFX-02 section 2a "J2-pre as shipped (r8)" (refuted frame cause replaced with the measured facts, new inputs, per-layer `Rot`), HLSL block regenerated from `hex_tile.py`; blueprint 2a row.

Windows (`sharp.py`, before -> r7 -> r8): 04 16.95 -> 15.44 -> 16.22; 04-sun20 18.40 -> 18.35 -> 18.24; ctx-ground-1 9.73 -> 9.41 -> 9.40; ctx-ground-2 8.74 -> 7.86 -> 7.69; ctx-ground-3 9.15 -> 9.16 -> 8.93; ctx-ground-4 mud 11.15 -> 10.76 -> 9.93; ctx-ground-4-sun20 10.49 -> 10.48 -> 10.15.
Rib-angle `R` on the earth gate crops (before -> r8): A1 [705,730,110,96] 32 px 0.95 -> 0.93; A2 [815,600,64,256] 32 px 0.92 -> 0.03; A3 [884,600,32,215] 16 px 0.76 -> 0.57.

### Repeat gate, round 8
`GROUND_REPEAT PASS crops 9 max_peak 0.1666 limit 0.25` (before `FAIL max_peak 0.665`).
| crop | before | r8 after (lag px) |
|---|---|---|
| A1 cobble | 0.225 | 0.119 (12,-30) |
| A1 earth | 0.544 | 0.124 (-15,-6) |
| A1 grass | 0.244 | 0.167 (-15,0) |
| A2 cobble | 0.583 | 0.096 (7,-4) |
| A2 earth | 0.665 | 0.095 (-1,-26) |
| A2 grass | 0.342 | 0.062 (-57,-15) |
| A3 cobble | 0.628 | 0.070 (4,-27) |
| A3 earth | 0.500 | 0.122 (-2,-18) |
| A3 grass | 0.433 | 0.083 (24,18) |

### Timing at bookmark 01, round 8 material
Frame time: before 7.33 / 9.67 ms median / p95, after 7.39 / 8.80 ms (+0.06). ProfileGPU (one frame): GPU frame 5.43 -> 5.87 ms, BasePass 0.84 -> 1.26 ms (+0.42).
## Round 7 (after REVIEW-r6.md: 9 PASS, 6 FAIL)
Implemented from the r6 fix list (04 window [600,450,400,250] / 04-sun20 strip [1400,200,520,380] with `sharp.py`; each variant one `import_ground.py` + `GFX_ONLY` capture, `bisect-<v>/04-forest-floor.jpg` kept):
1. `flatN` control (layer A `Rot` 1.0, normal (0,0,1)): 11.71 / 7.32. Below 14, so by the r6 rule the albedo would be the suspect; but the r6 GBuffer dump already shows the albedo at `Rot` 1.0 intact (BaseColor sharpness 5.01 vs 5.15 before), and a flat normal removes the relief the before has, so the control cannot separate the paths. Fix 2 was built and measured instead:
2. World-space normal frame: `MF_GN_HexTile` input `VN` (`VertexNormalWS`), `Tw = normalize(cross((0,1,0), VN))`, `Bw = cross(VN, Tw)`, `M_GN_Ground` `tangent_space_normal` False, bedrock C's plain normal wrapped into the same frame (`WorldNormalC` Custom node). Bisect branches removed. Measured:

   | variant (world frame) | 04 | 04-sun20 | ctx-ground-3 | ctx-ground-4 mud | ctx-ground-4-sun20 |
   |---|---|---|---|---|---|
   | before (plain) | 16.95 | 18.40 | 9.15 | 11.15 | 10.49 |
   | `Rot` 1.0, `Bw` +y | 13.28 | 15.28 | 7.17 | 8.66 | 9.79 |
   | `Rot` 1.0, `Bw` -y (wsBflip) | 17.14 | 19.03 | 7.55 | 9.02 | 7.74 |
   | `Rot` 0, `Bw` +y (wsRot0) | 15.80 | 18.61 | | | |
   | `Rot` 0, `Bw` -y (wsRot0all) | 11.21 | 14.07 | 6.64 | 9.27 | 8.36 |
   | `Rot` 1.0, `Bw` +y, normal turned -a (wsNeg) | 9.79 | 14.33 | 7.39 | 9.05 | 9.00 |
   | `Rot` 1.0, `Bw` -y, plain `Sample` for D (sampleD) | 17.10 | 19.18 | 7.32 | 8.88 | 7.84 |
   | `Rot` 1.0, `Bw` -y, `k` 1 (k1) | 17.13 | 19.03 | 7.06 | 8.35 | 7.45 |
   | `Rot` 0.25, `Bw` +y (wsRot025) | 14.82 | 18.46 | 8.68 | 10.37 | 10.72 |
   | `Rot` 0.15, `Bw` +y (wsRot015) | 15.41 | 18.47 | 9.05 | 10.79 | 10.75 |

   `Rot` 0 settles the frame: +y (15.8) is right, -y (11.2) is mirrored; with +y the un-negated normal rotation is right (wsNeg is worse). At full rotation the consistent frame still loses relief (13.3) and the mud smears (9.8); the -y frame scores high at 04 but is the mirrored frame and smears the mud worse (7.7). Remaining cause: the scans' baked shading turned against the sun. Deviation from the list: `layer()` default is `rot=0.15` (+-27 deg) instead of 1.0, the best measured value that keeps every window; B keeps 1.0 (fix 3), cobble R 0.0 + `Snap` 14.
3. Layer B `rot=1.0`.
4. Variance-preserving blend: `k = rsqrt(dot(W, W))` around the last-mip means of D, M, H; normal xy scaled by `k`.
5. A3 earth crop -> [884,600,32,215].
6. README 10.13 -> 9.87; GFX-02 section 2a HLSL block regenerated from `hex_tile.py` and a "J2-pre as shipped" paragraph; blueprint 2a "Today" row.
7. Recaptured `after/` and `after-measure/`, pairs (`SUN_TWIN_DIFF` 57.7 / 38.3 / 44.7), re-timed.

Window numbers (before -> after, `sharp.py` on the committed JPEGs): 04 16.95 -> 15.44; 04-sun20 18.40 -> 18.35; ctx-ground-1 [500,900,300,150] 9.73 -> 9.41; ctx-ground-2 [200,900,300,150] 8.74 -> 7.86 (a moss patch sits in this window in the after); ctx-ground-3 [100,850,400,200] 9.15 -> 9.16; ctx-ground-4 mud [100,850,300,150] 11.15 -> 10.76; ctx-ground-4-sun20 [0,700,900,380] 10.49 -> 10.48.

### Repeat gate, round 7
`GROUND_REPEAT PASS crops 9 max_peak 0.1727 limit 0.25` (before `FAIL max_peak 0.665`).
| crop | before | r7 after (lag px) |
|---|---|---|
| A1 cobble [876,850,256,222] | 0.225 | 0.122 (12,-30) |
| A1 earth [705,730,110,96] | 0.544 | 0.106 (-14,-7) |
| A1 grass [175,440,128,128] | 0.244 | 0.173 (18,-1) |
| A2 cobble [885,790,110,256] | 0.583 | 0.127 (7,-4) |
| A2 earth [815,600,64,256] | 0.665 | 0.110 (-18,-4) |
| A2 grass [1230,800,256,256] | 0.342 | 0.063 (-56,-16) |
| A3 cobble [925,600,55,215] | 0.628 | 0.072 (5,-55) |
| A3 earth [884,600,32,215] | 0.500 | 0.097 (1,81) |
| A3 grass [1075,520,190,190] | 0.433 | 0.085 (-26,-26) |

### Timing at bookmark 01, round 7 material
Frame time: before 7.33 / 9.67 ms median / p95, after 7.89 / 10.04 ms (+0.56 ms). ProfileGPU (one frame): GPU frame 5.43 -> 6.01 ms, BasePass 0.84 -> 1.30 ms (+0.46).
## Round 6 (after REVIEW-r5.md: 7 PASS, 8 FAIL)
Implemented from the r5 fix list:
1. Bisect of the `Rot 1.0` relief loss (`BISECT` in `import_ground.py`, `hex_tile.build(nograd=...)`, one 04 capture per variant in `bisect-<v>/`; `py -3.12 sharp.py <img> x y w h`, mean |dx|+|dy| of luminance). Variants other than `base` run with `HeightSharp` 0.

   | variant | 04 [600,450,400,250] | 04-sun20 [1400,200,520,380] |
   |---|---|---|
   | before (plain) | 16.95 | 18.40 |
   | base (`NEG_ANGLE` False, capped H, 96) | 13.26 | 14.96 |
   | rot0 (layer A `Rot` 0) | 15.41 | 18.50 |
   | plainN (A normal plain, unrotated) | 13.99 | 15.68 |
   | cell4 (A `CellTiles` 4) | 10.47 | 14.23 |
   | nograd (unrotated gradients) | 12.93 | 14.62 |

   GBuffer at 04 (`GFX_DUMP=1`, `dump-before/`, `dump-after/` = base): BaseColor p5-p95 spread 20.6 before, 18.8 after (-9 %), BaseColor sharpness 5.15 / 5.01; WorldNormal sharpness 11.6 before, 16.4 after. The albedo is intact and the normals are not weaker: the rotated normals disagree with the rotated albedo on this mesh's tangent frame (neither sign fixes it; r5 had `NEG_ANGLE` True at 9.83). Gradients are not the cause (nograd). Fix applied: `layer()` default `rot=0.0`, so every hex layer (A, B, T, S; cobble R already 0) uses per-cell offsets only. `BISECT` back to `''`; `NEG_ANGLE` back to `False` with the comment rewritten (no effect at `Rot` 0).
2. `hex_tile.py` height term capped: `exp2(max(HeightSharp * (Hv - max(Hv)), -3.0))`; `HeightSharpA` stays 96.
3. `EdgeNoise` UV domain-warped by the Mask's `warp` x `EdgeWarp` 2.5 (bounds 0-4).
4. A3 cobble crop -> [925,600,55,215].
5. `NEG_ANGLE = False`, comment rewritten.
6. Recaptured `after/` (all views) and `after-measure/`; pairs rebuilt (`SUN_TWIN_DIFF` 58.0 / 38.2 / 44.7); re-timed.

Window numbers (same metric, before -> after): 04 16.95 -> 15.78; 04-sun20 lit strip 18.40 -> 18.67; ctx-ground-1 [500,900,300,150] 9.73 -> 8.73; ctx-ground-3 [100,850,400,200] 9.15 -> 8.84; ctx-ground-4 mud [100,850,300,150] 11.15 -> 10.35; ctx-ground-4-sun20 [0,700,900,380] 10.49 -> 9.87 (r6 review: 10.13 was a misreport; 9.87 on the committed JPEG). Moss coverage (G-R > 8, y 500-1080): ctx-ground-2 0.053 -> 0.063, ctx-ground-3 0.011 -> 0.027.

### Repeat gate, round 6
`GROUND_REPEAT PASS crops 9 max_peak 0.1602 limit 0.25` (before `FAIL max_peak 0.8397`).
| crop | before | r6 after (lag px) |
|---|---|---|
| A1 cobble [876,850,256,222] | 0.225 | 0.122 (12,-30) |
| A1 earth [705,730,110,96] | 0.544 | 0.123 (-9,-24) |
| A1 grass [175,440,128,128] | 0.244 | 0.094 (21,-1) |
| A2 cobble [885,790,110,256] | 0.583 | 0.121 (-7,4) |
| A2 earth [815,600,64,256] | 0.665 | 0.110 (-6,-6) |
| A2 grass [1230,800,256,256] | 0.342 | 0.082 (10,-10) |
| A3 cobble [925,600,55,215] | 0.628 | 0.076 (1,6) |
| A3 earth [884,600,32,160] | 0.840 | 0.160 (5,-12) |
| A3 grass [1075,520,190,190] | 0.433 | 0.119 (-26,-23) |

### Timing at bookmark 01, round 6 material
Frame time: before 7.33 / 9.67 ms median / p95, after 7.25 / 8.84 ms. ProfileGPU (one frame): GPU frame 5.43 -> 5.88 ms, BasePass 0.84 -> 1.39 ms (+0.55), RenderDeferredLighting 0.83 -> 0.85, ShadowDepths 0.50 -> 0.50, Nanite::VisBuffer 0.43 -> 0.41 (`before/perf-01-gpu.json`, `after/perf-01-gpu.json`). The round-5 table below is superseded.
## Round 5 (after REVIEW-r4.md: 10 PASS, 5 FAIL)
Implemented from the r4 fix list:
1. `hex_tile.py` `NEG_ANGLE = True`. Deviation in how: the old switch replaced `a *= Rot` with `a *= -Rot`, which turns the UV and the normal together and changes nothing about their relation; the switch now transposes only the normal rotation (`n.xy = mul(n.xy, R)`), which is what the flag's comment and the r4 diagnosis describe.
2. `hex_tile.py` blend `exp2(HeightSharp * (Hv - max(Hv)))`; `HeightSharpA` 16 -> 96 (bounds 0-160).
3. `MaskNoise` 0.05 -> 0.02 (bounds 0-0.03).
4. A3 cobble crop -> [915,600,70,215]; gate re-run on both folders.
5. Recaptured `after/` (all views) and `after-measure/`; re-timed bookmark 01 on this material; pairs rebuilt (`SUN_TWIN_DIFF` 58.0 / 39.2 / 44.7).

### Repeat gate, round 5
`GROUND_REPEAT FAIL crops 9 max_peak 0.2563 limit 0.25` (before `FAIL max_peak 0.8397`).
| crop | before | r5 after (lag px) |
|---|---|---|
| A1 cobble | 0.225 | 0.124 (12,-30) |
| A1 earth | 0.544 | 0.085 (-34,10) |
| A1 grass | 0.244 | 0.164 (17,-1) |
| A2 cobble | 0.583 | 0.124 (7,-4) |
| A2 earth | 0.665 | 0.115 (-6,39) |
| A2 grass | 0.342 | 0.075 (-16,-9) |
| A3 cobble [915,600,70,215] | 0.667 | 0.256 (1,-23) FAIL |
| A3 earth | 0.840 | 0.139 (2,-25) |
| A3 grass | 0.433 | 0.067 (-20,-9) |
The cobble layer R did not change this round (`Rot` 0, so neither the normal sign nor H touches it); A3 cobble moved 0.227 -> 0.256 with the shorter crop alone.

### Timing and per-pass GPU at bookmark 01 (2560x1440 windowed, RX 7900 XTX, other editors running)
Frame time (`perf-01.json`, 500 frames): before (original `M_GN_Ground` from `d4e21b46^`, this session) 7.33 / 9.67 ms median / p95; after (round-5 material) 7.58 / 9.04 ms; +0.25 ms median, inside the +1 ms budget. Earlier runs: before 7.65-8.20, r1 after 7.66-8.02, r3 after 7.55.
`stat gpu` is an overlay that `HighResShot` does not draw; `perf_gpu.py` now runs `ProfileGPU` (`r.ProfileGPU.ShowUI 0`) and the tree goes to the `-abslog` file; `py -3.12 gpu_passes.py <tag>/perf-01.log` writes `<tag>/perf-01-gpu.json` (one profiled frame, inclusive ms of the direct children of `Scene`).
| pass | before ms | after ms |
|---|---|---|
| GPU frame | 5.43 | 6.17 |
| Scene | 5.25 | 6.00 |
| BasePass | 0.84 | 1.27 |
| RenderDeferredLighting | 0.83 | 0.84 |
| PostProcessing | 0.65 | 0.56 |
| ShadowDepths | 0.50 | 0.49 |
| VolumetricCloud | 0.50 | 0.48 |
| Nanite::VisBuffer | 0.43 | 0.46 |
The hex-tiled ground costs +0.43 ms of BasePass; the rest of the frame difference is single-frame noise (Lumen card captures, GBufferClear).
## Round 4 (after REVIEW-r3.md: 10 PASS, 5 FAIL)
Implemented from the r3 fix list:
1. `MaskNoise` 0.15 -> 0.05 (bounds 0-0.08).
2. `HeightSharpA` 6.0 -> 16.0 (bounds 0-24).
3. GFX-02 section 2a Interim paragraph: B hex (`Rot` 1.0, `CellTiles` 1.0), bedrock C plain, +30 taps.
4. Sun views: `capture_game.py` logs `J2PRE_SUN0` and `J2PRE_SUN <view> pitch`. The logged pitch was -55 (unchanged) before the fix: the map's sun is Stationary and ignores a runtime rotation. The script now sets the sun's root component Movable for the sun views only and runs them last (default views keep the map's lighting). Deviation: pitch -8 (as listed) gives near-black frames (mean luminance 04 5/255, A1 24/255 at fixed exposure), useless for a border check, so the views stay at pitch -20 (the logged pitch is -20.0 for all four). `make_pairs.py` asserts the twin difference: `SUN_TWIN_DIFF 04-forest-floor-sun20 55.9`, `A1-sun20 39.4`, `ctx-ground-4-sun20 45.4` (all > 8). The `before/` sun views were recaptured the same way with the original `M_GN_Ground` (from `d4e21b46^`, restored afterwards) so the sun pairs compare like with like; `GFX_ONLY=<views>` limits a run to named views.
5. Gate crops off the props: A1 cobble [876,850,256,222], A2 cobble [885,790,110,256], A3 cobble [915,600,70,256], A3 earth [884,600,32,160]. Gate re-run on both measure folders.
6. Recaptured `after/` and `after-measure/`, pairs rebuilt.

### Repeat gate, round 4
`GROUND_REPEAT PASS crops 9 max_peak 0.2265 limit 0.25`; before on the same crops `FAIL max_peak 0.8397`.

| crop | before | r4 after (lag px) |
|---|---|---|
| A1 cobble [876,850,256,222] | 0.225 | 0.123 (12,-30) |
| A1 earth | 0.544 | 0.085 (-19,19) |
| A1 grass | 0.244 | 0.148 (15,-1) |
| A2 cobble [885,790,110,256] | 0.583 | 0.119 (7,-4) |
| A2 earth | 0.665 | 0.117 (6,-39) |
| A2 grass | 0.342 | 0.090 (16,9) |
| A3 cobble [915,600,70,256] | 0.641 | 0.227 (1,-23) |
| A3 earth [884,600,32,160] | 0.840 | 0.137 (1,-26) |
| A3 grass | 0.433 | 0.066 (-20,-9) |

## Round 3 (after REVIEW-r2.md: 11 PASS, 4 FAIL; Opus hand-off)
Implemented from the r2 fix list, nothing else:
1. `import_ground.py`: layer B (`mossy_rock`, 150 cm) `hex=True` (`Rot` 1.0, `CellTiles` 1.0, `Snap` 0).
2. `import_ground.py`: `mkv = mk + nzA * MaskNoise` (`MaskNoise` 0.15, `nzA` = the first warp Simplex noise) before `- Low 0.4`; trail stones `sw` still read `mk`.
3. `hex_tile.py`: input `HeightSharp` (scalar, default 0, after `Snap`); blend `W = lerp(1, L, 0.6) * pow(w, 7) * exp2(HeightSharp * H)`. `import_ground.py`: layer A wires `H` = `T_GN_forest_ground_04_Displacement` (parameter `HeightA`) and `HeightSharpA` 6.0. T, S, R have no Displacement texture and keep the default (`HeightSharp` 0 = unchanged).
4. A1 earth crop -> `[705, 730, 110, 96]` (`silverbrook-reference-bookmarks.json`).
5. Recaptured `after/` (all views) and `after-measure/`; `before/`, `before-measure/` unchanged (original material). Pairs rebuilt; `04-forest-floor-before-after-compare.jpg` added (fix 3 acceptance view). The r2 crops `04-sun20-after-crop.png`, `A1-sun20-after-crop.png` were removed (stale).
One compile fix outside the list: the forest normal lerp read `nb` via its `RGB` pin; B is now a component mask, so the pin is `''`.
`import_ground.py` re-saved `M_GN_Surface`, `MI_GN_Road`, `MI_GN_Trail`, `M_GN_River` as side effects; reverted. No compile error for `M_GN_Ground` / `MF_GN_HexTile` (`M_GN_River` fails on the untouched master too).

Bookmark 04 colour history: r1 hex-tiled the Mask, so the moss-rock weight `wm` covered the whole 04 spot and the frame turned into green mossy rock (layer B); r2 put the Mask back to a plain warped sample, which returned 04 to the stony olive forest floor (layer A). Round 3 does not change which layer covers 04 except through `MaskNoise`.

### Repeat gate, round 3 (`repeat-measure.json`)
`GROUND_REPEAT PASS crops 9 max_peak 0.1974 limit 0.25` (before, same crops: `FAIL max_peak 0.665`).

| crop | before | r2 after | r3 after (lag px) |
|---|---|---|---|
| A1 cobble | 0.194 | 0.096 | 0.095 (12,-29) |
| A1 earth [705,730,110,96] | 0.544 | 0.078* | 0.086 (19,-19) |
| A1 grass | 0.244 | 0.280 | 0.098 (-16,0) |
| A2 cobble | 0.436 | 0.110 | 0.110 (-2,-19) |
| A2 earth | 0.665 | 0.118 | 0.116 (-6,39) |
| A2 grass | 0.342 | 0.236 | 0.094 (35,15) |
| A3 cobble | 0.592 | 0.204 | 0.197 (-1,-6) |
| A3 earth | 0.243 | 0.098 | 0.096 (0,-27) |
| A3 grass | 0.433 | 0.295 | 0.062 (-26,-5) |
(* r2 used the old crop [700,715,128,112].)

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

Note: the timing above was measured with the round-1 material (hex Mask). The round-2 material (plain Mask + 2 Simplex noises, CellTiles 1.0) was not re-timed.

## Status
Two Fable reviews (`REVIEW-r1.md`, `REVIEW-r2.md`) did not pass; stopped per the rule for an Opus hand-off. Open items are the fix list in `REVIEW-r2.md` (implemented in round 3) (layer B hex, Mask patch variation, `HeightSharp` tie-break, A1 earth crop).
