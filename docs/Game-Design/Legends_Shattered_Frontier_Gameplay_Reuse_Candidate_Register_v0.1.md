# Gameplay reuse candidate register v0.1

Reviewed 2026-10-18. Initial research candidates; subsequent Minimal Inventory Runtime pilot integration is tracked in the [gathering checkpoint](../gameplay-gathering-pilot.md). Other entries remain unadopted. The user started without gameplay packs and requested free Fab research beyond Epic samples. “Free” below means the Fab page showed Free / Add to My Library at review, not a permanent price guarantee. Listing claims are untested; exact UE 5.8, Linux dedicated-server and project fit require source inspection.

## Design governs selection

Reuse must serve the [Greenlands vertical slice](Legends_Shattered_Frontier_Vertical_Slice_Specification_v1.0.md). Preserve [Itemization](Legends_Shattered_Frontier_Itemization_Architecture_v1.0.md)'s single mutation owner, private projections and receipt/ledger contracts; [resources/crafting](Legends_Shattered_Frontier_Resource_and_Crafting_Architecture_v1.0.md)'s domain ownership; [construction](Legends_Shattered_Frontier_Construction_System_Architecture_v0.1.md)'s canonical 0.5 m grid, support, permissions and atomic costs; and [combat](Legends_Shattered_Frontier_Combat_System_v0.1.md)'s classless skills, ratings, GCD and authoritative pipeline. A pack's recipes, capacity rules, combat formulas, saving or client rewards do not become our design by adoption.

Editable Blueprint graphs can be adapted; a Blueprint-callable C++ plugin may instead require C++ changes. Inspect the delivered implementation before estimating savings. Adjustable values must follow the [shared configuration architecture](Legends_Shattered_Frontier_Tuning_and_Configuration_Architecture_v0.1.md).

## Currently free Fab shortlist

| Candidate / implementation | Intended value and decision |
| --- | --- |
| [Minimal Inventory Runtime – Multiplayer Ready](https://www.fab.com/listings/c566626e-0375-4aa4-8bf3-38394b4f884c), C++ source with Blueprint APIs | Integrated into the session-only gathering pilot; production evaluation remains open. Explicit UE 5.7/5.8 support; advertised platform is Windows only. Replicated slots, equipment, containers and inventory exchanges may save implementation. No bundled Blueprint assets/demo. Its inventory-only atomic exchanges and SaveGame hooks do not supply world-cost transactions or our durable ledger. Linux viability and a single-writer adapter are adoption gates. [Creator documentation](https://celestiadominance.com/minimal-inventory-runtime-documentation). |
| [Advanced Interaction System](https://www.fab.com/listings/eb4587a6-29c3-4d96-8b62-2cf738d9050d), Blueprints | Evaluate selective interaction/prompt reuse for resource nodes, doors and NPCs. Requires Enhanced Input, currently disabled in this project: account for that migration and duplicate-input risk. Retain server reach, LOS, permissions, revisions and commit feedback. Multiplayer marketing does not establish those guarantees. |
| [Easy Building System V10](https://www.fab.com/listings/b2c5a428-e9af-4c6a-9daf-d10b8b04bddf), Blueprints | Inspect separable chopping/mining interactions or menu presentation for gathering. Its prefab snapping, support/collapse, claims and saving differ from our construction contracts; replacing those systems is not proposed. Verify components can be extracted without importing the whole framework. |
| [Advanced Modular Inventory System AAA V1.0](https://www.fab.com/listings/ad233a1b-1830-4dba-b581-08b731105d27), C++ / Blueprints | Backup inventory candidate with replicated operations and UI separation. Free offer is described as lasting until v2.0; future UI/updates are not assumed included. Verify UE 5.8/Linux, privacy, replay behavior and persistence before choosing it over the first candidate. |
| [Inventory Grid System](https://www.fab.com/listings/69047165-a8b3-4938-81e6-749b50a5039e), Blueprints | Later inventory UX reference: drag/drop, stacks, equipment and hotbar. Listing explicitly says no network replication. Consider presentation over our authoritative read model only; no adoption of spatial-grid capacity or crafting rules. |
| [Dialogue Tree and Quest System](https://www.fab.com/listings/d4f2e8a9-61ed-4325-88d2-933019d2cafc), Blueprints | Later Greenlands introduction/dialogue authoring candidate. Older UE4 asset; UE 5.8 migration and multiplayer fit unverified. Keep objective progress and rewards in the [Quest architecture](Legends_Shattered_Frontier_Quest_Architecture_v1.0.md); local dialogue/save state cannot authorize rewards. |
| [Advanced Locomotion System V4](https://www.fab.com/listings/ef9651a4-fb55-4866-a2d9-1b38b028f9c7), Blueprint character / animation | Later foot placement, turning and landing presentation on rough ground and construction slopes. Explicitly not plug-and-play. Assess network prediction and animation dependencies. Mantling, rolls, speeds, jump and capsule changes require a separate movement-design decision. |
| [UI Material Lab](https://www.fab.com/listings/69680f34-e5d2-44e6-b023-f054bbf629eb), Epic material functions / UMG examples | Later gathering/crafting progress and readable status presentation. Preserve distinct pending, committed and denied states and feedback understandable without color/audio alone. Provides presentation examples, not gameplay authority. |

Fab's “Allows usage with AI” metadata was Yes for Minimal Inventory Runtime, Advanced Interaction, Inventory Grid, Dialogue Tree and ALS; No for Easy Building, Advanced Modular Inventory and UI Material Lab. This records listing metadata, not a legal interpretation. Resolve applicable license/workflow terms before AI-assisted inspection or modification of acquired source. Initial listing research did not inspect candidate source. Follow-up inspected the already-installed Minimal Inventory Runtime source for the isolated gathering pilot; other candidates remain uninspected.

## Other references and later watchlist

[Epic Lyra inventory/equipment](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-inventory-and-equipment-in-unreal-engine) remains a mixed C++/Blueprint reference; account for its framework dependencies before extraction. It is not the only reuse route.

[Ultimate Map System](https://www.fab.com/listings/0db9a37f-5c36-46ac-92af-5fc514e0a88d) is a future map/objective UI candidate, but current free pricing was not verified: checkout showed purchase controls despite “free” description text. Do not classify it as free or acquire on that assumption. Enemy icons must never disclose unauthorized server information. Paid kits and expired giveaways are excluded from the free shortlist.

## Inspected presentation fit

Minimal Inventory Runtime's built-in Slate inventory creates a separate desktop window. Keep it as a reference; the game's bag should use the supported existing-HUD/UMG integration route. Its actual inventory transactions and equipment handling are reused by the [gathering/refinement/tool pilots](../gameplay-gathering-tools-pilot.md). This is a concrete fit decision from installed-source inspection, not adoption of an entire UI framework.

## Player benefit and next implementation step

Possible improvements worth comparing are consistent action prompts/remappable controls, clearer transaction feedback, readable introductory dialogue and better foot contact. These are proposals, not approved mechanic changes. No listing alone establishes a better player experience. If an asset suggests a rule change, document the concrete player flow, expected benefit, accessibility/latency effects and design/dependency cost before amending the governing design.

Current implementation: the leading inventory candidate is owned and its installed source is being integrated into the session-only Win64 gathering pilot. Linux, durable ledger/quality-bearing instances and production fit remain open. Assess the interaction component alongside subsequent gathering presentation. Use one bounded integration before committing to a framework. Preserve existing construction and the separate environment session's assets. Check authority and the changed player flow at the integration boundary; this research requires no runtime tests.


## First harvesting visual candidates — 2026-10-19

Live listing research observed current Free status for the following. No acquisition/source inspection occurred; a browser surface was unavailable. Visual fit, exact UE5.8 compatibility and terms remain to be checked before adoption. None supplies the complete required harvesting experience.

| Candidate | Possible use and decisive gap |
| --- | --- |
| [HighPoly Tree Model](https://www.fab.com/listings/2f1f43fe-fd59-42ff-967d-c8eb1a0785e7) | Realistic tree source candidate, FBX/UE, advertised about500k triangles. Species/oak match, close-up quality and performance unverified; no cut/stump/felling contract. |
| [Wood_Crack_00](https://www.fab.com/listings/3a07db22-cf25-4016-84f3-6fadd16ac519) | Cut-trunk FBX/PBR reference; no complete harvestable tree or dynamic-cut implementation. |
| [Rocks and combined](https://www.fab.com/listings/4084b15b-c876-4df9-8e72-9f50b63175ba) | Environment rock/material candidate; ore composition, fracture interiors and localized extraction need review/work. |
| [Free Sample Animation Set](https://www.fab.com/listings/5b737d10-b4cd-4294-8f6b-757b9088039d) | Listing references selected resource-gathering animations; actual available chop/mine clips, rig and retargetability unverified. AI-use metadata No; inspect applicable workflow/terms before source use. |

The first three showed AI-use metadata Yes, generated-with-AI No; animation sample showed No/No. Metadata is not a license interpretation. Listings titled Free with purchase controls (Rock Collection04, Free Stylized+LowPoly Rocks) were not accepted as verified free. Keep the [quality contract](Legends_Shattered_Frontier_Gathering_Presentation_and_Material_Claiming_v0.1.md): source convenience must not change resource identity or harvest/reward mechanics.

## User-supplied downloads inspected2026-10-19

| Source | Actual contents and design fit |
| --- | --- |
| `cc0__sawtooth_oak_quercus_acutissima.glb` | Embedded author ffish.asia/floraZia and CC0-1.0; [source](https://sketchfab.com/3d-models/cc0-sawtooth-oak-quercus-acutissima-35f02bdd5cc34f90aff1324296f4db7c). Render shows flowering twig specimens and a scale chart, about25cm across, despite the oak title. 1,028,356triangles; possible botanical reference/detail, **not a complete harvest tree/crown**. |
| `quercus_A_packed_v1.zip` | Lauris Kaplinski complete low-poly oak with packed textures; intended LOD0 ~5,303triangles. Sparse shelf-like crown, disconnected image materials and open collision trunk need substantial work. The OpenGameArt listing declares CC-BY-SA3.0/GPL3.0; not adopted. |
| `tree_debris_pack_vdqjfjhiw_high.zip` | Six connected weathered wood pieces, FBX plus4K maps. Metadata describes3–25cm pieces. Useful ground-scatter candidate; aged bark-covered forms do not match fresh harvest splinters. Archive metadata has no explicit license/author, so no distribution/adoption yet. |
| `Free Sample.rar` | Bronze and painted-steel PNG/Substance material files; does not supply gathering animation. |
| `blenderkit-v3.21.3.261002.zip` | Blender add-on archive, not an art pack. Not installed. |

Downloads were preserved; inspection copies/reports/previews remain under ignored `Artifacts/HarvestSourceReview/`. Candidate availability does not override the harvesting quality contract.

[Mighty Oak Trees by Jagobo](https://sketchfab.com/3d-models/mighty-oak-trees-4f6ab5594a8a415aba3f958682b9ced5): user downloaded original Blender ZIP2026-10-19. Signed-in dialog identifies [CC-BY4.0](https://creativecommons.org/licenses/by/4.0/), creator attribution and commercial use. Three complete old oaks with bark color/normal/roughness and foliage color, advertised100–140k triangles/tree. Source/render review in progress; record source hash, credit and modifications before any adoption.
