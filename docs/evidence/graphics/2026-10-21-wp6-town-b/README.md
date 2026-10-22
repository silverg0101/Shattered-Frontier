# Silverbrook WP6b: foundations, debug overlays, street dressing (2026-10-21)

Run order in the open Silverbrook editor (`tools/Run-InEditor.py`): `GameClient/ArtSource/Greenlands/paint_street_edges.py`, `apply_dressing.py`, then the checks `check_foundations.py`, `GameClient/ArtSource/Greenlands/check_hidden_in_game.py`, `check_ground.py`. Captures: `-game -RenderOffscreen -ResX=1920 -ResY=1080 "-ExecCmds=py .../capture_game.py"`, then `make_pairs.py`.

Results: `foundation-check.json` (312 houses: gap 0 cm, minimum visible plinth 53.4 cm, burial >= 100 cm, proud 4 cm), hidden-in-game check (117 debug actors, 0 visible), `ground-gap.json` (1,298 placed bases, 0 floating). Compare pairs `*-gfx01-compare.png` and `*-hud-compare.png` (concept left, live right); no quality verdict here, an independent reviewer judges them.
