# GFX-02: Photoreal ground pipeline

Status: proposed for execution, 2026-10-21. Source: the owner's verdict on the live market-square shot ("grass looks NOTHING like we want ... we are making minecraft") against `docs/Game-Design/concept-art/UI/v5/gameplay-hud.png` and `concept-art/Greenlands-Forest-Look-v1.webp`. Supersedes the ground parts of [GFX-01](GFX-01-graphics-quality-plan.md) WP3 (terrain material) and WP4 (ground cover). Everything else in GFX-01 stands.

## Gap (evidence, verified)
1. The ground is a static mesh, not a landscape: `build_silverbrook.py:717-783` builds `SM_Silverbrook_Terrain` with GeometryScript from the 5 m `Heightfield` grid (`heightfield.py:97-103`, `spacing=5.0`), 501 x 501 vertices over the 2,500 m zone. Roads, plazas and fields are vertex colours G/B/A on that 5 m grid (`build_silverbrook.py:736-775`), so a road edge is a 10 m ramp (`ROAD_RAMP = 2 * HF.step`) that the material sharpens with noise. No Landscape Grass Types, no landscape layers, no landscape LOD, no RVT.
2. Ground cover is PCG at 8,000 grass clumps/ha (`greenlands-look-profile.json`, role `grass_clump`), i.e. 0.8/m², end-culled at 50 m, each clump 300-1,600 triangles (`prepare_arrival_assets.py:61`). A photoreal field is 20-40 clumps/m² of ~40-triangle cards near the camera. The live shot shows bare green ground with a clump every metre.
3. The ground material `M_GN_Ground` (`NatureFamilies/import_ground.py`) blends one forest-floor scan to bedrock by slope and paints roads by vertex colour. There is no grass layer: open ground is a tinted `forest_ground_04` scan, which is why the far field is flat yellow-green.
4. Nothing blends props into the ground: rocks, footings and logs are hard-edged (no RVT; `r.VirtualTextures` untouched in `DefaultEngine.ini`).
5. Lighting is non-physical: a 10 lux sun with exposure pinned to EV100 2-4 (`silverbrook-lighting.json`), no `ExtendDefaultLuminanceRange`, aerial perspective scaled 8x to fake haze, LUT off.
6. Lumen, VSM, TSR, Nanite are on (`DefaultEngine.ini:16-27`); the perf probe already works (`perf_bookmark_game.py`): bookmark 01 is 9.82 ms median / 12.49 ms p95 at 1440p (`perf-01-forest-path-town.json`). There is headroom for grass.

Assets: Poly Haven CC0 only (`NatureFamilies/acquire_sources.py`), Blender/Codex meshes, nothing paid.

## Decision
- The zone ground becomes an `ALandscape` built by script from the same `Heightfield`: 1 m quads, 8 painted layers, Landscape Grass Types, two Runtime Virtual Textures, one layered landscape material. The 5 m `Heightfield` stays the single source of heights; every placement and grounding script keeps working through `HF.at()` and line traces.
- Grass, herbs, flowers and litter move from PCG to Landscape Grass Types (GPU-instanced, generated per view, zero instances on the dedicated server because `ULandscapeSubsystem` never ticks on `NM_DedicatedServer`, `LandscapeSubsystem.cpp:676`). Trees, rocks, shrubs and saplings stay in PCG (they carry collision and gameplay candidates).
- C++ is used once, editor-only, because UE 5.8 exposes no Python or Blueprint entry that creates a landscape with components (`ALandscape::Import`, `LandscapeProxy.h:1421`, is the only path; `LandscapeImportHeightmapFromRenderTarget` needs existing components).
- Scope boundary: this is the client surface and visual ground of the published base terrain. [DEC-03](DEC-03-terrain.md) (voxel density is the terrain authority) and [TER-01](TER-01-mesh-terrain-fit-check.md) are unchanged; a runtime-editable adapter is still deferred. The landscape is not edited at runtime.

All tunables below are declared in the files named with shipped defaults and bounds (per the tuning architecture); scripts refuse out-of-bounds values. Values marked (start) are the first shipped profile and may be re-tuned only through those files after a review.

## 1. Landscape conversion

### Geometry
| Item | Value |
|---|---|
| Source heights | `Heightfield` after `apply_relief()` (`build_silverbrook.py:666-715`); `HF.at(x, y)` metres, map Z = metres x 100, unchanged |
| Zone | `L['zone_bounds']` = ±1,250 m (`zone_half_extent` 1250) |
| Quads per section / sections per component | 127 / 2x2 (254 quads per component) |
| Components | 10 x 10 = 2,540 quads = 2,541 vertices per side |
| Actor location | (-125000, -125000, 0) cm; vertex (i, j) is at x = -1250 + i x 2500/2540 m |
| Scale | XY = 250000 / 2540 = 98.4252 cm (exact zone fit, no overlap with the backdrop mesh that starts at ±1,250 m); Z = 100 (uint16 32768 = 0 m, 1 unit = 1/128 m = 0.78 cm, range ±256 m) |
| Height sample | `v = round(32768 + HF.at(x, y) * 128)`; abort if any v is outside 0..65535 (then raise Z scale to 200 in `landscape_build.py`, never clamp) |
| Collision | `CollisionMipLevel = 0`, `SimpleCollisionMipLevel = 0` (full-resolution heightfield collision, server and client) |
| LOD | `LOD0ScreenSize = 1.0`, `LOD0DistributionSetting = 1.25`, `LODDistributionSetting = 2.0`; Nanite landscape off in J1 (trial in J6 with evidence) |
| Lighting | Lumen only (`force_no_precomputed_lighting` stays true); `bCastShadowAsTwoSided` false |

Flatness and alignment: pads, road beds, plaza, fields and building footprints are already flat in `Heightfield` (5 m cells). Bilinear resampling to 0.98 m keeps a flat cell exactly flat and keeps ramps linear, so the landscape agrees with `HF.at()` to under 1 cm inside every cell and buildings placed with `ground='min'` / `HF.at` (`build_silverbrook.py:93-99`) still meet the ground. Gate: `check_dressing_grounded.py` 5 cm tolerance, and a new assert in `landscape_build.py` that traces 2,000 random points plus every building pad centre and compares with `HF.at()` (max 2 cm).

### Weight fields (`GameClient/ArtSource/Greenlands/landscape_fields.py`, stdlib)
Evaluated per landscape vertex (0.98 m), from the same layout data the vertex colours used, with the road ramps narrowed because the resolution allows it. `_dist`, `in_polygon`, `_samples` and `water_gap` are reused from `heightfield.py` / `build_silverbrook.py`; road and water samples are bucketed on a 10 m grid so the 6.46 M vertex pass stays under two minutes.

| Layer | Source | Field (0..1) |
|---|---|---|
| Cobble | primary + secondary roads (`ROADS`), `PLAZAS` | `clamp(0.5 + (hw - d) / 2.0)` where d = distance to the centreline or plaza rectangle, hw = half width (same rule as `road_field`, ramp 2 m instead of 10 m) |
| PackedEarth | trails (`tier == 'trail'`) | same, ramp 1.5 m |
| Field | `FIELDS` rectangles | same rectangle field, ramp 1.5 m |
| Rock | slope | `rockiness(v)` as today: 0 at 20°, 1 at 40° (`SLOPE_FLAT_DEG`, `SLOPE_ROCK_DEG`) |
| Mud | water edges (`WATERS`, basins) | 1 within 2 m of the water edge, 0 at 7 m (smoothstep) |
| ForestFloor | canopy trunks from the district dressing results (`dress_zone.dress`, layer `canopy`) | 1 within 4 m of a trunk, 0 at 9 m (smoothstep), union over trunks |
| Meadow | macro noise + verges | `smoothstep(0.55, 0.75, n)` with n = 2-octave value noise (40 m and 15 m cells, seed `L['seed'] + 11`), plus 0.5 within 12 m of a road edge |
| Grass | remainder | 1 minus everything above |

Normalisation: priority order Cobble, PackedEarth, Field, Rock, Mud, ForestFloor, Meadow, Grass; each layer takes `min(field, remaining)`, Grass takes the remainder, written as uint8 so every vertex sums to 255 (import type Additive). Output: `GameClient/Saved/Landscape/silverbrook_height.r16` (uint16 LE, row-major, j outer) and `silverbrook_<Layer>.r8`, plus `silverbrook-landscape.json` (sizes, scale, min/max height, per-layer coverage %). Dressing must be decided before the landscape is imported, so `build_silverbrook.py` moves the `dress_zone` calls ahead of the terrain step (they only need `HF`, not the spawned mesh).

### C++ (editor-only, the only C++ in this decision)
Files: `GameClient/Source/ShatteredFrontier/Public/Editor/SFLandscapeBuilder.h` and `Private/Editor/SFLandscapeBuilder.cpp`, whole body inside `#if WITH_EDITOR`. `ShatteredFrontier.Build.cs`: extend the existing `if (Target.bBuildEditor)` line with `"Landscape", "LandscapeEditor", "UnrealEd", "AssetRegistry"`.

```cpp
UCLASS() class USFLandscapeBuilder : public UBlueprintFunctionLibrary { GENERATED_BODY() public:
  UFUNCTION(BlueprintCallable, Category = "SF|Editor")
  static ALandscape* CreateLandscape(UWorld* World, FVector Location, FVector Scale, int32 QuadsPerSection, int32 SectionsPerComponent,
      int32 ComponentsX, int32 ComponentsY, const FString& HeightmapR16Path, const TArray<FString>& LayerNames,
      const TArray<FString>& WeightR8Paths, UMaterialInterface* Material, const FString& LayerInfoPackageDir);
};
```
Body, in order: load the `.r16`/`.r8` files (`FFileHelper::LoadFileToArray`; assert sizes equal `(QuadsX+1)*(QuadsY+1)` with `QuadsX = QuadsPerSection*SectionsPerComponent*ComponentsX`); for each layer load or create `ULandscapeLayerInfoObject` at `<LayerInfoPackageDir>/LI_<Layer>` (`CreatePackage`, `NewObject` with `RF_Public|RF_Standalone`, `LayerName`, `bNoWeightBlend=false`, `FAssetRegistryModule::AssetCreated`, mark dirty); `World->SpawnActor<ALandscape>(Location, FRotator::ZeroRotator)`, `SetActorRelativeScale3D(Scale)`, `bCanHaveLayersContent=false`, `LandscapeMaterial=Material`, new `FGuid`, `SetLandscapeGuid`; fill `TMap<FGuid, TArray<uint16>>` and `TMap<FGuid, TArray<FLandscapeImportLayerInfo>>` (`LayerName`, `LayerInfo`, `LayerData`); call `Import(Guid, 0, 0, QuadsX, QuadsY, SectionsPerComponent, QuadsPerSection, Heights, nullptr, Layers, ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>())`; then `GetLandscapeInfo()->UpdateLayerInfoMap()`, `ReregisterAllComponents()`, `PostEditChange()`; return the actor. Mirror the engine's New Landscape tool for the post-import calls (`Engine/Source/Editor/LandscapeEditor/Private/`, search `->Import(`). No runtime code, no new module, no tests beyond the Python gate below.

### Python (`GameClient/ArtSource/Greenlands/landscape_build.py`, editor)
`build(world, hf, fields, material) -> unreal.Landscape`: writes the files above, calls `unreal.SFLandscapeBuilder.create_landscape(...)`, sets collision/LOD properties, labels the actor `Terrain` in folder `Ground`, assigns the RVTs (section 4), saves `/Game/Frontier/Environment/Greenlands/Landscape/` (layer infos) and runs the alignment gate. `build_silverbrook.py` replaces lines 717-783 with one call; `TERRAIN` and the `SM_Silverbrook_Terrain` asset are deleted (the script deletes the old asset if present). `/build-editor` (or Live Coding) must precede the first map build; the runbook §2 gains that line.

### Retargets
| Script | Change |
|---|---|
| `pcg_dressing._terrain()` | return `(world, get_all_actors_of_class(world, unreal.Landscape)[0])`; `ray()` keeps a hit whose actor is a `unreal.LandscapeProxy` |
| `check_dressing_grounded.py`, `docs/evidence/.../wp6-town/check_ground.py` | unchanged (they go through `_terrain()` and `ray()`); the evidence copy is historical and is not re-run |
| `build_silverbrook.surface_z`, `perf_bookmark_game.ground_z`, `townsfolk_lanes`, `silverbrook_props.dress_all` | unchanged: visibility traces or `HF.at()` |
| `docs/evidence/graphics/2026-10-21-wp6-town/capture_game.py` | finds the ground by mesh name; new captures use `review_silverbrook_in_game.py`, which does not |
| `docs/dependency-map.md` rows "Silverbrook ground, roads, river" and "Zone dressing" | rewritten to point here |

## J1b: natural mountains and the outer region

Owner feedback (editor shots 18/19): the WP5 `SM_GraniteDomeFar_*` domes (`GameClient/ArtSource/backdrop.py`, af0f932) are smooth procedural bumps that read as "egg carton foam", and at least one stands over The Commons. J1b replaces the whole WP5 backdrop (domes, `SM_MountainRing`, the white `Backdrop` region mesh built at `build_silverbrook.py:786-797`) with one outer landscape shaped from real elevation data.

### Representation: a second `ALandscape`, not meshes
One outer `ALandscape` (same C++ helper as J1) rather than distance-only Nanite meshes, because: it shares `M_GN_Landscape`, the weight rules and the grass types, so the zone edge has no material or grass seam; its collision keeps players and townsfolk from walking off the zone; its LOD is distance-driven with no hand-made LOD rings; and 10 m quads over ±12.7 km cost the same 2,541² grid as the zone landscape. Nanite meshes would need their own material, no grass, and a hand-authored join. The two landscapes are separate actors (different scales), joined by a skirt (below).

| Item | Value |
|---|---|
| Quads per section / sections / components | 127 / 2x2 / 10x10 → 2,540 quads, 2,541 vertices per side |
| Scale | XY 1000 cm (10 m quads, ±12,700 m: the whole 10 x 12.5 km mainland of `greenlands-region-layout.json` plus sea); Z 900 (range ±2,304 m, 7 cm steps) |
| Actor | `OuterTerrain`, folder `Ground`, location (-1270000, -1270000, 0) |
| Collision | `CollisionMipLevel` 1, `SimpleCollisionMipLevel` 1 (20 m collision grid; ~3 MB on the server) |
| RVT | not written (the RVT volumes stay at the zone bounds); `virtual_texture_render_pass_type` Never |
| Grass | the shared material's grass output applies, so grass continues past the zone edge within cull range |

### Heights (`GameClient/ArtSource/Greenlands/outer_heightfield.py`, stdlib + numpy)
Source DEM: **Copernicus GLO-30** (30 m), tile `Copernicus_DSM_COG_10_N47_00_E011_00_DEM`, downloaded with no login from the AWS Open Data bucket `https://copernicus-dem-30m.s3.amazonaws.com/Copernicus_DSM_COG_10_N47_00_E011_00_DEM/Copernicus_DSM_COG_10_N47_00_E011_00_DEM.tif` (verified reachable 2026-10-21), read with `tifffile` + `numpy` (`py -3 -m pip install tifffile numpy`). Window: 25.4 x 25.4 km centred on 47.42 N, 11.30 E (Karwendel and Wetterstein, Northern Limestone Alps: jagged 2,000-2,700 m peaks above 600-900 m valleys, the silhouette of the concept's far range): lat ±0.1143°, lon ±0.1685° (823 x 1,213 px), resampled bilinearly to the 2,541² grid with 30.9 m N-S and 20.9 m E-W per source pixel. Licence: Copernicus DEM, free and open for any use with the credit "© DLR e.V. 2010-2014 and © Airbus Defence and Space GmbH 2014-2018 provided under COPERNICUS by the European Union and ESA; all rights reserved", recorded in `NatureFamilies/Sources/provenance.json` (url, md5, bytes, window) and in the game's credits list. The DEM is never shown as-is: it supplies relief only, inside the region layout's own mountain bands.

Composition, in zone-local metres (`zone_region()` frame; sea = `SEA`, ASL = z + `DATUM`), per outer vertex:
1. `base` = `RegionHeightfield(R, mountains=False).at()` (new flag: the region's coast, settlement datums, rivers and roads without its procedural mountain bumps; the region layout still wins on all of those).
2. `relief` = DEM minus its 3 km Gaussian mean (so Alpine valley floors do not leak in).
3. `M` = mountain-band mask from the region's `mountains` polylines: 1 within `width/2` of the line, feathered to 0 at `width/2 + 600 m`; `bump` = the region's existing ridge profile (`height` on the line, 0 at `width/2`).
4. `E` = exclusion: 0 inside the zone square plus 300 m, smoothstep to 1 at 900 m beyond the zone edge (foothills first, peaks only from 900 m out; nothing is ever raised over the city).
5. `h = base + E · (M · (bump + k · relief) + plains_relief · relief)` with `k = 0.3 · height / p99(|relief| over M > 0.5)` per range and `plains_relief` 0.03 (bounds 0-0.1): the Northern peaks reach ~1,500 m ± 450 m with Alpine ridgelines; the plains get ±30-40 m rolling ground instead of a billiard table.
6. Inside the zone square, the outer landscape is a skirt: `h = HF.at(x, y) - 3 · smoothstep(d / 100)` where d is metres inside the edge, so it coincides with the zone landscape on the boundary and dives under it; the zone landscape draws on top, any hairline gap shows the same material beneath. `pcg_dressing.ray()` keeps the first (highest) `LandscapeProxy` hit, so grounding is unaffected.
Gate: heights within the Z range (abort, never clamp); the boundary ring (every outer vertex on the zone edge) within 2 cm of `HF.at()`; no outer vertex inside the zone square above `HF.at()`.

Values live in `silverbrook-outer.json` (`{value, min, max, unit}` like the other profiles): DEM id and window, `exclusion_m` 300/900, `feather_m` 600, `k_fraction` 0.3, `plains_relief`, `snow_line_asl_m` 1100, `snow_feather_m` 150, `skirt_drop_m` 3.

### Weights and material
`landscape_fields.outer()` writes the same `.r8` layers with these rules at 10 m: **Snow** (new ninth layer, `snow_02` 2.0 m with `snow_field_aerial` 80 m as its macro) = `smoothstep(snow_line - feather, snow_line + feather, ASL) · saturate(1 - slope / 50°)`; **Rock** = slope 0 at 25°, 1 at 45°; **ForestFloor** = ASL < 900 m, slope < 35°, 2-octave noise (900 m / 300 m cells, seed `R`'s seed + 3) > 0.45 (forest colour at distance; trees themselves come from the forest density pass as PCG pines on the 1,250-2,500 m ring); **Mud** within 60 m of the coastline; **Meadow** noise 0.55-0.75 as in section 1; **Grass** remainder; priority Snow, Rock, Mud, ForestFloor, Meadow, Grass. The zone landscape gets Snow = 0 everywhere (zero-weight layers compile out per component). `M_GN_Landscape` gains the Snow layer (Weight blend, roughness 0.45, BaseColor from the scan with no macro tint).

Recession: section 5's sky atmosphere and fog apply (aerial perspective scale 1.2, height fog contribution 0.6, fog density 0.012, volumetric view distance 200 m); the far ridges at 3-12 km sit behind atmosphere, not behind a scaled-up haze. No per-mesh fade.

### Removal and hidden editor actors
`landscape_build.py` deletes from the map: every actor in folder `Backdrop`, the actors labelled `Backdrop`, `MountainRing` and `GraniteDomeFar_*`, and the `SM_Silverbrook_Backdrop` asset; `build_silverbrook.py` drops the `backdrop()` mesh step and the `backdrop.place()` call; `backdrop.py` and `silverbrook-backdrop.json` are deleted (the `GraniteDomeFar` family assets stay on disk, unplaced). The `Sea` plane and swim volume stay (sea at `SEA`).

Editor-only overlays: the yellow, pink and blue lines, the `Tt` sprite and the giant district names in the owner's shots are editor-viewport visualisations (ZoneShape lanes, volume and spline outlines, billboard sprites, `TextRenderActor` labels). Labels are already `set_actor_hidden_in_game(True)` (`build_silverbrook.py:240`); lane shapes and volumes never render in `-game`. Rule: every debug actor a build script spawns (labels, markers, splines, lane shapes) sets `set_actor_hidden_in_game(True)`, and the graybox `Marker` boxes in folder `Places` (`build_silverbrook.py:562`) get it too; `check_hidden_in_game.py` (editor) asserts it for folders `Labels`, `Places`, `Townsfolk/Lanes` and fails the build otherwise. Review and evidence captures come only from `-game` runs (`review_silverbrook_in_game.py`), never from the editor viewport or PIE.

### Position in the job order and acceptance
J1b runs after J1 (needs the C++ helper, `landscape_fields.py` and the shared material scaffold) and before J2, since J2's material work must include the Snow layer and the outer rules. Gates: the three height gates above; `check_hidden_in_game.py` passes; perf at bookmarks 02 and 08 within +1.5 ms of J1. Review per `docs/visual-review.md` with new bookmarks in `silverbrook-reference-bookmarks.json`: `02-main-street` (north up the street to the peaks), `06-north-edge` (xy (0, -1200), look (0, -2500), look height 5 m), `07-east-edge` (xy (1200, 0), look (2500, 0), look height 5 m) and `08-high-point` (the highest `HF.at()` cell inside the Northridge polygon, look (0, -3000), look height 1,200 m, fov 70); pairs against `gameplay-hud.png` (far range, upper right) and `Silverbrook-Design-Board-v1.webp` (mountain panel). Done when the fix list has no item about mountain silhouette (repeating or symmetric bumps), backdrop inside the zone, an edge seam or step, snow/rock/forest banding, or recession.

## 2. Layered landscape material

`NatureFamilies/landscape_material.py` builds `/Game/Frontier/Environment/Greenlands/Nature/Materials/M_GN_Landscape` and `MI_GN_Landscape` with the same `node/link` helpers as `import_ground.py`, from `TerrainMaterials/landscape-layers.json` (one row per layer: Poly Haven id, tile m, tint, saturation, grass type). `acquire_sources.py` SCANS gains the new ids and downloads `arm` as well (AO in R, roughness in G; the `Rough` channel is dropped for new ids). Textures import as `T_GN_<id>_<channel>` through `import_ground.import_scan`.

| Layer | Poly Haven id (physical size) | Tile m | Blend | Notes |
|---|---|---|---|---|
| Grass | `grass_ground` (2.51 m) | 2.51 | Weight | tint measured in J3 so it matches the grass cards |
| Meadow | `park_dirt` (3.0 m) | 3.0 | Weight | dry patchy ground |
| ForestFloor | `forest_ground_04` (3.15 m, in Sources) + `forest_leaves_03` (2.3 m) | 3.15 / 2.3 | Weight | leaves mixed in where macro mask is 0.45-0.60 (as `MI_GN_ForestFloor` does with moss today) |
| Cobble | `cobblestone_large_01` (4.0 m) | 4.0 | Height | plazas and primary/secondary streets |
| PackedEarth | `rocky_trail` (2.0 m, in Sources) | 2.0 | Height | trails |
| Rock | `marble_rock_01` (2.0 m) triplanar + `mossy_rock` (3.0 m) by mask | 2.0 / 3.0 | Height | the current `MI_GN_Bedrock` pair, ported |
| Mud | `brown_mud_02` (1.3 m, in Sources) | 1.3 | Weight | banks |
| Field | `brown_mud_03` (1.3 m) | 1.3 | Weight | keeps the procedural crop rows from `import_ground.py:166` (rows along world X, 0.75 m period) |

Graph (per layer, inside one `MaterialExpressionLandscapeLayerBlend` with `layers` = eight `LayerBlendInput`, `blend_type` `LB_HEIGHT_BLEND` for Cobble/PackedEarth/Rock and `LB_WEIGHT_BLEND` for the rest, `preview_weight` 1.0 for Grass):
- UVs: `WorldPosition.xy / (tile x 100)` (world-aligned, as today). Every layer samples once per texture through `MF_GN_HexTile` (section 2a); the earlier "sample twice at tile and tile x 3.7" rule is withdrawn (two periods still each show their own grid from 30 m up).
- Macro variation: `aerial_grass_rock` (15 m aerial grass scan) diffuse sampled at 15 m (through `MF_GN_HexTile`, `CellTiles` 0.5) and at 73 m (plain), multiplied over BaseColor of Grass, Meadow, ForestFloor and Mud as `lerp(1, macro x 2, MacroAmount)` with `MacroAmount` 0.35 (bounds 0-0.6); every other layer uses the same two samples' luminance (section 2a table).
- Distance blend: `far = saturate((PixelDepth - 2500) / 3500)` (cm); beyond it BaseColor is `lerp(detail, aerial_grass_rock at 15 m x layer tint, far)` and the normal is flattened by `far` (lerp to (0,0,1)). This is what makes the far field read as photographed grass and matches section 3's card colour.
- Height blend inputs: the layer's `Displacement` R; `height_blend` sharpening is the engine's; road edges therefore show stones poking through grass over the 2 m weight ramp with no `EdgeNoise` texture.
- Outputs: BaseColor, Normal, Roughness (arm G), AO (arm R) to the material; the same BaseColor/Normal/Roughness to the RVT output (section 4). `MaterialExpressionLandscapeGrassOutput` with one `GrassInput` per layer fed by `MaterialExpressionLandscapeLayerSample(parameter_name = layer)` (section 3).
- Sampler source `SSM_WRAP_WORLD_GROUP_SETTINGS` everywhere (the existing `WRAP`), `used_with_landscape` not required (LayerBlend implies it), two-sided off, `shading_model` DefaultLit.

Painting rules are section 1's fields; nothing is hand-painted. Repaint = rebuild.

### 2a. Anti-tiling (every layer)

Owner's verdict on the south approach from 30 m up: cobble, packed earth and grass all show their tile grid at 20-200 m. The grid is the texture's period in world space; mips blur it but never remove it, so the fix is per-texel randomisation in world space, not a distance trick.

**Technique: hex-tiling (Mikkelsen 2022, "Practical Real-Time Hex-Tiling", JCGT 11(3)) in one Custom HLSL node, applied to every scan sample of every layer, plus luminance macro variation for every layer and the normal flatten with distance for every layer.** The ground is split into a world-space hexagonal lattice; each hex samples the texture at its own random offset and rotation and the three nearest hexes are blended with contrast-preserving weights, so no two patches of ground show the same texels in the same place at any distance (the lattice is world-space, so it works the same at 20 m and 300 m). Three taps per texture, no pre-processing of the scans, no noise texture, no dither, mip-correct (`SampleGrad` with rotated gradients, so hex borders show no mip seam).

Verified alternatives in UE 5.8 (`C:\Program Files\Epic Games\UE_5.8\Engine\Content\Functions\`, checked 2026-10-22) and why not:
- `Engine_MaterialFunctions03/Texturing/TextureVariation` (+ `_RotateUV`, `_RotateNormals`): per-cell offset/rotation from a `Noise Sample` mask texture, cells blended by **dither** ("Texture Variation Dithering"); needs TSR to resolve and leaves speckle in `HighResShot` captures, which the repeat gate below would read as noise. Not used.
- `Engine_MaterialFunctions01/Texturing/Texture_Bombing` (+ `TextureBomb_SingleSample`, `_POM`): **four** taps per texture with a linear blend and a `Contrast` fix-up (ghosting at cell overlaps), normals handled only by `Normal Flip`. One tap more and lower quality than hex. Not used.
- `Engine_MaterialFunctions02/MacroUVs`: example-content helper, not a variation function. No hex or stochastic tiling function ships (`Engine/Content/Functions` and `C:\UnrealSource\UE_5.8.3\Engine\Shaders\Private` searched).
- Per-cell UV rotation/offset without a blend: hard cell seams. Two incommensurate scales (the withdrawn rule): both grids stay visible from altitude. RVT alone: caches the repeat, does not remove it.

**`MF_GN_HexTile`** (built by `NatureFamilies/hex_tile.py`, idempotent, saved to `/Game/Frontier/Environment/Greenlands/Nature/Materials/MF_GN_HexTile`; `landscape_material.py` and `import_ground.py` both `import hex_tile` and call `hex_tile.build()` first; expressions via `MaterialEditingLibrary.create_material_expression_in_function` as `foliage_interaction.py` does). Function inputs (`MaterialExpressionFunctionInput`: `input_name`, `input_type`, default): `UV` Vector2 (texture tiles = world cm / TileCm); `D`, `N`, `M`, `H` Texture2D (diffuse, normal, mask = `arm` or `Rough`, displacement; `M` and `H` get a `MaterialExpressionTextureObject` of `/Engine/EngineResources/DefaultTexture` on their `Preview` pin and `use_preview_value_as_default` True so a layer without them still compiles); `Rot` Scalar `preview_value` 1.0 (random rotation strength 0-1, `use_preview_value_as_default` True); `CellTiles` Scalar 0.5 (hex width in texture tiles, same default rule). Function outputs: `D` (float4), `N` (float3, decoded tangent-space normal), `M` (float4), `H` (float). One `MaterialExpressionCustom`: `inputs` = `CustomInput` named exactly `UV, D, N, M, H, Rot, CellTiles` (in that order, each linked from its FunctionInput), `output_type` `CMOT_FLOAT4`, `additional_outputs` = `CustomOutput('Nrm', CMOT_FLOAT3)`, `('Msk', CMOT_FLOAT4)`, `('Hgt', CMOT_FLOAT1)`, `code` =

```hlsl
// as built by hex_tile.py (J2-pre r8); hex_tile.py is the source of truth
float2 p = UV / CellTiles, pw = p + Warp / CellTiles;                       // lattice from the warped coordinate (borders bend by +-Warp tiles), texels from the unwarped p (r7 review)
float2 gx = ddx(UV), gy = ddy(UV);
float2 sk = float2(pw.x - 0.57735027 * pw.y, 1.15470054 * pw.y);
int2 b = int2(floor(sk));
float3 t = float3(frac(sk), 0.0); t.z = 1.0 - t.x - t.y;
float s = step(0.0, -t.z), s2 = 2.0 * s - 1.0; int si = (int)s;
float3 w = float3(-t.z * s2, s - t.y * s2, s - t.x * s2);
int2 v[3]; v[0] = b + int2(si, si); v[1] = b + int2(si, 1 - si); v[2] = b + int2(1 - si, si);
float4 cD[3]; float3 cN[3]; float4 cM[3]; float cH[3]; float3 L = 0;
[unroll] for (int i = 0; i < 3; i++)
{
    float a = abs(v[i].x * v[i].y) + abs(v[i].x + v[i].y) + 3.14159265;
    a = fmod(a, 6.28318531); a = (a > 3.14159265) ? a - 6.28318531 : a; a *= Rot;
    float cs = cos(a), sn = sin(a); float2x2 R = float2x2(cs, -sn, sn, cs);
    float2 cen = float2(v[i].x + 0.5 * v[i].y, v[i].y / 1.15470054);
    float2 h = frac(sin(float2(127.1 * v[i].x + 311.7 * v[i].y, 269.5 * v[i].x + 183.3 * v[i].y)) * 43758.5453);
    if (Snap > 0.0) h = floor(h * Snap + 0.5) / Snap;   // offsets in whole stone courses (cobble), so borders fall on mortar lines
    float2 uv = (mul(p - cen, R) + cen) * CellTiles + h;
    float2 rx = mul(gx, R), ry = mul(gy, R);
    cD[i] = D.SampleGrad(Material.Wrap_WorldGroupSettings, uv, rx, ry);
    float3 n = UnpackNormalMap(N.SampleGrad(Material.Wrap_WorldGroupSettings, uv, rx, ry)).xyz;
    n.xy = mul(R, n.xy); cN[i] = n;
    cM[i] = M.SampleGrad(Material.Wrap_WorldGroupSettings, uv, rx, ry);
    cH[i] = H.SampleGrad(Material.Wrap_WorldGroupSettings, uv, rx, ry).r;
    L[i] = dot(cD[i].rgb, float3(0.299, 0.587, 0.114));
}
float3 Hv = float3(cH[0], cH[1], cH[2]);
float3 W = lerp(1.0, L, 0.6) * pow(w, BlendSharp) * exp2(max(HeightSharp * (Hv - max(Hv.x, max(Hv.y, Hv.z))), -3.0)); W /= (W.x + W.y + W.z);   // height tie-break capped at 8x (r5 review); BlendSharp 24 narrows the 20-50 % band to ~3 % of the cell (r7 review)
float k = rsqrt(dot(W, W));                                                                 // variance-preserving blend (Heitz-Neyret 2018): a 50/50 band keeps the texel contrast (r6 review)
float4 mD = D.SampleLevel(Material.Wrap_WorldGroupSettings, float2(0.5, 0.5), 20.0);          // texture mean = its last mip
float4 mM = M.SampleLevel(Material.Wrap_WorldGroupSettings, float2(0.5, 0.5), 20.0);
float  mH = H.SampleLevel(Material.Wrap_WorldGroupSettings, float2(0.5, 0.5), 20.0).r;
float3 nb = float3((W.x * cN[0].xy + W.y * cN[1].xy + W.z * cN[2].xy) * k, W.x * cN[0].z + W.y * cN[1].z + W.z * cN[2].z);   // mean normal is (0,0,1): scale the xy deviation only
nb = normalize(nb);
float3 Tw = normalize(cross(float3(0.0, 1.0, 0.0), VN)), Bw = cross(VN, Tw);   // world-aligned frame: u = world +x, v = world +y (UV is WorldPosition.xy); measured at Rot 0: +y 15.8, -y 11.2 at 04; the mesh's UV tangents are not this frame
Nrm = normalize(Tw * nb.x + Bw * nb.y + VN * nb.z);                              // world-space normal; the material sets tangent_space_normal False
Msk = max(mM + (W.x * cM[0] + W.y * cM[1] + W.z * cM[2] - mM) * k, 0.0);
Hgt = max(mH + (W.x * cH[0] + W.y * cH[1] + W.z * cH[2] - mH) * k, 0.0);
return max(mD + (W.x * cD[0] + W.y * cD[1] + W.z * cD[2] - mD) * k, 0.0);
```

The Custom output pins feed the four FunctionOutputs (`D` from the main output, `N` from `Nrm`, `M` from `Msk`, `H` from `Hgt`). Normal convention: scans import with `flip_green_channel` True (`import_ground.import_scan`), so `UnpackNormalMap` yields the engine's DirectX-style normal and the rotation above is the one that keeps lighting continuous across hex borders; the self-check is a `-game` capture at bookmark 04 with the sun at 20° elevation showing no brightness step along any hex edge (if a step shows, negate `a` before `cos`/`sin`; never touch the blend).

**Per layer** (`landscape_material.py` reads these from `TerrainMaterials/landscape-layers.json`, new columns `hex_cell_tiles` bounds 0.25-1.0, `hex_rot` 0-1, `macro_amount` 0-0.6; the script refuses values outside the bounds):

| Layer | Tile m | `hex_cell_tiles` (hex width) | `hex_rot` | Macro (BaseColor x `lerp(1, m x 2, macro_amount)`) | Distance |
|---|---|---|---|---|---|
| Grass | 2.51 | 0.5 (1.25 m) | 1.0 | aerial colour, 0.35 | aerial blend 25-60 m, normal flatten |
| Meadow | 3.0 | 0.5 (1.5 m) | 1.0 | aerial colour, 0.35 | same |
| ForestFloor (+ leaves) | 3.15 / 2.3 | 0.5 / 0.5 | 1.0 / 1.0 | aerial colour, 0.35 | same |
| Mud | 1.3 | 0.6 (0.8 m) | 1.0 | aerial colour, 0.30 | same |
| Cobble | 4.0 | 0.5 (2.0 m) | 0.15 (stone courses drift <= 27°) | aerial luminance, 0.25 | normal flatten only |
| PackedEarth | 2.0 | 0.5 (1.0 m) | 1.0 | aerial luminance, 0.30 | normal flatten only |
| Field | 1.3 | 0.6 (0.8 m) | 1.0 | aerial luminance, 0.30 (crop rows stay procedural) | normal flatten only |
| Rock (both sets, all three projections) | 2.0 / 3.0 | 0.5 | 1.0 | aerial luminance, 0.20 | normal flatten only |
| Snow (J1b) | 2.0 | 0.5 | 1.0 | `snow_field_aerial` luminance at 80 m, 0.12 | normal flatten only |

Graph per layer: `UV = WorldPosition.xy / TileCm` -> `MF_GN_HexTile(UV, D, N, arm-or-Rough, Displacement, hex_rot, hex_cell_tiles)`; `D x Tint x macro` -> the layer's BaseColor, `N` -> Normal, `M` channel (arm G or Rough R) -> Roughness, arm R -> AO, `H` -> the layer's height-blend input. Macro once for the whole material: `m15 = MF_GN_HexTile(WorldPosition.xy / 1500, aerial_grass_rock D, N, default, default, 1.0, 0.5).D`, `m73` = plain sample at `/ 7300`, `m = m15 x m73 x 2`; soft layers use `m.rgb`, hard layers `dot(m.rgb, (0.30, 0.59, 0.11))`. Nothing is periodic below 73 m and the 73 m term carries at most 0.35 x 0.5 contrast. Normal flatten: `far = saturate((PixelDepth - 2500) / 3500)`, `Normal = lerp(N, (0,0,1), far)` for every layer (was soft layers only). The `Mask`, `EdgeNoise` and macro-73 samples stay plain: they are 3.5-73 m fields by design.

RVT: unchanged. The RVT output takes the same hex-tiled BaseColor/Normal/Roughness, so prop blending (section 4) gets the de-tiled ground for free; with `virtual_texture_render_pass_type` Always the hex cost is paid in the main pass and once per RVT page render. If J6 needs the lever, the landscape samples its own RVT past 10 m (`Virtual Texture Only` pass type + a near/far switch): a J6 decision, not J2.

Cost (RTX 3060, 1440p x 75%, estimates to be measured by the J2 perf gate; landscape allocation 1.0 ms): 3 taps per texture instead of 1 (soft layers were going to be 2), ~40 ALU per layer for the lattice; a component with 3 active layers samples 36 textures + 5 macro/noise (was 12 + 4); rock-slope components 72 (two sets x three projections). Expected +0.4-0.7 ms over a plain 8-layer material. Levers, in order, if the gate fails: `H` through a plain sample (-25% taps; the height blend is low-frequency), Rock side projections plain, `hex_cell_tiles` 0.5 -> 1.0.

**Interim (J2-pre; yes, do it now):** the live terrain mesh's `M_GN_Ground` gets the same function. `import_ground.py`: `layer()` grows a `hex=True` path that replaces its three `smp` nodes with one `MF_GN_HexTile` call (`D`, `N`, `M` = the `Rough` texture, `H` left on its default, `Rot` 1.0 except cobble `R` 0.15, `CellTiles` 0.5) for layers `A` (forest floor), `R` (cobble), `T` (mud) and `S` (rocky trail); `B` (mossy rock) hex too (it fills the moss patches on open ground in `M_GN_Ground`; `Rot` 1.0, `CellTiles` 1.0); bedrock `C` stays plain (triplanar); the macro, `Mask` and `EdgeNoise` samples stay plain. Size: `hex_tile.py` ~70 lines, `import_ground.py` ~12 lines, re-run `py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/NatureFamilies/import_ground.py` (masters only; `MI_GN_Ground` keeps its parameters, no level rebuild), captures A1-A3 below, the repeat gate, a fresh review. About half a day; +30 taps per pixel on the mesh terrain (no per-component compile-out there), expected +0.3-0.5 ms at bookmark 01, inside the J1 "+1 ms of today" margin. Evidence `docs/evidence/graphics/<date>-gfx02-J2pre/`.

**J2-pre as shipped (r10):** `MF_GN_HexTile` inputs are `UV, D, N, M, H, Rot, CellTiles, Snap, HeightSharp, VN, BlendSharp, Warp` (VN = `VertexNormalWS`; Warp = lattice warp in tiles); its `N` output is a WORLD-space normal in the world x/y frame of the UV (`Tw = normalize(cross((0,1,0), VN))`, `Bw = cross(VN, Tw)`), and `M_GN_Ground` sets `tangent_space_normal` False (bedrock C's plain normal is wrapped into the same frame). The world-space frame is kept because the +y / -y test at `Rot` 0 showed the frame is not mirrored (04 15.8 vs 11.2); it measured neutral at `Rot` 1.0 (13.26 tangent frame, 13.28 world frame). Blend: `pow(w, BlendSharp)` with `BlendSharp` 24 (bounds 7-32); the lattice (not the texels) warped by two Simplex noises (`scale` 0.001, 1 level, ~10 m features) x `HexWarp` 0.15 tile (bounds 0-0.2: steeper warps fold the lattice); height tie-break capped at 8x; variance-preserving gain `k = rsqrt(dot(W, W))` around the last-mip mean. At full rotation 04 reads 12.9-13.3 whether the bands are wide (`BlendSharp` 7) or narrow (24, unwarped control 13.29): the loss is the scans' baked shading turned against the sun, not the blend, so `Rot` is per layer: A forest floor 0.15 (04 15.0), T mud depth-ramped `lerp(RotTNear 0.5, RotTFar 1.0, saturate((PixelDepth - 2000) / 6000))` (relief at 1-5 m: 9.8 at 0.5, 8.6 at 1.0; A3 margin rib rows: strip R 0.76 at 0.5, 0.41 ramped), S rocky trail 1.0, B mossy rock 1.0 (moss-patch motif), cobble R 0.0 + `Snap` 14; `CellTiles` 1.0; A wires `H` = forest_ground_04 Displacement with `HeightSharp` 96; `MaskNoise` 0.02; moss weight from `MaskLow` (the Displacement map at mip 6, `Low` 0.415, `InvRange` 256: the full-res threshold at the map's median had dithered 10-14 % of the ground into an A/B crossfade with a flat normal; trail stones keep the full-res `Mask`); `EdgeNoise` UV warped by the Mask warp x `EdgeWarp` 2.5.

**Acceptance (repeat gate, `tools/Measure-GroundRepeat.py`, `py -3.12` with NumPy):** three new views in `silverbrook-reference-bookmarks.json`, `fov_deg` 70, `look_height_m` 0, camera over the Valemere Road bend looking down the cobbled south approach to the South Gate (the owner's shot): `A1-south-approach-30m` `xy_m` [181, 661], `eye_height_m` 30, `look_xy_m` [90, 574] (ground in frame from 43 m to the horizon); `A2-south-approach-100m` same, `eye_height_m` 100 (58-330 m); `A3-south-approach-250m` same, `eye_height_m` 250. Each view declares three 256 x 256 px `repeat_crops` at 1920 x 1080 (`cobble` on the carriageway, `earth` on the margin, `grass` in the open field; pixel rectangles fixed by the builder at the first capture and committed with the view, no props or grass cards inside). Per crop: luminance `0.2126 R + 0.7152 G + 0.0722 B`, mean removed, Hann window, autocorrelation `ifft2(|fft2|^2)` normalised to 1 at the origin; `peak` = the maximum over 6 px < radius <= 120 px (6 px is below the smallest period in frame, 14 px for the 2.51 m grass tile at 250 m). **Gate: `peak <= 0.25` for all nine crops**, written to `repeat-measure.json`; tiled ground measures 0.5-0.9, hex-tiled ground 0.05-0.15. J1b adds `08-high-point` crops `snow` and `rock` to the same gate. Human check for the Fable review: the `A1` before/after pair (`make_pairs.py`) beside `gameplay-hud.png`'s far range; the reviewer names any repeating motif and counts its copies; two or more copies of one motif inside a crop is a FAIL.

## 3. Grass

### Cards (`NatureFamilies/build_grass_cards.py`, Blender 5.1, like the other `build_*.py`)
Sources: `grass_medium_01` and `grass_medium_02` glTF (`acquire_sources.py` MODELS gain both). For each of the 4 clumps `prepare_arrival_assets.py:61` already selects from `grass_medium_01` plus the 2 largest clumps of `grass_medium_02`: orthographic front render, 512 x 512, transparent film, flat shadeless lighting, to RGBA plus a normal pass; pack six tiles into `T_GN_GrassCards_D.png` (2048 x 1024, 4 x 2 grid, slots 0-5) and `T_GN_GrassCards_N.png`. Meshes `SM_GN_GrassCard_00..05`: three quads crossed at 0°/60°/120°, 2 x 3 subdivisions, bent 12° outward at the top, width/height from the clump's bounds, pivot at the base centre, vertex colour R = normalised height (wind and interaction weight), UVs = the clump's atlas slot. LODs: LOD0 36 tris, LOD1 12 tris (unbent, 1 x 1), LOD2 4 tris (two quads), screen sizes 1.0 / 0.35 / 0.12. Export FBX to `NatureFamilies/GrassCards/`, import with `import_nature.py` conventions, no collision, Nanite off.

Material `M_GN_GrassCard` (built by `landscape_grass.py`): masked (`T_GN_GrassCards_D` alpha, clip 0.33), `MSM_TWO_SIDED_FOLIAGE`, two-sided, subsurface = BaseColor x 0.55; BaseColor = atlas x `lerp(TintA, TintB, PerInstanceRandom)` with TintA (0.86, 0.95, 0.62) and TintB (0.62, 0.74, 0.40) (bounds 0.3-1.2 per channel); dry variant: `lerp(colour, desaturate(colour) x (1.0, 0.92, 0.70), DryAmount)` with `DryAmount` an MI parameter (Grass 0.15, Meadow 0.55); roughness 0.7; distance colour: `lerp(colour, RVT_Silverbrook_Color sampled at ObjectPositionWS.xy, saturate((PixelDepth - 4000) / 4000))` so cards past 40-80 m take the ground's colour (the ground past 25 m is the aerial grass scan, so near and far agree); WPO = `SimpleGrassWind` clamped as in `add_leaf_wind.py` (`WindIntensity` 0.6, `WindMaxCm` 6, weight = vertex colour R) plus `MF_PlayerInteraction` (`foliage_interaction.py`, unchanged); `DitheredLODTransition` on. `MI_GN_GrassCard_Grass`, `_Meadow`, `_Forest` set `DryAmount` and a 0.9 darkening for `_Forest`.

### Grass types (`NatureFamilies/landscape_grass.py`, editor)
Writes `/Game/Frontier/Environment/Greenlands/Nature/Grass/LGT_<Layer>` (`unreal.LandscapeGrassType`, `grass_varieties` list of `GrassVariety`) from the look profile: the `ground_cover` and `litter` rows lose their `place` block (so `dress_zone.py` no longer places them; it already skips roles without `place`) and gain `"landscape_grass": [{layer, mesh, density_per_10m2, start_cull_m, end_cull_m, scale, ...}]`. Bounds: density 0-400, cull 10-150 m, scale 0.3-3.0. Every variety: `use_grid` true, `placement_jitter` 1.0, `random_rotation` true, `align_to_surface` true, `cast_dynamic_shadow` false, `cast_contact_shadow` false, `affect_distance_field_lighting` false, `receives_decals` true, `instance_world_position_offset_disable_distance` 2500, `scaling` uniform, `min_lod` -1. Density is per 10 m² (engine unit) and is scaled by the layer weight at the point, so verges and road edges fade with the weight ramps.

| Grass type | Variety (meshes) | Density /10 m² | Cull start / end m | Scale |
|---|---|---|---|---|
| LGT_Grass | `SM_GN_GrassCard_00..03`, `MI_GN_GrassCard_Grass` | 220 | 30 / 55 | 0.85-1.35 |
| | `SM_GN_GrassCard_04..05` (tall) | 25 | 50 / 100 | 1.2-1.8 |
| | `SM_PH_FloweringClump_0..2` | 4 | 0 / 45 | 1.0-1.5 |
| | `SM_LeafyDandelions_00`, `_02` | 3 | 0 / 35 | 1.0-1.4 |
| LGT_Meadow | cards 00..03, `MI_GN_GrassCard_Meadow` | 90 | 25 / 50 | 0.7-1.1 |
| | cards 04..05 | 12 | 40 / 90 | 1.0-1.5 |
| | `SM_PH_FloweringClump_0..2` | 8 | 0 / 45 | 1.0-1.6 |
| | `SM_WhiteEverlasting_00..01` | 3 | 0 / 40 | 1.0-1.5 |
| LGT_ForestFloor | cards 00..03, `MI_GN_GrassCard_Forest` | 30 | 20 / 40 | 0.6-0.9 |
| | `SM_PH_fern_02_00..03` | 10 | 0 / 45 | 1.1-1.8 |
| | `SM_HerbBroadleaf_00..02` | 25 | 0 / 40 | 1.2-2.0 |
| | `SM_LeafNeedleLitter_00..02` | 12 | 0 / 25 | 1.0-2.0 |
| | `SM_MossPatch_00..02` | 4 | 0 / 35 | 1.0-2.0 |
| LGT_Mud | cards 00..03 (Grass MI) | 20 | 0 / 35 | 0.6-0.9 |
| | `SM_CobblePebbleScatter_00..05` | 6 | 0 / 25 | 0.8-1.5 |
| LGT_PackedEarth | `SM_CobblePebbleScatter_00..05` | 8 | 0 / 25 | 0.8-1.5 |
| Cobble, Rock, Field | none | | | |

Expected load at bookmark 01 (open ground): ~210 k card instances within 55 m plus ~80 k tall within 100 m, about 8 M triangles, no shadow casting. `DefaultScalability.ini` adds `sg.FoliageQuality` 1 / 3 / 3 to the three GFX-01 tiers (the engine's `[FoliageQuality@N]` sets `grass.DensityScale` 0.5 / 1.0 and `grass.CullDistanceScale`); `DefaultDeviceProfiles.ini` Standard stays the shipped default. Grass still casts no shadow at any tier; the sun's contact is carried by the ground AO and VSM from trees and props.

Colour match near/far: after J3, `tools/Measure-GroundMatch.py` (stdlib PNG reader, filters 0-4, like `lighting.py`'s writer) reads the bookmark 01 capture, averages sRGB over a 200 x 200 px patch of near grass (10 m) and of far ground (100 m, both patches declared in `silverbrook-reference-bookmarks.json`) and fails if any channel differs by more than 10/255. The fix is only the Grass layer `Tint` in `landscape-layers.json`.

## 4. Runtime Virtual Texture

`landscape_build.py` creates (idempotent) under `/Game/Frontier/Environment/Greenlands/Landscape/`:

| Asset | Properties |
|---|---|
| `RVT_Silverbrook_Color` | `material_type` BaseColor_Normal_Roughness, `tile_count` 8 (= 256 tiles), `tile_size` 2 (= 256 px) → 65,536 px over 2,500 m = 3.8 cm/texel, `tile_border_size` 2, compressed, single physical space, `remove_low_mips` 0 |
| `RVT_Silverbrook_Height` | `material_type` WorldHeight, `tile_count` 6 (= 64 tiles), `tile_size` 2 → 16,384 px = 15 cm/texel |
| two `RuntimeVirtualTextureVolume` actors | `virtual_texture_component.virtual_texture` = each asset; location (-125000, -125000, Zmin - 2000) cm, scale (250000, 250000, Zmax - Zmin + 4000) from `silverbrook-landscape.json`; `bounds_align_actor` = the landscape; folder `Ground` |
| Landscape | `runtime_virtual_textures = [Color, Height]`, `virtual_texture_render_pass_type` = Always (the landscape still draws its own material in the main pass; the RVT is only for blending), `virtual_texture_num_lods` 6 |

Material side: `M_GN_Landscape` ends in a `MaterialExpressionRuntimeVirtualTextureOutput` fed BaseColor, Roughness, Normal transformed tangent→world (`MaterialExpressionTransform`), WorldHeight = `AbsoluteWorldPosition.z`. `MF_GN_RVTBlend` (built by `landscape_material.py`): inputs BaseColor, Normal, Roughness, `BlendHeightCm`; samples both RVTs (`MaterialExpressionRuntimeVirtualTextureSample`, `virtual_texture` set, `material_type` matching); `alpha = (1 - saturate((AbsoluteWorldPosition.z - Height) / BlendHeightCm))²`; outputs the lerps. Applied, with `BlendHeightCm`: `M_GN_Rock` 45 (boulders, slabs, cobble scatter, granite domes), `M_GN_Wood` FallenLogStump instances 30, the stone-footing material that `silverbrook_props.py` assigns (its `footing` entries in `silverbrook-props.json`) 25. `DefaultEngine.ini`: `r.VirtualTextures=1` (already the engine default; declared so it cannot drift). Dedicated server: RVT pages are never rendered there.

## 5. Lighting and post (`silverbrook-lighting.json`, applied by `lighting.py`)

Switch to physical units: `DefaultEngine.ini` `[/Script/Engine.RendererSettings] r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange=True` (required for EV100 values to mean EV100). Street-lamp candela values in `lamp-fx.json` are already physical and keep their numbers; they become correctly invisible by day.

| Key | Value (start) | Bounds |
|---|---|---|
| sun.intensity_lux | 80000 | 20000-120000 |
| sun.temperature_k | 5500 | 4500-7500 |
| sun.pitch_deg / yaw_deg | -42 / 135 | -80..-10 / ±180 |
| sun.source_angle_deg | 0.535 | 0.1-5 |
| sun.cast_cloud_shadows / cloud_shadow_strength | 1 / 0.4 | 0-1 |
| sun.sky_light_intensity | 1.0 (real-time capture, lower hemisphere colour (0.08, 0.09, 0.06)) | 0.2-8 |
| aerial_perspective.distance_scale / start_depth_m | 1.2 / 0 | 0.1-10 / 0-5000 |
| atmosphere.height_fog_contribution | 0.6 | 0-1 |
| fog.density / height_falloff / start_distance_m / max_opacity | 0.012 / 0.15 / 0 / 0.8 | as today |
| fog.volumetric / scattering_distribution / extinction_scale / view_distance_m | 1 / 0.7 / 1.2 / 200 | 0-1 / 0-1 / 0.1-4 / 50-500 |
| post.exposure_min_ev100 / max_ev100 / bias_ev | 4 / 15 / 0.4 | -2..16 / -2..16 / -3..3 |
| post.exposure_speed_up / speed_down | 3 / 1 | 0.1-20 |
| post.lumen_scene_lighting_quality / final_gather_quality / scene_detail | 2.0 / 2.0 / 2.0 | 0.25-4 |
| post.lumen_skylight_leaking | 0.05 | 0-0.2 |
| post.reflection_method | Lumen | fixed |
| grade.white_temp_k | 6200 | 5000-8000 |
| grade.saturation / contrast / gamma | 1.0 / 1.03 / 1.0 | 0.5-1.5 |
| grade.film slope / toe / shoulder / black_clip / white_clip | 0.88 / 0.55 / 0.26 / 0 / 0.04 (engine defaults, declared) | engine ranges |
| grade.bloom_intensity / vignette / film_grain | 0.35 / 0.3 / 0.12 | 0-1 |
| grade.chromatic_aberration | 0 | 0-1 |
| `[SystemSettings] r.Tonemapper.Sharpen` | 0.5 | 0-1 |

LUT stays off (its sampling is still broken per the dependency map); the grade lives in the post-process volume. Clouds unchanged. `lighting.py` gains the new keys with the same `{value, min, max, unit}` shape and refuses anything outside bounds.

## 6. Performance budget and measurement

Dev box: RTX 3060 12 GB (the GFX-01 baseline machine). Target at 2560 x 1440, Standard tier (75% screen percentage, TSR), `-game`: **p95 ≤ 16.7 ms (60 fps) and median ≤ 13 ms** at bookmarks 01, 02, 04 and the new 05; worst frame ignored (first-second hitches). Allocation on top of today's 9.8 ms median at 01: grass ≤ 3.5 ms, landscape material ≤ 1.0 ms, RVT ≤ 0.5 ms, lighting changes ≤ 0.5 ms.

Measurement: `perf_bookmark_game.py <view>` (unchanged launch line in its docstring) before and after each job, results committed as `perf-<view>.json`. It gains `--stat-gpu`: at frame 300 it runs `stat gpu` and `stat foliage`, takes a `HighResShot 1` into `perf-<view>-gpu.png`, then clears the stats; the GPU rows for Grass/Foliage, BasePass (landscape) and VirtualTexture are read from that image into the job's README. New bookmark `05-market-square` in `silverbrook-reference-bookmarks.json`: `xy_m` = the arrival point (`L['arrival']`), `look_xy_m` = arrival + (35, -35), `look_height_m` 3, `fov_deg` 70 (the owner's screenshot view).

## 7. Order of work

Every job ends with the concept → result review in `docs/visual-review.md`: 1920 x 1080 `-game` captures of bookmarks 01-05 (`review_silverbrook_in_game.py`, `GFX_EVIDENCE_DIR=docs/evidence/graphics/<date>-gfx02-J<n>/`), paired with `gameplay-hud.png` (01, 02, 03, 05) and `Greenlands-Forest-Look-v1.webp` (04), judged by a fresh Fable reviewer. A job is done when its technical gates pass and the review's fix list contains no item in the job's domain (listed below); other items are carried forward, not fixed in that job. The final PASS is J6's.

| Job | Work | Technical gates | Review domain that must be empty |
|---|---|---|---|
| J0 Sources and cards | `acquire_sources.py` ids; `build_grass_cards.py`; `landscape-layers.json` | all downloads hash-verified; six cards with three LODs; atlas 2048 x 1024 | Blender render of the six cards vs the concept's foreground grass: card silhouette, blade density, colour (ART) |
| J1 Landscape | C++ helper, `landscape_fields.py`, `landscape_build.py`, retargets, runbook | `/build-editor` clean; `SILVERBROOK_BUILT` with `Terrain=` stats; alignment gate ≤ 2 cm; `DRESSING_GROUNDED floating 0`; walk review §5 (roads, bridges, player start on ground); perf 01/02 within +1 ms of today | terrain geometry, seams, collision, floating or sunk props |
| J1b Outer region | `outer_heightfield.py`, `silverbrook-outer.json`, `landscape_fields.outer()`, `RegionHeightfield(mountains=False)`, backdrop removal, `check_hidden_in_game.py`, bookmarks 06-08 | height gates (Z range, boundary ≤ 2 cm, nothing above the zone inside it); hidden-in-game check; perf 02/08 within +1.5 ms of J1 | mountain silhouette, backdrop over the city, edge seam, snow/rock/forest banding, recession (02, 06, 07, 08) |
| J2-pre Anti-tiling now | `hex_tile.py` (`MF_GN_HexTile`), `import_ground.py` hex layers, views A1-A3, `Measure-GroundRepeat.py` | `peak <= 0.25` on all nine A1-A3 crops; perf 01 within +1 ms of today; hex-border lighting self-check at 04 | repeating motifs on cobble, earth or grass at A1-A3 (section 2a) |
| J2 Ground material | `landscape_material.py` (incl. Snow, section 2a columns), texture imports, Field rows | compiles with no default-material fallback in the log; `MI_Graybox_*` absent from the map; `peak <= 0.25` on A1-A3 (section 2a) | ground texture: repetition, road edges, plaza, trail, field, rock slopes (bookmarks 02, 04, 05, A1-A3) |
| J3 Grass | `landscape_grass.py`, `M_GN_GrassCard`, look-profile rows, scalability | `Measure-GroundMatch.py` passes; grass absent on a `-server` run (`stat foliage` 0 via `memreport`); perf 01 ≤ budget | grass density, height, colour, near/far continuity, wind and player push (01, 04, 05) |
| J4 RVT | RVT assets, volumes, output node, `MF_GN_RVTBlend` on the three materials | RVT pages render (no magenta/black ground when `r.VT.Flush`); perf +≤ 0.5 ms | prop grounding look: rock bases, footings, logs |
| J5 Light and post | `silverbrook-lighting.json`, `lighting.py`, `DefaultEngine.ini` | `lighting.py` accepts the profile; exposure settles within 2 s at 01 and 02; lamps invisible at noon | exposure, haze, sky, shadows, grade (all bookmarks) |
| J6 Budget and final review | Nanite-landscape trial (keep only if p95 improves), tier check, evidence README | p95/median targets at 01, 02, 04, 05 on Standard; Compatibility ≥ 45 fps p95 | ground, grass, lighting items all PASS; remaining items are canopy or town and are handed to the passes below |

Later passes that depend on this: the forest density pass (tree and shrub `per_ha`, understory; needs J1's PCG retarget and J3's ForestFloor grass) and the town dressing refresh (WP6 rerun on the landscape; needs J1 for grounding and J4 for footings). The character pass (WP7) is independent.

## Files owned
`GameClient/Source/ShatteredFrontier/{Public,Private}/Editor/SFLandscapeBuilder.*`, `ShatteredFrontier.Build.cs` (editor line); `GameClient/ArtSource/Greenlands/landscape_fields.py`, `landscape_build.py`, `outer_heightfield.py`, `silverbrook-outer.json`, `check_hidden_in_game.py`, `region_heightfield.py` (`mountains` flag), `build_silverbrook.py` (terrain step, backdrop removal, dressing order), `pcg_dressing.py` (`_terrain`, `ray`), `GameClient/ArtSource/backdrop.py` + `silverbrook-backdrop.json` (deleted), `greenlands-look-profile.json` (ground-cover rows), `silverbrook-lighting.json`, `lighting.py`, `silverbrook-reference-bookmarks.json`, `perf_bookmark_game.py`; `NatureFamilies/acquire_sources.py`, `build_grass_cards.py`, `hex_tile.py`, `landscape_material.py`, `landscape_grass.py`, `TerrainMaterials/landscape-layers.json`, `GrassCards/`; `GameClient/Config/DefaultEngine.ini`, `DefaultScalability.ini`; `tools/Measure-GroundMatch.py`, `tools/Measure-GroundRepeat.py`; `docs/settlement-build-runbook.md` §2, `docs/dependency-map.md`. Content under `/Game/Frontier/Environment/Greenlands/{Landscape,Nature/Grass,Nature/Materials}` only.

## Risks
- **Import API drift:** `ALandscape::Import` is editor C++ with no stability promise; the helper is 80 lines and the engine's New Landscape tool is the reference if 5.8.x changes it.
- **VSM and grass:** cards cast no shadow and disable WPO at 25 m; if p95 still misses, the first lever is `LGT_Grass` density 220 → 160 and end cull 55 → 45, declared in the look profile, never a console override.
- **Heightfield fidelity:** the ground is still a 5 m heightfield with 0.7 m relief; the landscape adds resolution to materials and grass, not to the shape. Finer relief is a `Heightfield` change (spacing 2.5 m) and a separate decision.
- **Licensing:** every new asset is Poly Haven CC0 with hash provenance; the DEM is Copernicus GLO-30 under its open licence with the required credit; nothing from Fab or Megascans is referenced.

## Job log

### J0 — 2026-10-21

- Added the six new scan IDs and both grass model IDs to the hash-verified Poly Haven downloader, with `arm` for the new scans and no new `Rough` downloads. The eight-layer JSON profile uses the specified IDs, tiles, blends and grass type names. Source files remain locally restorable from `Sources/provenance.json`, following the existing source-file Git convention.
- Built six 512 px diffuse and normal tiles, two 2048 × 1024 atlases, and six card meshes with 36/12/4-triangle LODs. Technical evidence and concept-left/live-right captures are in `docs/evidence/graphics/2026-10-21-gfx02-J0/`.
- **Deviations:** The specified LOD1 "1 × 1" grid on three quads mathematically makes 6 triangles, while the same sentence requires 12. LOD1 uses 2 × 1 subdivisions per plane to meet the numeric 12-triangle gate. Blender 5.1's FBX exporter did not supply an Unreal `LODGroup` structure for these generated meshes, so each card has explicit `_LOD0`, `_LOD1`, `_LOD2` FBX files with the specified screen sizes in `GrassCards/manifest.json`; the J3 Unreal import must attach LOD1 and LOD2 and assign the final grass-card material. J0's Blender render is the live side of the six-card comparison; the five `-game` views are baseline map captures because J0 does not place cards in the level.
- Independent visual review is pending; no builder verdict is recorded here.

### J0 redo — 2026-10-21

- Replaced the sparse/dry source-clump colour bake with six dense, concept-colour blade bakes (152–192 overlapping blades/card; 2–10% dry). Cards 04/05 carry the slightly drier/seed-headed variation. The downloaded Poly Haven models remain dimension/provenance references; **deviation:** the diffuse silhouettes are authored procedural blades rather than direct orthographic source renders, and the normal atlas uses a neutral blade normal rather than source-mesh normal detail.
- **Size deviation:** source clumps 02/03 were 0.100/0.140 m tall; their card meshes are 0.240/0.230 m tall so both read at 3 m. Original dimensions and new dimensions are recorded in `GrassCards/manifest.json`. Atlas remains 2048 × 1024 with six 512 px tiles; LODs remain 36/12/4 triangles and screen sizes 1.0/0.35/0.12.
- Straight-alpha PNGs now use 3× supersampling, area downsampling and nearest-colour dilation into transparent texels. The normal atlas shares the diffuse alpha. Direct box-filter mip checks through the 4 px level found no black RGB at alpha edges (minimum channel 22–45). `six-grass-cards-compare.jpg` now shows every card on mid-grey and grass-green at native 1:1 and projected 3 m scale; independent concept/result review passed. No Unreal import or runtime acceptance was performed.
