# WP6i round-10 review — Silverbrook town, pass 10 r10 (REVIEW-r9.md fix list 1-7)

Reviewer: independent (Fable), did not build the work. Judged per `docs/visual-review.md` against `Silverbrook-Design-Board-v1.webp` and `UI/v5/gameplay-hud.png` (mill plume x 1330-1360, y 100-170), the r9 fix list and its acceptances, the r1-r9 and WP6h standing acceptances, and the owner's four reports. Commit 15d19534. Every capture read at full resolution plus 2-6x crops and pixel measurements (`py -3`, PIL/numpy/scipy) against the r9 frames pulled from `15d19534~1`: per-image change maps (all 44 PNGs were recaptured; the noon views differ only by clouds and exposure, mean abs diff 0.1-3.5), cap / pane / plate boxes, the pools, whole-frame hot and saturated-orange components, `check_dusk_blobs.py --selftest` run in place (exit 1, output reproduced below), `measure_plume.py` re-run from a scratch copy on the committed r10 frames, the r10 diff of `lamp_fx.py`, `check_lights.py`, `check_dusk_blobs.py`, `dusk-panes.json`, `lamp-sockets.json` and `vfx.md` read in full, `extract_sockets.py` and `silverbrook_kit.lantern` (lines 337-358) for the lantern's parts. The editor MCP was down, so `LAMP_MATCH` / `LAMP_PIECES` / `LAMP_FX 731` / `LIGHT_CHECK` are the README's numbers, checked for consistency with the frames and the code, not reproduced.

**Result: 16 PASS / 3 FAIL.** Bookmarks 5 / 0, dusk lamp views 0 / 3, context views 11 / 0.

Fixed this round and holding: **the caps are dark.** Frontage cap x 362-412, y 350-374 p90 L 82 (r9 158), lamp-wall cap p90 19 (r9 227), side-b 40; `hot_px` 0 on all three; `DUSK_CAP` passes; the pane stays L 240. `dusk-panes.json` now carries the side-b far-lamp panes, the far windows and a `cap` box per view (fix 3 met); `check_dusk_blobs.py` has the L > 200 orange test, the ring excluding every pane and window, `DUSK_CAP`, and the `--selftest` orange patch (caught; fix 2 met in form). `SM_SilverbrookWeaponsVendor`'s two sockets are gone and `LAMP_NOMATCH` is absent (fix 4 met in form, see fix 3 below for durability). `vfx.md` line 17 no longer says "2-20 m" or "795" (fix 5 met). Noon bookmarks and context views are unchanged in content.

Not met: **the light was moved from the cap to 1.75 cm under the bottom plate, and now the plate glows instead of the cap.** `glass_sockets` line 129 puts the light at `socket z - 16`; the kit lantern's bottom plate is 19 x 19 x 2.5 cm at `body - 0.13` (`silverbrook_kit.py` line 357), i.e. its underside is 14.25 cm below the pane centre, so the 6 cm source sits 1.75 cm under it. The plate's underside is lit point-blank and reads as a bright orange ellipse hanging under every building lantern: frontage x 364-408, y 427-431 max L 252, p90 212 in the 44 x 15 box under the pane (r9: 93); lamp-wall x 446-505, y 1049-1057 p90 251, max 254, 237 px over L 200 (the eye looks up at it; r9: 19); side-b x 1455-1500, y 278-282 p90 236, max 251 (r9: 58). The gate does not see it because every one of those rows lies inside the pane box grown by 10 px. And because the plate now sits above the light, it shadows every wall point above its own plane: the wall beside the pane is dark (frontage x 410-560, y 280-400 median L 26, R - B -29; r9 L 86, +73; lamp-wall x 520-760, y 850-1000 median L 27, R - B -35; r9 L 84, +74). The lantern has become a downlight: in `house-frontage-dusk` the pool sits under the lantern (readable), in `context-lamp-wall-dusk` the lantern lights only the bottom 30 px of the frame (x 520-700 row means: y 850-1030 L 13-37, y 1050 L 94), and in `context-house-side-b-dusk` it lights nothing the eye can see (ring warm 0, soffit glint L 9; r9 76). The README reports the pools as open; it is right.

The r9 geometry claim ("with the light at the pane's mid-height the cap stays dark") was wrong about the outward placement: the builder's measurement that a light 14 cm outward of the pane centre leaves the wall behind the pane in the pane's shadow (warm 94 / 1,193) is consistent with a 15 x 24 cm opaque box 6.5 cm in front of a shadow-casting light. The r10 answer, under the plate, is the mirror of the r9 cap defect. Any iron face that faces a 153 lm light within about 15 cm clips to the colour of the glass; the light has to sit where every iron face is far and at a grazing angle, and the pane and plate must not stand between it and the wall beside the lantern. Fix 1 gives that position; it is a two-constant change plus the three lines r10 removed.

Also: the chimney set was recaptured in r10 (`chimney-smoke.png`, `-ref.png`, `motion-00..13.png` all differ from r9, mean abs diff 0.4-1.0) but `plume-measure.json` was not re-run; it still holds the r9 numbers. Re-measured on the committed frames: core L/ref 0.976 / 1.010 / 1.060, dense puffs R - B -4.6, hill 0.957, sky rows 0.973, MAD 1.33 / 1.50 (all inside), but growth at y 300 = 153 / 87 = **1.76** (band >= 2.0) and thin rows **2 / 1 / 3 / 4** in motion-02 / 10 / 12 / 13 (band 0 in 15 frames). `motion-times.json` has a 0.068 s gap between frames 02 and 03 (a HighResShot stall; the others 0.38-0.46 s). The view still reads as the r9 plume (fix 4).

## Owner reports

| Report | Status | Evidence |
|---|---|---|
| Floating houses | Fixed (WP6h), holds | `foundation-close`, `context-house-side-a/b/rear`: plinth under every wall, buried, no gap; assets unchanged. |
| Orange light with no visible source | Regressed in form | The sources are visible (lit panes) but every building lantern now carries a second, brighter source: the plate's underside, an orange ellipse under the lantern (frontage 44 x 6 px at L 252, lamp-wall 60 x 10 px at L 254, side-b 45 x 5 px at L 251). The wall pools are under the lanterns only; the wall beside a lit pane is dark. Fix 1. |
| Street clutter in mirrored rows on an empty approach road | Fixed (r1), holds | `context-road-approach`: groups only in front of houses, no twin across the road, irregular gaps. |
| White street | Holds | Tan cobbles kerb to kerb in every street view. |
| Lamps lit at noon (J5) | Holds | Every lantern dark in the noon views (`context-lamp-wall` pane cream in sun, `context-house-side-b` lantern dark). |

## Round-9 fix items 1-7: met?

| Item | Verdict | Measured (this review) |
|---|---|---|
| 1 Light at the pane centre, pushed outward; caps dark; pools stay | **Partly**: caps dark (p90 82 / 19 / 40, band <= 120) and `LAMP_MATCH` dist 20 on six townhouses, but the light is under the plate (deviation), the plate glows (p90 212 / 251 / 236 in the plate box) and the pools beside the panes are gone (frontage x 430-560, y 280-500 median R - B -9, band +20; lamp-wall x 520-700, y 900-1080 median L 31, band 80). `DUSK_BLOBS` frontage outside 2 (lit stone, 27 px, L 203). `LAMP_PIECES` shows the pane imports as thin faces (`[0,-16,30]:[1,33,8]`), so no 15 x 15 x 24 piece exists: accepted, the socket is used directly. `LAMP_PIECES` lines given as examples, not per mesh. `LIGHT_CHECK lights 731 on_glass 731 orphans 0 in_wall 0 unshadowed_wall 0 shadowed 586` (README) with the gate loosened to "within 20 cm of the box round the lantern's small pieces" (`check_lights.py` lines 47-50): accepted, documented as weaker. |
| 2 Dusk gate definitions | **Met in form**, one blind spot | Orange test L > 200; ring excludes every box; `WARM` side-b 50; `DUSK_CAP`; `--selftest` catches an L 224 patch. Exit 1 on r10 (side-b warm 0 < 50, frontage 2 blobs). The grown pane box hides the lit plate (fix 2). |
| 3 `dusk-panes.json` | **Met** | Side-b lantern boxes 3, window boxes 7 (far lamp panes, far windows), `cap` per view; the file differs from `b9fc1b5c`. |
| 4 WeaponsVendor sockets | **Met in form, not durable** | Removed by hand from `lamp-sockets.json`; `extract_sockets.py` (unchanged) re-adds them, and the README says its re-run also yields 12 new entries for other meshes that were not kept. Pipeline step 2 ("`extract_sockets.py` -> `lamp-sockets.json`") no longer reproduces the committed file (fix 3). |
| 5 `vfx.md` line 17 | **Met** | Describes the r10 placement and `LIGHT_CHECK 731 ... shadowed 586`; now needs the r11 placement. |
| 6 Lamp family | Owner decision, open | Unchanged. |
| 7 Resubmit | **Met in form**, two gaps | All views recaptured; README carries `LAMP_MATCH` (six meshes), `LAMP_PIECES` examples, `LAMP_FX`, `LIGHT_CHECK`, the `DUSK_*` table, cap p90s, and marks the pools and side-b warm open. Gaps: `plume-measure.json` stale against the recaptured chimney set (two bands now outside); `LAMP_PIECES` not given per mesh. |

## main-street — PASS

Unchanged street: tan cobbles kerb to kerb, five house tones, barrels, crates, hay cart, bench, ladder, signs; lamps and windows dark at noon; weeds at the kerb; far threads pale on the sky. Mean abs diff to r9 0.20. Seen, not failed: the white faceted range (J1b, deferred).

## market — PASS

Stalls with awnings, firewood sheds, ladder, sacks, mixed house tones, cobbles with earth margins, far houses on plinths. Stall stand-in: GFX-03, deferred. Unchanged (diff 0.19).

## house-frontage — PASS

Bay window with planter, boot sign, bracket lantern dark at noon, stoop with tub, ivy, plinth; unchanged (diff 0.23). Seen, not failed: the kerb-high cracked-mud bank at x 0-400, y 600-820 (J2, deferred).

## foundation-close — PASS

Rubble plinth under the ashlar on both faces of the corner, flush, buried; tub and planter on the stoop. Unchanged (diff 0.15).

## chimney-smoke — PASS

A soft pale-grey wisp leaves the pot at x 860-940, widens over the ridge, thins over the hazy hill, the weight of the HUD wisp; frames 00 / 07 / 13 differ in puff placement; no rectangle, no straight edge. Same read as r9 at 1x and in the x 700-1300 crops of r9 and r10.

Not a view defect but an evidence defect: the committed `plume-measure.json` is the r9 measurement of frames that are no longer in the commit. On the r10 frames (scratch re-run of `measure_plume.py`):

| Acceptance (r7 fix 4 / r8) | r10 frames | Met |
|---|---|---|
| densest puffs R - B <= 5 and >= -12 | -4.6 | Yes |
| core L/ref p5 >= 0.90, p50 and mean 1.00-1.15 | 0.976 / 1.010 / 1.060 | Yes |
| hill (y 60-300) p50 0.95-1.15 | 0.957 | Yes |
| sky rows (y < 60) p50 0.90-1.05 | 0.973 | Yes |
| growth >= 2.0 at y 300 | 153 / 87 = 1.76 | **No** |
| 0 thin rows, 15 frames | motion-02 2, -10 1, -12 3, -13 4 | **No** |
| MAD >= 1.0 | 1.33 min, 1.50 mean | Yes |

Both misses are the random puff placement of a single capture, not a parameter change (`silverbrook-props.json` `chimney_smoke` and the Niagara build are untouched in the commit). Fix 4: re-measure on every resubmit, recapture the set once, and report what the committed frames give.

## house-frontage-dusk — FAIL

Bay panes lit, the stoop, tub and planter readable (bottom quarter L p90 59), moon shadow across the field, far cottages with lit windows, sky x 0-250 L 88 (band 10-120), cap dark (p90 82). New and right: the pool under the lantern on the ashlar jamb x 414-541, y 410-577 (12,680 warm px, mean RGB 218 / 133 / 77) with the bracket's shadow in it.

1. **What's wrong:** the lantern's bottom plate glows. Under the pane (y 425) the rows y 427-431 across x 364-408 reach L 247-252 with R - B +92-95: a 44 x 6 px orange ellipse hanging below the iron plate (`frontage-lantern6x`), brighter than the pane itself (L 240). The plate box x 360-412, y 428-436 reads p90 L 237 (r9: 93). Cause: `lamp_fx.glass_sockets` line 129, the light 1.75 cm under the plate's underside with a 6 cm source. **Fix type:** CODE. **Exact instruction:** fix 1.
2. **What's wrong:** the wall beside the pane is unlit. x 410-560, y 280-400 (the ashlar level with the pane) median L 26, R - B -29; in r9 it was L 86, +73. The plate above the light shadows every wall point above its plane, so a lit lantern hangs against dark stone and lights only what is under it. **Fix type:** CODE. **Exact instruction:** fix 1.
3. **What's wrong (gate):** `DUSK_BLOBS outside 2`, both lit stone at x 490-494, y 448-459 (27 px, L 203): the pool is brighter than r9 (the light is nearer the jamb). After fix 1 the light is 14 cm lower and 12 cm farther out; if a stone still passes L 200, lower `_point_light`'s wall factor `.17` to `.14` (line 177) and nothing else. **Fix type:** CODE. **Exact instruction:** fix 1 acceptance.

## context-house-side-b-dusk — FAIL

Window glass lit, bottom quarter 19, the bench and clothes line cast moon shadows, the far street lamp's panes lit with a flame, the far house's windows and door lanterns lit, sky x 0-250 L 23 (band 10-60); no blob, no orange, cap dark (p90 40).

1. **What's wrong:** the corner lantern (x 1455-1500, y 228-276) has the same glowing plate: rows y 278-282, x 1455-1500 max L 251, R - B +115 at y 278; plate box x 1452-1502, y 278-286 p90 L 236 (r9: 58). At 1x it is a bright orange line under the lantern, the only lit iron in the frame (`sideb-lantern6x`). **Fix type:** CODE. **Exact instruction:** fix 1.
2. **What's wrong:** the lantern lights nothing the eye can see: ring warm 0 (`DUSK_WASH ... warm_px 0 need 50`), the jetty soffit above it L 9 (r9 glint L 76-164), the stoop riser unchanged (L 40, R - B -14). The r9 band of 50 assumed the soffit glint; with the light under the plate nothing above the plate can be lit, and after fix 1 the soffit stays in the plate's shadow. Side-b's `WARM` becomes 0: the view is judged on the lantern itself (pane lit, cap and plate dark, no blob). **Fix type:** CODE. **Exact instruction:** fix 2.

## context-lamp-wall-dusk — FAIL

Townhouse1 (`TheCommons_townhouse_037`): gable, jetty, lit upper and bay windows, stag sign, sky L 88, cap dark (p90 19, r9 227), the pane L 239.

1. **What's wrong:** the plate glows, and this eye looks up at it. x 446-505, y 1047-1065: p90 L 251, max 254, 237 px over L 200, 315 over 150; the row maxima y 1049-1055 are 250-254 with R - B +78-127 (`lampwall-lantern6x`: a white-orange ellipse the full width of the plate, brighter than the pane). **Fix type:** CODE. **Exact instruction:** fix 1.
2. **What's wrong:** the lantern no longer lights the wall it hangs on. Row means at x 520-700: y 850-1030 L 13-37 (r9 54-116), only y 1050+ warm (L 94); the wall above the lantern x 400-560, y 850-940 median L 15 (r9 45), beside it x 520-760, y 850-1000 L 27, R - B -35 (r9 L 84, +74). The frame shows a lit lantern at the foot of a dark gable with a sliver of warm stone at the bottom edge; in r9 the same lantern lit the ashlar, the voussoirs and the jetty soffit. **Fix type:** CODE. **Exact instruction:** fix 1.

## Context views

### context-overview-se — PASS

Eye 230 m up: the whole town, forest east and south-east, roads out of each gate, wall extremes in frame; no claim-pad slabs. Unchanged. Seen, not failed (deferred): lake causeway, straight canal and cut banks, flat cyan water, white faceted ring (J1b, GFX-02).

### context-overview-nw — PASS

Westfields meadow, roads, river and bridges fill the bottom; the smooth white dome at x 0-260, y 0-210 is at its 1.2 km ring (J1b). Unchanged.

### context-road-silvermere — PASS

The V cutting, pines on both crests, shrubs and tufts down both banks, cobbles only on the carriageway, dark earth verges. Unchanged.

### context-road-everwood — PASS

Wide cobbled road into the pines, irregular scrub, dark rocks on the left margin, tufts; no stones on the carriageway. Lollipop broadleaf and the white range: GFX-03 / J1b.

### context-road-valemere — PASS

Trees, shrubs, the boulder on the verge, striped fields at the left; no stone sets on the road. Unchanged.

### context-town-edge — PASS

Wall segments and towers stepping with the ground, the far wall and the road out; nothing floats, no lamps on the open road. Unchanged.

### context-house-side-a — PASS

Side wall on its plinth, fence, garden bed, iron lamp dark at noon. Unchanged.

### context-house-side-b — PASS

Plinth stepping to the stoop base; stag sign, bracket lantern and iron lamp dark; forecourt and street clear of stones. Unchanged.

### context-house-side-rear — PASS

Rear plinth following the slope, firewood shelter, fence, lamps dark. Unchanged.

### context-lamp-wall — PASS

Townhouse1 gable in sun: stag sign, bracket lantern dark at noon (pane cream in sun), no wash, windows dark, sill plaster plain. The r9-to-r10 difference (mean 3.5) is clouds and exposure across the whole frame, no content change.

### context-road-approach — PASS

Both sides fronted by houses, groups irregular, no twin across the road, timber-post lamps dark, no stones on the carriageway; chimney threads pale on the sky. Unchanged.

## Deferred (seen, not failed; GFX-02 / GFX-03 / owner, unchanged)

Bank texture streaks and bedrock faces on kerb-high banks (J2); the white faceted mountain ring and the far domes (J1b), 12 m cobble roads outside the wall (J1b); flat cyan water, lake causeway, canal and its cut banks, the zigzag fence lines on the Westfields field (GFX-02 world); ashlar repeats, plaster, stall stand-in, low-poly trees and shrubs, the same shrub mesh repeated (GFX-03); townsfolk mannequins (off for the captures); street-lamp family (r1 fix 9, owner); the claim pad's final form (DEC-03 levelling).

## Fix list (shared first)

1. **Light below and outside the bottom plate, not under it** — Engineer, CODE, `GameClient/ArtSource/Greenlands/lamp_fx.py`. The lantern (`silverbrook_kit.lantern`, non-cage): pane 15 x 15 x 24 at `body`, corner bars 1.6 cm at +-7.5, plates 19 / 20 cm wide x 2.5 thick at `body -+ 0.13`, cap prism +0.14..+0.24. A 153 lm light clips any iron face that faces it within about 15 cm (r9: the cap slope 4 cm away; r10: the plate underside 1.75 cm away). At 30 cm below the pane centre (15.75 cm under the plate's underside) and 12 cm outward of it (2.5 cm beyond the plate's edge) every iron face is >= 17 cm away at a grazing angle (plate underside cos 0.93 at 32 cm, plate edge face cos 0.15 at 17 cm, bar feet cos 0.2 at 18 cm), the pane and cap lie above the light's plane behind the plate (dark), and the wall beside and below the lantern is lit: the lantern body shadows only a strip directly behind itself above the plate (about 60 cm wide at a 40 cm wall), not the whole wall as in r10. Replace line 64
   ```python
   PANE_CLEAR = 14.   # cm: the light sits PANE_CLEAR + 2 cm below the pane centre, under the 24 cm opaque pane (WP6i r8-r10)
   ```
   with
   ```python
   PLATE_DROP = 30.   # cm below the pane centre: 15.75 cm under the 19 cm bottom plate (body - 0.1425 m), so no iron face is lit point-blank (WP6i r11)
   PLATE_CLEAR = 12.   # cm outward of the pane centre along the overhang axis: 2.5 cm beyond the plate's edge (WP6i r11)
   ```
   and lines 111-130 (`else:` through `out.append(c)`) with
   ```python
       else:
           box = mesh.get_bounding_box()
           centre = [(box.min.x + box.max.x) / 2, (box.min.y + box.max.y) / 2]
           half = [max((box.max.x - box.min.x) / 2, 1), max((box.max.y - box.min.y) / 2, 1)]
           for p in (sockets or load_sockets())[name]:
               t = [p[0] * 100, -p[1] * 100, p[2] * 100]
               # only the triangles inside a 120 cm box round the socket, split into pieces: lantern-sized ones are the pane
               sel = u.GeometryScript_MeshSelection.select_mesh_elements_in_box(
                   dm, u.Box(u.Vector(t[0] - 60, t[1] - 60, t[2] - 60), u.Vector(t[0] + 60, t[1] + 60, t[2] + 60)),
                   u.GeometryScriptMeshSelectionType.TRIANGLES, False, 3)[1]
               sub = u.DynamicMesh(); u.GeometryScript_MeshDecomposition.copy_mesh_selection_to_mesh(dm, sub, sel)
               boxes = [b for b in pieces(sub) if all(LANTERN_AXES[0] <= b[i + 3] - b[i] <= LANTERN_AXES[1] for i in range(3))]
               near = sorted(((sum(((b[i] + b[i + 3]) / 2 - t[i]) ** 2 for i in range(3)) ** .5, b) for b in boxes), key=lambda n: n[0])
               d, b = near[0] if near else (1e9, None)
               u.log(f'LAMP_MATCH {name} json {[round(v) for v in t]} piece {b and [round((b[i] + b[i + 3]) / 2) for i in range(3)]} size {b and [round(b[i + 3] - b[i]) for i in range(3)]} dist {round(d)}')
               u.log(f'LAMP_PIECES {name} ' + ' '.join(f'{[round((q[i] + q[i + 3]) / 2 - t[i]) for i in range(3)]}:{[round(q[i + 3] - q[i]) for i in range(3)]}' for q in pieces(sub)))
               if d > 40:   # no kit lantern there: no light from nowhere
                   u.log(f'LAMP_NOMATCH {name} json {[round(v) for v in t]}'); continue
               c = list(t)   # the socket is the pane centre (extract_sockets: the *LanternGlow island centre)
               # WP6i r9-r11: the pane is an opaque emissive box and the light casts shadows. Level with the cap (r9) the cap's
               # outward slope glowed; 1.75 cm under the bottom plate (r10) the plate's underside glowed and the plate shadowed
               # every wall point above it. Below and just outside the plate, every iron face is >= 17 cm away at a grazing
               # angle, and the wall beside and below the lantern is lit (the lantern body shadows only a strip behind itself).
               a = max(range(2), key=lambda i: abs(c[i] - centre[i]) / half[i])   # the horizontal axis the lantern overhangs on
               c[a] += PLATE_CLEAR if c[a] >= centre[a] else -PLATE_CLEAR
               c[2] -= PLATE_DROP
               out.append(c)
   ```
   Update the docstring's last sentence to "The kit pane is an opaque emissive box and the light casts shadows, so the light sits PLATE_DROP cm below the pane centre and PLATE_CLEAR cm outward of it, below and outside the bottom plate." No change to `_point_light`, `LANTERN_AXES` or `check_lights.py` (the light's gap to the union box of the lantern's small pieces is about 16 cm, under the gate's 20; if `LIGHT_CHECK` orphans any building light, report its `LAMP_PIECES` line rather than raising the threshold). Also `check_lights.py` line 5 "lamp_fx puts wall-panel lights 30 cm in front of the pane" -> "lamp_fx puts building lights 30 cm below and 12 cm outward of the pane centre" and `vfx.md` line 17 "moved 16 cm down under the opaque emissive pane (outward of it the pane shadowed the wall; level with the cap it lit the cap)" -> "moved 30 cm down and 12 cm outward, below and outside the bottom plate (level with the cap it lit the cap, outward of the pane the pane shadowed the wall, under the plate the plate glowed)". **Acceptance (all three dusk views, same eyes):** `DUSK_CAP` p90 <= 120 on every cap *and* plate box of fix 2; `DUSK_BLOBS outside 0`, `DUSK_ORANGE outside 0`, `hot_px 0`; `DUSK_POOL` frontage x 430-560, y 400-650 median L >= 60 and R - B >= +20; lamp-wall x 540-700, y 980-1080 median L >= 60 and R - B >= +30; frontage x 410-560, y 300-400 (the ashlar level with the pane, beside it) median L >= 40; pane L >= 200; `LIGHT_CHECK lights N on_glass N orphans 0 in_wall 0 unshadowed_wall 0` with N = `LAMP_FX`'s count; `LAMP_MATCH` and the full `LAMP_PIECES` line for `SM_SilverbrookTownhouse1`, `2`, `4` and the frontage house's mesh in the README. If after this a stone in the frontage pool still exceeds L 200, change `_point_light` line 177 `.17` to `.14` and recapture; nothing else. Fallback only if an iron face still reads p90 > 120 after the move (state the box): make `MI_SB_LanternGlow` translucent (`base_property_overrides`: `override_blend_mode` True, `blend_mode` `BLEND_TRANSLUCENT`; translucent sections cast no dynamic shadow) and put the light back at the socket — the buildings are Nanite (`ue-import-report.json`), so first prove on one townhouse that the translucent section still renders without `disallow_nanite` on the building.
2. **Dusk gate: the plate boxes, `WARM` side-b 0, `DUSK_POOL`** — Codex, CODE/LEVEL, `docs/evidence/graphics/2026-10-22-wp6-town-i/dusk-panes.json` and `check_dusk_blobs.py`. The pane box grown by 10 px hides the lit plate (every glowing row lies within 10 px of the pane), so the plate must be named. (a) In `dusk-panes.json` append to each `cap` list the bottom plate: frontage `[360, 412, 424, 438]`, side-b `[1452, 1502, 275, 288]`, lamp-wall `[440, 510, 1046, 1064]`; add a `pool` key: frontage `[[430, 560, 400, 650, 60, 20], [410, 560, 300, 400, 40, -999]]`, lamp-wall `[[540, 700, 980, 1080, 60, 30]]`, side-b `[]`; `_about`: "'cap' is every iron that must stay dark (cap and bottom plate, DUSK_CAP p90 <= 120); 'pool' boxes [x0, x1, y0, y1, min median L, min median R - B] must be lit (DUSK_POOL)". (b) In `check_dusk_blobs.py` set `WARM = {'house-frontage-dusk': 3000, 'context-lamp-wall-dusk': 3000, 'context-house-side-b-dusk': 0}` with the comment "side-b's lantern lights a face the eye cannot see and the soffit is in the plate's shadow; the view is judged on the lantern itself", and after the `DUSK_CAP` loop add
   ```python
       for c in p.get('pool', []):   # [x0, x1, y0, y1, min median L, min median R - B]: the wall the lantern lights
           sub = a[c[2]:c[3], c[0]:c[1]]; mL = float(np.median(L[c[2]:c[3], c[0]:c[1]])); mRB = float(np.median(sub[..., 0] - sub[..., 2]))
           print('DUSK_POOL', view, c[:4], 'L', round(mL), 'R-B', round(mRB)); bad += int(mL < c[4] or mRB < c[5])
   ```
   and the docstring line "DUSK_CAP: p90 L of each cap box <= 120." -> "DUSK_CAP: p90 L of each cap and plate box <= 120. DUSK_POOL: median L and R - B of each pool box at or above its minimums." **Acceptance:** on the r10 frames the script prints `DUSK_CAP` p90 > 200 for the three plate boxes and `DUSK_POOL` lamp-wall L about 40, R - B about -10 (exit 1); on the fix-1 recaptures exit 0.
3. **`lamp-sockets.json` must be what `extract_sockets.py` writes** — Engineer, CODE, `GameClient/ArtSource/VFX/StreetLamp/extract_sockets.py`. Pipeline step 2 derives the json from the script; after r10 the script re-adds the two WeaponsVendor sockets and (README) 12 entries for other meshes that were dropped by hand, so the committed file is unreproducible and `vfx.md` documents the trap ("re-running `extract_sockets.py` re-adds them"). In `build()` add before the loop `SKIP = {'SM_SilverbrookWeaponsVendor': 'its *Glow islands are the vendor sign, 208 cm from any lantern-sized piece (WP6i r9-r10)'}` and inside it, after `manifest = ...`, `if manifest['unreal_mesh'] in SKIP: continue`. Run `py -3 GameClient/ArtSource/VFX/StreetLamp/extract_sockets.py`, commit the result, then `lamp_fx.apply()` and read `LAMP_MATCH` / `LAMP_NOMATCH` for the 12 re-added entries: a `LAMP_NOMATCH` mesh goes into `SKIP` with its reason and the json is regenerated; a matched one keeps its light (the `LAMP_FX` count rises and is stated). Replace the `vfx.md` clause "`SM_SilverbrookWeaponsVendor`'s two sockets were removed from the JSON for that reason, so re-running `extract_sockets.py` re-adds them" with "`extract_sockets.py` `SKIP` lists meshes whose glow islands are not lanterns (`SM_SilverbrookWeaponsVendor`: its sign)". **Acceptance:** `py -3 extract_sockets.py && git diff --exit-code GameClient/ArtSource/VFX/StreetLamp/lamp-sockets.json` passes; the README names the 12 entries and their match result; `LAMP_NOMATCH` absent from the apply log.
4. **Plume evidence from the committed frames** — Codex, data, `docs/evidence/graphics/2026-10-22-wp6-town-i/`. `plume-measure.json` holds r9 numbers for frames that were replaced in r10. Every resubmit that touches `chimney-smoke*.png` or `motion-*.png` runs `py -3 measure_plume.py` and commits the json it writes. In the fix-6 capture run, recapture the chimney set (ref + 14 motion frames; `capture_game.py` unchanged) and measure; report the seven r7/r8 bands in the README's round-11 table with the values the committed frames give. If growth at y 300 is still < 2.0 or any motion frame has a thin row, list them as open items against the r7/r8 bands, not "unchanged". Note the `motion-times.json` gap (0.068 s between frames 02 and 03) and whether it recurs. **Acceptance:** the committed `plume-measure.json` reproduces from the committed frames (`py -3 measure_plume.py` leaves `git diff` empty); the README table matches it.
5. **Lamp family** — owner decision (r1 fix 9), unchanged. Listed, not scored.
6. **Resubmit** — Engineer. `lamp_fx.apply()` + `check_lights.py` (fix 1), `extract_sockets.py` (fix 3), `check_dusk_blobs.py` with the new json (fix 2), then the capture run with the r4 fix-2 command line (townsfolk off): the three dusk views and the chimney set at minimum; noon bookmarks and context views only if any asset other than the map's lamp actors changes. Revert every `.uasset` the editor resaves except `MI_SB_LanternGlow`. README round 11: the `LAMP_MATCH` / full `LAMP_PIECES` / `LAMP_NOMATCH` lines, `LAMP_FX`, `LIGHT_CHECK`, the full `DUSK_*` output including `DUSK_CAP` for every cap and plate box and `DUSK_POOL`, the plume table (fix 4). A value outside a band is a known open item, not a pass; a file the README says changed must be in the commit; a measurement file must come from the committed frames.
