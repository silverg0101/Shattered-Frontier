# Buildings blueprint: houses and other structures

Scope: one Silverbrook building package (Blender -> FBX -> `/Game/Frontier/Environment/Greenlands/Silverbrook/`), its foundation, surface materials, relief and per-house variation. Dressing around the house is in [props-and-dressing.md](props-and-dressing.md). Depth: [GFX-03](../decisions/GFX-03-surface-detail-and-buildings.md). Shared rules: [README.md](README.md).

## 1. Pipeline (in order)
| # | Step | Command / file | Done when |
|---|---|---|---|
| 1 | Concept | `docs/Game-Design/concept-art/Silverbrook/<Name>.png` (one image per package; new ones from Codex, same sheet style: elevation, 3/4 view, neutral background) | copied into the package folder as `<Name>-reference.png`, SHA-256 in `asset-manifest.json` |
| 2 | Scans | `py -3 GameClient/ArtSource/Silverbrook/acquire_materials.py <polyhaven id> ...` | rows in `materials-provenance.json`; realistic scans only, never stylized sets |
| 3 | Build | `"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P GameClient/ArtSource/Silverbrook/<Name>/build_<name>.py` | writes `Silverbrook<Name>.{blend,fbx,glb}`, `asset-manifest.json`; the build asserts the package fits its `SLOT` |
| 4 | Fresh-process review | `blender.exe -b --factory-startup Silverbrook<Name>.blend -P review_<name>.py` | `<Name>-{Beauty,Front,Side,Rear}.png`, `exchange-review.json` (GLB/FBX reimport clean) |
| 5 | Render review | fresh Fable reviewer: Beauty/Front vs the concept | PASS before import |
| 6 | Import (open editor) | `py -3 tools/Run-InEditor.py GameClient/ArtSource/Silverbrook/import_silverbrook.py <Name>` | `SM_Silverbrook<Name>` (+ `_Door1` etc.), `ue-import-report.json` with part pivots; no default material |
| 7 | Register | `build_silverbrook.py` `VARIANTS[kind]` / `KIND_BUILDING` tables (runbook §8) | the lot kind draws the package |
| 8 | Rebuild and dress | `py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/build_silverbrook.py --discard-unsaved` (calls `silverbrook_props.dress_all()` and `lamp_fx.apply()`), then `GameClient/ArtSource/Greenlands/paint_street_edges.py` | `SILVERBROOK_BUILT`, `HOUSE_FOUNDATIONS n`, `HOUSE_TINTS n` |
| 9 | Gates | `check_foundations.py`, `check_hidden_in_game.py`, `check_ground.py`, `check_details.py` (README table) | each prints its pass line |
| 10 | Capture + review | views `house-frontage` and `foundation-close` (`capture_game.py` in `docs/evidence/graphics/2026-10-21-wp6-town-f/`), pairs vs `Silverbrook-Design-Board-v1.webp` and `gameplay-hud.png` | fresh Fable PASS |

Package conventions (`silverbrook_kit.py`): metres, Z up, **front faces -Y**, origin at ground centre; `kit.scan(name, polyhaven_id, dark=, lichen=, normal=, damp=, aged=)` for scans at their measured physical size, `kit.solid(name, rgb, rough, metal)` for flat palettes (being replaced, GFX-03 §2.4), `kit.model()` for Poly Haven props; single-element `rect` sampling for one slate / one dressed stone / one beam; weathering in vertex colour R (damp below 0.65 m) and G (lichen). Moving parts (door leaves, gate leaves, portcullis, mill wheel) are their own FBX with the origin at the pivot (`kit.export_parts` / `split_doors`) and are attached by `place_parts.attach` after spawn. Keep each package under 400 k triangles (Townhouse1: 71 k, 7.08 x 7.0 x 8.97 m in an 8 x 7 x 9 m slot).

Import facts to design around: the FBX import **flips Y** (the Blender -Y front becomes +Y; `build_silverbrook.model()` turns the actor a further 180 deg) and **recentres each mesh on its bounds** (Townhouse1 moved +26 cm x, -18 cm y). Any attach coordinate (sills, sockets, smoke stacks) is traced on the **imported** mesh (`line_trace_component`), never read from the Blender file.

## 2. Foundations (owner: "every house foundation is too short")
Values in `GameClient/ArtSource/Silverbrook/silverbrook-props.json` `footing`: `visible_cm` **65** (bounds 50-70), `burial_cm` **100** (100-150), `proud_cm` **4** (0-8), `base_band_cm` 25; material `M_SB_FoundationStone` built by `silverbrook_props.dress_footings` with `Roughness` **0.86**, matte dark rubble, triplanar. The plinth is sized from the mesh's own wall base (`_base_box`), sits on the highest terrain under the footprint and fills down below the lowest; stoop steps (`<House>_Stoop0..k`, equal low risers) run from the doorstep to the street.
Gate `check_foundations.py` (copy from the wp6-town-f evidence): fails any house with `|gap| > 0.1 cm`, `min_visible < 50 cm`, `min_buried < 100 cm`, `proud < -0.5 cm` or `> 8 cm`. Pass line: `FOUNDATION_CHECK 312 failures 0 min_visible_cm 65.0 max_abs_gap_cm 0.0 proud_cm 4.0 4.0`.
GFX-03 K1 moves the plinth into the kit (`plinth(height=0.6, proud=0.06, bevel=0.03, block=(0.6,0.9))`, 0.5-0.7 m, 2 cm below grade) so it never floats on a landscape step; until then `dress_footings` owns it.

## 3. Surfaces and relief (GFX-03; K0-K2 not yet run: `import_silverbrook.py` `DISPLACEMENT_CM=0`)
| Rule | Value |
|---|---|
| Technique by surface | geometry for anything touched or in silhouette (slates, quoins, beams, plinth, ridge, chimney); Nanite tessellation displacement for ashlar/rubble/plaster/thatch; normal + AO only for cloth, iron, lead, glass, props < 40 cm |
| Displacement needs an edge mask | every displaceable quad is inset 3 cm in Blender (`Geo.poly`), vertex colour B = 1 inside, 0 on the rim, A = 1 marks displaceable; `disp = 0.5 + (Height - HeightMean) * DisplacementScale * VertexColor.B`, magnitude 3 cm. **Never** turn `enable_tessellation` on without the mask: hard edges crack (`Cottage1/review/displacement-on6cm.png`). |
| `relief` per scan | ashlar 0.8, rubble 1.0, lime plaster 0.3, slate 0 (geometry), thatch 1.0, timber 0, paving 0.7 |
| Texel density | >= 512 px/m within 1.5 m of the player, >= 256 px/m elsewhere; never scale a scan from its physical size, replace it |
| PBR set | `Diffuse, nor_gl, arm (AO R, rough G, metal B), Displacement`; `M_SB_Scan` v2 adds macro grunge (7.3 m, instance offset), detail normal 0.12 m, world-aligned moss/streaks/edge wear; no `kit.solid()` without a normal map after K2 |
| Glass | `M_SB_Glass`: opaque, roughness 0.06, emissive (1.0,0.5,0.14) x 2 x `MPC_Silverbrook.NightLit` (lamps set it); ends the orange glow at noon |
| Budget | +2.0 ms median at bookmarks 06/07 (tessellation <= 1.2, material <= 0.5, decals <= 0.3); levers in GFX-03 §1.5 |

## 4. Per-house variation
- Tones: `silverbrook_props.vary_house_finishes` makes `MI_SB_LimePlaster_HouseTone1..5` and `MI_SB_Slate_HouseTone1..5` from palettes `[(1.00,.93,.80),(.84,.86,.88),(1.00,.86,.66),(.93,.95,.84),(.78,.74,.70)]` and `[(.78,.86,1.02),(1.06,.90,.74),(.86,.98,.88),(.66,.68,.72),(1.00,1.00,1.00)]`, chosen by `_h(label+'tone')`. Rule: **>= 20 % luminance spread** between tones; tints of 0.96-1.04 are invisible at 1080p.
- Packages: `build_silverbrook.model()` picks by `crc32(label) % len(pool)` and skips to the next package when it equals `_last_mesh[kind]`: **no two adjacent lots draw the same mesh**.
- GFX-03 §3.3 (spec): six custom-primitive-data floats per house (`InstanceTint` RGB, `InstanceWear` by district 0.3-0.8, `InstanceTone`), set on the building and every attached part; door leaves must match their frame.

## 5. Known failure -> rule
| Failure | Rule |
|---|---|
| 20 cm glossy plank skirt, one-sided ledge, slit under the wall | plinth values in §2; matte stone; gap 0; proud on all four sides; steps at the door |
| Smeared pale pier block beside the door (Townhouse1) | every kit box gets box-projected UVs at the scan's tile or the plinth material; no element may sample a stretched strip |
| Sill planters floating 12-20 cm in front of the bay | trace sill/bay coordinates on the imported mesh (§1); gate: a trace from 1.5 m in front at sill height + 15 cm to the planter origin must not hit the house |
| Builder claimed "only Townhouse6 has bay sills" | read every `build_townhouse*.py` for bays (1, 3 and 6 have them); `check_details.py` must count 140 of 140 |
| Side window looks like a void | a glass plane 13 cm behind the wall face is invisible at 80 deg; trace from 2 m outside at local y -100 (import flips Y) before changing the mesh |
| White untextured box in the plaza | authoring boxes (`build_silverbrook.box()` in Places/Plots, any `MI_Graybox_*` slot 0) are `set_actor_hidden_in_game(True)`; gate `DEFAULT_MATERIAL_CHECK ... default_or_graybox 0` |
| Rows of the same two meshes at the same yaw, tints < 10 % apart | §4 |
| Ashlar repeats at 3 m, mortar runs round the corner, flat plaster, windows glow at noon | GFX-03 §2 (deferred items in every WP6 review); do not patch with per-house textures |
| Seams when displacement is switched on | §3 edge mask; if hairlines remain at chamfered corners widen the inset to 5 cm, never raise the master magnitude |

## 6. Concept refs
`concept-art/Silverbrook/<Name>.png` per package, `concept-art/Silverbrook-Design-Board-v1.webp` (townhouse elevation, TOWN STREETS tile), `concept-art/UI/v5/gameplay-hud.png` (weathered timber, stone and roofs); owner wall shot = GFX-03 bookmark 06 `inn-wall`, street shot = bookmark 07 `market-street`.
