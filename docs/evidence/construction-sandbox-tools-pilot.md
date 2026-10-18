# Neighbor meshes and standalone placement checkpoint

SBX-03B and the bounded standalone SBX-05 checkpoint were executed on Windows UE5.8.3. This is still an incomplete construction sandbox: support, doors, saves, line brushes, timber appearance, exact partial-solid clearance, packaged and two-client acceptance remain open.

## Geometry and activation

`BuildExposed` partitions each canonical boundary face against the six revisioned neighboring solids. Unknown borders reject; known empty borders restore the surviving face after removal. It emits convex outward patches; Unreal triangles reverse canonical winding. Collision retains the entire canonical convex solid even when all render triangles are culled. Canonical gameplay queries explicitly request **simple** collision; default Unreal world queries request complex collision and are unsuitable for this adapter.

The authority state prepares all touched/member-neighbor projections, preflights every activation, then publishes synchronously without callbacks or overlap events. Failed staging discards all candidates and preserves the assembly revision, members and active projections. Each accepted edit advances the assembly revision; a replay is stale. There is one member per address, with no silent replacement or complementary in-cell union.

Focused execution: `ShatteredFrontierEditor Win64 Development` builds successfully; eight `ShatteredFrontier.Construction.` editor automation tests succeed, zero failed/not-run. Tests include the existing 72 canonical rotated convex queries; 31,104 emitted patch-pair cases; crossing partial faces, reciprocal runtime seams/removal, fully culled solids retaining collision; failed/moved/stale staging; settings bounds; place/remove/retry/reach/occupied/pawn overlap; multi-member rollback with no leaked actor; and injected normal-mode rejection.

The optional `-ConstructionSurfaceOracle=<new absolute CSV>` on the automation command exports boundary areas. `py -3 tools/Compare-ConstructionSurfaceOracle.py <csv> --report <new json>` independently compares all 31,104 unique pair/direction IDs against the Python rational face-intersection oracle, with **zero absolute area error** in this run. This checks boundary area, while the engine tests separately check winding, degeneracy and runtime triangles; it does not establish broader curved geometry or final art fit.

## Controls and authority

Only the opt-in sandbox GameMode selects the new controller. B toggles building; 1/2/3 select cube/slab/slope; R cycles the 24 allowed orientations; left/right mouse place/remove. The wireframe ghost and HUD show allowed/blocked state, reason, active shape, orientation, snap and unlimited development stock. Inherited movement/jump/E are retained. A hit on an editable partial face selects the adjacent addressed cell; empty space inside an occupied address cannot hold another member.

The transient state independently checks standalone mode, current controller/pawn ownership, revision, finite position, shape/rotation, fixed plot, startup settings, actor-origin reach, eye LOS and physical clearance. Release-only settings ship reach 8 m (bounds 1–8 m) and 512 members (bounds 1–4096), captured at state start; only reviewed release authors change them for the next sandbox start. No live console/config-service transport is implemented. The fixed plot is 20 m square and 10 m high; topology and grid remain release-bound.

Clearance currently uses conservative canonical AABBs with a 0.1 cm inset for touching faces. This may reject valid empty slope space and is **not** exact canonical character/prop clearance acceptance. It must be refined before the complete placement gate. Single clicks can repeat placements; a bounded line brush is not implemented. Support is explicitly unvalidated, so this checkpoint allows floating pieces and cannot satisfy shelter/support acceptance.

## Evidence and limits

Raw ignored evidence is `Artifacts/ConstructionSandbox/tools-01/`; final reports are `Automation-recheck/index.json`, `Automation-recheck.log`, `Build-recheck.log` and `surface-oracle-recheck-results.json`. The earlier run correctly records six passes/two failures: its neighbor test used default complex traces and its synthetic controller had no local-player admission designation. Tests were corrected to explicit simple queries and local designation before BeginPlay; runtime guards were preserved. The initial naming compiler error was also corrected before the successful build.

A finite separate `-game -NullRHI` launch of `/Game/Frontier/Maps/Greenlands_ConstructionSandbox` logged `CONSTRUCTION_TOOLS_READY Standalone=1 PlacementOnly=1 UnlimitedDevelopmentStock=1 Support=0 Save=0 Network=0`, plus three reference solids. The dedicated map's stale geometry-only label was updated to the generic development label, matching its new-map generator. Normal E1 map/default configuration and the LAN service were preserved. All owned editor processes exit after these finite checks.

Two Sol/medium agents authored the engine geometry and controls separately; parent integrated and ran builds/commandlets/oracle; separate Sol/medium source/evidence review checked the bounded result. Luna/medium extracted the next support packet. No rendered/manual input exercise, packaging, network, support/collapse, functional clearance, persistent economy or final-mesh acceptance is established by this report. See [milestone](../construction-sandbox-milestone.md) for the remaining gates and [hash/result manifest](construction-sandbox-tools-results.json) for exact source/build/map identity.
