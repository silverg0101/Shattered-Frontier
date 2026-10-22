# WP6c review — Silverbrook town, pass 3 (WP6b fix list)

Reviewer: independent (Fable), did not build the work. Judged per `docs/visual-review.md` against `Silverbrook-Design-Board-v1.webp`, `UI/v5/gameplay-hud.png` and the owner's screenshots 16, 18 and 19. Commit b15f545. Every capture read at full resolution plus crops; code read: `silverbrook_props.py`, `silverbrook-props.json`, `build_silverbrook.py`, `paint_street_edges.py`, `VFX/StreetLamp/build_materials.py`, `make_chimney_smoke.py`, `capture_game.py`, `make_pairs.py`, the five `Prop*/` sheets.

**Result: 1 PASS / 4 FAIL.** Six of the eight WP6b items are resolved and visible; the smoke plumes (fix 4) and the wall-dressing placement (fix 8: 0 washing lines, planters not in frame) are not. Two new breaks: a white untextured box in the market plaza and the smoke cards reading as bright needles above the roofs in two views.

## Owner complaints

| Complaint | Status | Evidence |
|---|---|---|
| 16.webp: thin glossy plank foundation | **Fixed** | `foundation-close.png`: 65 cm matte rubble course, 4 cm proud, no slit, three steps; `foundation-check.json` 312 houses, 0 failures. |
| 18/19.webp: yellow/blue/pink lines, 3D labels, "Tt" sprites | **Fixed and now proven** | `capture_game.py` no longer forces `ShowFlag.TextRender 0`; no line, label or sprite in any of the five captures; 117 actors asserted hidden. |
| 17.webp / pass-1: bare strip, one cart, identical rows | **Mostly fixed** | `main-street.png`: carts, sacks, barrels, crates, a bench, a ladder, a firewood store, a bracket sign; adjacent houses are different packages with visibly different plaster and roof tones. Still open: the right-hand margin is empty for the first 20 m, no washing, and the camera stands behind a lamp post. |

## WP6b fix list — status

| # | Item | Status |
|---|---|---|
| 1 | Street group density, both sides, clusters | **Resolved in code** (701 groups, gate 636; clusters of 2-3 meshes visible). **Not evenly visible**: the 700 cap is spent on the first four primary streets, so the three "Road" primaries and all ten secondaries get none; `main-street.png` right margin x 1150-1900, y 560-700 holds only verge pebbles for 20 m. |
| 2 | Packed earth to the house line | **Resolved**: both house rows stand on earth (`main-street.png` x 150-700, y 560-640 and x 1150-1900, y 540-620). Left foreground x 0-430, y 620-1080 still reads as lawn (moss patches + tufts over the earth channel). |
| 3 | House variation | **Resolved and visible**: five tones (cream, grey-blue, ochre, pale green, umber plaster; blue, warm, green, dark slate roofs) and no repeated package on adjacent lots in either row. |
| 4 | Visible chimney smoke | **Not resolved**: see chimney-smoke. The plume is a 60-100 px wide streak at < 10 % contrast against the sky, and the cards appear as bright needles from the street. |
| 5 | Yards, planters, paths | **Partly**: L-fenced gardens, 10 crops, firewood stores, door tubs and stoop tubs exist (`house-frontage.png` tub on the top step x 480-540, y 840-930; `market.png` firewood stores). The tub plant is a brown twig clump and the door paths on the 5 m vertex grid are not visible in any capture. |
| 6 | Capture validity and framing | **Half**: TextRender override removed (good); market reframed and now shows the plaza; main-street eye `(24.5, 336)` stands 2.5 m behind a street lamp whose post covers x 930-1030 for the full frame height and whose lanterns cover x 760-1220, y 0-280. |
| 7 | Smeared pier beside the door | **Resolved**: `house-frontage.png` x 660-760, y 400-800 is clean world-projected ashlar. |
| 8 | Five wall-dressing props | **Assets delivered** (sheets read: sign with three boards, 3.7 m washing line with six garments, 10-rung ladder, geranium trough, four-card ivy clump — all match their prompts). **Placement short**: 0 washing lines (rule `120 <= gap <= 400 cm`, rows are 510 cm apart by construction: `frontage()` steps `2*r + 2.5` with r = 5.32 m for an 8 m townhouse); 22 sill planters but none on the frontage house because the sill loop sits inside the 40 % `wall` hash branch; ladders, signs and ivy are visible. |

## main-street — FAIL

1. **What's wrong:** A street lamp stands 2.5 m in front of the eye: its post fills x 930-1030 for all 1080 rows and its two lanterns fill x 760-880 and x 1100-1220, y 0-280. A quarter of the frame is lamp. The builder reported it and shipped it anyway.
   **Fix type:** CODE (capture). **Instruction:** fix 4.
2. **What's wrong:** Thin bright vertical needles above the right-hand roofs: x 1105-1118, y 300-480; x 1145-1155, y 350-390; x 1200-1215, y 330-480; x 1240-1250, y 380-400; and more down the row to x 1900. Each is 8-15 px wide and 40-250 px tall and brighter than the sky. They are the 1.4 x 4.2 m smoke cards seen edge-on; no plume is readable as smoke anywhere in the frame.
   **Fix type:** CODE. **Instruction:** fix 1.
3. **What's wrong:** Right-hand margin empty for the first 20 m (x 1150-1900, y 560-700 holds pebbles and two rock piles, no prop group); the nearest right-side group is the bench at ~25 m. The `max_groups` cap of 700 (also the config's upper bound) is reached on the primary streets, so the group pass never reaches the rest of the town.
   **Fix type:** LEVEL. **Instruction:** fix 3.
4. **What's wrong:** No washing line in any of the eight visible side gaps (0 placed town-wide).
   **Fix type:** CODE. **Instruction:** fix 5.
5. **What's wrong:** Left foreground x 0-430, y 620-1080 is green lawn with tufts while the right margin at the same distance is packed earth; the verge pass drops `SM_MossPatch_00/01` discs along the edge and they read as lawn.
   **Fix type:** LEVEL. **Instruction:** fix 8.

Resolved here: two-sided clusters (barrel + sacks + crate at the lamp base; hay cart at 15 m; sacks at the signpost; bench, cart and barrel on the right), five house tones, mixed packages, ladder at x 380-410, y 390-640, bracket sign at x 525-560, y 350-410, firewood store at x 30-180, y 540-640.

## market — FAIL

1. **What's wrong:** A flat white untextured box, ~1.5 m wide and ~2.5 m tall, stands in the plaza at x 990-1050, y 480-620 between the left house corner and the firewood store. Smooth default-white shading, no texture. It is at roughly world (13, -44) (25 m from the eye along the sight line). It is not hidden by `hide_debug_actors`, which hides `Places` boxes only when their material is `MI_Graybox_Marker`.
   **Fix type:** CODE. **Instruction:** fix 2.
2. **What's wrong:** Smoke needles: x 1380-1388, y 300-330 above the central chimney and x 1640-1650, y 200-310 above the right house, 8-10 px wide, brighter than the sky.
   **Fix type:** CODE. **Instruction:** fix 1.
3. **What's wrong:** The lower 35 % (y 700-1080) is still open lawn; two firewood stores (x 980-1100, y 760-920 and x 1520-1680, y 740-890) stand in it 5-8 m from any wall and read as sheds in a field. The board's house gaps are packed-earth lanes.
   **Fix type:** LEVEL. **Instruction:** fix 8.
4. Left house windows glow orange at noon (x 230-320 and 620-700, y 660-900): deferred, GFX-02 J5.

Resolved here: the plaza is now the subject (eight stalls, goods, barrels, hay, grindstone, firewood racks fill x 1000-1700, y 450-700); five house tones; no two identical neighbours.

## house-frontage — FAIL

1. **What's wrong:** No planter on the bay-window sill (sill at y 690-720, x 760-1440 is bare timber). This was asked in WP6b fixes 5 and 8. The planter loop runs only inside the `_h(label+'wall') < .4` branch, and this house's hash is outside it.
   **Fix type:** CODE. **Instruction:** fix 6.
2. **What's wrong:** The stoop tub (x 480-540, y 840-930) holds `SM_PH_FloweringClump_0` at 0.35 scale: at 3 m it is a brown dead twig clump, not flowers. Same in `foundation-close.png` x 290-370, y 400-520 and x 1280-1440, y 740-930.
   **Fix type:** ART + CODE. **Instruction:** fix 7.
3. The road embankment at x 0-400, y 690-800 shows the cobble paint stretched down a ~1.5 m terrain bank and reads as a retaining wall: deferred to GFX-02 J2 (ground relief), listed below.

Resolved here: pier block, plinth, steps, ivy clump at the far corner (x 1420-1600, y 1000-1080), lantern, sign.

## foundation-close — PASS

Matte rubble course, 4 cm proud, no slit, three steps, cobble below, ivy clump at the corner, tubs on the step and at the door. Notes, not fails: the ivy's four cards are readable as flat sheets at this distance (right card x 1700-1900 has a straight top edge); the tub plants are the same twig clump as above (fix 7).

## chimney-smoke — FAIL

1. **What's wrong:** The plume is a faint vertical streak x 900-1000, y 250-600, under 10 % contrast against the clouds behind it, with no visible chimney at its base (the only roof fitting in frame is the finial at x 1100, y 580). The acceptance in WP6b fix 4 was a plume covering >= 150 x 400 px at 20 m; the builder's own figure is at most 129 x 387 px, and the capture shows far less because the card is lighter than the sky. Smoke against a bright sky reads by being greyer than the sky, not whiter.
   **Fix type:** CODE + LEVEL. **Instruction:** fix 1 (material, card size, tint) and fix 4 (view).
2. Far white faceted mountains x 0-220 and 1280-1920, y 850-1080: deferred, GFX-02 J1b/J2.

## Deferred to GFX-02 / GFX-03 (seen, not failed here)

- Flat cobble and lawn tiles without relief; the cobble paint stretched down the bank in `house-frontage.png` x 0-400, y 690-800 — GFX-02 J2.
- Lamps and windows lit at noon (`main-street.png` lamps, `market.png` left house) — GFX-02 J5.
- Far mountains: white faceted shapes (`main-street`, `chimney-smoke`), smooth brown-grey domes (`market`) — GFX-02 J1b/J2.
- Ashlar tiles repeat, plaster is flat, no corner wrap — GFX-03 1-2.
- `SM_MarketStall` stand-in with flat striped cloth — GFX-03 gap 7.
- No decals, no per-instance weathering — GFX-03 gaps 5-6.

## Fix list (shared first)

1. **Smoke cards: no needles, visible plume** — Engineer, CODE. Covers main-street 2, market 2, chimney-smoke 1.
   a. `GameClient/ArtSource/VFX/StreetLamp/build_materials.py`, in `build()` after the `EdgeFade` lerp (line ~166), add a view-angle fade so an edge-on card is invisible, then route opacity through it:
      ```python
      cam = _node(smoke, u.MaterialExpressionCameraVectorWS); nrm = _node(smoke, u.MaterialExpressionVertexNormalWS)
      dot = _node(smoke, u.MaterialExpressionDotProduct); _link(cam, dot, 'A'); _link(nrm, dot, 'B')
      facing = _node(smoke, u.MaterialExpressionAbs); _link(dot, facing, '')
      view = _node(smoke, u.MaterialExpressionSaturate)   # 0 within 15 deg of edge-on, 1 beyond ~37 deg
      _link(_mul(smoke, _add(smoke, facing, _constant(smoke, -.25)), _constant(smoke, 1.8)), view, '')
      vf = _node(smoke, u.MaterialExpressionLinearInterpolate); _link(_constant(smoke, 1), vf, 'A'); _link(view, vf, 'B')
      _link(_node(smoke, u.MaterialExpressionScalarParameter, parameter_name='ViewFade', default_value=0.), vf, 'Alpha')
      _output(_mul(smoke, _mul(smoke, _mul(smoke, _alpha(smoke, image), opacity), fade), vf), u.MaterialProperty.MP_OPACITY)
      ```
      (replace the existing `_output(... MP_OPACITY)` line). Lamp wisps keep `ViewFade` 0.
   b. `silverbrook_props.py` `dress_house_details`: after the `EdgeFade` set, add `mel.set_material_instance_scalar_parameter_value(smoke,'ViewFade',1.)`. Card: `spawn(f'ChimneySmoke_{label}_{k}',plane,x,y,p.z+sz*100+230,yaw+35+90*k,(1.4,4.2,1))` → `spawn(...,p.z+sz*100+320,yaw+35+90*k,(2.2,6.0,1))` (2.2 x 6 m, base 20 cm above the pot).
   c. `silverbrook-props.json` `chimney_smoke`: `opacity.value` 0.45 → 0.7; `tint.value` [0.8,0.8,0.83] → [0.58,0.58,0.62] (greyer than the sky).
   d. `make_chimney_smoke.py`: `profile = np.exp(-((u[None, :] - sway) / .24) ** 2)` → `/ .30`, and `alpha = ... * .9` → `* 1.0`; rerun, reimport `T_ChimneySmoke`.
   Acceptance: in `chimney-smoke.png` the plume is >= 150 x 400 px and at least 15 % darker than the adjacent sky; no needle narrower than 40 px above any roof in `main-street.png` or `market.png`.
2. **White box in the market plaza** — Engineer, CODE. In the editor (`tools/Run-InEditor.py`): `[(a.get_actor_label(),a.get_class().get_name(),str(a.get_folder_path())) for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors() if math.dist((a.get_actor_location().x/100,a.get_actor_location().y/100),(13,-44))<5 and not a.get_editor_property('hidden')]` names it. Then: `silverbrook_props.py` `hide_debug_actors`: `marker=... str(a.get_folder_path())=="Places" and ...get_name()=="MI_Graybox_Marker"` → `marker=isinstance(a,u.StaticMeshActor) and str(a.get_folder_path()) in ("Places","Plots")` (hide every authoring box regardless of material), and apply the same in `build_silverbrook.py`'s hidden-in-game pass. If the actor is a kit mesh with an unassigned slot, assign it in `import_silverbrook.py`. Gate: in `GameClient/ArtSource/Greenlands/check_hidden_in_game.py` add, for every `StaticMeshActor` not hidden and inside the wall, `assert all(m and m.get_name() not in ('WorldGridMaterial','BasicShapeMaterial','DefaultMaterial') and not m.get_name().startswith('MI_Graybox') for m in [c.get_material(i) for i in range(c.get_num_materials())])`.
3. **Street groups reach every street** — Engineer, LEVEL. `silverbrook-props.json` `street_groups`: `max_groups` `{"value": 700, "max": 700}` → `{"value": 1500, "max": 2000}`; `spacing_m.value` 4 → 5. `dress_street_groups`: print `road_counts` already; add `assert min(road_counts.values())>0, road_counts` so no street inside the wall is left bare. Acceptance: `main-street.png` right margin has >= 2 groups within the first 20 m.
4. **Capture framing** — Engineer, CODE. `capture_game.py`:
   - `('main-street', (24.5, 336, 1.7), (0, -30, 4), 70)` → `('main-street', (29.0, 336, 1.7), (4.5, -30, 4), 70)` (South Street's centre line at y = 336 is x = 29.0; same sight-line direction, lamps on the verges are 4.8 m off-axis and out of a 70 deg frame at 3 m).
   - Let `eye` take an absolute z like `look`: `z = eye[3] if len(eye) > 3 else ground(x, y) + h*100`. Chimney view: `views.append(('chimney-smoke', (sp.x/100 + 3, sp.y/100 + 15.7, None, sp.z - 280), (sp.x/100, sp.y/100, None, sp.z), 55))` — eye 16 m away at pot height, aimed at the card centre; with the 2.2 x 6 m card this is ~275 x 530 px.
   - Before each `HighResShot`, assert no `StreetLamp*` actor within 1.5 m of the eye-to-look ray in the first 10 m: `assert not [l for l in lamps if 0 < dot(l-eye, dir) < 1000 and |cross(l-eye, dir)| < 150]`, `lamps` collected once from `StaticMeshActor` labels starting `StreetLamp`.
   Recapture all five views and remake the pairs.
5. **Washing lines that place** — Engineer, CODE. `silverbrook_props.py` `dress_house_details`: add `pp,rects=_obstacles(A)` after `A=_actors()`, compute `x0,x1,y0,y1=_base_box(mesh,band)` right after `label=...` (cached, cheap), remove the `for o in houses:` washing block from the 40 % branch, and after it add for every house with `_h(label+'line')<.5`:
   ```python
   gside=(-1 if _h(label+'side')<.5 else 1) if h<.75 else 0   # this house's garden side
   spot=None
   for o in houses:  # a free gap to a same-row neighbour: lots are 5.1 m apart, so the line (3.7 m) scales to the gap
       if o==a:continue
       op=o.get_actor_location();dx,dy=op.x-p.x,op.y-p.y;ox,oy=co*dx+si*dy,-si*dx+co*dy
       if abs(oy)>200 or abs(((o.get_actor_rotation().yaw-yaw)+180)%360-180)>15:continue
       toward=1 if ox>0 else -1;ol=o.get_actor_label()
       if toward==gside or ol in lined:continue                                   # our garden fills that gap
       if _h(ol)<.75 and (-1 if _h(ol+'side')<.5 else 1)==-toward:continue        # the neighbour's garden fills it
       ob=_base_box(o.static_mesh_component.static_mesh,band)
       gap=(ox+ob[0]-x1) if ox>0 else (x0-(ox+ob[1]))
       if not 300<=gap<=800:continue
       gx=(x1+ox+ob[0])/2 if ox>0 else (x0+ox+ob[1])/2
       spot=(gx,(y0+y1)/2,yaw,max(.8,min(1.3,(gap-100)/370)));lined.add(ol);break
   if spot is None:spot=(-gside*100,y0-230,yaw,1.0)   # rear yard on its own post pair, parallel to the back wall
   gx,gy,lyaw,sx_=spot;x,y=pt(gx,gy);z=terrain_z(x,y)
   if z is not None and pp._free((x-200,y-60,x+200,y+60),rects):
       spawn('WashingLine_'+label,line,x,y,z,lyaw,(sx_,1,1));lined.add(label);counts['washing_lines']+=1
   ```
   The line runs along the house X axis (wall to wall across the gap, or parallel to the back wall). Acceptance: `washing_lines >= 60` in `HOUSE_DETAILS`, and at least one visible in `main-street.png`.
6. **Sill planters on every bay window** — Engineer, CODE. `dress_house_details`: move the `for lx,ly,lz in sills.get(mesh.get_name(),[]):` loop out of the `if _h(label+'wall')<.4:` branch to run for every house (it matches Townhouse6 only). Acceptance: planter on the sill in `house-frontage.png`.
7. **Flower tub prop** — Codex, ART, then Engineer, CODE. `GameClient/ArtSource/Silverbrook/PropFlowerTub/` built like `PropSillPlanter` (Blender, <= 6k tris, origin bottom centre, 0.5 m half-barrel 0.45 m tall with geraniums 0.3 m above the rim, red and pink bloom variants on the `SB_GeraniumBloom` slot, imported as `SM_SilverbrookPropFlowerTub`). Prompt: "Medieval oak half-barrel planter, iron hoops, filled with red geraniums and trailing thyme, damp soil, 3/4 view asset sheet with front/side/top on a neutral grey background, reference the FLOWERS & FOLIAGE and SEATING & DECOR tiles of the Silverbrook design board." Then in `dress_house_details` replace each `spawn('FlowerTub_'+label,barrel,...)` + `spawn('Flower_'+label,flower,...)` pair (and the `StoopTub_`/`StoopFlower_` pair) with one `spawn('FlowerTub_'+label,tub,x,y,z,yaw,(1,1,1))` with the bloom MI chosen by `_h(label+'bloom')`.
8. **Ground between houses reads as lanes, not lawn** — Engineer, LEVEL. `silverbrook-props.json` `verge.meshes`: remove `SM_MossPatch_00` and `SM_MossPatch_01` (moss discs on packed earth read as lawn). `paint_street_edges.py` door-path pass: widen the strip `<= 0.6` → `<= 1.2` (2.4 m) and, for each house, also paint B = 1 in a 2.4 m strip along the lot edge opposite its garden side from the road edge to 3 m behind the rear wall (same `nearest`/cell loop with the segment from `pt(-gside*(x1+120), y1+400)` to `pt(-gside*(x1+120), y0-300)`). Acceptance: in `market.png` the ground between the left house and the plaza (y 700-1080) shows earth lanes, and `main-street.png` x 0-430, y 620-1080 is earth, not green.

Resubmit with all five views recaptured; the review is repeated against the same concepts.
