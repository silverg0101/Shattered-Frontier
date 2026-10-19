# Silverbrook zone map — Wave 0 plan

Status: **graybox**, 2026-10-19. The main route traversal passed; zone size is not yet confirmed. Zone `zone/greenlands_silverbrook` (Starter, levels 1–8), per the [Greenlands region](Game-Design/Regions/Legends_Shattered_Frontier_Region_Greenlands_v0.1.md), the [Silverbrook settlement definition](Game-Design/Regions/Legends_Shattered_Frontier_Settlement_Silverbrook_v0.1.md) with its design board, and the [Zone Definition Standard](Game-Design/World-Simulation/Legends_Shattered_Frontier_World_Zone_Definition_Standard_v0.1.md).

## Map

`/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook` is generated, not hand-edited:

| Piece | File | Role |
| --- | --- | --- |
| Layout data | `GameClient/ArtSource/Greenlands/silverbrook-town-layout.json` | Metres; north −Y, east +X; origin at the town centre. Read from the board at 1.8 m per board pixel. Holds the wall, gates, river, named roads, districts, the 20 numbered places, building sizes, plots, arrival and the review route. |
| Terrain | `GameClient/ArtSource/Greenlands/heightfield.py` | Standard library only. Shapes a 5 m grid from the layout's elevation data: markers, ridges, river banks and the east cliff, then rolling hills, levelled pads for every building and open area, grade-limited road beds, and the river channel. `python heightfield.py` checks landmark heights (±6 m), banks above the water and road grades (≤ 0.08 plus 0.02 tolerance), and prints `TERRAIN_OK`. |
| Builder | `GameClient/ArtSource/Greenlands/build_silverbrook.py` | Editor Python. Plans every building first, then writes the terrain mesh `/Game/Frontier/Environment/Greenlands/Terrain/SM_Silverbrook_Terrain` (GeometryScripting, complex collision) and puts everything on it. Fails with a list of any place that overlaps a road, the river, the wall or another building. Colours are `/Game/Frontier/Core/Graybox/MI_Graybox_*`. |
| Review | `GameClient/ArtSource/Greenlands/review_silverbrook_in_game.py` | Walks the project pawn along the review route in `-game` and captures an arrival view and an aerial view. |

**What the graybox contains:**
- **Ground:** a 2.5 × 2.5 km shaped terrain mesh, −8 to 59 m. Its relief is still gentle compared with the board (see gap 1).
- **River:** the Silverbrook River, visual only, 24 m wide, at its layout water levels in a carved channel.
- **Roads:** primary, secondary and trail roads, including the roads out to Silvermere, Everwood and Valemere.
- **Bridges:** five, placed wherever a road crosses the river. One of them is the Silverbrook Bridge. Each deck reaches dry ground on both banks and tilts to meet it.
- **Walls:** a 0.8 × 1.06 km elliptical wall with towers, plus four gates.
- **Key places:** the 20 numbered places at the board's sizes, the Market Square with eight stalls, and the farmlands, cemetery and ruins.
- **District houses:** 105 seeded houses across the five districts.
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
```

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

1. **Terrain (Z2).** The ground is shaped from the layout's elevation data, but it still reads as nearly flat next to the board. Still to do:
   - Raise Northridge onto a plateau with a steep edge.
   - Add the cliffs and waterfalls east of town.
   - Add a backdrop of mountains beyond the zone edge. Today the horizon is open sky.
   - Raise the watchtower hill so the overlook works.
2. **Water.** The river has no collision or swimming, so the wall has open gaps where the river crosses it. The gap by the South Gate runs about 90 m, because the river and the gate exclusions overlap.
3. **Mountain backdrop.** Missing.
4. **Resources (Z8).** No gathering areas or spawn groups.
5. **Wildlife and enemies (Z9).** No boar territories, wolf dens, bandit camps or ambient animals. Combat does not exist yet.
6. **Areas and safety (Z4, Z5).** Districts exist as layout data only. There are no `level_area` polygons and no protected or no-edit volumes for the walled town.
7. **Respawn (Z2).** There is one player start and no respawn points.
8. **Zone data.** There are no `content/world/regions/greenlands` or `content/world/zones/greenlands_silverbrook` records.
9. **Size.** The 2.5 × 2.5 km extent is still a proposal (settlement §13). The countryside beyond the town is empty.
10. **Defaults.** `GameDefaultMap` and `ServerDefaultMap` still point at `Greenlands_E1`. Switch them once the zone passes its checks.
11. **Editable terrain.** The ground is a static mesh, not the editable voxel terrain of DEC-03.

## Pipeline work (brief received 2026-10-19)

Silverbrook is the first run of the [settlement layout-to-world pipeline](settlement-world-pipeline.md). Stage rules and exit checks live there. This section holds the Silverbrook-specific inputs and status.

**Blockers named in the brief are already resolved:**
- **Districts:** the definition was fitted to board v1. Its five districts (Northridge, The Commons, Riverside, Westfields, Eastwood) replace the original seven, whose functions now live inside them (settlement §6).
- **North:** north is −Y and east is +X (zone standard, "Compass axes").

**Gaps in scope**, in this plan's numbering: 1 terrain (surface only), 2 water, 3 backdrop, 4 resources (candidates file only), 6 district volumes (no server protection) and 11 editable terrain (TER-01 spike only).

| Stage | Silverbrook specifics | Status |
| --- | --- | --- |
| 1. Layout + validator | `silverbrook-town-layout.json` holds district polygons, explicit `zone_bounds`, a `requirements` block mirroring the settlement definition, and proposed `elevation` data (markers, ridges, east cliff, river levels, the upper town falls, flatten rules). `validate_layout.py` uses the standard library only; run it with any Python 3, such as UE's `Engine/Binaries/ThirdParty/Python3/Win64/python.exe -I`. It checks places, districts, wall radius (530 m), bounds, gates on wall and road, the roads out to Silvermere (N), Everwood (SW) and Valemere (SE), the river under the Silverbrook Bridge, polygon self-intersection, and that water flows downhill. Its self-test confirms 10 broken copies fail. `build_silverbrook.py` refuses an invalid layout and fills districts inside their polygons. The first run caught the West and East Gates 19 m off the wall; both are fixed. | **Done** 2026-10-19. Elevation is data only until stage 2 shapes the terrain. |
| 2. Builder | `build_silverbrook.py` reads the layout, is idempotent, and builds the terrain from `heightfield.py`. Roads, river and walls are box pieces that follow the terrain. Still to do, alongside stage 3, which consumes them: splines for roads, river and walls, tagged district volumes, and PCG volumes. | Terrain **done** 2026-10-19 (walk passes, 7/7 legs, no stalls). Splines and volumes not started. |
| 3. PCG dressing | First district end to end: Eastwood (**proposed**: woodland, cottages and foragers exercise the forest, meadow and town-verge graphs). Compare against `dress_region.py`. | Not started; PCG plugin not enabled |
| 4. Missing-asset report | Compare the Greenlands look profile (`ArtSource/Greenlands/greenlands-look-profile.json`, from [`Greenlands-Forest-Look-v1.webp`](Game-Design/concept-art/Greenlands-Forest-Look-v1.webp)) and settlement §8 buildings with the biome library. Write the batch to `docs/art-templates/asset-lists/`. | Profile written; report not started |
| Mesh Terrain spike | On a copy of this map only. See [TER-01](decisions/TER-01-mesh-terrain-fit-check.md). | Not started |

**Elevation:** stage 1 adds elevation data to the layout, and stage 2 shapes the terrain from it. That covers Northridge's high ground, the hills, the river valley and the waterfall drop east of town, and closes gap 1 and the flat-ground note in the review. The finished target is the look reference: rocky, elevated mixed pine and broadleaf forest with layered ground cover.

## Order of work

1. Match the board's shape before any art goes in. All four items are layout and terrain data:
   - **Streets:** trace the ring streets and lanes.
   - **Buildings:** fill them in rows along street edges instead of scattering them, at the board's density (about 300–400 buildings, against 105 today).
   - **River:** re-trace the loop around The Commons and add the Riverside harbour basin.
   - **Terrain:** add the bolder relief and the mountain backdrop listed in gap 1.
2. Pipeline stages 3 and 4 above: PCG dressing, then the missing-asset list for the building kit and vegetation. TER-01 can run alongside, on a copy.
3. Water (gap 2): collision and swimming, then close the wall gaps.
4. Gathering areas and boar territories (gaps 4 and 5), using the stage 3 candidates file, to make the slice loop playable.
5. Gaps 6–8, with the World Simulation work that uses them.
