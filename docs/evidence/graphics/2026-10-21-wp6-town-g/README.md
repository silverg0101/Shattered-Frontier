# Silverbrook WP6g: WP6f review fix list (2026-10-21)

Implements fix 1 (1a, 1b) in `../2026-10-21-wp6-town-f/REVIEW.md` (commit 7b12738). Nothing else changed.

- Fix 1a: `GameClient/ArtSource/VFX/StreetLamp/build_materials.py` `build()`: the `sin(pi V)` end fade is replaced by `saturate(8 V) * saturate(5 (1 - V))` (foot x head), exactly as specified. `fade = lerp(1, ends, EdgeFade)` and `edge_fade` 1.0 unchanged.
- Fix 1b: `_texture()` sets `address_x = TA_CLAMP` on `T_ChimneySmoke` (V stays Wrap). Confirmed after the reimport: `address_x TA_CLAMP`, `address_y TA_WRAP`.

Run order (own editor on `Greenlands_Silverbrook`, `tools/Run-InEditor.py`): `apply_dressing.py` (`build_materials.build()`, asserts the clamp, `dress_all`, saves the level), then `check_foundations.py`, `GameClient/ArtSource/Greenlands/check_hidden_in_game.py`, `check_ground.py`, `check_details.py`. Editor closed, then `-game -RenderOffscreen -windowed -ResX=1920 -ResY=1080 "-ExecCmds=py .../capture_game.py"` (the WP6f script, filtered to `main-street` and `chimney-smoke`), `make_pairs.py` (same concepts, those two views), `measure_plume.py`.

`market`, `house-frontage`, `foundation-close` (+ their compares) are copied unchanged from WP6f. `dress_all` reran with unchanged inputs, so the dressing is the same.

Gates: `FOUNDATION_CHECK 312 failures 0 min_visible_cm 65.0 max_abs_gap_cm 0.0 proud_cm 4.0 4.0`; `HIDDEN_IN_GAME_CHECK checked 126 visible_in_game 0`; `DEFAULT_MATERIAL_CHECK visible_in_town 8723 default_or_graybox 0`; `DRESSING_GROUND checked 5850 floating_over_5_cm [] max 0.01 cm`; `SILL_PLANTERS 140 expected 140 GROUP_DETAIL_OVERLAPS 0`; `WP6_SIGHTLINE_CLEAR` for both views.

## Fix-1 acceptance numbers (measured by `measure_plume.py`, `plume-measure.json`; WP6f value in brackets, same script)

Definitions as in the review: sky rows use the 7-row-averaged R-B excess over a background interpolated between x 820 and x 1070 (threshold 4). At y >= 400 the right end of that window (x 1000-1070) is the next roof and its stack, so those rows interpolate to x 980 instead; field rows (y >= 440) also give the L excess > 3 over the same-row field fit at x 560-700, counted over x 700-990 (beyond x ~990 is roof).

| Check (acceptance) | WP6g | [WP6f] |
|---|---|---|
| y 520 plume within x 820-1000, span <= 140 | R-B: none > 4 (peak 3.3); field L: x 744-962, 47 columns | [R-B 1 col; field x 740-960, 42] |
| y 150 span >= 1.5 x y 520 span | 864-1003, 140 px | [876-991, 116] |
| peak R-B excess >= 12 at y 400-520 | 17.1 | [20.8] |
| peak R-B excess >= 8 at y 100 | 9.5 | [9.2] |
| <= 160 columns of field brightening, any row y 460-520 | max 249 (y 490: x 719-980, 201) | [251] |
| plume L / sky L within 10 % at rows 30, 80, 260 | 1.022, 1.000, 1.038 | [1.021, 1.000, 1.051] |
| top rows y 10 / 30 / 60 span | 94 / 102 / 148 | [116 / 125 / 131] |
| stack top in frame | pots at y ~580 | |
| main-street: >= 15 columns R-B excess >= 5 above the right roofs (x 790-830 or 1085-1125, y 300-400) | 10 (y 381) / 0 | [11 / 1] |

Whole-frame g-vs-f mean absolute difference on `chimney-smoke.png`: 1.18; the change is concentrated in x 940-1050 at y 300-580.

No quality verdict here; an independent reviewer judges the `*-compare.png` pairs.
