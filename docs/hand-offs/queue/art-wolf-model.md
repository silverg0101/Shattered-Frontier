# Greenlands grey wolf model

- **Area:** art
- **Owner of the next step:** agent
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-a (`pwsh tools/Enter-Lane.ps1 -Name a`); the lane was removed 2026-10-25, so recreate it on pickup. Files are kept in `C:\Projects\Shattered-Frontier\Artifacts\Creatures\Wolf\` (git-ignored).

## Goal

A reviewed grey wolf model replaces the boar placeholder used for npc/wolf (and the Wolf Alpha) in Greenlands; rig and motion follow.

## State

v1 failed review: four ears (front and side inputs disagreed on ear position), fur turned into hard ridges, smeared texture, a pale tail band. Legs and proportions were fine.

v2 (round 2 build) is done but not yet independently reviewed. The builder and I both checked it: 2 ears in all four head close-ups, so the four-ear defect is fixed. Still wrong:
- hard stepped ridges on the neck, nape and shoulder ruff, with a curved flap behind the ears;
- lumpy blobs at the ear bases;
- ragged muzzle and lip edge;
- the texturing pass paints a pale band at the tail base that the inputs don't have.

The Forge has no smoothing or detail setting beyond `quad_resolution` and `remesh_band`.

Overlap: `art-starter-creatures.md` (another session's parent task) lists Wolf as not started, with Coyote as its motion source. Fold this task into that one when it reaches the wolf.

## Evidence

- v1 and v2 live in `C:\Projects\Shattered-Frontier\Artifacts\Creatures\Wolf\`.
- v1: Forge jobs 32 (image) and 34 (3D); review `v1\REVIEW-round1.md`.
- v2: Forge job 58 (image redraw: far ear hidden in profile, ears set back) and job 59 (3D from front, left and mirrored right; preset normal, rig off). Job 57 was discarded for a second ear tip (`v2\inputs\`).
- v2 files: `inputs_b\` (views used), `wolf.glb`, `renders\` (2048 px clay in six views plus a textured turntable), `review\` (head close-ups at 45/135/225/315, clay and textured), `render.py`, `head.py`.

## Blocked on

none

## Next step

Launch a fresh Opus reviewer on v2 with `docs/visual-review.md` (round 2 of 3) and implement only its fix list. Likely input-level fixes: a shorter, smoother coat at the neck and ruff in the input views, and tail colour that runs continuously from the body. After round 3, hand it to the owner.

## Rules for this task

- Quadrupeds: side hero + front redraw + mirrored side, no back view (see MV_HERO comments in `tools/asset-manager/app.py`).
- Review in clay from every side and from below.
- Fixes at the input-image level only; no Blender patching.

## Outcome

