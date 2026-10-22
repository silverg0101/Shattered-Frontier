# Silverbrook WP6f: WP6e review fix list (2026-10-21)

Implements the fix list in `../2026-10-21-wp6-town-e/REVIEW.md` (commit 58cea8d). Deferred items (water surface, tub blooms, GFX-02/03) not touched. Run order in the open Silverbrook editor (`tools/Run-InEditor.py`): `GameClient/ArtSource/VFX/StreetLamp/make_chimney_smoke.py` (plain Python, writes `T_ChimneySmoke.png`), then `apply_dressing.py` (runs `NatureFamilies/import_ground.py`, `build_materials.build()` which reimports `T_ChimneySmoke`, `dress_all`, saves the level, then `Greenlands/paint_street_edges.py`), then `check_foundations.py`, `GameClient/ArtSource/Greenlands/check_hidden_in_game.py`, `check_ground.py`, `check_details.py`. Captures: `-game -RenderOffscreen -ResX=1920 -ResY=1080 "-ExecCmds=py .../capture_game.py"` (all five views), then `make_pairs.py` (HUD v5 and GFX-01 board concepts, same as WP6e).

Gates: `foundation-check.json` 312 houses, 0 failures, gap 0 cm, minimum visible plinth 65 cm, proud 4 cm. Hidden in game: 126 debug actors, 0 visible; default-material gate: 8,723 visible static meshes inside the wall, 0 default/graybox. `ground-gap.json` 5,850 bases, 0 floating over 5 cm (max 0.01 cm). `check_details.py`: 140 sill planters of 140, 0 group/detail overlaps. Sight-line assert clear in all five views. `paint_street_edges.py`: 11,899 vertices, ramp 5.01 m, 312 door paths, 312 lot lanes. Trail spot-check (terrain vertex colours): 1,465 trail vertices with G = 0 and B > 0.9, 684 of them beyond 400 m from the origin (e.g. (-8, -1210) m), so the cobble weight there is 0 and the trail renders as earth.

Notes against the fix list:
- Fix 1a: taper ends swapped as specified; comment corrected. Rise direction was measured, not assumed: 14 `-game` frames 0.3 s apart at the chimney view, plume row profile (R-B, temporal mean removed) cross-correlated frame to frame. With `pan_v` +0.12 the puffs moved +26 px per step (down, toward the pot); `pan_v` is now -0.12 and the same measurement gives -27 px per step (up). The sign is noted in the `pan_v` unit string in `silverbrook-props.json`.
- Fix 1b: `thin` lerp (1 at the pot, 0.45 at the top) multiplied into the opacity product, as specified.
- Fix 1c: alpha and sway changed as specified; edge-zero line and assert kept (alpha max 1.0, mean 0.30). Rerun and reimported.
- Fix 1d: `chimney_smoke.opacity` 0.7 -> 0.5.
- Fix 2a: `M_GN_Ground` loop order changed as specified (earth first, cobble on top); comment updated. Rebuilt in the editor.
- Fix 2b: `build_silverbrook.py` both lines changed as specified (no full town rebuild run; the fields are the ones `road_field`/plaza loops write into vertex colour B at line 790, so a rebuild paints the same). `paint_street_edges.py` line 97 as specified; the docstring's idempotency note now says G and B only rise. Open map repainted.
- `house-frontage` and `foundation-close` were recaptured too: `dress_all` reran (it sets `MI_SB_ChimneySmoke` from the JSON) with unchanged, deterministic inputs.

No quality verdict here; an independent reviewer judges the `*-compare.png` pairs.
