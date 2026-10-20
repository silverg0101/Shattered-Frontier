# Settlement build runbook

How to take a settlement from its design board to a playable zone map, step by step, with the checks that prove each step and what to do when one fails. Silverbrook (`zone/greenlands_silverbrook`) is the worked example. Its numbers are the reference for "what good looks like".

The [settlement layout-to-world pipeline](settlement-world-pipeline.md) says what each stage is for and why. This runbook says how to run it. The [Silverbrook zone plan](greenlands-silverbrook-zone-plan.md) records Silverbrook's status and gaps. A new town gets its own zone plan, copied from that one.

## 0. Before you start

**Tools**
- Unreal 5.8 at `C:\Program Files\Epic Games\UE_5.8`.
- UE's Python: `Engine/Binaries/ThirdParty/Python3/Win64/python.exe`. Run the standard-library scripts with `-I`, from the repository root.
- Editor scripts run inside the open editor with `python tools/Run-InEditor.py <script> [args]`. This needs Project Settings > Plugins > Python > "Enable Remote Execution?".
- C++ builds need the editor closed: `/build-editor`, or `Build.bat ShatteredFrontierEditor Win64 Development -Project=... -WaitMutex -NoHotReloadFromIDE`.

**Rules that apply to every step**
- Write only under `/Game/Frontier/`. Megascans and Fab content is licensed, read-only, and never fills a look-profile role.
- No lighting changes (Lumen, GI, reflections, exposure, VSM, post) without the owner.
- The zone map is generated, never hand-edited. Any lasting change is a script or data change, followed by a rebuild.
- Tuning values are declared, typed settings with defaults and bounds (AGENTS.md). Never hard-code them.

**Several sessions share one checkout and one editor.**
- **Before closing the editor or rebuilding,** check for unsaved packages. The probe below lists them: dirty map and content packages. Ask the sessions that own them. Only then pass `--discard-unsaved` or close the editor.
- **Never run `git stash` or `git pull --autostash`.** The editor locks `.uasset` files, the reset fails half way, and other sessions' uncommitted work is reverted. If it does happen, restore with `git checkout <stash-hash> -- <paths>` then `git reset -q -- <paths>`.
- **Commit with single-line git commands,** each in its own call: `git add <paths>`, `git commit -F <msgfile>`, `git push`. Check `git diff --cached --name-only` is empty first, and read the staged `--stat` before committing.
- **Stage shared files byte-exact.** `.uproject`, `Config/*.ini` and `docs/dependency-map.md` often carry another session's uncommitted edits. Build the blob from HEAD plus your lines, write it with `git hash-object -w --no-filters`, and stage it with `git update-index --cacheinfo`. Never `git add` the whole file. See memory "partial staging byte-exact".
- **The RTK shell hook filters git output.** Use `rtk proxy git ...` when you need exact lines (status, diff, stash show).
- **Line endings:** Python `write_text` on Windows writes CRLF, so pass `newline='\n'` for LF files. Git Bash `sed -i` strips CR from CRLF files. Check with `file <path>` before editing.

The editor probe used throughout (save it to scratch):

```python
import unreal as u
w = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
print('PROBE world', w.get_path_name())
print('PROBE dirty', [p.get_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_map_packages() + u.EditorLoadingAndSavingUtils.get_dirty_content_packages()])
```

## 1. Region and layout data

| Step | Do | Check (pass looks like) |
| --- | --- | --- |
| Region | Trace the region map into `greenlands-region-layout.json` (or the new region's file) at the owner-confirmed scale. | `python validate_region.py` passes; `region_heightfield.py` preview reads like the map. |
| Town layout | Write `<town>-town-layout.json` in metres, origin at the town centre, north −Y, east +X. Silverbrook's keys are the template (see below). | `python validate_layout.py` prints `LAYOUT_OK … self-test passed`. |
| Terrain | `heightfield.py` shapes the layout's `elevation` block: markers, ridges, scarp and cliffs, rolling hills, pads, road beds, channels, basins. | `python heightfield.py` prints `TERRAIN_OK`: landmark heights within ±6 m, banks above water, road grades ≤ 0.08 (+0.02). |

**Layout keys** (see `silverbrook-town-layout.json`):

| Group | Keys |
| --- | --- |
| Frame and outline | `zone_half_extent`, `zone_bounds`, `seed`, `wall`, `gates` |
| Water and streets | `river`, `brooks`, `basins`, `roads` (primary, secondary and trail tiers, with widths) |
| Districts | `districts`: polygon, `inside_wall`, `fill` type, `count`, `dress` multipliers |
| Ground and requirements | `elevation`, `requirements` |
| Buildings and places | `building_types` ([w, d, h] m), `places` (numbered), `market_stalls`, `market_vendors`, `construction_plots` |
| Street dressing | `street_lamps` (tiers, spacing, verge, clear) |
| Townsfolk | `townsfolk` (households, jobs, district archetypes, guards, neighbour radius) |
| Arrival and review | `arrival`, `review_route`, `backdrop` |

**If the validator fails,** it names the place. The usual causes:
- A gate off the wall ellipse.
- A road that misses its gate.
- A self-intersecting district polygon.
- Water flowing uphill.

Fix the data, never the validator. Silverbrook's first run caught the West and East Gates 19 m off the wall.

## 2. Build the map

Copy `build_silverbrook.py` to `build_<town>.py`, then change the zone path, the layout file and the Silverbrook-specific tables (§8). Run it in the open editor:

```
python tools/Run-InEditor.py GameClient/ArtSource/Greenlands/build_<town>.py [--discard-unsaved]
python tools/Run-InEditor.py GameClient/ArtSource/Greenlands/pcg_dressing.py      # a few seconds later
```

The first command deletes and regenerates the map and logs `SILVERBROOK_BUILT … Counts={…}`. The second waits for PCG to generate, checks instance counts, saves, and prints `DRESSING_SAVED`. Always run both.

**What the builder places, and the rule for each:**

| Piece | Rule | Silverbrook result |
| --- | --- | --- |
| Terrain and backdrop | GeometryScripting mesh from the heightfield, with complex collision. The backdrop is visual only. | 2.5 km tile, −6 to 122 m |
| Roads, river, bridges | Box strips pitched to the ground, set to the `NoCollision` profile (a bare flag is reapplied on load, and the pawn catches on lips). | 8 bridges |
| Delivered buildings | The layout type maps to a look-profile building with `have` meshes. The variant is `crc32(label) % n`, stable across builds. **The FBX import flips −Y to +Y, so the actor yaw is the target yaw + 180.** Fronts face the street, the district centre or the nearest road. | 330 models |
| Foundations | Terrain levelled under every footprint after roads and water. Where the 5 m grid can't hold it, the model stands on the highest ground under it, with a stone plinth down to the lowest. | 246 plinths, tallest 2.2 m |
| Doors, gates, wheels | `place_parts.attach(actor)` on every spawn (Assets session's script) | 521 moving parts |
| Street props | `place_props.place_all()` after buildings and lamps | 131 props |
| Street lamps | Along primary and secondary streets inside the wall, alternating sides, clear of gates, bridges and junctions | 275 lamps |
| Wall | 20 m `Wall section`s on their lowest ground. Gatehouses face out of town. **The wall continues across water on the channel bed:** those pieces are `Water gate`s, whose grated arches let the river through but block swimmers. Towers skip water. | 164 pieces, 13 water gates |
| Water | Visual strips plus a water `PhysicsVolume` under every river and brook piece, basin and the sea. Each is 8 m deep with its top on the surface. | 491 volumes |
| Crossings | Narrowed river, banks built up to the road. The main crossing uses the stone bridge. Others use the river bridge kit: a 22 m deck stretched 0.85–1.15 along X and pitched to both banks, with an abutment 3 m past each deck end at that end's height (yaw ±90 plus the FBX flip). Riprap boulders only where no kit fits. Road strips stop over water, or they show under the arches. | 7 kit bridges at 19–25 m, plus the stone Silverbrook Bridge |
| Townsfolk footways | `townsfolk_lanes.build_in_editor(*plan(L), ground=HF.at)`: ZoneGraph shapes, then a graph rebuild | 30 lanes |
| Roster | `<town>-buildings.json`: every building's label, type, position and yaw, for the census | 312 dwellings and more |
| Review cameras | `ReviewCamera_Aerial`, `_Eastwood`, `_Street`, `_Bridge` and similar | 5 captures |

**Rebuilds wipe anything not in the builder.** Every placement owned by another script must be called from the builder: parts, props, footways. If something vanishes after a rebuild, its hook is missing.

## 3. Dress, list gaps, record the zone

| Step | Command | Pass looks like |
| --- | --- | --- |
| PCG dressing | Runs inside the builder; `pcg_dressing.py` saves. Each district's `dress` block scales look-profile densities (forest 1, town canopy 0.08, gardens 0.2, fields 0.12). | `DRESSING_SAVED`, counts equal expected; ~83k instances. Writes `<town>-<district>-candidates.json` (harvestable trees and rocks). |
| Missing assets | `python missing_assets.py` | Rewrites the batch list under `docs/art-templates/asset-lists/`. Hand the P1 rows to the Assets (Blender) session. |
| Zone record | `python zone_record.py` | `ZONE_RECORD_OK`: respawn points, level areas, gathering groups (N ≥ 2K), NPC groups beyond clearances. Writes `content/world/zones/<zone>.json`. Re-run after every rebuild, since candidates change. |
| Census | `python townsfolk_census.py` | `CENSUS_OK <version>` with the same version on a re-run; residents per district, jobs filled. Writes `<town>-census.json`. Needs the roster from a rebuild. |
| World content | `BackendServices/WorldContent.Tool` validates every zone JSON (see the dependency map row). | No violations. |

**The zone record's hostile placements** follow the settlement definition's exterior-content table:
- **Boar:** forest and farm edges.
- **Wolves:** beyond the playable radius, away from arrival.
- **Bandits:** on a rural trail beyond the playable radius, never at reserved places such as the Ruins.

The validator rejects hostile points in water, inside the wall, or within 80 m of plots and respawn points.

## 4. Gameplay in the town (development opt-ins)

**Gathering pilot.** Start a dedicated server on `<map>?game=/Script/ShatteredFrontier.SFConstructionNetworkGameMode` with `-ConstructionNetworkSandbox -GatheringPilot`. Nodes come from the zone record's gathering groups (`active_count` per group): wood is oak, stone is rough stone. Each node removes the dressing instances rooted within 1 m of it and replicates out to 500 m. Check with `tools/Test-GatheringPilot.ps1 -Silverbrook`: `Passed`, each client harvests its node, and a replay grants nothing.

**Townsfolk prototype.** Pass `-TownsfolkPrototype` on the map in `-game`. 50 census residents of `USFTownsfolkSettings.District` walk their day on the footways. Check with `review_townsfolk_in_game.py` on a 6-minute day (`"-ini:Game:[/Script/ShatteredFrontier.SFTownsfolkSettings]:GameDayMinutes=6"`). The `TOWNSFOLK_HOUR` lines must show:
- residents outdoors rising in the morning and falling to 0 at night;
- `RouteFailures` small;
- `ProcessorMs` well under the 1.5 ms town budget.

Silverbrook: 25 outdoors at 10:00, 0 from 17:00, 4 of 175 routes failed, 0.01–0.06 ms.

**Default map.** When the zone passes its checks, point `GameDefaultMap` and `ServerDefaultMap` at it in `DefaultEngine.ini`, and add it to `MapsToCook` in `DefaultGame.ini`. Stage the ini byte-exact (§0). E1 tests name their map, so they don't depend on the default.

## 5. The walk review (every rebuild)

```
UnrealEditor.exe <uproject> <map> -game -RenderOffscreen -unattended -nosound -windowed -ResX=1280 -ResY=720 -ExecCmds="py <repo>/GameClient/ArtSource/Greenlands/review_silverbrook_in_game.py"
```

`-ExecutePythonScript` is refused in `-game`; use `-ExecCmds="py …"`. The review writes `<town>-game-review.json`.

**Pass looks like:**
- `passed: true`, and every `review_route` leg reached with 0 stuck events.
- The swim check: `swimming_after_4s: true`, `climbed_out: true`.
- Frame times as a comparison, not a budget. Silverbrook: 3.7 ms median, 5.8 ms p95.
- Captures for the arrival view, the aerial view and each review camera. Look at them: doors facing the street, nothing floating, the wall closed over the river.

## 6. Troubleshooting

| Symptom | Cause | Fix | Validate |
| --- | --- | --- | --- |
| The editor opens an old map after a crash | `EditorStartupMap` is the art review map; nothing is lost | Open the zone map | `PROBE world` |
| `No open ShatteredFrontier editor answered` | Editor closed, busy compiling shaders, or remote execution off | Wait and retry; relaunch; enable remote execution. Don't write ping files from PowerShell (BOM). | Probe answers |
| Builder: `Save or discard these first` | Unsaved packages in the editor | Ask their owners, then `--discard-unsaved` | `PROBE dirty []` |
| A bridge pier stops in mid-air above the water | Mesh masonry too shallow for built-up banks over a deep channel | Bridge and abutment stone runs to z=-10 m; anything below the bed hides in the terrain | Crossing capture |
| Walk review stalls swimming at a bridge pier | The review pawn walks straight lines; a crossing at an angle sends it off the bank into the river, and a full-depth pier blocks the swimmer | Put the bridge ends in `review_route`, so the walk follows the road across the deck | Walk passes |
| A pale slab under a bridge's arches | Road strip pieces continue across the river at deck height | Builder skips road pieces whose centre is over water | Crossing capture |
| Floating flat plates in the flowers | A placeholder mesh (`SM_Wildflowers`) fills a role | Remove it from the look-profile role; cluster the real flowers | Street capture |
| Front doors face away from the street | FBX import flips −Y to +Y | Actor yaw = target yaw + 180 | `ReviewCamera_Street` shows a door |
| Pawn stalls on a road | Pieces kept the default collision profile, so lips catch | Builder sets the `NoCollision` profile | Walk: 0 stuck events |
| A capture shows the next camera's view | `HighResShot` writes a frame later | Alternate phases: switch, wait, capture | Captures match labels |
| Doors, props or footways gone after a rebuild | Their script isn't called by the builder | Add the hook next to `place_parts` | Counts in the build log |
| Walking into the river; gaps in the wall | No water volumes; wall skipped water | Swim volumes and wall over water (§2) | Swim check; aerial capture |
| Python can't read a volume's `brush` | Protected property | Don't read it. `spawn_actor_from_class(PhysicsVolume)` builds a 2 m cube brush; scale = size / 2 | Bounds |
| Gathering nodes don't appear | Not admitted: needs a dedicated server, the construction game mode URL, both flags, and an admitted map path | Run as in §4; add the new map to admission (§8) | `GATHERING_PILOT_READY Source=ZoneRecord Nodes=N` |
| Two clients pick different nodes | Net relevancy: default cull 150 m | Nodes cull at 500 m; the scenario picks the oak nearest the origin and the rock nearest that oak | `-Silverbrook` test passes |
| Node inside a dressing tree; harvest denied | PCG instance overlaps the node; line of sight blocked | Node removes instances rooted within 1 m | Harvest commits |
| Compile error in a file you didn't touch (`redefinition`) | Unity build: two files share an anonymous-namespace name, exposed when your edits change the blobs | Rename the clashing name at the root | Build succeeds |
| Linker: `Z_Construct_UScriptStruct_FMassFragment` unresolved | Mass core types live in the `MassCore` module | Add `MassCore` to Build.cs | Build succeeds |
| `Fragment type is not trivially copyable` | `TArray` or `FString` in a Mass fragment | Keep arrays in a subsystem, indexed by entity | Build succeeds |
| ZoneGraph builds 0 lanes | Lane profile missing: ZoneGraph settings are `config=Plugins` | Put `[/Script/ZoneGraph.ZoneGraphSettings]` in `Config/DefaultPlugins.ini`, not DefaultEngine.ini | `TOWNSFOLK_LANES … lanes>0` |
| Residents at Z=0, invisible | Footway ground traces ran in the same tick as loading or building, before collision existed (or not tracing complex) | Builder passes heightfield heights; standalone runs only on the loaded map, tracing complex | `TOWNSFOLK_HOUR … one at … Z>0` |
| ZoneShape points offset | `SpawnActor<AZoneShape>(location)` ignores the location | Spawn at the origin with world-space points | Lanes on the streets |
| Runtime instanced bodies don't render | Instanced-mesh updates at runtime | Placeholder actors now, MassRepresentation in step 3 | `TOWNSFOLK_REVIEW N … visible` and a close capture |
| Residents stranded or `RouteFailures` high | ZoneGraph A* follows outgoing links only (no U-turns); destination far from footways | Own Dijkstra allows crossing to the opposite lane; extend footways to the destination | Low `RouteFailures` |
| Streets don't join at a T | Street ends drawn metres short | `townsfolk_lanes.py` snaps ends within 12 m | `LANES_PLAN_OK` junction count |
| Other sessions' work reverted | `git pull --autostash` with locked files | Never use it; restore from the stash hash | `git status` matches |

## 7. Commit checklist

- [ ] Validators print OK: layout, terrain, zone record, census. The walk review passes, and the townsfolk and gathering checks if they were touched.
- [ ] Rebuild outputs staged together: map, terrain and backdrop meshes, graybox materials, PCG graphs, candidates, roster, census, zone record, review JSON and captures.
- [ ] Shared files staged byte-exact; `git diff --cached --stat` lists only your files.
- [ ] `docs/dependency-map.md` row updated when files, roles or commands change.
- [ ] Zone plan updated: stage rows and gaps, with dated evidence.

## 8. What is still Silverbrook-only (generalize when town two starts)

These are fixed values today. Turn them into per-zone data (zone id → map path, layout, record and census files) as the second town's first step, and keep the Silverbrook run passing.

| Where | Hard-coded |
| --- | --- |
| `build_silverbrook.py` | Zone map path, layout file, `KIND_BUILDING`, `WALL_PIECES`, review camera spots |
| `zone_record.py` | Output path, `HOSTILE` table (group centres and radii) |
| `townsfolk_census.py`, `townsfolk_lanes.py` | Layout and output names; `DISTRICT` and the square radius |
| `Construction/SFConstructionNetwork.cpp` | `IsZoneGatheringWorld()` map path; `SpawnZoneGatheringNodes()` record path |
| `Townsfolk/SFTownsfolk.cpp` | `ZoneMap` path and census path (district is already a setting) |
| `review_silverbrook_in_game.py`, `review_townsfolk_in_game.py` | Map path, camera labels |
| `Silverbrook/place_parts.py`, `place_props.py` | Silverbrook mesh library and kinds (Assets session) |
