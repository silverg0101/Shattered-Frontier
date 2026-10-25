# Corpse and ground loot interaction

- **Area:** gameplay, backend, UI
- **Owner of next step:** independent runtime-fixture recheck, then loot workflow worker
- **Updated:** 2026-10-25
- **Worktree:** C:/Projects/SF-lane-d

## Goal

Corpse click/action opens authorized loot and own inventory together; shift-click/drag transfers. Inventory drops retain WorldMesh; action picks ground loot directly into the bag. One local targeting highlight covers interactables. Layout/style remains in the existing Widget Blueprints.

## State

Reviewed service/native API, interaction fixes, NPC hook2dd4605d and fixture/cancel/lifecycle fixes are published through main8bf651fc. Actual two-client run03 observed real boar/wolf defeat, committed sources, replicated outer hit bounds/shared rim, both existing inventory windows, native Shift withdrawal and native wolf-pelt drag. It stopped on a rejected drop preflight. Run04 stopped at an unclassified client selection gate; no cause is claimed for that stop.

Runtime-fixture correction: rendered clients replace NullRHI only for this UI scenario; NullRHI had produced two windows/items with zero Slate-ticked rows. The actual-map probe pins recorded drop originZ7303.356 versus floorZ389.105; both Visibility and WorldStatic hit the same floor. Correct grounding atZ478.105 passes the unchanged production drop ray, outer body collision,131.5cm reach and eye LOS. Both real NPCs/pawns now stage at that verified XY using their own capsule heights. Production authority/LOS/floor policy is unchanged. This new checkpoint requires independent recheck before another full run.

## Evidence

Final build `combined-build-07.log` and `recorded-map-geometry-03` PASS. Earlier focused Loot (including deliberate-release/cancel and bonus/privacy lifecycle), six Inventory.Panel, service unit/host and durable restart/concurrent-pickup/exact37%-condition/eligibility/capacity checks remain applicable. All evidence is ignored `Artifacts/LootWorkflow/` or lane-d `Artifacts/CombatPilot/network-20261025-*`; full workflow remains open.

| Required behavior | Current evidence |
| --- | --- |
| Corpse click | Implemented; actual cursor-click path unobserved; add first-boar click once fixture is stable. |
| Exact shared action opens corpse plus own bag | Observed in run03 for both real sources (native dispatch, not hardware key input). |
| Own bag with other authorized inventories | Trade/market implementation; general supported-target regression pending. |
| Shift each actual row / native drag | Boar Shift and pelt native drag observed; rare bonus row drain targeted-test-only. |
| Deliberate world drop / cancellation safety | Positive release request observed, drop rejected; cancel/rejected-UI negatives pass targeted tests. Ground source/pickup pending. |
| Common target body/ground/harvest | Body network geometry/rim observed; ground pending; exact harvest target/occluder targeted-test-only. |
| Privacy and economic invariants | End-to-end observer barrier pending; range/invalid/capacity/unique-condition/concurrency/retry remain targeted/durable evidence. No live unique gear transfer claim. |

## Blocked on

No setup blocker claimed. Finish reviewed two-client acceptance before deployment; matching current client/server packages and actual durable service-host rollout remain required. The unused installed vendor-plugin manifest is not a demonstrated packaging blocker. Controller owns deployment readiness through the documented steam-server route; preserve other containers and rollback.

## Next step

Independent recheck of runtime fixture/diagnostics, then bounded full two-client run after NPC slot release. Add actual corpse-click and supported external-inventory bag coupling coverage once stable. Commit/review genuine fixes before release. Source/body appearance, textures and lighting remain explicitly deferred; import629a0c84 stays mechanically accepted.

## Outcome

In progress.
