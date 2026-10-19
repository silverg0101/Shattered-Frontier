# Legends of the Shattered Frontier — Settlement Design Board Standard
### Version 1.0 — Authoring standard (2026-10-19)

## Purpose

Each settlement gets one **design board**: a single production image that world builders, level designers, environment artists, Blender modellers and Pixel3D asset creators work from. It covers world partition planning, landscape generation, level design, environment art, Blender and Pixel3D asset creation, quest design, NPC placement, housing, economy, navigation, landmarks and future expansion.

The board is a working document, not marketing art. Readability, consistency and buildability come before presentation.

## Where it fits

A settlement is defined in three layers. Each answers different questions, and conflicts resolve in this order:

| Layer | Holds | Wins on |
| --- | --- | --- |
| [Region definition](../Regions/Legends_Shattered_Frontier_Region_Greenlands_v0.1.md) and its concept sheet | Geography, settlement names and locations, neighbours, archetype | Where the settlement is and what it is called |
| **Settlement definition** (text, per settlement) | Scale in metres, districts, landmarks, rules, exterior content, board brief | Every number, name and requirement |
| **Design board** (image, per settlement) | Look, layout intent, architecture, materials, biome mood | Visual direction |

A board is usually generated or painted, so its distances are approximate. When the board and the settlement definition disagree, the definition wins; regenerate or annotate the board. The board never overrides the region map or the [Zone Definition Standard](Legends_Shattered_Frontier_World_Zone_Definition_Standard_v0.1.md): districts on a board are **areas** inside a zone, and the zone's budgets, borders and checklist still apply.

## Files

| Item | Location |
| --- | --- |
| Settlement definition | `docs/Game-Design/Regions/Legends_Shattered_Frontier_Settlement_<Name>_v<version>.md` |
| Board image | `docs/Game-Design/concept-art/<Name>-Design-Board-v<n>.webp` |
| Board brief (the generation or painting prompt) | A section in the settlement definition, so the brief and the numbers change together |

Adopt a board by recording its file and date in the settlement definition. Superseded boards stay in the repository and are marked superseded, like region concept sheets.

## Canvas

| Item | Value |
| --- | --- |
| Aspect ratio | 3:2 landscape |
| Resolution | The largest size the generation tool produces. gpt-image currently outputs at most 1536 × 1024 for landscape; that is the minimum. Store the original output, not an upscale. |
| Legibility | Every label, the numbered key and the scale bar can be read at 100% zoom. If the map panel is too small to read, generate it as a separate image (`<Name>-Town-Map-v<n>.webp`) at full resolution and adopt it with the board. |
| Measurements | Approximate. Scale bars and elevation dimensions show intent; the settlement definition records the numbers that are built. |
| Style | AAA game development design board; fantasy realism |
| Presentation | A mix of development blueprint, architectural concept board, geographic planning map and environment art bible |

## Required sections

| # | Section | Position | Purpose |
| --- | --- | --- | --- |
| 1 | Regional top-down map | Upper left | The intended settlement layout |
| 2 | Isometric settlement overview | Upper right | A realistic view of the finished settlement |
| 3 | Building reference library | Lower left | Standard architectural references |
| 4 | Gameplay infrastructure | Lower centre | Gameplay-related assets |
| 5 | Biome reference | Lower right | Environmental identity |
| 6 | Development notes | Around the board | Implementation guidance |

Positions are a guide. Sections 4 and 5 may share one panel, and the development notes may sit in a panel of their own, provided every listed item appears somewhere on the board.

### 1. Regional top-down map

- **Must show:** settlement boundaries, roads, rivers, bridges, terrain, elevation markers, walls, gates, district boundaries, landmarks, future expansion space, resource nodes and travel routes.
- **Labels:** district names, landmark names, gate names, road names and terrain features.
- **Style:** a readable game-world map comparable to an MMORPG zone map, legible at a glance and ready for production.
- **Implementation:** this panel feeds the zone checklist rows Z2–Z8, Z10 and Z14. Name each district and landmark exactly as the settlement definition does.

### 2. Isometric settlement overview

Shows the realistic settlement: the actual district layout, terrain elevation, roads, waterways, major buildings, walls and gates. It must match the top-down map, use believable scale and consistent architectural style, and show how the settlement is navigated.

### 3. Building reference library

A front elevation for each building type the settlement uses. Each building shows a front view, approximate scale in metres, its category and its main materials, cleanly enough to model in Blender.

| Category | Typical buildings |
| --- | --- |
| Core | Town hall, inn, blacksmith, stable, marketplace, adventurer guild, watchtower |
| Residential | Small, medium and large houses |
| Utility | Storage building, workshop, warehouse |

The settlement definition lists which buildings this settlement actually needs.

### 4. Gameplay infrastructure

- **Roads:** main roads, secondary roads and trails.
- **Navigation:** sign posts, road markers and bridges.
- **Utility assets:** fences, wells, street lamps, market stalls, crates, barrels and benches.
- **Service areas:** crafting stations, vendor areas and quest areas.

Everything here is a reusable environment reference for the biome library (`/Game/Frontier/Environment/<Biome>/`).

### 5. Biome reference

- **Vegetation:** trees, bushes, flowers and ground cover.
- **Geology:** rock formation types, cliff styles and water features.
- **Mood:** weather, lighting and seasonal appearance.

### 6. Development notes

- **Settlement metrics:** population, radius, district count and building count.
- **Gameplay metrics:** NPC density, vendor density and housing density.
- **Expansion planning:** expansion zones, future districts and reserved land.

Use "expansion area" or "reserved land" on the board; "zone" means a server map in this project.

## Visual rules

**A board must:**
- Put readability first.
- Use consistent iconography.
- Use a limited, region-appropriate colour palette.
- Show realistic proportions.
- Be usable in production.

**A board must not:**
- Use cinematic composition as its main focus.
- Hide important structures.
- Crowd the gameplay-critical areas.
- Show buildings that are not represented on the map layout.

The board is the visual source of truth for the settlement's look, and every environment and world-building asset made for the settlement follows it.

## Checklist before adoption

1. The content of all six sections is present.
2. Every district, landmark and gate on the board matches a name in the settlement definition, and no retired region names appear.
3. Every building in the isometric view appears on the top-down map.
4. The development notes repeat the definition's metrics. If they differ, the definition is correct and the board gets a correction note.
