# Cursor palette and doorway playtest follow-up

UI-01A / SBX-06J, 2026-10-18. Standalone development implementation and automated acceptance passed; human comfort review remains open. [Source hashes and report provenance](construction-sandbox-cursor-door-results.json), [UI contract](../decisions/UI-01A-cursor-construction-prototype.md), [door contract](../decisions/SBX-06J-doorway-target-and-safe-close.md).

Free cursor selects placement and interaction; hold RMB for camera in either mode. LMB places in Build mode and removes in explicit Demolition mode. X/Delete or palette switches mode without editing. RMB never deletes. Native UMG presents selection, mode, surface/layer depth and feedback; C++ State retains authority. Camera/restart/focus handling releases capture safely. Click-time ray resampling retains the observed revision fence and line seed.

Door selection uses the doorway envelope with occlusion, reach and validated interaction points. Closing can displace the exact owned character capsule to a swept, checked side; if neither side is safe it preserves the open door and player pose. The transaction preflights projection/support, rolls back fallible movement, and publishes before deferred callbacks. This is an endpoint prototype, not animated hinge physics.

## Evidence

- `cursor-door-build-01/Build-final.log`: full focused ten-case report clean. Subsequent cursor/line resampling report has two clean cases; boundary report has three clean cursor/line/door cases. `Build-boundary.log` and final fixture-only `Build-fixture-settle.log` succeed. These are separate builds/reports; the initial full report's DLL hash was not captured.
- `cursor-line-input-02`: 21 actual engine-input semantic steps pass, including line revision fencing, movement, RMB read-only and mode switching.
- `cursor-door-input-03`: 19 steps pass, including grounded walking, 89.1 cm safe close displacement, reopen without displacement, trapped-close denial and exactly-once E/probe dispatch.
- `cursor-structural-input-02`: 16 steps pass, including stale previews, supported stacking, deliberate demolition, confirmation cancellation and world-time collapse.
- `cursor-save-restart-02`: sequential writer/reader pass with three cells and one open door, generation 1/source revision 5, matching content digest and native collision. Copied project/module/Saved paths remain isolated; ordinary save root is unchanged.

All four finite runners pass normal-mode rejection and owned cleanup. Tests use synthetic view-derived rays and real engine input without moving the desktop pointer. Default maps/configuration match the previous evidence hashes. Line02 used the resample binary, Door03 the boundary binary, and final structural/save runs the fixture-only rebuild; the JSON retains individual provenance rather than implying one common binary.

## Corrections and remaining gates

Earlier failed reports are retained under `Artifacts/ConstructionSandbox`: restart clearing controller ignore flags required reapplying owned look suppression; a door test's initial capsule overlapped reserved approach space by 9 cm and was moved without relaxing authority; computed oblique envelope entry needed a boundary clamp while supplied points remain strictly checked; Door02's runner expected revision 2 instead of 3 after replacement. Structural01 and Save01 clicked before their intended aim had settled; bounded four-frame, 0.25-second, under-0.2-cm ray checks now precede those fixture actions, with a three-second failure deadline and trace diagnostics. No production authority guard was weakened for fixture convergence.

Human mouse/Slate interaction and visual comfort, approved art, packaged-client and two-client acceptance remain open. Preserve concurrent art work, existing copied-preview saves, E1 defaults/cook list and live LAN service. Next: user comfort check on the updated isolated preview, then the scoped packaged-client acceptance route; do not import unapproved models.

Independent Sol review cleared production, fixture diffs, raw reports and source/report/config/map/module hashes. Updated visible eight-file isolated preview cursor-preview-20261018-191656 (PID31112 at launch) reached renderer readiness; no human comfort result inferred.
