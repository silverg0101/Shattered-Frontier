# Legends of the Shattered Frontier — Settlement: Silverbrook
### Version 0.1 — Settlement definition (2026-10-19)

The first settlement definition written to the [Settlement Design Board Standard](../World-Simulation/Legends_Shattered_Frontier_Settlement_Design_Board_Standard_v1.0.md). Silverbrook anchors `zone/greenlands_silverbrook` (Starter, levels 1–8) in the [Greenlands region](Legends_Shattered_Frontier_Region_Greenlands_v0.1.md). Zone rules come from the [Zone Definition Standard](../World-Simulation/Legends_Shattered_Frontier_World_Zone_Definition_Standard_v0.1.md). Items marked **proposed** need confirming in a graybox traversal test.

## 1. Role

Silverbrook is the primary starter settlement of the Greenlands and the first major player hub. Players arrive at the edge of the zone, learn the basics on the approach, and reach the town as their first hub.

It should feel welcoming, memorable, easy to navigate and instantly recognisable. Its layout teaches players the core systems through exploration: gathering, crafting, trade, housing and the first fights.

## 2. Design board

| Item | Value |
| --- | --- |
| Adopted board | None yet. The Silverbrook starter town map image is awaited. |
| File when adopted | `docs/Game-Design/concept-art/Silverbrook-Design-Board-v1.webp` |
| Brief | §12 |

## 3. Architectural style

- **Style:** high-fantasy medieval realism. Buildings look grounded and believable.
- **Visual references:** World of Warcraft, New World, The Elder Scrolls Online and The Lord of the Rings, for mood and readability. Do not copy any of them.
- **Materials:** timber, stone foundations, plaster walls and slate roofs, with blue and gold banners.

This is the region's half-timbered settlement style at town scale. Silverbrook's banners carry the blue and gold of the Greenlands concept sheet.

## 4. Geography

- **Setting:** Silverbrook sits where its namesake brook meets the main river from the northern highlands, at the confluence of two rivers. The River Bridge crosses below the confluence.
- **Surroundings:** rolling hills, pine forests, oak forests, farmland and wildflower meadows, with snow-capped mountains in the distance to the north.
- **Climate:** temperate, fertile and prosperous.

The region map places Silverbrook in the north-west of the mainland, with Silvermere to the north, Everwood to the south-west and Valemere to the south-east.

## 5. Scale

| Ring | Radius | Meaning |
| --- | --- | --- |
| Town core | About 350 m from Town Square | All districts and key buildings. It follows from the one-minute rule in §8. |
| Playable radius | 750 m | The town plus nearby farms, gathering areas and the first fights. Most early play happens here. |
| Settlement radius | 1,250 m | The zone's countryside out to its borders. It sets the zone extent at about 2.5 × 2.5 km (**proposed**). |
| Influence radius | 5,000 m | How far Silverbrook's routes, quests and trade reach into neighbouring zones. It places no geometry outside this zone. |

- **Resident population:** 350–500. This is the in-world population, used for building count and density. The number of NPCs actually spawned is a gameplay metric, decided when NPCs are placed. Player capacity is the zone's (200 soft / 250 hard).
- **Zone extent:** the 2.5 km extent is larger than the standard's 2 km starting point. The standard allows that once a graybox traversal test confirms it.

## 6. Districts

Districts are **areas** inside the zone (checklist Z5). Each one is a named area with its own purpose and safety rule.

| District | Contains | Slice role |
| --- | --- | --- |
| Town Square | Great Oak Tree, Central Fountain, notice boards, gathering area, seasonal event space | Settlement beacon |
| Market District | General merchant, food vendors, trade brokers, supply vendors, equipment vendors | Market access |
| Adventurer District | Adventurer Hall (guild and quest hub), recruitment boards, training grounds | Quest contact; starter combat practice |
| Crafting District | Blacksmith, carpenter, tailor, alchemist, cook, smelter | Crafting stations |
| Residential District | Starter homes, gardens, decorative lots, community spaces | Build plots, at its outer edge (**proposed**) |
| Stable District | Mount vendor, mount trainer, animal pens, wagon storage | Mounts and companions |
| Inn District | Tavern, rental rooms, social areas, NPC storytelling areas | Social hub |

- **Safety:** the town core is a protected, no-edit area.
- **Player plots:** player claims sit outside the protected core, on the residential edge, so the core stays protected (**proposed**).

## 7. Landmarks

Each landmark is a point of interest (Z6) and must be visible from at least one major route:

- Adventurer Hall
- Central Fountain
- Great Oak Tree
- River Bridge
- Watchtower (the Northern Watchtower; it is also the slice's return overlook)
- Marketplace

At least one of them must be visible from the arrival point.

## 8. Gameplay design rules

- **No minimap needed:** players can find their way using landmarks, roads and signposts alone.
- **One-minute rule:** every important building is within one minute's walk of Town Square. At the current run speed of 7 m/s (`RunSpeedMetresPerSecond`), that is about 420 m of path, or about 350 m in a straight line. If the run speed changes, recheck this.
- **Supported systems:** housing, guilds, the economy, seasonal events and future expansion.

## 9. Roads

| Tier | Surface | Use |
| --- | --- | --- |
| Primary | Paved stone | Town Square to the gates, and the roads out to Silvermere, Everwood and Valemere |
| Secondary | Packed earth | Between districts, and out to the farms |
| Rural trails | Natural dirt | To gathering areas, camps, ruins and the cave |

Roads lead players naturally toward gameplay. Every road that reaches the zone border meets a `border_entry` anchor (Z3, Z7).

## 10. Exterior

All of these sit within the 1,250 m settlement radius.

| Content | Placement |
| --- | --- |
| Farms | Southern Farms, along the river flats and secondary roads |
| Gathering areas | Timber, stone, herbs and fibre within the playable radius. The resource route of the slice. |
| Fishing locations | Both rivers, away from the bridge |
| Boar territories | Forest and farm edges. This is the slice's enemy clearing and the starter combat area. |
| Wolf dens | Beyond the playable radius, in packs. Kept away from the arrival area, per the region definition. |
| Bandit camps | Beyond the playable radius, on rural trails |
| Small ruins | Points of interest on trails and hilltops |
| Hidden cave | A point of interest inside the zone |
| Future dungeon entrance | A `dungeon_entrance` point of interest linking to an instance (Z13), reserved for later |

## 11. Required outputs

Each output feeds a zone checklist row or a board section.

| Output | Zone checklist | Board section |
| --- | --- | --- |
| Settlement and district layout | Z5 | 1, 2 |
| Terrain plan and elevation markers | Z2 | 1 |
| Road network and travel routes | Z3, Z7 | 1, 4 |
| River network | Z7, Z12 | 1 |
| Landmark placement | Z6 | 1, 2 |
| Building footprints | Z5 | 1, 3 |
| Housing plots | Z10 | 1 |
| Future expansion areas | Z10 | 1, 6 |
| NPC activity areas | Z9, Z14 | 6 |
| Resource node placement | Z8 | 1 |

All outputs must be ready to implement in Unreal Engine 5.8.

## 12. Design board brief

This is the brief for generating or painting the board. Keep it in step with §3–§10.

> Create a professional AAA game development design board for Silverbrook, the starter settlement of the Greenlands in Legends of the Shattered Frontier. It must read as a production-ready environment design package for Unreal Engine 5.8 world builders, level designers, environment artists, Blender artists and Pixel3D asset creators.
>
> **Style:** fantasy medieval realism, presented as AAA development documentation that is highly readable, technical but beautiful, and a source-of-truth production board.
>
> **Canvas:** 4096 × 4096, square.
>
> **Colours:** Greenlands colours, blue, gold, forest green and natural parchment accents.
>
> Divide the board into clearly labelled sections:
>
> 1. **Top-down world map.** A detailed regional map of Silverbrook showing rivers (the confluence of two rivers), roads, bridges, forests, farms, hills, gates, districts and expansion areas. Label Town Square, Market District, Adventurer Hall, Crafting District, Inn District, Stable District, Residential District, Northern Watchtower and Southern Farms. It should resemble a modern MMORPG zone map.
> 2. **Isometric settlement view.** A realistic overview matching the top-down map, with accurate road placement, the districts, the Central Fountain, the Great Oak Tree, Adventurer Hall, the Marketplace, the stables and the crafting district. It should feel alive and believable, at realistic scale, in medieval fantasy realism.
> 3. **Building reference library.** Front elevations of the town hall, Adventurer Hall, blacksmith, inn, stable, watchtower, market stall, cottage, workshop and storage building. Each is a clean architectural reference suitable for Blender modelling, showing materials and structural details.
> 4. **Gameplay infrastructure.** Roads, bridges, signposts, lamps, wells, fences, barrels, crates, market props and crafting stations, as reusable environment art references.
> 5. **Biome references.** Pine forests, oak forests, meadows, riverbanks, farmland, wildflowers and local rock formations, with seasonal visual guidance.
> 6. **Development notes.** Annotated callouts around the board: settlement hierarchy, district functions, population (350–500 residents), expansion areas, navigation routes, architectural rules and environmental rules.
>
> **Visual references:** World of Warcraft, New World, The Elder Scrolls Online and The Lord of the Rings.
>
> The result should look like a professional internal game development document and serve as the canonical design board for Silverbrook in Unreal Engine 5.8.

Changes from the original brief:
- **Label names:** labels use the §6 district names, so "Crafting Plaza" became Crafting District and "Residential Quarter" became Residential District.
- **"Zones":** "expansion zones" became expansion areas, because in this project a zone is a server map.

## 13. Open decisions

1. **North:** which world axis is north in Unreal. Decide this before placing the rivers and the roads out.
2. **Walls and gates:** the board standard expects walls and gates to be shown, but the region definition gives stone walls only to Valemere and Eldenfall. Decide whether Silverbrook has a palisade with gates, gates only, or neither.
3. **Zone extent:** confirm the 2.5 × 2.5 km extent from §5, or shrink the settlement radius to fit 2 km.
4. **Player plots:** whether claims stay outside the protected core, as proposed in §6.
5. **NPC count:** how many NPCs actually spawn compared with the 350–500 residents.
