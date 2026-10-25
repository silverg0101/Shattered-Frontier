# Greenlands starter creatures and a hybrid

- **Area:** art
- **Parent:** `art-forge-creatures.md`, which holds how we work, the pipeline facts and what to track where.
- **Owner of the next step:** agent
- **Updated:** 2026-10-25
- **Worktree:** lane `a` (`pwsh tools/Enter-Lane.ps1 -Name a`, which gives `C:\Projects\SF-lane-a`). This is the same pipeline code as the boar. If lane `a` is busy, use any free lane.

## Goal

Make the starting-area creatures players kill: Deer, Stag, Rabbit, Wolf and Bear, plus one harder hybrid, "Bramble Stag" (a deer with thorny antlers and a lizard tail). Each needs five clips (idle, walk, run, attack, death) that pass the gates. Each then gets its own fresh Opus visual review and owner sign-off.

## State

Not started.

| Creature | Animal | State |
|---|---|---|
| Deer | DEER | not started |
| Stag | DEER | not started |
| Rabbit | Hamster | not started |
| Wolf | Coyote | not started |
| Bear | BEAR | not started |
| Bramble Stag | DEER | not started |

## Evidence

none

## Blocked on

`art-greenlands-boar.md`: the pipeline fixes must pass review and be deployed first.

## Next step

Submit the Deer as a creature job on the live Forge (`POST /api/jobs`, kind `creature`; see API.md). Send its sheets to a fresh Opus visual review. Update the creature's row above after every run and every review.

## Rules for this task

- Run one creature job at a time, because RA-SIEMS has little disk space.
- A creature allows 3 visual rounds at most. If it still fails after 2 fix rounds, hand it to a Fable agent with a full brief.

## Outcome
