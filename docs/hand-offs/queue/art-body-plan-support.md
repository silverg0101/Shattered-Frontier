# Add Asset Forge body-plan families

- **Area:** art; agent owns next step
- **Updated:** 2026-10-25
- **Worktree:** `C:\Projects\SF-lane-c`, branch `lane/c`

## Goal and contract

Deliver grounded Chicken and a second compatible bird through a reusable family adapter. Preserve quadruped assets and class compatibility. `grounded_bird_v1` has root/pelvis/spine/neck/head/tail plus WL/WR folded wings and HL/HR ground legs (four segments each). Only HL/HR are floor contacts. Actions: idle, walk, run, attack, death; no flight. Catalogue taxonomy is not readiness: Chicken/Ostrich explicitly opt into this experimental adapter; Bat and unimplemented families fail closed.

## Source implementation

`motion/body_plans.py` defines typed structural contracts; `bird_rig.py` labels paired raised wings and paired ground legs without species branches. Family identity now flows through curation role maps, rig build/verify, procedural gait, compatible-library selection/retarget, concepts/model reviews, manifests, API catalogue capabilities/UI text and the Unreal importer. Bird concepts use local depth or supplied images; no external image fallback. Bird generated geometry is not patched. Existing quadruped defaults and `SK_Quadruped_v1` stay compatible; birds use `SK_GroundedBird_v1`.

## Evidence and open gates

- Thirteen focused tests cover quadruped gait/contact parity, two bird proportions, missing wings, explicit catalogue routing, unsupported Bat/flight, two-foot/wing checks and local-only concepts. The committed real Chicken topology fixture covers three-joint source wings, root/tip/order failures and both branched feet. Mocked motion tests reject a frozen right foot, exercise padded/unpadded wing movement/isolation guards and reject flight video instructions before generation while forwarding valid instructions to review. App self-check passes against initialized throwaway data/assets; Python compile checks pass. These are source-contract checks, not generated-asset proof.
- Chicken end-to-end, second-species reuse, actual Blender GLB bone/animation export and Unreal import remain unproved. No Unreal job is authorized in this lane yet; another worker owns that slot. Family readiness is explicitly experimental/pending.
- Bounded raw Chicken proof950007 completed in64s; its original two-lowest-tip result was invalid because it picked two left toes. The independent review's four source fixes passed recheck: source wing root/tip requirements now differ from four target segments; raw contacts collapse distinct leg/toe branches and ignore arbitrary End Site directions; padded wings still prove weighted deformation/isolation; actual video instructions are validated and reviewed. A local-only rescore of cached Walk selects `Bip01_R_Foot` and `Bip01_L_Foot`, closes frames0..43 (score4.7780), and reports no source defects. No pack rerun, external provider, GPU or Unreal work occurred during fixes. Remote950007 input folder was removed; no process from this lane remains running.
- Independent 6.1-Sol source recheck PASS for2a93b4ff; reviewed source rebased/pushed as eb37337e. Runtime/visual gates and owner signoff remain open. No live deployment.

## Bounded Chicken runtime proof

Cached family curation950008 passed in53s through the existing provider. Full scratch proof950009 (one model attempt, local-only depth, library Walk only, no motion fallback) stopped after566s before model build: side3 passed, front1/2 failed, final front wings were spread/raised versus the folded-wing side. External fallback stayed blocked; no rig, animation export or import exists. Saved side3/front1/front2 plus stand-in images and result.json: main scratch `bird-proof/950009/`. All owned sessions/watchers ended; no GPU1 work remains from this lane.

## Bounded boar diagnostic

Reviewed `666a716d` probe ran once with cached Buffalo, GPU1 pin, local-only guards and in-memory `DEPTH=0.9,1.0`: scratch950006 first side review PASS, 433s. `--one-try` intentionally ended `interrupted` after that verdict. Front/model/engine are unproved. No1.0/1.0 retry because retry was authorized on failure only. Defaults, live settings and tokens unchanged. Evidence: main checkout `Artifacts/Orchestration/20261025-resume/boar-concept/depth09/`.

## Input-review fix checkpoint (source held for recheck)

Commits5c50c886 and ff4a301c implement only the first functional input review's three fixes: shared standing/folded-wing anatomy plus accepted-side geometry reference; tapered torso-bound wing proxy and two forward foot anchors without rig topology changes; functional geometry gates that allow explicitly harmless cosmetic/occlusion/lighting differences. Unknown failures still fail closed; connected foreground/slab risks remain blocked. Sixteen focused tests, isolated app.check and compile/diff checks pass. Direct Blender needed the sibling-module import path (setup950010 failed before image generation; ff4a301c corrected that bootstrap).

One actual bounded retry950011 ended after351s before model build. Revised Blender proxy checks pass: HL/HR anchors share one floor, wings taper backward/down with lateral bound1.1 torso radii, every view has feet baseline row749. Side1 passed; both front attempts failed a tall plume/appendage above the head versus the side's horizontal rump tail. No appearance-only gate failed. Saved all concepts/prompts/controls/verdicts, proxy metadata/log/checks and summary.json in main scratch `bird-proof/950011/`. Session79296 ended; no owned process remains. No more generation, push or deployment pending independent source/retry recheck.

## Tail-only retry checkpoint

Fresh bird-gate-review.md passed source5c50c886+ff4a301c and the unchanged proxy. Reviewed source was rebased/pushed through3605248b (7d9ffa8d/fe10f8ee). Tail-only candidate938b3d8d remains local for recheck: explicit horizontal backward rump tail below head, allowed head-on occlusion, no above-head plume; accepted-side reference always carries it.16tests/app.check/compile/diff pass.

Bounded scratch950012 completed1045s with temporary side0.55/front0.90/end1.0. Side1 and front1 passed; one model/SkinTokens GLB generated. Rig labeling then rejected2ground-leg chains/0separate wing chains. No labels were written; the later surfaced FileNotFoundError obscures the original Blender topology rejection. No standardized rig, Walk, animation export or import exists. Retained7.3MB raw GLB: main scratch `scratch-assets/Generated/Creatures/Diseased_Chicken_Retry2/source/Diseased_Chicken_Retry2_skintokens.glb`. All concepts/prompts/controls/verdicts, Blender logs, raw_rig_topology.json and summary.json are in `bird-proof/950012/`. controls.json proves persisted-before/after and shipped defaults stayed0.55/0.75/0.7. Session95164 ended; no owned process remains. No more generation or deployment.

## Failure propagation checkpoint

Fresh `bird-rig-review.md` passed tail938b3d8d and proved the retained39-joint model lacks independent torso wing chains/skin ownership: the rear pair belongs to the tail fan. Do not relabel tail/legs or weaken wing guards. `motion/blender_stage.py` now invalidates declared stale outputs, confines removal to job-local outputs that cannot contain inputs, and validates fresh nonempty outputs on both Blender routes. Python-Direct uses `--python-exit-code 1`; topology rejection stops before label review/build. Cached actual Blender-loaded negative fixture carries the source SHA and topology/weight summaries, without mesh/motion data. Nine precise stage tests pass, including actual Windows candidate/ancestor junctions that preserve targets, sentinels and earlier outputs on invalid declarations. Lexical reparse validation now precedes resolution/deletion. Prior16 family tests and isolated app.check remain applicable. Real CPU-only Blender5.1 exits1 with the original2legs/0wings error and no labels; proof in main scratch `bird-blender-failure-proof/`. No generation, GPU job, Unreal or deployment ran in this slice.

## Conditioned source-rig implementation and candidate

Reviewed tail/diagnostic/junction source is published through c5c6db9e; the only format followup normalized the retained fixture to LF with identical JSON values/source SHA. Full unpublished-range diffcheck passed before push. Current candidate adds `source_rig.py`, `source_rig_generate.py` and7 focused tests; no live routing/provider files change. Exact pinned node/tokenizer/model source hashes fail closed. The existing Comfy venv/provider installation is reused; no new package or deployment. The runner takes existing typed graph SkinToken options, one explicitly selected GPU, cached mesh and fresh output/report paths, with network/model downloads disabled. It generates an entirely new learned rig/skin, retaining cached input unchanged, and stops before target standardization. Candidate source topology is20 predicted joints, including3per folded wing, separate from4target padding. Per-inference shallow model views own their grammar wrappers; shared tokenizer state is untouched, including concurrent calls and exceptions. Passing parents alone was rejected as an implementation: this pinned TokenizerPart ignores that argument.

Evidence:7source +9stage +16family tests PASS. Actual pinned upstream logits-processor/tokenizer CPU proof enforces the exact source-parent tree against premature-EOS preference and natively decodes all20 parents,81grammar calls; CUDA remains uninitialized. Main scratch native-grammar-cpu-proof.json and source_grammar_cpu_probe.py hold reproducible evidence. This is grammar compatibility, not learned-coordinate/skin proof. Production creature/quadruped routing and defaults are unchanged.

### Frozen raw-rig acceptance before any standardization

- Retain actual raw GLB, full predicted joint/parent/skin metadata, source/input/output SHA and every verdict. Semantic role captions/counts are insufficient.
- Two torso-attached folded-wing root/tip lineages must occupy actual wing geometry and own meaningful independent wing skin. Tail-fan, leg or torso skin must not be accepted merely because a branch has a wing caption. Independently inspect actual POSITION/JOINTS/WEIGHTS and bone coordinates.
- Pose each real wing lineage on the raw rig: its owned wing region must deform while the opposite wing and both leg/foot regions remain isolated. Confirm two distinct ground legs/feet; wings are not contacts. Retain final existing exact hierarchy and weighted-wing deformation checks, including legitimate unweighted target padding.
- If genuine wing chains exist but the current classifier rejects them, retain the raw proof for fresh independent review before any classifier change. Cached39-bone model remains a fail-closed negative; never relabel its tail/legs or edit generated mesh/weights.

## Actual bounded learned-rig checkpoint950012-c1

Reviewed source5ada5e7a rebased/pushed as9e78985b. Queue0/0, GPU1util0%/2.4GB baseline and only ComfyPID1399496 passed just-in-time idle guard. One learned prediction ran with CUDA_VISIBLE_DEVICES=1 and existing normal typed rigoptions; source950012 GLB SHA stayed3f2a07f6...ce3ac56f.810grammar calls/10completed beams predicted20-joint source tree and skin54000x20 samples/29383x20 retained mesh weights. After67.59s export failed because standalone SSH lacked SKINTOKEN_BLENDER_BIN; no new inference was performed.

The unchanged3,065,402-byte predicted provider payload (SHA b8fdc3c3...374a213) and original provider-exported input mesh were preserved locally/remotely. Read only the exact configured SKINTOKEN_BLENDER_BIN from the existing Comfy process: /home/raadmin/blender/blender-4.2.3-linux-x64/blender. One CPU-only22.017s export recovery used that executable, existing blender_headless_bridge.py --op transfer and the same retained payload; response status ok. Raw candidate.glb:7,284,876bytes,20actual GLB joints, SHA e33cabef856a3c68dda13037c936ad3cdf9b5253330125e164d4ee007d80b170. This is an exported candidate, not anatomical acceptance.

All evidence in main scratch bird-source-rig-950012-c1/: launch/options/preflight, inference log/runtime, prediction joint coordinates/tree, retained_payload.pkl+input mesh/provenance, export_only.log/response, export_recovery.json and raw_export_topology.json. Original cached39-bone mesh/defaults/providers untouched; no service/backend/install/live Forge changes. Sessions23861/86663/71249/37229 ended; PID2906920 absent; GPU1 returned0%/baseline with only original Comfy process. No owned process remains. No standardization/Walk/Unreal ran. Narrow followup preflights the configured Blender executable before provider/CUDA imports/inference and records it;8source tests/compile pass, prior25stage/family evidence remains applicable.

## Next step

Fresh independent actual raw GLB/POSITION/JOINTS/WEIGHTS and pose review against the frozen acceptance above, plus bounded recheck of the pre-inference Blender dependency guard. Do not standardize before that verdict. If genuine wings reveal a classifier false negative, retain raw proof for independent review before changing it. No more inference or full model generation authorized in this checkpoint. Chicken rig/Walk export, second compatible bird, coordinated Unreal import and visual/owner acceptance remain open. No live deployment before compatible functional proof, app regression and idle queue.
