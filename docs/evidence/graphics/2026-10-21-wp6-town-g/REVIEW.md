# WP6g review — Silverbrook town, pass 7 (WP6f fix 1: chimney-smoke and main-street)

Reviewer: independent (Fable), did not build the work. Judged per `docs/visual-review.md` against `Silverbrook-Design-Board-v1.webp` (TOWN STREETS tile) and `UI/v5/gameplay-hud.png` (the mill chimney plume, x 1330-1360, y 100-170 of the 1672 x 941 concept). Commit 395abf6. Both captures read at full resolution plus 3x crops (`chimney-smoke.png` x 700-1240 / y 340-640 and x 760-1120 / y 0-360; `main-street.png` x 700-1250 / y 250-450); R-B excess, luminance and g-vs-f pixel diffs re-measured independently of `measure_plume.py` (same definitions, own code), and the builder's `plume-measure.json` cross-checked. Code read: the 395abf6 diff (`build_materials.py` fix 1a/1b, exactly as specified), the full `M_LampSmoke` graph, `silverbrook_props.py` (card spawn, lines 362-369 and 465-469), `silverbrook-props.json` `chimney_smoke`, `capture_game.py` (eye 14 m out at card-centre height, FOV 55: 131.7 px per metre at the stack).

**Result: 1 PASS / 1 FAIL.** main-street holds (no regression). chimney-smoke fails for the sixth time (WP6b, c, d, e, f, g). Fix 1a and 1b were implemented as written and they did not move the picture: whole-frame mean absolute difference g-vs-f 1.58 (builder: 1.18), concentrated in x 940-1050, y 300-580. This review stops prescribing card parameters and specifies the replacement technique (Niagara sprites, §Technique) with its acceptance numbers.

## WP6f fix 1 — acceptance status (measured here; builder's number in brackets where it differs)

| Acceptance | WP6g | Met |
|---|---|---|
| y 520: plume within x 820-1000, span <= 140 px | no sky-tinted plume (R-B peak 3.3 < 4); field brightening x 744-962, 219 px wide, 47 columns > 3 | No |
| y 150 span >= 1.5 x y 520 span | 140 px (864-1003) vs a 219 px field veil at y 520: 0.64 x | No |
| peak R-B excess >= 12 at y 400-520 | 17.1 [20.8 in WP6f] | Yes |
| peak R-B excess >= 8 at y 100 | 9.1 [9.5] | Yes |
| <= 160 columns of field brightening, any row y 460-520 | 262 px wide, 201 columns at y 490 (x 719-980) [249 max] | No |
| plume L / sky L within 10 % at rows 30, 80, 260 | 1.04 / 1.00 / 1.04 | Yes |
| stack top in frame | pots at y 575-620, x 880-920 | Yes |
| main-street: >= 15 columns R-B excess >= 5 above the right roofs | 10 (x 790-830, y 381) and 0 (x 1085-1125) [11 / 1 in WP6f] | No |

Four of eight. The two that matter for the shape (foot width, foot-to-top growth) are as far off as in WP6f.

## main-street — PASS (no regression)

Same street as WP6f: g-vs-f mean absolute difference 1.54, largest 120 x 120 block 5.95 at x 240-360, y 600-720 (the hay stalks' wind, as before). Cobble carriageway to both kerbs, five house tones, mixed packages, barrels, crates, hay stacks, sacks, bench, hay cart, ladder, bracket signs, washing line, lamps, nothing broken. The 3x crop of the roofline (x 700-1250, y 250-450) shows no chimney thread above any roof at 40-60 m; the fix-1 thread criterion is not met (10 and 0 columns), but the TOWN STREETS tile is judged on the street, and the thread belongs to the chimney technique below, whose acceptance includes it. Deferred items unchanged (lamps lit at noon, flat cobble relief, faceted far peaks: GFX-02 J5/J2/J1b).

## chimney-smoke — FAIL

1. **What's wrong:** there is still no column leaving the pots. At 3x (x 700-1240, y 340-640) the "plume" is a uniform soft vertical haze about 160 px (1.2 m) wide standing to the left of and over the stack, from below the pot level (it brightens the field at y 490 across x 719-980, 262 px, 2 m) to the frame top, with no origin at the pots, no puffs, no widening and no drift; it reads as a fog bank or a lens smear beside the chimney, not smoke from it. At the top (x 760-1120, y 0-360) it is a faint straight-sided veil 94-148 px wide over the mountain (R-B excess 5.5-8.6, a tint). Per-row FWHM over sky: 128 px at y 20, 166 at y 60, 62 at y 100, 60 at y 260: no monotonic growth, the width is noise. The concept's mill plume (`gameplay-hud.png`, 4x crop) leaves a ~1 m stack at about the stack's width, is lighter than what is behind it, breaks into soft rounded puffs and reaches about 2.3 x the stack width three stack-widths up, drifting to one side.
   **Fix type:** CODE (technique). **Instruction:** §Technique and the fix list. No further `M_LampSmoke` / `T_ChimneySmoke` / card parameter changes are to be made for chimneys.

**Root cause (technique, not parameters).** Two crossed static 2.2 x 6 m planes with a scrolling column texture cannot make a chimney plume from this eye, and six rounds of tuning show it:
- Width can only be a function of V (the taper), but what the camera sees is the product of that taper with the two cards' view-angle terms (`ViewFade`, the +35 / +125 deg yaws give cos 0.82 and 0.57 on the sight line) and with their overlap, which doubles alpha where both cards cross (mid-card) and leaves a one-card veil at each edge: that is the 1.2-2 m foot, whatever the taper says.
- A panning texture has no per-puff growth or dispersal: puffs cannot get bigger and fainter as they rise, only brighter or dimmer bands can slide along a fixed rectangle, so the alpha has to be driven to zero at the plane's edges and every fade fights the rectangle (the foot ramp, the head ramp, the sine before it, `thin`, `sides`).
- Static geometry does not move: nothing in the plume drifts, churns or thins over time, and in `main-street.png` at 40-60 m the cards sit near edge-on to the street and `ViewFade` removes them, so the town has no chimney smoke from the street at all.

## Technique: Niagara sprite emitter per chimney

**Decision.** One Niagara system `NS_ChimneySmoke` (`/Game/Frontier/VFX/ChimneySmoke/`), one CPU-sim sprite emitter, camera-facing lit translucent puff sprites from a 4 x 4 puff atlas, placed by `silverbrook_props.dress_house_details` as one `NiagaraActor` per smoking chimney (the two `ChimneySmoke_*_k` plane actors are removed). Tuning stays in `silverbrook-props.json` `chimney_smoke` (typed, bounded) and is pushed to User parameters at placement.

Why this and not the alternatives:
- Camera-facing sprites have no view-angle term and no card edge, so `ViewFade`, `sides`, the foot and head ramps and the crossed-card overlap all disappear; the width at every height is just the puff size there.
- Size-over-life and alpha-over-life give the growth and dispersal the cards cannot: each puff starts small and dense at the pot and ends large and faint; the plume shape is emergent and the same from every eye, including the 40-60 m street views.
- CPU sim, not GPU: 85 emitters x <= 32 particles. A GPU emitter costs a compute dispatch per emitter per frame plus fixed-bounds bookkeeping; ~2,700 CPU particles simulate on worker threads in well under half a millisecond and keep spawn, culling and warmup simple. GPU sim buys nothing below ~10k particles.
- A random-frame puff atlas, not an animated flipbook: at 20-120 m a 0.6-2.2 m puff is 10-290 px; overlap of 20-25 live puffs, random rotation, rotation rate and size growth already read as churn, while a looping 64-frame flipbook costs a 2048^2 texture, needs a seamless loop an image model cannot make, and its frame animation is invisible past 30 m.
- Not heterogeneous volumes / sparse volume textures: 85 instances are far beyond budget, and nothing volumetric is visible past 20 m.
- Not another card or mesh with WPO: same rectangle, same view-angle term.

**Can it be authored from Python in 5.8? No.** Verified in `C:\UnrealSource\UE_5.8.3\Engine\Plugins\FX\Niagara\Source\NiagaraEditor\Public`: the only Blueprint/Script-callable editor classes are `UpgradeNiagaraScriptResults` (module version upgrades) and the baker library; `UNiagaraSystemFactoryNew::SystemToCopy` / `EmittersToAddToNewSystem` are plain C++ members (no `UPROPERTY`), so `AssetTools.create_asset` can only make an empty system, and module stacks (`FNiagaraStackGraphUtilities`) are editor-private. The same is true from project C++ without linking private NiagaraEditor headers. **Fallback (this spec):** author `NS_ChimneySmoke` once in the Niagara editor from the engine template emitter `Minimal` (`Engine/Plugins/FX/Niagara/Content/DefaultAssets/Templates/Emitters/Minimal.uasset`) with the module list below, commit the `.uasset`, and keep everything else reproducible in Python: the material (`build_materials.py`), the puff atlas (Codex or `make_chimney_puffs.py`), the placement and User-parameter push (`silverbrook_props.py`), and a check script that asserts the asset exists, exposes every User parameter named below and that every smoking house has exactly one `NiagaraActor`. The template path and module list are the recipe, so the asset can be rebuilt by hand in ten minutes if it is lost.

### NS_ChimneySmoke (author in the editor, commit the asset)

System: `Effect Type` = `ET_ChimneySmoke` (below); `Warmup Time` 4.0 s, `Warmup Tick Delta` 1/15 s (plumes are full on level start); `Fixed Bounds` on, box (-400, -400, -100) to (700, 400, 900) cm.

Emitter `Smoke` (from `Minimal`): `Sim Target` CPUSim, `Calculate Bounds Mode` Fixed with the same box, `Local Space` off, `Determinism` off, `Allocation Mode` Manual Estimate 32, `Interpolated Spawning` on.

| Stack | Module | Value |
|---|---|---|
| Emitter Spawn | Emitter State | Life Cycle Mode Self, Loop Behavior Infinite |
| Emitter Update | Spawn Rate | `User.SpawnRate` (default 6 /s) |
| Particle Spawn | Initialize Particle | Lifetime uniform 0.85-1.15 x `User.Lifetime` (default 4.0 s); Position Offset random sphere 10 cm; Color (1,1,1, A 1); Sprite Size uniform 0.85-1.15 x `User.StartSize` (default 60 cm); Sprite Rotation uniform 0-360 deg; Mass 1 |
| Particle Spawn | Add Velocity | (0, 0, 1) x `User.RiseSpeed` (default 160 cm/s) with Velocity Scale uniform 0.8-1.2 |
| Particle Spawn | Set `Particles.SubImageIndex` | Random Range Int 0-15 (one of the 16 atlas puffs, never animated) |
| Particle Spawn | Set `Particles.RotationRate` (float) | uniform -15..15 deg/s |
| Particle Update | Particle State | Kill Particles When Lifetime Has Elapsed |
| Particle Update | Wind Force | Wind Speed `User.Wind` (default (80, 0, 0) cm/s), Air Resistance 1 |
| Particle Update | Drag | 0.4 |
| Particle Update | Curl Noise Force | Noise Strength 40, Noise Frequency 0.015, Pan Noise Field 0.1 |
| Particle Update | Scale Sprite Size | curve over NormalizedAge: 1.0 at 0, 1.8 at 0.3, 2.9 at 0.7, `User.EndSize` / `User.StartSize` (default 3.67 = 220 cm) at 1 |
| Particle Update | Scale Color | Alpha curve over NormalizedAge: 0 at 0, 1.0 at 0.08, 0.8 at 0.5, 0.35 at 0.8, 0 at 1; RGB 1 |
| Particle Update | Sprite Rotation Rate | from `Particles.RotationRate` |
| Particle Update | Solve Forces and Velocity | default |
| Render | Sprite Renderer | Material `M_ChimneySmokeSprite`, Alignment Unaligned, Facing Mode Face Camera, Sub Image Size (4, 4), Sub UV Blending off, Sort Mode View Depth, Default Pivot (0.5, 0.5), Remove HMD Roll off |

User parameters (all exposed, set per actor from `silverbrook-props.json`): `User.SpawnRate` float, `User.Lifetime` float, `User.RiseSpeed` float (cm/s), `User.Wind` Vector (cm/s, world), `User.StartSize` float (cm), `User.EndSize` float (cm), `User.Opacity` float, `User.Tint` LinearColor. `Scale Sprite Size`'s final key reads `User.EndSize / User.StartSize` through a Divide dynamic input. `User.Opacity` and `User.Tint` reach the material through the Sprite Renderer's **Material Parameters > Attribute Bindings** (`Opacity` <- `User.Opacity`, `Tint` <- `User.Tint`: a binding maps a User/Emitter/Particle attribute onto a material scalar or vector parameter by name, per renderer, no material instance needed).

`ET_ChimneySmoke` (`NiagaraEffectType`, same folder): `Cull By Distance` on, `Max Distance` 12000 cm (120 m); `Cull Reaction` Deactivate Immediately (the material's distance fade hides the pop, see below); `Update Frequency` Every Frame; `Cull By Effect Type Instance Count` off; no significance handler; no cull proxy. Niagara is enabled by default in the project (`Niagara.uplugin` `EnabledByDefault` true, no `.uproject` change).

### M_ChimneySmokeSprite (Python, `GameClient/ArtSource/VFX/StreetLamp/build_materials.py`, new function `build_chimney_sprite()` called from `build()`)

```python
def build_chimney_sprite():
    """Lit camera-facing puff sprite for NS_ChimneySmoke (WP6g review): opacity = atlas density x particle alpha x Opacity x distance fade."""
    m = _material('M_ChimneySmokeSprite', blend=u.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('shading_model', u.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property('translucency_lighting_mode', u.TranslucencyLightingMode.TLM_VOLUMETRIC_PER_VERTEX_NON_DIRECTIONAL)
    m.set_editor_property('used_with_niagara_sprites', True)
    m.set_editor_property('two_sided', False)
    puff = _node(m, u.MaterialExpressionParticleSubUV, texture=u.load_asset(DEST + '/T_ChimneyPuffs'))   # SubImageIndex picks the atlas cell
    pcol = _node(m, u.MaterialExpressionParticleColor)
    tint = _node(m, u.MaterialExpressionVectorParameter, parameter_name='Tint', default_value=u.LinearColor(.42, .42, .45, 1))
    _output(_mul(m, tint, pcol), u.MaterialProperty.MP_BASE_COLOR)                     # lit grey; ParticleColor.rgb stays 1
    opacity = _node(m, u.MaterialExpressionScalarParameter, parameter_name='Opacity', default_value=.55)
    depth = _node(m, u.MaterialExpressionPixelDepth)
    far = _node(m, u.MaterialExpressionSaturate)                                           # full to 80 m, gone at 120 m (= ET_ChimneySmoke Max Distance)
    _link(_mul(m, _add(m, _mul(m, depth, _constant(m, -1.)), _constant(m, 12000.)), _constant(m, 1 / 4000.)), far, '')
    density = _node(m, u.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False); _link(puff, density, '')
    palpha = _node(m, u.MaterialExpressionComponentMask, r=False, g=False, b=False, a=True); _link(pcol, palpha, '')
    _output(_mul(m, _mul(m, _mul(m, density, palpha), opacity), far), u.MaterialProperty.MP_OPACITY)
    _save(m)
```
`T_ChimneyPuffs` import (`_texture()` path): sRGB off, compression `TC_GRAYSCALE`, address Clamp on both axes, mips on, `LODGroup` Effects. The lamps keep `M_LampSmoke` and `T_LampSmoke` untouched; `T_ChimneySmoke` and the `MI_SB_ChimneySmoke` instance become unused and are deleted in the same change.

### Placement (`GameClient/ArtSource/Silverbrook/silverbrook_props.py` `dress_house_details`)

Replace the plane spawn at the `h<.4` townhouse branch (`for k in range(2): spawn(f'ChimneySmoke_{label}_{k}', plane, ...)`) with one actor at the pot top plus 20 cm:
```python
ns = _asset('/Game/Frontier/VFX/ChimneySmoke/NS_ChimneySmoke'); assert ns, 'author NS_ChimneySmoke (docs/blueprints/vfx.md section 2)'
...
a = A.spawn_actor_from_class(u.NiagaraActor, u.Vector(x, y, p.z + sz*100 + 20), u.Rotator(yaw=yaw))
c = a.niagara_component; c.set_asset(ns)
for k in ('spawn_rate', 'lifetime', 'opacity'): c.set_variable_float('User.' + ''.join(w.title() for w in k.split('_')), float(val(cs[k])))
for k in ('rise_speed', 'start_size', 'end_size'): c.set_variable_float('User.' + ''.join(w.title() for w in k.split('_')), float(val(cs[k])) * 100)   # m -> cm
c.set_variable_vec3('User.Wind', u.Vector(*[w * 100 for w in val(cs['wind'])]))
c.set_variable_linear_color('User.Tint', u.LinearColor(*val(cs['tint']), 1))
a.set_actor_label(f'ChimneySmoke_{label}'); a.set_folder_path('Silverbrook/Dressing'); a.tags = [TAG]
```
`dress_all`'s clear step must also delete existing `ChimneySmoke_*` `StaticMeshActor`s and `NiagaraActor`s (it clears by `TAG`; confirm Niagara actors carry it). `silverbrook-props.json` `chimney_smoke` becomes: `spawn_rate` 6 (2-12 /s), `lifetime` 4.0 (2.5-6 s), `rise_speed` 1.6 (0.8-3 m/s), `wind` [0.8, 0, 0] (each -2..2 m/s), `start_size` 0.6 (0.3-1 m), `end_size` 2.2 (1.2-3.5 m), `opacity` 0.55 (0.2-0.8), `tint` [0.42, 0.42, 0.45]; `pan_v`, `edge_fade` and `texture` are removed. `capture_game.py`'s chimney view keeps the same eye: 14 m out, 3.2 m above the pots, looking 1.5 m down, FOV 55, found from the `ChimneySmoke_*` actor label (now a `NiagaraActor`: take `get_all_actors_of_class(world, u.NiagaraActor)`).

### T_ChimneyPuffs — puff atlas (Codex)

1024 x 1024 PNG, 4 x 4 grid of 256 x 256 cells, 16 different puffs, greyscale on pure black, no alpha channel (luminance is the density; the material reads R). Each puff centred in its cell, filling 70-85 % of the cell, fully black for the outer 8 % so no cell shows a square edge, soft feathered outline, no baked lighting direction (flat mid-grey mass with lighter cores), no background, no text. Import as above and check: every cell's outer 20-px frame has max value <= 2/255, every cell's peak >= 230/255, mean over the cell 0.18-0.30.

Codex prompt: "Sprite sheet for a game VFX smoke atlas. A 4 by 4 grid of 16 separate soft smoke puffs on a pure black background, each puff centred in its square cell, filling about three quarters of the cell with black margins, no cell touching another. Greyscale only: wispy, rounded cumulus-like wood-smoke puffs with soft feathered edges that fade fully to black, slightly lighter dense cores, varied shapes (round, lopsided, two-lobed, ragged), no lighting direction, no shadows, no shine, no background, no grid lines, no text. Reference the plume above the mill chimney in docs/Game-Design/concept-art/UI/v5/gameplay-hud.png (crop x 1330-1360, y 100-170) for the puff character. Output 1024 by 1024 pixels."

Fallback if the sheet fails the cell check: `GameClient/ArtSource/VFX/StreetLamp/make_chimney_puffs.py` (Engineer, numpy): 16 cells of radial falloff `clip(1 - r/0.42, 0, 1)**1.5` times `periodic_noise` from `make_chimney_smoke.py` remapped to 0.4-1.0, three noise offsets per cell, zeroed outside r 0.45.

### Acceptance (fresh capture, same eyes; `measure_plume.py` extended by the builder to print every number)

`chimney-smoke.png` (plume column = pixels whose 7-row-averaged R-B exceeds the sky interpolated across x 760-1100 by 4; over field rows y >= 440 the L excess > 3 over the same-row field fit at x 560-700; pots at y ~580, x 880-920):
1. Origin: at y 550 the plume spans >= 30 and <= 130 px and its centre lies within x 860-940 (it leaves the pots, no wider than 1 m).
2. Growth: the span at y 150 is >= 2.0 x the span at y 550 and <= 450 px.
3. No veil beside the stack: no row in y 460-540 has more than 160 columns of field brightening, and none of it lies left of x 790.
4. Density: peak R-B excess >= 12 somewhere in y 380-540 and >= 6 at y 100.
5. Luminance: plume L within 10 % of the sky at rows 30, 80 and 260, and never brighter than the sky by more than 5 %.
6. Puffs: over every 100-row window in y 100-500 the plume's left-edge x has a standard deviation >= 8 px, and the column-integrated density along y 100-500 has >= 3 local maxima separated by >= 40 px.
7. Motion: 14 `-game` frames 0.3 s apart: the row-profile cross-correlation shows the plume moving up by >= 20 px/s and its drift sideways by <= 60 px/s; mean absolute difference between consecutive frames inside x 760-1100, y 0-560 is >= 2.
8. Stack top in frame; no rectangle, straight edge longer than 100 px or hard cut anywhere in the plume.

`main-street.png`: above the right roofs (x 790-830 or x 1085-1125, y 300-400) at least one row shows >= 15 columns with R-B excess >= 5, i.e. a chimney thread is visible at 40-60 m, and no thread is brighter than the sky.

Performance (1440p, `profilegpu` and `stat Niagara`, chimney eye and street eye, twice each): GPU delta FX on/off <= 0.5 ms (same gate as the lamps, pass 3 was 0.313 ms); Niagara game-thread total <= 0.2 ms and worker-thread total <= 0.6 ms; `stat NiagaraOverview` active `NS_ChimneySmoke` instances <= 85 within 120 m and 0 with the eye 200 m from the nearest chimney (`ET_ChimneySmoke` cull works); no instance exceeds 32 particles.

## Deferred (seen, not failed here; unchanged from WP6f)

Flat cobble relief and the cobble paint down the bank (GFX-02 J2); lamps and windows lit at noon (J5); faceted far peaks and the dark wedge at `chimney-smoke.png` x 560-640, y 0-50 (J1b); flat cyan water and the grazing-angle ribbon across the field (GFX-02 water); ashlar repeats, plaster, stall stand-in, decals, disc blooms (GFX-03).

## Fix list (shared first)

1. **Puff atlas `T_ChimneyPuffs.png`** — Codex. 1024 x 1024, 4 x 4 greyscale puffs on black per §T_ChimneyPuffs, with the prompt there; commit under `GameClient/ArtSource/VFX/StreetLamp/`. Engineer runs the cell check on import and falls back to `make_chimney_puffs.py` if it fails.
2. **`M_ChimneySmokeSprite`** — Engineer, CODE. `build_materials.py`: add `build_chimney_sprite()` exactly as above, import `T_ChimneyPuffs` through `_texture()` with sRGB off, `TC_GRAYSCALE`, Clamp/Clamp; call it from `build()`.
3. **`NS_ChimneySmoke` + `ET_ChimneySmoke`** — Engineer, editor authoring (not scriptable in 5.8), commit both assets under `/Game/Frontier/VFX/ChimneySmoke/`. Template emitter `Minimal`, module table and values per §NS_ChimneySmoke, eight User parameters, renderer attribute bindings `Opacity` and `Tint`, warmup 4 s, fixed bounds, CPU sim, allocation 32; effect type cull at 120 m.
4. **Placement** — Engineer, CODE. `silverbrook_props.py` `dress_house_details`: one `NiagaraActor` per smoking house at pot top + 20 cm with the User-parameter push above; delete the plane spawn, the `MI_SB_ChimneySmoke` instance build, `T_ChimneySmoke` and `make_chimney_smoke.py`; `silverbrook-props.json` `chimney_smoke` replaced by the eight bounded values; `capture_game.py` finds the chimney view from `NiagaraActor`s. Add `check_chimney_smoke.py` to the next evidence folder: asserts the system asset, its eight exposed User parameters (`NiagaraSystem.get_editor_property('exposed_parameters')` or, if that is not readable from Python, a `set_variable_float` round trip on a spawned component), one `NiagaraActor` per house with `_h(label+'smoke') < .4`, and zero `ChimneySmoke_*` static-mesh actors.
5. **Blueprint and map** — Engineer, docs. `docs/blueprints/vfx.md` §2 is rewritten with this spec and marked "spec, not yet passing" (done in this commit); update `docs/dependency-map.md`'s Silverbrook WP6 row and the lamp-FX row when the files above change.
6. **Capture, measure, resubmit** — Engineer. Rerun `apply_dressing.py` (materials, `dress_all`), the WP6f gates (`check_foundations`, `check_hidden_in_game`, `check_ground`, `check_details`, plus `check_chimney_smoke`), recapture `chimney-smoke`, `main-street` and the 14-frame motion set in `-game` with the editor closed, the 1440p `profilegpu` / `stat Niagara` pair, extend `measure_plume.py` to print acceptance 1-8 and the street thread, then resubmit for review. `market`, `house-frontage` and `foundation-close` stand unless `dress_all`'s inputs change (they will, by the chimney branch: recapture all five if any dressing count other than `smoking` changes).
