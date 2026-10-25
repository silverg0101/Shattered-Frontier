# Corpse and ground loot interaction

- **Area:** gameplay, backend, UI
- **Owner of next step:** independent recheck, then NPC hook and loot workflow worker
- **Updated:** 2026-10-25
- **Worktree:** C:/Projects/SF-lane-d

## Goal

Corpse click/action opens authorized loot and own inventory together; shift-click/drag transfers. Inventory drops retain WorldMesh; action picks ground loot directly into the bag. One local targeting highlight covers interactables. Layout/style remains in the existing Widget Blueprints.

## State

Independent checkpoint review found three interaction defects. Follow-up fixes preserve opaque body identity for sibling-only LOS exemptions, replicate/apply mesh-derived hit centre/extents, and carry one shared actor/contact into exact dispatch and HUD prompts. Editor build, four focused Loot tests, existing Gathering.Pilot.Authority, unit/host builds and durable body-ID/instance/restart/concurrency checks pass; quick independent recheck remains before integration. Evidence is under ignored `Artifacts/LootWorkflow/review-fixes-*`; client hit bounds test exercises the receive callback and physics query, not two-client replication transport.

Service/C++ consumers implemented; import dependency is main629a0c84, mechanically complete with appearance deferred. Rewards adds durable create/list/open/take/drop; existing ledger transactions freeze all recipients/rolls, preserve exact copies/condition and receipts, and fence direct-vs-corpse delivery. Source actors recover after restart; C++ routes existing inventory Widget Blueprints, click/F, shift-click/drag, WorldMesh drops and direct ground pickup. Shared local targeting component owns the rim. Combat worker still owns the final RewardKill hook into SFLootWorldSubsystem; this API checkpoint does not claim a tested full player workflow.

## Evidence

Editor build passes; ShatteredFrontier.Loot.Authority/UniqueRows and six Inventory.Panel tests pass. InventoryLedger.Tests --world-loot passes. Owned PostgreSQL immediate-restart/concurrent-pickup/exact37%-condition/eligibility/capacity rollback passes in ignored `Artifacts/LootWorkflow/durable-20261025-04`; Unreal reports are alongside it. API/host build clean. Independent code review and two-client full workflow remain. Frozen source/UE import evidence is unchanged.

## Blocked on

None for implementation. Serialize Unreal builds with controller/NPC worker. Missing economic/reach/expiry decisions must be recorded with an explicit proposed current default, never invented silently.

## Next step

NPC worker integrates the agreed hook after this API commit. Add/run the precise two-client source/UI/drag/drop/pickup test, capture actual workflow state, resolve independent code-review findings, and push named work after fetch/rebase. No further loot appearance review.

## Outcome

In progress.
