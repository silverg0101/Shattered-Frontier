# SBX-05C: finite standalone engine-input exercise

The authority/collision/support tests do not establish that the controller bindings, ghost selection and actual owning-player input path work together. Add one explicitly enabled finite development scenario on the existing opt-in sandbox map. This is automated engine input, not a physical keyboard, rendered/manual, packaged or network acceptance claim.

## Boundary

New `Tests/SFConstructionInputScenario` source and a bounded `tools/Test-ConstructionSandboxInput.ps1` runner. The explicit sandbox GameMode may call its startup hook, but only a dedicated command-line opt-in, standalone mode, development test build and actual local sandbox controller/pawn may run it. Normal E1 has no scenario hook. Shipping builds have no active driver. No arbitrary paths/mesh/authority in player intents, default maps, cook lists, external services, asset imports or environment changes.

Drive real bound input events through `APlayerController`/`PlayerInput` and normal subsequent input processing. Arrange a deterministic local view and wait for the ordinary ghost/controller ticks. Do not call `State::Apply`, private commit/selection callbacks or inject member state as a substitute for keys. Read-only state/projection queries may inspect results. Fixture camera/pawn positioning is setup, not a manual movement/camera acceptance claim.

## Finite evidence

Exercise build toggle (disabled clicks do not edit), shape selection, allowed rotation without stale sampled placement, normal place, an adjacent supported placement, removal preview/confirmation/cancel as applicable and support-loss/collapse using actual world time. Verify authoritative member counts/identity or canonical projection geometry after input; do not pass from input injection returning true alone. Emit unique step/result markers and one final success marker only after assertions. Record failure reasons and exit nonzero on timeout/assertion. No test inventory/salvage is granted.

The runner creates a new owned evidence child under ignored `Artifacts/ConstructionSandbox/`, starts a hidden finite `UnrealEditor-Cmd -game -NullRHI` process with explicit map/opt-in, captures logs/exit and cleans only its process. Bound execution time and reject existing evidence output. Require semantic success markers and observed states, not process exit alone. A finite normal E1 launch with the flag must not activate construction/scenario behavior. Preserve the live LAN service and concurrent model work.

Initial run uses NullRHI; offscreen/rendered inspection is a separate later gate. Parent serializes editor builds and checks source/build provenance. Separate independent reviewer checks authority isolation, input-path fidelity, failure/timeout cleanup and raw observed results before closure. Final controls/remapping remain governed by the UI proposal, not this fixture's temporary B/1–3/R/mouse/Enter/Esc inputs.
