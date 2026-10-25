# Forge creatures into Unreal (SK_Quadruped_v1)

- **Area:** art
- **Parent:** `art-forge-creatures.md`, which explains how we work and what to track where.
- **Owner of the next step:** agent
- **Updated:** 2026-10-25
- **Worktree:** lane `c`. Run `pwsh tools/Enter-Lane.ps1 -Name c` to get `C:\Projects\SF-lane-c`. If lane `c` is busy, use any free lane. Only one Unreal-heavy job may run at a time.

## Goal

Forge creature GLBs import into Unreal on one shared skeleton, `SK_Quadruped_v1` under `/Game/Frontier/`, and their clips play. The skeleton asset is committed.

## State

The import check from pipeline Task 7 is written and described in `docs/dependency-map/art-pipeline.md`. Its re-run is still pending, because other editors were open. `SK_Quadruped_v1` is not committed yet.

## Evidence

none yet

## Blocked on

`art-greenlands-boar.md`: there is no signed-off creature to import yet. The re-run also needs no other editor holding the assets.

## Next step

With no other editor open:
1. Re-run the import check on the signed-off boar. With the editor open, run it inside the editor using `python tools/Run-InEditor.py <script.py>`.
2. Commit `SK_Quadruped_v1` and the imported boar.

## Rules for this task

- Write only under `/Game/Frontier/`.
- Lasting changes must be committed scripts or assets, not editor-only clicks.
- Kill only Unreal process IDs you started; never stop Unreal by name.

## Outcome
