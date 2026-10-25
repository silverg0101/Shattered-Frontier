# Corpse and ground loot interaction

- **Area:** gameplay, backend, UI
- **Owner of next step:** independent recheck, then NPC hook and loot workflow worker
- **Updated:** 2026-10-25
- **Worktree:** C:/Projects/SF-lane-d

## Goal

Corpse click/action opens authorized loot and own inventory together; shift-click/drag transfers. Inventory drops retain WorldMesh; action picks ground loot directly into the bag. One local targeting highlight covers interactables. Layout/style remains in the existing Widget Blueprints.

## State

Independent recheck PASS cleared all three interaction findings; reviewed API33497dcc and fixesbb35b3d0 are pushed as enabling work. Fixture review found three bounded issues: cancellation could debit inventory, a bonus boar row could prevent progress, and observer success ended privacy monitoring early. Corrections use a positive viewport release target, drain all authorized boar rows, and scan every source through a driver-complete barrier (pickup also waits for observer ground-model receipt). Final correction build passes; WorldRelease and FixtureLifecycle regressions cover these paths. Combined two-client execution still waits for reviewed NPC hook integration and fixture quick recheck; no gameplay/appearance acceptance claimed.

Service/C++ consumers implemented; import dependency is main629a0c84, mechanically complete with appearance deferred. Rewards adds durable create/list/open/take/drop; existing ledger transactions freeze all recipients/rolls, preserve exact copies/condition and receipts, and fence direct-vs-corpse delivery. Source actors recover after restart; C++ routes existing inventory Widget Blueprints, click/F, shift-click/drag, WorldMesh drops and direct ground pickup. Shared local targeting component owns the rim. Combat worker still owns the final RewardKill hook into SFLootWorldSubsystem; this API checkpoint does not claim a tested full player workflow.

## Evidence

Editor build passes; ShatteredFrontier.Loot.Authority/UniqueRows and six Inventory.Panel tests pass. InventoryLedger.Tests --world-loot passes. Owned PostgreSQL immediate-restart/concurrent-pickup/exact37%-condition/eligibility/capacity rollback passes in ignored `Artifacts/LootWorkflow/durable-20261025-04`; Unreal reports are alongside it. API/host build clean. Independent code review and two-client full workflow remain. Frozen source/UE import evidence is unchanged.

## Blocked on

None for implementation. Serialize Unreal builds with controller/NPC worker. Missing economic/reach/expiry decisions must be recorded with an explicit proposed current default, never invented silently.

## Next step

NPC worker integrates the agreed hook after this API commit. Add/run the precise two-client source/UI/drag/drop/pickup test, capture actual workflow state, resolve independent code-review findings, and push named work after fetch/rebase. No further loot appearance review.

## Outcome

In progress.
