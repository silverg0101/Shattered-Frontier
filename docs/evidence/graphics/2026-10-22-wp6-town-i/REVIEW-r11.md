# WP6i round-11 review — Silverbrook town, pass 11 r11 (REVIEW-r10.md fix list 1-6)

Reviewer: independent (Fable), did not build the work. Judged per `docs/visual-review.md` against `Silverbrook-Design-Board-v1.webp` and `UI/v5/gameplay-hud.png` (mill plume x 1330-1360, y 100-170), the r10 fix list and its acceptances, the r1-r10 and WP6h standing acceptances, and the owner's four reports. Commit 6487cd8a. Every capture read at full resolution plus 3-4x crops and pixel measurements (`py -3`, PIL/numpy/scipy) against the r10 frames pulled from `6487cd8a~1` (all 44 PNGs recaptured; per-frame mean abs diff 0.13-2.72, the only cells over 5 are the two moved lanterns and clouds). Reproduced in place or in a scratch copy: `check_dusk_blobs.py` (exit 1, output below, identical to the README table), `check_dusk_blobs.py --selftest` (`DUSK_SELFTEST orange patch caught True`), `measure_plume.py` on the committed chimney set (json identical to the committed `plume-measure.json`), `extract_sockets.py` (`38 lamp-bearing meshes`, `git diff --exit-code lamp-sockets.json` clean). The r11 diff of `lamp_fx.py`, `check_lights.py`, `extract_sockets.py`, `check_dusk_blobs.py`, `dusk-panes.json`, `vfx.md` read in full; `silverbrook_kit.lantern` (lines 337-358) for the geometry. The editor MCP was down (connection refused), so `LAMP_MATCH` / `LAMP_PIECES` / `LAMP_FX 731` / `LIGHT_CHECK` are the README's numbers, checked for consistency with the frames and the code, not reproduced. Working tree clean before and after the checks.

**Result: 19 PASS / 0 FAIL.** Bookmarks 5 / 0, dusk lamp views 3 / 0, context views 11 / 0.

Fixed this round and holding: **the plate no longer glows and the wall beside and below each lantern is lit.** Light at `socket - 30 cm z`, `+-12 cm` along the overhang axis (`lamp_fx.py` lines 64-65, 114-116, 135-137), i.e. 15.75 cm under the 19 cm bottom plate (`lantern`: plates at `body -+ 0.13`, 2.5 cm thick). Frontage plate box x 360-412, y 424-438 p90 L 94 (r10 237), row maxima y 425-439 L 54-105 with R - B negative from y 430 (blue-grey iron, no ellipse); lamp-wall plate x 440-510, y 1046-1064 p90 110, no pixel over L 134 below the pane's last row (r10 254, 237 px over 200); side-b plate x 1452-1502, y 275-288 p90 89 (r10 236). `hot_px 0`, `DUSK_BLOBS` / `DUSK_ORANGE outside 0` on all three; caps p90 82 / 21 / 49. Pools: frontage x 430-560, y 400-650 median L 98, R - B +78 (band 60 / +20; r10 -9) with the bracket's shadow in it; lamp-wall x 540-700, y 980-1080 L 86, +85 (band 60 / +30; r10 L 31, -25). Side-b: 224 warm px, a faint pool on the stoop corner (x 1431-1534, y 561-614, L 49-54, R - B +19-23), nothing hot. `dusk-panes.json` carries the plate boxes and pools, `check_dusk_blobs.py` prints `DUSK_POOL`, side-b `WARM` 0 (fix 2 met). `extract_sockets.py` `SKIP` and the regenerated json (fix 3 met: the file reproduces; the one re-added mesh is `SM_SilverbrookGildedTankard`, not placed in this map per the README; the r10 README's "12 new entries" was a miscount — the deterministic script adds exactly one mesh). `plume-measure.json` reproduces from the committed frames (fix 4 met in form). `vfx.md` line 17 and `check_lights.py` line 5 describe the r11 placement (fix 1 docs).

Open, not failing a view: (a) the committed gate exits 1 on its own accepted frames because the second frontage `pool` box x 410-560, y 300-400 (median L 33, band 40) is mostly the bracket's iron mount and the stone in its shadow — x 410-440 median L 12 (the mount block, `lantern`: 8 x 8 x 36 cm at `base - 0.12`), x 440-500 L 25 with R - B -20 (behind the mount and arm, above the light's plane), x 500-560 L 60 with R - B +22 (lit stone). The light cannot reach iron-covered or occluded stone; the box must sit on the stone (fix 1). (b) Plume growth at y 300 = 155 / 84 = 1.85 (band >= 2.0) on a single random frame: r9 2.52, r10 1.76, r11 1.85 with the Niagara parameters unchanged, so the band measures puff placement, not the asset (fix 2). (c) README evidence gaps against the r10 fix-1 acceptance: `LAMP_PIECES` given with ellipses, the frontage house's mesh not named (fix 3).

## Owner reports

| Report | Status | Evidence |
|---|---|---|
| Floating houses | Fixed (WP6h), holds | `foundation-close`, `context-house-side-a/b/rear`: plinth under every wall, buried, no gap; frames differ from r10 by 0.14-0.85 (exposure). |
| Orange light with no visible source | Fixed (r11) | Every lit pane has a dark cap and a dark plate; the only sources in the dusk frames are the panes (frontage L>235 px 1664 = the pane's 1664; lamp-wall 2602 = 2602; side-b 1327 = 1322 + 5 at the pane's edge). The pools sit beside and below their lanterns with the bracket's shadow in them. |
| Street clutter in mirrored rows on an empty approach road | Fixed (r1), holds | `context-road-approach`: groups only in front of houses, no twin across the road, irregular gaps. |
| White street | Holds | Tan cobbles kerb to kerb in every street view. |
| Lamps lit at noon (J5) | Holds | Every lantern dark in the noon views (`context-lamp-wall` pane cream in sun, `context-house-side-b` lantern dark, `house-frontage` lantern dark). |

## Round-10 fix items 1-6: met?

| Item | Verdict | Measured (this review) |
|---|---|---|
| 1 Light below and outside the plate | **Met** | `PLATE_DROP` 30, `PLATE_CLEAR` 12, overhang axis from the mesh bounding box, exactly the r10 code. Plate p90 94 / 110 / 89, cap 82 / 21 / 49 (band 120); `hot_px` 0; blobs / orange 0; pools frontage 98 / +78 and lamp-wall 86 / +85 (bands met); pane L 240 / 239 / 240. Open band: frontage x 410-560, y 300-400 median L 33 (band 40): iron and occluded stone, see fix 1. `LIGHT_CHECK lights 731 on_glass 731 orphans 0 in_wall 0 unshadowed_wall 0 shadowed 586` is the README's (editor down). No fallback (translucent glass) needed. |
| 2 Dusk gate | **Met** | Plate boxes in `cap`, `pool` boxes with minimums, `DUSK_POOL` printed, side-b `WARM` 0, docstring updated; output identical to the README table. |
| 3 `lamp-sockets.json` from `extract_sockets.py` | **Met** | `SKIP` added at line 100 and used at line 111; `py -3 extract_sockets.py` leaves `git diff` empty; `vfx.md` names `SKIP`. README names the one re-added mesh and its (unplaced) status. |
| 4 Plume evidence | **Met in form**, one band open | `plume-measure.json` reproduces from the committed frames; thin rows 0 / 15 frames; MAD 1.41 / 1.55; dense puffs -3.4; core 0.959 / 1.008 / 1.038; hill 0.958-1.033; sky rows 0.966; frame dt 0.278-0.357 s (no stall). Growth 1.85 (band 2.0) listed open in the README. |
| 5 Lamp family | Owner decision, open | Unchanged. |
| 6 Resubmit | **Met**, evidence gaps | All views recaptured; assets changed: map and `MI_SB_LanternGlow` only. README round 11 carries `LAMP_MATCH` (three meshes), `LAMP_PIECES` truncated with ellipses, no frontage-house mesh (fix 3). |

## main-street — PASS

Tan cobbles kerb to kerb, five house tones, barrels, crates, hay cart, bench, ladder, signs; lamps and windows dark at noon; weeds at the kerb; far threads pale on the sky. Differs from r10 by 0.17 mean (exposure). Seen, not failed: the white faceted range (J1b, deferred).

## market — PASS

Stalls with awnings, firewood sheds, ladder, sacks, mixed house tones, cobbles with earth margins, far houses on plinths. Stall stand-in: GFX-03, deferred. Unchanged (0.15).

## house-frontage — PASS

Bay window with planter, boot sign, bracket lantern dark at noon (pane cream), stoop with tub, ivy, plinth; unchanged (0.13). Seen, not failed: the kerb-high cracked-mud bank at x 0-400, y 600-820 (J2, deferred).

## foundation-close — PASS

Rubble plinth under the ashlar on both faces of the corner, flush, buried; tub and planter on the stoop. Unchanged (0.14).

## chimney-smoke — PASS

A soft pale-grey wisp leaves the pot at x 860-940, widens over the ridge and the far houses, thins into the hazy hill; the weight of the HUD wisp; the x 650-1350 crops of `chimney-smoke`, `motion-00 / 07 / 13` differ in puff placement; the |excess| map shows a soft fan at the pot and a diffuse column, no rectangle, no straight edge over 100 px. The committed `plume-measure.json` is the measurement of the committed frames (re-run identical).

| Acceptance (r7 fix 4 / r8) | r11 frames | Met |
|---|---|---|
| densest puffs R - B <= 5 and >= -12 | -3.4 | Yes |
| core L/ref p5 >= 0.90, p50 and mean 1.00-1.15 | 0.925 / 1.008 / 1.038 | Yes |
| hill (y 60-300) p50 0.95-1.15 | 0.958-1.033 | Yes |
| sky rows (y < 60) p50 0.90-1.05 | 0.966 | Yes |
| growth >= 2.0 at y 300 | 155 / 84 = 1.85 | **No** (open; fix 2) |
| 0 thin rows, 15 frames | 0 in all 15 | Yes |
| MAD >= 1.0 | 1.41 min, 1.55 mean | Yes |

The growth miss is the third single-frame reading of an unchanged asset (2.52 / 1.76 / 1.85); the view reads as the r9 plume at 1x. Fix 2 makes the number measure the asset, not one frame.

## house-frontage-dusk — PASS

Bay panes lit (L 240), the lantern pane lit with a dark cap (p90 82) and a dark plate (p90 94; rows y 425-439 max L 54-105, blue-grey), the pool on the ashlar jamb x 430-560, y 400-650 median L 98, R - B +78 (p90 155, max 198: no clipped stone) with the bracket's shadow across it, the stone right of the sign lit (x 500-560, y 300-400 L 60, +22), the stoop, tub and planter readable (bottom quarter p90 60), moon shadow across the field, far cottages with lit windows, sky x 0-250 L 88. The stone directly behind the bracket mount at pane level is dark (x 440-500, y 300-380 L 25) because the 8 x 8 x 36 cm mount and the arm stand between it and a light 30 cm lower; at 1x it reads as the bracket's own shadow. No blob, no orange, no second source (`frontage-lantern` crop 3x).

Not a view defect, an evidence defect: the gate's second pool box is drawn over the iron (fix 1).

## context-house-side-b-dusk — PASS

Corner lantern lit (pane L 240), cap p90 49, plate p90 89 (rows y 277-291 max L 58-151, the 151 a one-row warm rim at y 279-280), no lit iron under it (`sideb-lantern` crop 4x). Window glass lit, the far street lamp's panes lit with a flame, the far house's windows and door lanterns lit, bench and clothes line with moon shadows, sky x 0-250 L 23, bottom quarter 19. The lantern's pool is on the stoop corner it faces (x 1431-1534, y 561-614, L 49-54, R - B +19-23), the only warm wall it can reach from this eye. No blob, no orange.

## context-lamp-wall-dusk — PASS

Townhouse1 gable, jetty, lit upper and bay windows, stag sign, sky L 88. The lantern at the foot of the wall: pane L 239, cap p90 21, plate p90 110 (rows y 1046-1067 max L 106-130 with R - B +145-166: a warm-lit underside seen from below, dimmer than half the pane, no clipping; r10 254). The wall beside and below the lantern is lit: x 540-700, y 980-1080 median L 86, R - B +85; row means at x 520-700 rise from L 38 at y 950 through 53 / 74 / 91 at y 990 / 1010 / 1030 to 112 at y 1070; the voussoirs are orange, the ashlar above the lantern's plane fades to blue (x 400-560, y 850-940 L 15). A lantern hung low on a gable lighting the arch under it (`lw-wide` crop 2x). No blob, no orange.

## Context views

### context-overview-se — PASS

Eye 230 m up: the whole town, forest east and south-east, roads out of each gate, wall extremes in frame; no claim-pad slabs. Unchanged (0.66). Seen, not failed (deferred): lake causeway, straight canal and cut banks, flat cyan water, white faceted ring (J1b, GFX-02).

### context-overview-nw — PASS

Westfields meadow, roads, river and bridges fill the bottom; the smooth white dome at x 0-260, y 0-210 is at its 1.2 km ring (J1b). Unchanged (0.69).

### context-road-silvermere — PASS

The V cutting, pines on both crests, shrubs and tufts down both banks, cobbles only on the carriageway, dark earth verges. Unchanged (1.43, clouds).

### context-road-everwood — PASS

Wide cobbled road into the pines, irregular scrub, dark rocks on the left margin, tufts; no stones on the carriageway. Lollipop broadleaf and the white range: GFX-03 / J1b. Unchanged (1.09).

### context-road-valemere — PASS

Trees, shrubs, the boulder on the verge, striped fields at the left; no stone sets on the road. Unchanged (1.36).

### context-town-edge — PASS

Wall segments and towers stepping with the ground, the far wall and the road out; nothing floats, no lamps on the open road. Unchanged (0.82).

### context-house-side-a — PASS

Side wall on its plinth, fence, garden bed, iron lamp dark at noon. Unchanged (0.56).

### context-house-side-b — PASS

Plinth stepping to the stoop base; stag sign, bracket lantern and iron lamp dark; forecourt and street clear of stones. Unchanged (0.85).

### context-house-side-rear — PASS

Rear plinth following the slope, firewood shelter, fence, lamps dark. Unchanged (0.84).

### context-lamp-wall — PASS

Townhouse1 gable in sun: stag sign, bracket lantern dark at noon (pane cream in sun), no wash, windows dark, sill plaster plain. The r10-to-r11 difference (1.90, max cell 11.4 in the sky) is clouds and exposure.

### context-road-approach — PASS

Both sides fronted by houses, groups irregular, no twin across the road, timber-post lamps dark, no stones on the carriageway; chimney threads pale on the sky. Unchanged (1.03).

## Deferred (seen, not failed; GFX-02 / GFX-03 / owner, unchanged)

Bank texture streaks and bedrock faces on kerb-high banks (J2); the white faceted mountain ring and the far domes (J1b), 12 m cobble roads outside the wall (J1b); flat cyan water, lake causeway, canal and its cut banks, the zigzag fence lines on the Westfields field (GFX-02 world); ashlar repeats, plaster, stall stand-in, low-poly trees and shrubs, the same shrub mesh repeated (GFX-03); townsfolk mannequins (off for the captures); street-lamp family (r1 fix 9, owner); the claim pad's final form (DEC-03 levelling).

## Fix list (shared first; evidence items, no view fails)

1. **Frontage pane-level pool box on stone, not on the bracket** — Codex, LEVEL, `docs/evidence/graphics/2026-10-22-wp6-town-i/dusk-panes.json`. The box `[410, 560, 300, 400, 40, -999]` covers the bracket mount (x 410-440, median L 12), the stone the mount and arm occlude from a light 30 cm below (x 440-500, L 25, R - B -20) and the lit stone (x 500-560, L 60, R - B +22); the gate exits 1 on the accepted frames. Replace it with `[500, 560, 300, 400, 40, 10]` ("the ashlar level with the pane, right of the bracket mount: lit from below at a grazing angle"). **Acceptance:** `py -3 check_dusk_blobs.py` on the committed r11 frames prints `DUSK_POOL house-frontage-dusk [500, 560, 300, 400] L 60 R-B 22` and exits 0; `--selftest` still exits 1 with `orange patch caught True`; README round 12 shows the exit-0 table.
2. **Growth measured on the motion set, not one frame** — Codex, CODE, `docs/evidence/graphics/2026-10-22-wp6-town-i/measure_plume.py`. `r6_growth_span_y300_over_y550_x760_1100` is one frame's chord ratio and has read 2.52 / 1.76 / 1.85 on three captures of an unchanged asset. Compute the same span ratio (same rows, same x 760-1100 window, same |excess| >= 4 against `chimney-smoke-ref.png`) on `chimney-smoke.png` and `motion-00..13.png`, write `r6_growth_span_y300_over_y550_x760_1100` as `[span300_mean, span550_mean, ratio_of_means, per_frame_ratios]`, and in the README's plume table report the ratio of means against the band (>= 2.0). If the ratio of means is under 2.0, it is a real open item against r7/r8 and the next round names the parameter (`silverbrook-props.json` `chimney_smoke` spread or the Niagara size-over-life keys) rather than recapturing. **Acceptance:** `py -3 measure_plume.py` leaves `git diff` empty after the commit; the README table carries the new number.
3. **README round-11 evidence** — Codex, data, `docs/evidence/graphics/2026-10-22-wp6-town-i/README.md`. Give the full `LAMP_PIECES` lines (no ellipses) for `SM_SilverbrookTownhouse1`, `2`, `4` and name the frontage house's mesh with its `LAMP_MATCH` / `LAMP_PIECES` lines, pasted from the apply log of the editor run that produced the committed map. **Acceptance:** the lines are present verbatim; no other README change.
4. **Lamp family** — owner decision (r1 fix 9), unchanged. Listed, not scored.
5. **Resubmit** — Codex. Fixes 1-3 touch no asset and no capture: no recapture; commit the json, the script, its regenerated `plume-measure.json` and the README. The views stand as passed on the r11 frames.
