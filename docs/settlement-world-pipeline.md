# Settlement layout-to-world pipeline

Status: proposed 2026-10-19; Silverbrook is the first user ([zone plan](greenlands-silverbrook-zone-plan.md), "Pipeline work"). This is the reusable method for turning a settlement design board into a playable area, then dressing whole regions automatically and fine-tuning them by hand. Every step must be repeatable from committed scripts and assets.

## Inputs and authority

| Input | Owns |
| --- | --- |
| Region definition and region map (`docs/Game-Design/Regions/…Region_<Name>…md`) | Where each settlement and zone sits, plus coast, mountains, main rivers and the roads between settlements. A zone may add detail inside its bounds but never contradicts the region. |
| Settlement definition (`docs/Game-Design/Regions/…Settlement_<Name>…md`) | Every number and name inside the settlement. Wins all conflicts within the zone. |
| Design board ([standard](Game-Design/World-Simulation/Legends_Shattered_Frontier_Settlement_Design_Board_Standard_v1.0.md)) | Visual direction only. Layout is traced from it, then reconciled to the definition. |
| [Zone Definition Standard](Game-Design/World-Simulation/Legends_Shattered_Frontier_World_Zone_Definition_Standard_v0.1.md) | Z-checklist, budgets, compass axes (north −Y, east +X) |
| [DEC-03](decisions/DEC-03-terrain.md) | Voxel density is the terrain authority. The client surface/mesh adapter is still open (see [TER-01](decisions/TER-01-mesh-terrain-fit-check.md)). |
| [Tuning architecture](Game-Design/Legends_Shattered_Frontier_Tuning_and_Configuration_Architecture_v0.1.md) | How seeds and dressing parameters are declared |
| Look reference and look profile (per biome) | The finished look: species, rocks, ground cover, terrain character. Greenlands: [`Greenlands-Forest-Look-v1.webp`](Game-Design/concept-art/Greenlands-Forest-Look-v1.webp), read into `GameClient/ArtSource/Greenlands/greenlands-look-profile.json` |

## Target: the agent zone workflow

The owner supplies a region map, then per settlement a design board and one or more look references. Agent sessions turn them into a dressed, elevated, playable zone, plus a list of assets that are still missing. Each step is a separate session-sized task with a committed output and a check, so any step can be rerun without redoing the others.

**Hard rule: region first.** No zone layout, terrain or build may start until its region has a layout that passes its validator. The region layout fixes everything a zone inherits from its surroundings:
- where the zone sits;
- its ground height along every edge;
- where rivers and roads enter and leave it;
- what lies beyond it, such as coast, mountains or neighbouring zones.

A zone's backdrop is the region terrain beyond the zone's edge. Never invent it per zone.

| Step | Agent work | Committed output | Check |
| --- | --- | --- | --- |
| 0. Lay out the region | Trace the region map into region layout data at the owner-confirmed scale. First instance: `GameClient/ArtSource/Greenlands/greenlands-region-layout.json`, checked by `validate_region.py`. `region_heightfield.py` writes `greenlands-region-heightfield-preview.png`, and its `zone_edges()` lists a tile's edge heights plus the rivers and roads that cross its edge. Include coastline and sea level, mountain ranges with heights, the main rivers from source to mouth with water levels and falls, lakes, settlements with their zone footprints, the roads between settlements, and biome areas. From that, build one coarse region heightfield. | `<region>-region-layout.json` and the region heightfield script | Region validator: rivers run downhill to the sea, settlements and zones sit on land and don't overlap, roads connect the settlements named in the region definition, and every zone's edge heights and its river and road entry and exit points can be read from it |
| 1. Read the board | Trace the board into layout data, using its scale bar, and reconcile it to the settlement definition. Read the whole board, not only the town plan: the town map, the isometric view and every panel together describe one area. Capture the relief at the centre (such as a raised district), the land around the town, and the backdrop (mountains, cliffs and waterfalls), as well as the streets and buildings. These tie into each other: streets climb the high ground, the river cuts through the relief, and the backdrop frames the views. Includes elevation: high ground, ridges, cliffs, water levels, waterfall drops and the backdrop's heights and distances. | Layout JSON (Stage 1) | Layout validator |
| 2. Read the look | Turn each look reference into a look profile: asset roles per layer (canopy, understory, ground cover, rocks, litter, surface, backdrop), with variants, shares and placement rules. Record which project assets already fill each role. | `<biome>-look-profile.json` | Profile schema check; owner review of the role list |
| 3. Shape the land | Generate the terrain from the layout's elevation data: rolling macro forms, high ground, valleys cut for rivers, flattened road and building beds, cliffs and outcrop zones. Also the backdrop beyond the zone edge (mountains and far hills), so no view ends at the edge of the world. Not flat: the relief must read at the board's scale from the arrival point. | Terrain source plus build script (Stage 2; the surface type depends on [TER-01](decisions/TER-01-mesh-terrain-fit-check.md)) | Height samples at landmarks, roads within grade, rivers below banks |
| 4. Build the town | Splines, district volumes and landmark blockouts on the shaped terrain | Zone map via the builder (Stage 2) | One-line summary; arrival capture |
| 5. Dress it | Layered PCG driven by the look profile's slope, height, shade and distance rules: macro rocks and bedrock on steep ground, canopy by height and moisture, understory, ground cover, litter. | PCG graphs plus saved output (Stage 3) | Regenerate-and-compare; capture compared against the look reference |
| 6. List what's missing | Compare look-profile roles and the settlement's building library against the biome library | Missing-asset batch (Stage 4) | Every gap row traces back to a role or building |
| 7. Build missing assets | Separate sessions: concept image (Qwen Image), then model (Pixal3D or Blender) through the existing art pipeline | Assets in the biome library, plus updated `have` lists | Model production gates |
| 8. Close the loop | Re-run steps 5 and 6 with the new assets | Updated captures; fewer gaps | Captures read like the look reference |

**Visual target:** the captures from step 5 should read like the look reference: rocky, elevated ground with layered vegetation and cover from foreground to background. Lighting stays out of scope, so the comparison covers composition, species, rocks, ground layers and terrain form. It does not cover light or colour grading.

## Rules

- **Lighting is out of bounds.** Do not change Lumen, GI, reflections, exposure, VSM or post-processing.
- **Write only under `/Game/Frontier/`.** Megascans/Fab content is read-only.
- **Every lasting change is a committed script or asset.** No editor-only clicks.
- **Use the editor's MCP server to inspect only.** Close the editor before building.
- **Run targeted checks only.** Update `docs/dependency-map.md` and the settlement's zone plan in the same change.
- **Report each stage** by what can be observed in-game, separately from enabling work.

## Stage 1 — Layout file and validator

The layout file is one JSON per settlement, `GameClient/ArtSource/<Biome>/<settlement>-town-layout.json`. It is written in metres, in the zone frame (north −Y, east +X, origin at the town centre). Trace it by hand from the board using the board's scale, then reconcile it to the definition.

**Contents:**
- **Polylines:** primary and secondary roads, trails, rivers and brooks, and walls.
- **Polygons:** districts.
- **Points:** landmarks (number, name, footprint, height from the board elevations) and gates.
- **Elevation:** elevation markers (height in metres at named points), ridge and valley lines, cliff lines, water surface levels, waterfall drops, and areas to flatten for buildings and plots. The terrain build (workflow step 3) reads these.
- **Backdrop:** the peaks and ranges seen beyond the zone, with direction, distance and height. Take these from the board's isometric view and backgrounds. They are part of the area, not decoration added later.
- **Bounds:** zone bounds.
- **Other:** plots, arrival and a review route.

**Validator:** a standard-library-only Python script with an assert-based `__main__` (first instance: `GameClient/ArtSource/Greenlands/validate_layout.py`; the builder imports it and refuses invalid layouts). It must fail when:
1. A landmark or district required by the definition is missing.
2. The walled-town radius from the definition is exceeded, or anything lies outside the zone bounds.
3. A gate does not sit on both a road and the wall.
4. The roads out to each neighbour named in the definition do not reach the zone edge in the stated direction.
5. The main river does not pass under the named main bridge.
6. A polygon intersects itself.

**Exit:** the validator passes on the real file and fails on a deliberately broken copy.

## Stage 2 — Unreal builder

A headless editor Python builder reads the layout file and regenerates the zone map. Re-running it produces the same map.

It creates:
- **Terrain:** shaped from the layout's elevation data (workflow step 3), with river valleys cut, road and building beds flattened, and slopes left steep where the profile wants exposed bedrock. Until TER-01 picks the surface type, use a heightfield built by script that can be regenerated. It must stay convertible to DEC-03 base density later.
- **Splines:** spline actors for roads, rivers and walls.
- **District volumes:** one volume per district, tagged with the district id. These are the future `level_area` polygons (Z4/Z5), but server protection is not wired in at this stage.
- **Landmark blockouts:** blockout boxes at each landmark, at the definition's dimensions.
- **PCG volumes:** the volumes Stage 3 dresses.

**Exit:** a commandlet run logs a one-line summary of counts by type, and the in-game review captures the town from the arrival point.

## Stage 3 — PCG world dressing

The aim is that the owner can dress a region from the layout file in minutes, then fine-tune it.

**Graphs:**
- **Plugin and location:** PCG is enabled by default in UE 5.8 (`EnabledByDefault` in `PCG.uplugin`), so the project needs no plugin entry. Graphs live under `/Game/Frontier/PCG/`.
- **Rules and spawning (decided 2026-10-19, first instance Silverbrook Eastwood):** the placement rules run in standard-library Python, and PCG spawns and tunes the result:
  - `dress_zone.py` reads each role's `place` block in the look profile: density, spacing, slope, height above the valley floor, sun or shade, clearance and scale. It runs on the zone heightfield and the builder's building footprints. It places canopy first, so the lower layers can tell sun from shade.
  - `pcg_dressing.py` writes `PCG_<Zone>_<District>`. Per mesh, the graph runs Create Points (world space) → Difference against volumes tagged `PCG_Exclude` → Static Mesh Spawner. Ground cover has no collision.
  - **Why this split:** rules such as height above the valley floor need the zone heightfield anyway. In Python they are testable and deterministic without the editor, and PCG still gives instanced output and the owner's exclusion volumes.
  - **Missing meshes:** a role with no mesh uses a fallback role's meshes and is listed as a gap for Stage 4.
- **Inputs:** the terrain surface, the layout splines (roads, river and walls act as exclusions), the district volumes and the biome look profile.
- **Layers:** each graph places layers in the profile's order: rocks and bedrock first, then canopy, understory, ground cover and litter. Each layer samples slope, height above the valley floor, shade under canopy and distance to roads or water, following the profile's rules. Shares and variant counts come from the profile, so a new look reference changes the data, not the graph.
- **Output:** instanced static meshes, one component per mesh, generated in the editor and saved with the map (generation trigger: on demand). Partitioned generation needs a World Partition map; the zone maps aren't World Partition yet.
- **Meshes:** use only meshes that already exist in the project.

**Not chosen: the Procedural Foliage Tool** ([Epic docs](https://dev.epicgames.com/documentation/unreal-engine/procedural-foliage-tool-in-unreal-engine?lang=en-US)).
- **What it is:** an older experimental editor feature, behind Editor Preferences > Experimental. A spawner volume simulates "Num Steps" years of seeding, spreading, shade and overlap competition, and scales instances with age ("Max Age", "Procedural Scale Max").
- **Why it isn't used:** it is edit-time only. Its pages don't document runtime use, World Partition, fixed seeds or spline exclusions, and it targets Landscape. PCG covers all of these.
- **Ideas worth borrowing:** its ecology rules make natural forest edges. These are spread from parent trees, shade suppression, and age-based scale variety. Mimic them inside the PCG forest graph if plain density scatter looks artificial.
- **Not for regrowth:** its "age" only sets the placed scale, so it does not help with runtime regrowth ([GAT-01](decisions/GAT-01-growing-resource-nodes.md)).

**Determinism:**
- **Content version** = seed + graph asset versions + layout-file hash. The builder logs it.
- **Configuration:** the seed and the dressing parameters are typed configuration, following the tuning architecture.
- **Same version ⇒ identical output.** A regenerate-and-compare check proves it by comparing the instance count and a hash of the transforms.

**Generation policy:**
- Output is generated in the editor and saved. Clients and the server never generate at runtime.
- A world's seed is fixed. A new seed only arrives as a new content release with a GM wipe, never as a live command.

**Two kinds of output:**

| Kind | Content | Destination |
| --- | --- | --- |
| Decoration | Grass, flowers, small rocks, background trees | Client instanced meshes only |
| Gameplay candidates | Harvestable trees, stone, herbs | A separate graph output, exported by script to a server spawn-data file. Each entry has a stable ID derived from the content version and its position. |

The gameplay-candidates file is produced but not used yet. Harvestable plant assets, and how they look as they regrow, may be authored in the Procedural Vegetation Editor; see [VEG-01](decisions/VEG-01-procedural-vegetation-editor-fit-check.md). Its validator must fail on:
- duplicate IDs
- entries closer together than the minimum spacing
- entries on a road or inside a build plot

**Fine-tuning, in this order:**
1. Exclusion and override volumes, and painted density masks used as graph inputs.
2. Manual per-instance edits in 5.8. Check Epic's 5.8 PCG documentation to confirm the edits survive regeneration, and record the result here. Forum reports, which are not yet verified, say the 5.8.0 editing tool can freeze. If it proves unreliable, document the volume-based workaround instead.

**Player terrain edits (DEC-03):** write a design note on how decoration instances inside an edited voxel chunk are removed at runtime. This is a design note only; nothing is implemented.

**Exit:**
- The regenerate-and-compare check passes. **Met 2026-10-19:**
  - Two full rebuilds gave content version `eastwood-1019-1-0b79e48657` and transforms hash `d61a51ba9795`.
  - `dress_zone.py` also asserts that the same version gives the same output and that a new seed changes it.
  - `pcg_dressing.py` checks that PCG generated exactly the decided instance count, 22,036.
- One district is dressed end to end, with in-game captures. **Met 2026-10-19:** Silverbrook Eastwood, `Silverbrook-Eastwood-InGame.png`, with 1,055 gameplay candidates.
- Frame time and instance count are compared against the per-actor scatter in `dress_region.py`. **Half met:**
  - The Silverbrook walk records frame time: a 2.2 ms median and 3.0 ms p95, offscreen at 1280 × 720.
  - The art-review map hasn't been measured the same way yet.
- The checklist below is filled in. **Not yet.** Also open: confirming that 5.8's per-instance edits survive regeneration (fine-tuning step 2).

### How to dress a new region

To be written when Stage 3 exits. It must be 10 steps or fewer, written for the owner.

## Stage 4 — Missing-asset report

A standard-library script reads three inputs:
- **Look profile:** its roles and variant targets.
- **Building library:** the settlement definition's building reference (for Silverbrook, §8).
- **Biome library index:** each role's `have` list, plus the meshes under `/Game/Frontier/Environment/<Biome>/`.

It writes one batch file under `docs/art-templates/asset-lists/`, in the existing row format from [image-jobs.md](art-templates/image-jobs.md), and adds a row to [asset-catalog.md](art-templates/asset-catalog.md). Image sessions (Qwen Image) and model sessions (Pixal3D or Blender, [model pipeline](art-templates/model-pipeline.md)) then work through it as they do any batch.

**Rows:**
- **What gets a row:** each role with fewer assets than its target variants, and each building type with no matching mesh. Each row records the role, the main concept from the profile description, its source (look reference or definition section), and a priority.
- **Priority:**
  - P1: canopy and large rocks, which shape the look.
  - P2: ground cover and litter.
  - P3: backdrop.
- **Growing nodes:** harvestable roles also list their [GAT-01](decisions/GAT-01-growing-resource-nodes.md) growth style. `swap` needs a mesh per stage; `rise` and `scale` need only the mature mesh.

**Licensing:** licensed Fab or Megascans assets never fill a role in the report. They are read-only and may not be restorable on every machine.

**Exit:** the report for the Greenlands look profile names the current gaps: tall pines, young broadleaf, saplings, large moss boulders, bedrock slabs, cobble scatter, litter, forest-floor and bedrock materials, broad-leaf herbs, moss patches and backdrop domes. Re-running it after an asset lands removes that row.

## Deferred

- **Lighting:** any lighting work.
- **Image tracing:** automatic, non-agent map extraction and an image-tracing UI. Agent tracing of boards (workflow step 1) is in scope; layout data stays agent-written and validator-checked.
- **Spawn wiring:** wiring gathering, enemy and wildlife spawns. The gameplay-candidates file is not consumed yet.
- **Zone data:** zone records under `content/world/…`, respawn points and server-protected volumes.
- **Runtime changes:** runtime PCG generation, and GM tools that change seeds live.
- **Default maps:** switching `GameDefaultMap` or `ServerDefaultMap`.
