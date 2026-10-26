# Match gathering skill controls to the actual progression rules

- **Area:** ui
- **Owner of the next step:** agent (root triage)
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-b

## Goal

The skills page exposes Raise/Hold/Lower only when the designed gathering progression rules require them.

## State

Owner questions why gathering skills need Hold/Lower if every gathering skill may reach maximum. No rule conclusion has been established yet; do not invent a total skill cap.

## Evidence

Owner report in the live-client session. Parent: `ui-live-client-regressions.md`. Relevant progression/design sources are indexed in the gameplay dependency area.

## Blocked on

Rule check and a worker slot, not a request for broad game-design changes.

## Next step

Read the gathering progression rule and actual service enforcement. If skills can all reach maximum, remove the irrelevant controls for gathering while preserving any separately designed capped skill categories; otherwise explain the cap with precise references before changing behavior.

## Rules for this task

No invented cap, skill decay or balance values. Keep layout in Widget Blueprints, logic in C++, and test the exact gathering path. Record any genuine design contradiction for the owner.

## Outcome

Rule check complete; no UI text or behavior changed. The design has a 4,000-point gathering-track cap and allows a lowered skill to donate one point only when another gathering skill gains at the shared cap; lowering never decays automatically. The UI's Raising/Held/Lowering states represent those gain preferences, so they are meaningful in the local progression adapter.

Production enforcement is incomplete: `BackendServices/Progression/Progression.cs` currently caps gathering using only Logging + Mining, and this gain path has no lock/donor handling. Do not add player-facing help that implies these controls govern production progression until that service path is reconciled. The local adapter's exact semantics are in `SFGatheringProgression.cpp`; design caps and lock rule are in `Legends_Shattered_Frontier_Character_Skills_Mastery_v1.0.md` §§1 and 6. The four visible rows total at most 3,600 unspecialized points, but Farming and Hunting are also gathering skills and count against the designed track cap.
