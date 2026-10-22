# Silverbrook WP6h: WP6g review fix list 2-6, plus three owner reports

Implements fix items 2-6 of `../2026-10-21-wp6-town-g/REVIEW.md` (commit 56ce679); item 1 (the puff atlas) is Codex's commit dc633f5 and passed its cell check on import (every cell: outer 20 px frame max 0, peak 235-241, mean 0.225-0.233). Also, from the coordinator in the same pass: the owner's floating houses (foundations), "light sources from nowhere" (lamp lights) and the context-view requirement of `docs/visual-review.md` (3ec8d2e). No verdict here; the reviewer writes `REVIEW.md`.

## Run order (own editor on `Greenlands_Silverbrook`, MCP on its own port)

1. `py -3 GameClient/ArtSource/VFX/ChimneySmoke/build_chimney_niagara.py --port <editor MCP port>`: runs `build_effect_type.py` in the editor (imports `T_ChimneyPuffs`, builds `M_ChimneySmokeSprite`, `ET_ChimneySmoke`), then authors `NS_ChimneySmoke` through the engine's NiagaraToolsets (`NiagaraToolset_System` over the editor MCP) and saves it. Prints `CHIMNEY_NS /Game/Frontier/VFX/ChimneySmoke/NS_ChimneySmoke.NS_ChimneySmoke ok`. Compile state after the build: `aggregateStatus UpToDate, bHasErrors false, bHasWarnings false`; stack issues 0 errors, 0 warnings.
2. `py -3 tools/Run-InEditor.py docs/evidence/graphics/2026-10-21-wp6-town-h/apply_dressing.py`: `dress_all` (footings, details with the Niagara chimneys, street groups, then `lamp_fx.apply()`), saves the level, deletes `MI_SB_ChimneySmoke` and `T_ChimneySmoke`. Counts: `HOUSE_FOUNDATIONS 312`, `HOUSE_DETAILS {... 'smoking': 85 ...}`, `STREET_GROUPS 1501`, `LAMP_FX {"fireflies": 466, "lights": 795, "sockets": 795}`.
3. Gates (below), editor closed, then `-game -RenderOffscreen -ResX=1920 -ResY=1080 "-ExecCmds=py .../capture_game.py"`, `make_pairs.py`, `measure_plume.py`; `-ResX=2560 -ResY=1440 "-ExecCmds=py .../capture_perf.py"` and `py -3 parse_perf.py <Saved/Logs/wp6h-perf.log>`.

All five review views are recaptured, not copied from -f: `dress_all`'s inputs changed (footprints, stoop bases, lamps), and its counts moved (`PROPS_PLACED firewood` 1 -> 0, `STREET_GROUPS` 1500 -> 1501), which per review item 6 means recapture all five.

## Gates (final map, all run after the last `dress_all` and lamp pass)

```
FOUNDATION_CHECK houses 312 points 12891 overhang_points_skipped 1122 failing_houses 0 failing_points 0 min_points_per_house 36
FOUNDATION_WORST max_gap_cm 1.5 min_gap_cm 0.0 min_buried_cm 101.7
LIGHT_CHECK lights 795 on_glass 795 orphans 0 in_wall 0 unshadowed_wall 0 shadowed 444
CHIMNEY_SMOKE_CHECK smoking_houses 85 niagara_actors 85 missing [] wrong_asset 0 card_actors 0 effect_type ET_ChimneySmoke float_roundtrip {SpawnRate, Lifetime, RiseSpeed, StartSize, EndSize, Opacity, Wind, Tint: all True}
HIDDEN_IN_GAME_CHECK checked 126 visible_in_game 0 []
DEFAULT_MATERIAL_CHECK visible_in_town 8905 default_or_graybox 0 []
DRESSING_GROUND {'checked': 5850, 'floating_over_5_cm': [], 'max_positive_gap_cm': 0.01}
SILL_PLANTERS 140 expected 140 GROUP_DETAIL_OVERLAPS 0 []
DRESSING_GROUNDED {"floating": 0, "checked": 549714, "worst_gap_cm": 0.0}
WP6_SIGHTLINE_CLEAR for every captured view (capture_game.py)
```

Before the fixes the same two new gates failed: `check_foundations.py` on the old plinths, `failing_houses 312 failing_points 9046` (open air under the side and rear walls); `check_lights.py` on the old lights, 939 of 939 orphans (building sockets 2-20 m from any lantern glass, see below).

## Fix items

2. `build_materials.py`: `build_chimney_sprite()` as specified, called from `build()`; `T_ChimneyPuffs` imported sRGB off, `TC_GRAYSCALE`, Clamp/Clamp, `TEXTUREGROUP_EFFECTS`. Deviation: the spec's `ComponentMask(a=True)` on ParticleColor does not compile (its default output is RGB: "Not enough components ... for component mask 0001") and the engine then draws the default material (opaque black squares in the first capture); the opacity now takes ParticleColor's `A` pin.
3. `NS_ChimneySmoke` + `ET_ChimneySmoke`: authored by script, not by hand (`GameClient/ArtSource/VFX/ChimneySmoke/`). The review's module table, User parameters, renderer and bindings are followed; deviations, all in `docs/blueprints/vfx.md` §2: Emitter State sits in Emitter Update (where 5.8's `Minimal` has it); the size and alpha curves are HLSL piecewise-linear expressions with the review's keys (EndSize/StartSize computed in the expression) instead of curve data interfaces; ranges use Initialize Particle's Random modes with HLSL min/max, the 10 cm sphere offset and the SubImageIndex / RotationRate randoms are HLSL; Pan Noise Field (0.1, 0.1, 0.1); Wind Force "air resistance 1" = Wind Speed Scale 1; warmup 4.01 s (the editor floors 4.0 / (1/15) to 59 ticks); sprite renderer shadows off; Scale Sprite Size mode set to Uniform (default is Uniform Curve). Cull reaction is **DeactivateImmediateResume**, not Deactivate Immediately: with plain DeactivateImmediate, chimneys culled at level start never came back (the first capture had no smoke at 14 m).
4. Placement: `silverbrook_props.dress_house_details` spawns one `NiagaraActor` per smoking townhouse at the pot top + 20 cm and pushes the eight User values; the card spawn and `MI_SB_ChimneySmoke` build are gone; `T_ChimneySmoke.png/.uasset`, `MI_SB_ChimneySmoke` and `make_chimney_smoke.py` deleted; `silverbrook-props.json` `chimney_smoke` holds the eight bounded values. Smoking rule kept as the code's `_h(label) < .4` (the review text says `_h(label+'smoke')`; keeping it keeps the same 85 houses). `check_chimney_smoke.py` is in this folder.
5. `docs/blueprints/vfx.md` §2 rewritten with the commands and values used, still marked "spec, not yet passing"; `docs/dependency-map.md` WP6 and lamp rows updated.
6. Captures, gates, measure and perf as below.

## Owner reports handled in this pass

- **Floating houses.** Root cause as reported: `_base_box` took only vertices near the mesh's lowest point, which on most houses is the front doorstep, so the plinth was a front strip (Townhouse4: y 190-236 of a 650 cm deep house) and the raised house floated; `check_foundations.py` reused the helper. Now `dress_footings` traces the house's own mesh (`_wall_shell`): per side the outermost wall plane hit by >= 30 % of horizontal rays at 1.0-1.8 m, widened to the stone base course at the foot (<= 15 cm), plus a stone base under each run of the mesh's own doorstep/porch (`<house>_StoopBase<k>`, same top and burial; the street risers start in front of it); burial uses terrain sampled every 50 cm round the footprint. The gate is rewritten independently of that code: every 50 cm along each side it finds the wall at 60 cm, follows it down to its foot, and traces down 2 cm outside the face: footing top 0-2 cm under the foot (a foot set into the stone up to 10 cm is accepted), no open air, footing buried >= 100 cm. Faces > 20 cm out of a side's main wall (bays, porches; 1122 points) and samples past the perpendicular walls (corner trims) are not tested. Views: `context-house-side-a.png`, `context-house-side-b.png`, `context-house-side-rear.png` (eye 1.7 m, 9 m out).
- **Light from nowhere.** The coordinator's hypothesis (lights left behind when footings raised the houses) explained 3 lights only. The real cause: the building entries of `lamp-sockets.json`, derived from the GLBs, are 2-20 m from the imported meshes' glass (e.g. Townhouse1 socket on the back wall, its glow pane on the front at 4.2 m); street lamps matched to 0 cm. `lamp_fx.glass_sockets` now reads each mesh's `*Glow` pieces (merged within 60 cm, panes get the light 10 cm in front; building glow volumes >= 30 cm in every axis, which are lit rooms/doorways behind glass, and glow centred under 1 m get no lamp: 939 -> 795 lights). Lights are attached to their owner, wall-mounted lights cast shadows (444), and `dress_all` runs `lamp_fx.apply()` last. Budget: the street eye's whole GPU frame stays 4.2-4.4 ms at 1440p with the 444 shadowed lights in the map (perf below). View: `context-lamp-wall.png`.
- **Context views** (seed 20261022, all `-game`, no prop within 1.5 m of the sight line, ground spots must have open sky above): `context-overview-se.png` (230 m up, 38 deg down), `context-overview-nw.png` (200 m, 42 deg), `context-road-silvermere.png`, `context-road-everwood.png`, `context-road-valemere.png` (one on each road out of town, outside the wall), `context-town-edge.png` (25 m outside the wall), plus the house and lamp views above. The street-group placement along the approach road is unchanged, as instructed.

## Compares

`main-street-{hud,gfx01}-compare.png`, `market-…`, `house-frontage-…`, `foundation-close-…`, `chimney-smoke-…` (concept left, live right). Motion: `motion-00..13.png` with `motion-times.json`. Context: `context-*.png` (no concept pair; judged against the brief).

## Acceptance numbers (`measure_plume.py` -> `plume-measure.json`; review definitions; item 8 left to the eye)

| Item | Measured |
|---|---|
| 1 origin, y 550 | plume px x 710-1094, span 385, centre 902 (row 550 lies over roofs, not field, in this frame; the field-fit definition picks up the roofs) |
| 2 growth, y 150 | no plume pixels over the 4 R-B threshold at y 150 (ratio n/a) |
| 3 veil y 460-540 | max 351 field-brightening columns per row; leftmost x 560 |
| 4 density | peak R-B excess 56.8 in y 380-540; 1.8 at y 100 |
| 5 luminance | plume/sky L 1.036 at row 30, no plume pixels at rows 80 and 260; max over rows 10-430 1.264 |
| 6 puffs | left-edge sd per 100 rows (100-199 / 200-299 / 300-399 / 400-499): 8.5 / 99.0 / 36.1 / 41.0 px; density maxima at y 107, 197, 241, 309, 370, 418, 464 |
| 7 motion | upward 14.3 px/s mean (per pair 0-32.5), sideways drift 6.6 px/s, consecutive mean abs diff min 0.96 / mean 1.12; frame spacing 0.23-0.41 s (HighResShot stalls the game clock; times logged) |
| street thread | x 790-830: 21 columns >= 5 at y 382, thread L/sky 0.973; x 1085-1125: 1 column |

## Performance (2560x1440 `-game`, `capture_perf.py`, `perf.json`; FX off hides the 85 chimney components)

| Eye | GPU graphics root on-1 / off-1 / on-2 / off-2 (ms) | mean on - off | Niagara GT (on-1 / on-2) | NS_ChimneySmoke worker sum (on-1 / on-2) | active within 120 m |
|---|---|---|---|---|---|
| chimney | 7.055 / 4.703 / 4.494 / 4.609 | +1.118 (on-1 is the first profiled frame of the run; on-2 - off-2 = -0.115) | 0.112 / 0.100 ms | 0.360 / 0.307 ms | 9 of 9 |
| street | 4.230 / 4.447 / 4.171 / 4.328 | -0.187 | 0.101 / 0.074 ms | 0.363 / 0.287 ms | 9 of 9 |
| 224.6 m from the nearest chimney | 5.334 | | 0.087 ms | 0 | 0 (0 active in total) |

GT = `STAT_NiagaraOverview_GT` of one `stat dumpframe`; worker = sum of every `NS_ChimneySmoke [GT_CNC]` entry in that dump (nested entries counted twice, so an upper bound). Particles per instance were not read from stats; with allocation 32, 6 /s and lifetimes up to 4.6 s the steady state is about 28. The far eye could not be put at exactly 200 m from the nearest chimney with the town layout; it is 224.6 m.
