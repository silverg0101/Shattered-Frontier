# Corpse and ground loot interaction

- **Area:** gameplay, backend, UI
- **Owner of next step:** NPC ground-geometry fix, then controller release integration
- **Updated:** 2026-10-25
- **Worktree:** C:/Projects/SF-lane-d

## Goal

Corpse click/action opens authorized loot and own inventory together; shift-click/drag transfers. Inventory drops retain WorldMesh; action picks ground loot directly into the bag. One local targeting highlight covers interactables. Layout/style remains in the existing Widget Blueprints.

## State

Reviewed combined main d9737a5e passed one actual grounded rendered loop in55s: lane-d `Artifacts/CombatPilot/network-20261025-152835-215/results.json`. Real boar/wolf sources, both existing windows, Shift/drag, positive pelt ground drop/model/rim, direct pickup without loot UI, target clear and final observer privacy barrier all passed. SQL alpha revision4 has2 hides+1 pelt, beta revision0 empty; exactly2 defeat receipts. This is shared native action dispatch, not hardware key input. Prior failed runs remain superseded diagnostic evidence.

Reviewed combined main9163eb2b passed one rendered native-cursor coverage run in90.4s: lane-d `Artifacts/CombatPilot/network-20261025-160137-352/results.json`. Real native mouse-left press/release opened the exact corpse and both windows; no direct-open fallback. Shift/drag, positive ground-model drop and CENTER pickup without loot UI, target clear, observer receipt/final privacy and exact durable bags/2 receipts passed. Three actual PNGs were saved and retained with logs/results under main ignored `Artifacts/LootWorkflow/native-click-coverage-9163eb2b/` (ungraded functional evidence).

Known separate integration defect remains: dedicated ground sources lack persisted item mesh bounds and retain constructor25cm collision. Center pickup passing does not prove visible outer-ground hit geometry. NPC worker owns authoritative item-bounds snapshot/persistence, oversized/off-centre and restart regressions before generic rename. This blocks closing the full task; corpse metadata/geometry and UI/cursor success stand.

## Evidence

New combined `cursor-combined-build-01.log` PASS30.92s, `cursor-combined-dedicated`/`cursor-combined-coupling` PASS,90.4s native-cursor runtime PASS on9163eb2b. Earlier55s grounded runtime, actual-map geometry, six Inventory.Panel/ExactActionTarget and deliberate-release/cancel/bonus/privacy/service/durable checks remain applicable. Exact unique37%-condition/restart/concurrency/capacity remain targeted evidence. Ground outer geometry on a dedicated authority remains unaccepted until its fix.

| Required behavior | Current evidence |
| --- | --- |
| Corpse click | Observed native mouse-left press/release through existing E1 click handler; exact corpse+two windows, no direct-open fallback. |
| Exact shared action opens corpse plus own bag | Observed in run03 for both real sources (native dispatch, not hardware key input). |
| Own bag with other authorized inventories | Existing loot/trade/market WBP coupling regression passed (UI stack only); service authorization remains with existing callers and was not replaced by this test. |
| Shift each actual row / native drag | Boar Shift and pelt native drag observed; rare bonus row drain targeted-test-only. |
| Deliberate world drop / cancellation safety | Drop/model/CENTER direct pickup without loot widget observed; ground outer geometry gap open; cancel/rejected-UI negatives targeted-only. |
| Common target body/ground/harvest | Corpse outer geometry/rim and ground CENTER rim/target clear observed; ground outer bounds gap open; harvest exactness targeted-only. |
| Privacy and economic invariants | End-to-end observer barrier and exact final durable bags passed; range/invalid/capacity/unique-condition/concurrency/retry remain targeted/durable evidence. No live unique gear transfer claim. |

## Blocked on

No setup blocker claimed. Finish reviewed two-client acceptance before deployment; matching current client/server packages and actual durable service-host rollout remain required. The unused installed vendor-plugin manifest is not a demonstrated packaging blocker. Controller owns deployment readiness through the documented steam-server route; preserve other containers and rollback.

## Next step

Prepared fixture-only follow-up (compile/runtime pending): ground cursor projects outside the open bag, validates the shipped IMC_Player F mapping, and sends native F press/release through the character/controller guard instead of direct dispatch; the controller attempt count and durable pickup must both pass. A local camera requests a fixed-view unselected/selected ground pair clear of legs and the bottom prompt. Corpse click and existing privacy/SQL barriers stay intact. No pre-fix runtime run.

NPC worker fixes dedicated ground geometry with authoritative pre-debit bounds, oversized/off-centre traces and durable restart tests; fresh review/integration required. After its explicit Unreal-slot release, the reviewed combined base needs one incremental build/focused gate and one rendered integration run including this input/capture coverage. Controller then owns matching release packages, actual durable hosts and rollback through the documented deployment target. Source/body appearance, textures and lighting remain deferred; import629a0c84 stays mechanically accepted.

## Outcome

In progress: cursor/UI/center pickup observed pass; dedicated ground outer geometry and deployment integration remain open.
