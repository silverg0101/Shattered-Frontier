# First tree and rock harvesting presentation

2026-10-19. Functional development pilot; **visual quality is not accepted**. The [quality contract](Game-Design/Legends_Shattered_Frontier_Gathering_Presentation_and_Material_Claiming_v0.1.md) requires credible localized damage, material-specific fragments, confirmed acquisition, contact animation and tree felling. [Compact evidence](evidence/harvest-localized-pilot-results.json) distinguishes executed checks from remaining work.

## Current runtime

`SFHarvestPresentation` reconstructs server-approved localized cuts on a bounded CPU mesh, preserves exterior UVs/normals and assigns exposed interiors. The private geometry helper subtracts closed convex angular rock chips or wood notches; it rejects invalid/open/over-budget output before the timed action can grant materials. Triangle groups are preserved, including the imported-mesh regression. The source and result stay unchanged on failure.

The server mints a fracture seed per accepted contact, resource location, generation and cut ordinal. Replicated cut records carry that seed and actual radius/depth. Rock profiles vary sides, orientation, proportions, taper and offset; size variation is captured in the approved event. Retries do not reroll; reconstruction uses the published event. Local fragments vary with that seed and material, create no items and do not obstruct gameplay. Late-join/initial snapshots suppress historical fragment bursts. Cross-platform bit-identical Boolean output remains unproven.

Presentation settings are typed, release-author controlled and effective next world: nominal rock radius18cm [2,35], depth12cm [1,25]; wood radius14cm [2,25], depth9cm [1,18]; fracture size variation0.2 [0,0.35], with actual dimensions clamped to the material bounds. Existing captured fragment count, lifetime, speed and restitution settings remain in `DefaultGame.ini`. Algorithms/profile topology/safety ceilings are versioned code/content. Production controls must use the shared configuration workflow.

## Imported source packets

| Source | Current integration |
| --- | --- |
| [Mighty Oak v1](../GameClient/ArtSource/HarvestOak/MightyOak-v1/README.md) | Jagobo CC-BY4.0 Tree3, explicitly scaled to10m. Closed/welded lower1m trunk (88tri), separate upper structure and masked foliage at the same ground origin. Imported3meshes/6maps into `Gameplay/Harvest/MightyOakV1`. Lower chunk supports the intended~0.85m contact; upper branches are visual source geometry, not a closed cutting volume. |
| [Iron-bearing rock rev02](../GameClient/ArtSource/HarvestRock/rev02/README.md) | Naturally unbroken shell,171vertices/338tri, hidden ground closure only. Fresh revision-specific atlas; no pre-baked exposed side extraction. Imported9meshes/4maps into `Gameplay/Harvest/RockV2`. Authored extraction stages are source comparisons; runtime damage follows actual contacts. |
| [Earlier oak cutting study](../GameClient/ArtSource/HarvestOak/README.md) | Compact closed trunk/stump/fragment source retained; rejected procedural crown excluded from Git/runtime. V1 wood fragments remain a cosmetic prototype fallback. |
| [Audio candidates](../GameClient/ArtSource/HarvestAudio/README.md) | Four Kenney CC0 OGG/mono PCM16 WAV pairs with license/hashes and a fixed-destination importer. Imported into `Gameplay/Harvest/AudioV1`; spatial, seeded wood/mining variants play only for new approved extraction contacts, with bounded per-resource concurrency. Not yet accepted by auditory review. |

Original/generated source provenance is retained. [Generated references](../GameClient/ArtSource/HarvestReferences/README.md) preserve exact image prompts. The [reuse register](Game-Design/Legends_Shattered_Frontier_Gameplay_Reuse_Candidate_Register_v0.1.md) records free Fab and user-supplied candidates. The misleadingly titled CC0 sawtooth-oak download is flowering twigs with a scale chart, not a whole tree; the older OpenGameArt tree remains below the crown quality bar. BlenderKit was not installed.

## Executed evidence and remaining visual defect

Build16 passed. All7 focused `ShatteredFrontier.Gathering.` checks passed with zero warnings/errors: Authority, Lifecycle, SurfaceProtocol, Tools, ArtContact, LocalizedGeometry and LocalizedImportedContact. ArtContact exercises the actual preferred rock AND oak mesh, three distinct authoritative seeds, no precommit item, three grants, changed contact geometry, depletion, renewal and stale replay. Geometry checks cover varied profiles and repeatability in the current build/process.

A dedicated server plus rendered client completed the revised rock's three cuts/grants, live fragments and unchanged replay (`harvest-art-20261019-032408-201`). The development-only capture gate finishes loaded asset compilation, verifies material shaders and13 resident texture resources, then allows rendered frames to settle. Review-only studio fill makes the rock readable under the oak canopy; no production lighting changed. Viewed intact and final frames show an unbroken rock developing localized irregular cut damage. The oak remains too pale in this fixture, and this functional review is not final visual acceptance.

Build19 passed with the impact-audio helper bound to the same de-duplicated new-cut path as fragments. AudioV1 import verified all four SoundWaves. The focused actual oak/rock contact check passed after the hookup (`harvest-audio-contact-01`); its `-nosound` run establishes no auditory quality. Initial snapshots, renewal and replay do not trigger historical impact sounds. Typed release-author audio controls are volume0.65 [0,1], pitch variation0.06 [0,0.2], full-volume radius2m [0,10], maximum distance20m [2,60; greater than full-volume radius], concurrent impacts per resource4 [1,8], captured at resource BeginPlay for the next world.
Earlier two-rock images were an optional native-source-versus-procedural comparison, not two resources or a final destruction presentation. Their flat side face existed in the original intact art; rev02 removes it. Neither those frames nor headless checks establish professional visual quality.

## Design and integration boundaries

User direction is now explicit in GatheringÂ§1.5/Â§2.0 and Crafting C5.1: ordinary wild trees/freestanding rocks and all other gatherable families follow material-appropriate skill/tool progression, with accessible starting resources and tougher/mature/rare/magical types deeper into the world. Refining and recipes preserve distinct material identities. The two-node session pilot does **not** implement whole-world instance coverage, a regional material ladder or production persistence. [Session skill practice and requirement feedback](gameplay-gathering-progression-pilot.md) now implement a bounded first progression increment.

The [environment handoff](gameplay-environment-integration-handoff.md) separates gameplay and environment-owned files and records the agreed baseline merge boundary. [Concept alignment review](greenlands-concept-alignment-review.md) makes `Designer (6).png` the primary Greenlands visual target. The grey mechanics sandbox is not the finished biome.

Next: add higher-tier material identities with crafting uses; correct remaining source/material issues and add contact-aligned tool/character animation and controlled felling/stump behavior; verify updated two-client/late-join presentation and representative performance before environment integration. Current crown stays standing on depletion until proper felling exists; no visual completion, packaged/Linux or durable-world acceptance is claimed.
