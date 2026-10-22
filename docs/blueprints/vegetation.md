# Vegetation blueprint: trees, shrubs, saplings, logs, stumps, boulders, forest density

Scope: one Greenlands **nature family** (3-4 mesh variants) from CC0 Poly Haven data, imported under `/Game/Frontier/Environment/Greenlands/Nature/`, placed by the look profile. Placement density and the ground beneath are in [landscape-and-ground.md](landscape-and-ground.md). Shared rules: [README.md](README.md).

## 1. Pipeline (in order)
| # | Step | Command (repo root) | Done when |
|---|---|---|---|
| 1 | Sources | `py -3 GameClient/ArtSource/Greenlands/NatureFamilies/acquire_sources.py` | `Sources/provenance.json` has a row per file, 0 md5 mismatches |
| 2 | Concept sheet (new family only) | Codex image, saved as `NatureFamilies/<Family>/Concept/concept-sheet.png`; prompt names the reference tile of `concept-art/Greenlands-Forest-Look-v1.webp` | one sheet: front and 3/4 views, 1.8 m figure, neutral grey background |
| 3 | Build | `"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P GameClient/ArtSource/Greenlands/NatureFamilies/<Family>/build_<x>.py` | writes `SM_<Family>_00..0N.fbx`, `<Family>.blend`, `asset-manifest.json`, `<Family>-Variants.png`, `<Family>-Detail-3m.png` |
| 4 | Blender render review | fresh Fable reviewer judges `-Variants.png` (eye level, 1.8 m marker) and `-Detail-3m.png` against the concept | PASS before any import |
| 5 | Import (open editor) | `py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/NatureFamilies/import_nature.py <Family>` | meshes `SM_<Family>_NN`, `MI_<Family>_*` under `.../Nature/`; editor log has no `Failed to compile Material` |
| 6 | Leaf wind + player push (once per project, idempotent) | `py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/NatureFamilies/add_leaf_wind.py` then `.../foliage_interaction.py` then `.../tune_wp4c_materials.py` | log has no `Default Material will be used` |
| 7 | Register the role | `GameClient/ArtSource/Greenlands/greenlands-look-profile.json`: role row with `have` = the mesh names and a `place` block (§3) | `python GameClient/ArtSource/Greenlands/dress_zone.py` prints `DRESS_OK` |
| 8 | Place | `py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/build_silverbrook.py --discard-unsaved` then `py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/pcg_dressing.py` | `SILVERBROOK_BUILT`, PCG `generated N/N expected` |
| 9 | Ground gate | `py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/check_dressing_grounded.py` | `DRESSING_GROUNDED {"floating": 0, ...}` |
| 10 | Capture and review | bookmarks 01 (forest path), 04 (forest floor) via `review_silverbrook_in_game.py` with `GFX_EVIDENCE_DIR=docs/evidence/graphics/<date>-<pass>/`; pairs vs `gameplay-hud.png` (01) and `Greenlands-Forest-Look-v1.webp` (04) | fresh Fable PASS |

Mesh conventions (every family): metres, Z up, front -Y, origin at the base centre (logs/stumps: ground contact, buried part below Z=0), no ground plane or figure exported, Nanite on, no LOD chain, trees get trunk-only UCX prisms (no canopy collision), rocks convex decomposition. Materials are only the manifest specs (`asset-manifest.json`) that both the Blender look and `import_nature.py` read.

## 2. Passing families (copy the nearest one for a new asset)
| Family / role | Size at scale 1 | Tris | How it was built (and why) | Builder |
|---|---|---|---|---|
| `PineTall` / `tree_pine_tall` | 21-25 m | 305-330 k | Poly Haven `pine_tree_01` (a,b,c) + `pine_sapling_medium` reshaped to Scots habit: bare straight stem 56-62 % of height, crown squeezed into the top 40-45 %, sprays clumped. Opaque geometry needles, no alpha (Nanite). | `PineTall/build_pine.py` |
| `BroadleafYoung` / `tree_broadleaf_young` | 9-10.8 m, 1-3 pale stems | 245-290 k | Procedural stems/twigs (no CC0 birch exists), leaf sprays at twig tips (2x first density), leaves cut from `tree_small_02` alpha, bark `japanese_hackberry_bark` whitened with lenticel bands in vertex R. | `BroadleafYoung/build_broadleaf.py` |
| `ShrubBroadleaf` / `shrub_broadleaf` | 2.52x2.02x1.70, 2.14x1.78x1.42, 1.74x1.50x1.10 m | 57.7-58.1 k | Pass 3: 24-26 slender canes from a 0.3-0.5 m base, suckers and side shoots leaf low on the stems, outer foliage droops to the ground, darker inner leaves hide the stems. Reads as a hazel/bramble mound. | `ShrubBroadleaf/build_shrub.py` |
| `SaplingBroadleaf` / `sapling_broadleaf` | 1-3 m | 14.4-16.7 k | Three leafy single-trunk saplings (WP4c rework); leaf instance on `M_GN_Leaf` with `MF_PlayerInteraction`. | `SaplingBroadleaf/build_sapling.py` |
| `FallenLogStump` | logs 5.10 and 6.35 m, one stump | per manifest | Rounded tapered trunks, ragged broken ends, 4 snapped branch bases each, slightly buried; stump with 6 separate curving roots; `Col.R` = up-facing moss mask; two slots `MI_FallenLogStump_Bark` / `_BrokenWood`. Render review passed. | `FallenLogStump/build_fallen_log_stump.py` |
| `BoulderLargeMoss` / `boulder_large_moss` | 1.6-2.6 m | 40 k each | Scan meshes `boulder_01`, `namaqualand_boulder_03/04/06` decimated to 40 k, regraded to grey granite, moss (`mossy_rock`) by up-normal + noise in vertex R. | `BoulderLargeMoss/build_boulder.py` |
| `BedrockSlab`, `HerbBroadleaf`, `MossPatch`, `LeafNeedleLitter`, `CobblePebbleScatter` | see each `asset-manifest.json` | | same kit (`nature_kit.py`, `rock_family.py`, `veg_kit.py`) | `<Family>/build_*.py` |

New family recipe: copy the folder of the nearest row above, rename the family in the build script's `FAMILY`/mesh prefix, keep the kit calls, change only shape parameters and sources, keep triangle budgets (trees <= 330 k, shrubs <= 60 k, saplings <= 20 k, rocks 40 k), rerun steps 3-10.

## 3. Placement values (`greenlands-look-profile.json`, `place` block; `per_ha` is the forest target, districts scale it)
| Role | per_ha | spacing m | slope deg | scale | cull m | other |
|---|---:|---:|---|---|---:|---|
| `tree_pine_tall` | 45 | 7 | 0-35 | 0.95-1.4 (20-35 m placed) | | `clear` 4, valley >= 4 |
| `tree_broadleaf_young` | 30 | 6 | 0-25 | 0.6-0.85 (5.5-9 m) | | `clear` 3, valley 0-8 |
| `tree_oak` | 6 | 10 | 0-15 | 0.9-1.2 | | harvest wood |
| `shrub_broadleaf` | 270 | 2 | 0-30 | 0.9-1.35 | 100 | `cluster` 3, `sink_cm` 10, outside town |
| `sapling_broadleaf` | 20 | 4 | 0-20 | 0.8-1.3 | 60 | `collision` true, `sink_cm` 5, clearings |
| `boulder_large_moss` | 6 | 8 | 8-90 | 1.0-1.6 | | `sink` 0.25 |
| `bedrock_slab` | 40 | 5 | 30-90 | 1.5-2.6 | | `sink` 0.35 |
| `rock_medium` | 14 | 2 | 0-90 | 0.25-0.5 | | `sink` 0.2, cluster |
| `herb_broadleaf` | 1300 | 0 | 0-25 | 1.2-2.2 | 50 | shade, cluster 3 |
| `fern` | 3800 | 0 | 0-28 | 1.2-1.9 | 50 | shade |
| `grass_clump` (PCG, to be replaced by grass types) | 8000 | 0 | 0-15 | 1.8-3.2 | 50 | sun |
| `moss_patch` | 350 | 0 | 0-35 | 1.0-2.0 | 40 | shade, rock tops |
| `leaf_needle_litter` / `cobble_pebble_scatter` | 500 / 220 | 0 | 0-35 / 0-30 | 1.0-2.0 / 0.8-1.5 | 25 | cluster 3 |
District `dress` scale (`silverbrook-town-layout.json`): forest 1, town canopy 0.08, gardens 0.2, fields 0.12. Bookmark 01 (154, 632) is outside every district: a view with no cover is a missing district polygon (WP4c added Outer South and River Verge), not a density problem.

## 4. Known failure -> rule
| Failure (owner or reviewer) | Rule |
|---|---|
| Shrubs read as topiary balls (pass 1), then as umbrella trees on a stem (pass 2) | A shrub is a multi-stem mound: canes start across the whole base, leaves from 0.2 m up, outer foliage touches the ground, no visible single trunk, no flat crown. Judge `-Detail-3m.png` at eye level before import. |
| Saplings as cubes / blocky cards | A sapling is one slender trunk with leaf sprays (`build_sapling.py`); never a box or billboard. Keep `M_GN_Leaf` + `MF_PlayerInteraction` on the leaf instance. |
| Boxy logs | Logs are revolved tapered trunks with ragged ends, snapped branch bases, bark + moss by vertex R, partly buried; stumps have separate roots. Never a scaled cylinder or box. |
| Shrubs, saplings and every leaf mesh rendered black/grey | `M_GN_Leaf` failed to compile: SimpleGrassWind `AdditionalWPO` was unconnected. Rule: after any material script, grep the editor log for `Failed to compile Material`; the gate is zero hits. Rerun `add_leaf_wind.py` (idempotent) and `tune_wp4c_materials.py`. |
| Dry or black grass cards | See [landscape-and-ground.md](landscape-and-ground.md) §5: straight alpha with nearest-colour dilation, zero black RGB in transparent texels through the 4 px mip, cards >= 0.23 m so they read at 3 m. |
| `rock_medium` and litter black (WP4) | Material gap on the family; import with `import_nature.py <Family>` and recompile; the default-material gate must print 0. |
| Sparse approach (WP4, WP4c) | Raise `per_ha`/scale only in the look profile, never in the PCG graph; add a district polygon where the view is undressed; the target frame needs trees, shrubs, ferns and herbs all present in one view. |
| Pale mountains show through a thin pine canopy | Canopy density is not the fix; the backdrop is (landscape doc §3). |
| Placing by hand in the editor | Never. Everything comes from the look profile + `build_silverbrook.py` + `pcg_dressing.py`; the map is regenerated, not hand-edited. |
| Stylized or Fab/Megascans foliage | Never; CC0 Poly Haven or Codex-generated only, hash rows in `Sources/provenance.json`. |

## 5. Concept refs
`docs/Game-Design/concept-art/Greenlands-Forest-Look-v1.webp` (mixed woodland, undergrowth, mossy boulders, roots), `concept-art/UI/v5/gameplay-hud.png` (forest path foreground, density and colour), `<Family>/Concept/concept-sheet.png` where present.
