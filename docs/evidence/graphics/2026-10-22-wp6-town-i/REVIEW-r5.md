# WP6i round-5 review — Silverbrook town, pass 9 r5 (REVIEW-r4.md fix list 1-9)

Reviewer: independent (Fable), did not build the work. Judged per `docs/visual-review.md` against `Silverbrook-Design-Board-v1.webp` and `UI/v5/gameplay-hud.png` (mill plume x 1330-1360, y 100-170 re-measured: mean RGB 144/143/152, L 144, 90th-percentile L 190; the trees behind it 100/110/123 (L 108), the ridge 110/108/115 (L 110), the sky at the top of the frame L 147: a pale, cool wisp as bright as the sky and lighter than everything else behind it), the r4 fix list and its acceptances, the r1-r4 and WP6h standing acceptances, and the owner's four reports. Commit 0cc38edf. Every capture read at full resolution plus 2-4x crops and pixel measurements (`py -3`, PIL/numpy/scipy): signed `L - L_ref` over the chimney pair and all 14 motion frames, plume-pixel L/ref percentiles by background band, connected-blob search in the people box, dusk sky / ground / stoop / step / soffit / pane / window / clip statistics with x3 boosted crops, neutral-grey blob search in the SE overview, 16-px block-sd smooth band per column in the NW overview, far-thread columns against the sky in `main-street`, `market` and `-approach`, and the r4 captures (commit 1addd44d) for before/after. README numbers were checked, not trusted; where they differ it is stated.

**Result: 13 PASS / 6 FAIL.** Bookmarks 2 / 3, dusk lamp views 1 / 2, context views 10 / 1.

Fixed this round and holding: **the townsfolk are gone from every plume frame** (no blob of 30 px or more with |dL| >= 8 against the reference in x 1100-1900, y 780-1080 in `chimney-smoke.png` or any of the 14 motion frames; the residual max |dL| 26-58 is single-pixel TSR shimmer on barrel and roof edges, no mannequin anywhere), **the three claim-pad slabs are gone** (`se_east_3x`: pine shadows and forest floor at x 1625-1690, y 735-760 and x 1735-1790, y 745-770, no straight-edged neutral-grey blob in x 1500-1920, y 640-860), **the backdrop dome is out of the NW overview** (`nw_bottom_2x`: grass, the Westfields roads, the river and a bridge fill the bottom; the 16-px smooth band over x 800-1072 is the meadow itself, verified by crop) and **out of `context-house-side-a`** (top-right x 1650-1920, y 0-260 is sky and a pale distant range), **the windows read at dusk** (bright-half L 117 / 115 / 115, inside 100-220), **the side-b dusk floor is lit** (bottom-quarter median 8.2), **lantern panes stay at 238, no clipping within 1 m** (0.0 / 0.0 / 0.14 %), **the street lamp lights its cobbles** (median 28.8), **no stones on paved ground, no flame at noon, plinths, frontage rule, no lamps on the open road** all hold. Three r4 items are not met: (1) **the plume is still soot** — tint 0.10 moved the core from 0.71 to 0.76 of the field behind it (RGB-mean ratio, the r4 metric; L-ratio mean 0.789, median 0.746, 5th percentile 0.63), the plume stays brown over the field (R - B +27) and dark grey over the hill (0.845) and the sky (0.918), and every far chimney is a dark-grey ribbon (`main-street` x 1385-1425 darkest 0.59 of the sky, `market` x 1635-1665 0.58, `-approach` x 530-580 10th percentile 0.66); the concept's wisp is at the sky's brightness and lighter than the trees; the builder's own README numbers (core 0.819-0.888, p5 0.665, R - B 27.3, hill 0.814-0.889) are outside every band of r4 fix 1 and were submitted anyway; (2) **the frontage stoop is still black at dusk** — bottom-quarter median 4.3 (>= 8), the steps x 100-700, y 880-1080 median 4.3 (>= 6); sky light 1.0 -> 3.0 changed the steps by 0.3 L (r4 4.0), i.e. the sky light is not reaching the scene at all (below); (3) **the orange panel under the lamp-wall window is still there** — x 760-1180, y 590-690 mean L 43 (<= 25), max 207, a trapezoid whose hot edge is the window sill (x 870-1000, y 525-545) and which fades downward: a point light sits at the first-floor sill with no lantern body there, which is the owner's "orange light with no visible source" in a new place.

## Owner reports

| Report | Status | Evidence |
|---|---|---|
| Floating houses | Fixed (WP6h), holds | `foundation-close`, `context-house-side-a/b/rear`: plinth under every wall, buried, no gap. |
| Orange light with no visible source | Fixed at the lanterns; **recurs at the lamp-wall sill** | `context-house-side-b-dusk`: the wash on the ashlar has its lantern (pane 237.5). `context-lamp-wall-dusk` x 760-1180, y 590-690: an orange trapezoid lit from the window sill, 4 m above the only lantern in frame (x 445-495, y 935-1020). |
| Street clutter in mirrored rows on an empty approach road | Fixed (r1), holds | `context-road-approach.png`: groups only in front of houses, no twin across the road, irregular gaps. |
| White street | Holds | Tan cobbles with joints kerb to kerb in every street view; `main-street` carriageway unchanged from r4 (mean |d| 0.3 over the frame). |
| Lamps lit at noon (J5) | Fixed, holds | Every noon lantern dark (`context-lamp-wall` 0 warm pixels; side-a/b/rear iron lamps gilt in sun, no flame). |

## Round-4 fix items 1-9: met?

| Item | Verdict | Measured (this review) |
|---|---|---|
| 1 Pale plume: tint 0.10/0.10/0.13 | **Not met** | Core (y 380-540, x 880-1060) L/ref p5 / p50 / mean 0.63 / 0.746 / 0.789 (band 0.84-0.92, min/mean/max inside); 5th-percentile L/ref in x 760-1200, y 0-560 0.67 (>= 0.72); field plume R - B +27.4 (<= 10; plume 124/112/97 against 160/147/115); hill (y 60-300) p50 0.845 (0.86-0.95); sky rows (y < 60) 0.918 (0.85-0.97, met); far threads dark (above). Continuity holds (0 rows in y 150-560 with < 5 columns, all 15 frames); growth ratio within x 760-1100: y 550 span 926-1098 (172, > 140) against y 150 span 859-1130 (271): 1.58 (>= 2.0 not met; the base is already a 1.3 m column). Motion 89.5 px/s up, drift 3.1, MAD min 2.31: met. |
| 2 No townsfolk in the plume frames | **Met** | `WP6_TOWNSFOLK_OFF` in the run; no mannequin in `chimney-smoke`, `-ref` or any motion frame (no blob >= 30 px at |dL| >= 8 in the people boxes; residual single pixels 26-58 are TSR edge shimmer). The literal "< 6" is not reachable with TSR; the intent is met. |
| 3 Claim-pad slab hidden in game | **Met** | `SetActorHiddenInGame(true)` after the tag (`SFConstructionNetwork.cpp` 968); SE overview x 1500-1920, y 640-860 has no neutral-grey straight-edged blob; 33 construction tests passed per README (not re-run here). |
| 4 Night row sky 3.0 / moon 2.5 | **Not met for the frontage** | Sky top 60 rows L 16.9 / 16.8 / 13.4 (10-60, met). Bottom-quarter median 4.3 / 8.2 / 13.9 (>= 8: frontage fails). `house-frontage-dusk` stoop x 0-700, y 700-1080 median 11.9 (met), steps x 100-700, y 880-1080 median 4.3 (>= 6 fails), open street x 0-400, y 560-800 25.1. Clip within 1 m 0.0 / 0.0 / 0.14 % (met). Sky 1.0 -> 3.0 moved the steps 4.0 -> 4.3: the sky light contributes nothing (fix 2). |
| 5 No point light on window panes | **Half met** | `_lantern()` filter in place (`lamp_fx.py` 58-60); the light count stayed 589, so no LitGlass/Window slot ever had a light. The panel under the lamp-wall window is unchanged: x 760-1180, y 590-690 mean L 43 (<= 25 fails), 0.0 % > 235 (met), max 207; the hot edge is the sill (fix 3). |
| 6 Windows 0.1 | **Met** | Window glass bright-half L 116.8 (frontage bay) / 114.7 (side-b) / 114.7 (lamp-wall upper), inside 100-220; clip rule holds. |
| 7 Backdrop domes 1200-2000 m, scale <= 2 | **Met** | `BACKDROP_DOME_MIN_DIST 834 m` (README; the json values are in). NW overview bottom is terrain (crop); `context-house-side-a` top-right is sky and distant hills; the wall extremes are inside both overview frames. |
| 8 Lamp family | Owner decision, open | Unchanged. |
| 9 Resubmit | Met in form | All views, ref, motion set, perf pair present (Niagara GT 0.125-0.155 ms, workers 0.32-0.38 ms, on - off -0.004 / -1.23 ms: noise); gates listed. |

## main-street — FAIL

Same street as r4 (mean |d| 0.3 over the frame): cobbles kerb to kerb, five house tones, barrels, crates, hay cart, bench, ladder, signs, lamps and windows dark at noon, weeds at the kerb, nothing floating, no placeholder material.

1. **What's wrong:** the far chimneys are dark-grey soot ribbons on the sky (`ms_sky_2x`): x 1385-1425 is a 40-column ribbon from the pot at y 200 to the top of the frame, darkest pixel 0.59 of the sky beside it (175), 10th percentile 0.68, mean RGB 144/144/152 against sky 170/172/180; the left chimney at x 555-575, y 50-230 is the same; x 1080-1090 at y 330-390 a third. The design board's chimney wisps are pale against the sky; the HUD plume is at the sky's brightness. A player reads these as forges or fires. **Fix type:** LEVEL. **Exact instruction:** fix 1.

## market — FAIL

Stalls with awnings, firewood sheds, ladder, sacks, mixed house tones, cobbles with earth margins, far houses on their plinths on the green (stall stand-in: GFX-03, deferred).

1. **What's wrong:** the chimney at x 1635-1665, y 150-290 puts a dark-grey ribbon on the blue sky: 30 columns, dark in 65 % of rows, darkest 0.58 of the sky (147), mean RGB 110/120/135. Same defect as main-street; r4 let it pass at the darker tint and should not have. **Fix type:** LEVEL. **Exact instruction:** fix 1.

## house-frontage — PASS

Bay window with planter, boot sign, bracket lantern dark at noon, stoop with tub, ivy, plinth; unchanged. Seen, not failed: the kerb-high bank at x 0-400, y 600-820 as cracked mud (J2, deferred).

## foundation-close — PASS

Rubble plinth under the ashlar on both faces of the corner, flush, buried; tub and planter on the stoop. Unchanged.

## chimney-smoke — FAIL

Measured on `chimney-smoke.png`, `-ref.png` and `motion-00..13.png` (pots x 885-920, y 575-620; 131.7 px per m).

| Acceptance (r4 fix 1 / 2) | r5 | Met |
|---|---|---|
| core L/ref 0.84-0.92 over y 380-540, x 880-1060 (min / mean / max) | p5 0.63, p50 0.746, mean 0.789 | No |
| 5th-percentile plume L/ref, x 760-1200, y 0-560 >= 0.72 | 0.67 | No |
| field plume mean R - B <= 10 | +27.4 | No |
| hill (y 60-300) 0.86-0.95 | p50 0.845 (plume 128/131/141 on 150/158/169) | No |
| sky rows (y < 60) 0.85-0.97 | 0.918 | Yes |
| continuity: no 5-px row in y 150-560 with < 5 plume columns, 15 frames | 0 rows | Yes |
| `main-street` thread darkest L / sky >= 0.75, B >= R | 0.59; B > R (152 vs 144) | Half |
| `-approach` x 530-580, y 50-250: dark in >= 80 % of rows, darkest L / sky >= 0.75 | 100 %; p10 0.66 | Half |
| growth ratio >= 2.0 | 1.58 (172 -> 271 within x 760-1100) | No |
| people boxes: no mannequin | none; max |dL| 26-58 single pixels | Yes |
| motion >= 20 px/s up, MAD >= 2 | 89.5; 2.31 | Yes |

1. **What's wrong:** the plume is a brown-black column from the pot (`cs_core_3x`: RGB 124/112/97 over a field of 160/147/115) into a dark-grey cloud over the hazy ridge (`cs_top_2x`: 128/131/141 over 150/158/169). Nowhere is it lighter than what is behind it; 49 % of its pixels are under 0.85 of their background and 2.4 % over 1.0. Tint 0.06 -> 0.10 moved the field ratio 0.71 -> 0.76: the lit term is linear in tint, so the band (field parity or paler, sky parity) sits near tint 0.4, not 0.1. The r1 value 0.24 looked invisible only because its per-puff opacity was 0.19; at opacity 0.7 the same tint is a visible grey. **Fix type:** LEVEL. **Exact instruction:** fix 1.
2. **What's wrong:** the column is 1.3 m wide at the pot (span 172 px at y 550 against the 30-140 px band) and only 1.6x wider 3 m up; the concept's wisp starts thin and spreads. Not failed on its own (the treatment is the blocker); it is the second step of fix 1 so the tint calibration is not confounded.

## house-frontage-dusk — FAIL

Pane x 365-415, y 365-425 bright-half L 238 (met). Sky 16.9 (met). Bay panes 117 (met). Clip 0.0 % (met). The open street at the left is moonlit (L 25) and the facade reads, but everything in the house's own moon shadow is black: bottom-quarter median 4.3, the steps x 100-700, y 880-1080 4.3, the planter and bay base under L 8 (`house-fronta_x3boost`: the steps are a black mass even at x3). 52 % of the frame is under L 8. **Fix type:** CODE + LEVEL. **Exact instruction:** fix 2.

## context-house-side-b-dusk — PASS

Pane 237.5; the wash on the ashlar has its lantern and no clipping (0.0 % within 1 m); the iron street lamp's panes glow (max 219 / 203) and pool on the cobbles (median 28.8); window glass 115; sky 16.8; bottom-quarter median 8.2; the bench casts a moon shadow. Seen, not failed: the dark chimney column at x 150-180, y 30-330 against the night sky is right at night (fix 1 makes it pale by day only).

## context-lamp-wall-dusk — FAIL

Lantern x 445-495, y 935-1020 L 237.8; within 1 m 0.14 %. Sky 13.4. Bottom-quarter median 13.9. Window glass 115 (met).

1. **What's wrong:** the plaster panel between the first-floor window sill and the jetty beam, x 760-1180, y 590-690, is a saturated orange trapezoid (mean L 43, max 207; `context-lamp_x3boost`); its hot edge is the sill at x 870-1000, y 525-545 and it fades downward, so its source is a point light at the sill, 4 m above the only lantern in frame. The window emissive doubled this round (glow 0.05 -> 0.1) and the panel changed 0.25 points, so it is not the emissive; the builder's note says it is "a lantern socket": a `*Glow` chamber in the townhouse mesh at the sill that `glass_sockets` turned into a 27-lm light 30 cm in front of the wall, with no lantern body. A light with no visible source. **Fix type:** CODE. **Exact instruction:** fix 3.

## Context views

### context-overview-se — PASS

Eye 230 m up, 848 m out: the whole town, forest east and south-east, roads out of each gate, wall extremes inside the frame. The three claim-pad slabs are gone (`se_east_3x`). Seen, not failed (deferred, unchanged): the wall as a lake causeway, the straight canal and its grey cut banks at x 1230-1450, y 880-1000, flat cyan water, the white faceted mountain ring behind (J1b; its silhouette shows polygon steps at 2x, `cs_top_2x`).

### context-overview-nw — PASS

The r4 frame; the near dome is gone and the bottom of the frame is the Westfields meadow with its roads, the river and a bridge (`nw_bottom_2x`). Seen, not failed: the smooth white dome at x 0-260, y 0-210 is now at its 1.2 km ring (J1b).

### context-road-silvermere — PASS

The V cutting, pines on both crests, shrubs and tufts down both banks, cobbles only on the carriageway, dark earth verges. Unchanged. GFX-03 / J2 / J1b items as before.

### context-road-everwood — PASS

Wide cobbled road into the pines, irregular scrub, dark rocks on the left margin, tufts; no stones on the carriageway. Lollipop broadleaf and the white range: GFX-03 / J1b.

### context-road-valemere — PASS

Trees, shrubs, the boulder on the verge, striped fields at the left; no stone sets on the road. Unchanged.

### context-town-edge — PASS

Wall segments and towers stepping with the ground, the far wall and the road out; nothing floats, no lamps on the open road. Unchanged.

### context-house-side-a — PASS

Side wall on its plinth, fence, garden bed, iron lamp dark at noon. The dome edge at the top right is gone: sky and a pale distant range (`sa_topright_2x`).

### context-house-side-b — PASS

Plinth stepping to the stoop base; bracket lantern and iron lamp dark; forecourt and street clear of stones. Unchanged.

### context-house-side-rear — PASS

Rear plinth following the slope, firewood shelter, fence, lamps dark. Unchanged.

### context-lamp-wall — PASS

Gable facade in shadow at noon, lantern dark, no wash, windows dark, sign clear of the bracket.

### context-road-approach — FAIL

The owner's spot holds: both sides fronted by houses, groups irregular, no twin across the road, timber-post lamp dark, no stones on the carriageway, the chimney at x 545-565 one continuous ribbon. It fails on the ribbon's colour: dark pixels in 100 % of rows of x 530-580, y 50-250, 10th-percentile L 0.66 of the sky (152), mean RGB 120/129/144 against sky 140/157/175; a second dark thread at x 1440-1460, y 0-100 (`appr_sky_3x`). **Fix type:** LEVEL. **Exact instruction:** fix 1.

## Deferred (seen, not failed; GFX-02 / GFX-03 / owner, unchanged)

Bank texture streaks and bedrock faces on kerb-high banks (J2); the white faceted mountain ring and the far domes (J1b: the ring's polygon steps are visible at 2x in `chimney-smoke`, `main-street`, `-everwood`, the SE overview), 12 m cobble roads outside the wall (J1b); flat cyan water, lake causeway, canal and its cut banks, the zigzag fence lines on the Westfields field (GFX-02 world); ashlar repeats, plaster, stall stand-in, low-poly trees and shrubs, the same shrub mesh repeated (GFX-03); townsfolk mannequins (townsfolk prototype; off for the captures); street-lamp family (r1 fix 9, owner); the claim pad's final form (DEC-03 levelling).

## Fix list (shared first)

1. **Pale plume: calibrate the tint to the sky, not to charcoal** — Codex, LEVEL. `GameClient/ArtSource/Silverbrook/silverbrook-props.json` `chimney_smoke.tint.value` `[0.10, 0.10, 0.13]` -> **`[0.40, 0.40, 0.46]`** (bounds 0-1 unchanged; the 1.15 B/R cast cancels the warm sun that makes the 0.10 grey brown, R - B +27 today). Nothing else changes in step 1 (spawn 7, lifetime 4.5, rise 1.6, start 0.7, opacity 0.7, Curl 10, alpha keys). Push with `apply_dressing.py` (`dress_house_details` sets `User.Tint` per actor; no Niagara rebuild). Recapture `chimney-smoke` + `-ref`, run `measure_plume.py`, read the field ratio `r` = mean L/ref of plume pixels (|dL| >= 4) in y 380-540, x 880-1060. The lit term is linear in tint: r5 measured r = 0.789 at tint 0.10 (r4 0.771 at 0.06). If `r` is outside 1.00-1.15, set the final tint once by `t = 0.10 + (1.08 - 0.789) * (0.40 - 0.10) / (r - 0.789)` (keep B = 1.15 R), push and recapture; log both points in the README. Then step 2, only after step 1 passes: `start_size` 0.7 -> **0.5** (bounds 0.3-1.0; the r3 value that gave the thin base; spawn 7 /s at 1.6 m/s keeps 23 cm between 50 cm puffs, so continuity holds) and recapture. `docs/blueprints/vfx.md` line 27 values row: the final tint and start size (WP6i r5); line 31 acceptance: replace "plume L within 10 % of the sky ... never > 5 % brighter" with the bands below. **Acceptance** on `chimney-smoke.png` against `-ref.png`: core (y 380-540, x 880-1060) L/ref p5 >= 0.90, p50 and mean 1.00-1.15; hill (y 60-300) p50 0.95-1.15; sky rows (y < 60) p50 0.90-1.05 (at the sky's brightness, as the HUD plume); roofs (y 300-380, x 880-1060) p50 >= 1.2; field plume mean R - B <= 8; no 5-px row in y 150-560 with < 5 plume columns in x 880-1080 in all 15 frames; `main-street` x 1385-1425, y 0-190 and x 555-575, y 50-230: >= 10 columns at |sky - L| >= 5 and the 10th-percentile L / sky >= 0.90 with B >= R; `market` x 1635-1665, y 150-290 and `-approach` x 530-580, y 50-250 the same; after step 2, y 550 span within x 760-1100 <= 140 px and growth ratio >= 2.0; motion >= 20 px/s up, MAD >= 2.
2. **Night row: a sky light that reaches the scene** — Engineer, CODE + LEVEL. The steps moved 0.3 L for a 3x sky light, so the SkyLight is not contributing. `GameClient/ArtSource/Greenlands/lighting.py` lines 114-116: replace `sk = _one(u.SkyLight, at=(0, 0, 1000)).light_component` with
   ```python
   sky = _one(u.SkyLight, at=(0, 0, 1000))
   sky.root_component.set_mobility(u.ComponentMobility.MOVABLE)   # real-time capture needs a movable sky light (WP6i r5 fix 2)
   sk = sky.light_component
   ```
   keep `real_time_capture` True and the intensity line. `capture_game.py` line 208: after `s.light_component.set_intensity(NIGHT['sky'])` add `s.light_component.recapture_sky()` and extend the `WP6_NIGHT_ROW` log with `sky_mobility {s.root_component.mobility}`. Rerun `lighting.apply(save=True)`, recapture the three dusk views with `NIGHT = {'sun_pitch': 12., 'sky': 3.0, 'moon_lux': 2.5}`. If `house-frontage-dusk` steps x 100-700, y 880-1080 median is still < 6 with a movable sky light, set **`'sky': 6.0`** (bounds: `sun.sky_light_intensity` max 8) and recapture once; record both step medians in the README. `docs/blueprints/lighting-and-atmosphere.md` line 40: "sky light <value> real-time capture, movable". **Acceptance**, all three dusk views: sky top 60 rows L 10-60; bottom-quarter median >= 8; `house-frontage-dusk` x 0-700, y 700-1080 median >= 8 and steps x 100-700, y 880-1080 median >= 6; within 1 m of any lantern, outside the pane, <= 1 % > 235; panes >= 200; window glass 100-220; the `WP6_NIGHT_ROW` line shows `Movable`.
3. **No light at the lamp-wall sill** — Engineer, CODE, `GameClient/ArtSource/Greenlands/lamp_fx.py` `glass_sockets()`. (a) After `size = [...]` (line 98) add `u.log(f'LAMP_SOCKET {name} centre {[round(v) for v in c]} size {[round(v) for v in size]}')` and run `lamp_fx.apply()`; the mesh is the `context-lamp-wall` subject (the `_townhouse_` light owner `capture_game.py` picks; log its label as `WP6_LAMP_WALL_SUBJECT <label> <mesh>` in `capture_game.py` where the view is added, line 146). (b) The chamber at the first-floor sill is the one whose centre z is at the first-floor window's sill height for that mesh (about 4 m above the lantern's chamber); exclude it by the rule that fits the logged box: a horizontal strip (`size[2] < 5`) or a chamber higher than every bracket lantern (`c[2] > <lantern z> + 100`), written as one more condition on line 99's `continue` with a comment naming the mesh and the logged box, never a per-actor special case. (c) Rerun `lamp_fx.apply()` and `check_lights.py`: `LIGHT_CHECK lights` drops by the number of such chambers (below 589) and `orphans 0` holds. **Acceptance:** `context-lamp-wall-dusk` x 760-1180, y 590-690 mean L <= 25 and no pixel > 150; the sill x 870-1000, y 525-545 has no orange streak (mean R - B <= 20); lantern pane and pool as r4; `LAMP_SOCKET` lines and the removed chamber listed in the README.
4. **Street-lamp family** — owner decision (r1 fix 9), unchanged: twin-arm cast-iron standard vs `vfx.md` §6 plain post. Listed, not scored.
5. **Resubmit** — Engineer. `apply_dressing.py` (fix 1), `lighting.apply` (fix 2), `lamp_fx.apply()` + `check_lights.py` (fix 3), all gates, then the capture run with the r4 fix-2 command line (townsfolk off): the five views, `chimney-smoke-ref`, the 14 motion frames, the eleven context views, the three dusk views, `measure_plume.py` with the fix-1 bands added (core / hill / sky / roofs p50, R - B, per-thread 10th percentile), the 1440p perf pair. Report the README numbers against the bands; a value outside a band is a known open item, not a pass.
