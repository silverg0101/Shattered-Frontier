# WP6i review — Silverbrook town, pass 9 (WP6h fix list 1-8)

Reviewer: independent (Fable), did not build the work. Judged per `docs/visual-review.md` against `Silverbrook-Design-Board-v1.webp` and `UI/v5/gameplay-hud.png` (mill plume, x 1330-1360, y 100-170: a pale grey column one stack-width wide at the pot, clearly visible against the trees, rising well over six stack-widths), the WP6h review's fix list and acceptances, and the owner's four reports. Commit b7cb13be. Every capture read at full resolution plus crops (2-4x) and pixel measurements (`py -3`, PIL/numpy): lantern panes, wall clipping, sky luminance, noon flame pixels, |L - L_ref| plume maps over the 14 motion frames, WP6h-vs-WP6i foliage counts, NW ridge fraction. The README's numbers were checked, not trusted.

**Result: 9 PASS / 10 FAIL.** Bookmarks 3 / 2, dusk lamp views 2 / 1, context views 4 / 7.

The pass has one regression that fails seven views on its own: **every tree, shrub, rock and tuft outside the wall is gone.** `context-overview-se` has 16 dark (L < 70) pixels where WP6h had 30,339 (the forest south and east of the wall); `context-road-everwood` and `-valemere`, which passed WP6h with pines, shrubs and pebble verges, are bare dunes with a 12 m cobble strip. `build_silverbrook.py` line 924 writes the district dressing into PCG volumes and generates on demand; `pcg_dressing.py` says the build "ends with a second run of this file" (`finish()`: asserts every volume's instance count and saves). The README's run order has no such step, so the map was saved with empty PCG volumes and captured that way.

## Owner reports

| Report | Status | Evidence |
|---|---|---|
| Floating houses | Fixed (WP6h), still holds | `foundation-close`, `context-house-side-a/b/rear`: plinth under every wall, buried, no gap. |
| Orange light with no visible source | **Fixed at dusk, but the wash clips.** | `context-house-side-b-dusk.png`: the bracket lantern pane at x 1455-1495, y 225-295 glows (glass L 212); the wall it lights is clipped over 27 % of the pixels within 1 m and 44 % of the wall face below it (max channel 255). `house-frontage-dusk` and `context-lamp-wall-dusk`: panes L 211 / 208, no wall clipping beyond the pane itself. |
| Street clutter in mirrored rows on an empty approach road | **Fixed.** | `context-road-approach.png` (the owner's spot): every group has a house behind it, no identical pair across the road, no two groups at one chainage. `chimney-smoke.png` x 100-1300, y 230-420 (2x crop): the far road along the field is clean where WP6h had sacks, barrels and posts in a row. |
| White street | Holds | Cobbles tan/ochre in every street view. |
| Lamps lit at noon (J5) | **Half fixed.** | Lights, lantern panes and windows are off at noon (`context-lamp-wall.png`: lantern x 430-500, y 900-1030 dark, no wash; `main-street`: no lit window). The flame and halo cards are not gated: `context-house-side-b.png` x 1720-1810, y 280-400 has 1,588 warm pixels (max > 200, R - B > 60, mean RGB 216/183/137) inside the two lanterns; `context-house-side-rear.png` x 1840-1910, y 180-330: 171; `context-road-approach.png` x 385-430, y 330-400: 53. A player sees a flame burning at noon. |

## WP6h fix items 1-8: met?

| Item | Verdict | Measured |
|---|---|---|
| 1 plume values | **Not met.** The values were applied exactly; the result is an invisible plume. | Near stack (x 820-1000, y 250-580): motion-mean minus reference has 0 columns with |dL| >= 3 above row 300; rows 300-550 have 6-33 columns, all townsfolk and roof-edge noise (the diff map shows a mannequin, not smoke). `plume-measure.json` item 1 "origin x 1039-1457" is the crowd at the far end of the street. Peak per-puff opacity 0.19 and a 0.24 grey lit by the sun against a L 150 field gives L/ref 0.975: 2.5 % contrast is below sight. The concept plume is unmistakable. |
| 2 reference-frame measure | Met in form, unusable in practice | `chimney-smoke-ref.png` is the same eye with smoke hidden; the measure is `|L - L_ref| >= 4`. Townsfolk (UE mannequins) walk through the frame, so every item reports people, not smoke. |
| 3a lantern panes glow | **Met** | Glass-only (brightest half of the pane box) L: lamp-wall-dusk 208, side-b-dusk 212, frontage-dusk 211 (>= 200). |
| 3b no clipping within 1 m | **Not met** | side-b-dusk x 1280-1660, y 70-450: 27.0 % of pixels > 235, max 255; wall below the lantern x 1100-1440, y 380-640: 44.3 %. lamp-wall-dusk and frontage-dusk: only the pane itself exceeds 235. |
| 3c one sun gate | **Half met** | M_LampGlass, M_LampLightFunction, MI_SB_LanternGlow, M_SB_WindowGlow gated; M_LampFlame, M_LampHalo, M_LampSmoke not. |
| 4 street groups | **Met** | approach view and chimney far road, above. |
| 5 lamps near buildings | **Met** | `context-town-edge.png` x 1200-1920, y 380-560 (2x): the road through the open ground has no lamps; the far wall and gate only. |
| 6 NW overview | **Not met** | 52 % of each column from the frame bottom is smooth near-ridge terrain (69 % of 16-px blocks have L sd < 2); the town occupies the top-left half, its southern half behind the ridge. The sight-line assert passes because it tests one ray to 5 m above the aim point, not the frame. |
| 7 Silvermere banks | **Met for the trench** | Banks read ~30 deg, cobbles only on the carriageway, dark earth verge beside it; the banks' ground texture is smeared in down-slope streaks (`context-road-silvermere.png` x 0-700, y 0-560, 2x), which is the terrain material on slopes (J2, deferred). |
| 8 resubmit | **Not met** | Gates ran, but the PCG dressing was never finished and saved (above); the dusk captures do not use the lighting blueprint's night row (sky L 4.0, i.e. black, in all three). |

## main-street — FAIL

Same street as WP6h: cobbles kerb to kerb, five house tones, barrels, crates, hay cart, bench, ladder, signs, lamps off at noon, windows dark, nothing floating, no placeholder material. Fails on the chimney thread, which was part of this view's acceptance since WP6g: x 790-830 now 5 columns (WP6h 23, criterion >= 15), x 1085-1125 0. Same cause and fix as chimney-smoke (fix 2). Also seen: field stones lying on the paved carriageway (x 440-520, y 670-690; x 1390-1440, y 650-660; x 1550-1575, y 685-700): the verge pebble scatter is placed on the cobbles (fix 8).

## market — PASS

Stalls with awnings, firewood sheds, ladder, sacks, mixed house tones, cobbles with earth margins, far houses on plinths. Fewer groups than WP6h and the square reads as a square. Stall stand-in: GFX-03 (deferred).

## house-frontage — PASS

Bay window with planter, boot sign, bracket lantern (dark at noon, correct now), stoop with tub, ivy, plinth. Seen, not failed: the 1 m bank between the field and the street at x 0-400, y 600-820 now renders as a vertical face of cracked bedrock (road paint x (1 - rockiness) puts bedrock on any bank over SLOPE_FLAT_DEG, even a kerb-high one): J2, deferred.

## foundation-close — PASS

Rubble plinth under the ashlar on both faces of the corner, flush, buried; tub and planter on the stoop. Unchanged from WP6h. `capture_game.py` logged a street group within 10 m of the sight line (`StreetGroup_SouthStreet_069_L`); it does not enter the frame.

## chimney-smoke — FAIL

Measured on `chimney-smoke.png`, `chimney-smoke-ref.png` and `motion-00..13.png` (pots at x 885-920, y 575-620; 131.7 px per m):

| Acceptance | WP6i | Met |
|---|---|---|
| 1 origin 30-130 px at y 550, centre x 860-940 | no plume pixels at the stack in any frame; the JSON's x 1039-1457 is people on the street | No |
| 2 growth at y 150 | nothing | No |
| 3 no veil | nothing to veil | n/a |
| 4 density >= 12 at y 380-540, >= 6 at y 100 | builder 14.9 (townsfolk); 1.2 at y 100 | No |
| 5 luminance | L/ref 0.975 at row 260 (one row) | the criterion as written is met and is the problem: 2.5 % contrast is invisible |
| 6 puffs | n/a | No |
| 7 motion | 41 px/s mean from five non-zero pairs; nothing to move | No |
| 8 stack in frame, no card edges | yes | Yes |
| street thread >= 15 columns | 5 | No |

1. **What's wrong:** there is no visible smoke from the nearest 85 stacks at 14 m (`c_stack` 2x crop: pots, slate, field; the only smoke in the frame is a faint column from a far chimney at x 960-980, y 0-280 in the diff map, |dL| ~2). The WP6h fix-1 values give a peak per-puff opacity of 0.94 x 0.9 x 0.22 = 0.19 and a 0.24 grey that, sunlit, lands within 3 % of the field's luminance. WP6h's acceptance item 5 ("within 10 % of the sky") was written for a sky background; this eye has a sunlit field and roofs behind the plume, where real wood smoke reads **darker** than the field and lighter than the slate. The concept's plume is lighter than the trees behind it by ~30 % (HUD px 144/143/152 over trees ~60) and is the most visible thing on the mill roof.
   **Fix type:** LEVEL (values) + CODE (acceptance + capture). **Exact instruction:** fix 2.
2. **What's wrong:** two UE mannequins (the townsfolk prototype, `c_figure` 4x crop at x 1225-1260, y 860-905, and a crowd at x 1620-1750, y 920-1080 in the diff map) move between frames and are what every `measure_plume.py` item reports.
   **Fix type:** CODE. **Exact instruction:** fix 2 (hide pawns for the chimney, reference and motion frames).

## house-frontage-dusk — PASS

Lantern pane x 365-415, y 365-425 glass L 211; wall within 1 m: 1.2 % > 235, all of it the pane; bay panes glow; jetty top x 350-800, y 0-120 7.8 % > 235 from the window glow above, 2.4 m from the lantern. Capture note (fix 5): sky L 4.0, 75.6 % of the frame under L 8. That is night, not the "6 deg below the horizon" dusk the README names, and not the lighting blueprint's night row either (sun pitch -12, sky 0.0, exposure 0.25, §3).

## context-house-side-b-dusk — FAIL

Pane glows (glass L 212), the wash has a visible source, and the sign's bracket casts a correct shadow. The wall within 1 m of the lantern is clipped: x 1280-1660, y 70-450 27.0 % of pixels > 235 (max 255); the ashlar face below the lantern x 1100-1440, y 380-640 is 44.3 % clipped, mean RGB 185/119/16. WP6h 3b asked for max channel <= 235 within 1 m. 450 lm at 30 cm is still a floodlight; a lantern is one candle (~12 lm) behind amber glass.
**Fix type:** CODE. **Exact instruction:** fix 4.

## context-lamp-wall-dusk — PASS

Lantern x 445-495, y 935-1020 glass L 208; wall within 1 m 4.6 % > 235, all of it the pane. The jetty soffit x 760-1180, y 590-690 is washed to clipping (39.1 % > 235) by the lit first-floor window above it, which is a visible source; fix 4 covers it. Sky L 4.0 (fix 5).

## Context views

### context-overview-se — FAIL

1. **What's wrong:** the forest is gone. WP6h's frame had pine stands east of the wall (x 1350-1920, y 150-700) and scattered broadleaf across the south (x 100-1000, y 600-1050); WP6i has bare ochre terrain everywhere outside the wall: 16 pixels under L 70 against 30,339. The PCG dressing volumes were not generated and saved before capture.
   **Fix type:** LEVEL (pipeline). **Exact instruction:** fix 1.
2. **What's wrong:** three identical flat blue-grey rectangles, each about 20 m, in a straight row 40 m apart in the open field east of the wall (x 1420-1700, y 350-430; `c_se_slabs` 3x crop), no road, yard or fence near them. They match `silverbrook-town-layout.json` `construction_plots.centres` [[430,215],[470,215],[510,220]] at `size` 20 (graybox 'Plot' boxes, folder Plots). They were there in WP6h too, half-hidden by trees. `hide_debug_actors()` is meant to hide every graybox box in Places/Plots in game, yet these render.
   **Fix type:** LEVEL. **Exact instruction:** fix 7.
Seen, not failed (world pass, unchanged): the wall as a lake causeway, the straight canal, flat cyan water.

### context-overview-nw — FAIL

The eye is 230 m above the town-centre ground as asked, but the northern ridge still fills the lower 52 % of the frame (smooth beige slope from (0, 870) to (1920, 320)); the town is cut by it along the whole width and its southern half is hidden. The assert traces one ray to the aim point; it needs to test the bottom of the frame. **Fix type:** CODE. **Exact instruction:** fix 6.

### context-road-silvermere — FAIL

The trench is fixed: a V cutting with ~30 deg grass banks, cobbles only on the carriageway, dark earth verges. It fails with the other outside views on missing foliage: the look profile puts pines on rises and ridge lines and this is the north ridge, cut open, with no tree, rock or tuft in 120 m of bank. Seen, not failed: the bank texture smears in down-slope streaks (J2).

### context-road-everwood — FAIL

WP6h: "wide cobbled road into the woods, dirt verges with shrubs, pebbles and tufts, trees". WP6i, same seed: no tree, no shrub, no pebble, no tuft; a cobble strip across bare dunes to the horizon, one barn and the far wall at x 1150-1920, y 480-620. Fix 1. The 12 m cobble carriageway outside the wall and the smooth beige backdrop hills remain world items (J1b).

### context-road-valemere — FAIL

Same: no verge dressing or forest at all; the road climbs to a crest with a grey bedrock notch at x 870-1100, y 470-520 and the lake wall beyond. Fix 1.

### context-town-edge — PASS

Wall segments and towers on the grass; beyond the near wall the road through open ground is empty of lamps (WP6h's eight lamps in pairs are gone), the far wall and gate carry theirs. Nothing floats.

### context-house-side-a — PASS

Side wall on its plinth, buried; fence, garden bed, lamp in shadow with no flame visible (1 warm pixel). Seen, not failed: the backdrop granite dome at x 1650-1920, y 0-260 reads as a smooth pale egg in the sky from inside the town (J1b); the twin-arm cast-iron street lamp at x 100-200, y 270-530 is a 19th-century gas standard, not the concept's "plain brown post, iron bands" (`vfx.md` §6, `Street Lamp-1.png`); fix 9.

### context-house-side-b — FAIL

Plinth right, stepping to the stoop base; bracket lantern dark at noon (correct). The ornate street lamp at x 1720-1810, y 280-400 (`c_sb_lamp` 4x crop) shows an amber flame and lit glass in both lanterns at noon: 1,588 warm pixels. J5 reproduced. **Fix type:** CODE. **Exact instruction:** fix 3.

### context-house-side-rear — FAIL

Rear wall on a plinth following the slope, firewood shelter, fence, nothing floats. The lamp at x 1840-1910, y 180-330 shows a flame at noon (171 warm pixels). Fix 3.

### context-lamp-wall — PASS

A different lamp than WP6h (the seeded pick changed with the lamp set). Gable facade in shadow at noon, lantern x 430-500, y 900-1030 dark, no wash on the stone, windows dark, sign clear of the bracket. Correct noon behaviour.

### context-road-approach — PASS

The owner's image-21 spot. Both sides fronted by houses; groups: barrels x 600-700, y 500-580 and crates x 770-860 (left), sacks x 1010-1150, hay x 1090-1140, trough x 1300-1450, barrel and rack x 1480-1700 (right); no identical pair across the road, no two at one chainage, irregular gaps. Lamp post lantern x 385-430, y 330-400 shows a small flame at noon (fix 3). Field stones on the cobbles beside the lamp plinth x 390-470, y 640-660 and at x 1340-1420, y 620-640 (fix 8).

## Deferred (seen, not failed; GFX-02 / GFX-03, unchanged)

Bank texture streaks and bedrock faces on kerb-high banks (J2); backdrop dome and beige hills, 12 m cobble roads outside the wall (J1b); flat cyan water, lake causeway, canal (GFX-02 world); ashlar repeats, plaster, stall stand-in, low-poly trees (GFX-03); townsfolk mannequins (townsfolk prototype, not this WP; hidden for the plume measure by fix 2).

## Fix list (shared first)

1. **Finish the PCG dressing before saving and capturing** — Engineer, LEVEL. After `build_silverbrook.py` (and after `paint_street_edges.py`), run `py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/pcg_dressing.py` until it prints `DRESSING_SAVED` with `instances == expected` for every `PCG_Dressing` volume (it asserts while PCG is still generating; rerun after a few seconds). Add that step to the README run order and, in `build_silverbrook.py` after line 939, `u.log('PCG_FINISH_REQUIRED run pcg_dressing.py before capture')`. Acceptance: `context-overview-se.png` dark-pixel (L < 70) count >= 24,000 (WP6h 30,339); `context-road-everwood` and `-valemere` show trees, shrubs and verge pebbles as in WP6h.
2. **Visible plume** — Engineer, LEVEL + CODE. `silverbrook-props.json` `chimney_smoke`: `opacity` 0.22 -> **0.5** (max 0.8), `spawn_rate` 3.5 -> **5**; keep `tint` [0.24,0.24,0.26], `lifetime` 5.5, `rise_speed` 2.4, `wind` [0.35,0,0], sizes 0.35 / 2.8. Expected: single-puff peak 0.94 x 0.9 x 0.5 = 0.42, two overlapping puffs 0.66 at the foot, core L ~ 115 against the field's 150. Rerun `apply_dressing.py`. `measure_plume.py`: replace item 5 with "core L / ref L in **0.65-0.88** (darker than the field) over rows y 380-540 within x 840-960, |L/ref - 1| >= 0.05 at y 150, and never > 1.05 against sky rows (y < 60)"; keep items 1-4, 6-8 and the street thread (>= 15 columns). `capture_game.py` before the chimney view: hide every pawn that is not the player (`for c in u.GameplayStatics.get_all_actors_of_class(world, u.Character): c is pawn or c.set_actor_hidden_in_game(True)`) and unhide after the motion set, so people never enter the measure. Update `vfx.md` §2 values row (still "spec, not yet passing").
3. **Flame, halo and lamp wisp follow the sun gate** — Engineer, CODE, `GameClient/ArtSource/VFX/StreetLamp/build_materials.py` `build()`. M_LampFlame line 191-193: `energy = _mul(flame, _mul(flame, _mul(flame, tint, _alpha(flame, image)), _mul(flame, _flicker(flame, cfg), _constant(flame, 5))), _night(flame, cfg))`. M_LampHalo line 198: wrap the emissive in `_mul(halo, ..., _night(halo, cfg))`. M_LampSmoke line 250: multiply the opacity product by `_night(smoke, cfg)`. Rerun `lamp_fx.apply()`. Acceptance: in every noon view, no lantern box contains a pixel with max channel > 200 and R - B > 60 (`context-house-side-b.png` x 1720-1810, y 280-400 currently 1,588; `-rear` x 1840-1910, y 180-330: 171; `context-road-approach` x 385-430, y 330-400: 53).
4. **Lantern lumens** — Engineer, CODE + LEVEL. `lamp-fx.json` `light_intensity` 900 -> **450** (street lamps); `lamp_fx.py` `_point_light` line 144: `cfg['light_intensity'] * (.33 if wall else 1.)` (wall lanterns 150 lm). Also cap the window glow that clips the jetty soffit in `context-lamp-wall-dusk` (x 760-1180, y 590-690, 39 % > 235): `silverbrook-props.json` `windows` emission strength x 0.6. Acceptance at dusk: no pixel within 1 m of a wall lantern or lit window, other than the pane itself, with max channel > 235 in `context-house-side-b-dusk`, `context-lamp-wall-dusk`, `house-frontage-dusk`; panes stay >= 200.
5. **Dusk captures per the lighting blueprint** — Engineer, CODE, `capture_game.py` lines 160-163. Replace the ad-hoc `pitch=6` with the `lighting-and-atmosphere.md` §3 night row (sun pitch -12, sky light 0.0, exposure offset 0.25, applied to the directional light, sky light and post-process volume at runtime) and log `WP6_NIGHT_ROW`. Acceptance: sky at the top of each `-dusk` frame reads L 10-60 (now 4.0), the ground outside the lamp pools is visible, panes >= 200. If the row as written puts the sun above the horizon, fix the row in the blueprint (not the capture) and note the UE sign convention there.
6. **Overview frames clear of the near ridge** — Engineer, CODE, `capture_game.py` lines 68-75. Replace the single-ray assert with a bottom-of-frame test and raise the eye until it passes:
```python
VFOV = 2 * math.degrees(math.atan(math.tan(math.radians(80 / 2)) * 9 / 16))   # 50.5 deg for FOV 80 at 16:9
for name, (bx, by), alt, pitch in (('context-overview-se', (520, 640), 230, 38), ('context-overview-nw', (-430, -470), 230, 42)):
    tx, ty = 0, -30; dx, dy = tx - bx, ty - by; n = math.hypot(dx, dy)
    for alt in range(alt, 601, 50):
        e = u.Vector(bx*100, by*100, ground(0, -30) + alt*100); pitch = math.degrees(math.atan2(alt, n))
        down = math.radians(pitch + VFOV / 2)   # the ray through the bottom-centre of the frame
        bottom = u.Vector((bx + dx/n*math.cos(down)*400)*100, (by + dy/n*math.cos(down)*400)*100, e.z - math.sin(down)*40000)
        if not terrain.static_mesh_component.line_trace_component(e, bottom, True, False): break
    else: raise AssertionError(f'{name}: terrain inside the frame bottom up to 600 m')
    u.log(f'WP6_OVERVIEW_FRAME_CLEAR {name} alt {alt}')
    add(name, e, u.Vector(tx*100, ty*100, e.z - alt*100), 80)
```
   Acceptance: no smooth near-terrain band across the bottom of `context-overview-nw.png` (fraction of column height that is smooth hill from the bottom < 0.1); the whole wall ellipse in frame.
7. **Three graybox plots in the east field** — Engineer, LEVEL. In the open editor, `find_actors` within 15 m of (470, 215): if they are `ConstructionPlot_0..2` (expected), `hide_debug_actors()` is not reaching them; set `a.set_actor_hidden_in_game(True)` directly in `build_silverbrook.py` `box()` for `folder='Plots'` and add the Plots folder to `check_hidden_in_game.py`'s set; paint the terrain A channel (tilled) under each plot so the reserved ground reads as a field, not a slab. If they are Eastwood buildings, they need yards (fence, muck heap, track to the road) and differing yaws. Acceptance: `context-overview-se.png` x 1420-1700, y 350-430 shows field or tilled earth, no flat grey rectangles.
8. **No field stones on the cobbles** — Codex, CODE, `silverbrook_props.py` `dress_verge`: skip pebble/rock instances where the cobble field at the point is >= 0.5 (terrain vertex colour G at the instance, or `build_silverbrook.ROAD_G`), so scatter stays on the earth margin. Acceptance: no rock cluster inside the painted carriageway in `main-street` or `context-road-approach`.
9. **Street-lamp family** — Engineer, LEVEL (owner to confirm). The twin-arm cast-iron standard (`context-house-side-a` x 100-200, y 270-530; `-side-b` x 1720-1810) is off-concept (`vfx.md` §6: plain brown post, iron bands, amber glass). Restrict `build_silverbrook.py` `_lamp_names` (line 630) to the timber-post family seen in `context-road-approach` x 250-440, y 160-720, or regenerate the iron family's mesh to the Street Lamp-1 concept.
10. **Resubmit** — Engineer. Rebuild, fix 1 finish, all gates plus `DRESSING_SAVED`, recapture the five views, `chimney-smoke-ref`, the 14 motion frames (people hidden), the eleven context views and the three dusk views with the night row, `measure_plume.py` with the fix-2 acceptance, the 1440p perf pair.
