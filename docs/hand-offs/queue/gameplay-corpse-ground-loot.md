# Corpse and ground loot interaction

- **Area:** gameplay, backend, UI
- **Owner of next step:** independent runtime-fixture recheck, then loot workflow worker
- **Updated:** 2026-10-25
- **Worktree:** C:/Projects/SF-lane-d

## Goal

Corpse click/action opens authorized loot and own inventory together; shift-click/drag transfers. Inventory drops retain WorldMesh; action picks ground loot directly into the bag. One local targeting highlight covers interactables. Layout/style remains in the existing Widget Blueprints.

## State

Reviewed combined main d9737a5e passed one actual grounded rendered loop in55s: lane-d `Artifacts/CombatPilot/network-20261025-152835-215/results.json`. Real boar/wolf sources, both existing windows, Shift/drag, positive pelt ground drop/model/rim, direct pickup without loot UI, target clear and final observer privacy barrier all passed. SQL alpha revision4 has2 hides+1 pelt, beta revision0 empty; exactly2 defeat receipts. This is shared native action dispatch, not hardware key input. Prior failed runs remain superseded diagnostic evidence.

Remaining coverage source adds first-corpse projected mouse press/release through native controller input with no direct-open fallback, and common own-bag coupling for admitted existing loot/trade/market windows. Incremental build, new WBP stack coupling regression with gathering disabled, all six Inventory.Panel checks and ExactActionTarget pass. Independent review and coverage run remain before the actual mouse route is accepted. Three optional bounded `Shot showui` requests are included; actual images remain unobserved. Geometry policy/verified grounding are unchanged.

## Evidence

Core build `grounded-combined-build-01.log`/55s runtime and `recorded-map-geometry-03` PASS. Coverage checkpoint `cursor-coupling-build-01.log` PASS24.98s; `external-coupling-01`, six `cursor-coupling-inventory` checks and `cursor-coupling-action` PASS. Earlier deliberate-release/cancel, bonus/privacy lifecycle, service/durable restart/concurrent-pickup/exact37%-condition/capacity checks remain applicable. Evidence is ignored `Artifacts/LootWorkflow/` or lane-d `Artifacts/CombatPilot/network-20261025-*`; full workflow remains open.

| Required behavior | Current evidence |
| --- | --- |
| Corpse click | New actual projected mouse-input path written; runtime pending. |
| Exact shared action opens corpse plus own bag | Observed in run03 for both real sources (native dispatch, not hardware key input). |
| Own bag with other authorized inventories | Existing loot/trade/market WBP coupling regression passed (UI stack only); service authorization remains with existing callers and was not replaced by this test. |
| Shift each actual row / native drag | Boar Shift and pelt native drag observed; rare bonus row drain targeted-test-only. |
| Deliberate world drop / cancellation safety | Drop/model/direct pickup without loot widget observed; cancel/rejected-UI negatives targeted-only. |
| Common target body/ground/harvest | Body+ground network bounds/rim/target clear observed; exact harvest target/occluder targeted-test-only. |
| Privacy and economic invariants | End-to-end observer barrier and exact final durable bags passed; range/invalid/capacity/unique-condition/concurrency/retry remain targeted/durable evidence. No live unique gear transfer claim. |

## Blocked on

No setup blocker claimed. Finish reviewed two-client acceptance before deployment; matching current client/server packages and actual durable service-host rollout remain required. The unused installed vendor-plugin manifest is not a demonstrated packaging blocker. Controller owns deployment readiness through the documented steam-server route; preserve other containers and rollback.

## Next step

Independent review of immutable cursor/coupling checkpoint, fetch/rebase reviewed NPC integration, then bounded rendered two-client coverage run after slot release. Verify actual left-click route and retain only screenshots actually written; artifacts do not gate functional success. Commit/review genuine fixes before release. Source/body appearance, textures and lighting remain explicitly deferred; import629a0c84 stays mechanically accepted.

## Outcome

In progress.
