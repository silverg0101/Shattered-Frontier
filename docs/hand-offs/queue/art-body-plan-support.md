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

## Next step

Fresh source/input recheck `bird-gate-review.md`: source5c50c886+ff4a301c PASS (16 independently rerun tests); reviewed proxy correct, saved inputs FAIL only actual tail projection drift. Reviewed source rebased/pushed through3605248b (source7d9ffa8d/fe10f8ee). Keep proxy unchanged. New bounded fix explicitly carries a horizontal backward rump tail below the head, optionally hidden head-on, into both prompts and accepted-side front reference. One scratch retry uses side0.55/front0.90/end1.0 in memory; shipped defaults0.55/0.75/0.7 stay unchanged. Save every input/verdict and stop at actual outcome; no deployment before model/rig/Walk+import proof.

Fresh independent6.1-Sol source/input review of5c50c886+ff4a301c and950011 evidence, then implement only its fix list (max3 external review rounds, then owner). Complete actual Chicken rig/Walk export and compatible Ostrich reuse before readiness/deployment. Unreal requires the controller's assigned slot; deployment requires integration proof, app regression and idle live queue. Shared scratch report: `Artifacts/Orchestration/20261025-resume/asset-forge-body-plans-report.md` in main checkout.
