# WP6i round-7 review — Silverbrook town, pass 9 r7 (REVIEW-r6.md fix list 1-5)

Reviewer: independent (Fable), did not build the work. Judged per `docs/visual-review.md` against `Silverbrook-Design-Board-v1.webp` and `UI/v5/gameplay-hud.png` (mill plume x 1330-1360, y 100-170: measured 143/143/152 overall, brightest 30 % 185/184/192, R - B -7), the r6 fix list and its acceptances, the r1-r6 and WP6h standing acceptances, and the owner's four reports. Commit 4552ab91. Every capture read at full resolution plus 2-4x crops and pixel measurements (`py -3`, PIL/numpy/scipy): connected bright-blob search (L > 200, >= 20 px; orange R > B + 80) in the three dusk views, step / bottom-quarter / sky medians, wall-wash columns, pane and 1 m pools, plume L/ref by band on `chimney-smoke.png` against `-ref.png`, densest-puff colour, far threads in `main-street`, consecutive mean abs diff over the 14 motion frames, and every `.uasset` blob in the commit compared with `af48e04f` by `git ls-tree`. README numbers were checked, not trusted.

**Result: 18 PASS / 1 FAIL.** Bookmarks 5 / 0, dusk lamp views 2 / 1, context views 11 / 0.

Fixed this round and holding: **the frontage stoop is lit** (steps x 100-700, y 880-1080 median 8.5, bottom quarter 8.8, x 0-700 / y 700-1080 median 33; r6 was 5.4 / 5.6 / 18.5), **the lamp-wall subject is pinned** (`TheCommons_townhouse_037`, SM_SilverbrookTownhouse1, same house at noon and dusk; the first-floor sill plaster reads mean L 19, max 33: no sill glow), **the plume is cooler** (densest puffs 141/132/134, R - B +7.7, r6 +20; core p5 / p50 / mean 0.914 / 1.035 / 1.051; hill 0.97; sky rows 0.968; roofs 1.10; motion MAD 1.33-1.70 in the r6 box, 0 thin rows), **the commit is clean** (43 of the 44 `.uasset` files are byte-identical to `af48e04f`; only `MI_SB_LanternGlow`, rebuilt by `lamp_fx.apply()`, differs; the map stays 98.3 MB), and the r5 / r6 passes hold (no townsfolk in any plume frame, claim pads hidden, domes out of the overviews, plinths, frontage rule, no stones on paved ground, no flame at noon, no lamps on the open road, sill chamber unlit).

Not met: **the flame still burns beside the lantern on `SM_SilverbrookTownhouse2`** — `context-house-side-b-dusk` x 1379-1429, y 212-225, the r6 bbox to the pixel (273 px orange, L max 233, 241/187/88), on bare ashlar at the midpoint between the stag sign and the lantern, with the wall wash brightest at x 1360 (117 px left of the lantern's centre column 1477) and the lantern pane a flat yellow slab. The r6 fix was applied as written for a lantern-sized chamber, but the builder's own `LAMP_SOCKET` log shows the chamber is **52 x 3 x 98 cm** on Townhouse2 and **50 x 2 x 95 cm** on Townhouse4: a 1 m tall, 3 cm thin glow slab is not a lantern pane, it is the sign emblem's *Glow slot (the stag / tankard emblem, 26 x 98 cm, 2 cm thin) merged with the lantern glass 45 cm away under the 60 cm merge gap. The chamber centre lands between sign and lantern, and the 30 cm "flush panel" offset puts the flame cards and light on the wall there. The r6 review named this branch ("if the chamber is not thin but wide, two pieces merged across the bracket") and the builder took the other one; the README's "both are 95-98 cm tall, so they count as flush panels" is the defect restated. Fix 1 removes the sign-sized piece before the merge; the same defect exists on every Townhouse2 / Townhouse4 sign-and-lantern corner in the town at night (the r6 lamp-wall subject, Townhouse4, had it at x 936-999), so it is the owner's "orange light with no visible source" across the town, not one house.

## Owner reports

| Report | Status | Evidence |
|---|---|---|
| Floating houses | Fixed (WP6h), holds | `foundation-close`, `context-house-side-a/b/rear`: plinth under every wall, buried, no gap; unchanged. |
| Orange light with no visible source | **Open: a flame card on the wall beside every Townhouse2 / Townhouse4 lantern** | `context-house-side-b-dusk` x 1379-1429, y 212-225 (`sb_lantern_4x`, `sb_wash_2x`): an orange dash on ashlar between the sign and the lantern; the chamber log says why (fix 1). Townhouse1 (`context-lamp-wall-dusk`) and the frontage lantern are clean: no bright blob outside their panes. |
| Street clutter in mirrored rows on an empty approach road | Fixed (r1), holds | `context-road-approach.png`: groups only in front of houses, no twin across the road, irregular gaps, lamp dark at noon. |
| White street | Holds | Tan cobbles kerb to kerb in every street view. |
| Lamps lit at noon (J5) | Fixed, holds | Every noon lantern dark (`context-lamp-wall`, `context-house-side-a/b/rear`, `house-frontage`). |

## Round-6 fix items 1-5: met?

| Item | Verdict | Measured (this review) |
|---|---|---|
| 1 Lantern FX at the glass | **Not met on Townhouse2 / Townhouse4; met on Townhouse1 and the frontage** | Side-b: orange blob 273 px at x 1379-1429, y 212-225 outside the pane (acceptance: none >= 20 px); wash brightest column x 1360 vs lantern 1477 (acceptance within 40 px); pane 240.8 (>= 200 met); within 1 m 0.0 % > 235 (met). Lamp-wall: no blob outside the pane, pane 239, within 1 m 0.0 %. Frontage: no blob outside the pane, pane 239.8. The "flame column L > 245 inside the pane" clause is not meetable: every lit pane and every bright blob in all three views tops out at L 240.8-241.0, the grade LUT's white ceiling; the clause is dropped (fix 1 acceptance). The gate delivered (`check_lights.py`, 3 cm rule for chambers under 60 cm) covers no townhouse lantern by the builder's own statement, because both chambers are >= 60 cm; after fix 1 every building chamber is lantern-sized and the same rule covers them all. |
| 2 Night row: moon 5 lux | **Met for the ground; sky band open** | Frontage steps 8.5 (>= 6), x 0-700 / y 700-1080 33 (>= 8), bottom quarter 8.8 (>= 8); side-b bottom quarter 21.2, lamp-wall 20.9; panes 239-241; window glass 86-98 with lattice bars in the box (115-117 without, r6). Sky top 60 rows: frontage x 0-330 **109** (band 10-60), side-b x 0-330 84 / x 1000-1900 21, lamp-wall 52. The sky is 4-5x brighter toward the sun's azimuth (sun 12 deg below the horizon: nautical twilight) and the frontage looks that way; the band was written for the dark quadrant. What a player sees is one coherent blue dusk with moon shadows and lit windows, so the band is revised, not the scene (fix 3). |
| 3 Cool the plume | **Met by eye; one band open** | Densest puffs (top 3 % of L - L_ref in y 560-760, x 880-1020, n 841) 141/132/134 on a 57/49/43 field: R - B +7.7 (band <= 5; r6 +20); lit colour with the 20 % background removed 162/153/156, R - B +6 (concept -7). Core 0.914 / 1.035 / 1.051 (met), hill p50 0.97 (met), sky rows 0.968 (met), roofs p50 1.10 (met at the bound), growth 2.12 at y 300 (met), 0 thin rows in 15 frames (met), MAD 1.33-1.70 (>= 1.0 met), 70 px/s up (met). `vfx.md` and `measure_plume.py` carry the r6 metrics. One tint step remains (fix 4). |
| 4 Pin the lamp-wall subject | **Met** | `WP6_LAMP_WALL_SUBJECT TheCommons_townhouse_037`, SM_SilverbrookTownhouse1 at noon and dusk (same gable, sign and door lantern in both frames); the plaster between the first-floor sill and the jetty beam mean L 19.0 (<= 25), max 33 (no pixel > 150 outside the panes); sill R - B: no orange anywhere on the wall (no orange blob outside the pane). |
| 5 Commit only the list | **Met** | `git ls-tree`: 43 `.uasset` blobs identical to `af48e04f`; `MI_SB_LanternGlow` differs (rebuilt by `lamp_fx.apply()`, listed in the README); `.umap` 98,268,143 bytes. |
| 6 Lamp family | Owner decision, open | Unchanged. |
| 7 Resubmit | Met in form | All views, ref, 14 motion frames (dt 0.29-0.31 s), perf pair at 1440p (Niagara GT 0.093-0.131 ms <= 0.2, workers 0.28-0.41 <= 0.6, on - off -0.154 / -0.031 ms <= 0.5, 9 active within 120 m, 0 at 225 m), `WP6_SIDE_B_SUBJECT` logged (`TheCommons_townhouse_129`, Townhouse2). |

## main-street — PASS

Unchanged street: tan cobbles kerb to kerb, five house tones, barrels, crates, hay cart, bench, ladder, signs, lamps and windows dark at noon, weeds at the kerb, nothing floating. Far threads: above the right roofs x 790-830 L/sky 0.98 (9 columns, pale, `plume-measure.json`); the thread at x 1395-1418 over the sky rows y 60-130 is 164/163/170 on sky 170/175/185 (p50 0.94, p10 0.873, B > R), over the white range it is a mid-grey ribbon darker than the range behind it (`ms_right_2x`), which is what grey smoke does against a white backdrop. Seen, not failed: the white faceted range (J1b, deferred); the thread is neutral grey on a blue sky and so reads faintly warm (R - B -6 on a sky at -14), as in r6.

## market — PASS

Stalls with awnings, firewood sheds, ladder, sacks, mixed house tones, cobbles with earth margins, far houses on their plinths; the far chimney thread at x 1635-1665 is a faint grey ribbon. Stall stand-in: GFX-03, deferred. Unchanged from r6.

## house-frontage — PASS

Bay window with planter, boot sign, bracket lantern dark at noon, stoop with tub, ivy, plinth; unchanged. Seen, not failed: the kerb-high cracked-mud bank at x 0-400, y 600-820 (J2, deferred).

## foundation-close — PASS

Rubble plinth under the ashlar on both faces of the corner, flush, buried; tub and planter on the stoop. Unchanged.

## chimney-smoke — PASS

Measured on `chimney-smoke.png`, `-ref.png` and `motion-00..13.png` (pots x 885-920, y 575-620).

| Acceptance (r6 fix 3) | r7 | Met |
|---|---|---|
| core L/ref p5 >= 0.90, p50 and mean 1.00-1.15 | 0.914 / 1.035 / 1.051 | Yes |
| hill (y 60-300) p50 0.95-1.15 | 0.970 | Yes |
| sky rows (y < 60) p50 0.90-1.05 | 0.968 | Yes |
| roofs (y 300-380) p50 >= 1.1 | 1.098 (1.10) | Yes, at the bound |
| densest puffs R - B <= 5 | +7.7 (lit colour +6; concept -7) | No (fix 4) |
| growth: y 300 span / y 550 span >= 2.0, <= 450 px | 140 / 66 = 2.12 | Yes |
| continuity: no thin 5-px row, 15 frames | 0 | Yes |
| MAD >= 1.0 in x 760-1200, y 100-760 | 1.33-1.70 | Yes |
| motion >= 20 px/s up, <= 60 drift | 70 up, 3.7 drift | Yes |
| far threads p10 >= 0.88, B >= R | x 790-830 0.98; x 1395-1418 0.873 (B > R) | At the bound |
| people boxes: no mannequin | none | Yes |

What a player sees (`cs_plume_2x`, `motion_trio`): a soft pale wisp leaves the pot, widens over the ridge and thins into a translucent veil over the hazy hill and the far row, different in every frame, matching the HUD wisp in weight and value. The one measurable remainder is hue: the densest puffs are 14 R - B units warmer than the concept wisp. **Fix type:** LEVEL. **Exact instruction:** fix 4 (one tint value; luminance unchanged).

## house-frontage-dusk — PASS

Lantern x 364-408, y 374-425 lit (L 240), no bright blob outside its pane anywhere in the frame (`hf_lantern_4x`), bay panes lit, the stoop, tub and planter readable (steps median 8.5, x 0-700 / y 700-1080 median 33), moon shadow of the house across the field, far cottages with lit windows, no clipping. Sky x 0-330 top 60 rows L 109: the view looks toward the sun's twilight (side-b reads 84 on the same side and 21 away from it), so the r6 band 10-60 does not describe this direction. The scene is one coherent blue dusk; the blueprint row is corrected (fix 3), no recapture.

## context-house-side-b-dusk — FAIL

Window glass lit, bottom quarter 21, the bench and clothes line cast moon shadows, the far street lamps and windows lit, nothing clipped (within 1 m of the lantern 0.0 % > 235).

1. **What's wrong:** an orange dash x 1379-1429, y 212-225 (50 x 13 px, 273 px over R > B + 80, L max 233, mean 241/187/88) sits on bare ashlar between the stag sign (right edge x 1352) and the lantern (pane x 1455-1500, y 228-276), 40 cm left of and 15 cm above the glass, with no bracket, glass or flame under it; the pane itself is a uniform slab (L 240.8, min 37 at the mullion) with no flame or halo; the wall wash peaks at x 1360, 117 px left of the lantern (`sb_lantern_4x`, `sb_wash_2x`). Same bbox as r6 and r5. The chamber log explains it: `SM_SilverbrookTownhouse2` chamber 52 x 3 x 98 cm at (-316, 156, 199) = sign emblem + lantern glass merged, centre between them, cards 30 cm in front of that point. **Fix type:** CODE. **Exact instruction:** fix 1.

## context-lamp-wall-dusk — PASS

Townhouse1 (`TheCommons_townhouse_037`), the same house as the noon `context-lamp-wall`: gable, jetty, lit upper windows, the stag sign and the door lantern (x 447-504, y 976-1046, L 240), lit arch stones below it, no bright blob outside the pane, within 1 m 0.0 % > 235, sky 52, bottom quarter 21; the first-floor sill plaster mean L 19, max 33 (r5 fix 3 acceptance met, first frame where it could be measured). The lantern's wall pool cannot be measured here: the sign board and the lantern hood stand between the camera and the wall at the pane's height (x 400-445 reads L 10 under the hood); the r6 column test is kept for side-b, where the wall is clear.

## Context views

### context-overview-se — PASS

Eye 230 m up: the whole town, forest east and south-east, roads out of each gate, wall extremes inside the frame; no claim-pad slabs. Unchanged. Seen, not failed (deferred): the lake causeway, the straight canal and its cut banks, flat cyan water, the white faceted mountain ring (J1b, GFX-02).

### context-overview-nw — PASS

The Westfields meadow, roads, river and bridges fill the bottom; the smooth white dome at x 0-260, y 0-210 is at its 1.2 km ring (J1b). Unchanged.

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

Plinth stepping to the stoop base; stag sign, bracket lantern and iron lamp dark; forecourt and street clear of stones. (The noon frame of the dusk subject; the sign emblem and lantern are 45 cm apart on the corner, which is the merge in fix 1.)

### context-house-side-rear — PASS

Rear plinth following the slope, firewood shelter, fence, lamps dark. Unchanged.

### context-lamp-wall — PASS

New subject (Townhouse1 gable in sun): stag sign, bracket lantern dark at noon, no wash, windows dark, sill plaster plain. Same house as the dusk frame.

### context-road-approach — PASS

Both sides fronted by houses, groups irregular, no twin across the road, timber-post lamp dark, no stones on the carriageway; the chimney threads pale on the sky. Unchanged.

## Deferred (seen, not failed; GFX-02 / GFX-03 / owner, unchanged)

Bank texture streaks and bedrock faces on kerb-high banks (J2); the white faceted mountain ring and the far domes (J1b), 12 m cobble roads outside the wall (J1b); flat cyan water, lake causeway, canal and its cut banks, the zigzag fence lines on the Westfields field (GFX-02 world); ashlar repeats, plaster, stall stand-in, low-poly trees and shrubs, the same shrub mesh repeated (GFX-03); townsfolk mannequins (off for the captures); street-lamp family (r1 fix 9, owner); the claim pad's final form (DEC-03 levelling).

## Fix list (shared first)

1. **Sign-sized glow pieces never join a lantern chamber** — Engineer, CODE, `GameClient/ArtSource/Greenlands/lamp_fx.py` `glass_sockets()`. The sign emblem's *Glow slot (26 x 98 cm, 2 cm thin) merges with the lantern glass 45 cm away, the chamber centre lands between them, and the "flush panel" offset puts the cards on the wall. (a) Filter pieces before the merge: replace lines 86-88
   ```python
            for comp in next(x for x in comps if isinstance(x, u.Array)):
                bb = u.GeometryScript_MeshQueries.get_mesh_bounding_box(comp)
                boxes.append([bb.min.x, bb.min.y, bb.min.z, bb.max.x, bb.max.y, bb.max.z])
   ```
   with
   ```python
            slot = mesh.static_materials[mid].material_slot_name
            for comp in next(x for x in comps if isinstance(x, u.Array)):
                bb = u.GeometryScript_MeshQueries.get_mesh_bounding_box(comp)
                b = [bb.min.x, bb.min.y, bb.min.z, bb.max.x, bb.max.y, bb.max.z]
                size = [b[i + 3] - b[i] for i in range(3)]
                u.log(f'LAMP_PIECE {name} slot {slot} centre {[round((b[i] + b[i + 3]) / 2) for i in range(3)]} size {[round(v) for v in size]}')
                if not name.startswith('SM_SilverbrookStreetLamp') and max(size) >= 60:
                    continue   # a sign emblem (26 x 2 x 98), a sill strip (204 x 19 x 2) or a lit room: never lantern glass (WP6i r7 fix 1)
                boxes.append(b)
   ```
   (b) Delete the flush-panel offset: replace lines 106-109
   ```python
            thin = min(range(2), key=lambda i: size[i])
            if size[thin] < 5 and max(size) >= 60:   # a flush glow panel (window-sized): light 30 cm in front of it, outward from the mesh bounds centre
                c[thin] += 30 if c[thin] >= centre[thin] else -30
            out.append(c)   # a lantern pane (every axis under 60 cm) keeps its cards and light at the glass centre (WP6i r6 fix 1)
   ```
   with
   ```python
            out.append(c)   # cards and light at the glass centre; every building chamber is lantern-sized after the piece filter (WP6i r7 fix 1)
   ```
   and drop the now-unused `box = mesh.get_bounding_box(); centre = ...` on line 73; keep line 102's room / grille / strip filter and the 60 cm merge gap (the lantern's own panes are within a few cm; the sign is gone before the merge). Update the docstring (lines 66-68). (c) `check_lights.py` needs no change: with every building chamber under 60 cm its 3 cm rule now covers every townhouse lantern; it must report `orphans 0 in_wall 0` and the light count. (d) Rerun `lamp_fx.apply()`, `check_lights.py`, recapture the three dusk views, and put in the README the `LAMP_PIECE` lines for `SM_SilverbrookTownhouse1`, `2` and `4` (slot name and box of every glow piece, so the sign slot is named) and the resulting `LAMP_SOCKET` chambers (expected: one lantern chamber per lantern, every axis under 60 cm). **Acceptance:** in all three dusk views no connected blob of >= 20 px with L > 200 outside the lantern pane and window panes (a blob counts as inside when its bbox lies within a pane's bbox grown by 10 px); `context-house-side-b-dusk`: the wall band y 100-600, x 900-1450 has its brightest column within 40 px of the lantern's centre column, and no pixel with R > B + 80 and L > 150 outside the pane in x 1300-1450; every lantern pane >= 200; within 1 m of each lantern, outside the pane, <= 1 % > 235; lights count and `orphans 0 in_wall 0` in the README. (The r6 "flame column L > 245 inside the pane" clause is withdrawn: the grade LUT tops out at 241.)
2. **Dusk capture gate** — Codex, CODE. Add `docs/evidence/graphics/2026-10-22-wp6-town-i/check_dusk_blobs.py` (PIL + numpy + scipy.ndimage, same style as `check_details.py`): for each of `house-frontage-dusk`, `context-house-side-b-dusk`, `context-lamp-wall-dusk` read the PNG, label connected pixels with L > 200 (L = 0.2126 R + 0.7152 G + 0.0722 B), and fail on any component >= 20 px whose bbox is not inside one of the pane boxes listed in a `dusk-panes.json` beside it (per view: lantern pane and window-pane boxes in 1920x1080 px, each grown by 10 px), printing `DUSK_BLOBS <view> outside <n> largest <x0,x1,y0,y1,count,L>`; also print the orange test (R > B + 80, L > 150, >= 20 px). Seed `dusk-panes.json` from the r7 frames (frontage lantern 364-408 / 374-425, bay 780-1430 / 230-700; side-b lantern 1455-1500 / 228-276, window 650-830 / 220-490, far windows x > 1700; lamp-wall lantern 447-504 / 976-1046, windows per the frame). Run it after fix 1 and paste its lines in the README. **Acceptance:** `outside 0` for all three views on the r8 captures.
3. **Night-row sky band by direction** — Engineer, doc only, no recapture. `docs/blueprints/lighting-and-atmosphere.md` line 40: replace "The row is right when the sky's top 60 rows read L 10-60 and the ground's bottom quarter has median L >= 8." with "The row is right when the sky's top 60 rows read L 10-60 away from the sun's azimuth and L 10-120 toward it (sun 12 deg below the horizon is nautical twilight: WP6i r7 frontage 109, side-b 84 left / 21 right, lamp-wall 52), the ground's bottom quarter has median L >= 8, and the frontage steps x 100-700, y 880-1080 read median >= 6 (r7: 8.5 at moon 5 lux, sky light 6.0 movable)." `capture_game.py` line 66 comment: append "r7: steps 8.5, frontage sky 109 (sun side)". **Acceptance:** the three r7 sky medians sit inside the written bands; no capture changes.
4. **Cool the plume one more step** — Codex, LEVEL. `GameClient/ArtSource/Silverbrook/silverbrook-props.json` line 383 `chimney_smoke.tint.value` `[0.28, 0.31, 0.40]` -> **`[0.25, 0.31, 0.43]`** (luminance 0.306; the r6 -> r7 tint step of R - B -0.07 moved the lit puff colour by -16 R - B, so -0.06 more lands the lit colour near the concept's -7 and the measured dense puffs, 20 % warm field included, at about -5). `docs/blueprints/vfx.md` line 27: the tint. Push with `apply_dressing.py`; recapture `chimney-smoke`, `-ref`, the 14 motion frames; rerun `measure_plume.py`. **Acceptance:** densest puffs R - B <= 5 and >= -12; core p5 >= 0.90, p50 and mean 1.00-1.15; hill 0.95-1.15; sky rows 0.90-1.05; roofs >= 1.1; growth >= 2.0 at y 300; 0 thin rows; MAD >= 1.0; up >= 20 px/s; far threads p10 >= 0.87 with B >= R.
5. **Lamp family** — owner decision (r1 fix 9), unchanged. Listed, not scored.
6. **Resubmit** — Engineer. `lamp_fx.apply()` + `check_lights.py` (fix 1), `check_dusk_blobs.py` (fix 2), `apply_dressing.py` + `measure_plume.py` (fix 4), then the capture run with the r4 fix-2 command line (townsfolk off): the three dusk views and the chimney set at minimum; the five bookmarks and eleven context views again only if any asset other than the map's lamp and chimney actors changes. Commit only the files the fixes touch (the r7 revert stands: every other `.uasset` stays at its `af48e04f` blob) and list in the README which assets each fix rebuilt. Report the README numbers against the bands; a value outside a band is a known open item, not a pass.
