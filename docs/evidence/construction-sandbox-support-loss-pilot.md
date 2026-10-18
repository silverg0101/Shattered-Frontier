# Standalone support-loss checkpoint

SBX-06B implements [the bounded runtime packet](../decisions/SBX-06B-support-loss-pilot.md) on the existing three-shape, rough-timber, synthetic-foundation sandbox. No paid inventory/salvage, real terrain/damage, save, network, packaged or manual-input acceptance is implied.

## Delivered behavior

RMB samples the target and obtains an authority-validated removal preview. If additional members will lose support, Enter submits the recorded cell/revision confirmation; Esc, selection/rotation changes or build toggle cancel it. Authority binds the preview to controller/cell/revision and rechecks it. Forged, cancelled, stale and replayed confirmations cannot apply. A changed revision never silently refreshes an old confirmation.

Each newly unsupported member receives its own authoritative world-time deadline: shipped grace 10 s, typed release-only 1–60 s bounds, reviewed release authors, next state-start application. Existing episodes retain their deadline through previews/unrelated edits/retries. Supported new columns can be built in multiple steps during recovery; a new floating member or newly destabilized previously stable member cannot use that exemption. Recovery cancels its episode. At or after expiry, user edits/previews fence until due processing resolves, closing controller/state tick-order races.

Due waves recompute support, remove only due still-unsupported members, stage all affected neighbor projections and the copied lifecycle, preflight, then publish one revision. Independent supported sections survive; later deadlines keep their time through earlier waves. Failed staging preserves members, collision, revision and expired deadlines. Retry interval is declared startup configuration: 0.25 s default, 0.05–5 s bounds; it does not extend grace. Warnings deduplicate by revision/reason. Missing foundation/projection proof fences edits and collapse.

The pure helper validates finite nondecreasing clock observations across both refresh and due queries, preserves state/output on invalid input and retains due entries until a successful reconciliation. Runtime uses world time; a private editor-test friend supplies deterministic time only for automation. There is no client clock, RPC, console override or shipping test hook.

HUD text distinguishes stable/strained/unstable, pending confirmation and countdown; the due-wave fence explains blocked edits, while retry failures are logged. Removed projections create no persistent rubble collision or economic salvage grants. The 25% production collapse entitlement remains a later contribution-ledger integration. Full limiting-contact highlighting and final construction UI remain open. The [controls/UI proposal](../construction-controls-ui-proposal.md) is separately reviewed planning, with temporary current keys.

## Execution and independent review

Raw ignored root: `Artifacts/ConstructionSandbox/support-loss-01/`. `Build.log` succeeds. `Automation/index.json` records **15 successful named tests: 14 clean plus one succeeded-with-warnings**, zero failed/not-run. The warning is the intentional `CONSTRUCTION_COLLAPSE_RETRY` caused by the invalid-neighbor-transform rollback probe, not an unexpected runtime error.

Cases include cancellation/forged/stale/replay confirmation, exact expiry fencing, original deadline preservation, two-step recovery, no floating additions, recovery/new episode, separate 10/12 s waves, independent survivor, missing foundation, bounded retry and failed-stage preservation. New pure-helper cases cover just-before/exact deadline, observed-time reversal, nonfinite time, 4096-member bound, deterministic due ordering and nonconsuming retry.

After that run, only `SFConstructionSupportLossTests.cpp` gained real simple-collision probes. `Build-final.log` succeeds; the precise `ShatteredFrontier.Construction.SupportLoss.AuthorityLifecycle` rerun in `Automation-physics/index.json` succeeds with the same intentional warning. It verifies the due cube still blocks after staging failure, has no stale blocking body after successful collapse, and the surviving half-slab's rebuilt collision still blocks. Production source is unchanged between those two runs; the final targeted test strengthens the earlier suite's geometry-state assertions.

Luna/medium authored the bounded lifecycle helper; Sol/medium authored authority/controller/settings/tests; parent corrected the shared observed-clock invariant, reviewed the Luna/low UI proposal, integrated tracking and ran serialized checks. Separate Sol/medium reviewed source and raw evidence, including deadline and retry corrections. [Final hashes/results](construction-sandbox-support-loss-results.json) bind source/DLL and preserved maps/defaults. The fixture map is unchanged this cycle. No editor processes remain after finite checks.

Next: automated engine-input exercise before claiming the bindings work end-to-end; ordinary door geometry/motion/functional-weight and 1 m approach interface; separate versioned development save/reload; then art, packaged and matching two-client acceptance. Existing E1 defaults/live service and other-session art remain preserved.
