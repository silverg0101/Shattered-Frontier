# Greenlands boar: first animated Forge creature

- **Area:** art
- **Parent:** `art-forge-creatures.md`, which holds how we work, the pipeline facts, deploy steps and what to track where.
- **Owner of the next step:** agent
- **Updated:** 2026-10-25
- **Worktree:** lane `a` (`pwsh tools/Enter-Lane.ps1 -Name a`, which gives `C:\Projects\SF-lane-a`). If lane `a` is busy, use any free lane.

## Goal

Get one animated boar from the prompt "a wild boar" with five clips: idle, walk, run, attack and death. It needs to pass the pipeline gates, then a fresh Opus visual review, then owner sign-off.

## State

- Visual round 1 failed every clip on `Greenlands_Boar_14`:
  - a doubled back shell with a ghost head;
  - a backflip death;
  - open loops;
  - a sprawling run;
  - an attack where the head folds back.
- Round 2 code is on main (from Fable, commits "creature clips, round 2" and "a build whose SkinTokens rig cannot be labelled"). It adds:
  - a model review against the concept, rebuilding on a fail;
  - loop cutting with a `loop_gap_cm` gate;
  - procedural idle, death, attack and gallop;
  - pitch and neck gates.
  
  It also fixes all the code-review findings (Important 1-6, Minor 7-11).
- Tested only in isolation on an old rig, where all five procedural clips passed. The model review correctly failed the doubled rig.
- **Not yet done:**
  - a full end-to-end run with rebuilds;
  - the UniMate loop cut on a real take;
  - a scoped code re-review of the round 2 commits;
  - visual round 2.

## Open fix list (visual round 1, items not covered by round 2)

- The head bone should end at the snout tip (`rig_standard`). Do it once a clean single-body model exists.
- Neck skin weights smear the head during the attack.
- The tail should be tufted and bushier at the end, like the concept. This is art: fix it in the concept or prompt, not the mesh.

## Evidence

Round 2 code: `creature.py` (`model_review`, `MODEL_BUILDS`), `motion/gait_synth.py`, `motion/zoo_retarget.py` (gates) and `motion/zoo_curate.py --loop`.

## Blocked on

none

## Next step

1. Run a scoped Opus code re-review of the two round 2 commits. Fix what it finds.
2. Do the full scratch run:
   `py tools/asset-manager/scratch_run.py '{"name": "Greenlands_Boar", "prompt": "a wild boar", "animal": "Buffalo", "clips": ["idle", "walk", "run", "attack", "death"], "size_cm": 150}'`
   Use `creature_unimate_seeds` 6.

   Expect the model review to rebuild until one body passes. If every rebuild fails, change the concept, not the mesh: a cleaner side view, or `concept: "heroes"`.
3. Send the new folder's `animations/*.png` and `source/*_rig_labels.png` to a fresh Opus for visual round 2 of 3.
4. If both reviews pass, deploy the live Forge using the steps in the parent file.

## Rules for this task

- This asset is stuck, so a Fable agent does the fix rounds (owner direction). Only fresh Opus agents review it.
- After round 3, it goes to the owner.

## Outcome
