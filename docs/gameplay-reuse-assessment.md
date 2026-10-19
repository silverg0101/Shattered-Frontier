# Gameplay reuse checkpoint

2026-10-18. User prioritizes development progress and asked about existing Blueprints before more custom gameplay implementation. No gameplay Blueprint `.uasset` files were found in this gameplay checkout; the project uses one C++ runtime module and engine components. Only UE5.8 is installed under the inspected Epic installation directory; library ownership is unverified.

| Candidate | Documented offering | Fit still to establish |
| --- | --- | --- |
| [Epic Lyra inventory/equipment](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-inventory-and-equipment-in-unreal-engine) | Controller-owned inventory, pawn equipment, pickup examples and widget Blueprints | Requires sample source/assets; check framework dependencies and Itemization ownership before migration |
| [Survival Game Kit V2](https://defusestudios.com/survival-game-kit-v2-feature-list/) | Multiplayer inventory, crafting, harvesting and modular building; interactive and tool-hit resource examples | Licensed assets/source and exact UE5.8 compatibility unverified; its prefabricated building/support model is not the project's canonical voxel contract |

Direction: assess available owned packs or Epic sample components before writing the INV/GAT loop. Preserve the custom construction implementation and environment chat's assets. Blueprint server logic is possible; C++ is not a requirement for multiplayer authority. Reuse decisions must retain one inventory mutation owner and existing server admission rules. No purchase, asset import, framework replacement or new inventory code has occurred. Question pending: existing owned packs / Epic-first / paid candidates.

[Lyra acquisition instructions](https://dev.epicgames.com/documentation/unreal-engine/lyra-sample-game-in-unreal-engine?lang=en-US) use Fab/Launcher. No sample or kit has been downloaded. Documentation claims establish candidates, not successful integration or performance.
