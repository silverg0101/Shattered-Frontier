# Corpse and ground loot interaction

- **Area:** gameplay, backend, UI
- **Owner of next step:** Controller: review fixture camera-settling fix, then bounded validation
- **Updated:** 2026-10-25
- **Worktree:** C:/Projects/SF-lane-d

## Goal

Corpse click/action opens authorized loot and own inventory together; shift-click/drag transfers. Inventory drops retain WorldMesh; action picks ground loot directly into the bag. One local targeting highlight covers interactables. Layout/style remains in the existing Widget Blueprints.

## State

Reviewed combined main d9737a5e passed one actual grounded rendered loop in55s: lane-d `Artifacts/CombatPilot/network-20261025-152835-215/results.json`. Real boar/wolf sources, both existing windows, Shift/drag, positive pelt ground drop/model/rim, direct pickup without loot UI, target clear and final observer privacy barrier all passed. SQL alpha revision4 has2 hides+1 pelt, beta revision0 empty; exactly2 defeat receipts. This is shared native action dispatch, not hardware key input. Prior failed runs remain superseded diagnostic evidence.

Reviewed combined main9163eb2b passed one rendered native-cursor coverage run in90.4s: lane-d `Artifacts/CombatPilot/network-20261025-160137-352/results.json`. Real native mouse-left press/release opened the exact corpse and both windows; no direct-open fallback. Shift/drag, positive ground-model drop and CENTER pickup without loot UI, target clear, observer receipt/final privacy and exact durable bags/2 receipts passed. Three actual PNGs were saved and retained with logs/results under main ignored `Artifacts/LootWorkflow/native-click-coverage-9163eb2b/` (ungraded functional evidence).

Reviewed ground fix1715e683 now captures actual canonical/fallback mesh bounds before debit and persists pose/bounds. All8mesh/no-debit/oversized/off-centre dedicated→client and PostgreSQL restart checks passed. Combined fixture c4f4ba8f is published; its build PASS28.81s and GroundGeometryBeforeDebit regression PASS. This clears the identified constructor25cm structural defect; new combined live ground/F acceptance remains pending.

Exactly one combined rendered attempt `network-20261025-162330-344` stopped before the first corpse click: real boar defeat and outer-body target/rim/bounds passed, then Stage17 repeatedly reported `CursorTarget Selected=None Expected=SFLootSourceActor_0`. Exact cause is unclassified; no floor/range cause inferred. Exact owned driver was stopped and runner cleanup completed; UE slot released. No new transfer/drop/F/capture observations. Earlier passing loops remain separate evidence.

One diagnostic-only probe `network-20261025-163935-217` on rename4a852610 plus diagnostics/access fix0fe5d46c returned `Observed`, InputSent=0. Camera lag after fixture teleport demonstrably invalidated the one-time projection: mouse stayed607,393 while current source projection moved888,584→960,634 over1s; initial view moved about14m. Deprojection succeeded, hit had no blocker, and expected root distance131.1cm was within300cm reach. Focus/WindowActive were1; NativeForeground0 is recorded but no focus cause is needed to explain the measured drift. Combined rebuild PASS14.09s; no workflow acceptance claimed; UE released.

## Evidence

New combined `ground-native-combined-build-01.log` PASS28.81s and `ground-native-combined-geometry` PASS on c4f4ba8f. New full loop stopped at Stage17 before the first click. Earlier `cursor-combined-build-01.log` PASS30.92s, dedicated/coupling checks and90.4s native-cursor runtime PASS on9163eb2b stand;55s grounded loop, actual-map geometry, Panel/ExactActionTarget, release/cancel/bonus/privacy/service/durable checks remain applicable. Exact unique37%-condition/restart/concurrency/capacity and corrected ground outer geometry remain targeted evidence.

| Required behavior | Current evidence |
| --- | --- |
| Corpse click | Observed native mouse-left press/release through existing E1 click handler; exact corpse+two windows, no direct-open fallback. |
| Exact shared action opens corpse plus own bag | Observed in run03 for both real sources (native dispatch, not hardware key input). |
| Own bag with other authorized inventories | Existing loot/trade/market WBP coupling regression passed (UI stack only); service authorization remains with existing callers and was not replaced by this test. |
| Shift each actual row / native drag | Boar Shift and pelt native drag observed; rare bonus row drain targeted-test-only. |
| Deliberate world drop / cancellation safety | Previous drop/model/CENTER direct pickup without loot widget observed; corrected outer geometry targeted PASS; native F new combined runtime pending; cancel/rejected-UI negatives targeted-only. |
| Common target body/ground/harvest | Previous corpse outer geometry/rim and ground CENTER rim/target clear observed; corrected ground outer bounds targeted PASS; same-view selected/unselected pair not yet captured; harvest exactness targeted-only. |
| Privacy and economic invariants | End-to-end observer barrier and exact final durable bags passed; range/invalid/capacity/unique-condition/concurrency/retry remain targeted/durable evidence. No live unique gear transfer claim. |

## Blocked on

No setup blocker claimed. Finish reviewed two-client acceptance before deployment; matching current client/server packages and actual durable service-host rollout remain required. The unused installed vendor-plugin manifest is not a demonstrated packaging blocker. Controller owns deployment readiness through the documented steam-server route; preserve other containers and rollback.

## Next step

Published fixture-only follow-up c4f4ba8f (compile PASS, new runtime pending): ground cursor projects outside the open bag, validates the shipped IMC_Player F mapping, and sends native F press/release through the character/controller guard instead of direct dispatch; the controller attempt count and durable pickup must both pass. A local camera requests a fixed-view unselected/selected ground pair clear of legs and the bottom prompt. Corpse click and existing privacy/SQL barriers stay intact.

Minimal fixture-only fix prepared for review (not yet compiled/run): wait for two consecutive stable camera/projection samples before setting the real cursor, require two exact normal Refresh selections before native click, and fail within8s if settling/selection fails. No reproject/focus/direct-open fallback or product camera change. Controller owns validation authorization, matching release packages, actual durable hosts and rollback through the documented deployment target. Ground geometry fix remains reviewed/published/tested; no repeated asset audit. Source/body appearance, textures and lighting remain deferred; import629a0c84 stays mechanically accepted.

Prior9163 passing run also logged Stage17 None three times over30s before click; its pass stays separate evidence. New measured probe classifies the stale projection, rather than inferring a cause from those old logs. No full workflow retry occurred.

## Outcome

In progress: earlier cursor/UI/center pickup observed pass; corrected ground outer geometry targeted PASS; combined native cursor/F/capture runtime and deployment integration remain open.
