# Town NPCs for the starting area

- **Area:** art
- **Owner of the next step:** agent
- **Updated:** 2026-10-25
- **Worktree:** lane `b`. Run `pwsh tools/Enter-Lane.ps1 -Name b` to get `C:\Projects\SF-lane-b`. If lane `b` is busy, use any free lane.

## Goal

A first set of human Town NPCs for the starting town, built through the NPC route in `docs/dependency-map/characters.md`. Each passes a fresh Opus visual review, then the owner signs off.

## State

Not started.

| NPC | State |
|---|---|
| (pick 3 roles) | not started |

## Evidence

none

## Blocked on

none

## Next step

1. Read the NPC route in `docs/dependency-map/characters.md`.
2. Pick 3 roles from `docs/greenlands-silverbrook-zone-plan.md` and fill in the table above.
3. Brief one subagent to build the first NPC.

## How this task is worked

- The controller writes a short brief file for each NPC and gives it to one subagent. The subagent returns only its status and commits. The controller checks every image.
- A fresh Opus reviewer judges each NPC with `docs/visual-review.md`. Implement only its fix list. The limit is 3 rounds, then the owner decides.
- If an NPC is still stuck after 2 rounds, it goes to a Fable agent.
- Track each NPC's state in the table above. Don't commit briefs or raw logs.
- Git: rebase onto origin/main, stage files by name, and push right away.

## Rules for this task

- Generation uses local models only.
- Write only under `/Game/Frontier/`.

## Outcome
