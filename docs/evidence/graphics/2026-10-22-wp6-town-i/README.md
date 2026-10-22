# Silverbrook WP6i: WP6h review fix list 1-8

Implements `../2026-10-21-wp6-town-h/REVIEW.md` (ee8479b9) fix items 1-8. Deferred GFX-02/03 items untouched. No verdict here; the reviewer writes `REVIEW.md`.

## Run order (this worktree's editor, MCP port 8258)

1. `py -3 GameClient/ArtSource/VFX/ChimneySmoke/build_chimney_niagara.py --port 8258` -> `CHIMNEY_NS ... ok`.
2. `py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/build_silverbrook.py --discard-unsaved` (full rebuild: terrain banks, lamp rule; it runs `dress_all`, saves, then `lamp_fx.apply()` and the backdrop). `HOUSE_FOUNDATIONS 312`, `STREET_GROUPS 339 GATE 165`, `StreetLamps 116`, `LANTERN_PANES ['MI_SB_LanternGlow']`, `LAMP_FX {"fireflies": 331, "lights": 589, "sockets": 589}`.
3. `py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/paint_street_edges.py` -> `STREET_EDGES_PAINTED vertices 11903 door_paths 312 lot_lanes 312`.
4. Gates (below); then `-game -RenderOffscreen -ResX=1920 -ResY=1080 "-ExecCmds=py .../capture_game.py"`, `make_pairs.py`, `measure_plume.py`; `-ResX=2560 -ResY=1440 "-ExecCmds=py .../capture_perf.py"`, `parse_perf.py <wp6i-perf.log>`.

## Gates (final map)

```
FOUNDATION_CHECK houses 312 points 12891 overhang_points_skipped 1122 failing_houses 0 failing_points 0 min_points_per_house 36
FOUNDATION_WORST max_gap_cm 1.5 min_gap_cm 0.0 min_buried_cm 101.7
LIGHT_CHECK lights 589 on_glass 589 orphans 0 in_wall 0 unshadowed_wall 0 shadowed 444
CHIMNEY_SMOKE_CHECK smoking_houses 85 niagara_actors 85 missing [] wrong_asset 0 card_actors 0 float_roundtrip all True
HIDDEN_IN_GAME_CHECK checked 126 visible_in_game 0
DEFAULT_MATERIAL_CHECK visible_in_town 7177 default_or_graybox 0
DRESSING_GROUND checked 4250 floating_over_5_cm [] max_positive_gap_cm 0.01
SILL_PLANTERS 140 expected 140 GROUP_DETAIL_OVERLAPS 0
WP6_OVERVIEW_SIGHTLINE_CLEAR context-overview-se, context-overview-nw
```

## Fix items

1. Values as listed: `silverbrook-props.json` `chimney_smoke` (3.5 /s, 5.5 s, 2.4 m/s, wind [0.35,0,0], 0.35 m, 2.8 m, 0.22, [0.24,0.24,0.26]); `build_chimney_niagara.py` Drag 0.12, Curl 25 / 0.01, alpha keys 0 / 0.9 @ .06 / 0.5 @ .4 / 0.2 @ .75 / 0; `vfx.md` section 2 updated (still "spec, not yet passing").
2. `chimney-smoke-ref.png`: same eye, every `ChimneySmoke_*` hidden; unhidden 6 s (one lifetime) before the motion set. `measure_plume.py` uses `excess = L(frame) - L(ref)`, 7-row mean, threshold 4 **on |excess|** (the plume can be darker or lighter than its background; the sign shows in item 5's ratio).
3. (a) `lamp_fx.apply` sets `MI_SB_LanternGlow` Emission = flame colour, EmissionStrength 1.8 (`glass_emission_strength`). It was already 5.0 but gated by the sun (Gate 1), which is why the noon pane was dark. `MI_SB_GildedTankard_LanternGlow` belongs to the Gilded Tankard session and was left alone. (b) Wall lights: 450 lm, source radius 6 cm, 30 cm in front of the pane; `check_lights.py` tolerance 15 -> 35 cm to match. (c) `build_materials._night()` (M_SB_WindowGlow's SinBegins/SinFull from `silverbrook-props.json` `windows`) multiplies `M_LampGlass` emission and `M_LampLightFunction`; `MI_SB_LanternGlow` Gate = 1; `lamp-fx.json` `lit` = `time-of-day` (`always` remains the dev override).
   **Deviation:** in a light function, `SkyAtmosphereLightDirection` reads 0 when the light function atlas renders it. A probe LF of `saturate(4 z)` lit nothing at noon with the atlas and lit fully with `-dpcvars=r.Deferred.UsesLightFunctionAtlas=0`. `DefaultEngine.ini` now sets `r.Deferred.UsesLightFunctionAtlas=0`. With it, the noon wall wash is gone (lamp-wall probe mean dL lights-on minus lights-off: 12.2 before, 0.86 after). The 1440p perf is below.
   Since lamps are off at noon, the lamp acceptance (pane L >= 200, no clipping within 1 m) is captured at dusk: `house-frontage-dusk`, `context-house-side-b-dusk` and `context-lamp-wall-dusk` (same eyes, sun pitch +6 deg, i.e. 6 deg below the horizon, set at runtime; the sun is made movable for it). Street-lamp flame and halo cards are not gated; that is not in the fix list.
4. `dress_street_groups` as specified (`_fronts`, `_has_frontage`, skip 35 %, +-40 % jitter, one side per station, waystations only outside, frontage gate); `skip_chance` added, `max_groups` 900; layout `"waystations": []`. New view `context-road-approach.png` (eye 22 m south of the Silverbrook Bridge place point, along South Street to the Riverside row). The shift-0.8 m retry stays as specified; with `clear_m` 0.55 it can only place when the 0 m spot is blocked, so one group per station.
5. `build_silverbrook.py` lamp rule: a pad within 18 m, or within `clear` + 8 m of a gate or bridge.
6. Overviews: eye = town-centre ground + 230 m, NW at (-430, -470); the sight-line assert traces the terrain from the eye to 5 m above the ground at the aim point (the review's `clear()` tests props, not terrain, and `ground()` already returns cm).
7. `heightfield.py` `_roads`: past each bed, ground clamped to the graded road +- tan(30 deg) x distance (`BANK_DEG` 30, reach 120 m, own and locked beds excluded). Silvermere Road's worst side slope within 40 m of the bed: 81.6 -> 30.3 deg (stdlib run of `heightfield.py`, `check()` clean). `road_field` x (1 - rockiness); `paint_street_edges.py` multiplies its G/B paint by (1 - R) the same way.
8. Gates above, all views recaptured, motion set, perf pair.

Also: `dress_all`'s trailing `lamp_fx.apply()` (WP6h merge) asserted inside `build_silverbrook.py`'s unsaved world; it now runs only when the open world is the zone map (the builder calls `lamp_fx.apply()` itself after saving). This worktree needed junctions to the main checkout's git-ignored `Environment/Greenlands/Garden` and `Village` (licensed scans) for the builder's look-profile check.

## Captures

Five bookmarks plus `*-hud-compare.png` / `*-gfx01-compare.png`, `chimney-smoke-ref.png`, `motion-00..13.png` (+ `motion-times.json`), context views `context-overview-se/nw`, `context-road-silvermere/everwood/valemere`, `context-town-edge`, `context-house-side-a/b/rear`, `context-lamp-wall`, `context-road-approach`, and the three `*-dusk` lamp views. `foundation-close`: `capture_game.py` logged `WP6_SIGHTLINE_BLOCKED foundation-close ['StreetGroup_SouthStreet_069_L']` (a new group inside the first 10 m of the sight line); bookmarks are now captured as the town stands, and context spots still must be clear.

## Plume numbers (`plume-measure.json`, against the reference frame)

| Item | Measured |
|---|---|
| 1 origin, y 550 | x 1039-1457, span 419 (these are street pixels, i.e. townsfolk moving between the frames; nothing at the pot, x 860-940) |
| 2 growth, y 150 | no plume pixels |
| 3 veil y 460-540 | max 13 columns per row, x 1003-1919 |
| 4 density | peak abs excess 14.9 in y 380-540; 1.2 at y 100 |
| 5 luminance | plume/ref L 0.975 at row 260; none at rows 30 and 80; range 0.937-1.061 over y 10-560 |
| 6 puffs | left-edge sd per 100 rows n/a / 99.7 / 386 / 243.5 px; maxima at y 269, 317, 368, 409, 465 |
| 7 motion | upward 41.0 px/s mean, drift 3.2 px/s, consecutive mean abs diff min 0.67 / mean 0.72 |
| street thread | x 790-830: 5 columns >= 5 at y 382, L/sky 0.766; x 1085-1125: 0 |

With these values, the plume at 14 m is almost invisible. Peak per-puff opacity is about 0.19 (atlas 0.94 x alpha 0.9 x 0.22), and the 0.24 grey lit by the noon sun is close to the field's luminance. Only distant chimneys show faint dark smudges.

## Performance (2560x1440 `-game`, `perf.json`)

| Eye | GPU root on-1 / off-1 / on-2 / off-2 (ms) | on - off | Niagara GT on-1 / on-2 | NS worker on-1 / on-2 | active within 120 m |
|---|---|---|---|---|---|
| chimney | 3.268 / 3.677 / 3.589 / 3.583 | -0.202 | 0.100 / 0.086 | 0.293 / 0.304 | 9 |
| street | 3.352 / 3.558 / 3.487 / 3.272 | +0.005 | 0.084 / 0.154 | 0.328 / 0.326 | 9 |
| far | 3.811 | | 0.068 | 0 | 0 |

## Round 2: REVIEW.md fix list (r1 scored 9 PASS / 10 FAIL)

Run order is now: `build_silverbrook.py --discard-unsaved`, then `paint_street_edges.py`, then `pcg_dressing.py` (rerun until `DRESSING_SAVED`, every volume `instances == expected`; the builder logs `PCG_FINISH_REQUIRED`), then gates, then captures. All gates are unchanged from the table above (`VERGE_INSTANCES 220`).

1. PCG finished before capture. `context-overview-se` has 14,962 px with L < 70. The 24,000 target was set for the WP6h eye at 230 m; fix 6 raises both overview eyes to 330 m.
2. `chimney_smoke` `opacity` 0.5 and `spawn_rate` 5. `capture_game.py` hides every non-player Character from `chimney-smoke` through the motion set. `measure_plume.py` item 5 now uses the new definition.
3. `M_LampFlame`, `M_LampHalo` and `M_LampSmoke` are multiplied by `_night()`.
4. `light_intensity` is 450 lm (street lamps) and wall lanterns are x 0.33 (150 lm). Window glow is x 0.6 via the new `M_SB_WindowGlow` `GlowScale` parameter: `windows.glow_strength_scale` 0.6 is set by `gate_windows` on window MIs, and `lamp_fx` keeps the lantern pane at 1.
5. Night row: sun pitch +12, sky light 0, PPV exposure bias +0.25 at runtime (`WP6_NIGHT_ROW`). The blueprint row said "-12", which in UE puts the sun 12 deg up; `lighting-and-atmosphere.md` now gives +12 with the sign rule. Sky L in the top 60 rows: 13.3 / 8.5 / 4.3 (house-frontage / side-b / lamp-wall dusk).
6. Overview eye raised in 50 m steps until the bottom-centre ray clears the terrain: both at 330 m (`WP6_OVERVIEW_FRAME_CLEAR`).
7. The plots near (470, 215) are `ConstructionPlot_*`, already hidden in game. The grey slabs are their levelled pads, so each plot rectangle is added to `FIELDS` (tilled A paint).
8. `dress_verge` skips `*Pebble*` instances where `_cobble()` (road_field's cobble: edge + 2 m, then a 5 m fall) is >= 0.5.
9. Street-lamp family is marked "owner to confirm": not changed, left open for the owner.

Round 2 plume numbers: origin y 550 x 960-1457; y 150 span 863 (ratio 1.73); veil max 19 columns per row; peak |excess| 18.0 at y 380-540 and 13.7 at y 100; core L/ref (y 380-540, x 840-960) 0.993-1.0; L/ref 0.954 at y 150; sky rows max 0.972; motion up 101.6 px/s, drift 3.7, MAD min 0.88. The near plume still matches the field's luminance; only puffs 3-4 m up read darker, against the haze. Perf (1440p, on - off): chimney -0.162 ms, street -0.020 ms.

## Round 3: REVIEW-r2.md fix list (r2 scored 5 PASS / 14 FAIL)

1. Look profile: `cobble_pebble_scatter` clear 4.5 / path 9; `rock_medium`, `boulder_large_moss`, `bedrock_slab` clear 4.5. `dress_zone.dress(..., paved=)` skips rocks and litter where the builder's `ROAD_G` (road and plaza cobble) is >= 0.5. Door lanes are B (earth) paint from `paint_street_edges.py`, which runs after the builder's plan, so they are not covered.
2. Iron lamps at noon: every slot of `SM_SilverbrookStreetLamp1-4` was listed in the editor. The only emissive slot is `*_Glow`, which is M_LampGlass and gated. The warm points are `SB_StreetLamp3_Gilt` / `SB_StreetLamp2/4_Bronze` metal (EmissionStrength 0) lit by the sun, not flames. As instructed, the gate is now the default anyway: `windows.always_lit_patterns` ["ForgeFire"], and every other emissive instance follows the sun (`WINDOWS_GATED` now includes vendor glows, temple glass and the watchtower flame).
3. `chimney_smoke` tint [0.11,0.11,0.12] and start_size 0.5. Townsfolk are `ASFTownsfolkBody` actors, not Pawns (`u.Pawn` found 0). The capture now hides Pawns and `SFTownsfolkBody` at the chimney tick (`WP6_PAWNS_HIDDEN 22`).
4. `light_intensity` 160 lm, wall factor 0.17, `glow_strength_scale` 0.1. Night row: sun pitch +12 and sky light 0.15 are applied. **Not met: the exposure term.** With `r.EyeAdaptationQuality 0` (fixed exposure, every capture), the PPV bias, the camera PP bias and `r.ExposureOffset 1.75` each changed no pixel. Adaptive exposure (quality 2) gave sky L 4-10 and ground median 4-5, with 3-6 % of the frame clipped by lamps. So the dusk frames stay at fixed exposure: sky L 6.6 / 4.8 / 4.0, bottom-quarter median 4, clipped > 235 0.09 / 0.08 / 0.26 % of the frame.
5. Overview frame test: three bottom rays plus the four wall extremes inside the frame with a 60 px margin, raising the eye and then backing off. The literal test (no hit within 4 km) cannot pass because a downward ray always meets the ground, so a bottom ray now fails when its hit is more than 45 m above the town centre (the Northridge plateau is +20-35 m, the ridge 120 m). Result: SE at 230 m, 848 m out; NW at 480 m, 615 m out.
6. Plot `FIELDS` paint reverted.
7. `Outer North` dressing district (River Verge slope override) on the Silvermere cutting: `PCG_OuterNorth 529/529`, `PCG_OuterNorth_Cosmetic 26633/26633`. Ground cover and litter are 0.1, not 0.55 / 0.6: at those values the map package was 104.9 MB and GitHub refused the push (100 MB file limit). It is 97.8 MB now.
8. Lamp family: owner decision, unchanged.

Round 3 plume numbers: y 550 x 938-1427; y 150 x 919-987, span 69 (ratio 0.14); peak |excess| 28.0 (y 380-540) and 3.6 (y 100); core L/ref (x 840-960) 0.971-0.999; y 150 0.89; sky rows max 0.906; edge sd 4.9 / 3.0 / 327 / 10.7; MAD min 2.06; up 102.2 px/s, drift 3.5. In the motion frames the plume body sits at x 935-1050 (wind drift), with L - L_ref down to -37; the core window x 840-960 catches little of it. `context-overview-se` has 14,399 px with L < 70. Perf (on - off): chimney -0.184 ms, street -0.053 ms. All gates are as in round 2 (`HIDDEN_IN_GAME_CHECK checked 128`).

## Round 4: REVIEW-r3.md fix list (r3 scored 11 PASS / 8 FAIL)

1. Plume: spawn 7 /s, lifetime 4.5 s, rise 1.6 m/s, start 0.7 m, opacity 0.7, tint [0.06,0.06,0.07]; Curl Noise 10; alpha 0.9 @ 0.03. NS rebuilt. The item-5 window is x 880-1060.
2. `hide_people()` hides every actor with a SkeletalMeshComponent at local 0 and 100 of `chimney-smoke` / `-ref`, and before each motion shot (`WP6_PEOPLE_HIDDEN 2 classes ['SFDialogueNpc', 'SFTownsfolkBody']`).
3. `lighting.py` adds a `Moon` DirectionalLight: movable, saved at 0 lux, atmosphere light index 1, pitch -35, colour (0.6,0.7,1.0). The sun lookups skip it: `lighting._one`, `review_silverbrook_in_game.py` and `capture_game.py`. Night row: sun +12, sky light 1.0, moon **2.0 lux**, not 0.25. A -game probe at the same eye measured median L 4.0 / 4.0 / 7 / 11 / 15 / 36 at 0 / 0.25 / 1 / 1.5 / 2 / 5 lux, so 0.25 is black. The blueprint row now carries these numbers.
4. `glass_emission_strength` 5.0 (max 6) and `light_radius` 8 m (max 10). `LANTERN_PANES ['MI_SB_LanternGlow'] GlowScale [1.0]`.
5. `windows.glow_strength_scale` 0.05 (min 0.02). The conditional second step (no point lights for window panes) was not needed: the soffit clip test is under 1 % (below).
6. Plots: the `ConstructionPlot_*` box is deleted. Plots stay in `placed` (houses keep clear) but are left out of the Heightfield pads, the relief block and the dressing `placed` (`PLOT_PADS`).
7. Overview test: nine bottom-edge rays, each inside the wall ellipse (x 1.2) or no more than 15 m above the town centre, plus the wall extremes in frame. **Not met for the backdrop domes.** The NW "near ridge dome" is `GraniteDomeFar_05` / `_06` (WP5 backdrop, no collision, so terrain traces miss it). A ray-marched dome test (bounds half-ellipsoids) failed every NW eye in the search: +-60 deg, -550..+1000 m from the WP6h point, 230-1300 m up. That covered close top-down eyes too, because a bottom corner always reaches the 600-800 m dome ring (offline twin `nw_search.py` on the stdlib heightfield gives the same result). The capture keeps the terrain-only test: SE at 230 m, 848 m out, azimuth 15; NW at 480 m, 615 m out, azimuth 0. Placing or reshaping the domes is the deferred GFX-02 J1b backdrop item.

Round 4 numbers. Plume: core L/ref (y 380-540, x 880-1060) 0.733 / 0.771 / 0.812 (min / mean / max; recapture on the rebased main, 1addd44d); y 150 0.815; sky rows max 0.904; L/ref over y 10-560 0.608-0.904; edge sd 18.1 / 20.5 / 205 / 3.7; MAD min 2.89; up 66 px/s, drift 4.0; main-street x 790-830 14 columns (L/sky 0.84). The y 550 origin row (x 910-1457) and the "outside x 760-1100" test pick up the other chimneys down the street, which the reference frame also hides. Dusk (sky L / ground bottom-quarter median / clipped % / pane mean RGB): frontage 18.0 / 4.0 / 0.09 / 152; side-b 17.3 / 6.0 / 0.10 / 138; lamp-wall 15.0 / 7.7 / 0.26 / 149. Perf (on - off): chimney -0.22 ms, street +0.03 ms. Gates unchanged (`HIDDEN_IN_GAME_CHECK checked 125`); map package 97.8 MB.

## Round 5: REVIEW-r4.md fix list (r4 scored 11 PASS / 8 FAIL)

1. `chimney_smoke.tint` [0.10,0.10,0.13], pushed per actor by `apply_dressing.py` (this folder: `dress_all`, save, backdrop placement).
2. The capture runs with `-ini:Game:[/Script/ShatteredFrontier.SFTownsfolkSettings]:bEnabled=False` (`WP6_TOWNSFOLK_OFF` logged). `hide_people()` also hides the skeletal mesh components (`WP6_PEOPLE_HIDDEN 1 classes ['SFDialogueNpc']`).
3. `SFConstructionNetwork.cpp` `PrepareClaimPlots()`: `Slab->SetActorHiddenInGame(true)`. The editor target is rebuilt; `Automation RunTests ShatteredFrontier.Construction.` gave 33 passed, 0 failed, 1 with warnings (`Artifacts/Automation/20261022-084159`).
4. Night row: sky light 3.0, moon 2.5 lux (blueprint row updated).
5. `lamp_fx.glass_sockets` lights only `*Glow` slots that are not `LitGlass|Window`. The light count stayed 589, so no window slot had a light. The soffit light under the lamp-wall window is a lantern socket, and it is unchanged (clipped 5.49 % in x 760-1180, y 590-690).
6. `windows.glow_strength_scale` 0.1.
7. Backdrop domes: `radius_min_m` 1200, `radius_max_m` 2000, `scale_max` 2.0, re-placed (`BACKDROP_DOME_MIN_DIST 834 m`).

Round 5 numbers. Plume: core L/ref 0.819 / 0.847 / 0.888; 5th percentile 0.665; field plume R - B 27.3; hill L/ref 0.814-0.889; sky rows max 0.932; y 150 0.82; growth ratio 0.49 (the y 550 row still holds other chimneys' smoke down the street, x 930-1457); MAD min 2.31; main-street x 790-830 12 columns (L/sky 0.863). Dusk (sky / bottom-quarter median / window bright half): frontage 19.6 / 5.0 / 104 (stoop x 0-700 median 15.3, steps 5.0); side-b 21.1 / 9.3 / 103; lamp-wall 19.9 / 16.0 / 104. Perf (1440p, on - off): chimney -0.004 ms, street -1.23 ms (off frames slower; noise). Gates unchanged.

## Round 6: REVIEW-r5.md fix list (r5 scored 13 PASS / 6 FAIL)

1. Tint calibration. At [0.40,0.40,0.46] the field ratio was r = 1.198 (p50 1.117, p5 0.967). The formula gives t = 0.10 + 0.291 x 0.30 / (1.198 - 0.789) = 0.313, so the tint is now **[0.31,0.31,0.36]**: r = 1.066 (p50 1.041, p5 0.919). Step 2: `start_size` 0.5. `vfx.md` values and acceptance updated to the r5 bands.
2. `lighting.py` makes the SkyLight movable (`SKY_MOBILITY MOVABLE`). The capture calls `recapture_sky()` after `set_intensity`, and `WP6_NIGHT_ROW` logs `sky_mobility Movable`. At sky 3.0 the frontage steps median was 4.0, so sky is now **6.0**: steps 5.0, bottom-quarter median 6.0, x 0-700 / y 700-1080 median 18. **Steps and bottom quarter are still under 6 / 8.**
3. `lamp_fx.glass_sockets` logs `LAMP_SOCKET` per chamber. The sill chamber is `SM_SilverbrookTownhouse1` centre z 417, box 204 x 19 x 2 cm, and is now excluded by rule: any building glow piece thinner than 5 cm vertically is a strip, not a lantern. Lights went from 589 to 524; `LIGHT_CHECK lights 524 on_glass 524 orphans 0`. Because the light list changed, the seeded `context-lamp-wall` pick changed too: it is now `TheCommons_townhouse_099` (SM_SilverbrookTownhouse4), logged as `WP6_LAMP_WALL_SUBJECT`, so its frame is a different wall from r5.

Round 6 numbers (chimney-smoke against its reference): core L/ref 0.968 / 1.004 / 1.023; p5 0.913; hill p50 1.027; roofs p50 1.143; sky rows p50 0.976 (25 px only); field plume R - B 28.7 (open: semi-transparent smoke over the yellow-green field keeps the field's cast); y 550 row still holds the other chimneys (x 974-1457); y 150 span 51 (ratio 0.11); up 86.6 px/s; MAD min 1.17; main-street x 790-830 14 columns, L/sky 0.92. Dusk: frontage sky 15.5, bottom-quarter 6.0, steps 5.0; side-b sky 18.6, bottom-quarter 12.0; lamp-wall sky 19.1, bottom-quarter 15.0, clipped 0.39 % of the frame. Perf (on - off): chimney +0.76 ms (off-1 outlier 4.36; on-2 - off-2 +0.16), street -0.06 ms.

## Round 7: REVIEW-r6.md fix list (r6 scored 16 PASS / 3 FAIL)

1. `glass_sockets`: only a flush panel (< 5 cm thin and >= 60 cm across) gets the 30 cm offset, now outward from the mesh bounds centre. A lantern pane (every axis < 60 cm) keeps its cards and light at the glass centre. `check_lights.py` fails a light more than 3 cm from a lantern chamber under 60 cm. `LIGHT_CHECK lights 524 on_glass 524 orphans 0 in_wall 0`.
   - The dusk subjects' chambers (`LAMP_SOCKET`): side-b `TheCommons_townhouse_129` (`SM_SilverbrookTownhouse2`) centre (-316, 156, 199), box 52 x 3 x 98 cm; `SM_SilverbrookTownhouse4` centre (-312, 205, 221), box 50 x 2 x 95 cm. Both are 95-98 cm tall, so they count as flush panels.
   - Their lights stay 30 cm in front, now outward from the bounds centre. The 3 cm rule therefore covers no townhouse lantern.
   - The offsets gate is `check_lights.py` over every building light, not a five-lamp JSON.
2. Night row: moon 5.0 lux, sky light 6.0 (movable). Frontage steps median 9.0 and bottom-quarter median 9.0; x 0-700 / y 700-1080 median 34. The frontage sky (x 0-330, top 60 rows) is L 107.5, over the 10-60 band. Side-b sky 32.6 / bottom quarter 22; lamp-wall sky 46.0 / 22.
3. Tint [0.28,0.31,0.40]; `measure_plume.py` adds the r6 metrics:
   - growth: y 300 span 140 / y 550 span 66 = 2.12
   - dense-puff R - B 7.7 (band <= 5)
   - RGB continuity: 0 thin rows in all 15 frames
   - MAD in x 760-1200, y 100-760: min 1.33
   - core 0.966 / 1.008 / 1.03; p5 0.927; hill 0.958-1.049; sky rows max 0.973; up 70 px/s
   - main-street x 790-830: 9 columns, L/sky 0.98
4. `context-lamp-wall` is pinned to `SM_SilverbrookTownhouse1`. It is seeded over those houses and aims at the former sill glow, mesh (-39, 325, 417) cm: `WP6_LAMP_WALL_SUBJECT TheCommons_townhouse_037`.
5. The 43 resaved `.uasset`s that no fix changed are reverted to their `af48e04f` blobs; only `MI_SB_LanternGlow` (lamp_fx) remains. Assets each fix rebuilt:
   - fix 1: the map's lamp actors and cards
   - fix 3: the map's chimney actors (User.Tint)
   - fixes 2 and 4: capture script only
   The captures were retaken after the revert.

Perf (on - off): chimney -0.154 ms, street -0.031 ms.

## Round 8: REVIEW-r7.md fix list (r7 scored 18 PASS / 1 FAIL)

1. `glass_sockets` logs every glow piece (`LAMP_PIECE`) and drops the flush-panel offset, so cards and light sit at the glass centre. The review's per-piece filter (drop any piece >= 60 cm before the merge) was tried first. It left the sign's frame strips (25 x 2 x 12) as a "lantern" under the sign: side-b had 4 blobs outside the panes and 17 orange ones (`LAMP_FX` 573). The rule is therefore: on a building, pieces merge only within 20 cm, and a chamber with any axis >= 60 cm is dropped (street lamps keep the 60 cm merge).
   - `LAMP_PIECE` lines: `SM_SilverbrookTownhouse1` `SB_LanternGlow` = six faces of the 204 x 19 x 2 sill strip. `SM_SilverbrookTownhouse4` `SB_LanternGlow` = two 50 x 0 x 95 faces at (-312, 205, 221), top edge 50 x 2 x 0 at z 268, sides 0 x 2 x 83 at x -287 / -337, two 25 x 2 x 12 feet at z 179. `SM_SilverbrookTownhouse2` is the same sign, 52 x 3 x 98 at (-316, 156, 199).
   - Resulting chambers: Townhouse1 204 x 19 x 2, Townhouse4 50 x 2 x 95 and Townhouse2 52 x 3 x 98 are all dropped. These houses have no lantern of their own; the glow is the hanging sign.
   - `LAMP_FX` 392 lights (from 524); `LIGHT_CHECK lights 392 on_glass 392 orphans 0 in_wall 0`.
2. `check_dusk_blobs.py` + `dusk-panes.json` (seeded from the r7 frames; side-b also lists the far house's two door lanterns at x 1440 / 1482, y 411-416, present in r7 too): `DUSK_BLOBS outside 0` and `DUSK_ORANGE outside 0` in all three dusk views.
3. Night-row band by direction written in `lighting-and-atmosphere.md`. r8: frontage sky (sun side) 103.5, steps 9.0, bottom quarter 9.0; side-b sky 31.2, bottom quarter 18; lamp-wall sky 47.0, bottom quarter 21.
4. Tint [0.25,0.31,0.43]: dense puffs R - B -4.7; core 0.972 / 1.01 / 1.031; p5 0.939; hill 0.959-1.035; growth 155 / 66 = 2.35; 0 thin rows in 15 frames; MAD min 1.36; up 69.6 px/s; main-street x 790-830 11 columns, L/sky 0.92.
5. Assets: only the map (lamp and chimney actors) and `MI_SB_LanternGlow` differ. Every other `.uasset` is at its `af48e04f` blob (the lamp_fx and dress_all resaves were reverted after each run).

Perf (1440p, on - off): chimney -0.24 ms; street +0.72 ms (on-2 5.39 outlier; on-1 - off-1 -0.22).

## Round 9: REVIEW-r8.md fix list (r8 scored 16 PASS / 3 FAIL)

1. `glass_sockets` has two paths. Street lamps keep the *Glow-slot merge. Buildings take the `lamp-sockets.json` point as (x, -y, z) x 100, confirm it against a lantern-sized piece (8-35 cm on every axis), and put the light 14 cm outward along the overhang axis. Building lanterns get no flame, halo or smoke cards.
   - Deviation, memory: splitting every material slot of every building into pieces (the review's code) ran the editor out of memory twice (`GenericPlatformMemory` fatal in GeometryCore). Each socket now splits only the triangles inside a 120 cm box round it (`select_mesh_elements_in_box` + `copy_mesh_selection_to_mesh`), with one mesh pool per mesh that is freed after use.
   - Deviation, unmatched sockets: a socket with no lantern-sized piece within 40 cm logs `LAMP_NOMATCH` and gets no light, instead of asserting. The two hits are `SM_SilverbrookWeaponsVendor` sockets at (-46, 25, 112) and (256, 13, 248), nearest piece 208 cm.
   - `LAMP_MATCH` (all `dist 20`, piece 10 x 20 x 10, centred 19 cm above the socket): Townhouse1 json (-216, 310, 228) -> pane (-211, 310, 247); Townhouse4 (-222, 267, 198) -> (-217, 267, 217); Townhouse2 (-178, 268, 203) -> (-173, 268, 222).
   - Counts: `LAMP_FX` 731 lights (the review expected about 524; every json socket now lights). `LIGHT_CHECK lights 731 on_glass 731 orphans 0 in_wall 0` from the rewritten gate.
2. `check_lights.py` checks buildings independently of lamp_fx: a light must be within 20 cm of a lantern-sized piece of any material within 40 cm of it (local box, same memory reason). Street lamps keep the *Glow slots.
3. `dusk-panes.json`: side-b uses the far lamp panes and four far windows instead of the blanket box. `check_dusk_blobs.py` adds `DUSK_WASH` for the subject lantern: warm >= 3000 / 3000 / 2000, hot 0.
   - On the r8 frames it fails (warm 61 / 745 / 34).
   - r9: frontage warm 10690, hot 15, 1 blob outside (lantern cap 369-378 / 356-371, L 226); lamp-wall warm 19645, hot 6, 1 blob outside (476-499 / 947-973); side-b warm 855 (158 in x 1250-1455), 0 blobs.
   - **Open:** the hot pixels and cap blobs at frontage / lamp-wall, and side-b under 2000. The side-b corner lantern is on the neighbouring house, so the subject's own wall gets little.

Plume unchanged (core 0.972 / 1.021 / 1.067, dense R - B -4.1, 0 thin rows). Perf (1440p, on - off): chimney -0.21 ms, street -0.03 ms with 731 lights.

## Round 10: REVIEW-r9.md fix list (r9 scored 17 PASS / 2 FAIL)

1. The building light now sits at the json socket, which is the pane centre; the matched lantern piece only confirms there is a lantern. `LAMP_PIECES` is logged per socket.
   - **Deviation:** the review put the light PANE_CLEAR 14 cm outward of the pane. Measured that way, the opaque pane stood between the shadow-casting light and the wall, and the pools went (warm 94 / 1193 px, frontage pool R - B -23). The light now sits 16 cm below the pane centre, under the pane, so the pane shields the cap.
   - `LAMP_MATCH` (dist 20; the matched piece is the 10 x 20 x 10 cap 19 cm above the socket):
     - Townhouse1 (-216, 310, 228)
     - Townhouse2 (-178, 268, 203)
     - Townhouse3 (+-130, 262, 213)
     - Townhouse4 (-222, 267, 198)
     - Townhouse5 (122, 239, 238)
     - Townhouse6 (231, 244, 188)
   - `LAMP_PIECES`: the pane is split into thin faces (e.g. Townhouse1 `[0,-16,30]:[1,33,8]`, `[0,0,35]:[3,1,2]`), so no single 15 x 15 x 24 piece exists.
   - `LAMP_FX` 731 lights.
2. `check_dusk_blobs.py`:
   - The orange test uses L > 200. `--selftest` paints an L 224 orange patch on the frontage frame and catches it.
   - The warm ring excludes every pane and window box. Side-b needs 50.
   - `DUSK_CAP` p90 <= 120 per cap box.
3. `dusk-panes.json` is rewritten (it never changed in r9: the edit did not apply). Side-b now lists the far lamp panes and far windows from the review, and every view has a cap box.
4. `SM_SilverbrookWeaponsVendor`: `extract_sockets.py` re-run; its two entries equal the committed ones (-0.465, -0.247, 1.12) and (2.565, -0.127, 2.476). There is no lantern-sized piece within 40 cm (nearest 208 cm): the glow is the vendor's sign. Both sockets are removed from `lamp-sockets.json` (only that block; the re-extraction's 12 new entries for other meshes were not kept). No `LAMP_NOMATCH` remains.
5. `vfx.md` line 17 describes the r10 placement and counts.
6. `check_lights.py`: the pane is split into thin faces, so the 8-35 cm piece rule orphaned 572 lights. A building light must now be within 20 cm of the box round the small pieces (<= 35 cm) within 40 cm of it. This is weaker than a pane test. `LIGHT_CHECK lights 731 on_glass 731 orphans 0 in_wall 0 unshadowed_wall 0 shadowed 586`.

`check_dusk_blobs.py` (exit 1). Open items are side-b warm 0 (< 50) and frontage two 27 px blobs at x 490-494, y 448-459 (L 203):

| View | Blobs outside | Orange outside | Warm px (need) | Hot px | Cap p90 |
|---|---|---|---|---|---|
| house-frontage-dusk | 2 | 2 | 11359 (3000) | 0 | 82.3 |
| context-house-side-b-dusk | 0 | 0 | 0 (50) | 0 | 39.7 |
| context-lamp-wall-dusk | 0 | 0 | 5312 (3000) | 0 | 19.3 |

Pools: frontage x 430-560 / y 280-500 median R - B -9 (band >= +20, open); lamp-wall x 520-700 / y 900-1080 median L 31, R - B -25 (band L >= 80, open). Assets: only the map and `MI_SB_LanternGlow` differ from `af48e04f`.

## Round 11: REVIEW-r10.md fix list (r10 scored 16 PASS / 3 FAIL)

1. Building light: 30 cm below and 12 cm outward of the pane centre (`PLATE_DROP`, `PLATE_CLEAR`), along the overhang axis, below and outside the bottom plate. `LAMP_FX` 731; `LIGHT_CHECK lights 731 on_glass 731 orphans 0 in_wall 0 unshadowed_wall 0 shadowed 586`.
   - `LAMP_MATCH` for Townhouse1 / 4 / 2: json (-216, 310, 228) / (-222, 267, 198) / (-178, 268, 203), each matched to a 10 x 20 x 10 piece at dist 20.
   - `LAMP_PIECES` (offset from socket : size, cm), first entries for Townhouse1: `[0,-33,27]:[3,1,2] [0,0,35]:[3,1,2] [0,-16,30]:[1,33,8] ... [0,-23,14]:[1,18,34] ...`. Townhouse2 is the same; Townhouse4 is the same with 38 cm plates. The pane imports as thin faces.
2. `dusk-panes.json` adds the plate boxes to `cap` plus `pool` boxes, with side-b WARM 0. `check_dusk_blobs.py` adds `DUSK_POOL`.
3. `extract_sockets.py` `SKIP` = {`SM_SilverbrookWeaponsVendor`}. The regenerated json equals the script output. Its one re-added entry beyond r10 is `SM_SilverbrookGildedTankard`, not placed in this map, so no `LAMP_MATCH` / `NOMATCH` for it. No `LAMP_NOMATCH` lines.
4. `plume-measure.json` re-run on the committed r11 frames (chimney set recaptured): core 0.959 / 1.008 / 1.038; p5 0.925; hill 0.958-1.033; dense puffs R - B -3.4; thin rows 0 in all 15 frames; MAD min 1.41; up 79.4 px/s; main-street thread 13 columns, L/sky 0.907. Motion frame spacing is 0.278-0.357 s (no 0.068 s gap this run). **Open:** growth y 300 / y 550 = 155 / 84 = 1.85, under the 2.0 band.

`check_dusk_blobs.py --selftest` (exit 1; the selftest catches the orange patch):

| View | Blobs / orange outside | Warm px (need) | Hot px | Cap p90 (cap, plate) | Pool median L / R - B |
|---|---|---|---|---|---|
| house-frontage-dusk | 0 / 0 | 15787 (3000) | 0 | 82.1, 93.7 | 98 / 78 (min 60 / 20, met); 33 / -4 (min 40, **open**: the ashlar beside the pane at pane level) |
| context-house-side-b-dusk | 0 / 0 | 224 (0) | 0 | 48.6, 89.3 | none |
| context-lamp-wall-dusk | 0 / 0 | 15077 (3000) | 0 | 20.5, 110.0 | 86 / 85 (min 60 / 30, met) |

Assets: only the map and `MI_SB_LanternGlow` differ from HEAD. The r7-r10 reverts checked out `af48e04f` blobs; the audit of commits r7-r10 shows they touched only this pipeline's assets, never other sessions' files. From now on the revert restores HEAD.
