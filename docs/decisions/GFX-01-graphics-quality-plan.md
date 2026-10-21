# GFX-01: Graphics quality plan

Status: accepted for execution, 2026-10-21. Source: Fable rendering review against the owner's target reference, a realistic third-person daylight shot of a dense Greenlands forest trail with Silverbrook, its bridge and the mountains beyond in haze.

## Gap (evidence)
1. No GI or reflections: `DefaultEngine.ini` sets `r.DynamicGlobalIlluminationMethod=0`, `r.ReflectionMethod=0` and `AutoExposure=False`.
2. Baked lighting never bakes: the importers set `generate_lightmap_u_vs=False`, and there are 3,245 unbuilt objects (`docs/evidence/hud-gather-ring-live.png`).
3. Graybox terrain: `build_silverbrook.py:36-70` uses `MI_Graybox_*` with flat RGB for the ground, roads and river.
4. Ground-cover roles in `greenlands-look-profile.json` have no `place` rules: grass is 1,800/ha and fern 450/ha.
5. No backdrop: white low-poly mountains, `SM_GraniteDomeFar_*` unplaced, and no clouds or post-process volume.
6. The player is `/Engine/BasicShapes/Cube` (`SFE1Character.cpp:49`) and the townsfolk are grey mannequins.
7. No `DefaultScalability.ini` or `DefaultDeviceProfiles.ini`.
8. The town reads as a kit drop: no verges, cobbles or props, and the windows glow at noon.
9. No shadow or anti-aliasing policy (VSM and TSR undeclared).

## Plan (gain per effort; GPU ms at 1440p, estimated)
1. Lumen (software ray tracing), VSM, TSR and auto exposure, with Force No Precomputed Lighting: about 7 ms.
2. Sky, fog, clouds, post-process volume and LUT from a typed `silverbrook-lighting.json`: about 1.5 ms.
3. Real ground and water materials replace the graybox; the river becomes SingleLayerWater: about 1 ms.
4. Ground-cover `place` rules: grass 8,000/ha, fern 2,500/ha, with cull distances and wind: about 3 ms.
5. A distant mountain ring and granite domes at 700-1,500 m: under 0.5 ms.
6. Town dressing (roads, verges, props, windows lit by time of day): about 0.5 ms.
7. Player character: Manny/Quinn now, then a MetaHuman with clothing. Groom hair for the local player only: about 1.5 ms.
8. Scalability tiers: Compatibility, Standard and Enhanced, as declared config.
9. Reference camera bookmarks plus `HighResShot` before/after pairs under `docs/evidence/graphics/`.
HLOD and World Partition are deferred, since Silverbrook is a single 600 m map. The total is about 13 ms of a 16.67 ms frame.

## Work packages (each owns its files)
| WP | Owns | Done when |
|---|---|---|
| 1 Renderer config | `DefaultEngine.ini`, `DefaultScalability.ini`, `DefaultDeviceProfiles.ini` | Lumen, VSM and TSR are on, three tiers work, and the unbuilt-lighting banner is gone |
| 2 Lighting/post | `ArtSource/Greenlands/lighting.py`, `silverbrook-lighting.json`, LUT; one call in `build_silverbrook.py` | Clouds, haze, exposure and the LUT show in the bookmark capture |
| 3 Terrain/water | `build_silverbrook.py` ground section, `NatureFamilies/TerrainMaterials/` | No `MI_Graybox_*` on the ground, road or river |
| 4 Ground cover | the look-profile `layers`, `dress_zone.py`, `pcg_dressing.py`, wind on `M_GN_Leaf` | Every profile role is placed and the bookmark holds 60 fps |
| 5 Backdrop | `NatureFamilies/GraniteDomeFar/`, `backdrop.py` | A textured mountain range with haze |
| 6 Town props | `ArtSource/Silverbrook/` props, `silverbrook-props.json` | Cobbled roads, verges, props, and windows lit by time of day |
| 7 Character | `ArtSource/PlayerCharacter/`, `SFE1Character.*` | A clothed, rigged character replaces the cube |
| 8 Evidence | `review_silverbrook_in_game.py`, `docs/evidence/graphics/` | Bookmarks and before/after pairs |

## WP1 Windows graphics tiers

`DefaultDeviceProfiles.ini` ships Standard. Each preset sets `sg.AntiAliasingQuality`, `sg.GlobalIlluminationQuality`, `sg.ReflectionQuality`, `sg.ShadowQuality` and `sg.ResolutionQuality` together; the first four are integer group levels 0–3, and resolution is a screen percentage (67–100%, 1% steps). Preset CVars apply to client rendering; ray-tracing shader/permutation changes may require restart. A player-facing tier selector is not part of WP1. These are local, release-bound presentation settings, not LiveOps or server balance controls.

| Tier | Groups AA / GI / reflection / shadow | Resolution | Rendering |
|---|---|---:|---|
| Compatibility | 0 / 0 / 0 / 0 | 67% | SSGI, screen-space reflections, CSM, TAA; hardware RT off |
| Standard (shipped Windows default) | 2 / 2 / 2 / 2 | 75% | Software Lumen High, VSM, TSR; hardware RT off |
| Enhanced | 3 / 3 / 3 / 3 | 100% | Hardware RT Lumen, VSM High, TSR |

The declared presets are these three complete tuples; Standard is the shipped Windows fallback for systems where Enhanced hardware RT is unavailable. Screen percentage is bounded to 67–100%; group bounds are 0–3. Dedicated-server and NullRHI processes do not create a renderer, so these renderer CVars add no server rendering cost. WP8 reference captures force eye adaptation off and screen percentage 100 for image comparison, so they do not validate the default exposure or resolution setting. WP2 owns per-map Force No Precomputed Lighting.
## Risks
- **Performance:** Lumen plus VSM plus dense Nanite foliage is where 1440p/60 breaks. Clamp the wind distance, and measure p95 on the RTX 3060 baseline machine.
- **Dedicated server:** cosmetic PCG and foliage must not load there. Tag it and strip it on `NM_DedicatedServer`, then check with `memreport`.
- **Licensing:** Fab and Megascans content stays git-ignored and read-only; Poly Haven CC0 is safe to commit.
- **Art direction:** one `M_GN_*` master set, one LUT and one sun angle, judged only through the WP8 bookmarks.
