# Corpse and ground loot interaction

- **Area:** gameplay, backend, UI
- **Owner of next step:** Controller: fresh functional rim evidence review and coordinated release
- **Updated:** 2026-10-25
- **Worktree:** C:/Projects/SF-lane-d

## Goal

Corpse click/action opens authorized loot and own inventory together; Shift-click/drag transfers. Inventory drops retain WorldMesh; action picks ground loot directly into the bag. One local targeting highlight covers interactables. Logic stays in C++; layout/style stays in the existing Widget Blueprints.

## State

Reviewed combined main **76d9fa3e** passed one actual rendered two-client loop in **51.8s**: lane-d `Artifacts/CombatPilot/network-20261025-164416-630/results.json`. Actual authored boar/wolf deaths entered durable loot through the canonical RewardKill hook. Stable fresh cursor projection and native mouse-left press/release opened the exact corpse plus own inventory. Native Shift transferred2 hides; native drag transferred the pelt, then deliberate viewport release created its ground model. The shipped F mapping, actual projected cursor outside the open bag, and native F press/release reached the existing character/controller guard exactly once and picked the ground item directly into the bag without loot UI. Target cleared.

Observer received the ground model before pickup, then completed the full-source privacy/end barrier: PersonalSources=0, GroundMesh=1, DriverComplete=1. Final SQL: alpha revision4 has item/boar_hide=2 and item/wolf_pelt=1; beta revision0 empty; exactly2 canonical defeat receipts. No direct-open or ground-dispatch fallback.

All4 actual PNGs, hashes and logs/results are retained under main ignored `Artifacts/LootWorkflow/native-ground-76d9fa3e/`: corpse-and-bag00000.png, fixed-camera ground-unselected00000.png / ground-selected00000.png, and final-own-bag00000.png. Fresh functional rim review is pending; this is no object/texture/lighting appearance gate.

Reviewed ground geometry1715e683 freezes actual canonical/fallback bounds before debit and persists full pose/bounds. All8mesh/fallback/no-debit/oversized/off-centre dedicated-to-client outer trace and PostgreSQL restart checks passed. Live drop/F acceptance now uses this descriptor; the oversized/off-centre outer trace remains targeted evidence.

The earlier c4f4 loop stopped at stale first-corpse cursor coordinates. One diagnostic-only probe proved camera lag after fixture teleport moved the projection while the cursor stayed fixed; no obstruction/range cause. Reviewed fixture fix waits for2 stable camera/projection samples, freshly aims, requires2 exact normal Refresh selections and fails within8s. Its subsequent actual native click passed. Prior9163/d973 passes and frozen diagnostic evidence remain in the worker report.

## Evidence

Combined `stable-cursor-combined-build-01.log` PASS14.44s; full rendered runtime PASS51.8s on76d9fa3e. `ground-native-combined-geometry` PASS; reviewed NPC rename includes22 affected runtime checks and serialized-map/source-compatibility proof. Earlier Panel/ExternalCoupling/ExactActionTarget, release/cancel/bonus/privacy, ledger and PostgreSQL durability checks retain unchanged inputs. No full suite was run.

| Required behavior | Current evidence |
| --- | --- |
| Corpse click + own/external windows | Observed native mouse-left press/release through existing E1 click handler; exact corpse+2 windows, no direct-open fallback. |
| Nearby common action opens corpse | Observed exact shared action dispatch for authored wolf; native F input/guard dispatch separately observed on ground pickup. |
| Own bag with other authorized inventories | Existing loot/trade/market WBP coupling regression passed (UI stack only); authorization remains existing callers, not replaced by the test. |
| Shift each actual row / native drag | Guaranteed boar row Shift and pelt native drag observed; valid rare bonus row drain targeted-test-only. |
| Deliberate drop / direct action pickup | Corrected ground model/descriptor and native F pickup with bag open, no loot widget, observed. Cancel/rejected-UI negatives targeted-only. |
| Common target body/ground/harvest | Exact corpse/ground selection, overlay and target clear observed; actual same-view PNG pair retained for independent functional rim review. Harvest exactness and oversized/off-centre outer geometry targeted-only. |
| Authority/privacy/economic invariants | End-to-end observer barrier and exact durable bags/2 receipts observed. Range/invalid/capacity/unique37%-condition/concurrency/retry/restart remain targeted/durable evidence; no live unique gear claim. |

## Next step

Owner explicitly approved “Fix and verify highlight,” extending the prior3-round limit by one functional verification round. Source-only shared compatibility fix prepared: custom-depth/stencil outline through the owning camera, cached/restored primitive depth state and existing FX lower bits, original overlays/base materials/Nanite untouched. Versioned typed render defaults and new owned postprocess generator are in `content/interaction/`; no item/species exceptions or WBP/layout changes. CPU schema checks pass; C++ build/material generation/compatibility automation and one unobstructed fourth-round pair remain pending source review and explicit UE-slot grant. Full native gameplay PASS76d9fa3e is reused because input/authority are unchanged. Package worker owns the current Unreal-heavy slot; no parallel Editor/build/capture.

Controller reviews retained functional rim pair, then owns matching client/server packages, actual durable service-host rollout and rollback through documented existing deployment targets. Preserve other game/Forge containers and old-state/reward-route compatibility; don't substitute the historical unauthenticated E1 fixture. No further UE run is authorized from this checkpoint; all owned processes cleaned up and UE slot released.

Round2 functional review passed the UI display states but could not discern the rim because the owning pawn's leg occluded the model. Reviewed capture-only fixture is published at ada40e17; its combined build PASS14.75s and one `-LootWorkflowCapturePair` gate returned `Observed` in53.3s (`network-20261025-165720-807`). Own-pawn/attached-actor meshes2 were hidden locally and restored2 with ExactFlags=1; PickupInputSent=0. Actual pair/hash manifest/logs/results are retained under main ignored `Artifacts/LootWorkflow/final-rim-pair-ada40e17/`. No new SQL/workflow acceptance claim.

Render diagnostics: actual SM_WolfPelt NaniteEnabled=1, base wolf_pelt_material_0 Blend=0; unselected Overlay=None, selected M_ActionTargetRim OverlayBlend=3. No shader/lighting/geometry/policy change and no self-grading. Fresh final third functional review decides visible-rim proof; if still unproved, hand to owner under the3-round limit. Native click/F/privacy/SQL PASS76d9fa3e stands. All owned UE processes cleaned and slot released; no further capture run authorized.

Source/body appearance, textures and lighting remain explicitly deferred; mechanical import629a0c84 stays accepted. The unused installed vendor-plugin manifest is not a demonstrated packaging blocker.

## Outcome

Integrated native click/F loot workflow observed PASS. Fresh functional rim evidence review and deployment remain with the controller; targeted-only limitations are explicit above.
