# WP6i round-3 review — Silverbrook town, pass 9 r3 (REVIEW-r2.md fix list 1-9)

Reviewer: independent (Fable), did not build the work. Judged per `docs/visual-review.md` against `Silverbrook-Design-Board-v1.webp` and `UI/v5/gameplay-hud.png` (mill plume, x 1330-1360, y 100-170), the r2 fix list and its acceptances, the r1 and WP6h standing acceptances, and the owner's four reports. Commit 894a0ba7. Every capture read at full resolution plus 2-4x crops and pixel measurements (`py -3`, PIL/numpy): signed `L - L_ref` over the chimney pair and motion frames, per-row plume-column counts, street-thread columns and gap lengths, stone-pixel counts (L 60-110, sat < 25) in every r2 box, warm-pixel counts and their mean RGB in every lantern box at noon, dusk sky/ground/pane/clip statistics and x6/x12 boosted crops, 16-px block-sd smooth-band fraction per column in both overviews. README numbers were checked, not trusted.

**Result: 11 PASS / 8 FAIL.** Bookmarks 3 / 2, dusk lamp views 0 / 3, context views 8 / 3.

Fixed this round and holding: **no stones or rocks on any paved surface** (0 stone pixels in all eleven r2 boxes), **the Silvermere cutting is dressed**, **the gate is the default** (noon warm pixels in the iron lanterns are sunlit gilt, mean RGB 215/183/137 with B 137, not the 255/150/65 flame), **the plume is visible** (peak |dL| 28, a grey column from the pot to the frame top). Still failing on their own: the plume breaks into detached clusters and reads as a string of dots from the street (rows 100-125 and 380-400 have no plume at the stack; `context-road-approach` x 530-580 is seven separate 8-px dots), the townsfolk were not hidden (a mannequin walks through both plume frames, so the origin measure is still the street), the three dusk frames are black (sky L 4.0-6.5, ground median 4.0, 90 % of each frame under L 8; lantern panes 123-136 against >= 200), the three east plots are still three flat grey rectangles in the pines after the paint revert (so the paint was never the cause), and the NW overview's near ridge still fills the bottom centre (x 760-1560, up to 17.8 % of column height).

## Owner reports

| Report | Status | Evidence |
|---|---|---|
| Floating houses | Fixed (WP6h), holds | `foundation-close`, `context-house-side-a/b/rear`: plinth under every wall, buried, no gap. |
| Orange light with no visible source | Fixed in form; the night around it is black | `context-house-side-b-dusk.png`: pane at x 1455-1495 glows (L 123), wall within 1 m 1.1 % > 235 (r2 21.4 %), ashlar below 0.0 % (r2 12.5 %), wash mean RGB 46/22/5 (not saturated). Outside the pools the frame is black (91 % under L 8). |
| Street clutter in mirrored rows on an empty approach road | Fixed (r1), holds | `context-road-approach.png`: groups only in front of houses, no twin across the road. |
| White street | Holds | Cobbles tan/ochre in every street view. |
| Lamps lit at noon (J5) | Fixed | 0 warm pixels in the timber-post lamps (`-approach`, `-lamp-wall`); the iron-lamp warm pixels (1,543 / 136 / 94 in side-b / rear / side-a, unchanged by the gate) are the gilt lantern roofs and finials in sun: mean RGB 215/183/137, max R 241, B never below 83; the flame colour is 255/150/65. The r2 criterion (max > 200 and R - B > 60) counts gold; use R >= 240 and B <= 70 for a flame from here on. |

## Round-2 fix items 1-9: met?

| Item | Verdict | Measured |
|---|---|---|
| 1 No PCG stones on paved ground | **Met** | Stone pixels in every r2 box: `main-street` 0 / 0 / 6 / 74 of 1,740 (the 74 are cobble joints in shadow, no blob at 3x), `-approach` 0, `-side-b` 0 / 0 / 0, `-valemere` 1 / 24 (tufts, `vale_st` 4x) / 0, `-everwood` 1. `dress_zone.dress(..., paved=)` and the look-profile clearances did it. |
| 2 Iron lamps dark at noon | **Met** (gilt, above) | `always_lit_patterns` ["ForgeFire"]; `WINDOWS_GATED` includes the vendor glows and the watchtower flame. |
| 3 Visible plume | **Partly met** | Values applied (tint 0.11, start 0.5). Core L/ref over y 380-540: 0.971-0.999 in the fixed window x 840-960 (acceptance 0.65-0.88); 0.907 in the drifted core x 900-1050 where the plume actually is (wind 0.35 m/s puts the 3-6 m column at x 935-1050). Growth ratio 0.14 (>= 2.0). Origin at y 550: x 938-1427 (the mannequin and the far street again). `main-street` threads: x 790-830 3 columns, x 1085-1125 0 (>= 15). Townsfolk not hidden: a mannequin at x 1250-1300, y 860-930 in `chimney-smoke.png` and at x 1355-1375, y 870-900 in `-ref.png` (`cs_man` 3x). |
| 4 Dusk that reads as dusk, lanterns that do not clip | **Not met** | Clipping fixed: side-b wall 1.1 %, below 0.0 %, frontage jetty 0.0 %, lamp-wall soffit 5.8 % (r2 28.7 %; limit 1 %). Sky top 60 rows L 6.5 / 4.6 / 4.0 (10-60). Bottom-quarter median 4.0 / 4.0 / 4.0 (>= 8). Panes 136 / 123 / 132 (>= 200; r2 210). Street-lamp pool: cobbles under the side-b iron lamp x 1700-1830, y 560-700 L 4.0 flat. The exposure term is confirmed dead under `r.EyeAdaptationQuality 0`; fix 3 below lights the scene instead. |
| 5 Overview frame test | **Not met (NW)** | `context-overview-nw.png`: smooth near-ridge band from the bottom, 16-px block sd < 2: max 17.8 % of column height at x 1312, > 10 % over x 944-1344, the dome visible over x 760-1560, y 870-1080 (`nw_dome`). The 45 m hit-height rule admits it. Wall extremes inside the frame: yes. SE: no near ridge (the bottom is the lake and the causeway, deferred). |
| 6 East plots are not slabs | **Not met** | `context-overview-se.png` x 1420-1470, y 820-850; x 1540-1620, y 850-880; x 1680-1740, y 870-900 (`se_plots` 3x): three neutral-grey rectangles, RGB 119/123/122 and 100/107/109, sharp edges, no shadow, in a row among the pines (ground 134/133/112, tree shadow 76/90/100). The `FIELDS` revert changed nothing, so the paint was not the cause. |
| 7 Silvermere cutting dressed | **Met** | `context-road-silvermere.png`: 30+ pines and 20+ shrubs on both banks, a rock at x 1690-1730, y 150-200, tufts, nothing on the carriageway. Ground cover 0.1 (file-size cap) is fine. |
| 8 Lamp family | Owner decision, open | Unchanged. |
| 9 Resubmit | Met in form | All views, ref, motion set, perf pair present; perf on - off -0.18 / -0.05 ms. |

## main-street — FAIL

Same street as r2, now with the carriageway clear of stones; cobbles kerb to kerb, five house tones, barrels, crates, hay cart, bench, ladder, signs, lamps and windows dark at noon, weeds at the kerb line, nothing floating, no placeholder material.

1. **What's wrong:** the far chimneys still produce beads, not columns. Thread test over y 300-400: x 790-830 3 dark columns (max gap 69 px), x 1085-1125 0, x 1385-1425 6 (dark fraction 0.75, max gap 21 px). `ms_sky` 2x: the x 1400 column is 10 px wide and reads, the x 810 and x 1160 ones are strings of separate 10-px dots. Puffs 0.5 m wide spawned 0.48 m apart (2.4 m/s / 5 per s) do not merge until they have grown, and the curl noise (25) pulls them into separate clusters. **Fix type:** LEVEL. **Exact instruction:** fix 1.

## market — PASS

Stalls with awnings, firewood sheds, ladder, sacks, mixed house tones, cobbles with earth margins, far houses on their plinths on the green. Stall stand-in: GFX-03 (deferred).

## house-frontage — PASS

Bay window with planter, boot sign, bracket lantern dark at noon (19 warm pixels in x 365-415, y 365-425, mean B > 100: sunlit brass), stoop with tub, ivy, plinth. Seen, not failed: the kerb-high bank at x 0-400, y 600-820 renders as cracked bedrock (J2, deferred).

## foundation-close — PASS

Rubble plinth under the ashlar on both faces of the corner, flush, buried; tub and planter on the stoop. Unchanged.

## chimney-smoke — FAIL

Measured on `chimney-smoke.png`, `-ref.png` and `motion-00..13.png` (pots x 885-920, y 575-620; 131.7 px per m). The plume now exists: a grey column rises from the pot, drifts right with the wind to x 935-1050 by 3-6 m up, and darkens the hazy hill at x 920-1020, y 0-350 (`cs_signdiff`).

| Acceptance (r2 fix 3) | r3 | Met |
|---|---|---|
| 1 origin 30-130 px at y 550 centred in x 860-940, nothing outside x 760-1100 | x 938-1427, span 490 (mannequin and far street) | No |
| 2 growth ratio >= 2.0 | 0.14 | No |
| 3 no veil | 77 columns max in y 460-540, all at x > 931 | Yes |
| 4 density >= 12 / >= 6 | 28.0 / 3.6 | Half |
| 5 core L/ref 0.65-0.88 over y 380-540 | 0.971-0.999 at x 840-960; 0.907 at x 900-1050 (where the core is) | No |
| 6 puffs | left-edge sd 4.9 / 3.0 / 327 / 10.7 | n/a |
| 7 motion >= 20 px/s up | 102.2 px/s; MAD min 2.06 | Yes |
| 8 stack in frame, no card edges | yes | Yes |
| street thread >= 15 columns | 3 / 0 | No |

1. **What's wrong:** the column is not continuous. Rows with fewer than 5 plume columns in x 880-1080 (|dL| >= 4): y 100-125 and 380-400 in `chimney-smoke.png`; y 85-165 (80 px, a whole missing cluster) and 415-445 in `motion-03.png`; y 375-410 in `motion-10.png`. `cs_core` 2x shows the puffs over the field as three detached cauliflower clusters with clear field between them; from the street (`appr_sky` 3x, x 530-580, y 50-250) the same chimney is seven separate dots 8 px wide 25 px apart. The concept wisp is one soft continuous ribbon.
   **Fix type:** LEVEL. **Exact instruction:** fix 1.
2. **What's wrong:** the core over the field is still a faint haze: L/ref 0.907 at x 900-1050, y 380-540 (field L 137-154). The lit sprite at tint 0.11 under the same sun is about a third darker than the grass, and at opacity 0.5 the per-puff coverage is about 0.3, so the field shows through at 90 %. **Fix type:** LEVEL. **Exact instruction:** fix 1 (opacity 0.7, tint 0.06, overlap from the spacing change).
3. **What's wrong:** a UE mannequin walks through both frames (`cs_man` / `cs_man_ref` 3x): x 1250-1300, y 860-930 in the plume frame, x 1355-1375, y 870-900 in the reference. `WP6_PAWNS_HIDDEN 22` hid Pawns and `SFTownsfolkBody` at the chimney tick, but this walker is neither (or spawned after the tick), so item 1 reports the street and item 2's ratio is meaningless. **Fix type:** CODE. **Exact instruction:** fix 2.

## house-frontage-dusk — FAIL

Pane x 365-415, y 365-425 L 136 (>= 200 required; r2 210). Bay panes and far windows glow; the jetty top is clean (0.0 % > 235). Everything else is black: sky top 60 rows L 6.5, bottom quarter median 4.0, 90 % of the frame under L 8; the stoop, planter and street are not visible. **Fix type:** CODE + LEVEL. **Exact instruction:** fixes 3 and 4.

## context-house-side-b-dusk — FAIL

Pane L 123; the wash has a source and no longer clips (1.1 % > 235 within 1 m, mean RGB 46/22/5; 0.0 % below the lantern). The iron street lamp's two panes glow (x 1717-1737 and 1785-1805, y 342-372, L max 166-195 in a x12 boost) but light nothing: cobbles under it x 1700-1830, y 560-700 L 4.0 flat, because the lantern sits 3.2 m up and `light_radius` is 4 m. Sky L 4.6, ground median 4.0, 91 % under L 8. **Fix type:** CODE + LEVEL. **Exact instruction:** fixes 3 and 4.

## context-lamp-wall-dusk — FAIL

Lantern x 445-495, y 935-1020 L 132. The jetty soffit under the lit first-floor window, x 760-1180, y 590-690, is still 5.8 % > 235 (mean RGB 68/32/5, a saturated orange trapezoid; limit 1 %). Sky L 4.0, 90 % under L 8. **Fix type:** CODE + LEVEL. **Exact instruction:** fixes 3, 4 and 5.

## Context views

### context-overview-se — FAIL

Eye at 230 m, 848 m out; the whole town, the forest east and south-east (14,450 px under L 70), roads leaving each gate.

1. **What's wrong:** three flat neutral-grey rectangles in a row among the pines, x 1420-1470, y 820-850; x 1540-1620, y 850-880; x 1680-1740, y 870-900 (`se_plots` 3x): RGB 119/123/122 and 100/107/109 (the forest floor is 134/133/112, tree shadow 76/90/100), sharp straight edges, no cast shadow, each the plot's 20 m square foreshortened. They were the same at the r2 eye with the tilled paint and are the same without it, so the vertex-alpha paint was never the cause; what remains at exactly that square is the hidden `ConstructionPlot_*` cube (5 cm thick, `MI_Graybox_Plot`) and the PCG exclusion of `placed`. A player sees three concrete pads in a forest.
   **Fix type:** LEVEL + CODE. **Exact instruction:** fix 6.
Seen, not failed (world pass): the wall as a lake causeway at the bottom, the straight canal, flat cyan water, the smooth beige backdrop mountains (J1b).

### context-overview-nw — FAIL

Eye at 480 m, 615 m out; the town is inside the frame with its wall extremes clear of the edges (fix 5's second test holds), but the near ridge is a smooth green-beige dome over the bottom centre, x 760-1560, y 870-1080 (`nw_dome`): 16-px blocks with L sd < 2 run 17.8 % of the column height at x 1312 and more than 10 % over x 944-1344. The 45 m hit-height rule passes it because the dome's crest is under 45 m above the town centre; the acceptance was < 0.1 in every column. **Fix type:** CODE. **Exact instruction:** fix 7.

### context-road-silvermere — PASS

The V cutting now has pines along both crests, shrubs and tufts down both 30 deg banks, a rock on the right bank, cobbles only on the carriageway, dark earth verges, nothing on the pavement. It reads as a road cut through a wooded ridge. Seen, not failed: the same shrub mesh 20 times (GFX-03), bank texture streaks (J2), 12 m carriageway (J1b).

### context-road-everwood — PASS

Wide cobbled road into the pines, scrub lines on both verges (irregular, mixed sizes, not a row), dark rocks on the left earth margin, tufts, far hills; the pebble set at x 615-680, y 600-612 is gone (1 stone pixel). The lollipop broadleaf at x 1140-1300 and the beige hills remain GFX-03 / J1b items.

### context-road-valemere — PASS

Trees, shrubs, the boulder at x 1290-1430, y 510-580 on the verge, striped fields at x 0-400, y 500-560; the six r2 stone sets are gone (0 / 1 / 24 stone pixels, the 24 are a tuft's shadow at x 240-340, y 700-720, `vale_st` 4x).

### context-town-edge — PASS

Wall segments and towers on the grass, stepping with the ground; the far wall with its towers and the road out (`edge_far` 3x); nothing floats, no lamps on the open road. Unchanged. Seen, not failed: the near lawn x 0-1920, y 660-1080 has no cover instance at all (Outer South ground cover; GFX-03 ground pass).

### context-house-side-a — PASS

Side wall on its plinth, buried; fence, garden bed. The iron lamp at x 100-200, y 270-530: 94 warm pixels, all gilt tips and arm leaves (`sa_lamp` 3x; mean B > 100), no flame, no glow in the cages. Seen, not failed: the backdrop dome edge at x 1650-1920, y 0-260 (J1b); the lamp family (owner).

### context-house-side-b — PASS

Plinth right, stepping to the stoop base; bracket lantern dark at noon; forecourt and street clear of stones (0 pixels in all three r2 boxes). The iron lamp at x 1720-1810, y 280-400 (`sb_lamp` 4x): 1,543 warm pixels, mean RGB 215/183/137, the gilt roofs of the lanterns and the finial (202/162/112) in sun; the cages are dark. At dusk these panes glow (above), so the gate works both ways.

### context-house-side-rear — PASS

Rear wall on a plinth following the slope, firewood shelter, fence, nothing floats; the timber-post lamp dark. The iron lamp at x 1840-1910, y 180-330: 136 warm pixels at y 234-328, mean RGB 211/183/136 (gilt), none at the flame position.

### context-lamp-wall — PASS

Gable facade in shadow at noon, lantern dark (0 warm pixels), no wash, windows dark, sign clear of the bracket.

### context-road-approach — FAIL

The owner's spot holds: both sides fronted by houses, groups irregular, no twin across the road, timber-post lamp dark (0 warm pixels), no stones on the carriageway (0 in x 420-507, y 593-610), the stoop base at x 1300-1440, y 530-600 belongs to the house behind it. Fails on the chimney at x 545-565: `appr_sky` 3x shows seven separate 8-px dots 25 px apart over y 50-240 before they merge into a smudge; a second dotted thread at x 1440-1460, y 0-100. **Fix type:** LEVEL. **Exact instruction:** fix 1.

## Deferred (seen, not failed; GFX-02 / GFX-03 / owner, unchanged)

Bank texture streaks and bedrock faces on kerb-high banks (J2); backdrop dome and beige hills, 12 m cobble roads outside the wall (J1b); flat cyan water, lake causeway, canal, the zigzag white fence lines on the Westfields field at `chimney-smoke` x 30-450, y 520-560 (GFX-02 world); ashlar repeats, plaster, stall stand-in, low-poly trees and shrubs, bare Outer South lawn (GFX-03); townsfolk mannequins (townsfolk prototype; must be hidden for the plume measure, fix 2); street-lamp family (r1 fix 9, owner).

## Fix list (shared first)

1. **Continuous, denser plume** — Engineer, LEVEL. `GameClient/ArtSource/Silverbrook/silverbrook-props.json` `chimney_smoke`: `spawn_rate` 5 -> **7**, `lifetime` 5.5 -> **4.5** (31.5 live puffs, under the 32 gate), `rise_speed` 2.4 -> **1.6** (spacing 0.23 m), `start_size` 0.5 -> **0.7** (three puffs overlap at the pot), `opacity` 0.5 -> **0.7**, `tint` [0.11,0.11,0.12] -> **[0.06,0.06,0.07]** (expected core L/ref 0.80-0.86 at coverage 0.45-0.6), `end_size`, `wind` unchanged. `GameClient/ArtSource/VFX/ChimneySmoke/build_chimney_niagara.py`: `put(PU, curl, 'Noise Strength', local(T_FLOAT, value=25.))` -> **10.**, and the alpha key `0.9 @ 0.06` -> **0.9 @ 0.03** (`{a} < 0.03f ? {a} / 0.03f * 0.9f : ({a} < 0.4f ? lerp(0.9f, 0.5f, ({a} - 0.03f) / 0.37f) ...`), so the first 5 cm are already dense. Rebuild the system (`build_chimney_niagara.py --port <editor port>`), rerun `apply_dressing.py`. `measure_plume.py`: the item-5 core window follows the drift: x 840-960 -> **x 880-1060** (the wind moves the 3-6 m column 1.75 m = 230 px right). Update `vfx.md` §2 values row. Acceptance: item 5 core L/ref 0.65-0.88 over y 380-540, x 880-1060; in `chimney-smoke.png` and every `motion-*.png` no 5-px row in y 150-560 with fewer than 5 plume columns (|dL| >= 4) in x 880-1080; `main-street` x 1385-1425 and x 790-830 each >= 10 columns with ref - L >= 5 over y 300-400 and no gap > 15 px along the darkest column; `context-road-approach` x 530-580, y 50-250: no gap > 15 px between dark pixels along the darkest column.
2. **Hide every person in the plume frames** — Engineer, CODE, `docs/evidence/graphics/2026-10-22-wp6-town-i/capture_game.py` lines 182-186. Replace the class list with a component test and run it just before each shot, not only at the tick: `def hide_people(): hid = [a for a in u.GameplayStatics.get_all_actors_of_class(world, u.Actor) if a != pawn and not a.is_hidden() and a.get_components_by_class(u.SkeletalMeshComponent)]; [a.set_actor_hidden_in_game(True) for a in hid]; state.setdefault('others', []).extend(hid); return hid`, call it at `local == 0` and `local == 100` for `state['index'] in (CHIMNEY, REF)` and before every `shot(f'motion-..')`, log `WP6_PEOPLE_HIDDEN n classes [...]` with `set(a.get_class().get_name() for a in hid)`, unhide the stored list after the motion set (existing line 171). Acceptance: `chimney-smoke.png` and `-ref.png` have no |dL| >= 4 pixel below y 560 outside x 760-1100; item 1 origin span 30-130 px centred in x 860-940; item 2 ratio >= 2.0; `cs_man` region x 1200-1420, y 820-960 identical in both frames (max |dL| < 4).
3. **A dusk that is lit: moon light, not an exposure term** — Engineer, CODE + LEVEL. Under `r.EyeAdaptationQuality 0` no bias applies, so add light. `GameClient/ArtSource/Greenlands/lighting.py` `apply()`: with the same `_one` helper, a second `u.DirectionalLight` labelled **`Moon`**, movable, `intensity` **0.0** (saved off), `light_color` LinearColor (0.6, 0.7, 1.0), rotation pitch **-35**, yaw `cfg['sun.yaw_deg'] + 150`, `cast_shadows` True, `atmosphere_sun_light` True, **`atmosphere_sun_light_index` 1** (the sun keeps index 0, so every `SkyAtmosphereLightDirection(0)` gate stays night), `cast_cloud_shadows` False. `capture_game.py` `NIGHT`: `{'sun_pitch': 12., 'sky': 1.0, 'moon_lux': 0.25}` (0.25 = 2.5 % of the 10 lux sun: grass L 150 at noon becomes about L 22, sky about L 30); in the `-dusk` branch find the actor labelled `Moon`, `set_intensity(NIGHT['moon_lux'])`, set the sky light to `NIGHT['sky']` (real-time capture of the moonlit sky; 0.15 of a black sky was 0), drop the exposure log. `lighting-and-atmosphere.md` §3 night row: "sun 12 deg below the horizon (pitch +12), moon 0.25 lux at pitch -35, sky light 1.0 real-time capture, fixed exposure, no bias term (dead under quality 0)"; keep the rule: sky top 60 rows L 10-60, ground bottom quarter median L >= 8. Acceptance, all three dusk views: sky top 60 rows L 10-60; bottom-quarter median L >= 8; within 1 m of any lantern or lit window, outside the pane, <= 1 % of pixels > 235; the stoop, planter and street in `house-frontage-dusk` visible (x 0-700, y 700-1080 median L >= 8).
4. **Panes >= 200 and a pool under each street lamp** — Engineer, LEVEL. `GameClient/ArtSource/Greenlands/lamp-fx.json`: `glass_emission_strength` 1.8 -> **5.0** (`max` 4.0 -> 6.0; the pane is an opaque emissive whose L fell with the lumens because the 450 lm light in front of it was lighting its base colour; 123 -> 200 needs x2.9 linear), `light_radius` 4.0 -> **8.0** (`max` 5.0 -> 10.0; the iron lantern is 3.2 m up and the 4 m window cut the ground to 0). Rerun `lamp_fx.apply()`, then confirm the `gate_windows` order leaves `MI_SB_LanternGlow` `GlowScale` 1 (log it). Acceptance: lantern panes >= 200 in all three dusk views (`house-frontage-dusk` x 365-415, y 365-425; `-side-b-dusk` x 1455-1495, y 225-295 and the street-lamp panes x 1717-1737 / 1785-1805, y 342-372; `-lamp-wall-dusk` x 445-495, y 935-1020); `context-house-side-b-dusk` cobbles x 1700-1830, y 560-700 median L >= 12; clipping rule of fix 3 holds.
5. **Soffit under the lit window** — Engineer, LEVEL. `silverbrook-props.json` `windows.glow_strength_scale` 0.1 -> **0.05** (`min` 0.1 -> 0.02); rerun `gate_windows`. If the soffit still exceeds 1 % the light is a point light 30 cm in front of a window pane: in `lamp_fx.glass_sockets` skip pieces whose material slot matches `LitGlass|Window` (lights only for `LanternGlow` and `StreetLamp*_Glow`) and rerun `lamp_fx.apply()`, `check_lights.py`. Acceptance: `context-lamp-wall-dusk` x 760-1180, y 590-690 <= 1 % > 235; window glass in the three dusk views L 100-220.
6. **East plots: find the slab, then remove it** — Engineer, LEVEL + CODE, `GameClient/ArtSource/Greenlands/build_silverbrook.py` lines 574-581. First the probe, in the open editor with the sun at noon: for `ConstructionPlot_0` read `actor_hidden_in_game`, `is_hidden_ed`, `static_mesh_component.visible` and `cast_hidden_shadow`; then `-game` capture the SE overview twice with `ShowFlag.StaticMeshes 0` on the second: if the slabs vanish they are the cubes. Regardless of the finding: delete line 581 (`box(f'ConstructionPlot_{i}', ...)`; the layout is the reservation, no authoring mesh in the shipped map), and keep the plots out of every terrain and dressing pass: `PLOT_PADS = []` before the loop, `PLOT_PADS.append(placed[-1])` after line 580, then line 678 `Heightfield(L, pads=[p for p in placed if p not in PLOT_PADS] + flats, ...)`, line 699 `block([p for p in placed if p not in PLOT_PADS] + flats, 8.0)`, and the `dress_zone.dress(...)` call gets the same filtered list (houses still keep clear through `placed`). Rebuild, finish PCG, recapture. Acceptance: `context-overview-se.png` x 1370-1800, y 780-940 contains no rectangle with a uniform interior (16-px block L sd < 2) larger than 30x12 px; the ground there matches the forest floor around it (block mean RGB within 15 of the surrounding blocks).
7. **NW overview: no near terrain in the bottom tenth** — Engineer, CODE, `capture_game.py` `frame_ok` lines 78-88. Replace the 45 m rule: the bottom-edge rays at `s in (-1, -.75, -.5, -.25, 0, .25, .5, .75, 1)` must each hit ground that is either inside the wall ellipse (`((hx/100 - cx)/ax)**2 + ((hy/100 - cy)/ay)**2 <= 1.2`, `w0 = LAYOUT['wall']`) or no more than **15 m** above `G0` (`hit[0].z <= G0 + 1500`); keep the wall-extreme test. Search order: `alt` 230-800 in 50 m steps, then `back` 0-1000 in 100 m steps, then the eye's azimuth about the town centre `+-15, +-30 deg` at the same radius (a NW overview may swing to WNW or NNW); assert otherwise. Log `WP6_OVERVIEW_FRAME_CLEAR name alt dist azimuth`. Acceptance: `context-overview-nw.png` 16-px block smooth-band fraction from the bottom < 0.1 in every column; all four wall extremes inside x 60-1860, y 60-1020; same for SE.
8. **Street-lamp family** — owner decision (r1 fix 9), unchanged: twin-arm cast-iron standard vs `vfx.md` §6 plain post. Listed, not scored.
9. **Resubmit** — Engineer. `build_chimney_niagara.py`, `build_silverbrook.py --discard-unsaved` (with the `Moon`), `paint_street_edges.py`, `pcg_dressing.py` until `DRESSING_SAVED`, `lamp_fx.apply()`, `gate_windows`, all gates, recapture the five views, `chimney-smoke-ref`, the 14 motion frames (people hidden before every shot), the eleven context views (fix-7 frame test), the three dusk views (fix-3 row), `measure_plume.py` (fix-1 window), the 1440p perf pair.
