# GFX-02 J2-pre hex-tiled ground: independent review, round 4 (Fable, 2026-10-22)

Judged against `docs/decisions/GFX-02-photoreal-ground-pipeline.md` section 2a (Interim J2-pre, Acceptance) and section 7 row J2-pre, and against the REVIEW-r3.md fix-6 pass condition. Domain: repeating motifs on cobble, earth or grass at A1-A3, plus anything the hex-tiling or its fixes introduced (cells, seams, brightness steps, double exposure, layer-mix regressions). Every frame in `before/`, `after/`, `before-measure/`, `after-measure/` and the six compare sheets was read; the nine gate rectangles were cut before|after at 3x from the measure frames, open-field windows at 2x, ground-level windows at 3-4x, sun views as half-frames and 2-3x crops, plus a 6 px Gaussian high-pass of both 04 frames. `Measure-GroundRepeat.py` was re-run on `after-measure/` (identical: `GROUND_REPEAT PASS crops 9 max_peak 0.2265`). Moss coverage = fraction of pixels with G - R > 8 in the named window, same classifier on before and after. Edge sharpness = mean |horizontal gradient| + |vertical gradient| of luminance in the window (0-255 scale), before vs after.

Tally: PASS 10 (A1, A2, A3, 01, ctx-overview-south, ctx-overview-east, ctx-ground-4, A1-sun20, A1-margin-sun20, ctx-ground-4-sun20). FAIL 5 (04-forest-floor, 04-forest-floor-sun20, ctx-ground-1, ctx-ground-2, ctx-ground-3).

Verdict: NOT PASSED. The A-view repeat (the job's headline) holds for the second round running and the sun views are now real. The forest floor and the ground-level plain still fail: layer A is back at 04 but every slab and pebble on it is soft, without its dark underside, in the whole frame, not only in the hex overlap bands; `HeightSharp` 16 did nothing because the Displacement map's stone-to-soil step is 0.03-0.05, not the 0.25 the r3 fix assumed; and `MaskNoise` 0.05 still leaves the ctx-ground-2 foreground at 0.153 moss coverage against the 0.10 limit.

## A1-south-approach-30m — PASS

- Gate (after-measure): cobble [876,850,256,222] 0.123 at (12,-30), earth [705,730,110,96] 0.085, grass [175,440,128,128] 0.148 at (15,-1). The grass lag 15 px is 0.5 m on the ground at 30 m and the crop at 3x is an even olive field with two soft blotches; nothing in it repeats. Before on the same crops: 0.225 / 0.544 / 0.244.
- 3x crops: cobble is sand with no motif; earth has no chevron ribbing (before: diagonal lattice); grass has no cross-shaped tufts or rows.
- Open field [0,560,400,300] and [1250,520,400,300] at 2x: moss blotches of unequal size and outline, no stripes, no honeycomb. The before shows the 9 m stamp as rows of identical dashes in the far field; the after does not.
- Margin at the road edge: no cells, no seams, no tone step.
- Against `gameplay-hud.png` far range: same layout, road, tree line and town wall as the r3 after; the meadow reads as continuous ground.

## A2-south-approach-100m — PASS

- Gate: cobble [885,790,110,256] 0.119, earth [815,600,64,256] 0.117, grass [1230,800,256,256] 0.090 at (16,9). Before: 0.583 / 0.665 / 0.342.
- Grass 3x: 1-3 m moss patches with lobed, unequal outlines; no two of the same shape; the before's 14 identical diagonal dashes are gone. Earth 3x: fine mottle, no lattice. Cobble 3x: sand, no motif, no canopy in the crop any more.
- Open field [0,250,500,350] and [1250,560,500,350] at 2x: patches from 1 m blobs to 15 m sheets; coverage in [0,250,500,350] before 0.036, after 0.041.

## A3-south-approach-250m — PASS

- Gate: cobble [915,600,70,256] 0.227 at (1,-23), earth [884,600,32,160] 0.137, grass [1075,520,190,190] 0.066. Before: 0.641 / 0.840 / 0.433. The cobble 0.227 is the band edge of a 70 px strip plus the canopy that is still inside its bottom 40 px (measurement item 4); the road reads as one tone.
- Grass 3x: the before's 22 identical diagonal dashes are gone; the after shows irregular blobs, none alike.
- Open field [0,300,500,400] and [1300,650,500,400] at 2x: no rows, no stripes, no same-shaped patch. Moss coverage in [0,300,500,400]: before 0.200, after 0.222 with this classifier. The r3 limit "<= 0.13" was set against an r3 before figure of 0.090 that does not reproduce on the unchanged before frame; on the like-for-like figures the after is +11 %, and by eye the plain has the same density of pale-green sheets as the before with larger size variation. Not a fail.

## 01-forest-path-town — PASS

Cobble 3x at [300,800,500,280]: continuous courses, no doubled stones, no seams, no direction change (layer R untouched since r2). Earth margin and grass beyond: no cells, no repeat.

## 04-forest-floor — FAIL

- What's wrong (most visible): the forest floor is layer A again, but every slab is soft. 3x at [600,450,400,250]: the before shows 12 flat slabs 30-120 px with a crisp edge and a dark underside; the after shows the same slab shapes as pale smudges with no edge and no underside, under a faint green veil. Edge sharpness in that window 16.8 before, 10.8 after (-36 %); luminance std 27.0 -> 17.1. The 6 px high-pass of the whole after frame shows the fine gravel grain everywhere at full strength but not one crisp slab anywhere in the frame; the before's high-pass is slab edges wall to wall. The loss is frame-wide, not confined to the ~24 % of the area that is a hex overlap band (offline port of the function's weights on this tile: max weight < 0.9 on 24 % of pixels at `pow 7`).
- The same relief loss is on layer T (mud) at ctx-ground-4-sun20 [0,700,900,380]: 10.5 -> 8.8 (-16 %), while cobble R at A1-margin-sun20 [500,820,400,250] loses 12 % (6.07 -> 5.36) and keeps crisp joints. R is the only hex layer with `Rot 0.0`; A and T have `Rot 1.0`. The relief loss scales with the rotation, so it is the normal rotation, not the band blend. The slab crispness at 04 comes from the normal map, not the albedo (an offline blend of the Diffuse alone looks the same at `HeightSharp` 0 and 128).
- `hex_tile.py` line 36 rotates the tangent normal by `+a` (`n.xy = mul(R, n.xy)`), which is right only when the mesh's tangent frame has the same handedness as the material's `WorldPosition.xy` UV. A terrain mesh imported with V flipped has its bitangent along world -y: the frame is mirrored and the correct normal rotation is `-a`. In a cell rotated by ~90 deg the relief is then lit from the side, at ~180 deg bumps become dents; across random cells the slabs lose their consistent underside and read as flat smudges, exactly the after. `NEG_ANGLE` (line 12) exists for this case and has never been tried.
- Second item: 3x at [100,800,400,250] (after) shows a faint straight tone edge near x 285, y 830-1000 that is not in the before: a hex border where two cells' mis-rotated normals meet. Fix 1 removes it.
- Third item: 2x at [1100,300,500,300]: the upper middle of the spot is still a lichen smear (layer B) with no stone edge, 60 x 40 cm; the before has slabs and gravel there. Coverage from `MaskNoise` 0.05 (fix 3).
- `HeightSharp` 16 (r3 fix 2) could not help: the Displacement map's local stone-to-soil step (p90 - p10 inside 12 cm windows of `forest_ground_04_Displacement.jpg`) is median 0.027, p90 0.051; `exp2(16 x 0.04)` = 1.6, not the 16x the r3 fix computed from an assumed 0.25. The band overlap is therefore still a plain 3-tap blend.
- Against `Greenlands-Forest-Look-v1.webp` (loose hard-edged stones, litter): the after is further from the concept than the before.
- Fix type: CODE. Fixes 1, 2, 3.

## 04-forest-floor-sun20 — FAIL

- The sun change is real this round (twin difference to `after/04-forest-floor.jpg` 55.9; before pair recaptured the same way, before|after sun frames differ by 17.6): the lit strip at 2x [1400,200,520,380] shows the same soft, edgeless stones (sharpness 18.3 before, 12.5 after, -32 %), and the shaded floor at 3x [600,450,400,250] shows pebbles without undersides. No polygonal brightness step is visible along a hex border at this light, but with the relief flattened the test cannot show one either. Fails with 04.

## Context views

### ctx-overview-south, ctx-overview-east — PASS
Fields at 300 m: even mottle, no rows, no lattice, no honeycomb; moss sheets vary from 5 to 40 m. The world reads the same between the overviews and A1-A3.

### ctx-ground-1 — FAIL
- 4x at [500,900,300,150]: before shows 20+ discrete pebbles and slabs with a dark underside; after shows 8 soft pale blobs and no underside anywhere (r3 fix-6 condition: >= 15 with an edge). Sharpness 9.7 -> 8.9. 3x at [300,760,600,220]: the same, plus the far field (5-15 m) where the before's slab rows are a smooth smear. Same cause as 04.
- Fix type: CODE. Fix 1 (and 2).

### ctx-ground-2 — FAIL
- Moss coverage in the lower band [0,500,1920,580]: before 0.053, after 0.153 (r3 limit <= 0.10; r3 after at `MaskNoise` 0.15 was 0.354). The foreground right [1100,780,400,200] at 3x is still a continuous lichen carpet with the mossy_rock crack lines as dark dashes at one angle (6 dashes, same angle, in a 4 x 2 m window); 4x at [200,900,300,150] has the carpet entering from the lower right. Coverage is linear in `MaskNoise` on these three points (0.053 + 2.0 x MaskNoise), so 0.10 needs <= 0.023.
- No hex polygons, no straight patch edges.
- Fix type: CODE. Fix 3.

### ctx-ground-3 — FAIL
- Moss coverage in [0,500,1920,580]: before 0.011, after 0.047: inside the limit; the r3 moss finding is fixed here.
- New: the near field is layer A and it is smeared. 4x at [100,850,400,200]: before shows round pebbles 5-15 px with an edge; after shows diagonal streaks all at one angle (upper-left to lower-right) and no pebble outline; sharpness 9.1 -> 6.7 (-27 %). Mis-rotated relief lit from a consistent wrong direction inside one cell reads as directional streaking. Same cause as 04.
- Fix type: CODE. Fix 1.

### ctx-ground-4 — PASS
Cobble 1-5 m, 3x at [1000,900,400,180]: continuous courses, no ghosting, no seams. The mud margin (T) is flatter than the before (sharpness 8.9 -> 7.2 in [0,700,900,380]) but still reads as mud with no cells or seams; judged under fix 1.

## Sun-20 views (hex-border brightness steps)

All four sun views now move the sun (`J2PRE_SUN` logs pitch -20.0; twins differ by 39.4 / 45.4 / 55.9 against the > 8 condition). The builder's deviation (pitch -20 kept instead of -8, because -8 gave 5/255 mean luminance at fixed exposure) is accepted: the shadows are long and the light grazes the ground, which is what the check needs.

### A1-sun20 — PASS
Cobble, margin and field at 2x [700,500,500,300] and 4x [640,650,220,220]: no polygonal brightness step, no seam, no cell edge on R, T or the grass.

### A1-margin-sun20 — PASS
Earth (T) and cobble (R) at 1-5 m, 3x at [500,820,400,250] and the left half-frame: no step along any border, no mortar-line seam, courses continuous through every hex border.

### ctx-ground-4-sun20 — PASS
Cobble 3x at [900,700,600,300] and the right half-frame: no steps. Mud (T) in the left half: no steps, no cells; relief 16 % flatter than the before (noted under 04, fix 1).

## Measurement credibility

1. The gate reproduces (`max_peak 0.2265`). All nine lags are below 1.2 m on the ground or on a featureless crop; the r3 pass condition on the A views holds again.
2. The r3 before figure for A3 moss coverage (0.090) does not reproduce: the unchanged `before/A3-south-approach-250m.png` gives 0.200 with the same classifier. The after is 0.222. The r3 "<= 0.13" limit is void; the like-for-like change is +11 %.
3. Sun views: credible now. The `before/` sun views were recaptured with the original material the same way (README); before|after sun frames differ by 2.0-4.6 (A1-sun20, A1-margin-sun20, ctx-ground-4-sun20) and 17.6 (04-sun20), consistent with the default-view before|after differences (3.4 at A1, 18.9 at 04).
4. Gate crops: A1 cobble, A2 cobble and A3 earth are clear of props now. A3 cobble [915,600,70,256] still has a canopy in its bottom ~40 px (3x crop, dark foliage both bottom corners).
5. Perf: `ProfileGPU` at 01 (2560x1440): BasePass 0.84 -> 1.29 ms (+0.45), frame 5.43 -> 6.76 of which 0.83 is a one-frame Lumen card-capture burst; the 500-frame median 7.55 ms is inside the "+1 ms of today" budget (before runs 7.65-8.20). The README says the profile is the round-3 material and that "the final material is re-timed below", but no such row exists. Tap count is unchanged by round 4, so the budget holds; the sentence or the row must go in.
6. `HeightSharp` sizing: measured on `GameClient/ArtSource/Greenlands/NatureFamilies/Sources/forest_ground_04/forest_ground_04_Displacement.jpg`: global p5-p95 0.329-0.459, std 0.041; local p90-p10 in 64 px windows median 0.027, p90 0.051.

## Differences, most visible first

1. Slabs, pebbles and mud relief flattened on every `Rot 1.0` hex layer (04, 04-sun20, ctx-ground-1, ctx-ground-3; T at ctx-ground-4 and its sun view).
   What's wrong: edge sharpness -36 % at 04 [600,450,400,250], -32 % in the 04-sun20 lit strip, -27 % at ctx-ground-3 [100,850,400,200], -16 % on mud at ctx-ground-4-sun20; pebble count at ctx-ground-1 [500,900,300,150] 20+ -> 8, none with an underside; a straight tone edge at 04 near x 285, y 830-1000. Cobble (`Rot 0.0`) keeps its joints. The loss is frame-wide, so it is the normal rotation sign, not the 24 % overlap band.
   Fix type: CODE. Exact: `GameClient/ArtSource/Greenlands/NatureFamilies/hex_tile.py` line 12, replace
   ```python
   NEG_ANGLE = False  # flip to True only if the hex-border lighting self-check (bookmark 04) shows a step: negates the rotation sign on the normal
   ```
   with
   ```python
   NEG_ANGLE = True  # r4 review: the terrain mesh's tangent frame is mirrored against the WorldPosition.xy UV (V flipped on import), so the tangent normal turns by -a; +a flattened every Rot 1.0 layer (04, ctx-ground-1/3, mud)
   ```
   Rebuild (`py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/NatureFamilies/import_ground.py`; it calls `hex_tile.build()`), then `GFX_ONLY=04-forest-floor,04-forest-floor-sun20,ctx-ground-1,ctx-ground-3,ctx-ground-4-sun20` capture. Acceptance: sharpness in after/04 [600,450,400,250] >= 15 (before 16.8) with slabs showing a dark underside at 3x; ctx-ground-1 4x [500,900,300,150] >= 15 pebbles with an edge; ctx-ground-3 [100,850,400,200] sharpness >= 8.5 and round pebbles, no one-angle streaks; ctx-ground-4-sun20 mud [0,700,900,380] >= 10; no straight tone edge at 04. If `NEG_ANGLE True` does not reach 15 at 04, bisect with one capture of layer A at `rot=0.0` (`import_ground.py` line 136: add `rot=0.0` to the `layer('A', ...)` call): crisp slabs at `rot 0` confirm the rotation path and the sign must then be set per mesh from its tangent basis; soft slabs at `rot 0` mean the loss is elsewhere in the function and the H/HeightSharp path is next (`hsharp=0.0`, `height=None`).

2. `HeightSharp` 16 is a no-op on this Displacement map (04, ctx-ground-1 overlap bands).
   What's wrong: the local stone-to-soil height step is 0.03-0.05 (item 6), so `exp2(16 x H)` gives a 1.6x ratio in the bands; the overlap bands (24 % of the area) are still a plain 3-tap blend of stones from three offsets. The r3 instruction assumed 0.25.
   Fix type: CODE. Exact: `hex_tile.py` line 42, replace
   ```hlsl
   float3 W = lerp(1.0, L, 0.6) * pow(w, 7.0) * exp2(HeightSharp * Hv); W /= (W.x + W.y + W.z);
   ```
   with
   ```hlsl
   float3 W = lerp(1.0, L, 0.6) * pow(w, 7.0) * exp2(HeightSharp * (Hv - max(Hv.x, max(Hv.y, Hv.z)))); W /= (W.x + W.y + W.z);   // max-subtracted so HeightSharp up to 160 stays in float range
   ```
   and `import_ground.py` line 137 `hsharp=16.0` -> `hsharp=96.0` (bounds 0-160; 96 x 0.04 = 3.8 octaves, a 14x ratio for the median step, 28x for the p90 step), comment `(HeightSharp 0-24)` -> `(HeightSharp 0-160; the map's stone/soil step is 0.03-0.05)`. Acceptance (after fix 1 passes): 04 3x at [600,450,400,250] and ctx-ground-1 4x show no doubled or ghosted stone in any overlap band; no hard seam along a hex border (if a seam appears, 64).

3. Layer B still over-covers the ground-level plain and part of 04 (ctx-ground-2; 04 upper middle).
   What's wrong: ctx-ground-2 lower band coverage 0.153 against <= 0.10 (before 0.053); 04 2x [1100,300,500,300] has a 60 x 40 cm lichen smear where the before has slabs.
   Fix type: CODE. Exact: `import_ground.py` line 154, replace
   ```python
   mkv = g.op(u.MaterialExpressionAdd, mk, g.mul(nzA, g.sc('MaskNoise', 0.05, -900, 2750), -850, 2700), -700, 2600)
   ```
   with
   ```python
   mkv = g.op(u.MaterialExpressionAdd, mk, g.mul(nzA, g.sc('MaskNoise', 0.02, -900, 2750), -850, 2700), -700, 2600)  # bounds 0-0.03: coverage at ctx-ground-2 is 0.053 + 2.0 x MaskNoise (r3/r4 measured)
   ```
   and line 153's `(MaskNoise 0-0.08; ...)` -> `(MaskNoise 0-0.03; ...)`. Acceptance: ctx-ground-2 [0,500,1920,580] <= 0.10; ctx-ground-3 <= 0.10; 04 2x [1100,300,500,300] shows layer A slabs; A2/A3 open-field patches still vary in size.

4. A3 cobble gate crop still holds a canopy (measurement item 4).
   Fix type: CODE (data). Exact: `GameClient/ArtSource/Greenlands/silverbrook-reference-bookmarks.json` `repeat_crops` for `A3-south-approach-250m`: `cobble` [915,600,70,256] -> [915,600,70,215]. Re-run the gate on `before-measure/` and `after-measure/`; update the README table.

5. README perf sentence without the row (measurement item 5).
   Fix type: CODE (doc). Exact: `docs/evidence/graphics/gfx02-j2pre-hextile/README.md` Round 4, either re-run `perf_gpu.py` on the final material and add its BasePass / frame numbers as a third column of the per-pass table, or delete "(the final material is re-timed below)".

## Fix list (job domain, deduplicated)

1. Engineer — `NEG_ANGLE = True` in `hex_tile.py` line 12; rebuild; bisect with `rot=0.0` on A if 04 sharpness stays < 15 (difference 1).
2. Engineer — max-subtracted `exp2` in `hex_tile.py` line 42; `hsharp` 16.0 -> 96.0 on layer A, bounds 0-160 (difference 2).
3. Engineer — `MaskNoise` 0.05 -> 0.02, bounds 0-0.03, `import_ground.py` line 154 (difference 3).
4. Codex — A3 `cobble` crop -> [915,600,70,215]; re-run the gate on both measure folders (difference 4).
5. Codex — re-run `import_ground.py`; recapture `after/` (all views, the four sun views included) and `after-measure/`; re-run `perf_gpu.py` on the final material and put the row in the README (difference 5); rebuild pairs; fresh review. Pass condition: nine crops <= 0.25 with no lag at 1.2-1.5 m ground and no striped or same-shaped patch in any A-view crop (held in r3 and r4, must hold again); after/04 [600,450,400,250] sharpness >= 15 with slabs that have a dark underside at 3x and no doubled stone in any overlap band; ctx-ground-1 >= 15 pebbles with an edge; ctx-ground-3 [100,850,400,200] sharpness >= 8.5, no one-angle streaks; moss coverage ctx-ground-2 and ctx-ground-3 lower band <= 0.10; 04 2x [1100,300,500,300] shows layer A; sun views differ from their default twins by > 8 and show no brightness step or straight tone edge along any hex border on A, T, R; perf median at 01 within +1 ms of the before runs.

## r3 fix items: status

- r3 fix 1 (`MaskNoise` 0.15 -> 0.05): implemented. 04 is layer A again (met); ctx-ground-2 0.153 > 0.10 (not met); A3 limit void (item 2). Corrected to 0.02 in fix 3.
- r3 fix 2 (`hsharp` 6 -> 16): implemented; no visible effect, and it could not have one: the r3 instruction sized it from an assumed 0.25 height step where the map has 0.03-0.05 (item 6). Corrected in fix 2. The frame-wide relief loss it was meant to cure is a different defect (fix 1).
- r3 fix 3 (spec paragraph: B hex, +30 taps): done (`GFX-02-photoreal-ground-pipeline.md` line 220).
- r3 fix 4 (sun capture): done with a stated deviation (pitch -20 kept, sun made Movable, before sun views recaptured like for like); accepted, the twins differ by 39-56.
- r3 fix 5 (gate crops off the props): three of four done; A3 cobble still has a canopy in its bottom 40 px (fix 4).
- r3 fix 6 (recapture, gate, perf, pairs): done; the per-pass GPU profile is in (BasePass +0.45 ms) but on the round-3 material, with a README sentence promising a re-time that is not there (fix 5).
- New this round: the relief loss on `Rot 1.0` layers is reported with its cause for the first time (r2 and r3 saw it as "double exposure" and "dissolved stones" and treated it as band blending); the ctx-ground-3 streaking is new since r3's fix 1 restored layer A there.

## Carried forward (not this job)

- If fix 1 confirms the mirrored tangent frame, the before's plain normals (and bedrock C's) are y-flipped against the world-xy UV on this mesh too: a pre-existing lighting error on every ground layer, for J2 (landscape material) to settle with the mesh's tangent basis.
- Aerial colour blend 25-60 m and macro luminance variation per layer (J2).
- Low-poly lime-green blob trees and bushes at A1-A3 and ctx-overview; identical house rows; bare plain with no fields, hedges or tracks between town and river.
- White blob mountains without rock or snow detail (A1, 01, ctx-ground-1/2/3).
- Cobbled carriageway where `gameplay-hud.png` far range shows a dirt track; beige cobble vs the concept's grey-brown stone.
- Bookmark 04 vs `Greenlands-Forest-Look-v1.webp`: no leaf litter, needles, loose stones or undergrowth variety; hard 1 px moss-patch edges at 1 m from `Low 0.4 / InvRange 32` (present in the before).
- Hard-edged river bank and flat water colour (A2, A3).
- mossy_rock crack dashes inside the moss patches (ctx-ground-2) read as rock slab, not moss: a texture choice for layer B, not a tiling defect.
