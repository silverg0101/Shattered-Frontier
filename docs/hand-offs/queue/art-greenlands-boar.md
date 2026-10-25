# Greenlands boar: first animated Forge creature

- **Area:** art
- **Owner of the next step:** agent
- **Updated:** 2026-10-25

## Goal

The Asset Forge makes one animated Greenlands boar from a text prompt, with idle, walk, run, attack and death clips. It counts as done when:
- every clip passes the pipeline's own checks;
- a fresh Opus review against `docs/visual-review.md` passes it;
- the owner signs off.

The fixes go into the shared creature pipeline (`tools/asset-manager`), because the other starter creatures go through the same code.

## State

- Pipeline Tasks 1-10 are done and deployed to the live Forge: shared quadruped skeleton, Zoo-300K clip library, retarget checks and depth-guided concepts.
- Boar runs 6-14 fixed the following:
  - the rig: spine path, head choice, floor slab;
  - broken library clips are now rejected;
  - UniMate runs on the creature's own rig;
  - a gait plays in place with a treadmill stance;
  - a procedural gait is the fallback.
- `Greenlands_Boar_14` passes every automatic check.
- Visual review round 1 **failed every clip**:
  - doubled back shell with a ghost head (the mesh);
  - death does a backflip;
  - idle, walk and run loops don't close;
  - run sprawls;
  - attack head-bend glitch.
- The code review asked for changes: 6 important findings, 5 minor.
- A Fable agent is running visual round 2 in worktree `C:\Projects\SF-wt-creature`, branch `creature/forge`. Its in-progress commits:
  - a model judged against the concept and rebuilt on a doubled body;
  - a check that the loop closes;
  - procedural clips.
- About 23 commits on that branch are **not pushed**.

## Evidence

- Branch `creature/forge` (unpushed), latest d7f0c0ea.
- Ledger, reports and reviews (git-ignored, in the worktree): `.superpowers/sdd/2026-10-24-forge-animated-creatures/`:
  - `progress.md`;
  - `boar-fable-brief.md`, `boar-fable-report.md`;
  - `review-fable.md`.
- Sheets and visual review r1: the session scratchpad `scratchpad\boar\` (Greenlands_Boar_14_*.png, visual-review-r1.md).

## Blocked on

none (Fable round 2 in progress).

## Next step

When Fable's round 2 reports, get a fresh Opus visual review (round 2 of 3) of the new sheets and a scoped code re-review of the round 2 commits.

If both pass:
1. Rebase `creature/forge` onto origin/main.
2. Run the check (`bash $TEMP/runcheck.sh` → `check ok`).
3. Push to main.
4. Deploy the live Forge. Wait for zero active jobs, check in the container, then run `docker compose -p asset-manager --profile tailnet up -d --build`.
5. Owner sign-off.

## Rules for this task

- At most 3 visual review rounds, then it goes to the owner.
- Fable never does visual reviews.
- Generated meshes are never hand-patched in Blender; fix the cause upstream.
- Generation uses local models only. RA-SIEMS: GPU 1 only, ~4.6 GB disk, don't touch `~/sfam-dbg`.

## Outcome
