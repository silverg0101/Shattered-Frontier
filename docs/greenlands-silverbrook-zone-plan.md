# Silverbrook zone map — Wave 0 plan

Status: **graybox, rebuilt from the whole board** on 2026-10-19. The route walk passes on the region-fitted terrain. The zone is the 2.5 km tile centred on Silverbrook in the [region layout](Game-Design/Regions/Legends_Shattered_Frontier_Region_Greenlands_v0.1.md), and its zone frame equals the region frame. Zone `zone/greenlands_silverbrook` (Starter, levels 1–8), per the [Greenlands region](Game-Design/Regions/Legends_Shattered_Frontier_Region_Greenlands_v0.1.md), the [Silverbrook settlement definition](Game-Design/Regions/Legends_Shattered_Frontier_Settlement_Silverbrook_v0.1.md) with its design board, and the [Zone Definition Standard](Game-Design/World-Simulation/Legends_Shattered_Frontier_World_Zone_Definition_Standard_v0.1.md).

## Map

`/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook` is generated, not hand-edited:

| Piece | File | Role |
| --- | --- | --- |
| Layout data | `GameClient/ArtSource/Greenlands/silverbrook-town-layout.json` | Metres; north −Y, east +X; origin at the town centre. Read from the board at 1.8 m per board pixel. Holds the wall, gates, river, the Eastfall Brook, the harbour and pond basins, named roads including the ring streets, districts, the 20 numbered places, building sizes, plots, the mountain backdrop, arrival and the review route. Heights are relative to the region datum, 80 m above sea level. |
| Terrain | `GameClient/ArtSource/Greenlands/heightfield.py` | Standard library only. Shapes a 5 m grid from the layout's elevation data. It starts from markers, ridges, water banks, and the scarp and cliffs. Then it adds rolling hills and levels a pad for every building and open area. It lays grade-limited road beds one road at a time, so that later roads pin to earlier ones where they cross. Last it carves the river, brook and basins. `backdrop()` shapes the mountains beyond the edge. `python heightfield.py` checks landmark heights (±6 m), banks above the water and road grades (≤ 0.08 plus 0.02 tolerance), and prints `TERRAIN_OK`. |
| Builder | `GameClient/ArtSource/Greenlands/build_silverbrook.py` | Editor Python. Plans every building first, then writes the terrain mesh `/Game/Frontier/Environment/Greenlands/Terrain/SM_Silverbrook_Terrain` (GeometryScripting, complex collision) and puts everything on it. Fails with a list of any place that overlaps a road, the river, the wall or another building. Colours are `/Game/Frontier/Core/Graybox/MI_Graybox_*`. |
| Review | `GameClient/ArtSource/Greenlands/review_silverbrook_in_game.py` | Walks the project pawn along the review route in `-game` and captures an arrival view and an aerial view. |

**What the graybox contains:**
- **Ground:** a 2.5 × 2.5 km shaped terrain mesh, −6 to 122 m. Northridge sits on a plateau above a scarp, with cliffs to the east.
- **Edges and backdrop:** within 300 m of the edge, the terrain eases into the region terrain. The backdrop is the region terrain out to 6 km, visual only, with a sea surface at sea level. Both come from `heightfield.zone_region()`.
- **Water:** all visual only, at their layout water levels in carved channels:
  - the Silverbrook River, 24 m wide. It loops around The Commons and flows into the Riverside harbour.
  - the Eastfall Brook, which drops over the Riverside falls into the harbour.
  - the Eastwood pond.
- **Roads:** primary, secondary and trail roads. These include the Market, Commons and Northridge ring streets, Wall Street, the cross lanes, and the roads out to Silvermere, Everwood and Valemere.
- **Bridges:** eight, placed wherever a road crosses water. One of them is the Silverbrook Bridge. Each deck reaches dry ground on both banks and tilts to meet it.
- **Walls:** a 0.8 × 1.06 km elliptical wall with towers, plus four gates.
- **Key places:** the 20 numbered places at the board's sizes, the Market Square with eight stalls, and the farmlands, cemetery and ruins.
- **District houses:** 312 houses in rows along both sides of the streets: Northridge 60, The Commons 150, Riverside 60, Westfields 12, Eastwood 30.
- **Wave 0 construction plots:** three 20 × 20 m plots in Eastwood.
- **Arrival:** a player start and waystone on the Valemere road.
- **Labels and camera:** district and place labels, and an aerial review camera.

**Game mode:** the map runs the project game mode (`SFE1GameMode` with `SFE1Character`). The E1 interaction fixture spawns only in `Greenlands_E1`. Gathering and construction stay in their test maps, because they use development fixtures with hard-coded positions. Zone spawn data (checklist Z8) will replace them.

**Art review map:** `/Game/Frontier/Maps/Test/Greenlands_ArtReview` remains the art reference. Its kit (8 × 6 m cottage and workshop, roads, planting) returns in the art pass.

### Rebuild and review

Run both from the repository root in PowerShell, with the editor closed.

Rebuild:

```
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' "$PWD\GameClient\ShatteredFrontier.uproject" -run=pythonscript "-script=$PWD\GameClient\ArtSource\Greenlands\build_silverbrook.py" -unattended -nullrhi -nosplash '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities,GeometryScripting'
```

Review:

```
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' "$PWD\GameClient\ShatteredFrontier.uproject" /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook -game -RenderOffscreen -unattended -nosound -windowed -ResX=1280 -ResY=720 "-ExecCmds=`"py $($PWD -replace '\\','/')/GameClient/ArtSource/Greenlands/review_silverbrook_in_game.py`""
```

`-ExecutePythonScript` is refused in `-game`; use `-ExecCmds="py <script>"`.

With the editor open, rebuild inside it instead of headless, so the editor and the files on disk never disagree. First enable Project Settings > Plugins > Python > "Enable Remote Execution?", then run:

```
python tools/Run-InEditor.py GameClient/ArtSource/Greenlands/build_silverbrook.py
python tools/Run-InEditor.py GameClient/ArtSource/Greenlands/pcg_dressing.py
```

Run the second command a few seconds after the first, once PCG has generated. It checks the instance counts and saves the map.

The builder refuses to start while the editor has unsaved maps or assets. Some maps mark themselves unsaved as soon as they load. Once you have saved or discarded your own edits, add `--discard-unsaved` after the script path to proceed.

**Evidence (2026-10-19, on the shaped terrain):** `silverbrook-game-review.json`. The pawn walked arrival → South Gate → Silverbrook Bridge → Market Square → North Street bridge → North Gate, reaching all 7 legs in 226 s with no stalls. `SFE1GameMode`, no E1 fixture. Captures: `Silverbrook-Arrival-InGame.png` and `Silverbrook-Aerial-InGame.png`.

## Slice spaces in the graybox

| Slice space | Silverbrook area | Graybox |
| --- | --- | --- |
| Arrival landmark | Arrival on the Valemere road (420, 840) | Player start and waystone |
| Settlement beacon | Market Square (0, −30) | Plaza, stalls, guild, inn and blacksmith around it |
| Resource route | Gathering areas within 750 m | Not placed |
| Build plots | Eastwood plots (430…510, 215) | Three 20 × 20 m plots |
| Enemy clearing | Boar territories at the forest and farm edges | Not placed |
| Return overlook | Watchtower (−330, −520) | 9 m tower; needs high ground |

## Gaps

1. **Terrain edges (Z2).** Closed 2026-10-19: the zone edge matches the region (edge error 0.0 m). The region roads run through the zone's road exits, and the Eastfall Brook comes in from the region.
2. **Water.** The river has no collision or swimming, so the wall has open gaps where the river crosses it. The gap by the South Gate runs about 90 m, because the river and the gate exclusions overlap.
3. **Mountain backdrop.** Closed 2026-10-19: it is the region terrain. Rivers in the backdrop are carved but have no water surface yet.
4. **Resources (Z8).** No gathering areas or spawn groups.
5. **Wildlife and enemies (Z9).** No boar territories, wolf dens, bandit camps or ambient animals. Combat does not exist yet.
6. **Areas and safety (Z4, Z5).** Districts exist as layout data only. There are no `level_area` polygons and no protected or no-edit volumes for the walled town.
7. **Respawn (Z2).** There is one player start and no respawn points.
8. **Zone data.** There are no `content/world/regions/greenlands` or `content/world/zones/greenlands_silverbrook` records.
9. **Size.** The 2.5 km tile is fixed by the region grid. The countryside beyond the town is still empty.
10. **Defaults.** `GameDefaultMap` and `ServerDefaultMap` still point at `Greenlands_E1`. Switch them once the zone passes its checks.
11. **Editable terrain.** The ground is a static mesh, not the editable voxel terrain of DEC-03.
12. **Route walk.** Closed 2026-10-19. The road, river and other no-collision pieces kept their default collision profile, which is reapplied on load, so the pawn caught on the lip where two tilted road pieces meet. The builder now sets the NoCollision profile itself.
13. **Unassigned walled ground.** Parts of the walled town west and east of The Commons belong to no district, so they stay empty.

## Pipeline work (brief received 2026-10-19)

Silverbrook is the first run of the [settlement layout-to-world pipeline](settlement-world-pipeline.md). Stage rules and exit checks live there. This section holds the Silverbrook-specific inputs and status.

**Blockers named in the brief are already resolved:**
- **Districts:** the definition was fitted to board v1. Its five districts (Northridge, The Commons, Riverside, Westfields, Eastwood) replace the original seven, whose functions now live inside them (settlement §6).
- **North:** north is −Y and east is +X (zone standard, "Compass axes").

**Gaps in scope**, in this plan's numbering: 1 terrain (surface only), 2 water, 3 backdrop, 4 resources (candidates file only), 6 district volumes (no server protection) and 11 editable terrain (TER-01 spike only).

| Stage | Silverbrook specifics | Status |
| --- | --- | --- |
| 1. Layout + validator | `silverbrook-town-layout.json` holds district polygons, explicit `zone_bounds`, a `requirements` block mirroring the settlement definition, and proposed `elevation` data (markers, ridges, east cliff, river levels, the upper town falls, flatten rules). `validate_layout.py` uses the standard library only; run it with any Python 3, such as UE's `Engine/Binaries/ThirdParty/Python3/Win64/python.exe -I`. It checks places, districts, wall radius (530 m), bounds, gates on wall and road, the roads out to Silvermere (N), Everwood (SW) and Valemere (SE), the river under the Silverbrook Bridge, polygon self-intersection, and that water flows downhill. Its self-test confirms 10 broken copies fail. `build_silverbrook.py` refuses an invalid layout and fills districts inside their polygons. The first run caught the West and East Gates 19 m off the wall; both are fixed. | **Done** 2026-10-19. Elevation is data only until stage 2 shapes the terrain. |
| 2. Builder | `build_silverbrook.py` reads the layout, is idempotent, and builds the terrain from `heightfield.py`. Roads, river and walls are box pieces that follow the terrain. Still to do, alongside stage 3, which consumes them: splines for roads, river and walls, tagged district volumes, and PCG volumes. | Terrain **done** 2026-10-19 (walk passes, 7/7 legs, no stalls). **Delivered buildings** placed 2026-10-19: each layout type whose look-profile building has `have` meshes spawns one of them instead of a box (327 models), front to the street, district centre or nearest road; the variant comes from a stable hash of the label. The terrain is levelled under every footprint after roads and water; where the 5 m grid can't hold it, the building stands on the highest ground under it on a stone plinth (247 plinths, 206 under 0.5 m, tallest 2.2 m). `ReviewCamera_Street` checks a townhouse front. **Street lamps** (2026-10-19): the four `Street lamp` variants stand along primary and secondary streets inside the wall, every 22 m on alternating sides, arm over the road; the layout's `street_lamps` block sets tiers, spacing, verge and the clearance from gates, bridges and junctions (275 lamps). No point lights yet: the lantern glass glows from its material. **Stone bridge** (2026-10-20): the delivered `Stone bridge` mesh replaces the graybox deck at the Silverbrook Bridge when the measured crossing fits its 30 m span with level banks; `ReviewCamera_Bridge` views it from the river. The other seven crossings keep graybox decks. **Wall, gates and waterside** (2026-10-20): the town wall is the delivered `Wall section` (20 m segments resting on their lowest ground) with `Wall tower`s, and each gate is one `Gatehouse` facing out of town. The mill, granary, Fisherman's wharf and Riverside docks are delivered meshes; the wharf and docks face the nearest water at bank level. The layout's `market_vendors` put the Armor and Weapons vendors in the first two market ring places, and every stall faces the square. `place_parts.py` re-attaches doors, gate leaves, portcullises and the mill wheel on every rebuild. Splines and volumes not started. |
| 3. PCG dressing | All five districts dressed (2026-10-19), with these outputs:<br>- `dress_zone.py` rules from the look profile `place` blocks. Each district's `dress` block in the layout scales the layer densities: Eastwood is forest (1), The Commons and Riverside are town (canopy 0.08, ground cover 0.35), Northridge has gardens (0.2 / 0.6) and Westfields is open fields (0.12 / 1).<br>- `pcg_dressing.py` writes `/Game/Frontier/PCG/PCG_Silverbrook_<District>`, hosted by a `PCG_<District>` volume.<br>- 83,190 instances in total, 25,436 of them in Eastwood. Flowers grow in patches of five; the `SM_Wildflowers` placeholder is no longer used.<br>- `silverbrook-<district>-candidates.json` files of harvestable trees and rocks, not used yet.<br>- Walk passes 7/7 with no stalls; frame median 3.7 ms, p95 5.1 ms, up from 2.3 / 5.4 with Eastwood only.<br>- `Silverbrook-Eastwood-InGame.png`.<br><br>Oaks stand in for pines and young broadleaf, and mossy rocks for boulders and bedrock. The build ends with a second in-editor run of `pcg_dressing.py`, which saves once PCG has generated. Drop a volume tagged `PCG_Exclude` and regenerate to clear an area. | All districts done; frame-time comparison with `dress_region.py` and the fine-tuning check open |
| 4. Missing-asset report | `missing_assets.py` compares the look profile and the definition's §8 and §10 buildings, plus the layout's extra building types, with the biome library. It writes [batch 42](art-templates/asset-lists/environment-greenlands-silverbrook-42.md): 30 planned rows. P1 rows are pines, young broadleaf, large boulders, bedrock slabs, the five unique landmarks and townhouses. | **Done** 2026-10-19; rows unclaimed |
| Mesh Terrain spike | On a copy of this map only. See [TER-01](decisions/TER-01-mesh-terrain-fit-check.md). | Not started |

**Elevation:** stage 1 adds elevation data to the layout, and stage 2 shapes the terrain from it. That covers Northridge's high ground, the hills, the river valley and the waterfall drop east of town, and closes gap 1 and the flat-ground note in the review. The finished target is the look reference: rocky, elevated mixed pine and broadleaf forest with layered ground cover.

## Order of work

1. Board re-read (done 2026-10-19): ring streets, 312 houses in rows, the river loop and harbour, the Eastfall Brook and falls, the Northridge plateau and the east cliffs.
2. Region fit (done 2026-10-19): gaps 1, 3 and 12.
3. Pipeline stages 3 and 4 above: PCG dressing, then the missing-asset list for the building kit and vegetation. TER-01 can run alongside, on a copy.
4. Water (gap 2): collision and swimming, then close the wall gaps.
5. Gathering areas and boar territories (gaps 4 and 5), using the stage 3 candidates file, to make the slice loop playable.
6. Gaps 6–8, with the World Simulation work that uses them.
