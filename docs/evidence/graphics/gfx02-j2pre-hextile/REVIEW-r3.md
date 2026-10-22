# GFX-02 J2-pre hex-tiled ground: independent review, round 3 (Fable, 2026-10-22)

Judged against `docs/decisions/GFX-02-photoreal-ground-pipeline.md` section 2a (Interim J2-pre, Acceptance) and section 7 row J2-pre. Domain: repeating motifs on cobble, earth or grass at A1-A3, plus anything the hex-tiling or its fixes introduced (cells, seams, brightness steps, double exposure, layer-mix regressions). Every frame in `before/`, `after/`, `before-measure/`, `after-measure/` and the six compare sheets was read; the nine gate rectangles were cut before|after at 3x from the measure frames, 14 open-field windows at 2x, margins at 4-5x, ground-level views at 3-4x; `Measure-GroundRepeat.py` was re-run on `after-measure/` (identical: `GROUND_REPEAT PASS crops 9 max_peak 0.1974`). Moss coverage below is the fraction of pixels with G - R > 8 in the named window (same classifier on before and after).

Tally: PASS 10 (A1, A2, A3, 01, ctx-overview-south, ctx-overview-east, ctx-ground-4, A1-sun20, A1-margin-sun20, ctx-ground-4-sun20). FAIL 5 (04-forest-floor, 04-forest-floor-sun20, ctx-ground-1, ctx-ground-2, ctx-ground-3).

Verdict: NOT PASSED. The A-view repeat (the job's headline) is fixed; the r2 fix list as written broke the forest floor and the ground-level plain.

## A1-south-approach-30m — PASS

- Grass crop [175,440,128,128] (3x, measure frames): the r2 striped patches are gone; the after is a plain olive field with two soft pale blotches of different shape. No motif with two copies. Gate 0.098 at lag (-16,0): a 16 px lag again, but the crop is now featureless and the peak is a third of r2's 0.280; nothing in the crop repeats.
- Earth crop [705,730,110,96]: the chevron ribbing of the before (0.544) is gone; the after is mottle (0.086). The crop no longer holds the shrub corners.
- Cobble crop [876,816,256,256]: sand at 30 m, no motif (0.095). One shrub tip in the top-right corner, 20 x 30 px (measurement item 5 below).
- Margin 4x at [640,650,220,220]: the before's diagonal chevron lattice is gone from the packed earth; no hex cells or seams.
- Open field [0,560,400,300] and [1250,520,400,300]: no X tufts, no stripes, no rows. Moss blotches are irregular and of unequal size.
- Against `gameplay-hud.png` far range: same layout and proportions as the r2 after; the ground now reads as continuous meadow rather than printed fabric.

## A2-south-approach-100m — PASS

- Grass crop [1230,800,256,256] (3x): the 14 striped copies of r2 are gone. The crop holds two large moss areas with soft lobed outlines and a fine lichen mottle inside; no stripe, no fixed angle, no two patches of the same shape. Gate 0.094 at (35,15).
- Earth crop [815,600,64,256]: no lattice (0.116; before 0.665). Margin 4x [790,540,120,330]: fine mottle, no chevrons, no cells.
- Cobble crop [885,790,160,256]: sand, no motif (0.110). The right 40 px of the crop are a tree canopy (measurement item 5).
- Open field [0,250,500,350], [1250,560,500,350], [0,600,500,350], [600,150,500,300]: moss patches range from 2 m blobs to 20 m sheets, outlines irregular, no rows, no hex polygons. Coverage in [0,250,500,350]: before 0.033, after 0.051 (acceptable at this range).

## A3-south-approach-250m — PASS

- Grass crop [1075,520,190,190] (3x): the 22 striped copies are gone; the after shows one 60 px moss sheet and three small blobs, none alike. Gate 0.062 at (-26,-5).
- Earth [884,600,32,256] and margin 5x [860,540,80,320]: the before's chain-link lattice on the packed earth is gone; the after margin is an even brown with faint mottle. One shrub sits inside the earth crop at y ~770 (item 5).
- Cobble [915,600,85,256]: 0.197 at r = 6.08 px, still the inner band edge on an 85 px strip; the road reads as one tone, no motif. Canopies touch the right edge of the crop (item 5).
- Open field [0,300,500,400], [450,500,450,350], [0,700,500,380], [1100,300,400,300], [1300,650,500,400]: no stripes, no rows, no same-shaped patch anywhere. Moss coverage in [0,300,500,400]: before 0.090, after 0.198. From 250 m the plain reads as a two-tone camouflage of olive and pale-green sheets; it is not a repeat and it is not a hex artefact, so it does not fail this view, but it is the same coverage doubling that fails the ground-level views below.

## 01-forest-path-town — PASS

Cobble 3x at [300,800,500,280] and [900,900,500,180]: identical to r2 (layer R untouched this round): continuous courses, no doubled stones, no border seams, no direction change. Earth margin and grass beyond: no seams, no repeat.

## 04-forest-floor — FAIL

- What's wrong: the forest floor is gone. 3x at [600,450,400,250]: before shows 12 flat slabs 30-120 px with crisp edges; after shows 0 slabs, a smeared lichen mottle (bright yellow-green patches 10-40 px with soft edges over dull olive), and in [100,800,400,250] a dark red-brown rock smear with no stone edge. This is layer B (`mossy_rock`, hex-tiled, no height) covering the whole 04 spot, the same regression r1 reported ("the frame turned into green mossy rock"). The before|after compare sheet shows it at a glance: every slab, stick and gravel bed in the before is missing from the after.
- Cause: r2 fix 2 as implemented. `mkv = mk + nzA x MaskNoise` with `MaskNoise` 0.15 moves the `Low` 0.4 threshold by +-0.15 while `InvRange` 32 makes the ramp 0.03 wide; wherever the Simplex noise is positive, every mask value above 0.25 becomes moss. The README says so itself ("Round 3 does not change which layer covers 04 except through MaskNoise") and records no check of the result.
- Consequence for r2 fix 3: the `HeightSharp` tie-break is wired on layer A only, and layer A is not visible at 04, so fix 3's acceptance ("slabs with edges as crisp as the before") cannot be judged here. Where A is visible (ctx-ground-1) it does not pass either, see below.
- Against `Greenlands-Forest-Look-v1.webp` (loose hard-edged stones, needles, litter): the after is further from the concept than r2's after, which was further than the before.
- Fix type: CODE. See fixes 1 and 2.

## 04-forest-floor-sun20 — FAIL

Same content as 04 (mean abs pixel difference to `after/04-forest-floor.jpg` 1.5 of 255: the two frames are the same image). Fails for the same reason, and the sun did not move (measurement item 4): no hex-border lighting check was performed on any layer this round.

## Context views

### ctx-overview-south, ctx-overview-east — PASS
Fields at 300 m: even mottle, no rows, no lattice, no honeycomb. Moss sheets at the far plain are larger and more varied than r2's uniform 9 m count. The world reads the same between the overviews and A1-A3.

### ctx-ground-1 — FAIL
- Layer A at 1-5 m, 4x at [500,900,300,150]: before shows 20+ discrete pebbles and slabs with a dark underside edge; after shows fewer than 10, none with an edge, the rest dissolved into the soil. 3x at [300,760,600,220]: the same. This is the r2 "double exposure" finding on layer A, still present with `HeightSharp` 6.0 and `H` wired; the exponent is too small for this Displacement map's range (stone vs soil differ by ~0.25 in H, so the weight ratio is only 2^1.5 = 2.8, not enough to let a stone win inside the ~25 % overlap band).
- Fix type: CODE. See fix 2.

### ctx-ground-2 — FAIL
- Moss coverage in the lower band [0,500,1920,580]: before 0.053, after 0.354. The foreground 25 m of the plain is now a continuous lichen carpet (layer B) with the mossy_rock's dark crack lines running through it as 1-3 m dark dashes (4x at [200,900,300,150]; 3x at [1100,780,400,200]: 9 dashes at one angle). It reads as a lichen-covered rock slab, not a meadow; a player walking here would see the ground type change from the before.
- No hex polygons, no straight patch edges (that part of r2 holds).
- Fix type: CODE. See fix 1.

### ctx-ground-3 — FAIL
- Moss coverage in [0,500,1920,580]: before 0.011, after 0.135. The near field is a 30 m moss sheet with the crack dashes (3x at [100,850,400,200]). Same cause as ctx-ground-2.

### ctx-ground-4 — PASS
Cobble 1-5 m (3x at [1000,900,400,180]): continuous courses, no ghosting, no seams (unchanged from r2). Earth margin: no cells.

## Sun-20 views (hex-border brightness steps)

### A1-sun20 — PASS
Cobble and margin: no polygonal brightness steps. But the frame is the default-sun frame: shadows in [700,500,500,300] at 2x are the same length and direction as `after/A1-south-approach-30m.png` (mean abs difference 2.5 of 255). See measurement item 4; the view passes on what it shows and proves nothing about grazing light.

### A1-margin-sun20 — PASS
Earth (T) and cobble (R) at 1-5 m, 3x at [500,820,400,250]: no step along any border, no mortar-line seam. Same caveat as A1-sun20.

### ctx-ground-4-sun20 — PASS
Cobble 1-5 m, 3x at [900,700,600,300]: no steps. Mean abs difference to `ctx-ground-4.jpg` 0.9: same lighting as the default capture.

## Measurement credibility

1. The gate reproduces (`max_peak 0.1974`). All nine lags are now below 1.2 m on the ground or on a featureless crop; the r2 B-tile period (16 / 18.6 / 7.1 px) is gone from A1/A2/A3 grass (0.098 / 0.094 / 0.062). The r2 pass condition "nine crops <= 0.25, no lag at 1.2-1.5 m, no striped or same-shaped patch in any A-view crop" is met.
2. A3 cobble 0.197 at r = 6.08 px remains a band-edge reading on an 85 px strip, not a motif.
3. Perf: not re-timed this round (README says so); B hex adds 6 taps. Accepted as inside the budget on r2's margin (7.66-8.02 vs 7.65 ms) but a re-time belongs in the next capture run.
4. The four `*-sun20` captures are the default-sun frames. `capture_game.py` line 58 sets `sun.set_actor_rotation(Rotator(pitch=-20, ...))` on the first `DirectionalLight`, and the output is unchanged (A1 vs A1-sun20 2.5, ctx-ground-4 vs its sun20 0.9, 04 vs its sun20 1.5 mean abs difference; the before pair 04 vs 04-sun20 is 1.7, so it was the same in r2). Either the default pitch already is about -20 or another actor drives the light. The spec's "hex-border lighting self-check at 04" has therefore not been performed on any round.
5. Gate crops hold props, against the spec's "no props or grass cards inside": A2 cobble [885,790,160,256] right 40 px tree canopy; A3 cobble [915,600,85,256] canopies on the right edge at y 680-760 and 1000-1080; A3 earth [884,600,32,256] a shrub at y ~770; A1 cobble [876,816,256,256] a shrub tip in the top-right 20 x 30 px.

## Differences, most visible first

1. Forest floor and ground-level plain replaced by mossy rock (04, 04-sun20, ctx-ground-2, ctx-ground-3).
   What's wrong: `MaskNoise` 0.15 shifts a 0.4 threshold with a 0.03 ramp; coverage at ground level went 0.05 -> 0.35 (ctx-ground-2) and 0.01 -> 0.14 (ctx-ground-3); at A3 0.09 -> 0.20; at 04 the stone floor is gone (12 slabs -> 0 in the 3x crop).
   Fix type: CODE. Exact: `GameClient/ArtSource/Greenlands/NatureFamilies/import_ground.py` line 154, replace
   ```python
   mkv = g.op(u.MaterialExpressionAdd, mk, g.mul(nzA, g.sc('MaskNoise', 0.15, -900, 2750), -850, 2700), -700, 2600)
   ```
   with
   ```python
   mkv = g.op(u.MaterialExpressionAdd, mk, g.mul(nzA, g.sc('MaskNoise', 0.05, -900, 2750), -850, 2700), -700, 2600)  # bounds 0-0.08: 0.15 flooded 04 and ctx-ground-2/3 with layer B
   ```
   and line 153's comment `(MaskNoise 0-0.3)` -> `(MaskNoise 0-0.08)`. Acceptance: 04 3x at [600,450,400,250] shows layer A slabs; moss coverage (G - R > 8) in ctx-ground-2 [0,500,1920,580] <= 0.10 and in A3 [0,300,500,400] <= 0.13; A2/A3 open-field patches still vary in size (no uniform 9 m stamp). If 04 still shows B at 0.05, use 0.03; do not raise `Low`.

2. Stones on layer A dissolve in the hex overlap bands (ctx-ground-1 now; 04 once fix 1 restores A there).
   What's wrong: `exp2(6 x H)` gives a 2.8x weight ratio between a stone and its neighbour's soil for this Displacement map; the ~25 % overlap band still blends three full-contrast taps.
   Fix type: CODE. Exact: `import_ground.py` line 137, `hsharp=6.0` -> `hsharp=16.0` (bounds 0-24; 16 x 0.25 = 4 octaves, a 16x ratio). No HLSL change: `hex_tile.py` line 42 already multiplies by `exp2(HeightSharp * Hv)`. Acceptance: ctx-ground-1 4x at [500,900,300,150] shows pebbles with a dark underside edge as the before does (count >= 15); 04 3x at [600,450,400,250] shows slabs with edges as crisp as the before; no hard seam line along a hex border at 04 or ctx-ground-1 (if a seam appears, 12.0).

3. Spec wording still says B stays plain.
   What's wrong: r2 fix 1 asked for the section 2a Interim paragraph to say `B` is hex-tiled; `docs/decisions/GFX-02-photoreal-ground-pipeline.md` is unchanged (git status) and still reads "`B` (mossy rock) and bedrock `C` stay plain (slope-only, triplanar already breaks the grid)".
   Fix type: CODE (doc). Exact: line 220, replace that clause with "`B` (mossy rock) hex too (it fills the moss patches on open ground in `M_GN_Ground`; `Rot` 1.0, `CellTiles` 1.0); bedrock `C` stays plain (triplanar)", and "+24 taps" -> "+30 taps".

4. Sun-20 captures are the default sun (measurement item 4).
   Fix type: CODE (capture). Exact: `docs/evidence/graphics/gfx02-j2pre-hextile/capture_game.py` line 19, after `sun0 = sun.get_actor_rotation()` add `u.log(f'J2PRE_SUN0 pitch {sun0.pitch:.1f} yaw {sun0.yaw:.1f}')`; line 58, after the `set_actor_rotation` call add `u.log(f'J2PRE_SUN {v["name"]} pitch {sun.get_actor_rotation().pitch:.1f}')`; change every `sun_pitch=-20` (lines 22, 23, 32, 33) to `sun_pitch=-8` so the condition is unmistakable. If the logged pitch after the set equals `sun0.pitch`, the light is driven elsewhere: set the pitch through that actor (find it with `get_all_actors_of_class(world, u.Actor)` filtered on a `DirectionalLightComponent` or a `sun`/`sky`/`time` label) instead. `make_pairs.py`: after the sheets, assert `mean |after/<v> - after/<v>-sun20| > 8` for the three views that have a default twin, and write the three numbers to the README.

5. Gate crops hold props (measurement item 5).
   Fix type: CODE (data). Exact: `GameClient/ArtSource/Greenlands/silverbrook-reference-bookmarks.json` `repeat_crops`: A1 `cobble` [876,816,256,256] -> [876,850,256,222]; A2 `cobble` [885,790,160,256] -> [885,790,110,256]; A3 `cobble` [915,600,85,256] -> [915,600,70,256]; A3 `earth` [884,600,32,256] -> [884,600,32,160]. Re-run the gate on both measure folders.

## Fix list (job domain, deduplicated)

1. Engineer — `MaskNoise` 0.15 -> 0.05 (bounds 0-0.08), `import_ground.py` line 154 (difference 1).
2. Engineer — `hsharp` 6.0 -> 16.0 for layer A, `import_ground.py` line 137 (difference 2).
3. Engineer — spec section 2a Interim paragraph: B hex-tiled, +30 taps (difference 3).
4. Codex — sun-20 capture: log `sun0` and post-set pitch, pitch -8, drive the owning actor if the set has no effect, assert frame difference > 8 in `make_pairs.py` (difference 4).
5. Codex — gate crops re-fixed off the props (difference 5); re-run gate on `before-measure/` and `after-measure/`.
6. Codex — re-run `import_ground.py`; recapture `after/` (all views incl. the four sun views) and `after-measure/`; re-time perf-01 (B hex not timed); rebuild pairs; fresh review. Pass condition: nine crops <= 0.25 with no lag at 1.2-1.5 m ground and no striped or same-shaped patch in any A-view crop (held this round, must hold again); 04 3x at [600,450,400,250] shows layer A slabs as crisp as the before; ctx-ground-1 pebbles with edges; moss coverage ctx-ground-2 lower band <= 0.10, A3 [0,300,500,400] <= 0.13; sun views differ from their default twins by > 8 and show no brightness step along any hex border on A, T, R.

## r2 fix items: status

- r2 fix 1 (layer B hex): fixed. The 1.2-1.5 m stripes are gone from every A-view window and from the moss patches at ctx-ground-2/3.
- r2 fix 2 (Mask + 0.15 x noise): implemented as specified; the specified amplitude is wrong. Patch size now varies (the intended effect), but coverage doubled to quadrupled and 04 and the ground-level plain flipped to layer B. The r2 instruction carried the error (it set the value without a coverage bound); it is corrected in fix 1 above.
- r2 fix 3 (`HeightSharp`, `H` wired on A): implemented; no visible effect at 6.0 (ctx-ground-1). 04 cannot show it until fix 1 restores layer A there.
- r2 fix 4 (A1 earth crop): fixed.
- r2 fix 5 (recapture, gate, pairs): done; the gate passes. Captures were not checked for the 04 regression before hand-off.
- New this round: none beyond the above; the sun-20 issue (item 4) predates round 3 (the before pair shows the same 1.7 difference) and is reported here for the first time.

## Carried forward (not this job)

- Aerial colour blend 25-60 m and macro luminance variation per layer (J2).
- Low-poly lime-green blob trees and bushes at A1-A3 and ctx-overview; identical house rows; bare plain with no fields, hedges or tracks between town and river.
- White blob mountains without rock or snow detail (A1, 01, ctx-ground-1/2/3).
- Cobbled carriageway where `gameplay-hud.png` far range shows a dirt track; beige cobble vs the concept's grey-brown stone.
- Bookmark 04 vs `Greenlands-Forest-Look-v1.webp`: no leaf litter, needles, loose stones or undergrowth variety; hard 1 px moss-patch edges at 1 m from `Low 0.4 / InvRange 32` (present in the before).
- Hard-edged river bank and flat water colour (A2, A3).
- Perf `stat gpu` overlay not captured by `HighResShot`; per-pass GPU numbers still missing.
