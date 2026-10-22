# Lighting and atmosphere blueprint

Scope: sun, sky, clouds, fog, exposure, post, renderer tiers, shadows. Lamps and other emitters: [vfx.md](vfx.md). Depth: [GFX-01](../decisions/GFX-01-graphics-quality-plan.md) (WP1 tiers, WP2), [GFX-02 §5](../decisions/GFX-02-photoreal-ground-pipeline.md). Shared rules: [README.md](README.md).

## 1. Pipeline
| # | Step | Command / file | Done when |
|---|---|---|---|
| 1 | Edit values | `GameClient/ArtSource/Greenlands/silverbrook-lighting.json` (`{value, min, max, unit}` per key; out-of-bounds aborts) | JSON valid |
| 2 | Apply (open editor) | `py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/lighting.py [--map <path>]`; also runs inside every `build_silverbrook.py` (`lighting.apply()`, idempotent, reuses existing actors) | sun, sky light, sky atmosphere, clouds, height fog, post-process volume present once each |
| 3 | Renderer config | `GameClient/Config/DefaultEngine.ini` (Lumen SW RT, VSM, TSR, auto exposure, Force No Precomputed Lighting, `r.Nanite.AllowTessellation=1`), `DefaultScalability.ini`, `DefaultDeviceProfiles.ini` (Standard shipped) | the unbuilt-lighting banner is gone; no `MI_Graybox_*` lit by baked light |
| 4 | Capture | bookmarks 01/02/04/05 via `review_silverbrook_in_game.py` (`-GfxReferenceCapture`), 1920x1080 `-game` | exposure settles within 2 s at 01 and 02 |
| 5 | Perf | `perf_bookmark_game.py <view>` (2560x1440 windowed, 6 s warmup, 600 frames) | `perf-<view>.json` p95 <= 16.7 ms, median <= 13 ms |
| 6 | Review | pairs vs `gameplay-hud.png` (haze, sky, exposure, shadow) | fresh Fable PASS |

## 2. Values
Committed today (`silverbrook-lighting.json`, WP2): sun pitch -55 deg, yaw 135, **10 lux**, 5300 K, source angle 0.8, sky light 1.0; fog density 0.03, height falloff 0.08, start 80 m, max opacity 0.9, volumetric 1; aerial perspective distance scale **8.0**, start depth 60 m; clouds bottom 4.0 km, height 6.0 km, trace 40 km; post exposure min/max EV100 **2.0/4.0**, bias 0.5, Lumen final gather / scene lighting 1.0, LUT intensity 1.0 (LUT `T_Grade_Silverbrook_LUT` 16, sampling still broken, keep off), saturation 1.06, contrast 1.04.
GFX-02 §5 physical target (J5; apply by editing the same JSON and `lighting.py` keys):
| Key | Value | Bounds |
|---|---|---|
| `DefaultEngine.ini` | `r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange=True` (required for EV100 to mean EV100) | |
| sun.intensity_lux / temperature_k | 80000 / 5500 | 20000-120000 / 4500-7500 |
| sun.pitch_deg / yaw_deg / source_angle_deg | -42 / 135 / 0.535 | -80..-10 / +-180 / 0.1-5 |
| sun.cast_cloud_shadows / cloud_shadow_strength | 1 / 0.4 | 0-1 |
| sky_light_intensity | 1.0 real-time capture, lower hemisphere (0.08,0.09,0.06) | 0.2-8 |
| aerial_perspective.distance_scale / start_depth_m | 1.2 / 0 | 0.1-10 / 0-5000 |
| atmosphere.height_fog_contribution | 0.6 | 0-1 |
| fog density / falloff / start / max opacity | 0.012 / 0.15 / 0 / 0.8 | |
| fog volumetric / scattering / extinction / view distance | 1 / 0.7 / 1.2 / 200 m | |
| post exposure min / max / bias | 4 / 15 / 0.4 EV | -2..16 / -3..3 |
| post exposure speed up / down | 3 / 1 | 0.1-20 |
| lumen scene / final gather / scene detail; skylight leaking | 2.0 / 2.0 / 2.0; 0.05 | 0.25-4; 0-0.2 |
| reflection_method | Lumen | fixed |
| grade white temp / saturation / contrast / gamma | 6200 K / 1.0 / 1.03 / 1.0 | |
| film slope / toe / shoulder / black clip / white clip | 0.88 / 0.55 / 0.26 / 0 / 0.04 | engine defaults, declared |
| bloom / vignette / film grain / chromatic aberration | 0.35 / 0.3 / 0.12 / 0 | 0-1 |
| `[SystemSettings] r.Tonemapper.Sharpen` | 0.5 | 0-1 |
Tiers (`DefaultDeviceProfiles.ini`, groups AA/GI/reflection/shadow, resolution): Compatibility 0/0/0/0 67 % (SSGI, SSR, CSM, TAA); **Standard 2/2/2/2 75 % (software Lumen High, VSM, TSR; shipped)**; Enhanced 3/3/3/3 100 % (HW RT Lumen). `r.Nanite.Tessellation` 0/1/1 and `sg.FoliageQuality` 1/3/3 per tier. No player-facing selector yet; no console overrides.

## 3. Review-only overrides (never saved)
`review_silverbrook_in_game.py` fixes the directional light at pitch -40, yaw -60, disables eye adaptation and auto exposure, exposure offset 0, motion blur off, screen percentage 100, hides debug text. `capture_game.py` scripts set `r.EyeAdaptationQuality 0`, `r.DefaultFeature.AutoExposure 0`, `r.MotionBlurQuality 0`, `r.ScreenPercentage 100`. Lamp evidence uses review sun/sky values per plan row: night must be **sun 12 deg below the horizon (pitch +12), moon 2.5 lux at pitch -35 (0.25 measured black: ground L 4; 1 / 1.5 / 2 lux gave L 7 / 11 / 15) (the `Moon` DirectionalLight from `lighting.py`, atmosphere light index 1, saved at 0), sky light 3.0 real-time capture (it carries the houses' moon shadows), fixed exposure, no bias term** (WP6i: under `r.EyeAdaptationQuality 0` no exposure bias or `r.ExposureOffset` changes a pixel). The row is right when the sky's top 60 rows read L 10-60 and the ground's bottom quarter has median L >= 8. UE sign: a DirectionalLight's pitch is the light's direction, so pitch -55 is a sun 55 deg up and pitch **+12** is 12 deg below the horizon (WP6i: pitch +6 lit the sun-gated windows and lamps; the earlier "-12" row put the sun up). These exist in `-game` only; comparisons of exposure/resolution defaults are not valid from them.

## 4. Shadows
- VSM shadow trail behind the walking player (black rectangles along the path): the UE4 mannequin physics-asset bounds miss the striding feet, VSM invalidates cached pages only under primitive bounds. Fix: `SetBoundsScale(2)` on the character mesh (`b34e72e`); `ShadowCacheInvalidationBehavior=Always` did nothing. Evidence `docs/evidence/graphics/wp7-shadow-trail/`; repro script `GameClient/ArtSource/PlayerCharacter/diagnose_shadow_trail.py`. Rule: any new skinned mesh gets a bounds check against its running pose before it ships; look back along the walked path in the review capture.
- Grass casts no shadow at any tier (`cast_dynamic_shadow` false); contact comes from ground AO and VSM from trees/props.
- Lamps: no `cast_shadows` on the 938 movable point lights (budget); use `source_radius` 15 / `soft_source_radius` 40 to tame near-field hot spots ([vfx.md](vfx.md)).
- Leaf WPO is clamped to +-6 cm (`add_leaf_wind.py` `WindMaxCm`) and disabled beyond `WPO_DISABLE_CM` because VSM cost grows with WPO.

## 5. Known failure -> rule
| Failure | Rule |
|---|---|
| No GI/reflections, `AutoExposure=False`, 3,245 unbuilt lighting objects (before GFX-01) | Lumen + VSM + TSR + auto exposure + Force No Precomputed Lighting; importers never generate lightmap UVs; nothing bakes |
| Dark scene after "Build Lighting" | no lightmap UVs and Lumen off: never build lighting; a mesh swap only discards the bake |
| 10 lux sun with EV100 pinned 2-4, aerial perspective x8 as fake haze | physical units (§2 target) once `ExtendDefaultLuminanceRange` is on; haze comes from aerial perspective 1.2 + height fog 0.012, never a scale factor |
| Lamps and windows glowing at noon | `lamp-fx.json` `lit` is `always` until the time-of-day signal; window emissive moves to `M_SB_Glass` x `MPC_Silverbrook.NightLit` (GFX-03 §2.6) and lamps become candela that are invisible by day (J5); deferred in every WP6 review, not patched per house |
| "Night" captures with the sun 10 deg up | §3 night plan values |
| Blue band under the mountains, pale flat range | recession is atmosphere + fog over real relief ([landscape-and-ground.md](landscape-and-ground.md) §3), not a tinted backdrop |
| Perf unknown ("other sessions held the GPU") | record `perf-<view>.json` on an idle machine; a missing number is a missing gate |
| Capture of OS window / editor viewport | `-game` only, 1920x1080, HighResShot; UI via `Shot showui` |

## 6. Concept refs
`concept-art/UI/v5/gameplay-hud.png` (bright hazy daylight, sun front-left, far range in haze), `concept-art/Designer (6).png` (far range crop used for the backdrop direction), `concept-art/Silverbrook-Design-Board-v1.webp` (town light).
