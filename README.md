# Legends of the Shattered Frontier

The first alpha proof of concept for the vision in [concept.md](concept.md): a shared, persistent Greenlands sandbox. This is a browser test harness for gameplay and server rules. Unreal remains the proposed production client; this repository does not yet include an Unreal project.

## Play locally

New developer or new machine? Follow the [first-time setup requirements](docs/first-time-setup.md), including Unreal tooling and optional GitHub deploy-key setup.

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

```powershell
npm test
```

## What this proves

Server-authoritative actions, multiple browser clients receiving live world updates through server-sent events, shared resource depletion, persistent terrain edits, owned building pieces, use-based skills, crafting, NPC currency trade, and a starter quest. Terrain edits are tile changes; building is a wall-placement prototype.

## Alpha boundaries

This is a local development slice, not a public MMO server. It has no account recovery, combat, freeform voxel geometry, resource regeneration, housing interiors, player trading, offline simulation, or load validation. Player markers include saved characters who are offline. The map is a deterministic 32×32 tile region. Sessions use browser cookies; production authentication, action throttling, TLS, observability, and deployment hardening remain future work. The concept's concurrency and infrastructure estimates are unvalidated targets.

See [the alpha roadmap](docs/alpha-plan.md) for the next milestones.
