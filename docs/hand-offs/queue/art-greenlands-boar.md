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
- The scoped Opus code review of the round 2 commits (2026-10-25) failed it on 7 findings. Fable fixed 1-4 and 6 in `bd08e18a`; fresh Opus re-review passed and independently observed `check ok`:
  1. `zoo_curate.py --loop` scored every take as a gait, so no idle could ever loop-cut (its paws never lift; the procedural idle moves 0.003 deg/frame, under the stall rule). A stand now only has to close.
  2. A failed build (GPU, ComfyUI, ssh) was logged as a rig failure and rebuilt up to 4 times. It now fails the job.
  3. A review answer that was not JSON killed the job after the builds. It is retried once, then counts as a FAIL.
  4. A released pin kept its piled-up target, and convergence no longer stopped the IK passes. Released frames get their off-pin targets back, a pass solves with them, and the passes stop once the fixes converge.
  6. A leg whose only `_01` bone was dropped crashed the retarget. It falls back to the whole chain.

  Left as is, with the reasons: 5 (`max_neck_deg` is the documented head-to-`spine_03` angle, the procedural clips passed it on the old rig, and a false fail on a UniMate idle only drops that seed; re-check when the head bone moves to the snout tip) and 7 (the side-alone 4th build is the strategy chosen in commit c6cea807; the model review catches a fused build).
- **Not yet done:**
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

Scratch job `950002` (2026-10-25, 700 s) curated Buffalo and rendered the stand-in, but all three local Qwen side concepts failed review: staggered far legs, plus ground shadows on tries 2/3. The in-memory local-only guard blocked the external heroes fallback. No model, rig or animations were produced; visual round 2 remains unused. Evidence: `Artifacts/Orchestration/20261025-resume/{boar-runner-report.md,boar-950002-full-joblog.txt}`; reusable Buffalo library in `scratch-data/MotionLibrary/Buffalo/` there.

Fable's concept diagnosis (2026-10-25, `tools/asset-manager/scratch_concept.py`, scratch jobs 950003-950005, GPU 1 only, Buffalo library reused): the stand-in is not the cause. Its side view (`standin_left.png`) shows exactly two leg silhouettes (left legs at +X, right at -X on the same Y, so each far leg is hidden) and its depth map has no floor. Qwen adds the staggered far legs and the shadow itself, under the loose depth control (strength 0.55 to 70% of the steps). Three prompt-only attempts, all with the review unchanged:
  1. `before` (current prompt, one try): FAIL, staggered legs. Proof of the symptom.
  2. side prompt describing what is seen ("only two legs are visible ... far legs completely hidden"), "a clean cut-out, flat shadowless lighting": side PASS on try 1, but as a black vector silhouette; the front failed twice (flat icon, a doubled head) and no front can match a silhouette.
  3. the same leg wording with "photographic, lit evenly from every side, isolated on white": FAIL 3/3, staggered legs again, a faint shadow on try 3. Try 1 is the closest photoreal drawing (square stance, far hooves just visible).
  So prompt wording alone does not hold a photoreal Qwen drawing to two overlapping pairs; it only did when the drawing followed the stand-in's silhouette. `creature.py` is unchanged (both wordings reverted). PNGs, prompts and verdicts: `Artifacts/Orchestration/20261025-resume/boar-concept/{before,after,after2}/`; report `boar-concept-fable-report.md` there.

Probe code review (fresh Opus, `boar-probe-code-review.md` there) failed `scratch_concept.py` at 4c1de6f6 on two Medium findings; Fable fixed the whole list: the draw is pinned to GPU 1 (`creature.depth_host` is the identity, so a host that cannot draw fails the depth path closed instead of walking to GPU 2); `--one-try` stops after the first side verdict, pass or fail; the stand-in files are written to the out folder as soon as they are rendered, so the failure path keeps them; the scratch `DATA_DIR`/`ASSET_ROOT`/`SSH_KEY` are asserted before `app` is imported; `DEPTH=` is held in memory for the run, checked against the Settings bounds, and never written to the scratch database; the unused Anthropic key fetch is gone. Offline stub check `boar-probe-stub-check.py` there (one-try pass and fail, three fails under `DEPTH=0.9,1.0`, out-of-bounds `DEPTH`, GPU 1 only while GPU 1 reports no `qwen_depth`); it fails on the 4c1de6f6 script and passes on the fix. Report `boar-probe-fix-report.md`.

## Ruling (controller, 2026-10-25)

The three prompt-only concept attempts are complete and capped. A separate bounded test of the existing typed depth controls (`creature_depth_strength`, `creature_depth_end`, Settings, bounds 0.2-1.0) is authorised on the scratch root within those bounds; that cap was our own diagnostic limit and needs no owner approval. The evidence points at this lever: the one passing side view was the one that followed the stand-in's silhouette; its cost is a blob-shaped body from the capsule stand-in, which the model review and visual round 2 then judge. The shipped defaults stay 0.55 / 0.7 / front 0.75; changing them is an owner decision made on actual results, never on one sample PASS. Still GPU 1 only, local models, the cached Buffalo library, no live settings and no auth refresh. No mesh, full creature run, deploy or import until the concept passes side and front.

## Next step

1. Fresh short Opus re-review of the probe fixes (M1, M2, L1-L3, nit) before the probe runs again.
2. At most two concept-only probes, same current photoreal prompt, side and front PNGs and verdicts preserved:
   `DEPTH=0.9,1.0 py tools/asset-manager/scratch_concept.py '{"name": "Greenlands_Boar", "prompt": "a wild boar", "animal": "Buffalo", "size_cm": 150}' <outdir>`
   then, only if it fails, `DEPTH=1.0,1.0` (scratch `DATA_DIR`/`ASSET_ROOT`, `SSH_KEY`, `PYTHONIOENCODING=utf-8`; the Buffalo library must already be in the scratch `MotionLibrary`). A pass goes to the owner with the PNGs as the evidence for a default change, with a fresh Opus code re-review if one is made. If both fail, the stand-in's body shape (`motion/standin_depth.py` RADII) is the next input to look at, never the mesh.
3. Do the full scratch run:
   `py tools/asset-manager/scratch_run.py '{"name": "Greenlands_Boar", "prompt": "a wild boar", "animal": "Buffalo", "clips": ["idle", "walk", "run", "attack", "death"], "size_cm": 150}'`
   Use `creature_unimate_seeds` 6, `SSH_KEY=~/.ssh/id_ed25519_ra_siems`, `PYTHONIOENCODING=utf-8`, scratch-only data/assets, and an in-memory guard rejecting `app.draw_hero`/`app.hero_views`. Keep GPU 1 only.

   Expect the model review to rebuild until one body passes. If every rebuild fails, fix the local concept, never the mesh. Inspect IK release/neck logs and reject frozen or very short idle loops in visual review.
4. Send the new folder's `animations/*.png` and `source/*_rig_labels.png` to a fresh Opus for visual round 2 of 3.
5. If both reviews pass, deploy the live Forge using the steps in the parent file.

## Rules for this task

- This asset is stuck, so a Fable agent does the fix rounds (owner direction). Only fresh Opus agents review it.
- After round 3, it goes to the owner.

## Outcome
