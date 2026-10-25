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
- Bounded raw Chicken proof950007 completed in64s; its original two-lowest-tip result was invalid because it picked two left toes. The independent review's four source fixes are implemented and await recheck: source wing root/tip requirements now differ from four target segments; raw contacts collapse distinct leg/toe branches and ignore arbitrary End Site directions; padded wings still prove weighted deformation/isolation; actual video instructions are validated and reviewed. A local-only rescore of cached Walk selects `Bip01_R_Foot` and `Bip01_L_Foot`, closes frames0..43 (score4.7780), and reports no source defects. No pack rerun, external provider, GPU or Unreal work occurred during fixes. Remote950007 input folder was removed; no process from this lane remains running.
- Fresh independent 6.1-Sol code review, then in-engine/concept visual review and owner signoff remain required. Hold push pending controller review.

## Bounded boar diagnostic

Reviewed `666a716d` probe ran once with cached Buffalo, GPU1 pin, local-only guards and in-memory `DEPTH=0.9,1.0`: scratch950006 first side review PASS, 433s. `--one-try` intentionally ended `interrupted` after that verdict. Front/model/engine are unproved. No1.0/1.0 retry because retry was authorized on failure only. Defaults, live settings and tokens unchanged. Evidence: main checkout `Artifacts/Orchestration/20261025-resume/boar-concept/depth09/`.

## Next step

Independent code review, then GPU1-only scratch Chicken curation/generation and the same adapter on Ostrich. Keep the contract/visual gates honest; use the controller's Unreal slot only after assignment. Shared scratch report: `Artifacts/Orchestration/20261025-resume/asset-forge-body-plans-report.md` in main checkout.
