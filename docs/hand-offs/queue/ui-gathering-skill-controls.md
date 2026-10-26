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

Open.
