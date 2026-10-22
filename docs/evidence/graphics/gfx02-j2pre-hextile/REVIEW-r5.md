# GFX-02 J2-pre hex-tiled ground: independent review, round 5 (Fable, 2026-10-22)

Judged against `docs/decisions/GFX-02-photoreal-ground-pipeline.md` section 2a (Interim J2-pre, Acceptance), section 7 row J2-pre, and the REVIEW-r4.md fix-5 pass condition. Domain: repeating motifs on cobble, earth or grass at A1-A3, plus anything the hex-tiling or its fixes introduced (cells, seams, brightness steps, double exposure, loss of relief, layer-mix regressions). Every frame in `before/`, `after/`, `before-measure/`, `after-measure/` and the six compare sheets was read; the nine gate rectangles were cut before|after at 3x from the measure frames, open-field windows at 2x, ground-level windows at 3-4x, sun views as full frames and 2-4x crops, plus a 6 px Gaussian high-pass of both 04 frames. `Measure-GroundRepeat.py` was re-run on `after-measure/` (identical: `GROUND_REPEAT FAIL crops 9 max_peak 0.2563`). Metrics as in r4: edge sharpness = mean |dx| + |dy| of luminance (0-255); moss coverage = fraction of pixels with G - R > 8; new this round: band-pass contrast ratio after/before (std of a Gaussian difference-of-scales band, five octaves 0.7-22 px) to tell blur from blending, and an offline NumPy port of the `MF_GN_HexTile` HLSL on the `forest_ground_04` sources (scratch, not committed) to check what the function itself does to the slabs.

Tally: PASS 7 (A1, A2, 01, ctx-overview-south, ctx-overview-east, A1-sun20, A1-margin-sun20). FAIL 8 (A3, 04-forest-floor, 04-forest-floor-sun20, ctx-ground-1, ctx-ground-2, ctx-ground-3, ctx-ground-4, ctx-ground-4-sun20).

Verdict: NOT PASSED. The repeat gate fails on the committed crops (A3 cobble 0.256), and the forest floor is worse than in r4, not better: 04 [600,450,400,250] sharpness 16.95 before, 10.8 in r4, 9.83 now; `NEG_ANGLE` changed nothing measurable, the r4 bisect it was conditioned on (layer A at `rot 0`) was not run, and `HeightSharp` 96 adds straight polygon tone steps at 04 that r4 did not have. The r4 diagnosis (normal rotation sign) is refuted by this round's data: both signs give the same loss, the loss is scale-flat (a blend, not a blur) and uniform over the surface (not confined to the 25 % of hex overlap bands), and the offline port of the same HLSL keeps the slabs. The defect is engine-side and still unlocated; the fix list below is a one-round bisection with numeric decision rules, plus three fixes that stand on their own (A3 gate crop and the margin-edge period, the height term bound).

## A1-south-approach-30m — PASS

- Gate (after-measure): cobble [876,850,256,222] 0.124 at (12,-30), earth [705,730,110,96] 0.085 at (-34,10), grass [175,440,128,128] 0.164 at (17,-1). Before 0.225 / 0.544 / 0.244. 3x crops: cobble is sand with no motif; earth has no chevron lattice (before: diagonal ribbing wall to wall); grass is an even olive field with two soft blotches, nothing repeated.
- Open field [0,560,400,300] and [1250,520,400,300] at 2x: moss blotches of unequal size, no rows, no honeycomb; the before's rows of identical dashes in the far field are gone.
- Against `gameplay-hud.png` far range (compare sheet): same layout, road, tree line and wall as r4; the meadow reads as one continuous ground.

## A2-south-approach-100m — PASS

- Gate: cobble [885,790,110,256] 0.124, earth [815,600,64,256] 0.115 at (-6,39), grass [1230,800,256,256] 0.075. Before 0.583 / 0.665 / 0.342.
- Grass 3x: 1-3 m lobed patches, no two alike; the before's 14 identical diagonal dashes are gone. Earth 3x: fine mottle, no lattice (before: lattice). Cobble 3x: sand, no motif.
- Open field [0,250,500,350] and [1250,560,500,350] at 2x: patches from 1 m blobs to 15 m sheets; coverage 0.036 -> 0.038 and 0.242 -> 0.242.

## A3-south-approach-250m — FAIL

- Gate: cobble [915,600,70,215] **0.256 at (1,-23), FAIL**; earth [884,600,32,160] 0.139; grass [1075,520,190,190] 0.067. Before 0.667 / 0.840 / 0.433.
- The 0.256 is not the carriageway. Re-measured with the gate's own `peak()` on the same after-measure frame: [915,600,70,215] 0.256 (1,-23); [925,600,55,215] 0.075 (5,-55); [928,600,50,215] 0.079; [930,600,45,215] 0.082. The left 10 px of the committed rectangle hold the ragged margin edge (6x crop [895,600,110,215]: dark margin 895-920, carriageway 920-1000), and that edge repeats every 23 px along the road: 23 px at this range is ~3.5 m, which is `EdgeTileCm` 350: the margin edge is cut by `EdgeNoise`, a plain 3.5 m tile of `forest_ground_04_Displacement`, so the worn edge is the same shape every 3.5 m (9 copies inside the 215 px crop; before 0.667 at (0,-17) from the same edge plus the tiled cobble). That is an in-domain repeat on the earth margin and it is what fails the gate.
- Grass 3x: irregular blobs, none alike. Open field [0,300,500,400] and [1300,650,500,400] at 2x: no rows, no stripes; moss 0.200 -> 0.207 and 0.262 -> 0.265 (like for like).
- Fix type: CODE (EdgeNoise warp, difference 4) and CODE/data (crop on the carriageway only, difference 5).

## 01-forest-path-town — PASS

Cobble 3x at [300,800,500,280]: continuous courses, no doubled stone, no seam, no direction change (layer R unchanged since r2; sharpness 5.26 -> 4.89, band ratio 0.9-0.95). Full frame: same town, road, trees as the before.

## 04-forest-floor — FAIL

- What's wrong (most visible): the whole forest floor is a low-contrast blend. 3x at [600,450,400,250]: before 12 flat slabs 30-120 px with a crisp edge and a dark underside; after the same area is pale overlapping ovals with no edge and no underside. Sharpness 16.95 -> 9.83 (r4 acceptance >= 15; r4 after was 10.8), luminance std 27.4 -> 14.5, p5-p95 77-170 -> 104-151 at the same mean (140/123/60 -> 144/127/62). Near field [0,900,400,180]: 10.65 -> 6.19. Gravel [1200,700,300,150]: 14.79 -> 10.10 (r4's "fine grain at full strength" does not hold).
- Band-pass contrast ratio after/before in [600,450,400,250]: 0.56 (0.7-1.4 px), 0.56, 0.53, 0.45, 0.44 (11-22 px). A coarser mip would lose the fine octaves first; this loses every octave by the same ~0.5. That is the signature of each pixel being an average of ~3 uncorrelated samples (1/sqrt(3) = 0.58), everywhere in the window, not only in the overlap bands (25.5 % of the area has max weight < 0.9 at `pow 7`, 48 % < 0.99; 2 M-sample check of the function's barycentrics).
- `NEG_ANGLE` did nothing: r4 after 10.8 (+a), r5 after 9.83 (-a). On paper the original `+a` was the consistent one: `uv = mul(p - cen, R)` is a row-vector product, so the texture is sampled at `R^T p` and appears rotated by `+a` in the world; a texture normal then turns by `+a`, which is `mul(R, n.xy)`. `-a` is right only for a mirrored tangent frame, and the frame-wide loss did not move with the sign, so the sign is not the cause and r5's setting is the inconsistent one.
- Offline port of the HLSL (same lattice, weights, `L` term, rotation, `HeightSharp`, lit with a fixed light) on `forest_ground_04`: lit sharpness plain 36.8; hex `Rot 1 HeightSharp 0` 26.2; `+a` vs `-a` 26.2 / 29.5; `HeightSharp 16` 26.2; `HeightSharp 96` 26.2; bounded height term 26.2; `Rot 0` 25.9. The function as written costs ~29 % (the bands) and keeps the slabs; the engine loses 42 % and keeps none. The remaining difference between the port and the engine is what the engine does that the port does not: `SampleGrad` with `ddx/ddy` of a world-space UV, the shared `Wrap_WorldGroupSettings` sampler, the material's normal path after the function, and the renderer. None of this was bisected; r4 fix 1 said to run layer A at `rot 0` if 15 was not reached, and the README does not record it.
- Second item, new since r4: straight polygon tone steps on layer A. 4x at [1300,300,300,200] after: a lighter polygon with a straight right edge at x 1160-1340 (crop px, 4x), y 230-350, and a straight diagonal edge from (1000,560) to (1200,480) at the lower left; 3x at [100,800,400,250]: a straight tone edge near x 285 (as r4) and a second near x 1700-1800 (crop px). Before: none. These are hex borders where `exp2(96 x (Hv - max Hv))` flips the winner between two taps: with the map's local step 0.03-0.05 the factor is 7-28x, and in the 48 % of the surface where a second tap still holds 1-10 % of the weight, a taller texel in that tap wins outright, so the switch is a hard line along the lattice. r4 fix 2 said "if a seam appears, 64"; the seam appeared and the fix is a bound, not a smaller number (difference 2).
- 2x at [1100,300,500,300]: layer A (the r4 lichen smear is gone, moss 0.000), met; the same window shows the lighter polygon above.
- Against `Greenlands-Forest-Look-v1.webp` (hard-edged loose stones, litter, moss on rock): the after is further from the concept than the before.
- Fix type: CODE. Fixes 1, 2.

## 04-forest-floor-sun20 — FAIL

- The sun view is real (twin difference to `after/04-forest-floor.jpg` 65.8; before|after sun frames differ by 21.0, consistent with the default pair). Lit strip 2x at [1400,200,520,380] and [1000,100,900,500]: the before shows slabs with raking light and shadowed undersides; the after shows a uniformly crumpled fine relief with no slab anywhere, sharpness 18.40 -> 14.06 and 22.98 -> 15.77, luminance std 43.6 -> 34.6. Under raking light the relief that is there is crisp, so the normal is not blurred: it is relief that does not belong to the pale slab shapes in the albedo. 4x at [1300,300,300,200]: 25.37 -> 16.75, and the same lighter polygon as the default view sits in the lit strip, now as a brightness step across a hex border on layer A (r4 fix-5 condition: none allowed). Shaded floor 3x [600,450,400,250]: 7.73 -> 5.86, pebbles without undersides.
- Fix type: CODE. Fixes 1, 2.

## Context views

### ctx-overview-south, ctx-overview-east — PASS
Fields at 300 m: even mottle, no rows, no lattice, no honeycomb; moss sheets 5-40 m; sharpness and coverage within 2 % of the before. The world reads the same as A1-A3 from above.

### ctx-ground-1 — FAIL
- 4x at [500,900,300,150]: before 20+ discrete pebbles and slabs with a dark underside; after 7 soft pale blobs, none with an underside (r4 condition >= 15 with an edge). Sharpness 9.73 -> 7.54 (r4: 8.9). 3x at [300,760,600,220]: 8.67 -> 7.39, band ratio 0.84-0.97 (milder here, the floor is 3-10 m away and already at a coarse mip). Mid field [900,700,500,250]: 7.61 -> 6.87.
- Fix type: CODE. Fix 1.

### ctx-ground-2 — FAIL
- Moss coverage in the lower band [0,500,1920,580]: before 0.053, after 0.062 (r4 condition <= 0.10: met; r4 after was 0.153). The foreground lichen carpet at [1100,780,400,200] is one patch now (0.113 in that window) with the mossy_rock crack dashes still at one angle inside it (texture choice, carried forward).
- New: the near field is layer A and it is smeared like 04. 4x at [200,900,300,150]: before round pebbles with an edge; after diagonal smears, sharpness 8.74 -> 7.34, std 10.7 -> 9.5. Same defect as ctx-ground-1/3.
- Fix type: CODE. Fix 1.

### ctx-ground-3 — FAIL
- Moss in [0,500,1920,580] 0.011 -> 0.031: met.
- 4x at [100,850,400,200]: sharpness 9.15 -> 7.30 (r4 condition >= 8.5); the after is diagonal streaks at one angle (upper-left to lower-right) with no pebble outline; [600,900,300,150]: 10.48 -> 7.73, same streaks. The streak direction is constant across the frame, and a different constant in ctx-ground-4 (left-right), i.e. it follows the view, not the hex cell.
- Fix type: CODE. Fix 1.

### ctx-ground-4 — FAIL
- Cobble 1-5 m, 3x at [1000,900,400,180]: continuous courses, no ghosting, no seams (4.93 -> 4.39, mean colour identical).
- Mud (layer T, `Rot 1.0`) is smeared, not merely flatter: 4x at [100,850,300,150] the before is clumpy granular mud, the after is the same mud dragged into left-right streaks, sharpness 11.15 -> 8.49; [500,750,300,150] 8.76 -> 6.48; the 2x window [0,700,900,380] 8.95 -> 6.94 (-22 %), band ratio 0.80-0.84 flat across octaves. r4 passed this view "under fix 1"; fix 1 did not deliver and the smear is now documented at 4x, so it fails on its own.
- Fix type: CODE. Fix 1.

## Sun-20 views (hex-border brightness steps)

All four sun views move the sun (twins differ by 65.8 / 40.8 / 44.6 against the > 8 condition; `A1-margin-sun20` before|after 6.7 with no default twin).

### A1-sun20 — PASS
Field, margin and road at 2x [700,500,500,300] and 4x [640,650,220,220]: no polygonal step, no seam, no cell edge on R, T or the grass; sharpness 8.27 -> 7.71.

### A1-margin-sun20 — PASS
Cobble and earth margin at 1-5 m, 3x at [500,820,400,250] and the full frame: courses continuous through every hex border, no mortar-line seam, no tone step (6.10 -> 5.37). The earth margin (T) is flatter than the before (full-frame 6.02 -> 5.63) but shows no cell, step or streak at this light; judged under fix 1 and not failed here.

### ctx-ground-4-sun20 — FAIL
Cobble 2x at [900,700,600,300]: no steps (5.03 -> 4.62). Mud [0,700,900,380]: sharpness 10.49 -> 8.12 (r4 condition >= 10), band ratio 0.80-0.84 at every octave, the same left-right drag as the default twin. Fix 1.

## Measurement credibility

1. The gate reproduces (`max_peak 0.2563`, A3 cobble FAIL). The failing peak is the margin edge inside the crop (A3 section): [925,600,55,215] on the same frame gives 0.075. The committed crop must move off the edge and the edge's own 3.5 m period must go (differences 4, 5).
2. Band-pass ratios (new): 04 slab window 0.44-0.56 flat over five octaves; mud 0.80-0.84 flat; cobble 0.88-0.96. A blur would fall towards the fine octaves; these do not. The loss on `Rot 1.0` layers is a blend or an unrelated normal, not a mip.
3. The offline port of `hex_tile.py`'s HLSL (NumPy, bilinear taps, same lattice/hash/rotation/weights/height term) loses 29 % of lit sharpness at `Rot 1` and keeps every slab; `HeightSharp` 0/16/96 and the bounded form differ by < 0.1; `+a` vs `-a` differ by 3. The engine loses 42 % and keeps no slab. The cause is therefore in what the engine adds (gradients, sampler, normal path, renderer), which is why fix 1 is a bisection and not another parameter.
4. Sun views: credible (item above). The polygon step at 04 is on layer A in both the default and the sun frame at the same place, so it is a material feature, not a lighting artefact.
5. Perf: `perf-01.json` median 7.33 -> 7.58 ms (+0.25, p95 9.67 -> 9.04), inside the +1 ms budget; `ProfileGPU` BasePass 0.84 -> 1.27 ms (+0.43), the rest of the 0.74 ms frame difference is a 0.11 ms Lumen card burst and 0.23 ms GBufferClear noise. The README row r4 asked for is in. Met.
6. r4 bisect: not run. README Round 5 item 1 records the `NEG_ANGLE` transposition only; the conditional `rot=0.0` capture that r4 fix 1 required when 04 stayed below 15 is absent, so round 5 spent its capture on a sign that the maths already said was the consistent one before the change.

## Differences, most visible first

1. Relief and albedo contrast lost on every `Rot 1.0` hex layer, uniformly over the surface (04, 04-sun20, ctx-ground-1/2/3, mud at ctx-ground-4 and its sun twin).
   What's wrong: 04 [600,450,400,250] sharpness 9.83 against >= 15 (before 16.95), band ratio ~0.5 at every octave; ctx-ground-1 7 blobs against >= 15 pebbles; ctx-ground-3 7.30 against >= 8.5 with one-angle streaks; mud 8.12 against >= 10 with left-right drag; the sun view's lit relief does not belong to the albedo's slab shapes. Neither normal sign changes it; the offline port of the function does not reproduce it.
   Fix type: CODE. Exact: a four-variant bisection, one capture of `04-forest-floor` and `04-forest-floor-sun20` each (`GFX_ONLY=04-forest-floor,04-forest-floor-sun20`), sharpness of after/04 [600,450,400,250] recorded per variant in the README. In `GameClient/ArtSource/Greenlands/NatureFamilies/import_ground.py` add at module level
   ```python
   BISECT = ''  # r5 review fix 1: '' | 'rot0' | 'plainN' | 'cell4' | 'nograd'; one 04 capture per value, numbers in the README, then back to ''
   ```
   and change the layer A call (line 136) to
   ```python
   fa, na, ra, _ = layer('A', T + 'forest_ground_04_Diffuse', T + 'forest_ground_04_nor_gl', T + 'forest_ground_04_Rough', 200, (0.50, 0.66, 0.34), 0, hex=True,
                         rot=0.0 if BISECT == 'rot0' else 1.0, cell=4.0 if BISECT == 'cell4' else 1.0,
                         height=T + 'forest_ground_04_Displacement', hsharp=0.0 if BISECT else 96.0)
   ```
   and in `hexlayer` (line 123) replace `pick('N', y + 250, **rgb)` with
   ```python
   (g.smp('Normal' + p, tex(nor), S.SAMPLERTYPE_NORMAL, uv, -1100, y + 250) if BISECT == 'plainN' and p == 'A' else pick('N', y + 250, **rgb))
   ```
   For `'nograd'`, in `hex_tile.py` build the Custom node with `HLSL.replace('float2 rx = mul(gx, R), ry = mul(gy, R);', 'float2 rx = gx, ry = gy;')` when `os.environ.get('J2PRE_NOGRAD')` is set (the editor must be launched with it; or edit the string for that one run). Also capture the GBuffer for the baseline: in `capture_game.py` add `'r.BufferVisualizationDumpFrames 1'` to the console list when `os.environ.get('GFX_DUMP')`, run `before` and `after` once with `GFX_ONLY=04-forest-floor GFX_DUMP=1`, and keep the `_BaseColor` and `_WorldNormal` dumps (they land beside the shot or under `Saved/Screenshots/`; move them to `<tag>/vis/`). Decision rules, all on [600,450,400,250]: (i) BaseColor p5-p95 spread after within 15 % of before but WorldNormal sharpness < 0.7 x before: the loss is in the normal path only; `plainN` must then reach >= 15 and the fix is in the function's `N` handling (sampler/unpack/rotation) or the material's normal lerps; (ii) BaseColor spread also < 0.7 x before: the taps or the blend; then `rot0` >= 15 means the rotation path (gradients or hash) and `nograd` decides whether it is the gradient rotation; `cell4` >= 15 with visible 8 m polygons means the blend bands and the fix is `cell` 2.0-3.0 for A/T/S/B plus difference 2; none >= 15 means the sampling itself (`SampleGrad`/shared sampler) and the fix is a plain `Sample` per tap with the same `uv` as a further one-capture check. Acceptance for the round: the variant table in the README with the five numbers, the chosen fix applied with `BISECT` back to `''`, and after/04 [600,450,400,250] >= 15 with slabs showing a dark underside at 3x; ctx-ground-1 >= 15 pebbles with an edge; ctx-ground-3 [100,850,400,200] >= 8.5 with no one-angle streaks; ctx-ground-4-sun20 mud [0,700,900,380] >= 10 and no left-right drag at 4x; `NEG_ANGLE` set to whatever the bisect proves and the comment on line 12 rewritten to say why (the maths favours `False`).

2. Hard polygon tone steps along hex borders on layer A from the unbounded height term (04 [1300,300,300,200], [100,800,400,250]; 04-sun20 lit strip).
   What's wrong: `exp2(96 x (Hv - max Hv))` is 7-28x for the map's 0.03-0.05 local step and 5800x for its 0.13 global range, so any tap holding >= 1 % of the weight wins wherever its texel is taller; the switch is a straight line on the lattice.
   Fix type: CODE. Exact: `hex_tile.py` line 42, replace
   ```hlsl
   float3 W = lerp(1.0, L, 0.6) * pow(w, 7.0) * exp2(HeightSharp * (Hv - max(Hv.x, max(Hv.y, Hv.z)))); W /= (W.x + W.y + W.z);   // max-subtracted so HeightSharp up to 160 stays in float range
   ```
   with
   ```hlsl
   float3 W = lerp(1.0, L, 0.6) * pow(w, 7.0) * exp2(max(HeightSharp * (Hv - max(Hv.x, max(Hv.y, Hv.z))), -3.0)); W /= (W.x + W.y + W.z);   // height tie-break capped at 8x: decides a 50/50 band, cannot flip a 1 % ghost inside a cell (r5 review: unbounded gave straight tone steps along hex borders)
   ```
   `HeightSharpA` stays 96 (3 octaves reached at a 0.03 step; bounds unchanged). Acceptance: no straight tone edge at 04 in [1300,300,300,200], [100,800,400,250] or the sun-20 lit strip at 4x; no doubled stone in the bands at 3x once fix 1 has restored the slabs.

3. (Shared with A3) The earth margin's worn edge repeats every 3.5 m: `EdgeNoise` is a plain tile.
   What's wrong: A3 cobble crop peak 0.256 at (1,-23) from the margin edge inside the crop; 9 copies of the same edge shape along 215 px; the same period runs along every road and trail edge in A2/A3 (`EdgeTileCm` 350).
   Fix type: CODE. Exact: `import_ground.py` line 202, replace
   ```python
   edge = g.mask(g.smp('EdgeNoise', tex(T + 'forest_ground_04_Displacement'), S.SAMPLERTYPE_MASKS, g.uv(xy, g.sc('EdgeTileCm', 350, -1800, 9300), -1650, 9300), -1100, 9300), True, False, False, -800, 9300)
   ```
   with
   ```python
   euv = g.op(u.MaterialExpressionAdd, g.uv(xy, g.sc('EdgeTileCm', 350, -1800, 9300), -1650, 9300), g.mul(warp, g.sc('EdgeWarp', 2.5, -1500, 9200), -1400, 9250), -1250, 9300)  # r5 review fix 3: the same two Simplex warps as the Mask, x2.5 (+-0.75 tile = +-2.6 m) so the worn edge never repeats on the 3.5 m tile
   edge = g.mask(g.smp('EdgeNoise', tex(T + 'forest_ground_04_Displacement'), S.SAMPLERTYPE_MASKS, euv, -1100, 9300), True, False, False, -800, 9300)
   ```
   (`warp` is the `MaskWarp` product defined at line 149; `EdgeWarp` bounds 0-4.) Acceptance: A3 [915,600,70,215] on the after-measure frame <= 0.15 even before the crop moves; the road edge at A2 2x [815,600,64,256] shows no two identical bites.

4. A3 cobble gate crop holds the margin edge (measurement item 1).
   Fix type: CODE (data). Exact: `GameClient/ArtSource/Greenlands/silverbrook-reference-bookmarks.json`, `A3-south-approach-250m` `repeat_crops` `cobble` [915,600,70,215] -> [925,600,55,215] (carriageway only; 0.075 on the r5 frame). Re-run the gate on `before-measure/` and `after-measure/`; README table.

5. `NEG_ANGLE` is now the inconsistent sign with no evidence for it (04 section).
   Fix type: CODE. Exact: settled by fix 1's decision rules; the default to return to if the bisect does not implicate the normal path is `NEG_ANGLE = False` with the comment `# the uv rotation is a row-vector product (R^T p), so the texture turns by +a and the normal by mul(R, n.xy); True only if the mesh tangent frame is shown mirrored (r5 review)`.

## Fix list (job domain, deduplicated)

1. Engineer — bisect the `Rot 1.0` relief loss: `BISECT` constant in `import_ground.py` (`rot0`, `plainN`, `cell4`, `nograd`), GBuffer dumps (`r.BufferVisualizationDumpFrames 1` under `GFX_DUMP` in `capture_game.py`) for before/after at 04, five sharpness numbers in the README, fix applied per the decision rules, `BISECT` back to `''` (difference 1). This is the gate for the round; nothing else is worth capturing until 04 reads >= 15.
2. Engineer — bound the height term: `exp2(max(HeightSharp * (Hv - max(...)), -3.0))` in `hex_tile.py` line 42 (difference 2).
3. Engineer — domain-warp `EdgeNoise` with the Mask's `warp` x `EdgeWarp` 2.5, `import_ground.py` line 202 (difference 3).
4. Codex — A3 `cobble` crop -> [925,600,55,215]; gate re-run on both measure folders; README table (difference 4).
5. Engineer — `NEG_ANGLE` set from the bisect, default `False`, comment rewritten (difference 5).
6. Codex — re-run `import_ground.py`; recapture `after/` (all views, the four sun views included) and `after-measure/`; re-run `perf_gpu.py` and the 500-frame timing; rebuild pairs; README Round 6 with the bisect table; fresh review. Pass condition: nine crops <= 0.25 with no lag at 1.2-1.5 m ground and no striped or same-shaped patch in any A-view crop; after/04 [600,450,400,250] sharpness >= 15 with slabs that have a dark underside at 3x, band ratio after/before >= 0.8 at every octave, no straight tone edge in [1300,300,300,200] or [100,800,400,250]; 04-sun20 lit strip [1400,200,520,380] >= 17 with slab relief that matches the albedo and no brightness step along a hex border; ctx-ground-1 [500,900,300,150] >= 15 pebbles with an edge; ctx-ground-2 [200,900,300,150] and ctx-ground-3 [100,850,400,200] >= 8.5, no one-angle streaks; ctx-ground-4 mud [100,850,300,150] >= 10 and ctx-ground-4-sun20 [0,700,900,380] >= 10, no directional drag at 4x; moss coverage ctx-ground-2 and ctx-ground-3 lower band <= 0.10; sun views differ from their twins by > 8; perf median at 01 within +1 ms of the before run and BasePass within +0.6 ms.

## r4 fix items: status

- r4 fix 1 (`NEG_ANGLE True`; bisect with `rot=0.0` if 04 < 15): the flag was set, with a stated deviation in how (normal-only transposition; correct reading of the flag's intent). 04 reached 9.83, not 15, and the required bisect was not run. Not met.
- r4 fix 2 (max-subtracted `exp2`, `hsharp` 96): implemented as written. It introduced straight polygon tone steps at 04 (the "if a seam appears" branch); the bands it was meant to clean cannot be judged while the slabs are missing. Not met; corrected by fix 2.
- r4 fix 3 (`MaskNoise` 0.02): implemented; ctx-ground-2 0.062, ctx-ground-3 0.031, 04 upper middle is layer A. Met.
- r4 fix 4 (A3 cobble crop [915,600,70,215]): implemented; the new rectangle still holds the margin edge and the gate fails on it. Not met; corrected by fixes 3 and 4.
- r4 fix 5 (recapture, perf row, pairs, pass condition): captures, perf row and pairs done. Pass condition not met on the gate, 04, ctx-ground-1, ctx-ground-3, the mud, and the hex-border step.

## Carried forward (not this job)

- Aerial colour blend 25-60 m and macro luminance variation per layer (J2).
- Low-poly lime-green blob trees and bushes at A1-A3 and the overviews; identical house rows; bare plain with no fields, hedges or tracks between town and river.
- White blob mountains without rock or snow detail (A1, 01, ctx-ground-1/2/4).
- Cobbled carriageway where `gameplay-hud.png` far range shows a dirt track; beige cobble vs the concept's grey-brown stone.
- Bookmark 04 vs `Greenlands-Forest-Look-v1.webp`: no leaf litter, needles, loose stones or undergrowth variety; hard 1 px moss-patch edges at 1 m from `Low 0.4 / InvRange 32` (present in the before).
- Hard-edged river bank and flat water colour (A2, A3).
- mossy_rock crack dashes inside the moss patches (ctx-ground-2) read as rock slab, not moss: a texture choice for layer B.
- If the bisect shows the mesh tangent frame mirrored, the plain normals of the before and of bedrock C are y-flipped on this mesh too: for J2 to settle with the mesh's tangent basis.
