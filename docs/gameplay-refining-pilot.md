# Sawmill and forge gameplay pilot

Checkpoint passed, 2026-10-18; follows gathering commit `4311742` in `codex/construction-network-members`. The main checkout's environment/runtime edits remain separate.

The [recipe design, section 9.1](Game-Design/Crafting/Legends_Shattered_Frontier_Crafting_Recipes_Workshops_and_Learning_v1.0.md) supplies the shipped recipes: one `item/oak_log` becomes four `item/rough_timber`; one `item/iron_ore` becomes one `item/iron_ingot`. These are project recipes, not imported kit rules.

The opt-in world adds a sawmill and forge. Target the station and press E; a server-timed channel uses the same cancellation, range, LOS, lock, revision and request-replay checks as gathering. The complete ingredient/output change goes through the free inventory plugin's transaction API. Preflight explains missing materials or capacity; completion checks again. Denied/canceled work consumes nothing. Stations release their lock and advance revision after success; they do not deplete. The accepted station kind is captured; changing it mid-channel cancels before cost/output mutation. Recipe replay returns the original outcome.

`USFGatheringPilotSettings` declares release-author-only, next-world-start defaults: refinement 3 s [0.5,30]; timber input 1 log [1,20], output 4 units [1,20]; ingot input 1 ore [1,20], output 1 unit [1,20]. Existing bag/reach/movement settings remain in the [gathering checkpoint](gameplay-gathering-pilot.md). No new live/console tuning path is introduced.

`Gathering/SFGatheringPilot.*` now serves resource and workstation kinds through one owner-controller endpoint; the server resolves recipe and quantities. `tools/Create-RefiningPilotItems.py` creates only new project-owned RoughTimber/IronIngot definitions, refusing overwrite. It reads the typed material content defaults; generated assets are under `/Game/Frontier/Gameplay/Pilot/`. Vendor source stays external to Git.

The pilot still uses practice tools/workshops. Recipe knowledge, difficulty/success/quality, workshop tiers/ownership, progression and durable Itemization/Crafting contracts remain open. No production crafting ticket is marked complete. Construction retains unlimited development stock; these materials are not silently charged by its separate prototype.

Evidence: final editor build03 passed in 8.74 s; `Artifacts/ConstructionSandbox/refining-pilot-authority-01/Report/index.json` passed the extended authority case 1/1 clean. `tools/Test-GatheringPilot.ps1 -Refining` passed in `Artifacts/ConstructionSandbox/gathering-network-20261018-231605-642/`: primary ended with timber4/ingots0; observer timber0/ingots1; both had raw wood0/ore0, and both workstation revisions were1. Server recorded two harvest and two refinement commits; replays added nothing. Both clients exited0 and owned server cleanup completed. Independent review cleared captured recipe binding and atomic transaction handling. [Compact evidence/source hashes](evidence/refining-pilot-results.json).

The asset generator initially used a nonexistent Python class wrapper/property spelling; explicit reflected class/native property lookup corrected it before any asset write. Output definitions were then generated successfully. No vendor code was copied or changed.

Follow-up: [equipped gathering tools](gameplay-gathering-tools-pilot.md) now enforce logging/mining categories. The default desktop inventory window is held for an in-game UMG/HUD route; continue the resource loop without adopting that presentation. Keep checks at substantive integration boundaries.
