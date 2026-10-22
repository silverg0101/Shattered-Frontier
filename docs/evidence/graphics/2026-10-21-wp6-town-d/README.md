# Silverbrook WP6d: WP6c review fix list (2026-10-21)

Implements the fix list in `../2026-10-21-wp6-town-c/REVIEW.md`. Run order in the open Silverbrook editor (`tools/Run-InEditor.py`): `GameClient/ArtSource/Silverbrook/import_silverbrook.py PropFlowerTub`, `apply_dressing.py` (rebuilds `M_LampSmoke` with `ViewFade` and reimports `T_ChimneySmoke`), `GameClient/ArtSource/Greenlands/paint_street_edges.py`, then `check_foundations.py`, `GameClient/ArtSource/Greenlands/check_hidden_in_game.py`, `check_ground.py`. Captures: `-game -RenderOffscreen -ResX=1920 -ResY=1080 "-ExecCmds=py .../capture_game.py"`, then `make_pairs.py` (all five views against the HUD v5 and GFX-01 board concepts).

Results: `foundation-check.json` 312 houses, 0 failures, gap 0 cm, minimum visible plinth 65 cm, proud 4 cm. Hidden in game: 126 debug actors checked, 0 visible; default-material gate: 8,648 visible static meshes inside the wall, 0 with an empty, engine-default or graybox slot. `ground-gap.json` 5,853 placed bases, 0 floating over 5 cm. Street groups 1,501 (gate 636), every street with stations inside the wall has groups. House details: 162 washing lines, 62 sill planters, 158 door tubs and 158 stoop tubs (`SM_SilverbrookPropFlowerTub`), 85 smoking chimneys. Lanes: 312 door paths (2.4 m) and 312 lot-edge lanes painted.

Notes against the fix list:
- Fix 1: as specified (ViewFade, 2.2 x 6 m card 3.2 m above the pot, opacity 0.7, tint 0.58/0.58/0.62, texture column / .30 and alpha x 1.0).
- Fix 2: the white box is `Arrival_Waystone` (Places, 1.5 x 1.5 x 3 m cube, `MI_Graybox_Civic`) at (7, -39). The suggested rule (every StaticMeshActor in Places/Plots) would also hide the civic buildings and market stalls, which live in Places, so the rule hides Places/Plots actors whose slot 0 is any `MI_Graybox_*` material; `build_silverbrook.py` hides every `box()` in Places/Plots. The waystone keeps its collision (hidden only, as specified).
- Fix 3: with 1,500 groups the cap was still spent on the first streets (the loop places up to four groups per station), so each street gets a share of `max_groups` by its station count inside the wall. Primary "Road"s and Wall Street have no stations inside the 30 m wall margin and get none; the assert covers every street that has.
- Fix 4: as specified; the lamp assertion passed for all five views.
- Fix 5: the axis-aligned `_free` box against house world bounds rejected 114 of 123 lines (turned houses' bounds cover the gaps), so the line's posts and mid-point are tested against every house's own wall-base box (+50 cm) instead.
- Fix 6: as specified (only `Townhouse6` has bay sills).
- Fix 7: `PropFlowerTub/` built in Blender like `PropRainBarrel`/`PropSillPlanter` (5.6k triangles, 0.56 x 0.55 x 0.75 m). Door tubs at scale 1; stoop tubs at 0.75 so the 0.42 m tub fits the 0.4 m tread.
- Fix 8: moss removed from the verge (old moss foliage instances are cleared too). For a house without a garden the lot-edge lane goes on its +X side; the lane is on the far side of the wall-base box (x0 - 1.2 m or x1 + 1.2 m) rather than `-gside*(x1+120)`. The terrain grid is 5 m, so the 2.4 m strips land on the nearest vertices.

No quality verdict here; an independent reviewer judges the `*-compare.png` pairs.
