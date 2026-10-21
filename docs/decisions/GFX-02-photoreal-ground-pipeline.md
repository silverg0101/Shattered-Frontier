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
- UVs: `WorldPosition.xy / (tile x 100)` (world-aligned, as today). Each soft layer samples twice, at tile and at tile x 3.7 with UV rotated 90°, mixed 50/50: the two periods never coincide, which removes the visible repeat at 10-40 m. Height layers sample once (the stone pattern is the point).
- Macro variation: `aerial_grass_rock` (15 m aerial grass scan) diffuse sampled at 15 m and at 73 m, multiplied over BaseColor of Grass, Meadow, ForestFloor and Mud as `lerp(1, macro x 2, MacroAmount)` with `MacroAmount` 0.35 (bounds 0-0.6).
- Distance blend: `far = saturate((PixelDepth - 2500) / 3500)` (cm); beyond it BaseColor is `lerp(detail, aerial_grass_rock at 15 m x layer tint, far)` and the normal is flattened by `far` (lerp to (0,0,1)). This is what makes the far field read as photographed grass and matches section 3's card colour.
- Height blend inputs: the layer's `Displacement` R; `height_blend` sharpening is the engine's; road edges therefore show stones poking through grass over the 2 m weight ramp with no `EdgeNoise` texture.
- Outputs: BaseColor, Normal, Roughness (arm G), AO (arm R) to the material; the same BaseColor/Normal/Roughness to the RVT output (section 4). `MaterialExpressionLandscapeGrassOutput` with one `GrassInput` per layer fed by `MaterialExpressionLandscapeLayerSample(parameter_name = layer)` (section 3).
- Sampler source `SSM_WRAP_WORLD_GROUP_SETTINGS` everywhere (the existing `WRAP`), `used_with_landscape` not required (LayerBlend implies it), two-sided off, `shading_model` DefaultLit.

Painting rules are section 1's fields; nothing is hand-painted. Repaint = rebuild.

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
| J2 Ground material | `landscape_material.py`, texture imports, Field rows | compiles with no default-material fallback in the log; `MI_Graybox_*` absent from the map | ground texture: repetition, road edges, plaza, trail, field, rock slopes (bookmarks 02, 04, 05) |
| J3 Grass | `landscape_grass.py`, `M_GN_GrassCard`, look-profile rows, scalability | `Measure-GroundMatch.py` passes; grass absent on a `-server` run (`stat foliage` 0 via `memreport`); perf 01 ≤ budget | grass density, height, colour, near/far continuity, wind and player push (01, 04, 05) |
| J4 RVT | RVT assets, volumes, output node, `MF_GN_RVTBlend` on the three materials | RVT pages render (no magenta/black ground when `r.VT.Flush`); perf +≤ 0.5 ms | prop grounding look: rock bases, footings, logs |
| J5 Light and post | `silverbrook-lighting.json`, `lighting.py`, `DefaultEngine.ini` | `lighting.py` accepts the profile; exposure settles within 2 s at 01 and 02; lamps invisible at noon | exposure, haze, sky, shadows, grade (all bookmarks) |
| J6 Budget and final review | Nanite-landscape trial (keep only if p95 improves), tier check, evidence README | p95/median targets at 01, 02, 04, 05 on Standard; Compatibility ≥ 45 fps p95 | ground, grass, lighting items all PASS; remaining items are canopy or town and are handed to the passes below |

Later passes that depend on this: the forest density pass (tree and shrub `per_ha`, understory; needs J1's PCG retarget and J3's ForestFloor grass) and the town dressing refresh (WP6 rerun on the landscape; needs J1 for grounding and J4 for footings). The character pass (WP7) is independent.

## Files owned
`GameClient/Source/ShatteredFrontier/{Public,Private}/Editor/SFLandscapeBuilder.*`, `ShatteredFrontier.Build.cs` (editor line); `GameClient/ArtSource/Greenlands/landscape_fields.py`, `landscape_build.py`, `build_silverbrook.py` (terrain step and dressing order), `pcg_dressing.py` (`_terrain`, `ray`), `greenlands-look-profile.json` (ground-cover rows), `silverbrook-lighting.json`, `lighting.py`, `silverbrook-reference-bookmarks.json`, `perf_bookmark_game.py`; `NatureFamilies/acquire_sources.py`, `build_grass_cards.py`, `landscape_material.py`, `landscape_grass.py`, `TerrainMaterials/landscape-layers.json`, `GrassCards/`; `GameClient/Config/DefaultEngine.ini`, `DefaultScalability.ini`; `tools/Measure-GroundMatch.py`; `docs/settlement-build-runbook.md` §2, `docs/dependency-map.md`. Content under `/Game/Frontier/Environment/Greenlands/{Landscape,Nature/Grass,Nature/Materials}` only.

## Risks
- **Import API drift:** `ALandscape::Import` is editor C++ with no stability promise; the helper is 80 lines and the engine's New Landscape tool is the reference if 5.8.x changes it.
- **VSM and grass:** cards cast no shadow and disable WPO at 25 m; if p95 still misses, the first lever is `LGT_Grass` density 220 → 160 and end cull 55 → 45, declared in the look profile, never a console override.
- **Heightfield fidelity:** the ground is still a 5 m heightfield with 0.7 m relief; the landscape adds resolution to materials and grass, not to the shape. Finer relief is a `Heightfield` change (spacing 2.5 m) and a separate decision.
- **Licensing:** every new asset is Poly Haven CC0 with hash provenance; nothing from Fab or Megascans is referenced.

## Job log

### J0 — 2026-10-21

- Added the six new scan IDs and both grass model IDs to the hash-verified Poly Haven downloader, with `arm` for the new scans and no new `Rough` downloads. The eight-layer JSON profile uses the specified IDs, tiles, blends and grass type names. Source files remain locally restorable from `Sources/provenance.json`, following the existing source-file Git convention.
- Built six 512 px diffuse and normal tiles, two 2048 × 1024 atlases, and six card meshes with 36/12/4-triangle LODs. Technical evidence and concept-left/live-right captures are in `docs/evidence/graphics/2026-10-21-gfx02-J0/`.
- **Deviations:** The specified LOD1 "1 × 1" grid on three quads mathematically makes 6 triangles, while the same sentence requires 12. LOD1 uses 2 × 1 subdivisions per plane to meet the numeric 12-triangle gate. Blender 5.1's FBX exporter did not supply an Unreal `LODGroup` structure for these generated meshes, so each card has explicit `_LOD0`, `_LOD1`, `_LOD2` FBX files with the specified screen sizes in `GrassCards/manifest.json`; the J3 Unreal import must attach LOD1 and LOD2 and assign the final grass-card material. J0's Blender render is the live side of the six-card comparison; the five `-game` views are baseline map captures because J0 does not place cards in the level.
- Independent visual review is pending; no builder verdict is recorded here.
