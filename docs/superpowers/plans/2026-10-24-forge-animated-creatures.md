# Asset Forge animated creatures Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A `creature` job on the Asset Forge turns a prompt into a model folder holding a textured quadruped on the shared quadruped skeleton, one GLB per library clip, and a combined `<Name>_animated.glb`, with the same flow on the web page.

**Architecture:** New logic lives in a new module `tools/asset-manager/creature.py` (catalogue, matcher, manifest, job runners), imported by `app.py`, which only gains routes, the job dispatch and settings. Blender work is new scripts in `tools/asset-manager/motion/`, run on the GPU host through the existing `run_blender`. Curated motion lives in `DATA/MotionLibrary/<Animal>/`.

**Tech Stack:** Python 3 stdlib server (`app.py`), SQLite queue, Blender 4.2 headless over SSH (`run_blender`), RA-SIEMS GPU 1, UniMate in `~/unimate-eval` on RA-SIEMS, Anthropic key via `ssh_anthropic`, single-page UI `static/index.html`, Unreal 5.8 Python for the import check.

**Spec:** `docs/superpowers/specs/2026-10-24-forge-animated-creatures-design.md`

## Global Constraints

- Work in worktree `C:\Projects\SF-wt-creature` (branch `creature/forge`); push with `git push origin HEAD:main` after each task's commit, after `git fetch origin && git rebase origin/main`. Stage files by name only; check `git diff --cached --stat`. Never force-push.
- Tests are `assert`s added to `check()` in `app.py` (run `python tools/asset-manager/app.py --check`, expects `check ok`), plus one real run on the hosts where a task says so.
- GPU work on RA-SIEMS GPU 1 only (`CUDA_VISIBLE_DEVICES=1`); RA-SIEMS `/` has ~25 GB free: keep only curated BVH there, delete packs after curation.
- Don't touch the running asset-manager services until Task 9's deploy; deploy only with zero active jobs, `docker compose --profile tailnet up -d --build` in the steam-server checkout.
- Shared quadruped skeleton bone names, exactly: `root, pelvis, spine_01, spine_02, spine_03, neck, head, tail_01..tail_06, FL_01..FL_04, FR_01..FR_04, HL_01..HL_04, HR_01..HR_04` (limb segments 01 = upper, 04 = paw/toe). `root` is the parent of `pelvis`, static, at the origin.
- Body plan `quadruped` only; other plans fail with `body plan not supported yet` plus the ranked candidates.
- Adjustable values are settings in `DEFAULT_SETTINGS` with bounds: `creature_match_threshold` (0.5, 0–1), `creature_slide_cm` (1.0, 0–10), `creature_seam_deg` (3.0, 0–30), `creature_height_scale` (0.65, 0.2–1.0), `creature_unimate_seeds` (4, 1–8).
- Never write the Anthropic key anywhere; use `ssh_anthropic`.

## Review Focus

- A prompt that matches nothing well ("teapot"): job fails with ranked candidates, never picks the top one below threshold. (Task 1 test.)
- A name with spaces or punctuation ("Swamp Rat #2"): folder and file stems are sanitised the way `run_motion` does (`re.sub(r"[^\w-]+", "_", ...)`). (Task 5 test.)
- Combine with a clip on a different skeleton, or a missing clip file: refused with the clip's name, `_animated.glb` left as it was. (Task 5 test.)
- Re-running a creature job with the same name: a new folder (`unique()`), never overwriting an existing model folder. (Task 5 test.)
- A cancelled creature job mid-curation or mid-retarget: status `cancelled`, host work folder cleaned, no half-written model folder in `Generated/`. (Task 5 test with a stubbed stage.)

---

### Task 1: Animal catalogue and matcher

**Files:**
- Create: `tools/asset-manager/motion/zoo300k.json`, `tools/asset-manager/creature.py`
- Modify: `tools/asset-manager/app.py` (routes in `Handler.route` next to `/api/schema`; asserts in `check()`)

**Interfaces:**
- Produces: `creature.load_catalogue(library: Path) -> list[dict]` — every pack from `zoo300k.json` as `{id, name, aliases, body_plan, size_cm: [min, max], pack_mb, curated: bool, clips: [...]}` (clips only when `library/<id>/animal.json` exists; then read from it).
- Produces: `creature.match(query: str, catalogue: list[dict]) -> list[dict]` — up to 5 `{id, score, reason}` sorted by score, scores in 0–1.
- Produces routes `GET /api/animals` → `{"animals": [...]}`, `GET /api/animals/match?q=` → `{"candidates": [...]}`.

- [ ] **Step 1:** Build `zoo300k.json` from the Zoo-300K model repo listing (`https://huggingface.co/api/models/SteveZeyuZhang/Zoo-300K/tree/main`): one entry per animal pack zip (skip captions, `HumanML3D`, `momask*`, `zoo-300k*`, `archive.*`, `npy`, `new_joints`, `texts*`, `supp*`, `.pth`, `.py`, `.mp4`, duplicates like `Crocodile_vid`, `SABREALL_up`). Fields: `id` (zip stem), `name`, `aliases` (common names, e.g. Rat: rat, rodent, sewer rat, swamp rat; Hamster: hamster, gerbil, guinea pig), `body_plan` (quadruped / biped / bird / snake / insect / arachnid / fish / crustacean), `size_cm` (nose-to-tail real range), `pack_mb`, `file` (zip name).
- [ ] **Step 2: Failing asserts in `check()`:**
```python
cat = creature.load_catalogue(Path(tempfile.mkdtemp()))
assert any(a["id"] == "Rat" and a["body_plan"] == "quadruped" and not a["curated"] for a in cat)
top = creature.match("swamp rat", cat)
assert top[0]["id"] == "Rat" and top[0]["score"] >= 0.5 and len(top) <= 5
assert creature.match("teapot", cat)[0]["score"] < 0.5
assert creature.match("Rat", cat)[0]["score"] == 1.0
```
- [ ] **Step 3:** Run `python tools/asset-manager/app.py --check`; expect failure on `creature`.
- [ ] **Step 4:** Implement `load_catalogue` and `match`: exact id/alias = 1.0; otherwise best token overlap between query words and name/alias words (stdlib `difflib.SequenceMatcher` per token, ≥0.85 counts as a hit), score = hits / query words, reason names the matching alias. No AI call (spec leaves matcher internals to the builder; the web page and API share this function). Add the two routes.
- [ ] **Step 5:** `--check` prints `check ok`. Commit `creature.py`, `zoo300k.json`, `app.py`.

### Task 2: Library curation (stage A) as a `curate` job

**Files:**
- Create: `tools/asset-manager/motion/zoo_curate.py` (runs on the host, plain Python + numpy), `tools/asset-manager/motion/skeleton_sheet.py` (Blender: render the BVH skeleton with joint names, side + top)
- Copy in first, unchanged, as the proof base: RA-SIEMS `~/hamrun/pick.py`, `~/hamrun/retarget.py`, `~/hamrun/sheet.py`, `~/sfam-dbg/v100/wfix.py` → `tools/asset-manager/motion/proof/` (commit separately: "motion: commit hamster-run proof scripts").
- Modify: `creature.py` (`run_curate`), `app.py` (dispatch `"curate": creature.run_curate`, `POST /api/jobs` accepts `kind: "curate"` with `{animal}`).

**Interfaces:**
- Consumes: `load_catalogue`, `app.ssh`, `app.ssh_anthropic`, `app.run_blender`, `app.host_tools`.
- Produces: `DATA/MotionLibrary/<id>/animal.json` = `{id, body_plan, fps, rest: {joint: [x,y,z]}, role_map: {zoo_joint: standard_bone}, clips: [{name, file, frames, fps, loop, travel_cm_s}]}`, `clips/<name>.bvh`, `previews/<name>.png`, `skeleton.png`.
- Produces: `creature.run_curate(jid, host)` and `creature.ensure_curated(animal_id, host, jid) -> Path` (runs curation inline if missing; used by Task 5).

- [ ] **Step 1:** `zoo_curate.py <pack.zip> <out_dir>`: extract only `*/bvh/*.bvh`; group by source motion (the token before `_expansion_edit`/`_in_betweening_edit`, or the base name); score each file by per-20-frame-block mean rotation change (reject any block < 0.05°/frame) and loop closure (start vs end pose rotation + paw distance); keep the best per source motion; for cyclic motions (walk, run, trot, gallop, scamper, swim, fly) cut the best loop (≥ 15 frames) and distribute residual so frame end = frame 0; write `animal.json` minus `role_map`. Port the scoring from `proof/pick.py`.
- [ ] **Step 2:** Role map: `skeleton_sheet.py` renders `skeleton.png`; an `ssh_anthropic` step sends joint names + rest positions + the sheet to Claude (`claude-opus-5-5`) asking for a map onto the shared quadruped bone names (several zoo joints may map to one chain; unmapped joints listed), then a second call reviews the map against the sheet and answers `PASS` or `FAIL: <reason>`. FAIL or any missing required role (`pelvis, spine_01, head, FL_01..04, FR_01..04, HL_01..04, HR_01..04`) fails the job with the reason.
- [ ] **Step 3:** `run_curate`: download `https://huggingface.co/SteveZeyuZhang/Zoo-300K/resolve/main/<file>` on the host into the job folder, run steps 1–2, copy results to `DATA/MotionLibrary/<id>/`, delete the zip and job folder (also on failure/cancel).
- [ ] **Step 4:** `check()` asserts on a tiny synthetic BVH written in the test (3 joints, 40 frames, one 10-frame freeze): the frozen variant is rejected and the moving one kept; loop cut makes frame 0 == last frame within 1e-6.
- [ ] **Step 5 (real run):** queue `curate` for `Rat` on RA-SIEMS; expect `DATA/MotionLibrary/Rat/animal.json` with ≥ 3 clips and a role map that passed review. Open `skeleton.png` and one preview and check them by eye. Commit.

### Task 3: Rig standardisation

**Files:**
- Create: `tools/asset-manager/motion/rig_standard.py` (Blender)
- Modify: `creature.py` (`standardise_rig(jid, host, model_glb) -> dict`)

**Interfaces:**
- Consumes: SkinTokens-rigged GLB (bones `bone_N`), `motion/limbs.py` geometry grouping, `ssh_anthropic`.
- Produces: GLB on the shared quadruped skeleton (exact names in Global Constraints), `root` added as `pelvis`'s parent, chains collapsed/padded to fixed counts (merge weights of collapsed bones into their kept neighbour; padded bones inserted along the chain with no weights), cross-limb skin weights cleaned (port `proof/wfix.py`: a vertex weighted to two different limbs keeps only the limb nearest by bone distance). Returns `{"labels": {bone_N: standard}, "dropped": [...], "warnings": [...]}`.

- [ ] **Step 1:** Labelling: `limbs.py` groups give the limbs, head, tail and body; front vs hind by distance along the spine from the head; left vs right by sign of X. An `ssh_anthropic` review of a rendered labelled-skeleton sheet answers PASS/FAIL; FAIL fails the job.
- [ ] **Step 2:** `rig_standard.py <in.glb> <labels.json> <out.glb>` renames, adds `root`, collapses/pads, cleans weights, exports glTF (Y-up, metres, `export_yup=True`).
- [ ] **Step 3:** `check()` asserts on `creature.chain_resample(["a","b","c","d","e"], 3)` → `[["a","b"],["c"],["d","e"]]`-style grouping (pure function used for collapse; write it in `creature.py`), and padding 2 → 4 returns the 2 sources plus 2 inserted names.
- [ ] **Step 4 (real run):** run on `Generated/Greenlands_Rat_v100_weightfix.glb`; open the output in Blender and confirm the exact bone list and that a front paw's vertices don't move with a hind leg. Commit.

### Task 4: Retarget, cleanup and checks

**Files:**
- Create: `tools/asset-manager/motion/zoo_retarget.py` (Blender), `tools/asset-manager/motion/clip_sheet.py` (contact sheet + paw-height plot, from `proof/sheet.py`)
- Modify: `creature.py` (`retarget_clip(jid, host, rig_glb, animal_dir, clip, size_cm) -> dict`)

**Interfaces:**
- Consumes: standardised GLB (Task 3), `animal.json` + `clips/<clip>.bvh` (Task 2), settings `creature_height_scale`, `creature_slide_cm`, `creature_seam_deg`.
- Produces: clip GLB (rig + one action named the clip, in place, loop closed) and `checks = {"min_paw_cm", "max_slide_cm", "seam_deg", "flips", "passed": bool, "reason"}`, plus `sheet.png`.

- [ ] **Step 1:** Port `proof/retarget.py` to the role map + standard names: rest-relative rotation transfer per bone; for chains of different length, spread each source chain's total rotation over the target chain by bone length; root translation scaled by hind-leg length ratio; vertical bob × `creature_height_scale`; scale the model to `size_cm` (applied in the GLB).
- [ ] **Step 2:** Cleanup from the proof: paw contact by height (lowest 30% of each paw's range and within 1.5 cm of its minimum) pinned with leg IK, 2-frame blend, baked to FK, four correction passes; tail follow-through (each tail bone 35% of base rotation, one frame later per segment, plus once-per-loop sway); loop seam closed.
- [ ] **Step 3:** Checks: `min_paw_cm >= 0`, `max_slide_cm <= creature_slide_cm` during contact, `seam_deg <= creature_seam_deg`, `flips == 0` (any joint angle change > 90° between frames). Then, when an Anthropic key is set, an `ssh_anthropic` review of `sheet.png` answers PASS or `FAIL: <reason>`. `passed` is all four checks and the review.
- [ ] **Step 4:** `check()` asserts `creature.checks_pass({"min_paw_cm": -0.1, ...}, settings)` is False with reason naming `min_paw_cm`, and True on the hamster-run values (`min 0.2, slide 0.09, seam 1.9, flips 0`).
- [ ] **Step 5 (real run):** Rat library `Run` onto the Task 3 output; open `sheet.png`, compare with the proof's `hamrun_sheet.png`. Commit.

### Task 5: `creature` job, model folder, manifest, Combine

**Files:**
- Create: `tools/asset-manager/motion/combine.py` (Blender: merge clip GLBs' actions onto one rig, export with NLA tracks named by clip)
- Modify: `creature.py` (`run_creature`, `write_manifest`, `combine_model`), `app.py` (dispatch `"creature"`; `POST /api/jobs` `kind: "creature"`; `GET /api/models/<Name>`; `POST /api/models/<Name>/combine`; `queue_detector` not called for creature jobs; asset listing reads a folder's `<Name>.json` for the badge instead of the `_rigged` stem)

**Interfaces:**
- Consumes: Tasks 1–4, existing image job internals (`codex_image`, `codex_review`) and the 3D graph (`build_graph` with `rig: "now"`), UniMate on RA-SIEMS (`~/unimate-eval`, checkpoint `unimate_uniml3d_f60_v3`, prompts in training-caption style "An object …", `creature_unimate_seeds` seeds, best by `checks_pass` then lowest seam).
- Request: `{kind: "creature", name, prompt | image, animal?, clips: [..] | "all", fill_missing: true, bake?: [..], size_cm?, preset}`.
- Produces folder `Generated/<Name>/`: `<Name>.glb`, `<Name>_animated.glb`, `<Name>.json`, `source/`, `animations/<Name>_<Clip>.glb` + `<Name>_<Clip>.png` (sheet) per clip.
- Manifest: `{name, animal, match: {score, reason}, body_plan, size_cm, skeleton: "quadruped_v1", clips: [{name, file, source: "library"|"text"|"video"|"glb", checks, baked: bool}], unreal: {"state": "pending"|"pass"|"fail", "reason"}, job}`.
- `creature.combine_model(name: str, clips: list[str]) -> dict` — refuses (raises `ValueError` naming the clip) a missing clip file or a clip whose bone list differs from `<Name>.glb`; otherwise rebuilds `<Name>_animated.glb` via a temp file + rename, updates `baked` flags.

- [ ] **Step 1:** Failing `check()` asserts: name sanitising (`"Swamp Rat #2"` → `Swamp_Rat_2`); a second folder for the same name gets `unique()`'s suffix; `combine_model` on a temp folder whose clip manifest lists a bone set different from the model's raises `ValueError` containing the clip name and leaves the existing `_animated.glb` bytes unchanged; a stubbed stage raising `InterruptedError` ends `cancelled` with no folder under the output dir (build the folder in a temp dir and move it into `Generated/` only at the end).
- [ ] **Step 2:** `run_creature` order: match (threshold, quadruped only) → `ensure_curated` → concept image with the animal's stance appended to the prompt ("side-on, legs straight under the body, tail out behind, mouth closed, plain white background") and `codex_review` also checking body plan/stance → 3D + texture + SkinTokens rig → `standardise_rig` → per clip `retarget_clip` (missing clips: UniMate on the standardised rig when `fill_missing`) → `combine_model(bake or all passed)` → manifest → move folder into `output_folder`. Progress in `note`/`log` via `step()`.
- [ ] **Step 3:** Implement `combine.py`, the routes and listing change.
- [ ] **Step 4:** `--check` → `check ok`.
- [ ] **Step 5 (real run):** creature job "Swamp Rat", animal auto, clips all; expect the folder, ≥ 3 clips, `_animated.glb` with every passed clip as a named animation. Open the contact sheets. Commit.

### Task 6: New Animation and Add from GLB

**Files:**
- Modify: `creature.py` (`run_clip_job`), `app.py` (`POST /api/models/<Name>/clips`, dispatch `"clip"`)

**Interfaces:**
- Request: `{source: "library", animal, clip}` | `{source: "text", prompt, seeds?}` | `{source: "video", ...existing MOTION_PARAMS}` | `{source: "glb", from_glb, animations: [..]}`.
- Produces new files in `animations/` and manifest entries (not baked). Text source saves one candidate per seed (`<Name>_<Clip>_s<seed>.glb`), best marked `best: true`. Glb source: same bone names → copy actions; otherwise label the source skeleton like Task 3 and retarget like Task 4.
- Video source runs the existing `run_motion` steps on `<Name>.glb` and is the only path that may queue a detector.

- [ ] **Step 1:** `check()` asserts the request validator rejects an unknown `source`, a `glb` request without `animations`, and a `library` clip the animal doesn't have (message names the clip and lists the animal's clips).
- [ ] **Step 2:** Implement; real run: add the Rat library `Idle` (if not already) and one text clip "An object sniffs the ground." to the Task 5 folder; combine with it. Commit.

### Task 7: Unreal import check (desktop tool)

**Files:**
- Create: `tools/asset-manager/unreal_creature_check.py` (Unreal Python, run like `unreal_seam_check.py`)
- Modify: `app.py` (`POST /api/models/<Name>/unreal` stores `{state, reason}` in the manifest)

**Interfaces:**
- Imports `<Name>_animated.glb` as a skeletal mesh against the shared skeleton `/Game/Frontier/Creatures/Shared/SK_Quadruped_v1` (created from the first import if missing), under `/Game/Frontier/Tests/AssetForge/<Name>`; asserts the bone list equals the Global Constraints list, `root` is the top bone, one AnimSequence per manifest clip with `baked: true` and matching frame counts; POSTs the result to the Forge.
- Deviation from the spec, noted for the owner: Unreal runs on the desktop, not the Forge server, so the job finishes with `unreal: pending` and this tool flips it to `pass`/`fail`. The web page shows the state.

- [ ] **Step 1:** Implement; real run on the Task 5 output with the editor closed (headless `UnrealEditor-Cmd.exe ... -run=pythonscript`). Expect `pass` in the manifest. Commit.

### Task 8: Web page

**Files:**
- Modify: `tools/asset-manager/static/index.html`

- [ ] **Step 1:** Create > **Creature** tab: name, prompt, animal picker filled from `/api/animals/match` as you type (shows score, curated or not), clip tick boxes from the chosen animal (plus a free-text "other clips" field for UniMate fill), fill-missing toggle, size, preset → `POST /api/jobs` `kind: "creature"`.
- [ ] **Step 2:** Model folder view (a folder with `<Name>.json`): clips list with sheet thumbnail, source, check result, baked tick box; **Combine Animations** button → `/combine`; **New Animation** (library / describe / video) and **Add from GLB** (asset picker or upload, then its animation list) → `/clips`; Unreal state badge.
- [ ] **Step 3:** Check in the built-in browser against a local run (`python app.py` with `ASSET_ROOT`/`DATA_DIR` pointing at a copy of the Task 5 folder): every button issues the right request (network panel), no console errors. Screenshot for the owner. Commit.

### Task 9: Docs and deploy

**Files:**
- Modify: `tools/asset-manager/API.md` (new routes and job kinds), `docs/dependency-map/art-pipeline.md` (creature path, new files, MotionLibrary location).

- [ ] **Step 1:** Update both docs; commit and push.
- [ ] **Step 2:** On steam-server, with zero active jobs (`GET /api/queue`), pull and `docker compose --profile tailnet up -d --build`; `GET /api/animals` returns the catalogue with Rat curated. Report.
