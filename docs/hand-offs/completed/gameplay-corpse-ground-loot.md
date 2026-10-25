# Corpse and ground loot interaction

- **Area:** gameplay, backend, UI
- **Updated:** 2026-10-25
- **Worktree:** C:/Projects/SF-lane-d
- **State:** Gameplay and functional UI complete; owner approved for release.

## Goal

Click a corpse or use the shared action to open authorized loot with the player's bag; Shift-click or drag transfers items. Dropped loot uses its world model; the action key picks it up directly without a loot window. Selected actionable objects share a slight local highlight. Layout remains in the existing Widget Blueprints; logic and authority remain in code/services.

## Outcome

Published source through `ff59b35dd17fef0edd1a7b7f5b9161b867370bc8`. The owner explicitly approved the completed functional loot UI and blue target highlight for release on 2026-10-25 after the fresh fourth independent review passed. Item textures and lighting remain deferred; mechanical import `629a0c84` remains accepted.

The full rendered two-client run on `76d9fa3e` passed in 51.8s: real boar/wolf defeats, native corpse mouse input, both inventories, Shift/drag, deliberate world drop, native F through the existing UI/input guard, direct pickup without a loot window, target clearing and observer privacy. SQL matched alpha revision4 with2 hides and1 pelt, beta revision0 empty and exactly2 defeat receipts. No direct-open or ground-dispatch fallback.

Ground fix `1715e683` captures actual canonical/fallback mesh bounds before debit and persists the pose. Eight approved meshes, missing/invalid/no-debit cases, oversized/off-centre dedicated-to-client outer traces, bounds scaling, and PostgreSQL restart/concurrency checks passed. Shared selection, existing inventory/coupling, cancellation, authority and exact-copy invariants have precise targeted evidence.

Shared outline `ff59b35d` replaces the Nanite-incompatible overlay with reserved stencil bit7 and an owning-camera postprocess layer. Original meshes, faces, base/overlay materials and global Nanite remain intact. Exact SelectionOutlineCompatibility and ExactActionTarget passed, including real shader-map compilation with zero errors, mesh classes, camera layering, concurrent FX and detach cleanup. The separate fourth capture was Observed, not another economy acceptance run; fresh review found the selected-only pale blue rim and F prompt visible.

## Evidence

- Main ignored `Artifacts/LootWorkflow/native-ground-76d9fa3e/`: actual full workflow logs, results, four captures and hashes.
- Main ignored `Artifacts/LootWorkflow/outline-fourth-ff59b35d/`: unobstructed comparison pair, renderer logs and hashes; pawn visibility restored exactly.
- Main ignored `Artifacts/Orchestration/20261025-resume/outline-fourth-review.md` and `loot-outline-code-review.md`: independent visual PASS and source recheck.
- Worker report in the same orchestration folder retains exact targeted logs and superseded fixture-only failures. No full suite was run.

## Limits and follow-up

Unique-instance/37%-condition, invalid/range/capacity, retry/concurrency/restart, rare bonus rows and cancellation negatives retain targeted/durable evidence; no live unique-gear transfer is claimed. Harvest exactness and oversized/off-centre outer geometry retain targeted evidence. Stills alone do not prove the network behavior.

Matching package, durable-host rollout, privileged operator application and remote acceptance remain in `../queue/backend-playable-runtime-release.md` and `../queue/backend-matching-game-package.md`. Completing this gameplay handoff does not claim deployment. Frozen release source is `ff59b35d`; subsequent documentation commits do not alter that identity.
