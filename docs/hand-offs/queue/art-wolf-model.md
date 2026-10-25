# Greenlands grey wolf model

- **Area:** art
- **Owner of the next step:** agent
- **Updated:** 2026-10-25

## Goal

A reviewed grey wolf model replaces the boar placeholder used for npc/wolf (and the Wolf Alpha) in Greenlands; rig and motion follow.

## State

v1 failed review: four ears (front and side inputs disagreed on ear position), fur turned into hard ridges, smeared texture, a pale tail band. Legs and proportions were fine. Round 2 (v2) was running at hand-off time.

## Evidence

- v1: `C:\Projects\SF-lane-a\Artifacts\Creatures\Wolf\v1\` (Forge jobs 32 image, 34 3D), review `REVIEW-round1.md` there.
- v2 output goes to `...\Wolf\v2\`.

## Blocked on

none

## Next step

Check the v2 renders, especially the head close-ups at 45/135/225/315 for the ear count, then launch a fresh Opus reviewer with `docs/visual-review.md`. Max 3 rounds, then the owner.

## Rules for this task

- Quadrupeds: side hero + front redraw + mirrored side, no back view (see MV_HERO comments in `tools/asset-manager/app.py`).
- Review in clay from every side and from below.
- Fixes at the input-image level only; no Blender patching.

## Outcome

