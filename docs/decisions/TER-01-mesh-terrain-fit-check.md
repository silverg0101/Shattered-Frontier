# TER-01 — Mesh Terrain fit check

Status: **adopted, 2026-10-22: dig anywhere, on the DEC-03 voxel adapter.** TER-01b closed both open gates (section 4). Decision note for the owner's terrain choice rule ([OWNER-2026-10-22](OWNER-2026-10-22-decisions.md), "Terrain choice rule"), which now applies as "adopt it, with the limits, and don't ask again". It does not change [DEC-03](DEC-03-terrain.md): voxel density stays the terrain authority.

## Outcome

- **Mesh Terrain: authoring and backdrop only.** It cannot be dug at runtime. Its runtime form is static Nanite meshes plus collision that are built in the editor or at cook. Every modifier, and the collision rebuild, sits in editor-only code.
- **Dig anywhere: adopted on the DEC-03 voxel adapter.** The spike's thin path from density chunks to mesh and collision now has a manifold mesher with distance LOD and meshing off the game thread (TER-01b). All six "clean" criteria pass, measured in `-game`:
  - **Tears:** 0 in every check: the 51-edit sequence on server and client and at LOD 1, LOD 2 and mixed LOD 0-2, two 500-edit zone soaks across LOD 0-4 transitions, and a 551-edit offline self-test (2,023 whole-mesh checks). The flatten-pad cliff is included.
  - **Whole-zone performance:** the 2.5 km Silverbrook zone costs +0.31 to +0.68 ms GPU in its geometry passes over today's Silverbrook ground, with the same material, at bookmarks 01, 02, 05 and A3 on the RTX 3060 at 1080p (ProfileGPU). The 1.0 ms allocation is met.
- **GFX-02 J1 starts on the voxel ground**, with the job changes below (applied in GFX-02). The digging limits in [TER-02](TER-02-digging-limits.md) apply.

"Clean" means all six of these hold. Status after TER-01b; the spike's status is in brackets.

| Criterion | Voxel adapter | Mesh Terrain |
|---|---|---|
| Runtime digs replicate | **Pass.** Server and client density hashes match after all 51 edits (spike: pass). | **Fail.** No runtime edit API. |
| Collision updates fast enough | **Pass.** Collision is cooked async, LOD-0 leaves only. Edit-to-visible p95 is 27 ms on the client and 31 ms at 200 edits/s in the zone (spike: 22–34 ms, synchronous). | Not applicable (static). |
| No seams or cracks at chunk edges | **Pass.** 0 open edges and 0 border mismatches in every check, LOD transitions included: whole-zone `SPIKE_SEAMS` over 2.1–3.4 M edges per view (spike: pass, no LOD). | Not tested. Users report Nanite holes. |
| Zero tears after dig, raise, flatten and seam-crossing edits | **Pass: 0.** (Spike: 1, at a non-manifold cell under the flatten-pad cliff.) | Not applicable. |
| GFX-02 look and anti-tiling kept | **Pass for the material.** World-aligned `MI_GN_Ground` with slope rock, unchanged. Landscape Grass Types are unavailable (J3 change). | **Fail as is.** `M_GN_Ground` renders as field stripes; it needs a channel-aware material. |
| Performance within GFX-02 budget over the region | **Pass.** Whole 2.5 km zone, distance LOD: +0.31 to +0.68 ms GPU over today's ground (spike: +3.9 ms for 384 m, no LOD). | 96 m: +0.93 ms GPU (Nanite). Not measured at region scale. |

## Evidence

Machine: i7-14700K. GPU numbers are from the **RTX 3060** (`-graphicsadapter=1`, the GFX-02 budget card). Runs that defaulted to the RX 7900 XTX are labelled. Engine: launcher UE 5.8.3 (CL 58210709). Evidence is in [`docs/evidence/terrain/2026-10-22-ter01/`](../evidence/terrain/2026-10-22-ter01/), with JSON and tagged log lines next to each capture. Logs are reduced to `*.tagged.txt`.

### 1. What Mesh Terrain is

- **Plugins** (`C:\Program Files\Epic Games\UE_5.8\Engine\Plugins\Experimental\`, mirrored in `C:\UnrealSource\UE_5.8.3\...`; all `IsExperimentalVersion: true`, `EnabledByDefault: false`):
  - `MeshTerrainMode`: one Editor module.
  - `MeshPartition`: Runtime modules `MeshPartition` and `MeshPartitionCompute`; Editor modules `MeshPartitionEditor`, `MeshPartitionModelingToolset` and `MeshPartitionEditorUI`.
  - `MeshPartitionWater`, `PCGMeshPartitionInterop`, `PCGPrimitives_MeshPartitionInterop`.
- **Epic's only shipped sample** is `L_MeshTerrain_Basic`, in `PCGPrimitives_MeshPartitionInterop`: PCG scattering on Mesh Terrain.
- **Release notes and roadmap:** Epic's 5.8 notes call it an Experimental 3D-mesh terrain for authoring. Their stated benefits are all about authoring: non-destructive modifiers, automatic regeneration, and World Partition streaming. Neither the notes nor the roadmap mention runtime deformation, and an Epic forum question asking about it is unanswered.
- **Known issues** (forum, 5.8.0–5.8.2):
  - crashes in standalone play (we reproduced this; see Hands-on);
  - Nanite holes and invisible sections;
  - imports above 8k fail;
  - editor memory blow-up at 16 km;
  - VSM lag.
- **Runtime form** (source):
  - `ACompiledSection` (`MeshPartitionCompiledSection.h:160-185`). Its comment says sections "are built during the cook process, producing assets (StaticMesh, Collisions, MetaData)". It has `IsRuntimeOnly`.
  - Rendering uses `UMeshPartitionStaticMeshComponent`s (Nanite) plus a far-field mesh.
  - `UMeshPartitionCollisionComponent::SetCollisionData`/`RebuildIfNeeded` are `WITH_EDITORONLY_DATA` (`MeshPartitionCollisionComponent.h:46-50`).
  - All modifiers are in `MeshPartitionEditor`: spline, boolean, noise, patch, sculpt layers and others.
  - The runtime module has no grass support (0 matches for "grass").
  - RVT is supported (`SetRuntimeVirtualTextures`).
- **Landscape is not runtime-editable either.** `LandscapeEdit.h` is `#if WITH_EDITOR`, and so is `ULandscapeHeightfieldCollisionComponent::UpdateHeightfieldRegion` (`LandscapeHeightfieldCollisionComponent.h:240-257`). This confirms the owner's question: the GFX-02 Landscape cannot be dug.

### 2. Hands-on: Mesh Terrain

Steps: the same base terrain (96 × 96 m, 0.5 m vertices) was imported through the engine's `FHeightmapImporter`, then built with `WorldPartitionMeshPartitionBuilder`, then cooked and staged for Win64.

- **Build:** 2.9–8.6 s for the 96 m area (3 variants). The default `Common_GameplayData` variant QEM-simplifies collision from 74,498 to **582 triangles**.
- **Uncooked `-game` and `-server` crash** while loading the map: `Assertion failed: ComponentNativeClass` at `WorldPartitionActorDesc.cpp:944`. The Mesh Partition actor carries an editor-module component. Runtime testing therefore needs a cook.
- **Cooked Win64:**
  - The map loads.
  - 2 compiled-section meshes, **both Nanite**, 30,416 fallback LOD0 triangles.
  - Collision is ready 0.99 s after the probe starts.
  - Collision vs the true surface: **p50 3.2 cm, p95 8.8 cm, max 16 cm** (the voxel adapter is 0.3 / 0.7 / 1.6 cm).
  - GPU on the RTX 3060: **+0.93 ms** (6.26 vs 5.33 ms) for the 96 m patch.
- **Runtime dig:** there is no callable path. The cooked actor holds cooked static meshes and a cooked body setup.
- **Look:** `MI_GN_Ground` renders as field-row stripes. Mesh Partition feeds its own vertex and channel data, which the material reads as the Field mask. Mesh Terrain would need a channel-aware material ([look sheet](../evidence/terrain/2026-10-22-ter01/look-compare-sheet.jpg)).
- **Not tested:**
  - Q1 (splines) and Q2 (PCG sampling): moot for digging. The spline modifiers and the PCG interop both exist in source.
  - Q4 (Linux server): the launcher engine cannot build Server targets ("Server targets are not currently supported from this engine distribution").

### 3. Hands-on: voxel adapter, DEC-03 geometry

What was built and run:
- **Adapter:** `ASFTerrainSpike` (development only). It uses 0.5 m Q15 density voxels in 16 m chunks of 32³, surface nets, and one `ProceduralMeshComponent` with synchronous collision per chunk. Its ops use DEC-03's encoding: millimetres, radius, kind.
- **Network test:** a dedicated `-server` with one `-game` client on loopback.
- **Scenario:** 51 edits plus 2 rejections, using dig, raise, flatten and 6 TER-02-legal edits:
  - 3 bowls 1 m deep, two of them crossing the x/y = 48 m chunk planes;
  - 2 flatten pads;
  - 1 gentle mound.

Results:

| Measure | Result |
|---|---|
| Base build, 108 chunks, 96.7k triangles | 294–412 ms (editor binaries), 237 ms (cooked) |
| Edit, server, synchronous (extract + mesh + collision) | p50 9.5–15 ms, **p95 22–34 ms**, max 25–60 ms (8 chunks); collision upload p95 10–17 ms |
| Replication | Op log (17 bytes of payload per op). Density hash equal on server and client after **51/51** edits. Loopback latency p50 19 ms, p95 37 ms (tear check off). |
| Tunnel and overhang | Roof hit from inside at 257.5 cm, floor at 42.5 cm, surface above at 540.8 cm (base 541.7): two surfaces with collision on one vertical |
| Bedrock floor | A 7-step shaft stops at −1,425 cm (floor −1,400 cm) |
| Region border and protected volume | Both rejected server-side before any change |
| Chunk seams | 0 open edges and 0 border mismatches, every edit (8-chunk corner included); voxel vs 1 m heightfield seam: max 0.81 cm |
| Collision vs surface | p50 0.3 cm, p95 0.7 cm, max 1.6 cm |
| Memory | 64 KiB of density per 16 m chunk (7.1 MB for 108); about 100 KB of CPU mesh per chunk |
| GPU, RTX 3060, 1080p | 96 m area: **+1.62 ms**. 384 m area (1,728 chunks, 1.51 M triangles, no LOD): **+3.89 ms GPU, +4.2 ms frame**, build 4.9 s. |

**Time to collision, and the fall-through the owner saw.**
- The cause was not collision timing. The Dev-map construction fixture (`SFConstructionNetwork.cpp:1207`) teleports every pawn to (−200, −400, 96), outside the spike terrain, so the pawn fell.
- The fix is in the spike's spawn flow:
  - terrain and collision are built in `PostInitializeComponents`, before world BeginPlay and before any pawn exists;
  - the server snaps a pawn with no ground under it onto installed terrain (`TERRAIN_SPIKE_PAWN_SNAPPED`).
- Measured:
  - voxel: collision ready at 1.5 s process uptime (cooked) and 6.4 s (editor `-server`); the pawn is first seen at 3.3 s and 29 s respectively;
  - Mesh Terrain (cooked): collision ready 0.99 s after the probe starts; the same fixture teleport was snapped.
- No `SPIKE_FELL_THROUGH` occurred after the fix. Every run has a hard timeout and quits on a fall.

**Tears** (the owner's light-blue triangles):
- `TearCheck` runs after every edit and logs `SPIKE_TEARS <chunk> <count>` for each touched chunk. It counts:
  - odd-use (open) edges inside the area;
  - border mismatches on chunk planes;
  - triangles that face the solid side (field gradient);
  - coincident-vertex triangles;
  - a 25 cm vertical and 50 cm side ray grid. A hit counts as a hole when the first surface a ray meets from clear air is back-facing, or when it crosses from air to solid without a hit.
- **Cause:** the surface-nets quad split. One orientation test per quad with a fixed diagonal leaves a back-facing triangle in folded quads at sharp cut rims. Coincident cell vertices made zero-area triangles and unpaired edges.
  - It is not a chunk-seam problem: border mismatch is 0 in every run.
  - It is not LOD: the spike has none.
  - It is not Nanite: the voxel path has no Nanite.
- **Evidence:** `-SFTerrainSpikeLegacyWinding` reproduces the old mesher: **1,056 tear detections over 133 chunk checks** (`legacy-winding/`).
- **Fix in the spike:**
  - choose the diagonal whose two triangles both face the air side;
  - orient each triangle by the trilinear field gradient at its centroid;
  - drop coincident-vertex triangles.
- **After the fix:** **1 detection in 253 chunk checks**, at a non-manifold cell where two sheets touch under a flatten-pad cliff (`SPIKE_TEAR_RAY` in `server.tagged.txt`).
- **Remaining fix:** a manifold mesher, either marching cubes with a consistent case table or Transvoxel, which TER-01b needs anyway for LOD.
- **Mesh Terrain:** no runtime edits, so nothing to tear at runtime. The cooked captures show no holes at 96 m.

**Look.**
- [Dig sequence](../evidence/terrain/2026-10-22-ter01/dig-sequence-sheet.jpg) and [look comparison](../evidence/terrain/2026-10-22-ter01/look-compare-sheet.jpg), both at 1920×1080 in `-game`.
- The world-aligned ground and the slope rock carry onto pit and tunnel walls.
- The flatten pad on a slope makes a cliff over 1 m, which TER-02 would clip. The bowls and the mound are TER-02-legal.
- No independent visual review was run; this is a measured spike, not a look sign-off.

## 4. TER-01b: manifold LOD mesher and zone budget (done, 2026-10-22)

The bounded exit gate: Gate 1, 0 tears in every test, the flatten-pad cliff included. Gate 2, the whole 2.5 km zone within the GFX-02 1.0 ms ground allocation at 1080p on the RTX 3060 (ProfileGPU), with meshing off the game thread. Both pass. The RTX 3060 is `-graphicsadapter=1`; the log shows "Chosen D3D12 Adapter Id = 1". Evidence is in [`docs/evidence/terrain/2026-10-22-ter01b/`](../evidence/terrain/2026-10-22-ter01b/): JSON plus `*.tagged.txt` per run.

**What changed.** The core is `SFVoxelCore.h`, pure C++. `tools/ter01/mesher_selftest.cpp` compiles it without the engine.
- **Mesher: face-loop marching cubes, no case tables.** It replaces surface nets.
  - Each cell face is marched once in a canonical orientation. The two crossings around each solid run are paired; saddle squares use the asymptotic decider. So the two cells sharing a face always draw the same segment, in opposite directions.
  - Each cell chains its segments into closed loops. A loop is triangulated by its best fan, or by a star from its centroid. A diagonal never runs along a cell face.
  - A crossing within 0.1% of a grid point snaps to that point.
  - Result: watertight, manifold and consistently oriented by construction.
- **Distance LOD:**
  - Leaves form a 2:1-balanced octree. Each leaf is 32³ cells of 0.5 m × 2^L (L 0-5).
  - A leaf splits while the camera or the pawn is closer than K × its size. In the zone K = 1, so cells are 12-24 px. Cells up to 4 m reproduce the 5 m base exactly.
  - Where a coarse leaf meets a finer one, the coarse cells' faces are subdivided and their edges split at the fine points (transition cells). Every shared point takes the finest leaf's value. Seams are crack-free across levels.
- **Store:** only edited 16 m chunks are kept (64 KiB each), shared copy-on-write with meshing tasks. The base is regenerated from the heightfield (`base_content_version`). The first edit of a chunk remeshes every leaf that reads it.
- **Threads:**
  - Edits write density on the game thread (p95 0.7 ms).
  - Meshing runs as a `UE::Tasks` batch with `ParallelFor`, and each batch is applied atomically.
  - Collision is cooked async, for LOD-0 leaves only; the dedicated server meshes LOD 0 only.
  - Leaf components have Static mobility, so there is no velocity pass.
- **Tear check:** `SFVox::CheckTears` is shared by `SPIKE_TEARS`, `SPIKE_SEAMS` and the self-test. It counts:
  - open edges, with border mismatches counted separately;
  - inconsistent orientation;
  - coincident or zero-area triangles;
  - back-facing triangles;
  - the spike's ray grid (25 cm vertical and 50 cm horizontal, both ways).
- **Back-facing, redefined:** the field is clearly solid in front of the triangle and clearly air behind it, tested at the triangle's own scale. The spike's sign-only gradient test also counts near-tangent triangles in saddle and cliff cells whose front and back are on the same side. Those are now logged as `gradient_disagree`, which is informational, like edges shared by more than two triangles. Rays and the orientation check still catch every back face visible from air. The spike's old mesher passed the sign test only by flipping triangles one by one, and that is what left its non-manifold tear.

**Gate 1: tears (all 0).**

| Run | Checks | Coverage |
|---|---|---|
| Offline self-test (`Run-MesherSelftest.ps1`): spike sequence plus 500 random digs, raises and flattens, incremental remesh | 2,023 whole-mesh checks, 3.18 M rays | Uniform LOD 0, 1, 2 and two mixed sets |
| `-server` plus `-game` client, 51 edits (`net/`) | 51 server + 51 client; whole-mesh seams 0 | LOD 0 |
| Standalone, 51 edits (`lod-tears/`) | 51 per set | LOD 1, LOD 2, mixed LOD 0-2 |
| Zone soak, 500 edits within 300 m (`zone-soak/`) | 498 checks, 4.5 M rays | L0 21, L1 79, L2 251, L3 277, L4 41; 169 across transitions |
| Zone soak, 500 edits within 60 m (`zone-soak-near/`) | 500 checks, 1.6 M rays | L0 399, L1 227, L2 10; 135 across transitions |
| Whole-zone `SPIKE_SEAMS` at each perf view | 0.61–0.89 M edges per view | L0-L5 |

**Gate 2: zone GPU.** RTX 3060, 1920×1080, `tools/ter01/Run-ZonePerf.ps1` (`zone-k1-static/`). Each value is the median of three ProfileGPU frames. "Ground" is the delta of the ground's geometry passes (prepass, base pass, shadow depths, velocity) between ground shown and hidden. "Today" swaps in today's Silverbrook ground mesh (`SM_Silverbrook_Terrain`), with the same `MI_GN_Ground`, view and frame.

| View | Voxel ground | Today's ground | Voxel − today | Stat GPU p50, voxel / today | Triangles |
|---|---|---|---|---|---|
| 01 forest path | 3.64 ms | 2.96 ms | **+0.68 ms** | 9.21 / 9.37 ms | 483 k |
| 02 main street | 3.11 ms | 2.69 ms | **+0.42 ms** | 8.82 / 9.11 ms | 591 k |
| 05 market square | 2.91 ms | 2.55 ms | **+0.36 ms** | 8.44 / 8.74 ms | 553 k |
| A3, 250 m | 2.62 ms | 2.31 ms | **+0.31 ms** | 8.56 / 9.23 ms | 406 k |
| Overview, 600 m | 3.03 ms | 3.32 ms | −0.29 ms | 8.65 / 9.20 ms | 409 k |

- **Why the gate is the increment.** GFX-02 §6 sets its 1.0 ms as an allocation on top of today's frame, and J1 gates its geometry at "within +1 ms of today". Today's ground already costs 2.3–3.3 ms with this material. Most of that is BasePass pixel shading (2.0–3.0 ms), and it barely moves when the triangle count halves (K 3 to 1.5: 2.27 M to 1.0 M triangles, BasePass 3.2 to 3.0 ms). Any ground with this material pays it, the planned Landscape included. An absolute 1.0 ms would therefore fail for every ground option, and does not measure the terrain representation.
- **Levers tried, in the evidence:**
  - K 3, Movable: 1.4–2.3 M triangles, 2.9–4.4 ms vs no ground.
  - K 1.5, Movable: +0.24 to +1.09 ms vs today, with a 0.16 ms velocity pass.
  - K 1.5, Static: +0.30 to +1.05 ms.
  - **K 1, Static: adopted.**
- **Frame time:** frame p95 is 0.1–1.3 ms above today's at the same views. This is CPU for the per-leaf draw path; J6 checks it.

**Kept from the spike** (`net/`, editor binaries):

| Measure | Result |
|---|---|
| Replication | Density hash equal on server and client after 51/51 edits; loopback latency p50 4.8 ms, p95 10.5 ms |
| Collision | Dig floor 240.5 cm; tunnel roof from inside 270 cm, floor 30 cm, surface above 541.7 cm (base 541.7); shaft stops at −1,425 cm (bedrock −1,400 cm) |
| Limits | Region border and protected volume both rejected server-side |
| Seams | 0 open edges (interior and the 8-chunk corner); voxel vs 1 m heightfield max 0.84 cm, p95 0.74 cm (159 samples) |
| Edit-to-visible | Client p95 26.6 ms. Editor `-server` p95 102 ms, with the tear check running between batches; its mesh task p95 is 54 ms |
| Capacity (`zone-burst/`) | 200 edits/s for 2 s in the zone: edit-to-visible p50 20 ms, p95 31 ms, max 42 ms; ground game-thread tick p95 1.5 ms; frame p95 13.2 ms, max 15.7 ms |
| Fall-through | None. Every run has a hard timeout and quits on `SPIKE_FELL_THROUGH`. One zone run fell before the fix; LOD focus now includes the pawn, so its LOD-0 collision stays under it. |

**Not done here (the next owners):**
- **Grass on the voxel surface** (TER-01b item 3): GFX-02 J3.
- **DEC-03 snapshot compaction:** the sparse store exists, but snapshots are a World-service job.
- **Look review:** no independent visual review was run. The captures in `net/` are a measured check, not a look sign-off.
- **Lumen:** `ProceduralMeshComponent` has no mesh distance field or cards, so software Lumen sees the ground only through screen traces. J5/J6 must check GI and reflections at 04; the fallback is hardware ray tracing or a coarse static proxy.
- **Cooked dedicated server:** re-measure edit-to-collision there.

## GFX-02 job changes (applied in GFX-02, 2026-10-22)

- **J1:** the zone ground is the voxel adapter, fed by the `Heightfield` sampled to 0.5 m as DEC-03's published base (`base_content_version`). There is no zone `ALandscape`; the C++ landscape helper is used only by J1b.
  - The alignment gate (≤ 2 cm vs `HF.at()`) is kept; the spike measured 0.7 cm p95.
  - The perf gate is the TER-01b measure above.
- **J1b:** unchanged. The outer region beyond the region border is not diggable, which is a valid limit. A Landscape or a real-DEM Mesh Terrain is used as backdrop only. The zone edge meets it with a skirt; the measured voxel-to-heightfield seam is ≤ 0.84 cm.
- **J2:**
  - Becomes a mesh material, not a `LandscapeLayerBlend`.
  - Layer weights come from the DEC-03 material palette per vertex, plus the world-space masks.
  - `MF_GN_HexTile` is unchanged, plus triplanar for steep and vertical faces (tunnel walls).
- **J3:** Landscape Grass Types are replaced by GPU-instanced grass on the voxel surface, regenerated per edited leaf. The density, cull and colour tables are kept.
- **J4:** the voxel components write the RVTs (`UPrimitiveComponent::RuntimeVirtualTextures`), and the affected pages are invalidated on each edit.
- **J5:** unchanged.
- **J6:** the Nanite-landscape trial is dropped for the zone and replaced by the voxel LOD check (`Run-ZonePerf.ps1`).

## Adaptive foundations (spec stub; not built: dig-anywhere was adopted)

- **Goal:** a building sits level on any slope up to 35° with no floating corners or buried walls, and no terrain edit.
- **Shape:** a foundation piece has a level top at the snap height and auto-generated legs, a plinth or a stepped skirt down to the ground.
- **Ground sampling:** the server samples the ground under the footprint's corners and edges on a 0.5 m grid, using line traces against the Landscape.
- **Limits:** depth per column is limited by typed configuration (`build.foundation_max_drop_m`: default 3, bounds 0.5–6). Columns deeper than that block placement, and the preview shows the hatch.
- **Material:** stone or timber legs from the piece's material, priced by the leg volume.
- **Support:** the support solver treats each leg's ground contact as an anchor (Construction §3.4).
- **Preview:** the existing build ghost plus the generated legs.
- **Tests:** level, 20°, 35° and over-limit slopes; legs never pierce the Landscape by more than 5 cm; server and client agree.

## Risks and mitigations

- **Experimental Mesh Terrain:** used only as a backdrop or authoring source, never as storage or runtime ground.
  - Pin UE 5.8.3.
  - Bake its surface into DEC-03 base density through a thin adapter (TER-01 Q5: the heightmap or mesh is sampled to 0.5 m in the editor), so storage never depends on its format.
  - Fallback if Epic changes it: the GFX-02 Landscape backdrop.
- **Voxel adapter maturity:**
  - The mesher, LOD and async meshing are now done and tested (section 4). Grass (J3), the palette material (J2) and RVT writes (J4) are still project code to write.
  - Uses only stable engine APIs: `ProceduralMeshComponent`, `UE::Tasks` and Chaos runtime cooking, all of which work in the cooked build.
  - Guard: `tools/ter01/Run-MesherSelftest.ps1` must report `tears=0` after any change to `SFVoxelCore.h`.
- **Draw calls:** about 170–500 leaf components are shown at once (spike: 1,728 without LOD). Frame p95 is 0.1–1.3 ms above today's ground; J6 checks it, and merging far leaves is the lever.
- **Lumen:** procedural leaves have no distance field or cards, so software Lumen sees the ground only in screen traces. J5/J6 checks GI and reflections; the fallback is hardware ray tracing or a coarse static proxy.
- **Dev-map fixture teleport:** `SFConstructionNetwork` moves pawns to (−200, −400, 96) on every Dev map. Terrain maps need ground there, or a snap rule like the spike's.
- **Engine side effect, now reverted.** The first Mesh Terrain import wrote the engine asset `Engine/Plugins/Experimental/MeshPartition/Content/DataAssets/MPD_Default.uasset` under Program Files. It was restored from `C:\UnrealSource\UE_5.8.3`. The restored hash is E5719C77…, matching the source copy. Verify the launcher install once ("Verify" in the Epic launcher). The map script now duplicates the definition under `/Game/Frontier` and saves only `/Game` packages.

## Plain-language summary for the owner

We tested Unreal's new Mesh Terrain properly: in the editor, in a packaged build, and against Epic's source code. It makes beautiful 3D ground with cliffs and caves, but **it can't be dug while the game runs**. Its shapes are baked when the game is built. It's good for far mountains, not for digging.

We also built our own diggable ground, the voxel ground the design already calls for. Digging, raising, flattening, tunnels and caves all work with two computers connected. The server and the player always end up with exactly the same ground, and there are no gaps between chunks. The photoreal ground texture works on it.

Two things weren't clean after the first test, and both are fixed now:
- **Tears:** none left. We rebuilt how the ground is cut into triangles, so it can't leave holes or inside-out pieces. Over 3,200 checks after digs, raises, flattens and cliffs found zero.
- **Far ground:** distant ground now uses fewer, bigger triangles. The whole 2.5 km zone costs about the same as today's ground: +0.3 to +0.7 ms at the town views, within the 1 ms allowed.

So, under your rule, **digging is adopted**. It works everywhere except bedrock, region borders, cities and safe areas, roads, spawns and other people's claims, with 7-day healing. The far mountains stay photoreal Landscape. Still to come in the ground work: grass that regrows on dug ground, and a look check of the new ground against the concept art.

## Spike files

- **Source:**
  - `GameClient/Source/ShatteredFrontier/Private/TerrainSpike/` (`SFTerrainSpike.*`, `SFTerrainSpikeProbe.cpp`, development only; `SFVoxelCore.h`: field, LOD octree, mesher and tear check, engine-free);
  - `GameClient/Plugins/TerrainSpikeMeshTerrain/` (editor-only importer; disabled by default, enable it in the uproject only for the spike).
- **Map:** `/Game/Frontier/Maps/Dev/TerrainSpike/TerrainSpike_Voxel`. The Mesh Terrain map is regenerated by script and not committed, because it needs the experimental plugin enabled.
- **Scripts** (`tools/ter01/`):
  - `Create-TerrainSpikeMap.py`;
  - `Run-TerrainSpikeNet.ps1` (`-ClientPy`; `-LegacyWinding` no longer reproduces the old mesher, whose record stays in `legacy-winding/`);
  - `client_checks.py`;
  - `Create-MeshTerrainMap.py`;
  - `Run-MeshTerrainSpike.ps1`;
  - TER-01b: `Export-ZoneHeightfield.py` (zone base, written under `GameClient/Saved/`), `Run-MesherSelftest.ps1` and `mesher_selftest.cpp`, `Run-LodTears.ps1`, `Run-ZonePerf.ps1` (`-Soak`, `-Extra` for `-SFTerrainSpikeBurst` and `-SFTerrainSpikeSoakRadius`), `zone_perf.py`, `zone_gpu_passes.py`.
- **Probe:** `SFTerrainSpike.Probe`, for cooked builds.
