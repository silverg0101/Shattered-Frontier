# Forge animated creatures (parent)

- **Area:** art
- **Owner of the next step:** agent
- **Updated:** 2026-10-25
- **Worktree:** lane `a` (`pwsh tools/Enter-Lane.ps1 -Name a` gives `C:\Projects\SF-lane-a`). If it refuses because it is busy, use any free lane. All earlier work is on main; the old `SF-wt-creature` worktree is gone.

## Goal

The Asset Forge turns a text prompt into a rigged, animated game creature on the shared `quadruped_v1` skeleton. The Greenlands starting-area creatures go through it into the game.

## Sub-tasks

| File | State | Next |
|---|---|---|
| `art-greenlands-boar.md` | Concept diagnosis done: the stand-in is right, Qwen staggers the legs; three prompt attempts spent, none passed side and front | Fresh Opus re-review of the probe fixes, then at most two concept-only depth probes (`scratch_concept.py` with `DEPTH=0.9,1.0`, then `1.0,1.0`; defaults unchanged), then full run and visual round 2 |
| `art-starter-creatures.md` | Blocked on the boar | Deer |
| `art-quadruped-unreal-import.md` | Blocked on a signed-off creature | Re-run the Task 7 import |
| `gameplay-out-of-place-npc.md` | Sandbox server rule complete (`354c0284`); model blocked on the boar | Approved model, owner sign-off, then Greenlands placement |

## What to track, and where

- **This file:** the sub-task table, the pipeline facts below, and rulings that apply to every creature. One line each.
- **Each sub-task file:** its own State, Evidence, open fix list and Next step. Update it whenever a run or review finishes.
- **Not committed:** subagent briefs, reports, raw run logs and sheets. They live in the session scratchpad and are thrown away once their result is written into the task file. If a later session needs a result, write it into the task file in one line.
- When a sub-task finishes, fill in its Outcome, `git mv` it to `completed/`, and update the table here.

## How we work

- **The controller session delegates and checks; it does not build.** It writes each piece of work as a short brief file: goal, pass target, evidence paths, rules. One subagent does the work, writes a report file and returns only status, commits and concerns. This keeps the controller's context small. The controller looks at every returned image itself.
- **Routing:** easy, bounded work goes to Sonnet. Debugging goes to Opus. An asset still failing after two fix rounds goes whole to a Fable agent with a full brief (owner rule).
- **Reviews:** each round gets two reviewers, each a fresh Opus that did not build the work:
  - a code review of the diff, with file:line and severity;
  - a visual review using `docs/visual-review.md`, concept against sheets.
  
  Implement only their fix lists. The cap is 3 visual rounds, then owner sign-off. Fable never does visual reviews.
- **Rulings:** decide instead of waiting on the owner. Record each ruling here or in the sub-task as `Ruling: X — why — cost if wrong`.
- **Git:** rebase onto origin/main, stage by name, check `git diff --cached --stat`, push right away, never force. Commit messages end with the Co-Authored-By line.

## Pipeline facts

- **Code:** `tools/asset-manager/`. API: `API.md` (Creature jobs). Route summary: `docs/dependency-map/art-pipeline.md`. Main files:
  - `creature.py`: job flow and `choose_clip`;
  - `motion/rig_standard.py`: SkinTokens rig mapped to `quadruped_v1`;
  - `motion/zoo_retarget.py`: retargeting and its gates;
  - `motion/gait_synth.py`: procedural clips;
  - `motion/zoo_curate.py`: library curation and loop cutting;
  - `motion/standin_depth.py`: the depth stand-in.
- **Route:**
  1. A depth-guided concept from a stand-in of the matched Zoo-300K animal.
  2. The 3D build.
  3. A model review against the concept, rebuilding up to 4 times on a fail.
  4. The SkinTokens rig, then `rig_standard`.
  5. Each clip from the first passing source in this order: UniMate on the creature's own rig (cut to a loop), the procedural clip, then other animals' library clips.
- **Gates per clip:**
  - `max_slide_cm` ≤ 1.0;
  - `loop_gap_cm` ≤ 1;
  - `released_pct` ≤ 25;
  - `max_pitch_deg` ≤ 35 on deaths;
  - `max_neck_deg` ≤ 85;
  - seam and flip checks;
  - a Claude review of the clip sheet.
- **Library:** the Zoo-300K packs (Buffalo, Coyote, DEER, BEAR, Hippopotamus) contain no clean walk or run loops; their takes are in-betweening edits. Expect gaits to come from UniMate or the procedural clips.
- **Check:** from the repo root, with throwaway `DATA_DIR` and `ASSET_ROOT`, load `tools/asset-manager/app.py` as module `app` (without its `__main__` block), then call `init_db()` and `check()`. It must print `check ok`. Use `py` on Windows, not `python`.
- **Scratch runs:** `tools/asset-manager/scratch_run.py`. Set `DATA_DIR` and `ASSET_ROOT` to scratch folders first. It runs on RA-SIEMS GPU 1, reading the review keys from steam-server into memory only.
- **Live Forge:** http://192.168.50.133:8090 (`steam@192.168.50.133`, `/mnt/ssd2/asset-manager/app`). It still runs deploy 08ab3a29; none of the boar work is deployed. To deploy:
  1. Wait for zero active jobs.
  2. Copy the whole `tools/asset-manager` tree from origin.
  3. Run the check inside the container.
  4. Run `docker compose -p asset-manager --profile tailnet up -d --build`.
  
  A restart fails any running jobs.

## Rulings and parked gaps

- Ruling: species follow the Greenlands wildlife table. The closest animals are Buffalo (boar), DEER (deer, stag), Hamster (rabbit), Coyote (wolf) and BEAR (bear). Cost if wrong: a gait mismatch, which the clip review catches.
- Ruling: generated meshes are never hand-patched. A mesh defect is fixed upstream (concept, build, model review). Cost if wrong: slower fixes.
- Parked as rare or cosmetic, each falling back safely:
  - http hosts skip cleanup;
  - the depth host is checked but not reserved;
  - a depth fallback discards good side views;
  - the stand-in is not kept on retry;
  - seeds are not logged;
  - walk and trot both retarget Buffalo's Trottle.

## Rules for this task

- Generation runs on local models only. The Anthropic API is used only for the review steps, and the key is never printed.
- RA-SIEMS: GPU 1 only, about 4.6 GB of disk free, never touch `~/sfam-dbg`.
- Kill only process IDs you started.

## Outcome
