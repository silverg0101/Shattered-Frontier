# GFX-03: Surface detail and hand-built buildings

Status: proposed for execution, 2026-10-21. Source: the owner's verdict on two live shots ("make the surfaces rough to the textures they use, so it's not flat ... there has to be something, or a mixture of techniques, to make things look better"): a house wall (flat tiled ashlar over a thin glossy plank skirt, mortar lines running unbroken round the corner) and the market street (clean boxy houses, flat-colour stall cloth, a cobble sheet with no relief). Targets: `docs/Game-Design/concept-art/UI/v5/gameplay-hud.png` (the Silverbrook town and its weathered timber, stone and roofs) and the town overview on `concept-art/Silverbrook-Design-Board-v1.webp` (the "GFX-01 board" of the WP6 evidence), plus the per-building concepts in `concept-art/Silverbrook/`. Companion of [GFX-02](GFX-02-photoreal-ground-pipeline.md) (ground); this decision owns everything above the ground: building surfaces, building geometry, weathering, decals, props.

## Gap (evidence, verified)
1. `M_SB_Scan` (`GameClient/ArtSource/Silverbrook/import_silverbrook.py:36-75`) is Diffuse + Rough + Normal only: no AO, no height, no macro variation, no detail normal. Every wall is one 2k scan tiled by world projection, so the repeat is visible at 3 m and identical on every house.
2. Displacement exists in the graph but is off: `DISPLACEMENT_CM=0` ("displacing kit boxes opens seams at hard edges", `Cottage1/review/displacement-on6cm.png`). Root cause: kit geometry is unsubdivided boxes with split normals; Nanite tessellation displaces each face along its own normal from a height that differs per face, so every hard edge cracks. Nothing masks displacement to zero at edges.
3. 27 of the 41 `kit.solid()` palettes (`SB_ForgedIron`, `SB_LeadCame`, `SB_ChimneyPot`, `SB_PlanterSoil`, gilt, bronze, steel, cloth on the old `SM_MarketStall`) are flat colour: one constant, no normal, no roughness map.
4. The kit has no plinth helper: the inn's plinth is a 10 cm-proud, 44 cm stone box (`GoldenStagInn/build_*.py:136-140`) and some packages use a plank skirt. Eaves tiles start on the wall line, ridges are bare, and chimneys are per-package ad hoc.
5. Weathering is vertex colour R (damp below 0.65 m) and G (lichen on slates) only (`silverbrook_kit.py: Geo.object`); no streaks, no moss on eaves, no edge wear, no per-house difference: every instance of a package is pixel-identical.
6. No decals anywhere in the town; `DecalActor` is never spawned by `build_silverbrook.py` or `silverbrook_props.py`.
7. Props: Poly Haven models get 1k palette scans with roughness R / metal G (`acquire_models.py`), no AO; the Fab Megascans village props (`FabSource/MedievalVillage`, migrated by `import_village.py`) are full PBR but not Nanite-checked and not RVT-blended; the old `SM_MarketStall.fbx` in the live shot is a stand-in.

Assets: Poly Haven CC0 (`Silverbrook/acquire_materials.py`, `NatureFamilies/acquire_sources.py`), Codex image generation for grayscale masks, Blender 5.1 headless, nothing paid. Fab content stays local and git-ignored. Content only under `/Game/Frontier/Environment/Greenlands/{Silverbrook,Village,Decals}`.

## Decision
- Relief is three techniques by rule, not one: **geometry** for anything the player touches or that breaks a silhouette (slates, quoins, beams, plinth, ridge, chimney), **Nanite tessellation displacement** for the large continuous surfaces (ashlar, rubble, plaster, thatch, ground cobbles once the landscape is Nanite), **normal + AO only** for small, soft or metallic surfaces. POM is the Compatibility-tier fallback for the tessellated set and nothing else.
- The edge-crack problem is solved in Blender, not the material: every displaceable face is inset 3 cm and carries vertex colour B = 1 inside, 0 on the rim, so displacement fades to zero at every hard edge. Displacement is then switched on.
- `M_SB_Scan` becomes a complete PBR master (albedo, normal, roughness, AO, height, metal) with macro variation, a detail normal and world-aligned moss, streaks and edge wear, driven by vertex colour RGBA and per-instance custom data. No flat-colour material survives: `M_SB_Solid` gains a detail normal and roughness map.
- Buildings get a kit-level plinth, ridge, eaves overhang, chimney and glass; per-house variation comes from six custom-primitive-data floats set by the town builder; dirt, puddle and crack decals are placed by script.
- No C++, no new module. Editor-only Python and Blender, same helpers as today (`node/link/out/texture` in `import_silverbrook.py`, `Geo` in `silverbrook_kit.py`).

All tunables are declared in the files named, with shipped defaults and bounds; scripts refuse out-of-bounds values (tuning architecture). Values marked (start) are the first shipped profile.

## 1. Relief: which technique where

| Technique | Use for | Never for | Cost (RTX 3060, 1440p Standard) |
|---|---|---|---|
| Baked geometry (kit boxes/prisms) | slates, ridge tiles, quoins, voussoirs, beams, sills, plinth course, rafter tails, chimney, steps: relief ≥ 2 cm or anything with collision or a silhouette | large flat fields of texture | Nanite: ~free per triangle; keep each package under 400 k triangles |
| Nanite tessellation displacement | ashlar, rubble, plaster, thatch, boarding, cobble/packed-earth ground layers: continuous surfaces of 0.5-3 cm relief | faces narrower than 15 cm, single-element `rect` faces, anything non-Nanite, translucent | ≤ 1.2 ms at the market bookmark (measured gate, §1.5) |
| POM (`/Engine/Functions/Engine_MaterialFunctions01/Texturing/ParallaxOcclusionMapping`) | the same surfaces on the Compatibility tier (`r.Nanite.Tessellation 0`), 8 min / 16 max steps, height ratio 0.02 | Standard/Enhanced (tessellation is on), silhouettes | ~0.3 ms full-screen; 0 when the tier switch disables it (`POMEnabled` static switch) |
| Normal + AO only | fabric, leather, lead, iron, glass, far LODs, props under 40 cm | surfaces the camera reaches within 1.5 m | 0 |

### 1.1 Enabling tessellation (UE 5.8)
- `GameClient/Config/DefaultEngine.ini`: `[/Script/Engine.RendererSettings] r.Nanite.AllowTessellation=1` (already set, read-only at startup) and `[SystemSettings] r.Nanite.Tessellation=1` (runtime toggle; declared so it cannot drift). `DefaultScalability.ini`: `r.Nanite.Tessellation` 0 / 1 / 1 under the three GFX-01 tiers, next to GFX-02's `sg.FoliageQuality` line.
- Material (`import_silverbrook.scan_master`): `m.set_editor_property('enable_tessellation', True)`; `displacement_scaling.magnitude = 3.0` (cm), `center = 0.5`; the `Displacement` input of `MaterialExpressionMakeMaterialAttributes` is wired (the master already uses attributes because `MP_Displacement` is not exposed to `connect_material_property`). Keep the existing guard: never wire Displacement with `enable_tessellation` False (Nanite rejects the material and draws the fallback).
- Mesh: Nanite enabled (already, `import_mesh`). Nothing else: tessellation is adaptive in screen space and needs no mesh flag.
- Graph (replaces the `hgt/ds/half/disp` nodes): `disp = 0.5 + (Height.R - HeightMean) * DisplacementScale * VertexColor.B` where `HeightMean` is a scalar parameter per MI (the scan's mean height, computed in Blender from `_pixels(asset, 'Displacement').mean()` and written to the manifest row as `height_mean`), `DisplacementScale` a scalar parameter 0-1 (bounds enforced in `instance()`), `VertexColor.B` the edge mask of §1.2. Effective relief = ±3 cm × DisplacementScale × (h - mean). Because the mean is subtracted per MI, the surface never bulges off its wall plane and the master's single `center` is correct for every scan.

### 1.2 Edge mask (Blender, `silverbrook_kit.py`)
`kit.scan(name, asset, ..., relief=0.0)`: `relief` is the MI's `DisplacementScale`; `relief > 0` puts the material in `DISPLACED`. `Geo.poly(points, mat, uv=None, ...)`: when `mat.name in DISPLACED`, `uv is None` (world-projected face), the polygon is a quad and its shorter side is > 0.15 m, the face is emitted as five polygons: the quad shrunk 3 cm along both in-plane axes (`cut = min(0.03, 0.3 * shortest side)`), plus four rim quads. Weathering per corner becomes `(R, G, B, A)` with `B = 1` on the inner quad's corners, `0` on the outer rim corners, and `A = 1` on all five (A marks "displaceable face"; every other face keeps `B = 0, A = 0`). `Geo.object()` writes `col.data[li].color` per corner instead of per polygon. UVs for the five polygons come from `_project` as before (world projection is continuous across the split). `box()` passes its six faces through `poly()` unchanged, so walls, plinths, gables and roof decks pick this up with no per-package edits; `rect`, `grain` and `band` faces (slates, quoins, beams) are excluded by the `uv is None` test and stay undisplaced. Triangle cost: +4 faces per displaceable face, about +6 % per package.

Gate: re-export Cottage1 with `SB_Ashlar relief=0.8`, import, capture the §5 bookmark 06 view with `r.Nanite.Tessellation 1` and `0`; no gap wider than 1 px at any box edge at 1440p (compare to `Cottage1/review/displacement-on6cm.png`).

### 1.3 Relief per surface (start values; bounds 0-1, declared in the manifest rows)
| Material (scan) | `relief` | Note |
|---|---|---|
| `SB_Ashlar` (`stone_wall_04`), `SB_TownHall_Ashlar`, `SB_TempleOfTheDawn_*` (`sandstone_blocks_05/08`), `SB_Stables_Stone`, `SB_Watchtower_Stone` | 0.8 | ±2.4 cm: mortar recesses read in raking light |
| `SB_WallSection_Masonry`, `SB_Workshop_Rubble`, bridge masonry (`rough_block_wall`, `stone_wall`, `stone_brick_wall_001`) | 1.0 | rubble is the roughest |
| `SB_LimePlaster` (`patterned_clay_plaster`) | 0.3 | lumps, not craters |
| `SB_Slate` (`grey_roof_tiles_02`) and every `tile_plane` slate | 0 | slates are geometry already (individually tilted boxes) |
| Thatch (`reed_roof_04`) and roof planes that are a single textured deck | 1.0 / 0.5 | |
| `SB_OakTimber`, `SB_DoorBoards`, shutters, decking (`weathered_brown_planks`, `old_planks_02`) | 0 | beams are `grain`/`band` faces (excluded); the detail normal carries the grain |
| Paving on bridges/docks (`medieval_blocks_03`, `cobblestone_floor_01`) | 0.7 | walkable: collision is unchanged (complex-as-simple on the undisplaced mesh; ±2 cm is below the capsule step) |
| Cloth, leather, rope, iron, lead, glass | 0 | normal + roughness only |
| GFX-02 landscape layers Cobble / PackedEarth / Rock | 1.0 / 0.5 / 0.6 | only when GFX-02 J6 keeps the Nanite landscape (§1.4) |

### 1.4 Ground layers (shared with GFX-02)
Tessellation needs a Nanite landscape, which GFX-02 J1 ships off and J6 trials. Rule: `landscape_material.py` builds `M_GN_Landscape` in material-attributes mode with a `Displacement` input = the `LandscapeLayerBlend` of each height layer's `Displacement` R (the same samples that drive the height blend) × a `GroundRelief` scalar parameter per layer, `enable_tessellation` false and `Displacement` unwired by default. GFX-03 K6 flips both on only if J6 recorded the Nanite landscape as kept; otherwise the ground stays normal + height-blend (no POM on the landscape: its cost over the whole screen is not worth 2 cm of cobble). The RVT output and `MF_GN_RVTBlend` are untouched: RVT samples do not see displacement, and the 25-45 cm blend heights hide that.

### 1.5 Budget and cost controls
Measured with `perf_bookmark_game.py` at GFX-02's bookmarks 05 plus the new 06 and 07 (§5), same machine, same tier. GFX-03 may add at most **2.0 ms median** on top of whatever GFX-02 left: tessellation ≤ 1.2 ms, `M_SB_Scan` v2 over v1 ≤ 0.5 ms, decals ≤ 0.3 ms. The overall cap is GFX-02 §6 (p95 ≤ 16.7 ms, median ≤ 13 ms); if the two specs together miss it, the levers in order are: (1) `relief` on plaster and timber to 0, (2) `relief` on ashlar 0.8 → 0.5, (3) decal `fade_screen_size` 0.01 → 0.03, (4) `r.Nanite.Tessellation 0` on Standard (declared in `DefaultScalability.ini`, never a console override). Texture memory: 4 × 2k BC-compressed maps per scan (≈ 13 MB), ≤ 45 scans in the town library (≈ 600 MB streamed, 12 GB card); no 4k.

## 2. Complete PBR sets

### 2.1 Textures
`Silverbrook/acquire_materials.py` downloads `Diffuse, nor_gl, Displacement, arm` (drops `Rough`: Poly Haven's `arm` packs AO in R, roughness in G, metal in B) and re-runs for every id already in `materials-provenance.json`. New ids (hash-verified on download; a 404 fails the run): `castle_brick_broken_06` (broken rubble for the lower courses of Riverside and the wall), `plastered_stone_wall` (plaster with stones showing: Northridge gables). `import_silverbrook.texture()` imports `arm` as `TC_MASKS`, sRGB off, `T_SB_<id>_arm`; `Rough` is no longer imported. Models from `acquire_models.py` write their 1k palette as the same layout (R = 1.0 AO placeholder, G roughness, B metal) so one master reads both.

Shared textures, all CC0 or generated: `T_SB_Detail_Stone_N` = `rock_surface` nor_gl (NatureFamilies Sources), `T_SB_Detail_Wood_N` = `old_planks_02` nor_gl, `T_SB_Moss_D` = `mossy_rock` Diffuse; Codex grayscale masks (`GameClient/ArtSource/Silverbrook/Masks/`, made tileable and normal-mapped by stdlib+numpy `make_masks.py`: 12 % cross-fade on all four borders, Sobel normal for the crack height): `T_SB_Grunge_Dirt` 2048² ("seamless grayscale grime cloud, soft blotches, 30 % coverage, no objects"), `T_SB_Grunge_Leak` 1024² ("grayscale vertical rain streaks from a top edge fading out downward, seamless left-right"), `T_SB_Mask_Moss` 1024² ("grayscale moss clumps, dense at the top edge thinning to none at the bottom, seamless left-right"), `T_SB_Mask_Puddle` 1024² (one irregular puddle shape, white inside, soft edge), `T_SB_Crack_H` 1024² ("grayscale height of a few hairline cracks in flagstones on mid grey, seamless").

### 2.2 Texel density
2k scans at their measured physical size (`SIZE` in the kit): stone 2-3 m → 680-1024 px/m, plaster 1 m → 2048 px/m, planks 1 m → 2048 px/m. Targets: ≥ 512 px/m on any surface the player can stand within 1.5 m of (walls, doors, plinth, steps), ≥ 256 px/m elsewhere (roofs above 3 m, chimneys). No scan is scaled from its physical size; where a scan would fall under target it is replaced, not stretched. The detail normal (0.12 m tile) carries sub-texel grain so close-ups do not go soft.

### 2.3 `M_SB_Scan` v2 (`import_silverbrook.scan_master`, nodes left to right)
Parameters (texture: `Diffuse`, `Normal`, `ARM`, `Height`, `Lichen`, `DetailNormal`; scalar: `NormalStrength` 1, `DisplacementScale` 0, `HeightMean` 0.5, `LichenAmount` 0, `MetalAmount` 0, `MacroAmount` 0.3 (0-0.6), `DetailScale` 0.12 m, `DetailStrength` 0.5 (0-1), `StreakAmount` 0.5, `MossAmount` 0, `WearAmount` 0.5, `UseInstanceTint` 0, `UseInstanceTone` 0; vector: `DarkTint` (.42,.36,.28), `ToneDark` (.55,.5,.45); custom primitive data: `InstanceTint` vector parameter with `use_custom_primitive_data=True, primitive_data_index=0` (floats 0-2), `InstanceWear` scalar index 3, `InstanceTone` scalar index 4; static switch `POMEnabled` false):
1. UVs: `uv0` = TexCoord 0; `uv1` = TexCoord 1 (the kit's `WorldUV`: (x+y)/2, z/2 on vertical faces, x/2, y/2 on horizontal); `macroUV = uv1 / 7.3 + ObjectPositionWS.xy * 0.001` (instance-unique); `detailUV = uv0 * (SIZE / DetailScale)` via `MaterialExpressionDivide` of a `TileM` scalar parameter (the scan's physical size, written per MI).
2. Samples: `Diffuse(uv0)`, `Normal(uv0)` SAMPLERTYPE_NORMAL, `ARM(uv0)` SAMPLERTYPE_MASKS, `Height(uv0)` LINEAR_GRAYSCALE, `Detail(detailUV)` NORMAL, `Grunge(macroUV)` from `T_SB_Grunge_Dirt`, `Leak(uv1 * (1, 0.5))` from `T_SB_Grunge_Leak`, `MossMask(uv1 * 0.7)`, `MossColor(uv1 / 1.5)` from `T_SB_Moss_D`, `Lichen(uv1)` as today.
3. Vertex colour `vc` (RGBA) and `VertexNormalWS`; `up = saturate(VertexNormalWS.z * 2 - 0.4)`; `vertical = 1 - saturate(abs(VertexNormalWS.z) * 3)`.
4. Base colour chain: `worn = lerp(Diffuse, Diffuse * DarkTint, vc.R)` (today's node) → `tinted = worn * lerp(1, InstanceTint, UseInstanceTint)` → `toned = tinted * lerp(1, ToneDark, InstanceTone * UseInstanceTone)` → `macro = toned * lerp(1, Grunge * 1.6, MacroAmount)` → `streak = macro * lerp(1, 0.62, Leak * vertical * StreakAmount * InstanceWear)` → `rim = (1 - vc.B) * vc.A * Grunge * WearAmount * InstanceWear`; `wornEdge = lerp(streak, streak * 1.25, rim)` (chipped stone is lighter) → `lichen` lerp as today → `moss = saturate(up * MossMask * MossAmount + vc.G * MossMask * MossAmount)`; `BaseColor = lerp(wornEdge, MossColor * (0.8, 1.0, 0.6), moss)`.
5. Normal: `n0 = lerp((0,0,1), Normal, NormalStrength)`; `n = BlendAngleCorrectedNormals(n0, lerp((0,0,1), Detail, DetailStrength))` (`MaterialExpressionMaterialFunctionCall`, `material_function = /Engine/Functions/Engine_MaterialFunctions01/Texturing/BlendAngleCorrectedNormals`, inputs `BaseNormal`, `AdditionalNormal`); `Normal = lerp(n, (0,0,1), moss * 0.6)`.
6. Roughness: `ARM.G + (Grunge - 0.5) * 0.15 * MacroAmount + rim * 0.1`, then `lerp(., 0.9, moss)`, then `lerp(., 0.85, Leak * vertical * StreakAmount * InstanceWear * 0.5)`; saturate.
7. AO: `ARM.R` → `AmbientOcclusion`. Metallic: `ARM.B * MetalAmount`.
8. Displacement: §1.1 formula → `Displacement`. POM branch: under `POMEnabled`, `uv0` is replaced by `ParallaxOcclusionMapping(Height, HeightRatio 0.02, MinSteps 8, MaxSteps 16)` for the Diffuse/Normal/ARM samples and Displacement is 0.5; the switch is true only in the MIs' Compatibility variant (`MI_<name>_Compat`, selected by `DefaultDeviceProfiles.ini` material quality level `MaterialQualityLevel 0` through a `QualitySwitch` node; the Standard path has no POM cost).
9. `MakeMaterialAttributes` → `MP_MATERIAL_ATTRIBUTES`; `use_material_attributes` true, `enable_tessellation` true, `displacement_scaling` (3.0, 0.5).

`instance(row)` sets every parameter from the manifest row: `relief` → `DisplacementScale`, `height_mean`, `tile_m`, `moss`, `streak`, `wear`, `instance_tint`, `instance_tone`, `detail` (`stone` or `wood` detail normal); bounds checked, assertion on anything outside 0-1.

### 2.4 `M_SB_Solid` v2
Adds `DetailNormal` (texture, default `T_SB_Detail_Stone_N`), `DetailScale` 0.08 m on `uv1`, `DetailStrength` 0.35, and `Grunge` roughness variation (`Roughness + (Grunge(macroUV) - 0.5) * 0.2`). Iron rows get `detail = 'stone'` at 0.5 (hammered), lead 0.2, gilt/bronze 0.25, soil `T_SB_Detail_Stone_N` 0.8 at 0.05 m. Cloth and rope rows stop being solids: the kit maps `rough_linen`, `hessian_380`, `velour_velvet` (already in `Textures/`) with `relief 0`. After K2 no `kit.solid()` renders without a normal map; the review checks it with `r.Nanite.Visualize` off and a 0.5 m close-up of iron and lead.

### 2.5 Anti-tiling
Three layers, all above: macro variation (`Grunge` at 7.3 m, instance-offset), the world-aligned moss/streak blend (`up`/`vertical` from `VertexNormalWS`, so the pattern follows orientation rather than the tile), and the detail normal at 0.12 m. Plus the kit's existing single-element `rect` sampling for quoins, slates and voussoirs, which already breaks the repeat on dressed stone. No texture bombing: it doubles sample cost and the three layers already remove the visible repeat at 3-40 m (check at bookmark 06).

### 2.6 Glass (`M_SB_Glass`, new master)
Opaque DefaultLit: BaseColor (0.02, 0.02, 0.025), Roughness 0.06, Specular 1.0, Metallic 0, Normal = `T_SB_Detail_Stone_N` at 0.4 m × 0.15 (old crown-glass waviness). Emissive = (1.0, 0.5, 0.14) × 2.0 × `MPC_Silverbrook.NightLit` (new `MaterialParameterCollection` under `/Game/Frontier/Environment/Greenlands/Silverbrook/MPC_Silverbrook`, scalar `NightLit` 0-1) × `0.7 + 0.3 * PerInstanceRandom`. `lamp_fx.py` sets `NightLit` with the lamps (1 when lamps are lit, 0 by day) through `MaterialParameterCollectionInstance`. The kit's `glass` rows map to `MI_SB_LitGlass` on this master; the always-on orange glow of the daylight shots ends.

## 3. Buildings that read as hand-built

### 3.1 Kit additions (`silverbrook_kit.py`, used by every `build_*.py`)
- `plinth(G, M, outline, height=0.6, proud=0.06, bevel=0.03, block=(0.6, 0.9), gaps=())`: a dressed-stone course along the footprint outline (list of (c, o, half) wall frames), 0.5-0.7 m tall (bounds), 6 cm proud, each block its own `box` with `rect=rng.choice(stone_rects)` (single-element sampling, like quoins), top chamfer `bevel`, bottom 2 cm below grade (hidden in the ground, so the plinth never floats on a 2 cm landscape step). `gaps`: (a0, a1) spans left open for steps and doors. Replaces every plank skirt and the inn's 10 cm box plinth.
- `ridge(G, O, e, L, mat, pitch=0.33, r=0.09)`: half-round ridge tiles as 12-segment `vessel` half cylinders along the ridge line, overlapping 3 cm, random yaw ±2°, `SB_Slate` or `SB_RidgeClay` (`roof_tiles_14`), weathering `(0.3, lichen 0.4)`.
- `eaves(...)`: `tile_plane` is called with `O` moved 0.35 m beyond the wall face along `-u` (overhang, bounds 0.25-0.5) and `rafter_tails(G, O, e, L, mat, pitch=0.6, size=(0.08, 0.12, 0.45))` adds the exposed rafter ends under it; `deck` extends the same 0.35 m.
- `chimney(G, M, xy, z_base, z_top, size=(0.9, 0.6), rects)`: dressed-stone stack of `box` blocks with `rect` patches, 12 cm cap course 10 cm proud, two `SB_ChimneyPot` pots (now `M_SB_Solid` v2 with the stone detail normal), soot: `w=(0.7, 0)` on the top 0.4 m.
- `window(...)` unchanged except glass → `M_SB_Glass`; `door(...)` unchanged (already quoined and voussoired).
- Trim sheet `T_SB_Trim_{Diffuse,nor_gl,arm,Displacement}` 2048² built by `build_trim.py` (Blender, numpy): eight 256 px strips cut from the scans with `bands()`/`patches()`: oak beam face, beam chamfer, window frame, sill, plinth cap, quoin face, barge board, door frame. `kit.trim(name)` registers it with `SCALE` 1 and `frame()`-local V in the strip; `window()` frames, sills and `plinth()` caps use it so frames stop sampling a random plank interior and get a consistent, beveled profile with its own normal and height. Beams keep `band` sampling (it already works as a trim strip).

### 3.2 Weathering
| Effect | Where it comes from | Values (start, bounds) |
|---|---|---|
| Grime at the base | kit `damp` (vc.R below 0.65 m) as today, plus `plinth` blocks `w=(0.45, 0)` | 0.65 m (0.3-1.0) |
| Moss on roof edges | `tile_plane(..., lichen=lambda p: moss_eave(p))`: `moss_eave = 0.6 * smoothstep(1.2, 0, slope distance from the eave) * (1.5 if the plane faces north (e × u).y > 0 else 1)` → vc.G; `MossAmount` 0.8 on slate MIs, 0.4 on stone, 0 on timber/plaster | per MI |
| Leak streaks | material `Leak × vertical × StreakAmount × InstanceWear` | `StreakAmount` 0.5 plaster, 0.35 stone, 0 timber |
| Edge wear | `rim = (1 - vc.B) × vc.A` from the §1.2 inset × grunge × `WearAmount × InstanceWear` | `WearAmount` 0.6 stone, 0.3 plaster |
| Lichen on slates | as today (vc.G × `Lichen`) | unchanged |

### 3.3 Per-house variation (`build_silverbrook.piece`, `silverbrook-house-variation.json`)
`piece()` derives six floats from a stable hash of the actor label (`hashlib.sha1(label).digest()`), chosen from the declared table, and sets them on the building and on every part `place_parts.attach` returns (`a.static_mesh_component.set_custom_primitive_data_float(i, v)` for i in 0..5; door leaves must match their frame):

| Index | Meaning | Table (start) | Bounds |
|---|---|---|---|
| 0-2 | `InstanceTint` RGB (plaster lime wash; MIs with `UseInstanceTint` 1) | five washes: (1,1,1), (0.98,0.95,0.86), (0.93,0.9,0.84), (0.9,0.93,0.9), (0.97,0.9,0.82) | 0.75-1.05 per channel |
| 3 | `InstanceWear` | by district: The Commons 0.4, Northridge 0.3, Westfields 0.6, Eastwood 0.5, Riverside 0.8, ±0.15 jitter | 0-1 |
| 4 | `InstanceTone` (timber stain / roof tone; MIs with `UseInstanceTone` 1) | uniform 0.2-0.8 | 0-1 |
| 5 | reserved (shutter paint later) | 0 | |

Nothing else changes per house: same meshes, same materials; two neighbouring cottages differ in wash, stain, roof tone and wear, which is what the concept shows.

### 3.4 Decals (`place_decals.py`, editor; called by `build_silverbrook.py` after `silverbrook_props.dress_all()`)
Materials under `/Game/Frontier/Environment/Greenlands/Decals/`, built by `import_silverbrook.py`: domain `MD_DEFERRED_DECAL`, blend `DBM_DBUFFER_COLOR_NORMAL_ROUGHNESS` (`r.DBuffer` is engine-default 1; declared in `DefaultEngine.ini`):
- `M_SB_Decal_Grime`: opacity `T_SB_Grunge_Dirt` × `Strength` 0.6, BaseColor (0.12, 0.1, 0.08), Roughness 0.95, normal untouched (DBuffer colour-roughness only).
- `M_SB_Decal_Puddle`: opacity `T_SB_Mask_Puddle`, BaseColor = scene darkened (0.35, 0.35, 0.35), Roughness 0.04, Normal (0,0,1) blended fully (flat water), `Metallic` 0.
- `M_SB_Decal_Crack`: opacity `T_SB_Crack_H` thresholded (0.55-0.7), BaseColor (0.08, 0.07, 0.06), normal from `make_masks.py`'s Sobel map, Roughness 0.9.

Placement (metres, from the layout, stable hashed choice like `place_props.py`): 2-4 grime decals per building along each street-facing wall base (`decal_size` (40, 180, 90) cm, X is projection depth, rotated into the wall, 8 cm into the ground; the landscape and the plinth both receive it); puddles at local minima of `HF.at` on a 2 m grid inside `PLAZAS` and primary roads (size 150-320 cm, yaw random, at most 1 per 60 m², skipped within 1.5 m of a building); cracks 1 per 40 m² of plaza, size 250 cm. All: `fade_screen_size` 0.01, `sort_order` grime 1, crack 2, puddle 3, tag `SilverbrookDecal` (a re-run replaces), folder `Decals`. Hidden on the dedicated server by nothing: decals cost nothing without rendering. Counts are written to `SILVERBROOK_BUILT` (`Decals=`).

## 4. Props to the same standard
- Poly Haven models (`acquire_models.py`): palette becomes the ARM layout (§2.1); `kit.model()` rows get `relief 0`, `detail 'wood'` or `'stone'` by material name, `moss 0.3` for stone troughs and well heads, and the §3.3 custom data from `silverbrook_props.py`'s placement (`InstanceWear` from the district). Ground contact: every prop with a `footing` entry already takes `MF_GN_RVTBlend` from GFX-02 J4; barrels, carts and crates do not (they sit on the surface, no blend).
- Fab Megascans village (`/Game/Frontier/Environment/Greenlands/Village/`, migrated, local, git-ignored): `village_materials.py` (editor) sets `nanite_settings.enabled` on every migrated `SM_`, appends `MF_GN_RVTBlend` (BlendHeightCm 25) to the migrated `M_StandardMaster` copy's BaseColor/Normal/Roughness, and leaves the Megascans PBR as is: it already meets §2. Nothing from the pack is referenced by a committed asset; the script is the only committed artefact and refuses to run against `FabSource/`.
- Stand-ins: the old `SM_MarketStall.fbx` and any `stand_ins` row in the look profile still placed by the builder are replaced by their Silverbrook package (`MarketStall` is built); `missing_assets.py` must report no stand-in in use after K3.

## 5. Jobs

Evidence per job: `GFX_EVIDENCE_DIR=docs/evidence/graphics/<date>-gfx03-K<n>/`, 1920 × 1080 `-game` captures from `review_silverbrook_in_game.py` at bookmarks 05 (market), new **06 `inn-wall`** (the owner's wall shot: 1.4 m from the Golden Stag Inn's north-west corner, eye 1.2 m, FOV 70) and **07 `market-street`** (the owner's street shot: arrival + (8, -6), look at arrival + (40, -30), eye 1.7 m, FOV 80), both added to `silverbrook-reference-bookmarks.json`. Pairs per [docs/visual-review.md](../visual-review.md): 06 vs the crop of `concept-art/Silverbrook/The Golden Stag Inn.png` at the plinth, 07 vs the town in `gameplay-hud.png` and the board overview, package close-ups vs their own concept image. A job is done when its gates pass and the fresh reviewer's fix list has no item in its domain.

| Job | Needs | Work | Technical gates | Review domain that must be empty |
|---|---|---|---|---|
| K0 Textures, masks, trim (Codex; no editor) | nothing | `acquire_materials.py` arm + two ids; Codex masks + `make_masks.py`; `build_trim.py` | provenance rows carry `arm` md5 for every id; masks tile (border difference < 2/255 after cross-fade); trim sheet 2048² with all four channels | mask look vs concept weathering crops (ART) |
| K1 Kit relief and parts (Codex; Blender, no editor) | K0 | §1.2 inset + per-corner weathering, `relief`/`height_mean`/`tile_m` rows, `plinth`, `ridge`, `eaves`, `rafter_tails`, `chimney`, `trim`, glass rows; re-export every package (`build_*.py`), `check_exports.py` | every manifest row has `relief` in 0-1 and `height_mean`; each house package has a plinth 0.5-0.7 m, a ridge, ≥ 0.25 m eaves and a chimney where its concept shows one (asserted by the package's `review_*.py`); triangle count per package ≤ 400 k | Blender beauty renders vs each concept: plinth, eaves, ridge, chimney proportions (ART/LEVEL) |
| K2 Materials v2 (Engineer; editor) | K1 | `import_silverbrook.py`: `scan_master` v2, `solid_master` v2, `M_SB_Glass`, `MPC_Silverbrook`, decal masters, `instance()` rows; texture re-import (arm, masks, detail normals); import all packages; `DefaultEngine.ini`/`DefaultScalability.ini` tessellation lines; `lamp_fx.py` NightLit | compiles with no default-material fallback; §1.2 seam gate at 06; `r.Nanite.Tessellation 0/1` captures differ (relief is live); no `kit.solid()` MI without `DetailNormal`; perf 06/07 within +1.7 ms of before | surface relief, tiling, roughness, AO, glass (06, 07, close-ups) |
| K3 Hand-built town (Engineer; editor, map rebuild) | K2; run with or after GFX-02 J1 (the rebuild is shared, and J1 is where the ground under the plinths moves) | `build_silverbrook.piece` custom data, `silverbrook-house-variation.json`, stand-in replacement, `place_parts` custom data | `SILVERBROOK_BUILT`; `missing_assets.py` reports no stand-in; two instances of one package differ in tint/wear/tone in a side-by-side capture | building character and variation at 05, 07 and the board overview |
| K4 Decals and grounding (Codex; editor) | K3, GFX-02 J4 (RVT, so footings and decals land on the final ground) | `place_decals.py`, builder hook | `Decals=` count > 0; puddles only in plaza/road minima; no decal on a wall above 1.2 m; perf +≤ 0.3 ms | base grime, puddles, cracks at 05 and 07 |
| K5 Props (Codex; editor) | K4 | `acquire_models.py` ARM palette, `kit.model` rows, `village_materials.py` | every placed prop mesh is Nanite; no Fab path in any committed asset (`grep -r Village/ GameClient/Content` on tracked files is empty) | props at 05 and 07 (stalls, barrels, carts, well) |
| K6 Ground relief and budget (Engineer; editor) | GFX-02 J6 verdict | `landscape_material.py` Displacement input + `GroundRelief` rows, on only if the Nanite landscape was kept; tier check; final evidence README | GFX-02 §6 targets at 01, 02, 04, 05, 06, 07 on Standard; Compatibility ≥ 45 fps p95 with POM on | the combined final PASS with GFX-02 J6 (ground, grass, lighting, buildings, props) |

Order against GFX-02: K0-K2 are independent of J0-J6 and start now (they touch `Silverbrook/` only). K3 joins J1's rebuild. K4 and K5 follow J4. K6 follows J6, and the final review is shared. GFX-02 owns `landscape_material.py`; GFX-03 K6 adds only the Displacement rows to it.

## Files owned
`GameClient/ArtSource/Silverbrook/silverbrook_kit.py`, `import_silverbrook.py`, `acquire_materials.py`, `acquire_models.py`, `build_trim.py`, `Masks/` + `make_masks.py`, `place_decals.py`, `village_materials.py`, every `*/build_*.py` and `asset-manifest.json` (new rows), `*/review_*.py` (kit asserts); `GameClient/ArtSource/Greenlands/build_silverbrook.py` (`piece`, decal hook), `silverbrook-house-variation.json`, `silverbrook_props.py` (custom data), `lamp_fx.py` (NightLit), `silverbrook-reference-bookmarks.json` (06, 07); `GameClient/Config/DefaultEngine.ini` (`r.Nanite.Tessellation`, `r.DBuffer`), `DefaultScalability.ini`; `NatureFamilies/landscape_material.py` (K6 Displacement rows only); `docs/dependency-map.md`. Content under `/Game/Frontier/Environment/Greenlands/{Silverbrook,Village,Decals}` only.

## Risks
- **Tessellation at hard edges:** the inset mask fades displacement over 3 cm; if a review still finds hairlines at chamfered box corners, widen the inset to 5 cm in `Geo.poly` (one constant), never raise the master magnitude.
- **Collision vs relief:** walkable paving displaces ±2 cm visually while collision is the flat mesh; feet sink or float by that amount. Acceptable at 2 cm; if a reviewer flags it, paving `relief` drops to 0.4.
- **Custom primitive data on parts:** a door leaf spawned without the parent's floats renders with the default tint; `place_parts.attach` returns the actors so `piece()` can set them, and the K3 gate compares a door against its frame.
- **Codex masks:** generated images are not tileable; `make_masks.py` cross-fades borders and the K0 gate measures the seam. A mask that still seams is regenerated, not hand-fixed.
- **Fab licence:** the migrated village assets and `village_materials.py`'s edits stay local; nothing in the repo may depend on them (K5 gate).
