# VFX blueprint: lamps, flames, smoke, fire

Scope: street-lamp and building-lantern FX (`FX_StreetLamp_*`), chimney smoke cards, and any new flame or fire. Material and texture authoring lives in `GameClient/ArtSource/VFX/StreetLamp/`; placement in `GameClient/ArtSource/Greenlands/lamp_fx.py` and `Silverbrook/silverbrook_props.py`. No Niagara: shared HISM cards + material graphs built by Python. Depth: `GameClient/ArtSource/VFX/StreetLamp/README.md`, `docs/evidence/graphics/lamp-fx-3/REVIEW.md`. Shared rules: [README.md](README.md).

## 1. Lamps (pipeline, in order)
| # | Step | Command / value | Done when |
|---|---|---|---|
| 1 | Textures (offline, Python 3 + Pillow/NumPy/SciPy) | `py -3 GameClient/ArtSource/VFX/StreetLamp/generate_textures.py` | PNGs committed with the script |
| 2 | Sockets | `py -3 GameClient/ArtSource/VFX/StreetLamp/extract_sockets.py` -> `lamp-sockets.json` from the `*LanternGlow`/`*_Glow` islands of the source GLBs | socket per glass chamber (3 mm agreement across four manifests) |
| 3 | Values | `GameClient/ArtSource/Greenlands/lamp-fx.json` (typed, bounded) | within bounds |
| 4 | Apply (open editor, Silverbrook map) | `py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/lamp_fx.py --save` (also runs at the end of `build_silverbrook.py`) | `LAMP_FX` counts (938 sockets/lights); only `/Game/Frontier/VFX/StreetLamp/` assets and `FX_StreetLamp_*` actors touched |
| 5 | Offsets gate | write `five-lamp-offsets.json` (world offset of every flame/halo/smoke card origin from its lamp's authored glass centre, 5 lamps) | every flame and halo origin `(0,0,0)` cm; smoke `(0,0,+45..85)`; **any flame origin > 3 cm from its own lantern mesh is a FAIL** |
| 6 | Captures | `docs/evidence/graphics/lamp-fx-3/capture_game.py` plan: day/dusk/night close on a lamp that rolled fireflies (`StreetLamp_003`), dusk/night street; night = sun -12 deg, sky 0.0, exposure 0.25; 1920x1080 `-game` with the editor closed; `flicker-00..04.png` at 0.2 s | pairs vs `concept-art/Silverbrook/Street Lamp-1.png` |
| 7 | GPU | `capture_gpu.py` FX on/off twice at 1440p (`profilegpu`) | graphics-root delta <= 0.5 ms (pass 3: 0.313 ms) |
| 8 | Review | fresh Fable | PASS (pass 3 was 0/5; §4 lists its fix list = the next pass) |

Committed values (`lamp-fx.json`): `flame_colour` [1.0,0.38,0.08], `glass_emission_strength` 1.8 (0-4), `light_intensity` 900 lm (0-1200), `light_radius` 4.0 m (0.5-5), `flicker_speed` 1.7 Hz, `flicker_amount` 0.12, `smoke_opacity` 0.09 (0-0.2), `firefly_chance` 0.28 (0-0.6), `firefly_count` 3, `lit` `always`. Cards (`lamp_fx.py apply()`): two crossed additive flame cards 10x22 cm at the socket, two halo cards 26 cm, smoke 16x25 cm at +45 cm, fireflies 1.8 cm crossed-dot cards 1.3-2.6 m away gated by sun elevation; HISM end cull 1800 cm (2500 fireflies). Point light: movable, shadowless, named `unreal.Color(r=255, g=150, b=65, a=255)`. Street-lamp glass components disable Nanite (Nanite does not render translucent glass).

## 2. Chimney smoke (WP6f values; `main-street` PASS, `chimney-smoke` pending WP6f review)
| Item | Value | Where |
|---|---|---|
| Texture | `py make_chimney_smoke.py` -> `T_ChimneySmoke.png`: V-tileable column, puffs `alpha = clip(profile * clip((n-.3)/.7) * 1.4)`, sway 0.30, **card U edges exactly 0** (`alpha *= clip((.5-|u|)/.10)`, ramp offset half a pixel), assert max 1.0 / mean ~0.30 | `GameClient/ArtSource/VFX/StreetLamp/make_chimney_smoke.py` |
| Material | `M_LampSmoke`: **lit** (`MSM_DEFAULT_LIT`, `TLM_VOLUMETRIC_NON_DIRECTIONAL`, tint -> base colour, not emissive); taper `lerp(1.6, 0.7, V)` (U squeezed x1.6 at the pot V=0, x0.7 at the top); `thin` lerp 1 -> 0.45 to the top; `ViewFade` (0 within 15 deg of edge-on, 1 past ~37 deg); `far = saturate((12000 - PixelDepth)/4000)` (full to 80 m, gone at 120 m); `sides` zeroes the card's own U edges; opacity = alpha x Opacity x EdgeFade x ViewFade x far x sides x thin | `build_materials.py build()` |
| Instance | `MI_SB_ChimneySmoke`: `opacity` **0.5** (0.2-0.7), `tint` **[0.42,0.42,0.45]** linear base colour, `pan_v` **-0.12** (negative rises; +0.12 sank, measured), `edge_fade` 1.0, `ViewFade` 1 | `silverbrook-props.json` `chimney_smoke` |
| Cards | two crossed planes 2.2 x 6.0 m, base at pot + 3.2 m (`p.z + sz*100 + 320`), yaw +35 and +125, on townhouses with `_h(label+'smoke') < .4` (~40 %) | `silverbrook_props.dress_house_details` |
| Chimney view | eye 14 m out at card centre height, look 1.5 m down so the stack top is in frame, FOV 55 | `capture_game.py` |
Acceptance (`chimney-smoke.png`): plume >= 150 x 400 px; plume luminance within 10 % of the sky 50 px either side (neutral, **not brighter than the sky**; the concept's own smoke is slightly lighter, so "15 % darker" was withdrawn); width at the frame top >= 1.8 x the width 60 px above the stack; edge x per row has sd >= 8 px over every 100-row window (puffs, no straight edge > 100 px); slate under the card keeps L <= 1.5 x clear slate (veils, does not cover); `main-street.png` threads above roofs >= 20 px wide and no brighter than the sky. Rise direction is measured from 14 `-game` frames 0.3 s apart (row-profile cross-correlation), never assumed from the spawn roll.

## 3. New flame or fire (recipe)
Copy the lamp flame: two crossed additive cards at the emitter origin (offset 0), texture from `generate_textures.py`, flicker from the two-frequency graph, a warm shadowless point light with named RGB, smoke as one lit card above the emitter using §2's material, fireflies only if asked. Sizes scale with the chamber: 16x32 cm flame in a ~30 cm lantern (pass-3 fix), halo 2-3x the glass width. Gate = §1 step 5 offsets + a night capture with the sun below the horizon.

## 4. Known failure -> rule
| Failure (owner / reviewer) | Rule |
|---|---|
| "the lamps are blue" (pass 2) | positional `unreal.Color(255,150,65,255)` is BGRA in UE Python; always `unreal.Color(r=, g=, b=, a=)`; gate: no live pixel in any view has B > R |
| Flames outside the lantern, "sperm looking things" (ribbons, velocity-aligned ember trails) | flame/halo origins at the socket (0 cm; tolerance 3 cm); no ribbons, no velocity alignment, no ember cards; fireflies are small crossed dots with WPO drift |
| Orange post from arm to base; glass a clipped yellow slab; flame a 10x46 px white sliver (pass 3) | fix list: `glass_emission_strength` 1.8 -> 0.6; flame cards 0.16x0.32 m with energy 3.5 (not 5); `light_intensity` 900 -> 450, `light_radius` 4 -> 5, `source_radius` 15, `soft_source_radius` 40, keep `cast_shadows` off |
| No smoke above the lid; halo not past the cage; mid-street lamps lose glow at 18 m | smoke card at +85 cm, 0.24x0.55 m, `smoke_opacity` 0.18; halo 0.80 m, halo/firefly end cull 4000 cm |
| No fireflies in any frame | 5 cm dots, blink exponent 4 (not 12), `firefly_chance` 0.45, capture the lamp that rolled them, night sun -12 deg |
| Chimney cards invisible (1.6x0.8 m, +0.8 m), then bright needles edge-on, then white columns lighter than the sky, then an inverted taper (WP6b-e) | §2 values; a card is 2.2x6 m, lit, grey base colour, ViewFade, zero edges, measured pan sign |
| Lamp wisp texture reused for chimneys | `T_LampSmoke` has alpha <= 0.35 over a third of its width; chimneys use `T_ChimneySmoke` via the `SmokeTex` parameter |
| Lit at noon | deferred to the time-of-day signal (`lit` enum refuses to build until it exists); lamps become candela invisible by day under physical exposure (J5) |

## 5. Concept refs
`concept-art/Silverbrook/Street Lamp-1.png` (dark amber glass, teardrop flame with pale core, plain brown post, iron bands), `Street Lamp-2..4.png`, `concept-art/UI/v5/gameplay-hud.png` (chimney plume above the mill), board tile LAMPS & STREETSIGNS.
