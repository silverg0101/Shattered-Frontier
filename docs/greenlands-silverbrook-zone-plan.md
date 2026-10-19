# Silverbrook zone map — Wave 0 plan

Status: map created 2026-10-19; traversal check not yet passed. Further layout work waits for the Silverbrook design board. Zone `zone/greenlands_silverbrook` (Starter, levels 1–8), per the [Greenlands region](Game-Design/Regions/Legends_Shattered_Frontier_Region_Greenlands_v0.1.md), the [Silverbrook settlement definition](Game-Design/Regions/Legends_Shattered_Frontier_Settlement_Silverbrook_v0.1.md) and the [Zone Definition Standard](Game-Design/World-Simulation/Legends_Shattered_Frontier_World_Zone_Definition_Standard_v0.1.md).

## Map

`/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook`, built by `GameClient/ArtSource/Greenlands/build_silverbrook.py` from `/Game/Frontier/Maps/Test/Greenlands_ArtReview`. Re-running the script rebuilds the zone map from the art review map, so art changes made there carry over. Edits made only in the zone map are lost on a rebuild; script them.

The zone map differs from the art review map in one way: it has no game mode override, so it runs the project game mode (`SFE1GameMode`: `SFE1Character` and `SFE1PlayerController`) instead of the art walker. The E1 interaction fixture spawns only in `Greenlands_E1`. Gathering and construction stay in their test maps; they use development fixtures with hard-coded positions, and zone spawn data (checklist Z8) replaces them.

## Slice spaces as Silverbrook areas

The art layout (`region_layout.py`, metres, 600 × 600 m tract, 956 m loop road) already holds the six [vertical slice](Game-Design/Legends_Shattered_Frontier_Vertical_Slice_Specification_v1.0.md) spaces.

| Slice space | Area (centre, radius m) | Today |
| --- | --- | --- |
| Arrival landmark | Arrival (-185, -85), 10 | Player start, waystone, road start |
| Settlement beacon | Settlement (-90, -40), 30 | Cottage, workshop, herbalist, market stall, gardens |
| Resource route | Resource_Grove (90, 75), 22; Stone_Outcrop (155, 55), 16 | Scenery only; no gathering nodes |
| Build plots | Build_West/Central/East (-25…35, 5), 14 each | Three clear 20 × 20 m plots |
| Enemy clearing | Enemy_Clearing (130, -135), 22 | Clearing only; no enemies |
| Return overlook | Return_Overlook (-65, -150), 14 | Viewpoint on the loop |

## Gaps against the v2 concept and the zone checklist

1. **Size.** The art tract is 600 × 600 m. The settlement definition asks for a town core of about 350 m radius, a 750 m playable radius and a zone of about 2.5 × 2.5 km. Only part of the playable ring exists; the countryside is unbuilt.
2. **Town.** Today's village has a cottage, workshop, herbalist and market stall. The definition calls for seven districts (Town Square, Market, Adventurer, Crafting, Residential, Stable, Inn) and six landmarks (Adventurer Hall, Central Fountain, Great Oak Tree, River Bridge, Watchtower, Marketplace). None of the districts or landmarks exist. The layout waits for the design board.
3. **Orientation.** No world axis is declared as north (settlement open decision 1). Decide it before placing the roads and rivers below.
4. **Roads out (Z3, Z7).** v2 roads leave Silverbrook toward Silvermere (north), Everwood (south-west) and Valemere (south-east), as paved primary roads. Today's corridors point west and north-east and end at the tract edge.
5. **Rivers (Z7, Z12).** Silverbrook sits at the confluence of its brook and the main river, with the River Bridge below. The map has no water.
6. **Mountain backdrop.** v2 shows snow peaks to the north; the map has distant hills only.
7. **Resources (Z8).** No zone spawn groups for timber, stone, herbs or iron.
8. **Wildlife and enemies (Z9).** No boar territories, wolf dens or bandit camps (settlement §10), and no ambient stag, deer or rabbit; combat does not exist yet.
9. **Areas and safety (Z4, Z5).** Areas exist only as authoring coordinates. No `level_area` polygons or protected/no-edit volumes, and the arrival is not server-protected.
10. **Respawn (Z2).** One player start; no respawn points.
11. **Zone data.** No `content/world/regions/greenlands` or `content/world/zones/greenlands_silverbrook` records.
12. **Terrain.** The ground is an authored mesh, not the editable voxel terrain of DEC-03.
13. **Defaults.** `GameDefaultMap` and `ServerDefaultMap` still point at `Greenlands_E1`; switch after the zone passes its checks.

## Order of work

Board first: adopt the Silverbrook design board (settlement §2), then settle orientation (gap 3). Then lay out the town core, roads and rivers (gaps 2, 4, 5) and the backdrop (gap 6), which fix the zone's shape. Gaps 7–11 belong with the gameplay and World Simulation work that uses them.
