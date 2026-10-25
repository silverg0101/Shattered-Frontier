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
- The scoped Opus code review of the round 2 commits (2026-10-25) failed it on 7 findings. Fable fixed 1-4 and 6 in lane `a` (branch `lane/a`, not pushed until the fix diff has its own Opus re-review):
  1. `zoo_curate.py --loop` scored every take as a gait, so no idle could ever loop-cut (its paws never lift; the procedural idle moves 0.003 deg/frame, under the stall rule). A stand now only has to close.
  2. A failed build (GPU, ComfyUI, ssh) was logged as a rig failure and rebuilt up to 4 times. It now fails the job.
  3. A review answer that was not JSON killed the job after the builds. It is retried once, then counts as a FAIL.
  4. A released pin kept its piled-up target, and convergence no longer stopped the IK passes. Released frames get their off-pin targets back, a pass solves with them, and the passes stop once the fixes converge.
  6. A leg whose only `_01` bone was dropped crashed the retarget. It falls back to the whole chain.
  
  Left as is, with the reasons: 5 (`max_neck_deg` is the documented head-to-`spine_03` angle, the procedural clips passed it on the old rig, and a false fail on a UniMate idle only drops that seed; re-check when the head bone moves to the snout tip) and 7 (the side-alone 4th build is the strategy chosen in commit c6cea807; the model review catches a fused build).
- **Not yet done:**
  - a fresh Opus code re-review of the fix diff, then the push;
  - a full end-to-end run with rebuilds;
  - the UniMate loop cut on a real take;
  - visual round 2.

## Open fix list (visual round 1, items not covered by round 2)

- The head bone should end at the snout tip (`rig_standard`). Do it once a clean single-body model exists.
- Neck skin weights smear the head during the attack.
- The tail should be tufted and bushier at the end, like the concept. This is art: fix it in the concept or prompt, not the mesh.

## Evidence

Round 2 code: `creature.py` (`model_review`, `MODEL_BUILDS`), `motion/gait_synth.py`, `motion/zoo_retarget.py` (gates) and `motion/zoo_curate.py --loop`.

Code-review fixes: `app.py check()` prints `check ok` with new asserts for the idle loop cut (`LOOP 0 90 91 frames`), a failed build failing the job once, and the model review's retry, fail-closed and cancel paths. The IK release pass (`zoo_retarget.py`) runs only in Blender and is unverified until the scratch run; its `RT ik pass` lines show the pass count and "(release pass)".

## Blocked on

none

## Next step

1. Fresh Opus code re-review of the fix commits on `lane/a` (the review's findings 1-4 and 6; its 5 and 7 are answered above). Fix what it finds, then push.
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
