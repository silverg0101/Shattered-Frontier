# VEG-01 — Procedural Vegetation Editor fit check for harvestable plants

Status: proposed, 2026-10-19. Desk research is done; the spike has not started. This record asks whether UE 5.8's Procedural Vegetation Editor (PVE) should author harvestable trees and plants, including how they look as they regrow after harvest and, if Herbalism is implemented, flowers and herbs.

## What the game needs

From the [gathering node lifecycle](../Game-Design/Crafting/Legends_Shattered_Frontier_Crafting_Gathering_and_Resources_v1.0.md) (§3) and the [gathering lifecycle pilot](../gameplay-gathering-lifecycle-pilot.md):
- **Server-owned state:** nodes run `available → depleted → pending_spawn → available`, and all state is server-owned.
- **Respawn location:** a respawn picks a free spawn point in the node's pool at random. The design does **not** regrow a node in place, and it has no growth stages.
- **Felling presentation:** felling leaves a torn stump and a server-owned fall ([felling WIP](../gameplay-harvest-felling-wip.md)). The localized harvest geometry reads triangles from the authored oak meshes on the CPU.

Visible regrowth (stump → sapling → young → mature) would be new presentation. Doing it in place would also be a design change (see Decisions below).

## What PVE is (sources below)

- **Status:** experimental in 5.8, with the warning that "Substantial architectural changes can occur between releases." It is a per-project plugin, built on PCG.
- **Compatibility:** 5.7 PVE assets do not open in 5.8, and 5.8 removed the Preset Loader. Expect breaking changes on every engine upgrade.
- **Workflows:** a botanical **Grower** node (Cycles, Phyllotaxy, Growth, Phototropism, Gravity, Bifurcations, Foliage, Auxin), recursive growth (grafting), and import.
- **Output:** Nanite Foliage assets, which support instancing, GPU skeletal wind animation and a voxel representation at distance. An **Export** node writes standard static meshes. The graph itself is edit-time only, and only exported assets go into the world.
- **Growth stage at runtime: not confirmed.** Epic's 5.8 page lists no age or maturity slider. Growth is simulated in the editor graph, and its output is transient until exported. A third-party 5.7 guide describes a 0–1 "Growth Stage" that "can be animated at runtime", but it doesn't show how, and the 5.8 docs don't support it. Treat runtime growth as unavailable until the spike proves otherwise.
- **Undocumented:** the page says nothing on runtime use, dedicated servers or Linux.

## Working assumption

Use PVE as an **authoring tool** for growth-stage variants, not as a runtime growth system:
- **Assets:** for each harvestable species, grow and export a small fixed set of stage assets from one graph and seed, such as stump, sapling, young and mature.
- **Server:** keeps the node's stage as replicated state. Advancing it is a server event on the existing regeneration clock.
- **Clients:** swap the instance or mesh to match the stage. No geometry is ever generated at runtime.
- **Determinism:** this matches the [world pipeline](../settlement-world-pipeline.md)'s rule: generated in the editor, versioned, the same for every client.
- **Fallback:** if PVE breaks on an upgrade, the exported assets keep working. Only re-authoring is blocked until the graph is rebuilt in the new version.

Flowers and herbs are small plants. Use PVE for them only if its foliage output beats ordinary meshes in the spike. They do not need the felling path.

## Spike questions, each answered with evidence

1. **Stage variants:** can one graph and seed export a consistent stump, sapling, young and mature set for an oak, so that a species keeps its silhouette as it ages? Which parameters produce the stages (Cycles, Foliage lifespan scale, others)?
2. **Runtime growth:** does any 5.8 runtime path change growth on a placed asset (the third-party "Growth Stage" claim)? If yes, is it deterministic and cheap enough for many nodes? If no, confirm that swapping assets per stage is the method.
3. **Felling compatibility:** can PVE exports (static or Nanite skeletal) feed the felling path, which needs CPU-readable triangles, a separable stump and collar, and collision? Or does each tree need a non-Nanite felling twin?
4. **Server:** what does a dedicated Linux server need from these assets (collision only)? Does the plugin or its asset type load in a server build, and at what memory cost?
5. **Performance:** what do frame time and instance count look like for a stand of PVE trees with stage swaps, compared with the current MightyOak meshes and `dress_region.py` scatter? This reuses the pipeline's Stage 3 comparison.
6. **Upgrade cost:** what does it take to rebuild the graph in the next engine version? Keep the graph and seeds committed, so the stage assets can be re-exported.

Constraints: work only under `/Game/Frontier/`, using committed graphs, seeds and export scripts. Leave lighting alone. Megascans/Fab plants are read-only references.

## Owner decisions (2026-10-19, recorded in [GAT-01](GAT-01-growing-resource-nodes.md))

1. **Where regrowth happens:** growth happens at the pool's next free point. A one-point pool gives in-place regrowth.
2. **Harvesting young nodes:** young stages are harvestable for fewer charges. This applies to every growing node kind; rocks and ores rise out of the ground.
3. **Herbalism:** it is future work, and GAT-01's kind checklist keeps the path open.
4. **Stumps:** there are none. A felled tree is gone until something grows back. This simplifies question 3: PVE exports only need to feed the mature tree's felling, not a stump.

PVE matters only for the `swap` style (trees, and possibly bushes). The `rise` and `scale` styles reuse the mature mesh.

## Outcome

To be recorded when the spike finishes. Pick one:
- **Adopt** as the stage-asset authoring tool.
- **Adopt for background trees only**, as decoration.
- **Reject** and author by hand or in Blender.

Include the evidence for each question.

## Sources

- [Procedural Vegetation Editor in Unreal Engine (Epic, 5.8)](https://dev.epicgames.com/documentation/unreal-engine/procedural-vegetation-editor-in-unreal-engine?lang=en-US)
- [Procedural Vegetation Editor, Unreal Engine public roadmap](https://portal.productboard.com/epicgames/1-unreal-engine-public-roadmap/c/2158-procedural-vegetation-editor)
- [UE 5.7 PVE guide (StraySpark, third-party, unverified for 5.8)](https://www.strayspark.studio/blog/ue-57-procedural-vegetation-editor-complete-guide)
- [Unreal Engine 5.7: foliage, PCG and in-editor AI (Digital Production)](https://digitalproduction.com/2025/11/12/unreal-engine-5-7-foliage-pcg-and-in-editor-ai/)
