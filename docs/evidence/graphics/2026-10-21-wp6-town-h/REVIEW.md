# WP6h review — Silverbrook town, pass 8 (WP6g fix 2-6, foundations, lamp sockets, context views)

Reviewer: independent (Fable), did not build the work. Judged per `docs/visual-review.md` against `Silverbrook-Design-Board-v1.webp` and `UI/v5/gameplay-hud.png` (mill plume, x 1330-1360, y 100-170), and the owner's images 16 (floating house), 17 (orange light at a stall), 21 (mirrored road clutter), 22 (white street). Commits 09ebb4ca, 5413fa8e, 824627e6. Every capture read at full resolution plus crops (`chimney-smoke.png` x 600-1400 / y 0-700 and 2x at x 760-1160 / y 300-620; `main-street.png` x 700-1300 / y 0-420; `context-lamp-wall.png` 3x at x 1200-1480 / y 420-700; `context-house-side-b.png` 3x at x 1250-1560 / y 150-480; `context-town-edge.png` 2.5x at x 1200-1920 / y 380-520; `context-overview-se.png` 2x crops). Plume measured independently of `measure_plume.py` over the 14 motion frames (temporal minimum as background, luminance rows sampled by hand); street thread re-measured with the review definition; `plume-measure.json` cross-checked (it disagrees with the README table on item 1: 739-1094 / 356 / 916 in the JSON, 710-1094 / 385 / 902 in the README).

**Result: 9 PASS / 6 FAIL.** Bookmarks 4 / 1 (chimney-smoke fails for the seventh time, now on parameters, not technique). Context views 5 / 5. Owner reports: floating houses **fixed and visible**; orange light with no source **not fixed** (the light moved to the glass, but building-lantern glass does not glow); road clutter **not fixed** (left unchanged by instruction, and the context views were taken where it cannot be seen).

## Owner reports

| Report | Status | Evidence |
|---|---|---|
| Floating houses (image 16: plank strip under the front, open air under the side walls) | **Fixed, visible.** | `foundation-close.png`: rubble plinth under the full wall, buried, corner closed. `context-house-side-a/b/rear.png`: plinth follows every side wall, meets the foot of the ashlar with no gap, steps with the ground (rear: 1.0 m at the left, 0.6 m at the right). Gate `FOUNDATION_CHECK 312 houses, 0 failing, max_gap 1.5 cm, min_buried 101.7 cm` is independent of the placement code. |
| Orange light with no visible source (image 17) | **Not fixed.** | The socket fix put the light at the pane (`LIGHT_CHECK 795 on_glass, 0 orphans`), but the pane of every building lantern is dark: `context-lamp-wall.png` lantern glass mean RGB 97/64/37 while the wall 0.3 m from it reads 254/249/191 (clipped); `context-house-side-b.png` glass 142/101/56, wall wash 249/234/175. A street-lamp pane in the same light (`context-house-side-rear.png` x 260-300, y 400-470) glows yellow, so the difference is the material, not exposure. To a player the wall still lights itself. The lamp is also lit at noon (J5, below). |
| Street clutter in mirrored rows on the empty approach road (image 21) | **Not fixed.** | README: "street-group placement along the approach road is unchanged, as instructed". The three road context views are outside the wall, where `dress_street_groups` never places anything (`_inside`), so none of them shows the reported spot. The clutter is visible in this pass's own `chimney-smoke.png` (x 600-1300, y 340-420, 3x): sack piles, crates, barrels, benches and hitching posts in a row along the field side of the far road with no building behind them. The root cause is in the code: one station `k` places `side +1` and `side -1` with the same `mesh`, so every group has an identical twin across the road. |

## main-street — PASS (no regression)

Same street as WP6g: cobbles to both kerbs, five house tones, mixed packages, barrels, crates, hay, sacks, bench, cart, ladder, signs, washing line, lamps, nothing broken, no placeholder material. Chimney thread (review definition, R-B excess >= 5 over the sky median 5-45 px either side): x 790-830 23 columns at y 386, thread L / sky 0.967 (not brighter than the sky); x 1085-1125 0 columns. One thread of two, faint; meets the >= 15 criterion. Lit windows and lamp glows at noon: J5 (deferred).

## market — PASS

Stalls, firewood, sacks, barrels, ladder, mixed house tones, cobbles with earth margins; the far houses sit on their plinths. Nothing broken. Stall stand-in: GFX-03 (deferred).

## house-frontage — PASS

Bay window with planter, boot sign, bracket lantern, stoop with tub, ivy; plinth under the bay. The lantern pane here does glow (its glass faces the sun) and the first-floor jetty is lit orange by the lamp above at noon (J5).

## foundation-close — PASS

Rubble plinth 1.0 m high under the ashlar on both faces of the corner, buried, top flush with the foot of the wall; tub and bay planter sit on the stoop. This is the view that failed in image 16; it now reads as a house on a stone base. The plinth's flat, even top is acceptable for a built base course.

## chimney-smoke — FAIL

Measured on `chimney-smoke.png` and `motion-00..13.png` (pots at y 575-620, x 885-920; 131.7 px per m at the stack; wind +x = screen right):

| Acceptance (WP6g) | WP6h, measured here | Met |
|---|---|---|
| 1 origin: span 30-130 px at y 550, centre in x 860-940 | Dense core (L 152-169 over slate at L 53-61, i.e. >= 85 % coverage) spans x 950-1010 at y 560, 60 px, but a half-density veil continues to x 1290 (L 95-120 over the same slate): total 340 px, centre x 1120 | No |
| 2 growth: span at y 150 >= 2.0 x span at y 550, <= 450 px | Nothing at y 150 (14-frame mean excess over the temporal minimum 2.2, threshold 6); nothing at y 200 or 260 either. The plume ends about 3.3 m above the pot (y 300-350, where it is 690 px = 5.2 m wide) | No |
| 3 no veil beside the stack | Rows y 460-540: brightening across x 949-1408 (up to 250 columns), all to the right; none left of x 790 | Half: no left veil, but a 3.5 m right-hand veil |
| 4 density: peak R-B excess >= 12 in y 380-540, >= 6 at y 100 | 53.6 at y 380-540 (builder) and 1.6 at y 100: the foot is far past the target, the top has nothing | No |
| 5 luminance: plume within 10 % of the sky, never > 5 % brighter | Plume core L 169-174 against field at 150 and slate at 70: 15 % brighter than the field, 2.4 x the slate; at y 350 the plume is 1.33 x its surroundings | No |
| 6 puffs: left-edge sd >= 8 px per 100 rows, >= 3 density maxima | Builder's 6.4 / 109 / 35 / 41 px and 7 maxima come from a mask that includes roof edges; by eye (2x crop) the body is a smooth fog with two or three lumps, no separate puffs below 2 m | No |
| 7 motion: >= 20 px/s up, <= 60 px/s drift, consecutive MAD >= 2 | Consecutive mean abs difference inside x 760-1100, y 0-560: min 0.92, mean 1.18 (builder: 0.92 / 1.19); row-profile cross-correlation gives 0 px/s in 11 of 13 pairs (builder 25.6 mean with five zeros). The body of the plume does not move between frames; only the first metre above the pot and the faint top churn (`motion-00/04/08/12` crops) | No |
| 8 stack in frame, no rectangles or hard edges | Pots at y 575-620; no card edge anywhere | Yes |
| street thread | 23 columns at x 790-830, L / sky 0.967 | Yes |

1. **What's wrong:** the technique now works (camera-facing puffs, no card edge, a thread visible from the street, 0 orphan actors, perf inside the gate) but the plume is a squat, bright fog bank: about 3.3 m tall, 5 m wide, 85-90 % opaque in its first metre, 15-33 % brighter than what is behind it, and static from frame to frame. The concept's mill plume is a thin grey column a stack-width wide at the pot, lighter than the trees but darker than the sky, rising well over six stack-widths before it fades. Three parameter choices cause all of it: (a) `Drag` 0.4 on a 1.6 m/s rise gives a total rise of 1.6 / 0.4 x (1 - e^-1.6) = 3.2 m, so every puff dies at roof height and the 4 s lifetime piles 24 puffs into 3 m; (b) 24 live puffs of 60-220 cm at opacity 0.55 overlap to near-opaque, which also hides their motion (a saturated alpha does not change when its layers move); (c) `Tint` 0.42 lit by the noon sun renders at L 165-175, brighter than the field and the slate, so it reads as steam, not wood smoke. The remaining gap is **parameters** (eight User values and three module constants), not the technique.
   **Fix type:** LEVEL (values) + CODE (three constants). **Exact instruction:** item 1 of the fix list.

2. **What's wrong:** `measure_plume.py` measures R-B against an interpolated sky, but this eye has the hill, the far town and the roofs behind the plume (only y < 60 is sky), so items 2, 4 (y 100) and 5 report "no plume" where the plume is simply over green. The builder's numbers cannot be compared with the acceptance table.
   **Fix type:** CODE. **Exact instruction:** item 2.

## Context views

### context-overview-se — PASS

Whole town from 230 m: the inner rings, the Riverside row to the bridge, the wall, the lake, roads out of each gate. Nothing lined up in the fields, no road ends in nothing. Seen and not failed here (pre-WP6, world layout): the wall that crosses the lake as a 150 m causeway and the straight canal into the eastern pond; both are design questions for the world pass, not dressing.

### context-overview-nw — FAIL

The frame is 95 % bare terrain: the eye at (-560, -620) sits 200 m above the northern ridge and looks at its own slope; the town is a strip in the top-left corner. This is a broken capture, not a view of the area. **Fix type:** CODE. `capture_game.py` line 62-65: take the eye height from the town-centre ground, not the local ground, and refuse an eye whose sight line to the target is blocked:
```python
for name, (bx, by), alt, pitch in (('context-overview-se', (520, 640), 230, 38), ('context-overview-nw', (-430, -470), 230, 42)):
    e = u.Vector(bx*100, by*100, ground(0, -30)*100 + alt*100); tx, ty = 0, -30; dist = alt / math.tan(math.radians(pitch))
    dx, dy = tx - bx, ty - by; n = math.hypot(dx, dy); look = u.Vector((bx + dx/n*dist)*100, (by + dy/n*dist)*100, e.z - alt*100)
    assert not clear(e, look), f'{name}: terrain between the eye and the town'
    add(name, e, look, 80)
```

### context-road-silvermere — FAIL

The eye stands in a trench: a dark vertical terrain wall on the left, a near-vertical bank on the right with the cobble material stretched up it, sky in a slot. The road north of the North Gate is cut through the northern ridge (120 m) at road grade with no side slopes, and `road_field` paints cobbles by 2D distance from the centreline, so the trench walls are cobbled. A player would find a paved canyon. **Fix type:** LEVEL (Engineer). `build_silverbrook.py`: the Heightfield road corridor needs side slopes <= 30 deg (bank width >= cut depth / tan 30 deg each side, carved before `apply_relief`), and `road_field` must not paint slopes: multiply each cell's field by `1 - rockiness(v)` (the existing slope term, `SLOPE_FLAT_DEG`..`SLOPE_ROCK_DEG`) so banks over `SLOPE_FLAT_DEG` take bedrock, not cobbles. Recapture the same seed; the view must show a road with banks no steeper than 30 deg and cobbles only on the carriageway.

### context-road-everwood — PASS

Wide cobbled road into the woods, dirt verges with shrubs, pebbles and tufts, no props, trees and far hills. Nothing broken. The 12 m cobble carriageway outside the wall and the untextured beige mountains are world items (GFX-02 J1b), not dressing.

### context-road-valemere — PASS

Same character as Everwood: no props, rocks and shrubs on the verge, forest, open sky.

### context-town-edge — FAIL

Wall from 25 m out: segments and towers sit on the grass, nothing floats. Beyond the near wall (x 1600-1750, y 420-470, 2.5x crop) eight street lamps stand in pairs along an empty road through open ground inside the wall, no building near any of them. That is the brief's "props lined up along empty roads". **Fix type:** LEVEL. `build_silverbrook.py` street-lamp loop (lines 633-660): add a frontage test to `ok`:
```python
                  and any(math.dist(post, q[:2]) < 18 for q in placed)   # a building pad within 18 m: no lamps along empty roads
```
and keep lamps within `SL['clear']` of a gate or bridge regardless (they are the one place an empty road needs a light): `or any(math.dist(post, q) < SL['clear'] + 8 for q in bridges + list(gate_points.values()))`.

### context-house-side-a — PASS

Side wall on its rubble plinth, buried, fence and garden bed in front, firewood and lamp beyond. No gap anywhere along the foot.

### context-house-side-b — FAIL

The plinth is right (steps down to the stoop base at the right, buried at the left). The view fails on the lamp: the bracket lantern at x 1460-1500 is dark (RGB 142/101/56) while the ashlar beside it is washed orange to clipping (249/234/175) at noon. Same cause and fix as context-lamp-wall (item 3).

### context-house-side-rear — PASS

Rear wall on a plinth that follows the slope (1.0 m to 0.6 m), firewood, fence, street lamp with a glowing pane at the left. Nothing floats.

### context-lamp-wall — FAIL

1. **What's wrong:** the wall between the sign and the corner quoin is clipped white-orange (mean RGB 254/249/191 over x 1020-1100, y 560-640) and the quoin face is lit to 254 for 0.9 m, while the lantern that should be the source (x 1310-1370, y 520-600) shows a dark tan pane (97/64/37). The owner's "orange light with no visible source" is reproduced exactly: the light is now in the right place but nothing glows there, and 900 lm at 10 cm from a wall clips the stone.
   **Fix type:** CODE. **Exact instruction:** item 3.

## Deferred (seen, not failed here; GFX-02 / GFX-03, unchanged from WP6g)

Lamps and windows lit at noon (J5; item 3c closes it for lamps when the owner wants it now); flat cobble relief and cobble paint down the bank (J2); untextured beige mountains and faceted far peaks (J1b); flat cyan water, the lake causeway and the straight canal (GFX-02 water / world); ashlar repeats, plaster, stall stand-in, decals, disc blooms, low-poly trees (GFX-03).

## Fix list (shared first)

1. **Chimney plume values** — Engineer, LEVEL + CODE. `silverbrook-props.json` `chimney_smoke`: `spawn_rate` 6 -> **3.5** (2-12), `lifetime` 4.0 -> **5.5** (2.5-6), `rise_speed` 1.6 -> **2.4** (0.8-3), `wind` [0.8,0,0] -> **[0.35, 0, 0]**, `start_size` 0.6 -> **0.35** (0.3-1), `end_size` 2.2 -> **2.8** (1.2-3.5), `opacity` 0.55 -> **0.22** (0.2-0.8), `tint` [0.42,0.42,0.45] -> **[0.24, 0.24, 0.26]**. `build_chimney_niagara.py`: `put(PU, drag, 'Drag', local(T_FLOAT, value=.4))` -> **`.12`**; `Noise Strength` 40 -> **25**; `Noise Frequency` .015 -> **.01**; Scale Alpha keys `0 @ 0, 1 @ 0.08, 0.8 @ 0.5, 0.35 @ 0.8, 0 @ 1` -> **`0 @ 0, 0.9 @ 0.06, 0.5 @ 0.4, 0.2 @ 0.75, 0 @ 1`** (same expression shape: `a < 0.06f ? a / 0.06f * 0.9f : (a < 0.4f ? lerp(0.9f, 0.5f, (a - 0.06f) / 0.34f) : (a < 0.75f ? lerp(0.5f, 0.2f, (a - 0.4f) / 0.35f) : lerp(0.2f, 0.0f, (a - 0.75f) / 0.25f)))`); size keys stay. Expected: 19 live puffs, 0.69 m between puffs at the pot (distinct), rise 2.4 / 0.12 x (1 - e^-0.66) = 9.7 m so the column leaves the frame top, peak opacity at the foot ~0.33 (two 0.18 puffs), plume darker than the field. Rebuild `NS_ChimneySmoke` with the script, rerun `apply_dressing.py`, update `vfx.md` section 2 values (one row, keep "spec, not yet passing").
2. **Background-independent plume measure** — Engineer, CODE. `capture_game.py`: after `chimney-smoke`, hide every `ChimneySmoke_*` NiagaraActor (`set_actor_hidden_in_game(True)`) and capture `chimney-smoke-ref.png` from the same eye, then unhide before the motion set. `measure_plume.py`: every acceptance item uses `excess = L(frame) - L(ref)` (7-row averaged) with threshold 4 in place of the R-B-vs-sky mask; item 5 becomes "plume L / ref L within 0.90-1.05 at rows 30, 80, 260 where excess >= 4", item 7 unchanged. Print the eight numbers plus the street thread as now.
3. **Lamp source and clipping** — Engineer, CODE, `GameClient/ArtSource/Greenlands/lamp_fx.py`. (a) In `apply`, after the street-lamp glass loop, set the building lantern panes to glow: for `MI_SB_LanternGlow` (and the `*_Glow` instances `gate_windows` prints as ALWAYS_LIT that belong to lanterns) `mel.set_material_instance_vector_parameter_value(mi, 'Emission', u.LinearColor(*cfg['flame_colour'], 1)); mel.set_material_instance_scalar_parameter_value(mi, 'EmissionStrength', cfg['glass_emission_strength'])`, save; acceptance: the pane at `context-lamp-wall.png` x 1310-1370, y 520-600 reads mean L >= 200. (b) `_point_light`: for owners that are not `StreetLamp_` set `intensity` to `cfg['light_intensity'] * 0.5` (450 lm), `source_radius` 6 cm, and place the light 30 cm in front of the pane instead of 10 (`glass_sockets`: `pane + normal * 30`); acceptance: no pixel within 1 m of a wall lantern clipped (max channel <= 235) in `context-lamp-wall.png` and `context-house-side-b.png`. (c) Owner's noon complaint (J5): put the `M_SB_WindowGlow` sun gate (`SkyAtmosphereLightDirection.z` against `SinBegins` / `SinFull`) into `M_LampLightFunction` and `M_LampGlass` in `build_materials.py` so lights, panes and windows switch together at sun elevation 4 deg -> -4 deg; `lamp-fx.json` `lit` becomes `time-of-day` with `always` as the dev override. Recapture `context-lamp-wall`, `context-house-side-b`, `house-frontage`, `main-street`.
4. **Street groups: frontage rule, no mirrored pairs, irregular spacing, waystations only outside** — Engineer, CODE, `GameClient/ArtSource/Silverbrook/silverbrook_props.py` `dress_street_groups`. Rule: a group stands only on a side of the road that has a building within 12 m along and 18 m back; stations are skipped at random (35 %) and slid up to +-40 % of a step; one side per station, never both; roads outside the settlement get nothing except within 15 m of a waystation listed in the layout. Add two helpers above `dress_street_groups`:
```python
def _fronts(A,folder='Districts/'):
    """World (x, y) cm of every Silverbrook building: street groups stand only where a frontage has one."""
    return [(a.get_actor_location().x,a.get_actor_location().y) for a in A.get_all_level_actors()
            if isinstance(a,u.StaticMeshActor) and str(a.get_folder_path()).startswith(folder) and a.static_mesh_component.static_mesh
            and a.static_mesh_component.static_mesh.get_name().startswith('SM_Silverbrook') and '_Door' not in a.static_mesh_component.static_mesh.get_name()]
def _has_frontage(fronts,x,y,ux,uy,side,along=1200,depth=1800):
    """A building within `along` cm of the station (x, y cm) on this side of the road and within `depth` cm of it."""
    for bx,by in fronts:
        dx,dy=bx-x,by-y
        if abs(ux*dx+uy*dy)<=along and 0<side*(ux*dy-uy*dx)<=depth:return True
    return False
```
   In `dress_street_groups`, after `assert all(meshes),names` add `fronts=_fronts(A);ways=[tuple(w) for w in L.get('waystations',[])]`, and replace the station loop head (from `for k,(x,y,ux,uy) in enumerate(_stations(pts,step)):` through `for side,shift in ((1,0),(1,0.8),(-1,0),(-1,0.8)):` and its `d=` line) with:
```python
        for k,(x,y,ux,uy) in enumerate(_stations(pts,step)):
            key=f'group:{road}:{k}'
            if _h(key+'skip')<val(g['skip_chance']):continue                         # irregular spacing: about a third of the stations stay empty
            j=(_h(key+'jit')-.5)*step*.8;x,y=x+ux*j,y+uy*j                           # the rest slide up to +-40 % of a step
            way=any(math.hypot(x-wx,y-wy)<15 for wx,wy in ways)
            if total>=val(g['max_groups']) or road_counts[road]>=quota[road] or not (_inside(L,c,x,y) or way):continue
            sides=[s for s in (1,-1) if way or _has_frontage(fronts,x*100,y*100,ux,uy,s)]   # only a frontage with a building
            if not sides:continue
            side=sides[int(_h(key+'side')*len(sides))%len(sides)]                     # one side per station: never a mirrored pair
            mesh=meshes[int(_h(key)*len(meshes))%len(meshes)]
            for shift in (0,0.8):
                d=width/2+offset+shift
```
   (the body from `px=` on is unchanged). Replace the gate at the end with frontage-aware counts:
```python
    need=sum(1 for road,tier,pts,width in _roads(L,c) for x,y,ux,uy in _stations(pts,50)
             if _inside(L,c,x,y) and any(_has_frontage(fronts,x*100,y*100,ux,uy,s) for s in (1,-1)))*3
    print('STREET_GROUPS',total,'GATE',need,road_counts)
    assert total>=need,('STREET_GROUPS below 3 per fronted 50 m',total,need)
    bare=[r for r,tier,pts,width in _roads(L,c) if road_counts[r]==0 and any(_inside(L,c,x,y) and any(_has_frontage(fronts,x*100,y*100,ux,uy,s) for s in (1,-1)) for x,y,ux,uy in _stations(pts,step))]
    assert not bare,('fronted streets left bare',bare)
```
   `silverbrook-props.json` `street_groups`: add `"skip_chance": {"value": 0.35, "min": 0.0, "max": 0.6, "unit": "share of stations left empty"}`; `max_groups` 1500 -> 900. `silverbrook-town-layout.json`: add `"waystations": []` (metres; none yet, the Arrival waystone gets one when the arrival road is dressed). Recapture `main-street`, `market`, `chimney-smoke`, and add one context view at the owner's spot: eye at the Silverbrook Bridge's south end looking along the road to the Riverside row (image 21's view), named `context-road-approach.png`; acceptance: no group without a building behind it, no identical pair across the road, no two groups at the same chainage. Update `docs/dependency-map.md`'s WP6 row.
5. **Street lamps on empty roads** — Engineer, LEVEL. `build_silverbrook.py` lamp loop per context-town-edge above (pads within 18 m, or within `clear` + 8 m of a gate or bridge). Rebuild and recapture `context-town-edge` with the same seed.
6. **Overview NW capture** — Engineer, CODE. `capture_game.py` per context-overview-nw above (town-centre ground + 230 m, point (-430, -470), sight-line assert). Recapture both overviews.
7. **Silvermere road trench** — Engineer, LEVEL. Road corridor side slopes <= 30 deg and no cobble paint on slopes over `SLOPE_FLAT_DEG` per context-road-silvermere above. Rebuild the terrain, recapture the three road views.
8. **Resubmit** — Engineer. Rerun `apply_dressing.py`, all gates (`check_foundations`, `check_lights`, `check_chimney_smoke`, `check_hidden_in_game`, `check_ground`, `check_details`, `DEFAULT_MATERIAL_CHECK`), recapture the five views, `chimney-smoke-ref`, the 14-frame motion set, the ten context views plus `context-road-approach`, the 1440p perf pair, and resubmit with `measure_plume.py` printing items 1-8 against the reference frame.
