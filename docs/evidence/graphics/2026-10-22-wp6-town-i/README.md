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
