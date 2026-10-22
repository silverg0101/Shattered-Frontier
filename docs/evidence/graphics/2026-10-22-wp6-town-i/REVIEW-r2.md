# WP6i round-2 review — Silverbrook town, pass 9 r2 (WP6i REVIEW.md fix list 1-10)

Reviewer: independent (Fable), did not build the work. Judged per `docs/visual-review.md` against `Silverbrook-Design-Board-v1.webp` and `UI/v5/gameplay-hud.png` (mill plume, x 1330-1360, y 100-170), the WP6i round-1 review's fix list and acceptances, the WP6h standing acceptance, and the owner's four reports. Commit c437bd18. Every capture read at full resolution plus crops (2-4x) and pixel measurements (`py -3`, PIL/numpy): `|L - L_ref|` maps and signed diff over the chimney pair, 14-frame temporal std, street-thread columns, warm-pixel counts in every lantern box at noon, dusk sky/ground/pane/clipping statistics, overview dark-pixel count and flat-rectangle search, NW smooth-band fraction per column. The README's numbers were checked, not trusted.

**Result: 5 PASS / 14 FAIL.** Bookmarks 3 / 2, dusk lamp views 0 / 3, context views 2 / 9.

Two shared defects fail most of the views on their own: **field stones and rock sets lie on the paved carriageway in every street and road view** (fix 8 changed `dress_verge`, but the stones come from the PCG look profile: `cobble_pebble_scatter` clear 0.3 m / path 4 m and `rock_medium` clear 2 m, against a cobble paint that runs 2 m past the road edge and ramps over a 5 m grid step), and **the cast-iron street lamps still burn at noon** (`context-house-side-b.png` 1,534 warm pixels in the two lanterns, r1 1,588; fix 3 gated `M_LampFlame`/`Halo`/`Smoke`, which did not change what is lit). The plume is still invisible against the field (core L/ref 0.993-1.0, acceptance 0.65-0.88), the night row produces black frames (80-83 % of each dusk frame under L 8, ground median L 4), the three east plots are three flat grey rectangles in the forest, and the NW overview still has the near ridge across a third of the frame height.

## Owner reports

| Report | Status | Evidence |
|---|---|---|
| Floating houses | Fixed (WP6h), holds | `foundation-close`, `context-house-side-a/b/rear`: plinth under every wall, buried, no gap. |
| Orange light with no visible source | **Fixed in form; the wash still clips and the rest of the night is black** | `context-house-side-b-dusk.png`: pane x 1455-1495, y 225-295 glass L 210; wall within 1 m 21.4 % > 235 (r1 27.0 %), ashlar below the lantern x 1100-1440, y 380-640 12.5 % > 235 (r1 44.3 %), mean RGB 131/65/5 (R saturated). Outside the pools the frame is black (80 % under L 8). |
| Street clutter in mirrored rows on an empty approach road | Fixed (r1), holds | `context-road-approach.png`: groups only in front of houses, no twin across the road. |
| White street | Holds | Cobbles tan/ochre in every street view. |
| Lamps lit at noon (J5) | **Not fixed for the iron lamps** | `context-house-side-b.png` x 1720-1810, y 280-400: 1,534 pixels with max > 200 and R - B > 60, an amber flame at the top of each lantern (`sb_lamp` 4x crop); `context-house-side-rear.png` x 1840-1910, y 180-330: 148; `context-house-side-a.png` x 100-200, y 270-530: 89 (two 8-px amber points at the lantern tops, x 103-112, y 306-316 and x 173-182, y 308-318). The timber-post lamp is dark: `context-road-approach.png` x 385-430, y 330-400: 0 (r1 53); `context-lamp-wall` 0. |

## Round-1 fix items 1-10: met?

| Item | Verdict | Measured |
|---|---|---|
| 1 PCG finished before capture | **Met** | `context-overview-se`: pine stands SE and E, broadleaf W; 14,890 px under L 70 (target 24,000 was for the 230 m eye; the eye is now 330 m). `context-road-everwood/valemere`: trees, shrubs, tufts, pebbles. |
| 2 visible plume | **Not met** | Values applied (opacity 0.5, spawn 5). Core L/ref over y 380-540, x 840-960: 0.993 / 0.998 / 1.000 (acceptance 0.65-0.88). Near the stack (x 820-1000) the `|dL| >= 4` mask has 0 columns at y 360-480 and 7-23 at y 500-560 (lighter puffs over the slate); the only readable smoke is a distant column at x 937-1000, y 0-300 (L/ref 0.94-0.98). `others` is listed at script start, so a Character spawned later walks through `chimney-smoke-ref.png` (x 1250-1290, y 860-920) and item 1 still reports x 960-1457. |
| 3 flame/halo/wisp gated | **Not met** | Materials rebuilt (`M_LampFlame.uasset` 25,874 -> 28,170 bytes) but the lit element is unchanged: 1,534 / 148 / 89 warm pixels (above). |
| 4 lantern lumens, window scale | **Not met** | side-b-dusk wall within 1 m 21.4 % > 235 (max 255); below the lantern 12.5 %. lamp-wall-dusk soffit x 760-1180, y 590-690: 28.7 % > 235 (r1 39.1 %), mean RGB 122/76/16. house-frontage-dusk jetty top x 350-800, y 0-120: 5.4 % > 235 (r1 7.8 %), the lit soffit, not a pane. Panes 210 / 210 / 208 (>= 200, met). |
| 5 night row | **Not met** | Sky, top 60 rows: 12.9 / 8.0 / 4.3 (acceptance 10-60; two of three fail). Ground outside the pools: bottom quarter median L 4.0 / 4.2 / 4.4, 0 / 18 / 16 % of it over L 10. 82 / 80 / 83 % of each frame under L 8. Sun pitch +12 is below the horizon now, but sky light 0.0 with exposure +0.25 is a black night, not dusk. |
| 6 NW overview clear of the ridge | **Not met** | Smooth near-ridge band from the frame bottom: mean 4.4 % of column height, max 32.6 % (x 1000-1250), 16.7 % of columns over the 0.1 limit; the dome covers x 700-1920, y 850-1080. The wall ellipse leaves the frame at the left edge (x 0, y 600-700) and under the ridge at the bottom right. The bottom-centre ray clears; the corners do not. |
| 7 plots as field | **Not met** | `context-overview-se.png` x 1380-1433, y 893-917; x 1480-1540, y 920-957; x 1600-1633, y 960-983 (`se_slab` 3x): three flat uniform blue-grey rectangles with sharp edges, in a row among the pines, no furrow texture at 3x. The tilled paint on a levelled pad reads as a slab from 330 m; the Westfields fields at x 100-500, y 380-560 read as striped fields. |
| 8 no stones on the cobbles | **Not met** | `main-street.png`: stone sets on the carriageway at x 557-647, y 627-663; x 380-447, y 637-647; x 1213-1273, y 640-653; x 1340-1427, y 637-657. `context-road-approach.png` x 420-507, y 593-610. `context-house-side-b.png` x 1470-1520, y 565-585; x 1640-1750, y 597-660; x 1860-1920, y 635-660. `context-road-valemere.png` x 80-230, y 765-805 (three 60 cm rocks), x 240-340, y 700-720; x 360-420, y 680-695; x 550-600, y 665-680; x 1430-1500, y 660-685; x 1630-1700, y 650-665. `context-road-everwood.png` x 615-680, y 600-612. `dress_verge` now skips pebbles on cobble, but these are PCG instances (`cobble_pebble_scatter` and `rock_medium` roles). |
| 9 lamp family | Owner decision, open | Unchanged by instruction. |
| 10 resubmit | Met in form | All views, ref, motion set, perf pair present; perf on - off -0.16 / -0.02 ms, Niagara GT 0.11-0.14 ms, workers 0.45-0.57 ms, 9 active within 120 m (inside the gate). |

## main-street — FAIL

Same street as r1: cobbles kerb to kerb, five house tones, barrels, crates, hay cart, bench, ladder, signs, lamps and windows dark at noon, nothing floating, no placeholder material.

1. **What's wrong:** chimney thread x 790-830: 5 columns (criterion >= 15), x 1085-1125: 0. What the far chimneys produce is a vertical string of separate 10-px dark dots (`ms_sky` 2x crop: x 1400-1420, y 0-170; x 1150-1170, y 270-350; x 800-820, y 300-390), not a column: puffs 0.35 m wide spawned 0.48 m apart (2.4 m/s / 5 per s) never overlap near the pot. **Fix type:** LEVEL. **Exact instruction:** fix 3.
2. **What's wrong:** field-stone sets on the paved carriageway at x 557-647, y 627-663 (two sets, 3x crop `ms_stones_left`), x 380-447, y 637-647, x 1213-1273, y 640-653 and x 1340-1427, y 637-657 (`ms_stones_right`). **Fix type:** LEVEL. **Exact instruction:** fix 1.

## market — PASS

Stalls with awnings, firewood sheds, ladder, sacks, mixed house tones, cobbles with earth margins, far houses on their plinths on the green; the square reads as a square. Stall stand-in: GFX-03 (deferred).

## house-frontage — PASS

Bay window with planter, boot sign, bracket lantern dark at noon (x 365-415, y 365-425), stoop with tub, ivy, plinth. Seen, not failed: the kerb-high bank at x 0-400, y 600-820 renders as cracked bedrock (J2, deferred).

## foundation-close — PASS

Rubble plinth under the ashlar on both faces of the corner, flush, buried; tub and planter on the stoop. Unchanged.

## chimney-smoke — FAIL

Measured on `chimney-smoke.png`, `chimney-smoke-ref.png` and `motion-00..13.png` (pots x 885-920, y 575-620; 131.7 px per m):

| Acceptance (r1 fix 2) | r2 | Met |
|---|---|---|
| 1 origin 30-130 px at y 550, centre x 860-940 | mask at y 550: x 960-1457 (townsfolk and far street); at the stack 9-23 columns in y 500-560 | No |
| 2 growth at y 150 | 47-64 columns in x 820-1000 at y 100-160 (a far column, see below), 0 at y 200-240 | No |
| 3 no veil | 19 columns max in y 460-540 | Yes |
| 4 density >= 12 at y 380-540, >= 6 at y 100 | 18.0 / 13.7 (builder), but the y 380-540 peak is at x > 1000 (people), 0 columns at the stack in y 360-480 | No |
| 5 core L/ref 0.65-0.88 over y 380-540, x 840-960 | 0.993 / 0.998 / 1.000 | No |
| 6 puffs | n/a at the stack | No |
| 7 motion >= 20 px/s up | 101.6 px/s; temporal std in x 760-1100, y 0-600: mean 1.12, 9.4 % of pixels > 3 | Yes |
| 8 stack in frame, no card edges | yes | Yes |
| street thread >= 15 columns | 5 | No |

1. **What's wrong:** the plume from the nearest stack is invisible where it matters. The signed diff (`chimney_signdiff_crop`) shows lighter puffs only over the slate at x 940-980, y 550-660 (slate L 55), nothing over the field at y 360-480 (field L 150, ratio 1.00), and a darker column at x 937-1000, y 0-300 against the hazy hill (0.94-0.98). The sprite is lit (`MSM_DEFAULT_LIT`) with `Tint` 0.24, and a 0.24 albedo under the same sun renders the same L as the grass it is in front of; opacity cannot change a ratio of 1.0. The r1 expectation "core L ~115 against 150" assumed the grey would read darker; it does not.
   **Fix type:** LEVEL. **Exact instruction:** fix 3 (tint 0.11, start size 0.5).
2. **What's wrong:** a UE mannequin walks through `chimney-smoke-ref.png` (x 1250-1290, y 860-920) and the far street (x 960-1457 at y 550): `others` is computed when the script loads, before the townsfolk spawn, so they are not hidden. **Fix type:** CODE. **Exact instruction:** fix 3 (hide at the tick).

## house-frontage-dusk — FAIL

Pane x 365-415, y 365-425 glass L 210; wall within 1 m 4.6 % > 235 (the pane). Bay panes lit, far windows lit. Sky top 60 rows L 12.9. Everything else is black: 82 % of the frame under L 8, the bottom quarter median L 4.0 with 0 % of it over L 10; the stoop, planter and street under the lantern are invisible (`hfd_lantern` 3x: a glowing rectangle in black, lighting nothing visible). Jetty top x 350-800, y 0-120: 5.4 % > 235 from the window above (`hfd_jetty`). **Fix type:** CODE + LEVEL. **Exact instruction:** fix 4.

## context-house-side-b-dusk — FAIL

Pane glows (L 210), the wash has a visible source, the sign bracket casts a shadow. The wall is still clipped: x 1280-1660, y 70-450 21.4 % of pixels > 235 (max 255), the ashlar below the lantern x 1100-1440, y 380-640 12.5 %, mean RGB 131/65/5 (red saturated, `sbd_wash` 2x). Sky L 8.0 (acceptance 10-60). Ground outside the pool: median L 4.2. **Fix type:** CODE + LEVEL. **Exact instruction:** fix 4.

## context-lamp-wall-dusk — FAIL

Lantern x 445-495, y 935-1020 glass L 208; wall within 1 m 6.4 % > 235 (the pane). The jetty soffit x 760-1180, y 590-690 under the lit first-floor window is 28.7 % > 235 (`lwd_soffit` 2x: a saturated yellow trapezoid). Sky L 4.3; 83 % of the frame under L 8. **Fix type:** CODE + LEVEL. **Exact instruction:** fix 4.

## Context views

### context-overview-se — FAIL

The forest is back: pines east and south-east of the wall (x 1100-1920, y 700-1080), broadleaf and pine stands west (x 0-300, y 480-1080), scattered trees on the far slopes; roads leave each gate and go somewhere. 14,890 px under L 70 at the 330 m eye.

1. **What's wrong:** three flat grey-blue rectangles in a row among the pines east of the wall, x 1380-1433, y 893-917; x 1480-1540, y 920-957; x 1600-1633, y 960-983 (`se_slab` 3x): uniform interior, sharp edges, no furrows, each about 20 m. These are the `ConstructionPlot_0..2` pads: `FIELDS.append` paints tilled soil over a relief-free pad, and from 330 m a flat uniform dark patch reads as a slab, not a field (the Westfields fields at x 100-500, y 380-560 read as striped fields because they are large and far). A player sees three concrete pads in a forest.
   **Fix type:** LEVEL. **Exact instruction:** fix 6.
Seen, not failed (world pass, unchanged): the wall as a lake causeway at the bottom, the straight canal, flat cyan water, the smooth beige backdrop mountains (J1b).

### context-overview-nw — FAIL

The eye is at 330 m and the town is in frame, but the near ridge is a smooth beige-green dome over the bottom right, x 700-1920, y 850-1080 (max 32.6 % of column height at x 1000-1250; 16.7 % of columns exceed the 0.1 limit), and the wall ellipse is cut at the left edge (x 0, y 600-700, `nw_left_edge`) and under the dome at the right (`nw_right_edge`). The fix-6 test traces only the bottom-centre ray; the bottom corners and the frame edges are not tested. **Fix type:** CODE. **Exact instruction:** fix 5.

### context-road-silvermere — FAIL

The trench is a V cutting with ~30 deg banks, cobbles only on the carriageway, dark earth verges. The 120 m cutting has no tree, shrub, rock, tuft or pebble on either bank (`silv_left` 1x: a smooth textured slope with streaks). The road runs y -529 to -1250 and no district polygon covers it (Northridge ends at y -505; Westfields and Eastwood are to the sides), so the PCG dressing never reaches the cutting. A player finds a cobbled road through a sand dune. **Fix type:** LEVEL. **Exact instruction:** fix 7. Seen, not failed: bank texture streaks (J2).

### context-road-everwood — FAIL

Wide cobbled road into the pines, shrubs, pebbles and tufts on the verges, far hills; the dressing is back. Fails on one pebble set on the pavement at x 615-680, y 600-612 (`ever_st2` 4x: three stones on the cobble strip). **Fix type:** LEVEL. **Exact instruction:** fix 1. The 12 m carriageway outside the wall, the lollipop broadleaf at x 1140-1300 and the beige hills remain world/GFX-03 items.

### context-road-valemere — FAIL

Trees, shrubs, a boulder at x 1290-1430, y 510-580, pebbles: the dressing is back. Six stone sets lie on the paved carriageway: x 80-230, y 765-805 (three 60 cm rocks in the foreground, `vale_stones` 2x), x 240-340, y 700-720; x 360-420, y 680-695; x 550-600, y 665-680; x 1430-1500, y 660-685; x 1630-1700, y 650-665. **Fix type:** LEVEL. **Exact instruction:** fix 1.

### context-town-edge — PASS

Wall segments and towers on the grass, the segments step with the ground; beyond the near wall the road through open ground has no lamps; the far wall and gate carry theirs; the Riverside row on the horizon (`edge_far_houses`). Nothing floats.

### context-house-side-a — FAIL

Side wall on its plinth, buried; fence, garden bed. The cast-iron lamp at x 100-200, y 270-530 shows two amber points at the lantern tops at noon, x 103-112, y 306-316 and x 173-182, y 308-318 (89 warm pixels; r1 had 1), the same element that burns in side-b. **Fix type:** CODE. **Exact instruction:** fix 2. Seen, not failed: the backdrop dome at x 1650-1920, y 0-260 with a visible banded edge (`sa_dome`, J1b); the lamp family (fix 8, owner).

### context-house-side-b — FAIL

Plinth right, stepping to the stoop base; bracket lantern dark at noon. The iron street lamp at x 1720-1810, y 280-400 burns: 1,534 warm pixels, an amber flame in the top of each lantern (`sb_lamp` 4x). Stone sets on the cobbled forecourt and street: x 1470-1520, y 565-585; x 1640-1690, y 637-660; x 1690-1750, y 597-615; x 1860-1920, y 635-660 (`sb_stones`). **Fix type:** CODE (flame) + LEVEL (stones). **Exact instruction:** fixes 2 and 1.

### context-house-side-rear — FAIL

Rear wall on a plinth following the slope, firewood shelter, fence, nothing floats; the timber-post lamp at x 240-320, y 350-480 is dark (the 382 warm pixels in that box are the sunlit post). The iron lamp at x 1840-1910, y 180-330 burns: 148 warm pixels, flame and lit finial (`rear_lamp` 4x). **Fix type:** CODE. **Exact instruction:** fix 2.

### context-lamp-wall — PASS

Gable facade in shadow at noon, lantern x 430-500, y 900-1030 dark (0 warm pixels), no wash, windows dark, sign clear of the bracket.

### context-road-approach — FAIL

The owner's spot. Both sides fronted by houses; groups irregular, no twin across the road; the timber-post lamp lantern x 385-430, y 330-400 is dark (0 warm pixels, fixed). Stones on the paved carriageway at x 420-507, y 593-610 (`appr_st1` 3x, in the house's shadow); the set at x 140-230, y 680-710 beside the lamp plinth is on the earth margin and is fine. **Fix type:** LEVEL. **Exact instruction:** fix 1.

## Deferred (seen, not failed; GFX-02 / GFX-03 / owner, unchanged)

Bank texture streaks and bedrock faces on kerb-high banks (J2); backdrop dome and beige hills, 12 m cobble roads outside the wall (J1b); flat cyan water, lake causeway, canal (GFX-02 world); ashlar repeats, plaster, stall stand-in, low-poly trees (GFX-03); townsfolk mannequins (townsfolk prototype; hidden for the plume measure by fix 3); street-lamp family (r1 fix 9, owner to confirm).

## Fix list (shared first)

1. **No PCG stones on paved ground** — Engineer, LEVEL + CODE. (a) `GameClient/ArtSource/Greenlands/greenlands-look-profile.json`: `cobble_pebble_scatter` `place.clear` 0.3 -> **4.5**, `path` 4 -> **9**; `rock_medium` `clear` 2 -> **4.5**; `boulder_large_moss` and `bedrock_slab` `clear` 3 -> **4.5** (the cobble paint is `STREET_COBBLE_EXTRA_M` 2 m past the road edge plus a `ROAD_RAMP` of one grid step; 0.5 is reached at edge + 4.5 m). (b) The forecourt and door-lane cobbles (`paint_street_edges.py` lot lanes, `PLAZAS`) are not roads: in `build_silverbrook.py` pass the painted cobble to the planner, `dress_zone.dress(L, look, HF, placed, district, L['seed'], paved=lambda x, y: ROAD_G.get(round((y - HF.y0) / HF.step) * HF.nx + round((x - HF.x0) / HF.step), 0.0))`, and in `dress_zone.py` line 165-169 reject a point for any role in layers `rocks` and `litter` when `paved(x, y) >= 0.5` (plaza rectangles grown by `PAVED_MARGIN` are already excluded); then `paint_street_edges.py` must run before `pcg_dressing.py` and expose its lane polylines so the same test covers door lanes (`lane_field(x, y)` = 1 within lane half-width + 2 m). Rebuild, finish PCG, recapture. Acceptance: no stone or rock instance inside the painted carriageway or a forecourt in `main-street`, `context-road-approach`, `context-house-side-b`, `context-road-valemere`, `context-road-everwood` (the boxes above are empty of grey low-saturation blobs, L 60-110, sat < 25, over cobble).
2. **Iron street lamps dark at noon** — Engineer, CODE. The element that burns is not reached by fix 3. In the open editor, `find_actors` the `StreetLamp_*` nearest `context-house-side-b`'s eye (east of the side-b townhouse), list its mesh material slots and the `FX_StreetLamp_Cards` instances at its sockets, and set the sun to noon (pitch -55): whichever renders emissive is the culprit. Then, regardless of the finding, make the gate the default: `silverbrook_props.py` `gate_windows` line 158: `mel.set_material_instance_scalar_parameter_value(mi,'Gate',0. if any(re.search(p,mi.get_name(),re.I) for p in c['windows']['always_lit_patterns']) else 1.)` with `silverbrook-props.json` `windows.always_lit_patterns: ["ForgeFire"]` (replaces `name_patterns` as the gate selector; keep `name_patterns` for `GlowScale`), so every kit `*_Glow` / `*_Flame` instance (StreetLamp1-4, Watchtower_Flame, LanternGlow, vendor glows) follows the sun. If the culprit is the `M_LampFlame` card: `build_materials.py` `_night()` must be sampled per card, not per material default; add `mel.recompile_material` and re-save `M_LampFlame`/`M_LampHalo` and confirm the card instances reference the rebuilt material, not an older `MI_`. Rerun `lamp_fx.apply()`. Acceptance: in every noon view, 0 pixels with max channel > 200 and R - B > 60 inside each lantern box: `context-house-side-b` x 1720-1810, y 280-400 (now 1,534); `-rear` x 1840-1910, y 180-330 (148); `-side-a` x 100-200, y 270-530 (89).
3. **Visible plume** — Engineer, LEVEL + CODE. `silverbrook-props.json` `chimney_smoke`: `tint` [0.24,0.24,0.26] -> **[0.11, 0.11, 0.12]** (lit albedo half the grass's: expected core L/ref 0.67-0.79 at opacity 0.42-0.66), `start_size` 0.35 -> **0.5** (puffs overlap at the pot: spacing 2.4 / 5 = 0.48 m), keep `opacity` 0.5, `spawn_rate` 5 (5 x 5.5 = 27.5 live puffs stays under the 32-particle gate), `lifetime`, `rise_speed`, `wind`, `end_size`. Rerun `apply_dressing.py`. `capture_game.py`: delete line 66 and in `tick()` at `if state['index'] == CHIMNEY:` compute `others = [c for c in u.GameplayStatics.get_all_actors_of_class(world, u.Pawn) if c != pawn]` there (store in `state`) and hide them; unhide from the stored list after the motion set. Update `vfx.md` §2 values row. Acceptance: `measure_plume.py` item 5 core L/ref in 0.65-0.88 over y 380-540, x 840-960; item 1 origin span 30-130 px centred in x 860-940 at y 550 with no |dL| >= 4 pixels outside x 760-1100 at that row; item 2 ratio >= 2.0; `main-street` thread >= 15 columns in one of the two windows and the column reads as a continuous band (no gap > 15 px between dark pixels over y 300-400).
4. **Dusk that reads as dusk, lanterns that do not clip** — Engineer, CODE + LEVEL. The night row as written (sky light 0.0, exposure +0.25) is a black frame. `lighting-and-atmosphere.md` §3 night row and `capture_game.py` `NIGHT`: `sky` 0. -> **0.15**, `exposure` .25 -> **1.75**, sun pitch +12 unchanged; state the rule in the blueprint: the row is right when the sky's top 60 rows read L 10-60 and the ground's bottom quarter has median L >= 8. Then take the lamps down by the same +1.5 EV so the walls stop clipping: `lamp-fx.json` `light_intensity` 450 -> **160** (street); `lamp_fx.py` `_point_light` line 144 wall factor `.33` -> **.17** (27 lm, a candle behind amber glass); `silverbrook-props.json` `windows.glow_strength_scale` 0.6 -> **0.1** (lantern panes keep `GlowScale` 1). Rerun `lamp_fx.apply()`, `gate_windows`, recapture the three `-dusk` views. Acceptance, all three views: sky top 60 rows L 10-60; bottom-quarter median L >= 8; panes >= 200; within 1 m of a wall lantern or lit window, outside the pane, <= 1 % of pixels with max channel > 235 (now 21.4 % side-b wall, 12.5 % below, 28.7 % lamp-wall soffit, 5.4 % frontage jetty).
5. **Overview frames: corners and ellipse, not one ray** — Engineer, CODE, `capture_game.py` lines 73-83. Replace the loop body with a frame test: for each candidate `alt` build `f` (unit forward), `r = u.Vector(0,0,1).cross(f).normal()`, `up = f.cross(r)`, `th = math.tan(math.radians(40))`, `tv = th * 9 / 16`; the three bottom rays `dir = f + r * th * s - up * tv` for `s in (-1, 0, 1)` must not hit the terrain within 4 km (`terrain.static_mesh_component.line_trace_component(e, e + dir * 400000, True, False)` is falsy); and the four wall extremes `(400,0), (-400,0), (0,530), (0,-530)` m at ground height must project inside the frame with a 60 px margin: `d = p - e; z = d.dot(f); px = 960 + d.dot(r) / z / th * 960; py = 540 - d.dot(up) / z / tv * 540`, require `z > 0 and 60 <= px <= 1860 and 60 <= py <= 1020`. Raise `alt` in 50 m steps to 800 and, if it still fails, move the eye 100 m further along the eye-to-target bearing and repeat; assert otherwise. Log `WP6_OVERVIEW_FRAME_CLEAR name alt dist`. Acceptance: `context-overview-nw.png` smooth-hill fraction from the bottom < 0.1 in every column; every wall extreme inside the frame; same for SE.
6. **East plots are not slabs** — Engineer, LEVEL (owner may prefer a dressed yard). `build_silverbrook.py` line 581: delete the `FIELDS.append(...)` (revert the r1 fix-7 paint); keep `placed.append` (trees stay clear) and the hidden `box`. The plot then reads as a forest glade until a building is placed on it. If the owner wants the reservation visible, dress each plot as a cleared yard (fence section on three sides from `PropFenceSection`, one `PropSackPile`, a muck heap) with yaws 0 / 15 / -20 deg so the three never read as a row. Acceptance: `context-overview-se.png` x 1370-1650, y 880-990 contains no rectangle with a uniform interior (16-px block L sd < 2) larger than 30x12 px; the ground there matches the forest floor around it.
7. **Silvermere cutting dressed** — Engineer, LEVEL, `build_silverbrook.py` after line 899: add `L['districts']['Outer North'] = dict(polygon=[[-220, -520], [260, -520], [240, -1100], [-200, -1100]], exclude_polygons=[d['polygon'] for d in L['districts'].values()], outside_wall=True, inside_wall=False, dress=dict(canopy=.22, rocks=.35, understory=.75, ground_cover=.55, litter=.6))`, `DRESS_DISTRICTS.append('Outer North')`, and give it the River Verge slope override (`grass_clump`, `herb_broadleaf`, `fern` `place.slope[1]` 35) so the 30 deg banks take cover; pines (slope 0-35) and `rock_medium` (0-90) place on the banks as the look profile already allows. Rebuild, finish PCG (`DRESSING_SAVED` with the new volume), recapture. Acceptance: `context-road-silvermere.png` shows >= 6 trees or shrubs and >= 4 rock or tuft instances on the two banks within 60 m (x 0-900 and x 1050-1920, y 0-560), none on the carriageway (fix 1).
8. **Street-lamp family** — owner decision (r1 fix 9), unchanged: twin-arm cast-iron standard vs `vfx.md` §6 plain post. Listed, not scored.
9. **Resubmit** — Engineer. Rebuild (`build_silverbrook.py --discard-unsaved`, `paint_street_edges.py`, `pcg_dressing.py` until `DRESSING_SAVED`), `lamp_fx.apply()`, all gates, recapture the five views, `chimney-smoke-ref`, the 14 motion frames (pawns hidden at the tick), the eleven context views (fix-5 frame test), the three dusk views (fix-4 row), `measure_plume.py`, the 1440p perf pair.
