# WP6d review — Silverbrook town, pass 4 (WP6c fix list)

Reviewer: independent (Fable), did not build the work. Judged per `docs/visual-review.md` against `Silverbrook-Design-Board-v1.webp`, `UI/v5/gameplay-hud.png` and the owner's screenshots 16, 18 and 19. Commit d0fd309. Every capture read at full resolution plus 2-4x crops; pixel luminance measured on `chimney-smoke.png`; the smoke texture's alpha sampled. Code read: the d0fd309 diff, `silverbrook_props.py`, `silverbrook-props.json`, `build_silverbrook.py` (road/plaza fields), `paint_street_edges.py`, `check_hidden_in_game.py`, `VFX/StreetLamp/build_materials.py`, `make_chimney_smoke.py`, `capture_game.py`, `make_pairs.py`, the Townhouse1/2/3/6 and PropHayBales/PropSillPlanter/PropFlowerTub build sources.

**Result: 0 PASS / 5 FAIL.** Five of the eight WP6c items are resolved and visible (white box, street groups, lamp framing, washing lines, flower tub). The smoke still reads as white light, the frontage bay still has no planter, the market square is still lawn, and `foundation-close` regressed: a rain barrel stands 0.5 m from the eye and intersects the door tub.

## Owner complaints

| Complaint | Status | Evidence |
|---|---|---|
| 16.webp: thin glossy plank foundation | **Fixed** (unchanged) | `foundation-close.png` x 0-1180, y 520-900: matte rubble course, three steps; `foundation-check.json` 312 houses, 0 failures. |
| 18/19.webp: coloured lines, 3D labels, "Tt" sprites | **Fixed** (unchanged) | No line, label or sprite in any of the five captures; 126 debug actors asserted hidden, 8,648 visible meshes with 0 default/graybox slots. |
| 17.webp / pass-1: bare strip, one cart, identical rows | **Fixed** | `main-street.png`: both margins dressed for the full visible length (barrels, sacks, hay bales, bench, hay cart, ladder, signs, washing line), five house tones, mixed packages. Remaining street faults are paint and smoke, below. |

## WP6c fix list — status

| # | Item | Status |
|---|---|---|
| 1 | Smoke: no needles, visible plume | **Not resolved.** ViewFade removed the 8-15 px edge-on slivers, but every plume is *lighter* than the sky: `chimney-smoke.png` plume L 176-207 vs adjacent sky 160-181 at rows 100/300/500/700/780 (acceptance was 15 % darker). The near card shows as a hard-edged translucent slab; far cards are 15-25 px white columns. |
| 2 | White box in the market plaza | **Resolved.** `market.png` x 990-1050, y 480-620 is now plaza ground; `Arrival_Waystone` hidden; the default-material gate asserts 0 bad slots inside the wall. |
| 3 | Street groups reach every street | **Resolved.** `main-street.png` right margin within 20 m: sacks x 1050-1120 y 540-600, bench x 1240-1370, barrel x 1390-1460, hay bales x 1510-1730; left margin equally dense. |
| 4 | Capture framing | **Half.** No lamp in the sight line (x 930-1030 is now open street); chimney view aimed at the card. But the new `foundation-close` frame is 38 % rain barrel (x 1180-1920, y 360-1080) because the assert only knows `StreetLamp*` actors. |
| 5 | Washing lines that place | **Resolved.** 162 placed; one visible in `main-street.png` at x 700-722, y 507-527 (pale garments on a line across the gap after the second left house). Small at 40 m, but there. |
| 6 | Sill planters on every bay window | **Not resolved on the frontage house.** `house-frontage.png` sill x 760-1440, y 690-720 is bare timber; same in `foundation-close.png` y 100-140. The `sills` table has `Townhouse6` only; the frontage package (door left of the bay, round-headed arch, timber apron 0.22-0.95 m) is `Townhouse1`, whose canted bay (`build_townhouse1.py` 173-184) is not in the table. `Townhouse3` has a canted bay too (`build_townhouse3.py` 156-168). The builder's "only Townhouse6 has bay sills" is wrong. |
| 7 | Flower tub prop | **Resolved.** `house-frontage.png` x 470-570, y 790-930: oak half-barrel, iron hoops, red geranium umbels, trailing thyme; `foundation-close.png` x 260-400, y 320-520 same. Reads as the concept's planter at 3 m. |
| 8 | Ground reads as lanes, not lawn | **Partly.** Moss gone; `main-street.png` left foreground x 0-430, y 620-1080 is now cobble. Not met: the right half of the main street is green-tinted for the first 15 m (x 1250-1920, y 620-1080, mottled grass over cobble) and `market.png` y 650-1080 is still lawn with two firewood stores in it. |

## Builder's deviations

1. **Graybox hide rule is material-based** (`MI_Graybox_*` in Places/Plots, plus every `box()` in those folders hidden by the builder) — **accepted**. The waystone is gone, the civic buildings and stalls stay, and `check_hidden_in_game.py` now gates every visible mesh inside the wall against empty, engine-default and graybox slots.
2. **Per-street share by station count; primary "Road"s and Wall Street get none** — **accepted**. The "Road"s have no stations inside the wall margin, so `_inside` already skipped them; South Street (the main street) is dressed both sides from the first metre, so this does not leave the main street bare. Wall Street is the service lane along the inside of the wall; no groups there is fine until a wall-street view is captured.
3. **Washing-line clearance tested against wall-base boxes (+50 cm) at the two posts and the mid-point** — **accepted**. 162 lines, one visible, no line clipping a wall in any capture. (A line scaled to 1.3 has posts 2.4 m from its centre; the three-point test misses nothing at that length.)
4. **Stoop tubs at 0.75 scale** — **accepted**. The 0.42 m tub sits inside the 0.4 m tread and its umbels read as flowers at 3 m (`house-frontage.png` 100 x 140 px).

## main-street — FAIL

1. **What's wrong:** Chimney plumes are bright white columns above the right-hand roofs: x 795-815, y 355-470; x 1155-1175, y 325-445; x 925-940, y 430-500 and eight more down the row. Each is 15-25 px wide, uniformly brighter than the sky, with straight parallel edges. They read as steam jets, not smoke. Acceptance was "no needle narrower than 40 px above any roof".
   **Fix type:** CODE. **Instruction:** fix 1.
2. **What's wrong:** The right half of the street is green-tinted for the first 15 m: x 1250-1920, y 620-1080 shows mottled grass over the cobble; the left half at the same distance is clean cobble. The eye is on South Street's centre line (x = 29.0 at y = 336, checked against the layout), so this is the paint field: `road_field` gives `G = 0.5 + (hw - d) / ROAD_RAMP` with a 10 m ramp, i.e. only 0.85 cobble 2.5 m from the centre and 0.6 at the kerb; whichever vertex column of the 5 m grid lands further from the centre line shows 15-40 % grass.
   **Fix type:** LEVEL. **Instruction:** fix 2.
3. **What's wrong:** The hay-bale stacks (x 190-420, y 570-760 and x 1510-1730, y 560-740) read as flat tan cardboard boxes with pale blue-grey strapping: no stalk detail at 3 m, no gap or shadow between bales, and the twine (`kit.solid` 0.42/0.40/0.36) takes the sky colour. The board's hay (CARTS & SUPPLIES tile) is loose gold straw.
   **Fix type:** ART + CODE. **Instruction:** fix 5.

Resolved here: lamp out of the sight line, two-sided groups from the first metre, five house tones, washing line at x 700-722, y 507-527, ladder x 10-80, y 420-600, bracket signs x 180-230, y 330-430 and x 1730-1760, y 350-450, left foreground cobble.

## market — FAIL

1. **What's wrong:** The lower 40 % (y 650-1080, x 960-1920) is lawn inside the market square: the two firewood stores (x 985-1100, y 760-920 and x 1515-1680, y 740-890) stand on grass 5-8 m from any wall. The plaza rectangle is 60 x 50 m at (0, -30) in the layout, so the eye at (30, -62) and this ground are at its edge, where the plaza field is 0.5 and the street-margin earth field does not reach. The board's market square is paved to the house walls.
   **Fix type:** LEVEL. **Instruction:** fix 2.
2. **What's wrong:** Smoke needle above the central chimney: x 1383-1392, y 285-335, 10 px wide, brighter than the sky.
   **Fix type:** CODE. **Instruction:** fix 1.
3. Left house windows glow orange at noon (x 230-320 and 620-700, y 660-900): deferred, GFX-02 J5.

Resolved here: white box gone; eight stalls, goods, barrels, grindstone and firewood racks fill x 1000-1700, y 450-700; five house tones.

## house-frontage — FAIL

1. **What's wrong:** No planter on the bay-window sill (x 760-1440, y 690-720 is bare timber), the item asked in WP6b fixes 5/8 and WP6c fix 6. This package is `Townhouse1` (door at x = -1.4 left of the bay at x 0.23-2.0, round-headed arch, timber apron 0.22-0.95 m), and `sills` lists `Townhouse6` only.
   **Fix type:** CODE. **Instruction:** fix 3.
2. **What's wrong:** The side-wall window at x 1700-1770, y 400-690 is a black void in a timber frame: no leaded lattice, no mullion, unlike the bay lights 1 m away. Either the side glazing is missing from `Townhouse1` or the lattice is lost at a grazing angle.
   **Fix type:** CODE. **Instruction:** fix 7.
3. The road embankment at x 0-400, y 690-800 (cobble paint stretched down a terrain bank): deferred, GFX-02 J2.

Resolved here: flower tub on the stoop (x 470-570, y 790-930), pier block, plinth, three steps, lantern, bracket sign, garden fence at x 1700-1920, y 850-1000.

## foundation-close — FAIL (regression from PASS)

1. **What's wrong:** A rain barrel (`SM_SilverbrookPropRainBarrel`, a street group) stands about 0.5 m from the eye and fills x 1180-1920, y 360-1080 — 38 % of the frame — hiding the door, the door tub and the right half of the plinth. It also intersects the door tub: the tub's leaves and a bloom emerge through the barrel's staves at x 1200-1290, y 560-760 (`fc-clash` crop: geranium geometry in front of and inside the barrel). Street groups are placed before house details, and `dress_house_details` never tests its tubs against group rects.
   **Fix type:** CODE (placement + capture). **Instruction:** fix 4.

Resolved here (left of the barrel): matte rubble course, 4 cm proud, three steps, stoop tub with red geraniums, bay apron, cobble with moss at the kerb.

## chimney-smoke — FAIL

1. **What's wrong:** The plume is lighter than the sky at every row measured (y 100/300/500/700/780: plume mean L 207/178/192/178/176 vs sky 170-181/165-174/180/160-171/166-169). Smoke against a bright sky must be darker than the sky; the acceptance was 15 % darker.
   **Root cause:** `M_LampSmoke` is unlit and outputs `Tint` as emissive; 0.58 linear is sRGB ~200, above the sky's ~170.
   **Fix type:** CODE. **Instruction:** fix 1a/1b.
2. **What's wrong:** The near card shows as a rectangular translucent slab with hard vertical edges: x 867-1060, y 525-740 (the two crossed cards each draw their full 2.2 m width), with a bright narrow core x 907-978 continuing to the top of the frame. `T_ChimneySmoke.png` alpha at the card's U edges is 12-29/255 (sampled rows 64/256/448), so the card border is visible at opacity 0.7, and the column is the same width top to bottom (periodic texture, no taper).
   **Fix type:** CODE. **Instruction:** fix 1c/1d.
3. **What's wrong:** No chimney at the base of the plume. The eye sits 0.4 m above the stack top, so the stack is below the horizon line and the plume rises out of the far roofs.
   **Fix type:** CODE (capture). **Instruction:** fix 1e.
4. **What's wrong:** Twenty-plus far plumes along the back rows (y 740-820, e.g. x 590-610, 740-760, 1130-1140, 1300-1310, 1700-1715) are 15-25 px white columns brighter than the sky.
   **Fix type:** CODE. **Instruction:** fix 1a/1f.
5. Far white faceted mountains: deferred, GFX-02 J1b/J2.

## Deferred to GFX-02 / GFX-03 (seen, not failed here)

- Flat cobble and lawn tiles without relief; the cobble paint stretched down the bank in `house-frontage.png` x 0-400, y 690-800 — GFX-02 J2.
- Lamps and windows lit at noon (`main-street.png` lamps, `market.png` left house) — GFX-02 J5.
- Far mountains: white faceted shapes (`main-street`, `chimney-smoke`), smooth domes (`market`) — GFX-02 J1b/J2.
- Ashlar tiles repeat, plaster is flat, no corner wrap — GFX-03 1-2.
- `SM_MarketStall` stand-in with flat striped cloth — GFX-03 gap 7.
- No decals, no per-instance weathering — GFX-03 gaps 5-6.

## Fix list (shared first)

1. **Smoke darker than the sky, soft-edged, tapered, chimney in frame** — Engineer, CODE. Covers main-street 1, market 2, chimney-smoke 1-4.
   a. `GameClient/ArtSource/VFX/StreetLamp/build_materials.py` `build()`: light the smoke instead of emitting it. After `smoke = _material('M_LampSmoke', blend=u.BlendMode.BLEND_TRANSLUCENT)` add
      ```python
      smoke.set_editor_property('shading_model', u.MaterialShadingModel.MSM_DEFAULT_LIT)
      smoke.set_editor_property('translucency_lighting_mode', u.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
      ```
      and replace `_output(tint, u.MaterialProperty.MP_EMISSIVE_COLOR)` with `_output(tint, u.MaterialProperty.MP_BASE_COLOR)`. The lamp wisps keep their default tint (.31,.30,.27) and simply become sooty instead of glowing.
   b. `silverbrook-props.json` `chimney_smoke.tint`: `[0.58,0.58,0.62]` → `[0.42,0.42,0.45]` (base colour of lit smoke; mid grey reads darker than a lit sky). Unit string: "linear base colour". Keep `opacity` 0.7.
   c. `make_chimney_smoke.py`: zero the card edges. After `alpha = np.clip(profile * (.35 + .75 * n), 0, 1) * 1.0` add `alpha *= np.clip((.5 - np.abs(u)) / .10, 0, 1)[None, :]` and extend the assert: `and alpha[:, 0].max() == 0 and alpha[:, -1].max() == 0`. Rerun, reimport `T_ChimneySmoke`.
   d. `build_materials.py`: taper the column so it is narrow at the pot and wide at the top. The card's texture V runs downward (`spawn` roll -90: v = 0 top, 1 bottom). Replace `_link(uv, panner, 'Coordinate')` with
      ```python
      vs = _node(smoke, u.MaterialExpressionComponentMask, r=False, g=True, b=False, a=False); _link(uv, vs, '')
      widen = _node(smoke, u.MaterialExpressionLinearInterpolate)
      _link(_constant(smoke, .7), widen, 'A'); _link(_constant(smoke, 1.6), widen, 'B'); _link(vs, widen, 'Alpha')   # x0.7 at the top (wide), x1.6 at the pot (narrow)
      scale = _node(smoke, u.MaterialExpressionAppendVector); _link(widen, scale, 'A'); _link(_constant(smoke, 1), scale, 'B')
      centre = _node(smoke, u.MaterialExpressionConstant2Vector, r=.5, g=0.)
      tapered = _add(smoke, _mul(smoke, _sub(smoke, uv, centre), scale), centre)
      _link(tapered, panner, 'Coordinate')
      ```
      (add a `_sub` helper beside `_add`/`_mul` if there is none: `MaterialExpressionSubtract`, inputs 'A','B'). The lamp wisp is unaffected within 1 % because its texture is a thin central strand.
   e. `capture_game.py` chimney view: eye at the card centre height, 14 m out, looking 1.5 m down so the stack is in frame:
      `views.append(('chimney-smoke', (sp.x/100 + 3, sp.y/100 + 13.7, None, sp.z), (sp.x/100, sp.y/100, None, sp.z - 150), 55))`.
   f. Distance fade so back-row cards do not stack into a picket of streaks: multiply the opacity by `saturate((12000 - PixelDepth) / 4000)` — `depth = _node(smoke, u.MaterialExpressionPixelDepth)`; `far = _node(smoke, u.MaterialExpressionSaturate); _link(_mul(smoke, _add(smoke, _mul(smoke, depth, _constant(smoke, -1.)), _constant(smoke, 12000.)), _constant(smoke, 1/4000.)), far, '')` and include `far` in the final `_output(... MP_OPACITY)` product. Full to 80 m, gone at 120 m.
   Acceptance: in `chimney-smoke.png` the plume covers >= 150 x 400 px, its mean L is >= 15 % below the sky 50 px either side at three rows, no straight edge longer than 100 px, and the stack top is in frame under it; in `main-street.png` and `market.png` no column above a roof is both narrower than 40 px and brighter than the sky beside it.
2. **Streets and the market square painted at full weight** — Engineer, LEVEL. Covers main-street 2, market 1.
   a. `build_silverbrook.py` `road_field`: `min(1.0, max(0.0, 0.5 + (hw - d) / ROAD_RAMP))` → `min(1.0, max(0.0, 1.0 - (d - hw) / ROAD_RAMP))` and widen the search radius `width / 2 + extra + ROAD_RAMP / 2` → `width / 2 + extra + ROAD_RAMP`. Same change in `paint_street_edges.py` `field()`: `r = hw + ramp` and `f = min(1.0, max(0.0, 1.0 - (d - hw) / ramp))`. Set `ROAD_RAMP` / `ramp` to one grid step (5 m), not two.
   b. Plazas: in the `PLAZAS` loop, `_f = min(1.0, max(0.0, 0.5 - (...) / ROAD_RAMP))` → `_f = min(1.0, max(0.0, 1.0 - math.hypot(max(_dx, 0), max(_dy, 0)) / ROAD_RAMP))` (1 everywhere inside, one-step ramp outside), and add an earth band: the same rectangle grown by 10 m written into `ROAD_B` as `max(existing, f * (1 - ROAD_G))`, like the street earth. Mirror both in `paint_street_edges.py`: read `[(p['at'], p['size']) for p in L['places'] if p['type'] == 'plaza']` and fold them into `g` and `b` before the vertex loop.
   c. Lot lanes on a 5 m grid: `strip(p, q, half=1.2)` hits at most one vertex column; use `half = max(1.2, step / 2 + 0.05)` so every lane paints a continuous column.
   Acceptance: `main-street.png` has no 100 x 100 block in y 620-1080 whose mean sRGB G exceeds R by more than 8; `market.png` y 650-1080 reads as packed earth with no grass; `check_ground.py` still 0 floating.
3. **Sill planters on every bay package, verified visible** — Engineer, CODE. Covers house-frontage 1.
   `silverbrook_props.py` `sills` (mesh-local cm; the street face is local +Y, the planter's origin is the back edge of its trough, so the back face goes 2 cm in front of the glass line):
   ```python
   sills={'SM_SilverbrookTownhouse1':[(111,347,95)],            # canted bay: centre (A0+A1)/2, front D2+BP = 2.9+0.55, apron top .95
          'SM_SilverbrookTownhouse3':[(0,282,92)],              # canted bay: centred, front -GF+BP = 2.3+0.5, base panel .8 + sill .12
          'SM_SilverbrookTownhouse6':[(-86,267,84),(-60,297,432)]}  # ground bay BX/YG+BD, oriel XG/YF+OD; the old (-116,246) sat 19 cm inside the bay core
   ```
   Then gate it: after each `spawn(f'SillPlanter_...')`, trace against the house mesh from 1.5 m in front of the sill (at sill height + 15 cm) to the planter's origin, the same `line_trace_component` pattern `face_y` uses; a hit means the house wall stands between the street and the planter:
   ```python
   fx,fy=pt(lx,ly+150)
   assert not a.static_mesh_component.line_trace_component(u.Vector(fx,fy,p.z+lz+15),u.Vector(x,y,p.z+lz+15),True,False),('sill planter inside the wall',label,lz)
   ```
   Acceptance: `house-frontage.png` shows the trough across x 760-1440 at y 690-720 with blooms above the sill; `HOUSE_DETAILS sill_planters` counts every Townhouse1/3/6 instance.
4. **Props do not collide and the captures are not blocked** — Engineer, CODE. Covers foundation-close 1.
   a. `silverbrook_props.py` `dress_all`: run `dress_house_details()` before `dress_street_groups(L,c)`, and in `_obstacles` add the house-detail props as rects: `elif isinstance(a,u.StaticMeshActor) and a.get_actor_label().split('_')[0] in ('FlowerTub','StoopTub','GardenFence','GardenFirewood','WashingLine','SillPlanter'):rects.append(pp._rect(a))` (before the `TAG not in tags` branch). Groups then keep `clear_m` from every tub.
   b. `capture_game.py`: replace `lamps` with every dressing prop's bounds and test the ray against the sphere, not the pivot:
      ```python
      props=[a.get_actor_bounds(False) for a in u.GameplayStatics.get_all_actors_of_class(world,u.StaticMeshActor)
             if str(a.get_folder_path()).startswith('Silverbrook/Dressing') or a.get_actor_label().startswith('StreetLamp')]
      ...
      block=[c for c,ext in props if 0<(c-e).dot(d)<1000 and (c-e).cross(d).length()<150+ext.length()]
      assert not block,(name,'prop within 1.5 m of the sight line in the first 10 m',block[:3])
      ```
   Acceptance: `foundation-close.png` shows the plinth, steps, door, door tub and bay apron from the WP6c frame with nothing closer than 1.5 m to the eye; no tub geometry inside any barrel in any capture.
5. **Hay bales read as straw** — Codex, ART, then Engineer, CODE. Covers main-street 3.
   Codex: `GameClient/ArtSource/Silverbrook/PropHayBales/concept/straw-face.png`, 2048 x 2048 tileable albedo + height. Prompt: "Cut face of a tightly packed golden straw bale, individual stalks 2-4 mm thick with pale cut ends, dark shadowed gaps between stalks, slight sun bleaching, orthographic top-down tileable texture, no twine, reference the hay in the CARTS & SUPPLIES tile of the Silverbrook design board." Engineer: `build_prophaybales.py` — `M['straw']=kit.scan('SB_PropHayBales_Straw','reed_roof_04',dark=(.55,.45,.3),damp=False,normal=.5)` → the new texture via the kit's image path with `normal=1.0`; `M['twine']=kit.solid('SB_PropHayBales_Twine',(.42,.40,.36),.9)` → `(.30,.22,.12)`, and make each twine box 12 mm wide; triple the loose-stalk count and lengthen them to 0.12-0.25 m; inset each bale 2 cm from its neighbour so the stack has shadowed gaps. Rebuild, reimport `SM_SilverbrookPropHayBales`. Acceptance: at 3 m (`main-street.png` x 190-420) stalks and dark twine are readable; no face reads as a flat box.
6. **Chimney view shows the chimney** — folded into fix 1e.
7. **Side-window void on Townhouse1** — Engineer, CODE. Covers house-frontage 2. In the open editor trace from 2 m outside the right wall at `(W2 + 200, 100, WZ + 50)` toward `(W2, 100, WZ + 50)` on the frontage house: if the first hit is the plaster core or nothing, the side `glazing()` lattice is missing from the mesh — add the lattice to the side-wall windows in `build_townhouse1.py` line 170 (`glazing((x,y,WZ),o,WW,WH)` for the `(W2,1.0,(1,0,0))` and `(-W2,1.0,(-1,0,0))` entries) the way the bay lights are built, rebuild, reimport. If the hit is the lattice, close the item (grazing angle only).

Resubmit with all five views recaptured; the review is repeated against the same concepts.
