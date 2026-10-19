# Legends of the Shattered Frontier — Zone Definition Standard
### Version 0.1 — Authoring standard (2026-10-19)

## Purpose

This standard explains what a zone is and what every region must define before its zones are built. It turns the World Simulation contracts into an authoring checklist. The [World Simulation Architecture](../Legends_Shattered_Frontier_World_Simulation_Architecture_v1.0.md) and its parts remain the authority for fields, limits and runtime behaviour. Where this document and those parts disagree, those parts win and this document is corrected.

Use it in this order:

1. Write a region definition from the region's concept art (the [Greenlands region](../Regions/Legends_Shattered_Frontier_Region_Greenlands_v0.1.md) is the first one).
2. Split the region into zones with the decision rules below.
3. For each zone anchored by a settlement, write a settlement definition and its design board per the [Settlement Design Board Standard](Legends_Shattered_Frontier_Settlement_Design_Board_Standard_v1.0.md). Its districts become areas in this zone.
4. Complete the zone checklist for each zone before it leaves graybox.

## Terms

| Term | What it is | Runtime meaning | Example |
| --- | --- | --- | --- |
| **Biome** | An ecological and art family: terrain, foliage, wildlife, palette, building style. | An attribute of a region. No server, no map. | `greenlands` |
| **Region** | A named piece of geography with its own story, factions and map. What players call "a land". | `world_region` content record. Faction control is keyed by region. | The Greenlands |
| **Zone** | One continuous playable map simulated by one `zone-server` process (per layer). | `zone` content record with a stable `zone_index` and a `map_id`. | Silverbrook and its surrounding countryside |
| **Area** | A named part of a zone with its own purpose, rules or content level. | A `level_area` polygon and/or a volume (`protected`, `no_edit`, `no_mount`, `speed`, `water`, `roof`). | Silverbrook build plots |
| **Point of interest** | A place a player can discover. | `poi` record: position, radius, `poi_kind`, `hidden`. | Old watchtower ruin |
| **Settlement** | A town or village. Players see it as a "key location". | A `poi` of kind `settlement`, plus the areas and NPCs it contains. Usually the anchor of one zone. | Valemere |
| **Route** | A road, river path or trade line between places. | `route` record. Routes that cross a border must meet at the border's `border_entry` anchors. | North road |
| **Instance** | A separate map for a dungeon, raid or private space, entered from a zone. | `instance-server` map with its own `zone_index`. Not part of a zone. | A cave dungeon entered from Stonehollow |

The [vertical slice](../Legends_Shattered_Frontier_Vertical_Slice_Specification_v1.0.md) uses "zones" loosely for its arrival, settlement, resource, build, enemy and overlook spaces. Under this standard those are **areas** inside one zone.

## Is it a zone, an area or an instance?

Make a place its own **zone** when at least one is true:

- It needs its own population budget: more than about 200 concurrent players at peak (`capacity_soft`), or a different layering rule.
- It has a different level range from its neighbours.
- It would push its parent zone past the size or budget below.
- It has its own identity that players travel *to*, such as a settlement and its countryside.

Make it an **area** inside a zone when it shares the zone's population and server, and differs only in purpose, rules or content level.

Make it an **instance** when it needs a private, resettable or party-scoped copy, such as a dungeon, raid or story scene.

Water that players can't cross on foot (a sea, an island chain) separates zones but is not a border. Travel across it needs a route type the game doesn't have yet, such as ships. Areas reached that way are future zones, not part of the mainland build.

## Size and budgets

| Item | Value | Source |
| --- | --- | --- |
| Typical zone footprint | About 2 km × 2 km (about 1,000 cells) | Terrain Part 3, Cell activation |
| Cell | 64 m square, never authored | Cells Part 2 |
| Capacity | 200 soft / 250 hard players | Core model (`tune.world.zone_capacity_*`) |
| Ghost band | 48 m each side of a border | Cells Part 2, Border model |
| Combat-free border margin | No spawn or event area within 64 m of a border | Cells Part 2, Border model |
| Edit-free border margin | No terrain edits within 8 m of a border | Cells Part 2 (`tune.world.border_edit_exclusion_m`) |
| Layering | Off for any zone with claimable land | Cells Part 2, rule L1 |

These are design starting points. Exact sizes are confirmed by a graybox traversal test, as the vertical slice requires.

## Zone archetypes

Each zone gets one primary archetype. It decides the defaults for safety, layering, claims and content density.

| Archetype | Purpose | Claimable land | Layerable | Typical areas |
| --- | --- | --- | --- | --- |
| **Starter** | First arrival, tutorial loop, first home | Yes (small plots) | No, because of the claims | Arrival, settlement, resource route, build plots, starter enemy, overlook |
| **Hub / capital** | Social, trade, quests, services | No | Yes | Market, crafting quarter, quest givers, travel links |
| **Frontier / wilderness** | Exploration, gathering, combat | Yes (open claims) | No | Resource sites, ruins, caves, camps, events |
| **Resource** | Concentrated gathering for a material family | Optional | Only if no claims | Quarries, mines, groves, refineries |
| **Border** | Joins to another biome or region | Optional | No | Transition terrain, gate or pass, a cross-region route |

## Zone checklist

A zone is ready to leave graybox when every row has an answer. Rows marked *record* map to fields in the World Simulation `zone` record.

| # | Item | Notes |
| --- | --- | --- |
| Z1 | Identity: `zone/<snake>` id, display name, region, archetype | *record*. `zone_index` is assigned once when the content file is first committed and never reused. |
| Z2 | Geography: `origin`, `extent`, polygon shape, `default_spawn`, `respawn_points[]` | *record*. Origin is in continent-space metres. |
| Z3 | Neighbours: `neighbors[]` with border polylines and `border_entry` anchors | *record*. Borders run through calm terrain, never through a settlement or a combat area. |
| Z4 | Level: `level_range` and `level_areas[]` | *record*. Every spawn area, event area and open-world dungeon arena is inside a level area. |
| Z5 | Areas: named areas with purpose, safety rule and volumes | `protected`, `no_edit`, `no_mount` and so on. Safety is enforced on the server, never as a client visual only. |
| Z6 | Settlements and points of interest | At least one landmark visible from the arrival point. Each `poi` has a kind and a radius. |
| Z7 | Routes | Roads and rivers that connect areas and reach every border anchor. |
| Z8 | Resources: `spawn_groups[]` for gathering nodes | Material family, tier, points, `active_count`. Gathering groups need at least 2 × `active_count` points. |
| Z9 | Wildlife and enemies: `spawn_groups[]` for NPCs | Threat level per species. Tier `rare`+ spawns use `primary_only`. |
| Z10 | Claims and building | Where claimable land is, plot sizes, and where building is forbidden. |
| Z11 | Capacity and layering | `capacity_soft`/`capacity_hard` if different from defaults; `layerable` per rule L1. |
| Z12 | Environment: biome kit, environment themes, weather and day/night overrides | Lists the shared art library it uses. Zone-only assets are the exception. |
| Z13 | Instance entrances | Each one is a `poi` of kind `dungeon_entrance` linking to an instance map. |
| Z14 | Content level areas for quests and events | Where quest and world-event content may run. |

## Region definition checklist

Before any zone in a region is built, the region definition records:

1. **Source of truth.** The concept art and the date it was adopted. Superseded art and retired place names are listed so they are not reused by mistake.
2. **Identity.** Region id, biome, player-facing pillars and tone.
3. **Geography.** Coasts, seas, mountains, rivers, roads and neighbouring regions, read from the concept map.
4. **Key locations.** Each settlement with its role and archetype.
5. **Zone plan.** Which zone each key location anchors, the zone adjacency graph, the starting zone, and which places are future (for example, reached only by sea).
6. **Shared content.** Environment themes, wildlife roster, resource families and settlement style, with the zones each appears in.
7. **Build-out waves.** Which zones ship in which milestone, starting with one zone.
8. **Open decisions.** Anything the concept art doesn't settle.

## How zones map to the Unreal project

- **One project, one server build.** Every zone, instance and region lives in `GameClient/ShatteredFrontier.uproject`. The same `zone-server` build runs every zone. There is no per-zone or per-region project.
- **One map per zone.** Each zone record's `map_id` points at exactly one map. Proposed path: `/Game/Frontier/Maps/<Region>/<Zone>`, for example `/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook`.
- **One shared art library per biome.** Meshes, materials, foliage and building kits shared by every zone in a biome live in `/Game/Frontier/Environment/<Biome>/`. Assets used by one zone only are allowed, but are the exception and live under that zone's folder.
- **Zone data outside the map.** The zone record, areas, points of interest, routes and spawn groups are content data under `content/world/{continents,regions,zones,pois,spawn_tables}/` ([World agent guide](Legends_Shattered_Frontier_World_Agent_Implementation_Guide_v1.0.md)). The map supplies geometry and markers; the content record is what servers and tools trust. Persistent references use content ids, never Blueprint or map paths.
- **Test maps are not zones.** Graybox, sandbox and review maps stay under `/Game/Frontier/Maps/Test/` or their feature folder, and are never named as a production zone.
- **Not decided yet.** How a zone is structured inside Unreal (World Partition, sublevels or level instances), and how a client loads a neighbouring zone's geometry inside the ghost band. Decide both before a second zone is built.

## Zone maturity stages

Status belongs in the zone's content record and its region definition, not in folder or asset names. Don't name folders or maps "Preview", "Final" or "V2"; the name stays the same as the zone matures.

| Stage | Meaning | Exit check |
| --- | --- | --- |
| **Concept** | The region definition lists the zone. | Checklist rows Z1, Z3 and Z5 have proposed answers. |
| **Graybox** | A blocked-out map with real scale, routes and areas. | Traversal test passes; sizes and budgets confirmed. |
| **Playable** | Gameplay systems run in the zone on the dedicated server. | Focused gameplay checks pass in this zone. |
| **Art pass** | Biome art replaces the block-out. | Rendered review against the region concept art. |
| **Production** | Ready to ship in a release. | Cooks with no missing references, including licensed content restored on build machines; capacity measured. |

## Open decisions

1. The Unreal structure of a zone map and cross-border geometry loading (above).
2. Whether a large region is one `world_region` or several. Faction control is keyed by region, so the answer depends on how finely factions contest territory.
3. A route type for sea travel to island zones.
