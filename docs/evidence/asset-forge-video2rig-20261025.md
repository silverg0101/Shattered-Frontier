# Selectable video2rig motion stages

Deployed to Asset Forge on 2026-10-25. Existing defaults remain Forge reader,
Forge solver and MiniMax H3. Both reader and solver can independently select
video2rig; saved videos and saved-reading ZIPs support controlled comparisons.

Fork: [silverg0101/video2rig, Asset Forge branch](https://github.com/silverg0101/video2rig/tree/codex/asset-forge-stages),
pinned to `0a6607fab04c3f364ef2d0715edc67d933967b66` in `video2rig.lock.json`.
The adapter accepts Forge's own model/first frame/video. Training uses CUDA
Cycles on the allocated GPU and skips CPU denoising. ComfyUI's packages remain
unchanged; the fork has its own environment.

Validation:
- Five focused Forge contract tests pass, including four reader/solver routes,
  pinned-source/CUDA commands, raw-reading preservation and hash-checked replay.
- Three fork adapter tests, the existing solver self-check, Forge's isolated
  self-check with NumPy, and JavaScript syntax checks pass. The staging container
  self-check passes; its existing Zoo curation check skips without NumPy.
- Isolated GPU job `980025004`: 300 poses/100 training steps, native reader and
  native fit, successful GLB and stage bundle. Export retains nine joints and
  eight finite, normalized quaternion frames.
- Job `980025006`: replays those readings through Forge's solver. Arrays are
  exactly equal; model, video and original first-frame hashes match. Fresh
  stochastic renders are replaced with the archived reference during replay.
- Live schema exposes the selectors; both RA-SIEMS GPU hosts report available.

Fixtures/results are under ignored `Artifacts/forge-v2r-integration/`, separate
from product assets. These short, deliberately undertrained tests establish
plumbing, not creature animation quality. Real-monster A/B quality review is
still outstanding. The legacy Forge creation route remains quadruped-only;
the added video2rig creation workflow preserves native skeletons.

Deployment applies only this feature over the older live source. Unrelated local
creature changes were not deployed. Original files:
`/mnt/ssd2/asset-manager/video2rig-before-20261025`; original image:
`asset-forge-before-video2rig:20261025`. Queue was idle for restart; tailnet
connectivity and the pinned fork revision were checked afterward.

Creation follow-up: both 3D Asset and Creature now expose the shared Workflow
selector. Five additional HTTP/routing/persistence contracts pass, including
per-clip prompt selection after request normalization. The fork's geometry
fallback produced a 26-bone GLB with all 26,450 fixture vertices weighted;
SkinTokens-compatible rest export retained nine bones and 3,240 weighted vertices.
These are rig/plumbing checks, not visual acceptance. Live source backup:
`/mnt/ssd2/asset-manager/creation-before-20261025`; prior image:
`asset-forge-before-creation:20261025`. Both live forms were checked in-browser.
3D Asset's workflow controls were subsequently moved from the source-image card
to the right-hand Workflow & animation section, as marked by the owner. The
static update was verified without restarting the active creature job.

Form restructuring follow-up: Create tabs now read Image(s) to Asset, Images,
and Prompt to Asset. Workflow stays visible; animation tuning and model tuning
start collapsed under Advanced. Prompt Details adds length help and quality
buttons; seed/face/texture overrides use the shared typed parameter bounds and
reach both model builders. Six creation contracts pass locally, over the merged
live-source baseline, and in the staged Docker image; five motion contracts,
app --check and JavaScript syntax checks also pass. An isolated browser submit
queued high quality with seed 42, 45,000 faces and 1024px textures unchanged.
Preview captures: Artifacts/forge-prompt-form-preview.png and
Artifacts/forge-prompt-details-preview.png. These are functional evidence;
independent visual review and owner layout sign-off remain outstanding.

Deployment was staged while jobs 1 (Diseased Chicken) and 2 (Worldshaper) were
running. One-time server rollout: /mnt/ssd2/asset-manager/form-stage-20261025/;
rollout.json records waiting/deployed/failed state. It waits for an idle queue,
checks source hashes, freezes briefly and rechecks queue.db to avoid racing a new
job, then promotes the tested image and recreates Forge with the tailnet profile.
The freeze/stop path was verified on a disposable container. Staged image:
sha256:40b756981b669dfffe799d19fba97cd2e56fdb7f82849e096fbeaf80ef6ea3f9.
At this observation it was waiting; no active job was interrupted. Unrelated
live/local creature changes are excluded from this deployment.

Automated native QA follow-up: `native_quality.py` now gates native creation and
later native clips with fresh Claude Opus reviews, CPU-rendered orthographic and
branch-pose evidence, all-frame motion measurements, and at most three review
rounds per stage. A separate planner can submit only a validated skeleton JSON;
Blender re-skins it, then an independent call reviews the new candidate. Refused,
invalid or exhausted checks stop automatically. Settings exposes typed limits,
snapshotted at job start. Per-attempt models, reviews, measurements and source
stages are retained; automatic baking requires a passed motion review. Manual
Combine preserves the recorded QA flags. Native status now distinguishes rig,
reader inference/training, solver, baking and quality, with evidence links.

Real chicken evidence: the original 15-bone geometry fallback was rejected for
misplaced head/neck controls, missing wing/tail controls and generic leg chains.
The automated planner/build produced 23 anatomical bones. Independent review
passed that rig on round 3; owner/engine acceptance remains pending. Round 2
exposed diagnostic-camera orientation and imported use_deform metadata errors;
the renderer now asserts its vertical axis and reads actual skin influences.
The original walk independently failed QA: root travel was 181.51% of height.
An isolated in-place test reduced planar travel to 0.000008% while preserving
vertical bob and relative joint steps; it does not cure the original rig's anatomy.
Imported Blender 5.1 bone-display Icospheres are excluded before re-rig bounds
and remeshing (fixture: correct 0.859 m creature height instead of 2 m). Fork
revision: 3e7beea0acc1e955ee4441f5bd1cac2aa601e344.

The original Diseased_Chicken GLBs were preserved and their manifest now records
failed rig/walk QA, while retaining the historical baked flag. A separate
Generated/Creatures/Diseased_Chicken_QA contains the reviewed rig and source/review
history. Its replacement Walk was queued as job 2 with 3000 reader poses, 9000
steps and one MiniMax take. At the latest observation its video was generated in 11m55s and its joint reader
is training/reusing its checkpoint; no corrected walking quality is claimed. Other owner wolf jobs were untouched.
Proof captures: Artifacts/forge-chicken-stage-history.png and
Artifacts/forge-chicken-reviewed-rig.png; full rig views in
Artifacts/chicken-rig-investigation/rig-3/.

Validation: creation contracts 7, quality contracts 8, native-status contracts 7,
motion contracts 5, app --check and JS syntax pass. The fork's observation tests
(3) and Blender widget regression (1) pass. Deployment merged only these changes
over older live source. Backend image 90d2e11c1a2669a9f7057ae579c644ac9d127ad25fb6920ef605b2655f692f8a
is running, with the final static UI hot-copied without restarting active jobs;
the persistent future image is cbcc00f697d38418cd469c88cefd052ddbb3f6d141aecde5cf77e59c046bfb33.
Backups: /mnt/ssd2/asset-manager/native-qa-before-20261025 and
native-qa-camera-before-20261025. Deployment evidence and rollout status are in
/mnt/ssd2/asset-manager/native-qa-stage-20261025/.

## October 26 pipeline rerun

Original job5 used `video2rig` with `skintokens`. Native creation now defaults to the fork auto-rig; SkinTokens remains an explicit choice. The `source_glb` input preserves the exact unrigged mesh through the same pipeline.

Source correction: an initial rerun mistakenly selected an unrelated Oct 23 GLB; that failed run was removed. The correct wings-out chicken was recovered from the SkinTokens input cache for original Comfy prompt `95cd5170-d0ed-428f-b2d3-cd7bac01fa87`. It has 27,167 vertices, 29,996 triangles, 2 textures, no skins or animations, and SHA256 `1bc1b58f3747c7718e8f47d33986bc83bd22699c639b66792062bae52d89fbca`. The source is preserved at `Imports/Diseased_Chicken_2_unrigged.glb`.

Job 4 completed four UniRig draws and built a 24-bone candidate, but rig 2 failed while rendering evidence because the disk was full, before its independent review. After the user's authorization, only `/home/raadmin/sfam-dbg`, `/home/raadmin/rustmercy-worker/.cache/uv`, `/home/raadmin/unimate-eval`, and `/home/raadmin/anytop` were deleted. This recovered 25.15 GiB and left about 26 GiB free; the worker parent was preserved. Running image `780f7f38dc8089818f2e51b6707ecfd36e2fda7a80395507de6a6d24bf05ae6f` includes the storage preflight and preservation of the real unrigged source.

Fresh job 6 used that exact source with `video2rig` and `Walk`. Its candidate `3f2277d03cbbf7d070788aac32bdf99110554b06340a1a217e14686e20691ce6` passed independent Opus rig review on round 2 with 24 bones and no issues. All three Walk attempts failed in-place QA: root travel was 182.23%, 167.45%, and 188.14% of height. The unchanged source and rig passed; no motion is accepted. Failed Run attempts remain as `*_Run_FAILED_Round_{1,2}.glb`, with manifest `passed=false` and `baked=false`.

## Direct-gait proof and current limits

The direct-gait path uses the actual accepted controller and an Opus-authored plan; no hand-edited motion was used. Initial Walk and Idle reviews passed, but the wings remained spread. At the owner's request, the plan was revised to fold both wings against the body. Round 2 failed because the left proximal Shoulder was omitted; the full-wing-branch rule was tightened. In the final allowed round, Walk passed with folded wings, while Idle failed as too subtle. The three-round cap is exhausted; no further Idle attempt is authorized. The published combined preview contains Walk only, SHA-256 `57951aae207fd764eaa4fba30c7d17e908b337366d190e4793d3b504b2be2658`, at `Generated/Creatures/Diseased_Chicken_2_Direct_Gait_Preview`. Evidence is under `Artifacts/native-gait-proof/wings-ground-r3`.

The preview workflow is limited to Walk pending qualification of other clips. Fresh isolated Unreal 5.8 import of the published folded Walk passed with 31 keys and one warning: `geometry_0` is a skinned-mesh node that is not the root. The real game-project import remains blocked by missing `UnrealEditor-MinimalInventoryRuntime.dll`; this scoped import does not prove game startup or playback. The selectable workflow UI is visible and owner-confirmed. Backend image `76edfd44aec764134ea9cb1679b8bf939b0cb769874edf42df3ec2dc46c72e31` is deployed through the original Compose path; data mounts, SSH access and Tailscale sidecar were preserved. Deployment observed zero active jobs. Independent visual UI review and owner review remain outstanding.

