# Building dev-map tooling evidence

Worktree: `C:\Projects\SF-wt-tooling`, branch `tooling-devmap`. Editor build: `Build.bat ShatteredFrontierEditor Win64 Development -Project=<this worktree>/GameClient/ShatteredFrontier.uproject -WaitMutex -NoHotReloadFromIDE` → `Result: Succeeded`.

`Run-InEditor.py tools/Create-BuildingDevMap.py Blacksmith` → `DEV_MAP_READY /Game/Frontier/Maps/Dev/Blacksmith/Blacksmith_Review`.
`Run-InEditor.py GameClient/ArtSource/Silverbrook/silverbrook_props.py --folder Districts/Dev --terrain Terrain` → `HOUSE_FOUNDATIONS 1 CREATED 1`; `HOUSE_DETAILS` reported `signs: 1`, `sill_planters: 0`.

| Gate | Output |
|---|---|
| `tools/check_foundations.py --terrain Terrain --expect 1` | `FOUNDATION_CHECK 1 failures 0 min_visible_cm 65.0 max_abs_gap_cm 0.0 proud_cm 4.0 4.0` |
| `tools/check_details.py --expect 0` | `SILL_PLANTERS 0 expected 0 GROUP_DETAIL_OVERLAPS 0 []` |
| `check_hidden_in_game.py --all` | `HIDDEN_IN_GAME_CHECK checked 0 visible_in_game 0 []`; `DEFAULT_MATERIAL_CHECK visible_in_town 11 default_or_graybox 0 []` |
| Town default `check_hidden_in_game.py` | `HIDDEN_IN_GAME_CHECK checked 126 visible_in_game 0 []`; `DEFAULT_MATERIAL_CHECK visible_in_town 8723 default_or_graybox 0 []` |
| Town default WP6f `check_foundations.py` | `FOUNDATION_CHECK 312 failures 0 min_visible_cm 65.0 max_abs_gap_cm 0.0 proud_cm 4.0 4.0` |
| Town default WP6f `check_details.py` | `SILL_PLANTERS 140 expected 140 GROUP_DETAIL_OVERLAPS 0 []` |

`import_silverbrook.py --package-only Blacksmith` printed 18 `RESAVED` package mesh / manifest-listed material lines and `SILVERBROOK_IMPORT` for Blacksmith only. Generated import changes were removed afterward. The town map was opened read-only and never saved.

`capture_building.py Blacksmith docs/evidence/tooling-devmap` produced five 1920×1080 PNGs in `-game`; all five logged `BUILDING_SIGHTLINE_CLEAR`. These files are tooling evidence only; no visual judgement was made.

Deviation: `Run-InEditor.py` now chooses an available private TCP command port because other checkout clients can occupy its fixed port. The target editor still has to match this worktree's project root. Dev-map dressing saves the dev level. The Blacksmith detail pass always places a sign for the specified sign-close capture.
