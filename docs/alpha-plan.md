# Alpha PoC scope and next milestones

The concept combines persistent sandbox freedom, approachable MMO progression, and editable environments. The first slice focuses on proving that gathering → crafting → building changes a shared world and survives a restart.

## Milestone 0 — Local shared-world loop (implemented)

One Greenlands region, authoritative action validation, SQLite snapshots, session resumption, multiplayer updates, gathering, crafting, claimed walls, tile excavation, use-based skills, NPC stone sales, and the Frontier Founder quest.

Acceptance: two browser profiles observe the same resource depletion and construction; another player cannot reclaim an owned wall; restarting the server preserves world and character state.

## Milestone 1 — Engine and movement slice

Confirm Unreal Engine and toolchain availability. Create a third-person Greenlands graybox with movement, camera, interaction targeting, and a dedicated server. Decide which prototype rules become engine code and which remain backend services. Do not assume the current HTTP tile protocol is the production replication architecture.

## Milestone 2 — Persistent housing and terrain

Add foundation, wall, door, and roof pieces; claims and permissions; bounded voxel/chunk terrain edits; persistence migrations and recovery; selective resource regeneration that preserves structures. Validate reconnects, duplicate commands, and conflicts between players.

## Milestone 3 — Playable group alpha

Add starter combat, one enemy, death/recovery, one short quest chain, player trading, and inventory limits. Run a small invited group before scaling. Measure simulation tick time, replication bandwidth, persistence latency, and client frame time.

## Milestone 4 — Capacity evidence

Add automated clients and test progressively at 10, 25, 50, then 100–200 concurrent players. Treat 100–200 as a gate requiring measured results, not an existing capability. Introduce regions, PostgreSQL, caching, and hosting only as measured needs justify them.

Art, music, additional biomes, guilds, raids, siege warfare, and world expansion follow after the core loop and persistence are reliable.
