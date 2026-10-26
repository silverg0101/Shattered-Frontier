# Swim clips from Mixamo on the MetaHuman

**Area:** characters · **Worktree:** any free lane (not lane a while the lantern/trees task runs) · **Owner:** downloaded 2026-10-26

## Goal
Replace the procedural `AS_SF_Swim` (fb58ecc5, `tools/Build-SwimAnim.py`) with real swim motion: Mixamo **Swimming** (front crawl) while moving, **Treading Water** while still, blended by speed.

## Inputs
- `Artifacts/Mixamo/Swimming.fbx` (with skin, Mixamo default character) and `Artifacts/Mixamo/Treading Water.fbx` (without skin, same skeleton). 30 fps, no keyframe reduction. Git-ignored; import from there.

## Steps
1. Scripted import under `/Game/Frontier/Characters/Animations/Swim/` (Mixamo skeleton + both clips), committed as a Python import script.
2. IK Rig for the Mixamo skeleton, IK Retargeter to the project's MetaHuman body rig; bake both clips to MetaHuman sequences. Strip root motion (clips should be in place).
3. Hook into `ASFE1Character::UpdateSwimPose`: crawl when swim speed > threshold, tread when below; keep the existing swim flag/rules.
4. Fix the review-r3 swim items (`docs/evidence/graphics/2026-10-26-silverbrook-water/REVIEW-r3.md`): rides high, pale submerged shirt slab.

## Checks
- `tools/Test-SwimPose.ps1` passes (update it to accept the new sequences).
- Before/after captures (swim front/side, treading) → fresh Opus visual review, max 3 rounds, then owner.

## State
Queued; starts when a Claude worker slot frees.
