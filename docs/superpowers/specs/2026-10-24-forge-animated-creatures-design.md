# Asset Forge: animated creatures from a motion library

Status: draft for review, 2026-10-24.

## Intent

The owner wants AI agents to stock the game with creatures by calling the Asset Forge API. One request must return a finished creature: a textured, rigged model that already carries the animations it needs. The animations come from a library of real animal motion (Zoo-300K) instead of being generated per model. The owner must be able to do the same thing by hand from the Forge web page, without an agent.

The test is simple: an agent asks for "a swamp rat that can idle, walk and run", and a minute-scale job later there is a GLB in `Generated/` that drops into Unreal and plays those three loops without hand fixes.

Why this route: video-to-motion (MiniMax H3 + joint detector) drifts and needs training per model; text-to-motion (UniMate) works but is prompt- and seed-sensitive. Library motion is captured once per animal, curated once, and reused for every creature that matches that animal. The hamster run retargeted onto `Greenlands_Rat_v100` on 2026-10-24 (`~/hamrun/` on RA-SIEMS) proved the retarget step.

## Goals

- One API call (and one web form) produces a textured, rigged, animated creature GLB.
- The API lists which animals and motions exist, so a caller can pick the closest match.
- The Forge also offers its own match ("swamp rat" → Rat 0.92, Hamster 0.71, SandMouse 0.68) so the web page and agents get the same answer.
- Stages this path does not need (video motion, joint detector, detector auto-train) are not run for it.
- AI steps inside the pipeline (matching, labelling bones, judging the result) use the Forge's stored API key, never the caller's.

## Non-goals

- Body plans other than quadruped in this version (see Decisions).
- Humanoids. Players and NPCs stay on the MetaHuman and Mixamo routes.
- Replacing the existing video and UniMate motion jobs; they stay for one-off cases.
- Training any model.

## Anti-goals (it fails even if it "works")

- A creature whose paws sink into or skate over the floor, limbs that flip, or loops that pop at the seam.
- The caller having to know bone numbers, joint names or Blender.
- A job that silently ships a bad match (a snake's motion on a rat) instead of refusing.

## The workflow

Two halves: a one-time library build per animal, and the per-creature job.

### A. Motion library (offline, once per animal)

Zoo-300K (`huggingface.co/SteveZeyuZhang/Zoo-300K`, a model repo) has about 65 animal packs, 0.1–6 GB each (Rat 158 MB, Hamster 481 MB, SandMouse 1.4 GB, Dog, Cat, Fox, Horse, Bear, Spider, Crab, birds, reptiles, dinosaurs…). Each pack is BVH files on that animal's own skeleton. Inspection of Hamster showed what curation has to handle:

- Only ~5 source motions per animal (Hamster: Idle, Walk, Run, Sniff, RollAttack), each expanded into hundreds of near-identical synthetic variants.
- Many variants stall (Run_0 freezes for ~80 of 194 frames). The `.npy` joint files are unusable; use the BVH.

For each animal, a curation script:

1. Downloads one pack, keeps only the BVH, deletes the zip (RA-SIEMS has ~25 GB free; the library lives on steam-server `/mnt/ssd2`).
2. Groups files by source motion, scores each variant for continuous movement and loop closure (the `pick.py` method), and keeps the best one or two per motion, cut to a seamless loop where the motion is cyclic.
3. Writes the animal's **role map**: each skeleton joint → a body role (pelvis, spine 1..n, neck, head, tail 1..n, and per limb: front/hind/wing, left/right, segment 1..4, toe). This is drafted by the in-pipeline AI step from joint names and rest positions, then checked by the in-pipeline AI reviewer against a rendered skeleton sheet. A map it rejects, or one missing a required role, fails the curation with the reason; nothing waits on a person. The same script can be run by hand.
4. Records the rest pose, body plan (quadruped, biped, bird, snake, insect, arachnid, fish), real-world size range, and per clip: name, frames, fps, loop yes/no, travel speed.
5. Renders a small preview per clip for the web page.

The output is one folder per animal (`MotionLibrary/<Animal>/animal.json` + curated BVH + previews). The full list of Zoo-300K packs (id, aliases, body plan, size range, pack size) is a committed `zoo300k.json`; it is the matcher's universe. Each entry is `curated: true` once its folder exists. Start with Rat; others are curated on demand.

### B. Creature job (per request)

New job kind `creature`. Stages, each visible in the job's status:

1. **Match.** If the request names an animal, use it. Otherwise run the matcher and take the top hit if it clears a threshold; below it, fail with the ranked candidates so the caller can choose. Body plan must match exactly, and must be `quadruped` in this version; any other plan fails with "body plan not supported yet" and the candidates.
2. **Concept image.** Existing image job, with the prompt extended by the animal's reference stance (side-on, legs straight under the body, tail out, mouth closed), so the mesh comes out close to the library's rest pose. A caller may supply its own image instead.
3. **Model.** Existing 3D graph: Trellis2/Pixal3D geometry + UltraTex texture.
4. **Rig.** Existing SkinTokens rig, then the skin-weight cleanup that stops one limb's vertices following another (`wfix.py`/`wcopy.py` logic, made general).
5. **Label the rig.** Assign each generated bone a role, using geometry (`motion/limbs.py`) plus the AI step, and the library animal's role map as the target list. Fail if a required role (each limb, spine, head) is missing.
6. **Retarget.** For each requested clip: rest-relative rotation transfer by role, root scaled by leg-length ratio, then cleanup: paw contact locking with IK, tail follow-through, height scaling (the `~/hamrun/retarget.py` steps, generalised).
7. **Check.** Automatic gates per clip: no paw below the floor, contact paws don't slide more than a set tolerance, loop seam within tolerance, no bone flips (joint angle jumps). Then render one contact sheet per clip. If the Forge has an API key set, an AI reviewer looks at the sheets and can fail the job with a reason.
8. **Package.** Write the model folder (below). Every clip is saved on its own in `animations/`, and the clips listed in the request's `bake` list that passed their checks are combined into `<Name>_animated.glb`.

### C. The model folder: review clips one by one, ship one file

What the owner is after: each creature is a small project, not one output file. Every animation can be looked at on its own, kept or dropped, and more can be added later. What goes into the game is a single file built from the chosen clips, and it can be rebuilt any time the choice changes.

```
Generated/Creatures/<Name>/
  <Name>.glb             primary: textured and rigged, no animation
  <Name>_animated.glb    final: rig + every chosen clip as a named animation
  <Name>.json            manifest: animal, match score, scale, clip list, which clips are in _animated and why
  source/                concept image(s) and views
  animations/
    <Name>_Idle.glb      one clip per file, each with its preview and check results
    <Name>_Walk.glb
    <Name>_Run.glb
```

- **Choosing at creation.** The request's `bake` list names the clips that go into `_animated.glb` (default: every clip that passed its checks). Clips that fail their checks are still saved in `animations/`, marked failed, and are never baked automatically.
- **Combine Animations.** In the `animations/` folder the web page shows every clip with its preview, check result and a tick box, and a **Combine Animations** button. It rebuilds `<Name>_animated.glb` in the parent folder from the ticked clips and updates the manifest. The same action is an API call (`POST /api/models/<Name>/combine` with the clip list), so an agent can do it too.
- **Adding later.** Adding a clip to an existing model (another library motion, a UniMate clip, a video-motion clip) drops one more file into `animations/` on the same rig. Nothing is re-generated; re-combine to ship it.
- **New Animation.** The `animations/` folder has a **New Animation** button that makes a clip that doesn't exist yet for this creature. It queues a small job on the existing rig (no model or rig work) with one of three sources:
  - **From the library:** browse any curated animal's clips with previews (default: the creature's own animal) and pick one; it is retargeted like at creation.
  - **Describe it:** type the motion ("rears up and hisses"); UniMate generates it over several seeds, each seed is saved as a candidate with its preview, and the best-scoring one is ticked. Discard the rest with one click.
  - **From video:** the existing video-motion route, for motions neither of the others can make.
  
  Results go through the same cleanup and checks and land in `animations/` as `<Name>_<Clip>.glb`. API: `POST /api/models/<Name>/clips` with `source: "library" | "text" | "video"` and its fields.
- **Adding from an existing GLB.** The `animations/` folder has an **Add from GLB** action: pick any animated GLB in the Forge (or upload one) and tick which of its animations to take. If its skeleton matches this model's (same bone names and hierarchy), the clips are copied as they are. If it doesn't, the source skeleton is role-labelled like a library animal and the clips are retargeted, then go through the same cleanup and checks. Each picked animation becomes one file in `animations/`. API: `POST /api/models/<Name>/clips` with `from_glb` and `animations: [...]`.
- **Every clip file uses the same rig as `<Name>.glb`.** That is what makes combining a plain merge rather than another retarget. The combine step refuses a clip on a different skeleton.
- Clip names inside `_animated.glb` are the plain motion names (`Idle`, `Walk`, `Run`), so Unreal imports them as `<Name>_Idle` etc.

Stages not run for `creature` jobs: video motion (H3), joint detector, `track_solve`, `bake.py`, detector auto-train.

## API (additions to the existing `tools/asset-manager/app.py`)

- `GET /api/animals`: every pack in `zoo300k.json`: id, name, aliases, body plan, size range, `curated`, and, when curated, its clips (name, loop, seconds, preview URL).
- `GET /api/animals/match?q=swamp%20rat`: ranked candidates with scores and the reason.
- `POST /api/jobs` with `kind: "creature"`: `name`, `prompt` or `image`, optional `animal`, `clips` (list, or `"all"`), optional `fill_missing` (default true: clips the library lacks are made with UniMate), optional `bake` (subset of `clips` for the final file; default all that pass), optional `size_cm`, quality preset. Returns the job id.
- `GET /api/models/<Name>`: the model folder's manifest, clips and their check results.
- `POST /api/models/<Name>/combine` with `clips: [...]`: rebuild `<Name>_animated.glb` from those clips.
- `POST /api/models/<Name>/clips`: add clips to an existing model: `source: "library"` (`animal`, `clip`), `"text"` (`prompt`, `seeds`), `"video"` (existing motion params) or `"glb"` (`from_glb`, `animations`). Queues a job.
- `GET /api/jobs/<id>`: existing; progress in `note`/`log`, check results in the manifest.
- The result GLB and JSON download through the existing `/files/` route.

**Access.** Agents call the Forge the same way the web page does, on the LAN/tailnet, with no login (as today). "The API key" in the owner's concept is the key the Forge uses for its own AI steps. Inbound keys are out of scope for this version.

## Web page

A **Creature** tab under Create: prompt box, animal picker that fills from the matcher as you type (with each candidate's preview loops), clip checkboxes, size, preset. Submits the same `creature` job. The asset page shows a creature's clips and plays them in the viewer.

## Constraints

- GPU work stays on local models (RA-SIEMS GPU 1 only; one job per host). The AI steps (matching, labelling, reviewing) use the Anthropic key already stored in the Forge.
- The asset-manager app, static files and deploys are shared with another session; changes follow its deploy rules (gate on zero active jobs, `--profile tailnet`).
- Adjustable values (match threshold, slide tolerance, seam tolerance, height scale) go in Forge settings with defaults and bounds, not hard-coded.

## Calls made after review (builder may revisit)

- **Chains of different length.** The library animal and the generated rig rarely have the same number of spine, tail or toe bones. Retarget maps chain to chain: a chain's total bend is spread over the target's bones by length, and each bone's rest direction is aligned on its own rather than assumed equal.
- **Root bone.** Stage 5, after labelling and with the rename, adds a static `root` bone at the origin as the pelvis's parent, so Unreal root motion and in-place blending both work. Pelvis movement stays on the pelvis. Blender metres become Unreal centimetres on export; `size_cm` defaults to the middle of the animal's size range and is applied in the GLB, not at import.
- **Cheap checks first.** The image stage's existing AI review also checks body plan and stance against the matched animal, and fails before any GPU work. "Body plan must match" means the plan the reviewer sees in the concept image equals the animal's plan.
- **In-pipeline AI steps** use the Anthropic key already stored in the Forge (`DATA/anthropic_key`, `app.py:1636`).
- **Unreal check.** A headless import of `_animated.glb` (a variant of the existing `unreal_seam_check.py`) confirms bone names, root bone, clip count and clip lengths. A job is only "done" when this passes.
- **Proof scripts to git first.** `~/hamrun/` (`pick.py`, `retarget.py`, `sheet.py`) and the `wfix.py`/`wcopy.py` weight fixes on RA-SIEMS are committed under `tools/asset-manager/motion/` before anything is built on them.
- **Detector skip is per job.** Creature jobs don't queue detector training, but the global `detector_auto_train` setting stays, so a video-motion clip added to the same rig later can still train one.
- **Progress reporting** reuses the job's existing `note` and `log` fields; no new stage list.
- **`<Name>.glb` is the primary file: the rigged, textured model without animation.** No separate unrigged copy is kept.

- **The `_rigged` naming goes for creature folders.** The asset page's detector badge and the rig-job check key off a `_rigged` file name today; for creature folders they read the manifest instead.
- **The Unreal check imports against one shared quadruped Skeleton asset**, not a new skeleton per GLB, so the shared-skeleton claim is actually tested.

## Left to the builder

- How the retarget and cleanup scripts are split across Blender runs and hosts.
- Matcher internals (text embedding vs the AI step vs aliases), as long as the web page and API share it.
- The exact check tolerances, starting from what the hamster run passed.
- Storage layout under `/mnt/ssd2` and how previews are rendered.

## Decisions (taken as recommended on 2026-10-24; the owner can overturn)

- **Shared skeleton per body plan.** After stage 5 labels the bones, they are renamed to a standard set for the body plan (`root, pelvis, spine_01..03, neck, head, tail_01..06, FL_01..04, FR_01..04, HL_01..04, HR_01..04` for quadrupeds, the only plan in this version; other plans get their own set as a later decision), collapsing or padding chains to the fixed counts. Every quadruped creature then has the same bone names and hierarchy, so Unreal treats them as compatible skeletons and one Animation Blueprint serves all of them. Library role maps target the same names, so retarget is chain to chain into a known skeleton.
- **Missing clips are filled by UniMate.** If a request asks for a clip the animal's library lacks, the job generates it with UniMate (text) on the same rig, saved and checked like any other clip, and marked `source: text` in the manifest. Like every clip, it is baked only if it passes its checks. A request with `fill_missing: false` gets library clips only, and the missing ones are listed as unavailable.
- **Uncurated animals are curated on demand.** If the best match is an animal not yet in the library, the job queues its curation (stage A), waits for it, then continues; if curation fails its review, the job fails with that reason. The first creature of a new animal is slow; later ones are not.
- **Inbound auth** is out of scope (see Access).
- **Curation order:** Rat first, then on demand.
