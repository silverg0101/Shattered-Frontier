# Street-lamp VFX pass 3 — independent concept review

Reviewer: independent (did not build the work). Concept: `docs/Game-Design/concept-art/Silverbrook/Street Lamp-1.png` plus the owner's ask (warm glow, slight smoke, occasional firefly; earlier complaints: blue lamps, detached flame "sperm" shapes). Live pairs: `day-close`, `dusk-close`, `night-close`, `dusk-street`, `night-street` `*-compare.jpg` in this folder. Implementation read: `GameClient/ArtSource/Greenlands/lamp_fx.py`, `GameClient/ArtSource/Greenlands/lamp-fx.json`, `GameClient/ArtSource/VFX/StreetLamp/build_materials.py`, `generate_textures.py`, `README.md`, `five-lamp-offsets.json`, `capture_game.py`.

Measurements below are from the 1920x1080 live PNGs (not the JPG pairs). The lantern glass in the close views occupies roughly x 860–1000, y 260–440 (about 130 x 170 px).

Result: 0 PASS / 5 FAIL. The two earlier complaints are resolved (no cyan anywhere: no live pixel in any view has B > R; no flame or ember card is offset from its glass — `five-lamp-offsets.json` shows all flame/halo origins at 0 cm, the 112.8 cm entry on StreetLamp_004 is its second socket). What remains is the concept match itself.

## day-close — FAIL

1. **Glass is a flat clipped yellow slab, no flame.** The whole chamber averages RGB (237,197,108) in the chamber box; the flame cards show only as a 10 px wide x 46 px tall white sliver (47 pixels with all channels >= 235) at x 927–936, y 322–368. Concept: dark amber glass at the edges, a soft teardrop flame with a pale core in the middle. Root cause: `M_LampGlass` emissive = `flame_colour * glass_emission_strength` = (1,.38,.08) x 1.8, a constant with no gradient, through two-sided translucent glass at opacity .22; the tone mapper desaturates it to pale yellow and buries the flame cards (10 x 22 cm, emissive x5) in it.
   Fix type: LEVEL (JSON) + CODE (script). See fix list 1, 2.
2. **Post, bracket and arm are lit hot orange over their whole height, in full sun.** 29,268 orange pixels (R > 200, 90 < G < 190, B < 100) span y 9–1026, i.e. the entire post from the arm to the base. Concept: plain brown wood with dark iron bands; the lantern is the only warm element. Root cause: `_point_light` puts a 900 lm, 4 m, shadowless inverse-square light at the glass centre, 45–56 cm from the post (`lamp-sockets.json` StreetLamp1 socket x = 0.45 m), so the post receives ~16x the irradiance of the ground 2 m away.
   Fix type: LEVEL (JSON) + CODE (script). See fix list 3.
3. Out of scope but noted: the live mesh has a rigid bracket and no chain; the concept hangs the lantern on a chain from a scrolled bracket. That is the lamp mesh, not this VFX pass.
4. Lamp lit at noon is per `lit: always` in `lamp-fx.json` (time-of-day deferred to GFX WP6). Not counted against the pass, but item 2 makes it visible.

## dusk-close — FAIL

1. Same flat yellow glass, same 10 x 46 px white sliver instead of a flame (fix list 1, 2).
2. Same orange post from top to bottom (fix list 3).
3. **No smoke.** The 240 x 210 px region above the lid (x 820–1060, y 120–330) shows only the cap and roof tiles; nothing wisps. Root cause: the smoke card is 16 x 25 cm centred 45 cm above the glass centre, so its 32–58 cm span sits inside the opaque lid and cap; and peak opacity is `smoke_opacity` .09 x texture alpha max 100/255 = 3.5%. Fix list 4.
4. **No glow outside the cage.** The halo cards are 26 cm, the same size as the chamber, so no warm bloom extends past the iron. From 3 m the lamp is a bright rectangle with a hard edge; the owner asked for a warm glow. Fix list 5.

## night-close — FAIL

1. Flat yellow glass, no readable flame (fix list 1, 2). Flicker is present: frames `flicker-00..04` differ by up to 76/255 in the chamber, depth consistent with `flicker_amount` .12; keep.
2. Orange post full height (fix list 3). Measured post column (x 900–960, y 450–900) mean RGB (56,31,15) while the stone wall 1 m away is near black: the post is the brightest surface in frame after the glass.
3. No smoke (fix list 4), no glow past the cage (fix list 5).
4. **No firefly in any view.** Zero pixels match a green glint (G > 150, G > R + 20, B < 140) in night-close, night-street or dusk-street. Three causes: (a) the captured lamp StreetLamp_002 has none (28% chance; only 003 and 004 rolled them); (b) firefly cards are `u.Vector(.018, .018, 1)` = 1.8 cm, sub-pixel beyond ~4 m; (c) the material gate `saturate((0.07 - sunDir.z) * 8)` with the review "night" sun at pitch +10 deg (`capture_game.py` plan: `('night-close.png', 10, 0.9, 1.3, True)`) evaluates to 0, so fireflies are switched off in the night captures by the review lighting itself. Fix list 6, 7.

## dusk-street — FAIL

1. Near lamp (right edge): the arm, bracket and whole post are orange, glass is a flat yellow rectangle, no flame silhouette, no halo (fix list 1, 2, 3, 5).
2. The three distant lamps on the street (x 1090–1250, y 300–400) read as small lit rectangles with no warm halo and no visible ground pool; the halo HISM culls at 18 m (`instance_end_cull_distance` 1800 for non-firefly cards) so mid-street lamps lose their glow before they stop being visible. Fix list 5.
3. No smoke or fireflies visible anywhere (fix list 4, 6, 7).

## night-street — FAIL

1. Mean luminance of the frame is 16/255; the only readable surfaces are the near lamp's orange post (fix list 3) and six small lit glass rectangles. No warm pools on the road under any lamp except the near one. The point light's 4 m reach at 900 lm is spent on the post; move the budget to reach and halo (fix list 3, 5).
2. No glow beyond the cage, no smoke, no fireflies (fix list 4, 5, 6, 7). Note the night capture still has the sun 10 deg above the horizon; the firefly gate cannot pass under that lighting (fix list 7).

## Fix list

All items are JSON values, `lamp_fx.py` instance parameters or `build_materials.py` constants that `lamp_fx.py` applies on the next run (`py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/lamp_fx.py --save`), then recapture with `capture_game.py` and `make_pairs.py`.

1. **Glass reads amber with a visible flame** — Codex. `GameClient/ArtSource/Greenlands/lamp-fx.json`: `glass_emission_strength.value` 1.8 -> 0.6 (within min 0 / max 4). Leave `flame_colour` at [1.0, 0.38, 0.08]; at 0.6 the tone mapper keeps it amber instead of clipping to pale yellow, and the x5 flame core becomes the brightest element as in the concept.
2. **Flame silhouette large enough to read at 3 m** — Engineer. `GameClient/ArtSource/Greenlands/lamp_fx.py` `apply()`, the two `_instance(cards['Flame'], ...)` calls: scale `u.Vector(.10, .22, 1)` -> `u.Vector(.16, .32, 1)` (16 x 32 cm, fits the ~30 cm chamber). `GameClient/ArtSource/VFX/StreetLamp/build_materials.py` `build()`, flame energy: `_constant(flame, 5)` -> `_constant(flame, 3.5)` so the core is a pale-yellow teardrop, not a white bar. Acceptance: after fix 1 and 2 the flame is a distinct brighter shape at least 20 px wide x 50 px tall inside visibly darker amber glass in `night-close`.
3. **Stop over-lighting the post; spend the light on reach** — Codex for JSON, Engineer for script. `lamp-fx.json`: `light_intensity.value` 900 -> 450 (lumens), `light_radius.value` 4.0 -> 5.0 (max). `lamp_fx.py` `_point_light()`: after `light.set_editor_property('cast_shadows', False)` add `light.set_editor_property('source_radius', 15.0)` and `light.set_editor_property('soft_source_radius', 40.0)` (area-light near-field clamps the 45–56 cm post hot spot without shadows). Do not enable `cast_shadows` (938 movable shadow casters would blow the 0.5 ms budget). Acceptance: in `night-close`, mean RGB of the post column (x 900–960, y 450–900) below (30,16,8) while the road 2 m from the base still shows a warm pool in `dusk-street`.
4. **Smoke visible above the cap** — Engineer for script, Codex for JSON. `lamp_fx.py` `apply()`: `high = u.Vector(pos.x, pos.y, pos.z + 45)` -> `pos.z + 85` (clears the ~55 cm lid and cap); smoke instance scale `u.Vector(.16, .25, 1)` -> `u.Vector(.24, .55, 1)` (24 x 55 cm wisp). `lamp-fx.json`: `smoke_opacity.value` 0.09 -> 0.18 (max 0.2). Acceptance: a faint grey wisp readable in the 240 x 210 px region above the lid in `dusk-close` and `night-close`; not visible from the street views (still "slight").
5. **Warm glow past the cage, kept at street distance** — Engineer. `lamp_fx.py` `apply()`: both `_instance(cards['Halo'], ...)` scales `u.Vector(.26, .26, 1)` -> `u.Vector(.80, .80, 1)` (80 cm soft disc, 25 cm past the iron). `lamp_fx.py` `_component()`: `instance_end_cull_distance` `2500 if name == 'Fireflies' else 1800` -> `4000 if name in ('Fireflies', 'Halo') else 1800` so mid-street lamps keep their glow. Leave `_constant(halo, 1.4)`; the T_LampGlow alpha peak (115/255) keeps the disc below 0.65 emissive, invisible against daylight. Acceptance: in `night-street` each visible lamp has a soft warm spot at least 2x the glass width; no hard-edged disc in `day-close`.
6. **Fireflies large and frequent enough to exist** — Engineer for script, Codex for JSON. `lamp_fx.py` `apply()`: both firefly `_instance` scales `u.Vector(.018, .018, 1)` -> `u.Vector(.05, .05, 1)` (5 cm soft dot, 2–3 px at 10 m). `build_materials.py` `build()`: `gated = _node(firefly, u.MaterialExpressionPower, const_exponent=12.0)` -> `const_exponent=4.0` (each blink lasts seconds, not frames). `lamp-fx.json`: `firefly_chance.value` 0.28 -> 0.45 (max 0.6). Keep the crossed-dot cards and WPO drift; do not reintroduce ribbons or velocity alignment (the earlier "sperm" complaint).
7. **Night evidence must be night** — Engineer. `docs/evidence/graphics/lamp-fx-3/capture_game.py` `plan`: `('night-close.png', 10, 0.9, 1.3, True)` -> `('night-close.png', -12, 0.0, 0.25, True)` and `('night-street.png', 10, 0.9, 1.3, False)` -> `('night-street.png', -12, 0.0, 0.25, False)`. With the sun below the horizon the firefly gate `saturate((0.07 - sunDir.z) * 8)` in `build_materials.py` evaluates to 1 and the lamps, not the sun, light the street. Also capture the close views on a lamp that rolled fireflies (StreetLamp_003 or 004 per `five-lamp-offsets.json`): `lamp = ... get_actor_label() == 'StreetLamp_002'` -> `'StreetLamp_003'`, and update `glass = ... 'FX_StreetLamp_Light_StreetLamp_002_0'` -> `'..._003_0'`.
