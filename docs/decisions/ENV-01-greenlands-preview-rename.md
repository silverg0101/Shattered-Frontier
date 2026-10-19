# ENV-01 — Remove "Preview" from Greenlands names

Status: accepted and done, 2026-10-19. Evidence below.

## Why

The [Zone Definition Standard](../Game-Design/World-Simulation/Legends_Shattered_Frontier_World_Zone_Definition_Standard_v0.1.md) ("Zone maturity stages") keeps maturity in the zone's content record, not in names: no "Preview", "Final" or "V2" in folders or maps. The [Greenlands region](../Game-Design/Regions/Legends_Shattered_Frontier_Region_Greenlands_v0.1.md) §8 maps today's preview art onto the Greenlands biome library and the Silverbrook zone. This record says what moves where, and how.

## Renames

| Today | Becomes | Note |
| --- | --- | --- |
| `/Game/Frontier/Environment/GreenlandsPreview/` | `/Game/Frontier/Environment/Greenlands/` | Biome library. Includes the git-ignored Fab `Village/` and `Garden/` folders. |
| `.../GreenlandsPreview/Greenlands_EnvironmentPreview` map | `/Game/Frontier/Maps/Test/Greenlands_ArtReview` | Stays an art review map. Its layout seeds `/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook`; it is not renamed into the zone map. |
| `GameClient/ArtSource/GreenlandsPreview/` | `GameClient/ArtSource/Greenlands/` | Plain files: `git mv`. |
| `Launch-GreenlandsPreview.ps1` | `Launch-GreenlandsArtReview.ps1` | Follows the map. |
| `EditorStartupMap` in `GameClient/Config/DefaultEngine.ini` | The art review map, later Silverbrook | |

Not in scope: render thumbnails such as `previews/` folders, `Render-*-Previews.py` and `Waystone-Preview.png`, plus the Blender working file `ArtSource/Greenlands/GreenlandsPreview.blend` that `author.py` writes. Those are file names, not content or map names. Moving `Greenlands_E1` and `Greenlands_ConstructionSandbox` under `Maps/Test/` is a separate change, because C++, runners and `GameDefaultMap`/`ServerDefaultMap` name them.

## Procedure

1. Restore the Fab `Village/` and `Garden/` content first (`import_village.py`, `import_village_gardens.py`). A folder move without them leaves broken references that only show on machines that have them.
2. In the full editor, move the Content folder and the map (Content Browser move), run **Fix Up Redirectors**, and save all. Never move `.uasset`/`.umap` files with Git or Explorer.
3. Update path constants in the art scripts and settings JSON (`village-selection.json`, `garden-selection.json`, import targets), the launch script, `DefaultEngine.ini`, the Content `.gitignore` location, and docs. On 2026-10-19 that was 178 references in 63 tracked text files (`git grep -I 'GreenlandsPreview\|EnvironmentPreview'`).
4. `git mv` the ArtSource folder.
5. Verify: the art review map opens without missing-reference errors in the log, a village import re-run lands under the new path, and `ShatteredFrontier.Gathering.` passes.

After this lands, every machine with local Fab content re-runs the two import scripts, because the git-ignored copies keep their old paths.

## When

Before building `Greenlands_Silverbrook`, so the zone map is never built against paths that are about to move.

## Evidence (2026-10-19)

- Headless editor Python (`AssetTools.rename_assets`, then redirector fix-up and save) moved 433 assets: 432 to `Environment/Greenlands`, and the map to `Maps/Test/Greenlands_ArtReview`. No redirectors or assets were left under the old path. The editor refuses this move while `DefaultEngine.ini` still names the old map, so update that file first.
- 61 tracked text files updated. `git grep` finds no `GreenlandsPreview`/`EnvironmentPreview` paths outside the exclusions above and this record.
- Verification: the map loads with 588 actors and no mesh component missing its mesh. All `/Game/Frontier/...` paths in `village-selection.json`, `garden-selection.json` and `temperate-canopy-source.json` resolve. `ShatteredFrontier.Gathering.` passed 12/12.
- Logs: `Artifacts/ENV-01/` (local).
