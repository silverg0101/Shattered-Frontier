# Silverbrook WP6c: WP6b review fix list (2026-10-21)

Implements the fix list in `../2026-10-21-wp6-town-b/REVIEW.md`. Run order in the open Silverbrook editor (`tools/Run-InEditor.py`): `GameClient/ArtSource/Greenlands/build_silverbrook.py --discard-unsaved` (rebuild: fix 3's row rule and fix 2's earth width only take effect there), `GameClient/ArtSource/Greenlands/paint_street_edges.py`, `apply_dressing.py`, then `check_foundations.py`, `GameClient/ArtSource/Greenlands/check_hidden_in_game.py`, `check_ground.py`. Captures: `-game -RenderOffscreen -ResX=1920 -ResY=1080 "-ExecCmds=py .../capture_game.py"` (no `ShowFlag.TextRender` override), then `make_pairs.py` (all five views against the HUD v5 and GFX-01 board concepts).

Results: `foundation-check.json` (312 houses, gap 0 cm, minimum visible plinth 65 cm, proud 4 cm, 0 failures); hidden-in-game check 117 debug actors, 0 visible; `ground-gap.json` 4,625 placed bases, 0 floating over 5 cm (smoke cards, tub flowers, stoop tubs, wall signs and sill planters are not ground-placed and are skipped). Street groups 701 against a gate of 636 (6 per 50 m of street inside the wall). House details: 231 gardens, 158 door tubs, 158 stoop tubs, 158 firewood stacks, 85 smoking chimneys, 117 shop signs, 117 ladders, 117 ivy clumps, 22 sill planters, 0 washing lines.

Notes against the fix list:
- Fix 1: the 700-group cap is reached on the four primary streets, so secondary streets get no groups.
- Fix 4: the plane's height axis after the old pitch-90 spawn is local X, so the 1.4 m x 4.2 m card stands with roll -90 (scale Y is height). `T_LampSmoke` has alpha <= 0.35 over a third of its width, so `M_LampSmoke` gained a `SmokeTex` parameter and the chimney MI uses `T_ChimneySmoke` (`VFX/StreetLamp/make_chimney_smoke.py`) plus an `EdgeFade` parameter; lamp defaults are unchanged. The chimney view is now 20 m from the plume and aimed at the card centre. At 20 m and 55 degrees a 1.4 x 4.2 m card spans at most about 129 x 387 px.
- Fix 5: the street face is mesh-local +Y (the door side), so the door tub sits 45 cm in front of the traced street wall rather than at `b.min.y - 45`, and the firewood goes to the rear corner at `b.min.y`. Door paths are painted on the terrain's 5 m vertex grid, which is coarser than the 1.2 m strip.
- Fix 6: the main-street eye at (24.5, 336) stands behind a street lamp.
- Fix 7: the pier is in `Townhouse6`. After fix 3 the frontage lot holds a different townhouse package.
- Fix 8: same-row neighbours are at least 5.1 m apart, so the "< 4 m apart" rule places no washing lines. Only `Townhouse6` has bay windows, so its two sills are the only planter positions.

No quality verdict here; an independent reviewer judges the `*-compare.png` pairs.
