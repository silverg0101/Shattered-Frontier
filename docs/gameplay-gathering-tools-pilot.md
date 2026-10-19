# Equipped gathering tools pilot

Checkpoint passed, 2026-10-18, following gathering/refining commits `4311742` / `9299a6b` in the isolated gameplay worktree.

The [gathering design, section 4](Game-Design/Crafting/Legends_Shattered_Frontier_Crafting_Gathering_and_Resources_v1.0.md) requires an equipped axe for logging and pickaxe for mining. The pilot now uses the free inventory plugin's real equipment handling instead of assuming a practice tool. Press 5 for an owned iron axe, 6 for an owned iron pickaxe, then E to gather. The HUD reads replicated equipment; a locally queued request is not an authoritative equipment result.

An admitted server controller receives one of each tool through a single all-or-nothing inventory transaction. A per-controller completion flag prevents repeat initialization from duplicating tools. Missing definitions/capacity fail the feature closed without a partial loadout. This is an explicit development starter grant per session/controller, not account entitlement or a production world faucet.

The server checks its own `tool` equipment slot, quantity one and the two released tier-one definitions at channel admission and advancement. The client supplies no tool/category/tier/harvest yield. Switching away from the required tool cancels before output or depletion; normal key switching also sends cancellation before the equipment request. Stations retain their existing practice-workshop scope.

`bGrantStarterTools` is a release-author-controlled startup bool (default true). `ToolUnitWeightKg` is a typed content-generation default 2 kg [0.01,50], applied to newly generated definitions in a compatible content release. Nonstackable tool size1 and the two starter definitions are versioned content, not console expressions. The existing bag settings/capacity checks apply.

`tools/Create-GatheringPilotTools.py` creates only new `/Game/Frontier/Gameplay/Pilot/DA_IronAxe` and `DA_IronPickaxe` assets (`item/iron_axe`, `item/iron_pickaxe`), with use type Equip and slot `tool`, refusing overwrite. The plugin owns equipment swap/removal/return operations. No vendor source changed.

This implements the common-node category requirement for these two definitions. Durable tool instances/provenance, wear/repair, profession stats, progression, other tiers and public held-tool appearance remain open. No approved tool models are imported. Inventory contents/equipment remain on the owner controller in this pilot; INV-03 public appearance acceptance is not claimed.

Evidence: editor build01 passed in 27.78 s; both tool assets generated successfully. `Artifacts/ConstructionSandbox/gathering-tools-authority-01/Report/index.json` passed Authority and Tools 2/2 clean. The full two-client gather/refine scenario passed in `Artifacts/ConstructionSandbox/gathering-network-20261018-233210-856/`, equipping actual owned tools first. Primary ends with timber4/ingots0, observer timber0/ingots1; both raw materials zero; replay adds nothing. Clients exited0; owned server cleanup completed. Independent review found no blocking tool/loadout/lifecycle issue. [Compact evidence/source hashes](evidence/gathering-tools-pilot-results.json).

UI fit decision: inspected vendor Slate inventory creates a separate desktop window. It is not adopted as the game's bag presentation; use the documented existing-HUD/UMG route when building that view. This holds a presentation candidate rather than changing the game's interaction model for convenience.

Follow-up: [resource lifecycle and presentation hooks](gameplay-gathering-lifecycle-pilot.md) implements session-only recovery. Next visual work must meet the [gathering quality contract](Game-Design/Legends_Shattered_Frontier_Gathering_Presentation_and_Material_Claiming_v0.1.md). Production World regeneration remains separate.
