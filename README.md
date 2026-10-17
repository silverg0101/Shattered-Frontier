# Legends of the Shattered Frontier

**Legends of the Shattered Frontier** is a high-fantasy persistent-world MMO concept about exploration, player-built civilization, a player-driven economy, and territorial conflict. Players are meant to gather and craft, develop skills instead of choosing a fixed class, build settlements with their communities, and leave a lasting mark on a changing world.

The art below presents the long-term game vision. The playable code in this repository is an early browser proof of concept focused on a small shared Greenlands sandbox; it does not yet implement the large-scale features shown in the concept art. Unreal Engine is the proposed production client, and this repository does not currently contain an Unreal project.

## World and visual direction

The intended visual identity is high-fantasy realism at an epic MMO scale. Its central promise is a living world shaped by player exploration, construction, trade, crafting, alliances, and warfare. The launch-world vision spans four regions, with additional lands reserved for future growth.

| Concept | Art | What it communicates |
| --- | --- | --- |
| World overview key art | ![Adventurers overlooking the Shattered Frontier](docs/Game-Design/concept-art/Designer%20(9).png) | A panoramic world joining the Greenlands, Frozen North, Desert Wastes, and Corrupted Frontier. Rivers, settlements, trade routes, floating land, and distant volcanic corruption frame the promise of a persistent world shaped by its players. |
| Greenlands biome | ![Greenlands starter biome concept sheet](docs/Game-Design/concept-art/Designer%20(6).png) | The welcoming starter region: forests, rivers, waterfalls, fertile valleys, ruins, caves, wildlife, and resources. Silverbrook, Stonebridge, and Greenhollow represent settlement, trade, and farming. The biome introduces gathering, crafting, housing, trade, and exploration. |
| Character archetypes | ![Seven character archetypes](docs/Game-Design/concept-art/Designer%20(5).png) | Warrior, Ranger, Mage, Healer, Rogue, Craftsman, and Adventurer illustrate different progression paths. The design intent is no fixed classes: skills and choices define each character's identity. |
| Player city | ![Waterway city built by players](docs/Game-Design/concept-art/Designer%20(8).png) | A civilization built around waterways, bridges, and cliffside districts, including a guild citadel, market, crafting quarter, harbor, homes, temple, university, and defensive walls. Communities can create cities together. |
| Guild fortress | ![Guild stronghold on cliffs and waterfalls](docs/Game-Design/concept-art/Designer%20(1).png) | An endgame stronghold with a keep, guild hall, workshops, member quarters, walls, towers, gatehouses, a sky dock, and terrain-integrated waterfall defenses: a permanent community legacy. |
| World map | ![Launch regions and future expansion map](docs/Game-Design/concept-art/Designer%20(7).png) | The four launch regions are the civilized Greenlands, survival-focused Frozen North, exploratory Desert Wastes, and dangerous Corrupted Frontier. Emerald Archipelago, Jungle Continent, Sunken Realms, Shattered Isles, Celestial Expanse, and Eastern Continents suggest room to expand the world over time. |
| Siege key art | ![Armies assaulting a fortified city](docs/Game-Design/concept-art/Designer%20(2).png) | Guild armies, banners, archers, spellcasters, siege engines, defenders, fire, and destruction capture the intended stakes of territorial warfare: real players, kingdoms, alliances, and consequences. |

These images are concept direction, not a promise that the corresponding systems or locations are already playable. Together they set the aspiration: players build, fight, trade, craft, and shape the future of the world.

## Play the current prototype

This repository contains the first alpha proof of concept: a shared, persistent Greenlands sandbox in a browser. It is a test harness for gameplay and server rules, not a public MMO server.

Requires Node.js 24 or later. No dependencies or install step.

```powershell
npm start
```

Open http://localhost:3000 and create a character. Use a second browser profile or private window for a second player. Tabs in the same browser profile share one character cookie.

- Move with WASD or arrow keys. Stand beside resources and click to gather.
- Craft planks in the sidebar (2 wood each), select Build wall, then click adjacent empty grass.
- Place three walls and return to the gold settlement beacon for your founder reward.
- Dig grass for stone; sell three stone for two coins at the beacon.
- Reclaim your own walls to recover their planks.

World tiles, player inventory, skills, quest progress, wall ownership, and sessions survive server restarts in `data/world.sqlite`. Keep your browser cookie to return to your character. Back up the database with the server stopped, including its WAL files if present. The server binds to localhost.

## What the prototype demonstrates

Server-authoritative actions, multiple browser clients receiving live world updates through server-sent events, shared resource depletion, persistent terrain edits, owned building pieces, use-based skills, crafting, NPC currency trade, and a starter quest. Terrain edits are tile changes; building is a wall-placement prototype.

## Current boundaries

The prototype has no account recovery, combat, freeform voxel geometry, resource regeneration, housing interiors, player trading, offline simulation, or load validation. Player markers include saved characters who are offline. The map is a deterministic 32×32 tile region. Sessions use browser cookies; production authentication, action throttling, TLS, observability, and deployment hardening remain future work. The concept's concurrency and infrastructure estimates are unvalidated targets.

## Development

New developer or new machine? Follow the [first-time setup requirements](docs/first-time-setup.md), including Unreal tooling and optional GitHub deploy-key setup.

```powershell
npm test
```

See [concept.md](concept.md) for the broader game concept and [the alpha roadmap](docs/alpha-plan.md) for the next milestones.
