# GFX-02 J2-pre hex-tiled ground: independent review, round 2 (Fable, 2026-10-22)

Judged against `docs/decisions/GFX-02-photoreal-ground-pipeline.md` section 2a (Interim J2-pre, Acceptance) and section 7 row J2-pre. Domain: repeating motifs on cobble, earth or grass at A1-A3, plus anything the hex-tiling introduced (cells, seams, brightness steps, regressions). Every image in `before/`, `after/`, `before-measure/`, `after-measure/`, the five compare pairs and both sun-20 crops was read at full size; the nine gate rectangles and twelve extra open-field windows were cut before|after at 2-3x from the measure frames, ground-level views at 2-3x from `before/`/`after/`; `Measure-GroundRepeat.py` was re-run on both measure folders (numbers reproduce exactly).

Tally: PASS 11 (01, ctx-overview-south, ctx-overview-east, ctx-ground-1, -2, -3, -4, A1-sun20, A1-margin-sun20, ctx-ground-4-sun20, 04-forest-floor-sun20). FAIL 4 (A1, A2, A3, 04).

## A1-south-approach-30m — FAIL

- Cobble crop [876,816,256,256]: uniform beige sand, no motif (0.096). Stone pitch is not resolved at 30 m in either frame.
- Earth crop [700,715,128,112]: the before's diagonal chevron ribbing (0.559) is gone; after is irregular mottle (0.078). Two PCG shrubs sit in the top-right and bottom-right corners of the crop (~5 % of its area); the README's "crops avoid them" is not true for this one.
- Grass crop [175,440,128,128]: FAIL. Motif: a pale green patch 2-4 m long, filled with parallel diagonal pale/dark stripes at one fixed angle (~35° in the frame) and one fixed pitch. 9 copies in the crop; every copy carries the same stripes at the same angle. Gate 0.280 at lag (16,-1) = 16 px.
- Open field left [0,560,400,300] and right [1250,520,400,300]: the before's identical "X" tufts on a 2 m grid are gone; the after shows the same striped patches, scattered, ~30 in the left window. No honeycomb on the field (r1 difference 2 is fixed).

## A2-south-approach-100m — FAIL

- Cobble crop [885,790,160,256]: uniform sand (0.110); before had a diagonal stone lattice (0.436). No motif.
- Earth crop [815,600,64,256]: before had a hard chevron lattice (0.665); after is mottle (0.118). No motif.
- Grass crop [1230,800,256,256]: FAIL. Same striped patch motif. 14 copies in the crop (before: 19 on four straight diagonal rows). The rows are gone: copies are no longer collinear. Every copy has the stripes at the same angle and pitch. Gate 0.236 at lag (15,11) = 18.6 px, just under the limit.
- Open field [0,250,500,350], [1250,560,500,350], [0,600,500,350]: 25-40 copies per window, scattered. No hex cells, no 1 m honeycomb.

## A3-south-approach-250m — FAIL

- Cobble crop [915,600,85,256]: road only, no motif visible (0.204 at lag (1,6), r = 6.08 px, i.e. at the band edge; see Measurement).
- Earth crop [884,600,32,256]: 32 px strip, no motif (0.098).
- Grass crop [1075,520,190,190]: FAIL. Same striped patch motif at 8-12 px: 22 copies (before: 30 on rows). Gate 0.295 at lag (-5,5) = 7.1 px.
- Open field [0,300,500,400], [450,500,450,350], [0,700,500,380], [1100,300,400,300]: patches across the whole plain at a roughly even density (one per ~10 m), no straight rows, every patch striped at the same angle.

What the stripes are: the measured lag is 16 px at A1 far field, 18.6 px at A2, 7.1 px at A3; at those pixel sizes (0.07-0.10 m/px at A1-A2, 0.20 m/px at A3) all three are 1.2-1.5 m on the ground, one fixed world-space period. The moss patches are `wm` lerping layer A to layer B (`mossy_rock`, `TileCmB` 150 = 1.5 m, plain sample, `import_ground.py` line 134); B's tile lattice shows through every patch and is world-aligned, so every patch displays the same stripes. The before frames show the same stripes inside the X tufts (before A1 grass 0.244 at lag (17,-1): the same period, the before "passes" by 0.006). The spec exempts B from hex-tiling as "slope-only, triplanar"; in `M_GN_Ground` B is neither: a flat `xy` sample that fills the moss patches on open ground.

## 01-forest-path-town — PASS

- Cobble 1-5 m (3x at [100,900,400,180], [300,800,500,280], [900,900,500,180]): one continuous stone grid, courses straight through, no doubled stones, no direction change at any cell border. r1's hex borders every 1.2 m are gone (`Rot` 0, `CellTiles` 1.0, `Snap` 14). Stones differ between neighbouring 2.4 m cells; no stone set recurs in the foreground.
- Earth margin and grass beyond: no seams, no repeat.

## 04-forest-floor — FAIL

- Hex-border brightness step at sun 20 (`04-forest-floor-sun20`, `04-sun20-after-crop.png`): none along any edge; the visible ground is now layer A (the hex layer), so this capture tests the normal rotation and passes it.
- Layer mix: the r1 regression (lichen rock slab) is gone; the floor is forest_ground_04 with moss patches again.
- Regression introduced by the blend: stone definition at 1-3 m. 3x at [600,450,400,250]: before shows 12 flat slabs 30-120 px with crisp edges; after shows 3 soft-edged slabs and a mush of small gravel, and the whole crop reads as a double exposure. 3x at [100,800,400,250]: same, litter and sticks lose their edges. Cause: the three-way blend `W = lerp(1, L, 0.6) * pow(w, 7)` blends full-contrast scans wherever two cells overlap (~25 % of each 2 m cell at exponent 7) and nothing makes a stone win over its neighbour's soil; `H` is unwired (DefaultTexture), so the height-based tie-break the function was built for is off.
- Against `Greenlands-Forest-Look-v1.webp` (loose stones with hard edges, needles, litter): the after is further from the concept than the before at this range because of the softness.

## Context views

### ctx-overview-south, ctx-overview-east — PASS
Fields at 300 m: even fine mottle, no rows, no lattice. The moss-patch density is uniform across the plain (fix 2 below changes that), but no structure. The world reads consistently between the two overviews and A1-A3.

### ctx-ground-1, ctx-ground-2, ctx-ground-3 — PASS
3x at ctx-ground-2 [1100,780,400,200], ctx-ground-3 [100,850,400,200], ctx-ground-1 [300,760,600,220]: moss patches now have soft irregular outlines; no straight edges, no 120° corners. The 2.25 m polygonal cells of r1 are gone. Inside the patches the same parallel dark dashes as A2/A3 (B's 1.5 m tile) are visible at 3-10 m; that is the A-view defect (fix 1), present in the before as well, not a hex artefact.

### ctx-ground-4 — PASS
Cobble 1-5 m (3x at [1000,900,400,180]): continuous courses, no patchwork of orientations, no ghost bands. Earth margin: no cells.

## Sun-20 views (hex-border brightness steps)

### A1-sun20 — PASS
Cobble and margin at 30 m with long tree shadows: no polygonal brightness steps on either; the carriageway is one tone.

### A1-margin-sun20 — PASS
Earth (layer T) at 1-5 m, cells 2 m = 150-400 px: no step along any border on the margin; cobble at 3x [500,820,400,250] no step, no mortar-line seam.

### ctx-ground-4-sun20 — PASS
Cobble 1-5 m at sun 20 (3x at [900,700,600,300], [1300,850,600,230]): no brightness steps; the earth-cobble edge is the same as at the default sun.

### 04-forest-floor-sun20 — PASS
Layer A at 1-3 m: no step along any edge. `NEG_ANGLE` stays False.

## Measurement credibility

The gate now measures what it claims. Re-run: identical numbers (`GROUND_REPEAT FAIL crops 9 max_peak 0.2946`; before `0.665`). Checks:
1. It fails the before where the eye fails it: A3 grass before 0.433 (r1's demand), A2 grass 0.342, A2/A3 cobble 0.44/0.59, A1/A2 earth 0.56/0.67, all at lags that match the visible lattices.
2. The two after failures and A2's 0.236 are one world-space period (1.2-1.5 m across three distances) with a mechanism in the material (B's 150 cm tile). They are real.
3. The measure frames are shadow-free; the grass and cobble crops hold no props. A1 earth holds two shrub corners (minor; re-fix to [705,730,110,96] or hide the PCG actor for measurement captures).
4. Weak points, not disqualifying: the 0.25 limit sits on the B-tile signal itself (before A1 grass 0.244 with the same lag "passes"); A3 cobble 0.204 is at r = 6.08 px, the inner band edge, on an 85 px strip; A3 earth is a 32 px strip, so only lags within ±16 px horizontally exist. After fix 1 these three should drop well below 0.15; if A1/A3 grass stay above 0.20 at a 1.5 m lag the attribution is wrong and the next suspect is the hex-tiled layer A's 2 m cell lattice (`TileCmA` 200 x `CellTiles` 1.0).

Perf: after 7.66-8.02 ms median vs before 7.65 ms, inside the +1 ms budget; accepted as reported.

## Differences, most visible first

1. Striped moss patches, dozens of copies per A-view window (A1 9, A2 14, A3 22 inside the gate crops).
   What's wrong: every moss patch is a window onto the plain-tiled `mossy_rock` layer B at 1.5 m; the gate reads that period (lags 16 / 18.6 / 7.1 px) at 0.28 / 0.24 / 0.30.
   Fix type: CODE. Exact: `GameClient/ArtSource/Greenlands/NatureFamilies/import_ground.py` line 134:
   ```python
   fb, nb, rb, _ = layer('B', T + 'mossy_rock_Diffuse', T + 'mossy_rock_nor_gl', T + 'mossy_rock_Rough', 150, (0.34, 0.46, 0.20), 1200, hex=True)
   ```
   (`rot` 1.0, `cell` 1.0, `snap` 0 defaults). +6 taps on the mesh terrain. Update spec section 2a Interim paragraph: "`B` (mossy rock) hex too (it fills the moss patches on open ground in `M_GN_Ground`); bedrock `C` stays plain (triplanar)". Acceptance: A1/A2/A3 grass lag no longer 1.2-1.5 m on the ground; peaks <= 0.15.

2. Moss patches at one per 9 m everywhere, all the same size (A2/A3 open field, ctx-overview-east far plain).
   What's wrong: the Mask is one 9 m texture; the warp moves its bright spot (rows gone) but every tile still crosses `Low` 0.4 with the same spot, so the plain carries one patch per 81 m² of the same size. With the stripes removed (fix 1) they become soft blobs; the uniform count still reads as a stamp at A3.
   Fix type: CODE. Exact: `import_ground.py` lines 142-147: keep a handle on the first warp noise and add it to the mask before the threshold:
   ```python
   nzA = warp_noise(2900, ''); wxy = node(m, u.MaterialExpressionAppendVector, -1500, 2900); link(nzA, '', wxy, 'A'); link(warp_noise(3050, 'B'), '', wxy, 'B')
   ...
   mkv = g.op(u.MaterialExpressionAdd, mk, g.mul(nzA, g.sc('MaskNoise', 0.15, -900, 2750), -850, 2700), -700, 2600)
   sub = g.op(u.MaterialExpressionSubtract, mkv, g.sc('Low', 0.4, -600, 2700), -500, 2600)
   ```
   `sw` (line 190, trail stones) keeps reading `mk`. Zero extra taps; patches then range from absent to twice the size over 25 m noise features. Bounds for `MaskNoise`: 0-0.3.

3. Forest floor stones soft at 1-3 m (04).
   What's wrong: three-way blend with no height tie-break; slabs in the ~25 % overlap bands are double-exposed.
   Fix type: CODE. Exact: `hex_tile.py` `spec` gains `('HeightSharp', T.FUNCTION_INPUT_SCALAR, 0.0)` after `Snap` (and the matching `CustomInput`); HLSL line 40 becomes
   ```hlsl
   float3 Hv = float3(cH[0], cH[1], cH[2]);
   float3 W = lerp(1.0, L, 0.6) * pow(w, 7.0) * exp2(HeightSharp * Hv); W /= (W.x + W.y + W.z);
   ```
   (HeightSharp 0 or an unwired `H` = constant DefaultTexture leaves W unchanged, so `landscape_material.py` callers are unaffected.) `import_ground.py` `hexlayer()`: add `('H', 'Height', disp, S.SAMPLERTYPE_MASKS, 300)` to the texture loop when a `_Displacement` texture exists for the scan (A: `T + 'forest_ground_04_Displacement'`; T/S/R: wire if `import_scan` brought one in, else leave default) and link a constant `HeightSharp` 6.0 to the call. No extra taps: `H` is already sampled three times from DefaultTexture. Acceptance: 3x crop of 04 at [600,450,400,250] shows slabs with edges as crisp as the before.

4. A1 earth gate crop holds two shrub corners.
   Fix type: CODE (measurement). Exact: `silverbrook-reference-bookmarks.json` A1 `repeat_crops.earth` -> `[705, 730, 110, 96]`, or hide the PCG shrub actor for `GFX_MEASURE=1` captures in `capture_game.py`.

## Fix list (job domain, deduplicated)

1. Engineer — layer B hex-tiled (`hex=True`), spec 2a wording updated (difference 1).
2. Engineer — Mask + 0.15 x Simplex noise before `Low` threshold (difference 2).
3. Engineer — `HeightSharp` input and height-weighted blend in `MF_GN_HexTile`; wire `H` for layer A (and other scans with a Displacement) at 6.0 (difference 3).
4. Codex — A1 earth crop re-fixed or shrubs hidden for measurement captures (difference 4).
5. Codex — re-run `import_ground.py`, recapture A1-A3 (review and measure frames), 04, 04-sun20, ctx-ground-1..4, ctx-overview, re-run the gate on before/after, rebuild the pairs; fresh review. Pass condition: nine crops <= 0.25 with no crop's lag at 1.2-1.5 m ground, and no striped or same-shaped patch repeated in any A-view crop.

## r1 items: status

- r1 fix 1 (Mask plain + warp): fixed. Rows at A2/A3 gone, hex polygons at ctx-ground-1/2/3 gone, 04 layer mix restored. The residual motif is layer B's tile (new finding, fix 1 above) and the uniform 9 m count (fix 2).
- r1 fix 2 (`CellTiles` 1.0): fixed. No 1 m honeycomb at A1 margins or A2.
- r1 fix 3 (`Snap`, cobble `Rot` 0): fixed. 01 and ctx-ground-4 continuous courses, no ghosting, no border seams; A1 stone read unchanged (not resolvable at 30 m either way).
- r1 fix 4 (gate method and crops): fixed except the A1 earth shrub corners.
- r1 fix 5 (sun-20 ground-level captures): fixed; no brightness steps on T, R or A at 1-5 m.
- r1 fix 6 (recapture): done.
- New regression this round: 04 stone softness (difference 3).

## Carried forward (not this job)

- Aerial colour blend 25-60 m and macro luminance variation per layer (J2).
- Low-poly lime-green blob trees and bushes at A1-A3 and ctx-overview; identical house rows; bare plain with no fields, hedges or tracks between town and river.
- White blob mountains without rock or snow detail (A1, 01, ctx-ground-1/2/3).
- Cobbled carriageway where `gameplay-hud.png` far range shows a dirt track; beige cobble vs the concept's grey-brown stone.
- Bookmark 04 vs `Greenlands-Forest-Look-v1.webp`: no leaf litter, needles, loose stones or undergrowth variety; hard 1-px moss-patch edges at 1 m from `Low 0.4 / InvRange 32` (present in the before).
- Hard-edged river bank and flat water colour (A2, A3).
- Perf `stat gpu` overlay not captured by `HighResShot`; per-pass GPU numbers still missing.
