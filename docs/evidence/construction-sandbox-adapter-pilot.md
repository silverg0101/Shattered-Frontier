# Sandbox contact compatibility and Unreal adapter pilot

2026-10-18. Bounded SBX-03/04 execution under the [playable sandbox milestone](../construction-sandbox-milestone.md) and [BLD-01](../decisions/BLD-01-voxel-geometry-interface.md). Runtime execution status is recorded below; this document does not certify the finished construction sandbox or full GEO-02/03 coverage.

## Numerical compatibility

`tools/construction_compatibility.py` consumes the existing canonical full-cell, half-slab and slope solids. It derives transformed face polygons and uses rational convex polygon clipping for positive-area contacts and each side's exposed area. `None` means authoritative empty neighbor, not an absent streamed snapshot. Invalid rotations, shape identity mismatch and non-face adjacency reject. Coordinates do not depend on render buckets.

Executed: `py -3 tests/test_construction_compatibility.py` — nine tests pass. Cases include full/full, half faces, upper/lower slabs, identical/complementary/crossing slope triangles (including exact 1/4 contact), six-direction reciprocity/conservation, zero-area contact, negative/32-cell boundaries and removal. The existing geometry file's six tests remain the selected regression check.

Independent reviewer additionally enumerated 31,104 pilot shape/rotation/direction pairs and checked reciprocal contact, exposed-side symmetry, conservation and nonnegative bounded areas; all passed. This independent enumeration was a read-only review script, not a committed additional test suite. Luna/medium authored; parent requested nontrivial crossing geometry and strict rotation validation; separate sol/medium review/recheck found no remaining material finding within the numerical scope.

Limits: one convex part per cell from three fixed pilot shapes. The output contains contact/exposed **areas**, not a general polygon-difference mesh. It does not implement runtime neighbor-face culling, support solving, curved/multi-part geometry, terrain or live gameplay. SBX-03's numerical-area checkpoint is complete; emitted exposed faces and runtime seam/culling acceptance remain open.

## Unreal adapter execution

Selected the installed UE5.8 Runtime `ProceduralMeshComponent` module after inspecting its local source: synchronous `UpdateCollision` calls physics creation/state recreation. Added the private module dependency and explicit project plugin. The new Construction classes do not change the existing E1 controller, defaults or map. `Generate-ConstructionGeometryHeader.py --check` verifies the generated C++ payloads, exact integer vertices/faces and 24 rotations match the canonical Python source; this is a fixed table, not arbitrary content loading.

The standalone-only preview requires the explicit sandbox GameMode, game thread, unit scale/identity actor rotation and strictly increasing nonzero generation. Permitted shape rotations are baked into the canonical geometry. It stages a hidden collision-disabled component, synchronously cooks convex collision from the same solid vertices, checks the resulting Chaos convex/body, then activates the new component before destroying the old. Rejected shape/rotation/generation leaves the active projection unchanged. Render triangles use the appropriate Unreal winding. There is no edit RPC, economy or network replication.

Executed the documented `ShatteredFrontierEditor Win64 Development` build: succeeded. The first build passed; after tightening the readiness marker and isolation assertion, the final incremental build passed again. Final evidence root: `Artifacts/ConstructionSandbox/adapter-02/`. [Committed results/hashes](construction-sandbox-adapter-results.json) bind 14 source/input files, the editor module and new map to this checkpoint.

Focused command (repository root; use a new report/log path on rerun):

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' "$PWD\GameClient\ShatteredFrontier.uproject" -unattended -NullRHI -nosound -nop4 -NoSplash '-ExecCmds=Automation RunTests ShatteredFrontier.Construction.Geometry.' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$PWD\Artifacts\ConstructionSandbox\adapter-02\Automation" "-abslog=$PWD\Artifacts\ConstructionSandbox\adapter-02\Automation.log"
```

Three tests succeeded, zero failed/not-run: `CanonicalPilot`, `ConvexPhysicsAndGeneration`, `NormalModeIsolation`. They check pinned identities, proper rotations/volume, actual simple collision queries through occupied and empty regions of all 72 shape/orientation variants, stale/invalid generation preservation, zero normal-mode auto-spawns and refusal to activate an injected preview in normal E1 mode. No simulated or mocked collision pass is substituted for those queries.

The owned Python commandlet created only `/Game/Frontier/Maps/Greenlands_ConstructionSandbox`; defaults/cook list and `Greenlands_E1.umap` have no diff. A separate finite `-game -NullRHI` launch of that map logged `NetMode=0` and `CONSTRUCTION_PILOT_READY LocalGeometryOnly=1 Shapes=3 Edits=0 Economy=0`, then quit. Running the generator again rejected the existing map (exit -1) and preserved its SHA-256. All editor test processes exited. The live LAN service was not touched.

Separate source review/recheck passed, including the readiness marker (emitted only after all three installs) and explicit zero-auto-spawn test. Stronger review was used for C++/collision/activation; the bounded compatibility author used Luna/medium.

Limits: editor Windows standalone geometry/collision checkpoint only. No rendered pixel review, actor/headroom/motion traversal, packaged/Linux/network acceptance, injected cooking failure/restart recovery, neighbor-clipped runtime faces or streamed-border activation is established. These are required later gates, not exceptions to BLD-01. Neither full GEO-02/03 nor playable sandbox acceptance is complete.

Next: SBX-03B must emit neighbor-aware exposed meshes and prove runtime partial-face seams/removal; SBX-05 adds a sandbox-only controller, ghost/palette and server-owned edit intent. Support, ordinary door, development save/load, art integration and packaging follow their milestone gates.
