# GEAR-02 hood cape rebuild

- **Area:** art
- **Owner of the next step:** owner
- **Updated:** 2026-10-25

## Goal

The player hood matches its concept side by side in game: a bell-shaped cape that flares, clears the arms and stays closed while walking. Passes a fresh Opus review, then owner sign-off.

## State

Round 5 of 5 failed (the owner raised the cap from 3 to 5). The hood part is good: crown, face opening, no hair poking through, clearance in A-pose, arms-down and crouch, colour. The cape is not: the scripted reshaping of the generated mesh traded one defect for another each round (lobes, then flat yoke, then tube) and round 5 added a back-panel hole, torn corners and a stray patch on the thigh.

## Evidence

- Round 5 build: commit 91528d04; review f6cb2036, `docs/evidence/player-outfit-hood/round5/REVIEW-round5.md`.
- Earlier rounds: `docs/evidence/player-outfit-hood/round3/`, `round4/`.
- GEAR-02 decision doc records each round.

## Blocked on

Owner choice between:
1. Rebuild the cape as a clean circle-cape draped on the MetaHuman body in Blender, keeping the generated mesh for the hood only (recommended).
2. Keep the current shape and add Chaos Cloth to the lower cape.

## Next step

Once the owner chooses, an Opus builder implements that route in lane d, captures in game against the concept, and a fresh Opus reviewer judges it with `docs/visual-review.md`.

## Rules for this task

- Start from the raw v4 GLB; render raw vs each processing stage before blaming generation.
- Keep the checks: drift, 0 see-through rays, head clearance in three poses, face visible.
- No paid tools (Marvelous Designer is out).
- The review cap restarts only if the owner says so.

## Outcome

