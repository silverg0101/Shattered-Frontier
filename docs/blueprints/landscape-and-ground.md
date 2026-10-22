# Landscape and ground blueprint

Scope: the zone ground (terrain, layers, paint), roads and plazas, water, mountains, grass and ground cover, and the **density targets, the issues found and how we built them and why**. Asset-making steps for trees, shrubs, saplings, logs and rocks are in [vegetation.md](vegetation.md); this doc owns their placement and density. Depth: [GFX-02](../decisions/GFX-02-photoreal-ground-pipeline.md) (incl. J1b mountains). Shared rules: [README.md](README.md).

## 1. What exists today vs the photoreal path
| Layer | Today (committed, passing WP3c/WP6f gates) | Target (GFX-02, J1-J6 spec; J0 done) |
|---|---|---|
| Terrain | static mesh `SM_Silverbrook_Terrain` from the 5 m `Heightfield` (`build_silverbrook.py`), 501x501 vertices, relief by `apply_relief()` | `ALandscape` built by editor-only C++ `USFLandscapeBuilder::CreateLandscape` (`ALandscape::Import`; no Python path exists), 1 m quads (scale 98.4252 cm, 10x10 components, 127 quads/section, 2x2 sections), Z scale 100, collision mip 0; heights still from `HF.at()` |
| Ground material | `M_GN_Ground`/`MI_GN_Ground` (`NatureFamilies/import_ground.py`): forest floor to bedrock by vertex R (slope), cobble by G, packed earth by B, tilled fields by A | `M_GN_Landscape` 8 weight layers from `TerrainMaterials/landscape-layers.json`: Grass `grass_ground` 2.51 m, Meadow `park_dirt` 3.0, ForestFloor `forest_ground_04` 3.15 + `forest_leaves_03` 2.3, Cobble `cobblestone_large_01` 4.0 (height blend), PackedEarth `rocky_trail` 2.0 (height), Rock `marble_rock_01` 2.0 + `mossy_rock` 3.0 (height), Mud `brown_mud_02` 1.3, Field `brown_mud_03` 1.3; every scan sample hex-tiled through `MF_GN_HexTile` (§2a); macro `aerial_grass_rock` at 15 and 73 m, `MacroAmount` 0.35 (luminance only on hard layers); distance blend to the aerial scan from 25 to 60 m |
| Ground cover | PCG instances from the look profile (`dress_zone.py` -> `pcg_dressing.py`) | Landscape Grass Types (`landscape_grass.py`) for grass, herbs, flowers, litter; PCG keeps trees, rocks, shrubs, saplings (collision, gameplay candidates) |
| Prop grounding | hard edges | two RVTs (`RVT_Silverbrook_Color` 3.8 cm/texel, `_Height` 15 cm/texel) + `MF_GN_RVTBlend` on rocks 45 cm, logs 30, footings 25 |
| Mountains | WP5 domes + `SM_MountainRing` (`backdrop.py`): **FAILED** ("egg carton foam", one over The Commons, blue band under the range) | J1b: a second `ALandscape` `OuterTerrain` (10 m quads, +-12.7 km) shaped from Copernicus GLO-30 relief inside the region's mountain bands; domes, ring and `Backdrop` deleted |
| Water | flat planes with `MI_GN_River` (SingleLayerWater, panning normals), no tile-end seams | deferred line: reflection, depth tint, bank foam (board right panel as reference) |
| Lighting | `silverbrook-lighting.json` 10 lux pinned EV100 2-4 | physical (GFX-02 §5), see [lighting-and-atmosphere.md](lighting-and-atmosphere.md) |
Lumen, VSM, TSR and Nanite are already on (`DefaultEngine.ini`). Photoreal path rules: **Landscape, not a static mesh**; Poly Haven CC0 or Codex textures only; Nanite tessellation for ground relief only on a Nanite landscape (GFX-03 §1.4); RVT blending; grass types; physical lighting.

## 2. Ground paint (today; the rules every rebuild and repaint must keep)
Both scripts write the same fields: `GameClient/ArtSource/Greenlands/build_silverbrook.py` (rebuild) and `GameClient/ArtSource/Greenlands/paint_street_edges.py` (repaint the open map: `py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/paint_street_edges.py`, prints `STREET_EDGES_PAINTED vertices N step 5.0 door_paths 312 lot_lanes 312`). **Every paint change goes in both.**
| Value | Where | Setting |
|---|---|---|
| Road ramp | `build_silverbrook.py` `ROAD_RAMP = HF.step` (5 m); `paint_street_edges.py` `ramp` one grid step | field = `clamp(1.0 - (d - hw) / ramp)`: **1 to the kerb**, one-step ramp outside; never `0.5 + (hw-d)/ramp` (0.6 cobble at the kerb = green-tinted street) |
| Cobble margin | `STREET_COBBLE_EXTRA_M = 2.0` / `COBBLE_EXTRA = 2.0` | cobble past the nominal edge |
| Earth margin | `STREET_EARTH_EXTRA_M = 8.0` / `EARTH_EXTRA = 8.0` | packed earth (B) to the plinth line of both rows (half width 4 + 8 m); 4.5 m left a lawn |
| Earth under cobble | `ROAD_B[_k] = max(ROAD_B.get(_k, 0.0), _f)`; `nb = max(c.b, b.get(key, 0.0))` | B is 1 inside the cobble too; never `f * (1 - G)` |
| Blend order | `import_ground.py` loop `(('B',...), ('G',...))` | earth lerped first, cobble on top: a cobble ramp fades into earth, never into grass |
| Plazas | `PLAZA_EARTH_M = 10.0`; plaza field 1 inside, one-step ramp outside; earth band = rectangle + 10 m | `market.png` y 650-1080 has no 60x60 block with G-(R+B)/2 > 6 |
| Door paths / lot lanes | B = 1, 2.4 m strips (half = `max(1.2, step/2 + 0.05)` so a lane hits a vertex column) from each `_Stoop0` to the nearest road station; lot-edge lane opposite the garden side | visible lanes between houses, no lawn |
| Trails | B > 0.9 with G = 0 beyond the wall | render as earth (spot-check `(-8, -1210)` m) |
Gate for streets: `main-street.png` every 100x100 block in y 620-1080 has sRGB G-R <= 8 (passing pass: -13 to -51). Landscape version: the same geometry as weight fields in `landscape_fields.py` (Cobble ramp 2 m, PackedEarth 1.5 m, Field 1.5 m, Rock 20->40 deg, Mud 2->7 m from water, ForestFloor 4->9 m from trunks, Meadow noise 0.55-0.75 + 0.5 within 12 m of a road, Grass remainder; priority in that order, uint8 sum 255).

### 2a. Anti-tiling (GFX-02 §2a; every layer, today's mesh terrain and the landscape)
| Rule | Value |
|---|---|
| Technique | hex-tiling (Mikkelsen 2022) in `MF_GN_HexTile` (`NatureFamilies/hex_tile.py`, one Custom HLSL node, 3 taps per texture, `SampleGrad` with rotated gradients); never the engine's `TextureVariation` (dither) or `Texture_Bombing` (4 taps, ghosting); never two incommensurate scales |
| Applies to | every world-aligned scan sample of every layer: D, N, mask and displacement; Rock in all three projections; the 15 m macro too. Plain: `Mask`, `EdgeNoise`, the 73 m macro (3.5-73 m fields by design) |
| Per layer (`landscape-layers.json`) | `hex_cell_tiles` 0.5 (Mud/Field 0.6; bounds 0.25-1.0), `hex_rot` 1.0 (Cobble 0.15; 0-1), `macro_amount` Grass/Meadow/ForestFloor 0.35, Mud/PackedEarth/Field 0.30, Cobble 0.25, Rock 0.20, Snow 0.12 (0-0.6); normal flattened by `saturate((PixelDepth - 2500) / 3500)` on every layer |
| Today (J2-pre, passed r10) | Mesh terrain `M_GN_Ground`, built only by `import_ground.py` (values are in the code; `MI_GN_Ground` untouched). `MF_GN_HexTile` inputs `UV, D, N, M, H, Rot, CellTiles, Snap, HeightSharp, VN, BlendSharp, Warp`; its `N` is world space (frame `Tw = normalize(cross((0,1,0), VN))`, `Bw = cross(VN, Tw)`), so `M_GN_Ground` `tangent_space_normal` False and bedrock C's plain normal goes through the `WorldNormalC` Custom node. Blend `lerp(1, L, 0.6) * pow(w, BlendSharp) * exp2(max(HeightSharp * (H - maxH), -3))`, variance gain `k = rsqrt(dot(W, W))` around the last-mip mean. Per layer (`layer(...)`): A forest_ground_04 tile 200 cm, `Rot` 0.15, `H` = its Displacement, `HeightSharpA` 96; B mossy_rock 150 cm, `Rot` 1.0; T brown_mud_02 200 cm, `Rot` = `lerp(RotTNear 0.5, RotTFar 1.0, saturate((PixelDepth - 2000) / 6000))`; S rocky_trail 150 cm, `Rot` 1.0; R cobblestone_pavement 240 cm, `Rot` 0.0, `Snap` 14; all `CellTiles` 1.0, `BlendSharpX` 24 (7-32); lattice `Warp` = two Simplex `Noise` (`scale` 0.001, `levels` 1, z offset 0 / 8192) x `HexWarp` 0.15 tile (0-0.2). Moss weight `wm = saturate((MaskLow + Noise x MaskNoise 0.02 - Low 0.415) x InvRange 256)`, `MaskLow` = forest_ground_04 Displacement at 9 m (`TileCmMask` 900, `MaskWarp` 0.3 domain warp) sampled at `const_mip_value` 6; trail stones read the full-res `Mask`. `EdgeNoise` (350 cm) UV + Mask warp x `EdgeWarp` 2.5. Bedrock C plain triplanar. Cost: BasePass +0.42 ms at bookmark 01 (RX 7900 XTX, 1440p) |
| Gate | `tools/Measure-GroundRepeat.py`: autocorrelation peak (6 px < r <= 120 px) of the luminance in 256 px crops `cobble`/`earth`/`grass` at views `A1-south-approach-30m`, `A2-...-100m`, `A3-...-250m` (camera (181, 661) over the Valemere Road bend looking at (90, 574)) **<= 0.25** for all nine; tiled ground reads 0.5-0.9 |
| Human check | A1 before/after pair beside `gameplay-hud.png` far range; two copies of one motif in a crop = FAIL |
| Rebuild + verify (J2-pre) | 1. `py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/NatureFamilies/import_ground.py` -> `GROUND_IMPORT [...]`; revert the side-effect re-saves (`git checkout -- .../Materials/M_GN_Surface.uasset MI_GN_Road.uasset MI_GN_Trail.uasset M_GN_River.uasset`); commit only `M_GN_Ground` and `MF_GN_HexTile`. 2. Close the editor; captures `docs/evidence/graphics/gfx02-j2pre-hextile/capture_game.py` (`$env:GFX_TAG='after'`, then again with `$env:GFX_MEASURE='1'`). 3. `py -3.12 tools/Measure-GroundRepeat.py <evidence>/after-measure <evidence>/repeat-measure.json` -> `GROUND_REPEAT PASS crops 9 max_peak 0.1972 limit 0.25`. 4. `py -3.12 <evidence>/make_pairs.py` -> three `SUN_TWIN_DIFF` lines > 8. 5. `perf_gpu.py` (launch line in its docstring) + `py -3.12 <evidence>/gpu_passes.py after/perf-01.log` -> BasePass within +0.6 ms of the before (0.84 ms). 6. Fresh Fable review |
| Never (J2-pre lessons) | rotate a hex normal in the mesh's UV tangent frame (world-aligned UV: use the world frame); threshold a moss/blend mask at full resolution near its median (texel-dithered crossfade: sample a low mip); warp the lattice steeper than ~0.2 tile per tile (folds into wide blend patches); full `Rot` 1.0 on scans with baked shading at 1-5 m (forest floor loses ~20 % relief: 0.15) |

## 3. Mountains (J1b values, `silverbrook-outer.json`)
DEM `Copernicus_DSM_COG_10_N47_00_E011_00_DEM` (free, credit line in `Sources/provenance.json` and the credits), window 25.4 km centred 47.42 N 11.30 E, relief = DEM minus its 3 km mean; `h = base + E * (M * (bump + k * relief) + plains_relief * relief)`, `exclusion_m` 300/900 (nothing raised over the city; foothills from 300 m past the zone edge, peaks from 900 m), `feather_m` 600, `k_fraction` 0.3, `plains_relief` 0.03, `snow_line_asl_m` 1100 +- 150, `skirt_drop_m` 3 inside the zone. Gates: boundary ring within 2 cm of `HF.at()`, no outer vertex above the zone inside it, `check_hidden_in_game.py` passes, perf 02/08 within +1.5 ms. Review bookmarks `02-main-street`, `06-north-edge` (0,-1200)->(0,-2500), `07-east-edge`, `08-high-point`; pairs vs `gameplay-hud.png` far range and the board's mountain panel. Rules: never a procedural dome or ring; never a mesh backdrop inside the zone square; recession comes from the atmosphere (aerial perspective 1.2, fog 0.012), not scaled haze.

## 4. Density targets
PCG roles today (`greenlands-look-profile.json` `place.per_ha`; district `dress` scale forest 1, town canopy 0.08, gardens 0.2, fields 0.12): pine 45, young broadleaf 30, oak 6, shrub 270 (cluster 3), sapling 20, boulder 6, bedrock slab 40, rock_medium 14, herb 1300, fern 3800, flower_small 400 (cluster), moss 350, litter 500, pebbles 220, grass_clump 8000 (= 0.8 clumps/m2: the "minecraft" figure). Full table in [vegetation.md](vegetation.md) §3.
Grass target (GFX-02 §3, per 10 m2, density scaled by layer weight; `DefaultScalability.ini` `sg.FoliageQuality` 1/3/3):
| Grass type | Variety | /10 m2 | cull start/end m | scale |
|---|---|---:|---|---|
| LGT_Grass | cards 00-03 `MI_GN_GrassCard_Grass` | 220 | 30 / 55 | 0.85-1.35 |
| | tall cards 04-05 | 25 | 50 / 100 | 1.2-1.8 |
| | `SM_PH_FloweringClump_0..2` | 4 | 0 / 45 | 1.0-1.5 |
| | `SM_LeafyDandelions_00,_02` | 3 | 0 / 35 | 1.0-1.4 |
| LGT_Meadow | cards 00-03 `_Meadow` (DryAmount 0.55) | 90 | 25 / 50 | 0.7-1.1 |
| | tall 04-05 / flowering clumps / `SM_WhiteEverlasting` | 12 / 8 / 3 | 40-90 / 0-45 / 0-40 | |
| LGT_ForestFloor | cards 00-03 `_Forest` | 30 | 20 / 40 | 0.6-0.9 |
| | ferns / herbs / litter / moss | 10 / 25 / 12 / 4 | 0-45 / 0-40 / 0-25 / 0-35 | |
| LGT_Mud | cards 20, pebbles 6; LGT_PackedEarth pebbles 8; Cobble/Rock/Field none | | | |
Expected load at bookmark 01: ~210 k cards in 55 m + 80 k tall in 100 m, ~8 M tris, no shadows; budget grass <= 3.5 ms, p95 <= 16.7 ms / median <= 13 ms at 1440p Standard. First perf lever: `LGT_Grass` 220 -> 160, end cull 55 -> 45, in the look profile only. Forest density pass (trees/shrubs `per_ha`, understory) follows J3.

## 5. Grass cards (J0, PASS) and ground cover: how we built them and why
1. `NatureFamilies/build_grass_cards.py` (Blender 5.1, `py -3.12` with Pillow/NumPy/SciPy): six cards, three crossed quads at 0/60/120 deg, 2x3 subdivisions bent 12 deg outward, pivot base centre, vertex R = height (wind weight), LODs 36/12/4 tris at screen sizes 1.0/0.35/0.12 as separate `_LOD0/_LOD1/_LOD2` FBX (Blender 5.1 writes no LODGroup; attach LODs on import).
2. Atlases `T_GN_GrassCards_D/N` 2048x1024, six 512 px slots; **straight alpha, 3x supersample, area downsample, nearest-colour dilation into transparent texels**; gate: no black RGB in transparent texels and no dark box-filtered alpha edges from full size through the 4 px mip (minimum channel 22-45).
3. Why authored blades: the first bake from sparse, dry source clumps failed review; the redo paints 152-192 overlapping fresh-green blades per card (2-10 % dry) and enlarged cards 02/03 from 0.10/0.14 m to 0.24/0.23 m so they read at 3 m (concept = `gameplay-hud.png` foreground grass). Review sheet: every card on mid-grey and grass-green at 1:1 and projected 3 m (`six-grass-cards-compare.jpg`).
4. Material `M_GN_GrassCard` (`landscape_grass.py`): masked clip 0.33, two-sided foliage, subsurface 0.55, tint lerp (0.86,0.95,0.62)-(0.62,0.74,0.40) by PerInstanceRandom, `DryAmount` per MI, distance colour from `RVT_Silverbrook_Color` past 40-80 m, `SimpleGrassWind` clamped 6 cm, `MF_PlayerInteraction`, WPO off at 25 m. Near/far gate: `tools/Measure-GroundMatch.py` 200x200 px patches at 10 m and 100 m differ <= 10/255 per channel; the only fix is the Grass layer tint.
5. Why grass types, not PCG: GPU-instanced per view, zero instances on the dedicated server, densities in the 200+/10 m2 range that PCG actors cannot afford; why trees stay PCG: collision and gameplay candidates.
6. Flowers: `SM_PH_FloweringClump_0..2`, dandelions and everlasting are grass-type varieties (table above), never scaled into pots (the 0.35-scale clump in a tub read as a dead twig; use `PropFlowerTub`).

## 6. Issues found -> rule
| Issue | Rule |
|---|---|
| "grass looks NOTHING like we want / we are making minecraft": 0.8 clumps/m2, 300-1,600-tri clumps culled at 50 m, flat yellow-green `forest_ground_04` tint | grass = cards at 20-40/m2 on a Grass layer whose far colour is the aerial scan; never raise `grass_clump` PCG density as the fix |
| Bookmark 01 bare (WP4) | add a district polygon (Outer South, River Verge); density is per district |
| Shrubs/saplings/leaves black (WP4b) | material compile gate, [vegetation.md](vegetation.md) §4 |
| Still sparse after WP4c (549,714 instances, 0 floating) | the look is layers + grass types + relief, not more PCG instances |
| Dry/black card edges, tiny cards (J0 first pass) | §5 steps 2-3 |
| Mountains "egg carton", over the city, blue band at the base, pale faceted (WP5) | §3; no domes, no ring, no mesh inside the zone, atmosphere for recession |
| Market and street lawn, cobble fading to grass, green-tinted carriageway (WP6b-e) | §2 full-weight fields, earth under cobble, blend order |
| Road edge is a 10 m ramp on the 5 m grid; cobble stretched down a bank | landscape at 1 m quads with 2 m ramps (J1); relief stays a 5 m `Heightfield` decision |
| Cyan flat brook plane | water line deferred to GFX-02; not a debug line |
| Trail invisible | B > 0.9, G = 0, `rocky_trail` pushed through `brown_mud_02` (`StoneLow` 0.6, `StoneSharp` 14) |
| Yellow/pink/blue lines, giant district names, "Tt" sprites in the owner's shots | editor-only actors; `set_actor_hidden_in_game(True)` on every label, marker, lane shape, spline; gate `HIDDEN_IN_GAME_CHECK ... visible_in_game 0`; captures from `-game` only |
| Hand-painting the landscape | never; repaint = rebuild from the fields |
| "i hope to see the texture pattern gone, from a distance like this": cobble, earth and grass grids visible 20-200 m from 30 m up over the south approach | §2a: hex-tile every scan sample in world space (mips blur a period, they never remove it); gate `peak <= 0.25` at A1-A3; applied to `M_GN_Ground` now, not only to the landscape |

## 7. Commands (order for a ground pass today)
1. `py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/NatureFamilies/import_terrain.py` then `.../import_ground.py` (masters; no level touched).
2. `py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/build_silverbrook.py --discard-unsaved`; `.../pcg_dressing.py`; `.../check_dressing_grounded.py` -> `DRESSING_GROUNDED {"floating": 0`.
3. Repaint-only change: `paint_street_edges.py` (§2) + `check_ground.py` -> `DRESSING_GROUND ... 'floating': 0`.
4. Captures: bookmarks 01, 02, 03, 04, 05 and A1-A3 (`py -3.12 tools/Measure-GroundRepeat.py <evidence dir>` -> `repeat-measure.json`, every `peak` <= 0.25) (`review_silverbrook_in_game.py`, `GFX_EVIDENCE_DIR=...`), pairs vs `gameplay-hud.png` (01, 02, 03, 05) and `Greenlands-Forest-Look-v1.webp` (04); `perf_bookmark_game.py 01`; fresh Fable review.
GFX-02 jobs add: `/build-editor` for `SFLandscapeBuilder` before the first J1 build; `landscape_build.py` alignment gate <= 2 cm; `r.VT.Flush` shows no magenta/black ground (J4); `-server` run has `stat foliage` 0 (J3).
