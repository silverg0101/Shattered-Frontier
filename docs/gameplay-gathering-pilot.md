# Gathering gameplay pilot

Gathering checkpoint passed, 2026-10-18. Gameplay worktree: `codex/construction-network-members`. This checkpoint connects timed gathering to the free Fab [Minimal Inventory Runtime](https://www.fab.com/listings/c566626e-0375-4aa4-8bf3-38394b4f884c), rather than implementing another inventory. Source is already installed locally; no further Launcher installation was performed. The library page confirmed ownership during this session. Version 2.0.0 advertises UE 5.8/Win64. Vendor source remains outside Git; the repository contains our integration and two small project-authored raw-material definitions.

## Player flow and ownership

The opt-in dedicated construction world adds oak and iron nodes. Aim at a node and press E, stay still through the server-timed channel, then see the committed item count in your private bag. E/Escape cancel; focus loss requests cancellation. Moving, range/visibility loss, stale state, an occupied/depleted node or unavailable inventory denies/cancels without consuming a charge. A full/overweight bag leaves the entire charge available. Competing players share node depletion but receive only their own bag contents.

The owning controller is the command endpoint and owns the pilot inventory component (`USFInventoryComponent`, `Inventory/SFInventory.h`). The client supplies a node, expected node revision and operation sequence, never an item/yield/inventory/identity/time. Same-key replay retains the original outcome; changed-payload replay cannot grant another item. The server holds a node lock during the channel and rechecks pawn, location, LOS and admitted world at completion. The plugin performs the complete inventory grant; only success consumes a node charge. This synchronous in-memory sequence is not a durable cross-domain transaction.

Existing construction controls and unlimited construction stock remain separate. Both players may gather. E uses gathering when targeting a resource, otherwise the existing door path; it is consumed before the inherited probe binding.

Required presentation: [gathering quality and material claiming](Game-Design/Legends_Shattered_Frontier_Gathering_Presentation_and_Material_Claiming_v0.1.md) governs the first tree/rock art packet. Contact animation, staged extraction, felling/stump, interior materials, sound/VFX and confirmed collection feedback remain undelivered; primitive fixtures and headless authority checks cannot accept visual quality.

## Scope and design limits

This is a session-only development loop. The [tool follow-up](gameplay-gathering-tools-pilot.md) now enforces the released axe/pickaxe category requirement; authenticated character identity, skill/combat checks, quality-bearing item instances, progression receipts, durable harvest receipts, mailbox overflow and production World regeneration are not implemented here. The [lifecycle follow-up](gameplay-gathering-lifecycle-pilot.md) adds session-only renewal and Blueprint appearance hooks. Do not mark INV-02/GAT-01/GAT-02 production acceptance complete. The governing [gathering design](Game-Design/Crafting/Legends_Shattered_Frontier_Crafting_Gathering_and_Resources_v1.0.md) and [Itemization](Game-Design/Legends_Shattered_Frontier_Itemization_Architecture_v1.0.md) remain unchanged. The vertical-slice no-loss capacity behavior is used for this pilot; later mailbox overflow needs its domain integration.

The plugin's built-in save/load, automatic startup grants, tracing, prompts and automatic UI are disabled. The tool follow-up enables only the `tool` equipment slot and grants the explicit development loadout through an atomic server transaction. Its replicated contents live on an owner-relevant controller; public node state contains charges/revision/busy only. The optional plugin adapter compiles on Win64. Other platforms keep core compilation independent and report the inventory adapter unavailable; Linux inventory support is not established. Do not remove the vendor platform restriction or claim Linux acceptance without a separate port/build decision.

## Configuration and files

`USFGatheringPilotSettings` / `DefaultGame.ini` are typed, release-author controlled, next-world-start settings, with no live console overrides: channel 3 s [0.5,30], pawn-origin reach 3.5 m [0.5,5], movement cancellation 0.05 m [0.01,0.25], charges 3 [1,12], yield 1 [1,10], product <=30 units/node, bag 8 slots [1,64], bag weight 24 kg [1,1000]. Material content-generation defaults are 20 units/stack [1,100] and 1 kg/unit [0.01,50]; changes require a compatible content release and do not rewrite existing definitions at runtime. Production controls must use the shared configuration workflow.

Runtime: `Source/ShatteredFrontier/{Public,Private}/Gathering/SFGatheringPilot.*`; construction mode/controller only host and dispatch the optional pilot. `ShatteredFrontier.Build.cs` and `.uproject` declare the Win64 dependency. `tools/Create-GatheringPilotItems.py` creates new definitions under `/Game/Frontier/Gameplay/Pilot/` and refuses any overwrite. Raw IDs are `item/oak_log` and `item/iron_ore`; refinement is the next increment, using the design's rough timber and iron ingot recipes.

After adding the free plugin to the Fab library, install its UE 5.8 version through Launcher on a new machine. Build the editor before running the asset generator. Existing project-authored assets travel through Git; vendor code/binaries do not. Use the isolated construction map/game-mode URL from the laptop handoff plus server flags `-ConstructionNetworkSandbox -GatheringPilot`; clients join its loopback address normally. This does not change startup maps or connect the LAN service.

## Silverbrook zone run (owner opt-in, 2026-10-20)

The same development pilot also runs in `/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook`. The server needs that map with `?game=/Script/ShatteredFrontier.SFConstructionNetworkGameMode` and `-ConstructionNetworkSandbox -GatheringPilot`; without `-GatheringPilot` the zone is not admitted.

How it differs from the sandbox:
- **Node source.** Instead of the four fixed fixtures, the server reads the zone record's gathering `spawn_groups` (`content/world/zones/greenlands_silverbrook.json`). It spawns `active_count` points per group, chosen by a seed from the group id: 14 nodes, wood as oak and stone as the rock node.
- **Stone.** Zone stone groups spawn the `Stone` kind: the rock art and a pickaxe, mining practice, and `item/rough_stone`, the tier-one mining stone in Crafting Gathering. `tools/Create-GatheringPilotItems.py` now creates only missing definitions and never overwrites. The sandbox keeps its iron node.
- **No refinery.** There are no sawmill or forge stations in the zone.
- **Dressing cleared.** Each node removes the dressing tree or rock rooted within 1 m of it, on the server and on every client. The dressing art and collision therefore never overlap a node, and line-of-sight checks don't hit them.
- **Replication range.** Nodes replicate out to 500 m, so the spot never looks empty at dressing distances.
- **Player starts.** Players keep the map's arrival start. Construction controls come along, as in the sandbox.

What it still lacks:
- World slot rotation, respawn at other points and persistence.
- Packaged builds: the record is read from the source tree.

Evidence:
- `tools/Test-GatheringPilot.ps1 -Silverbrook` passed (2026-10-20). Each client harvested its node: wood for the primary, ore for the observer. Each saw only its own bag, and the replayed operation granted nothing.
- The sandbox gathering and `-Refining` runs and `ShatteredFrontier.Gathering.Pilot` still pass.

## Evidence and next step

Final editor build04 passed in 39.29 s. Focused authority report02 passed 1/1 clean, including loss-of-admission cancellation. The two-client run `Artifacts/ConstructionSandbox/gathering-network-20261018-230502-852/` passed: primary bag wood1/ore0; observer bag wood0/ore1; each client sees exactly one inventory; both nodes reach revision1; server commits exactly two grants. Replaying operation1 grants nothing extra. Both clients exited0 and owned server cleanup completed. Review found no remaining actionable authority/lifecycle issues after finite-coordinate and world-admission corrections.

The first network attempt canceled harvesting while the spawned character settled onto the floor. The fixture now waits for grounded/stationary state; gameplay still cancels movement. Initial build01 compiler issues were corrected; no package, native input or Linux acceptance is claimed. [Compact evidence and source hashes](evidence/gathering-pilot-results.json).

Run only `ShatteredFrontier.Gathering.Pilot.Authority` for authority checks. `tools/Test-GatheringPilot.ps1` launches one owned loopback editor server/two clients, checks both private grants, replay and public node revisions, then cleans only its processes. It uses synthetic resource targeting through the real owning-component RPC path; it does not prove native E input, rendered acceptance, durable recovery or Linux support.

Follow-up: [sawmill and forge refinement](gameplay-refining-pilot.md) now passes through the same server-owned recipe/transaction boundary. Keep free component selection tied to the [candidate register](Game-Design/Legends_Shattered_Frontier_Gameplay_Reuse_Candidate_Register_v0.1.md); do not substitute a kit's recipes or construction rules.
