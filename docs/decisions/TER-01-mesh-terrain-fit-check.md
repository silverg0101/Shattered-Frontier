# TER-01 — Mesh Terrain fit check

Status: **spike done, 2026-10-22.** Decision note for the owner's terrain choice rule ([OWNER-2026-10-22](OWNER-2026-10-22-decisions.md), "Terrain choice rule"). It does not change [DEC-03](DEC-03-terrain.md): voxel density stays the terrain authority.

## Outcome

- **Mesh Terrain: authoring and backdrop only.** It cannot be dug at runtime. Its runtime form is static Nanite meshes plus collision that are built in the editor or at cook. Every modifier, and the collision rebuild, sits in editor-only code.
- **Dig anywhere: adopt it on the DEC-03 voxel adapter, with two gates still open.** This is the thin path from density chunks to mesh and collision that the spike built. It already passes four of the six "clean" criteria, measured in `-game`: replication, collision time, chunk seams, and the GFX-02 material. It fails two today:
  - **Tears:** one non-manifold cell is left after the fix (down from 1,056 detections).
  - **Whole-zone performance:** there is no distance LOD yet. A 384 m × 384 m area already costs +3.9 ms GPU on the RTX 3060 at 1080p.

  Both failures have standard fixes, and neither needs dig zones. They form the exit gate of a bounded follow-up, **TER-01b** (below). **GFX-02 J1 waits on TER-01b.** If TER-01b misses its gate, the owner's rule applies with no further question: **no digging**, the GFX-02 Landscape as planned, and adaptive foundations (stub below).

"Clean" means all six of these hold. Status is from this spike.

| Criterion | Voxel adapter | Mesh Terrain |
|---|---|---|
| Runtime digs replicate | **Pass.** Server and client density hashes match after all 51 edits. | **Fail.** No runtime edit API. |
| Collision updates fast enough | **Pass.** Edit plus collision p95 is 22–34 ms, synchronous. | Not applicable (static). |
| No seams or cracks at chunk edges | **Pass.** 0 open edges and 0 border mismatches in 253 chunk checks, including the 8-chunk corner. | Not tested. Users report Nanite holes. |
| Zero tears after dig, raise, flatten and seam-crossing edits | **Fail (1).** One back-facing hit at one non-manifold cell; 1,056 before the fix. | Not applicable. |
| GFX-02 look and anti-tiling kept | **Pass for the material.** World-aligned `MI_GN_Ground` with slope rock renders unchanged. Landscape Grass Types are unavailable (J3 change). | **Fail as is.** `M_GN_Ground` renders as field stripes; it needs a channel-aware material. |
| Performance within GFX-02 budget over the region | **Fail today.** No LOD. | 96 m: +0.93 ms GPU (Nanite). Not measured at region scale. |

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

## TER-01b: exit gate before GFX-02 J1 (bounded)

Each item runs on the RTX 3060, in a 2.5 km zone made from the Silverbrook `Heightfield`, measured at bookmarks 01, 02 and 05.

1. **Manifold mesher with distance LOD:** Transvoxel or marching cubes with transition cells.
   - **Gate: 0 tears** over the same edit sequence, plus a 500-edit random soak, with TearCheck at each LOD.
2. **Zone render budget:**
   - Ground ≤ 1.0 ms GPU (the GFX-02 landscape allocation), with p95/median at GFX-02 §6.
   - Edit-to-visible ≤ 100 ms with meshing and collision cooking off the game thread.
   - 200 ops/s zone capacity without game-thread stalls.
3. **Grass on the voxel surface:** GPU-instanced (PCG runtime generation or HISM), regenerated per edited chunk, ≤ 3.5 ms.
4. **Server memory:** a sparse store holding only edited chunks, so the base is regenerated from `base_content_version` (DEC-03 snapshots).

If any gate fails after the bounded attempt, the outcome flips to **no digging**.

## GFX-02 job changes

**If dig-anywhere is adopted (TER-01b passes):**
- **J1:** the zone ground is the voxel adapter, fed by the `Heightfield` sampled to 0.5 m as DEC-03's published base (`base_content_version`). There is no zone `ALandscape`. The alignment gate (≤ 2 cm vs `HF.at()`) is kept; the spike measures 0.7 cm p95. The C++ landscape helper is used only by J1b.
- **J1b:** unchanged. The outer region beyond the region border is not diggable, which is a valid limit. A Landscape or a real-DEM Mesh Terrain is used as backdrop only. The zone edge meets it with a skirt; the measured voxel-to-heightfield seam is ≤ 0.81 cm.
- **J2:**
  - Becomes a mesh material, not a `LandscapeLayerBlend`.
  - Layer weights come from the DEC-03 material palette per vertex, plus the world-space masks.
  - `MF_GN_HexTile` is unchanged, plus triplanar for steep and vertical faces (tunnel walls).
- **J3:** Landscape Grass Types are replaced by GPU-instanced grass on the voxel surface, regenerated per edited chunk. The density, cull and colour tables are kept.
- **J4:** the voxel components write the RVTs (`UPrimitiveComponent::RuntimeVirtualTextures`), and the affected pages are invalidated on each edit.
- **J5:** unchanged.
- **J6:** the Nanite-landscape trial is dropped for the zone and replaced by the LOD budget check.

**If no digging:** J1–J6 are unchanged, and the adaptive-foundations job below is added.

## Adaptive foundations (spec stub; built only under "no digging")

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
  - The mesher, LOD, grass and async work are project code. That is about 2–4 engineer-weeks of risk before the look is back at GFX-02 parity. TER-01b bounds it.
  - Uses only stable engine APIs: `ProceduralMeshComponent` and Chaos runtime cooking, both of which work in the cooked build.
- **Draw calls:** one component per chunk costs CPU. The 384 m test spent +4.2 ms of frame time on 1,728 components. TER-01b must merge or LOD far chunks.
- **Dev-map fixture teleport:** `SFConstructionNetwork` moves pawns to (−200, −400, 96) on every Dev map. Terrain maps need ground there, or a snap rule like the spike's.
- **Engine side effect, now reverted.** The first Mesh Terrain import wrote the engine asset `Engine/Plugins/Experimental/MeshPartition/Content/DataAssets/MPD_Default.uasset` under Program Files. It was restored from `C:\UnrealSource\UE_5.8.3`. The restored hash is E5719C77…, matching the source copy. Verify the launcher install once ("Verify" in the Epic launcher). The map script now duplicates the definition under `/Game/Frontier` and saves only `/Game` packages.

## Plain-language summary for the owner

We tested Unreal's new Mesh Terrain properly: in the editor, in a packaged build, and against Epic's source code. It makes beautiful 3D ground with cliffs and caves, but **it can't be dug while the game runs**. Its shapes are baked when the game is built. It's good for far mountains, not for digging.

We also built our own diggable ground, the voxel ground the design already calls for. Digging, raising, flattening, tunnels and caves all work with two computers connected. The server and the player always end up with exactly the same ground, and there are no gaps between chunks. The photoreal ground texture works on it.

Two things aren't clean yet:
- One tiny tear is left after an edit. We found the cause and the standard fix.
- Distant ground has no level-of-detail yet, so a big area costs too much frame time.

Your options:
1. **Dig anywhere** (recommended). We finish those two fixes first, as a short bounded job, before the ground work starts. If they pass, digging works everywhere except bedrock, region borders, cities and safe areas, roads, spawns and other people's claims, with 7-day healing. The far mountains stay photoreal Landscape.
2. **No digging.** The photoreal Landscape goes ahead exactly as planned, and buildings get foundations that adapt to slopes. This is fastest and safest for the look. It is also what happens automatically if option 1's fixes don't pass.
3. **Mesh Terrain everywhere.** Not possible if you want digging: it can't change during play. We'd only use it for scenery.

## Spike files

- **Source:**
  - `GameClient/Source/ShatteredFrontier/Private/TerrainSpike/` (`SFTerrainSpike.*`, `SFTerrainSpikeProbe.cpp`, development only);
  - `GameClient/Plugins/TerrainSpikeMeshTerrain/` (editor-only importer; disabled by default, enable it in the uproject only for the spike).
- **Map:** `/Game/Frontier/Maps/Dev/TerrainSpike/TerrainSpike_Voxel`. The Mesh Terrain map is regenerated by script and not committed, because it needs the experimental plugin enabled.
- **Scripts** (`tools/ter01/`):
  - `Create-TerrainSpikeMap.py`;
  - `Run-TerrainSpikeNet.ps1` (`-ClientPy`, `-LegacyWinding`);
  - `client_checks.py`;
  - `Create-MeshTerrainMap.py`;
  - `Run-MeshTerrainSpike.ps1`.
- **Probe:** `SFTerrainSpike.Probe`, for cooked builds.
