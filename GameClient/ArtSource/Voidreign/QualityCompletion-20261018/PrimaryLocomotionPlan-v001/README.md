# Voidreign primary locomotion and attachment plan

Next production dependency: establish actual body/leg animation before further hair tuning. Current source is the corrected PrimaryOnlyCombinedFull-v001 prototype, with failed Tail maximum-step gate retained. PrimaryMotionDelivery-v001 indexes the verified1571file baseline kit. It contains rigid horse response tests, not walking/running animation.

## Finite sequence

1. VR-PRIMARY-01: inspect source geometry and propose body, four legs/hooves, pelvis/spine/neck/head and fixed tack domains plus anatomical landmarks. Preserve base source; distinguish fused anatomy, painted detail and hanging geometry. Review joints/stance and raised forefeet; do not force the global minimum to ground.
2. VR-RIG-01: create a primary skeleton/skin prototype on an immutable candidate, with explicit contact/IK controls and rider seat reference. Inspect a small set of leg flexion/extension and spine/neck poses first. No final hair simulation until owning anatomy/root adapters follow that rig.
3. VR-IDLE-01, VR-WALK-01, VR-RUN-01, VR-TURN-01: create or retarget real clips. Record provenance and clip conventions. Validate planted-foot motion, ground clearance, joint deformation, stride phase, root displacement, rider placement and loop seams. Then check starts/stops and idle/walk/run/turn transitions. Thresholds, units and reference contacts must be declared before automatic pass/fail; no invented universal defaults.
4. VR-ATTACH-01: source-hashed complete inventory of leg feathering, straps/chains/reins, skulls/ornaments and other loose geometry. Review attachment ownership and detached pieces; choose bone chains, constrained rigid articulation, flexible surface simulation or static retention per item. Preserve original material/UV correspondence.
5. VR-SECONDARY-01: replace the current rigid-harness-only input adapter with validated primary-skeleton root targets and deforming colliders. Revalidate hair/tack against real gait extremes, transitions, rider and environmental contacts. Reuse fixed-rest lesson; do not feed secondary motion back into primary/rest geometry.

Historical hair budgets3of3 and failures are unchanged. New anatomical rig work needs its own declared stage/profile and finite budget, not another renamed hair attempt. Keep CPU-only, preserve all unsaved Blender sessions and concurrent scenery files. No engine/server/dependency changes or commit/push. A Blender locomotion prototype is not engine/runtime acceptance.

Exact next item: VR-PRIMARY-01. Inventory/profile/pose proposals must be reviewable before skinning or full-clip bakes. Current baseline kit and failed-gate evidence remain immutable.


## First primary pilot completed

PrimaryRigPilot-v001 now supplies23primary bones and a60frame actual-mesh joint exercise, with old secondary/harness modifiers disabled. Five checked poses have0coincident seam separation and rest returns within0.000638mm. Source geometry/UV/material retention passes; new weights/binds intentionally differ.1139nonhair vertices stay unresolved/unweighted. The finite budget is1of3for this new primary skeleton work, without resetting historical hair counts. Current exact next is VR-RIG-01 standing-contact/foot-control preparation; VR-PRIMARY-01 proposals were reviewed only for this pilot, not final semantic approval.


## Standing controls and remaining skin gate

PrimaryStandingPilot-v001 adds fixed-length foot controls; all soles reachground and the checked planted hold has0slip. Whole-mount hair/nonhair ground penetration and torso/leg skin strain remain failures. PrimaryStandingSkinRepair-v001 improves one103mm edge error to1mm, but a second71mm edge error exposes a same-owner binary root-volume eligibility boundary omitted from the repair band. Its solver also did not meet recorded convergence tolerance. PrimarySkinBoundaryReview-v001 pins exact causes and design followup. Primaryrig budget remains3of3/needs-review, not reset. Review that code/coverage before more primary candidates or gait acceptance. Independently, LooseItemInventory-v001 indexes23unresolved disconnected components/subsets; isolation/ownership review can proceed.

Gait references for the next clip design: University of Kentucky author Ashley Griffin, [Horse Movement and Way of Going](https://horses.extension.org/horse-movement-and-way-of-going/) and [Horse Natural Gaits](https://horses.extension.org/horse-natural-gaits/). Reference gait definitions do not supply Voidreign joint poses, stride dimensions or acceptance thresholds.
