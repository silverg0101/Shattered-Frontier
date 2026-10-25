# Forge creatures into Unreal (SK_Quadruped_v1)

- **Area:** art
- **Owner of the next step:** agent
- **Updated:** 2026-10-25

## Goal

Forge creature GLBs import into Unreal on one shared skeleton, `SK_Quadruped_v1` under `/Game/Frontier/`, with their clips playing. The skeleton asset is committed.

## State

The import check (pipeline Task 7) was written, but its re-run is pending because other editors were open. `SK_Quadruped_v1` is not committed yet.

## Evidence

The earlier run's output is in the session scratchpad `t7\run2`. The import route is in `docs/dependency-map/art-pipeline.md`.

## Blocked on

`art-starter-creatures.md`: commit the skeleton when the first creatures go into the game. The re-run also needs no other editor holding the assets.

## Next step

With no other editor open, re-run the Task 7 import check on `Greenlands_Boar` once it is signed off. Then commit `SK_Quadruped_v1` and the imported boar.

## Rules for this task

- Write only under `/Game/Frontier/`.
- Kill only your own Unreal process IDs.

## Outcome
