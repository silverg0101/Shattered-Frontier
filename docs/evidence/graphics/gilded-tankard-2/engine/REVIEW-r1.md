# The Gilded Tankard — Unreal stage review (dev map `GildedTankard_Review`)

Judge: independent art director (did not build this). Inputs: concept `docs/Game-Design/concept-art/Silverbrook/The Gilded Tankard.png`; the eleven `<view>.png` captures (1920x1080) and their `-compare.png` pairs in this folder; `README.md` run order; `build_gilded_tankard.py`, `silverbrook_kit.py`, `silverbrook_props.py`, `tools/Create-BuildingDevMap.py`, `tools/capture_building.py`, `tools/check_foundations.py`, `GameClient/ArtSource/Greenlands/lighting.py` + `silverbrook-lighting.json`; `docs/blueprints/buildings.md` §5/§6/§8, `props-and-dressing.md`. Pixel coordinates below are in the raw 1920x1080 captures; luminance is the 0-255 mean of the named region.

Result: **3 PASS / 8 FAIL**. The model itself (Blender r5) carries over; the failures are the in-engine wrapper: the street face is captured in full shade, the dressing pass hangs a second, wrong-trade sign on a building that bakes its own, the foundation pass builds a stone apron around the baked barrels and bench, and the lanterns render as bare emissive cubes.

## frontage — FAIL

1. The whole street face is in shadow: the sun (`silverbrook-lighting.json` yaw 135, pitch -55) travels toward +Y, and the building's front is +Y after import. Front plaster (900-1100, 150-350) luminance 27, front ashlar (900-1300, 600-900) 17, versus rear plaster 125 and rear ashlar 115 in `rear.png`. Nothing of the stone, timber or plaster treatment is readable; the only visible elements are three clipped window panels and a lantern cube. LEVEL, fix 3.
2. A cobbler's banner sign (`ShopSign_GildedTankard`, board `MI_SB_PropShopSignBoard_Boots`, a pair of boots) hangs beside the door at (1360-1450, 300-370) under the baked tankard sign. §5: a package that bakes its sign never also gets the dressing sign. `silverbrook_props.py` line 431 `or folder!='Districts/'` forces one on every dev-map building. CODE, fix 2.
3. The right-hand lantern at (1690-1715, 325-360) is a 25 x 35 px pure-yellow rectangle (255,236,140) with no visible cage, cap or cone; the concept lantern is an iron cage with a conical roof and a warm amber glass. CODE (kit), fix 5.
4. The tankard emblem (820-1000, 60-300 in the 3x crop) reads as a flat olive-green cut-out (about (170,170,110)) on the black shield, not gilt: metallic 0.85 in shade reflects only sky. Re-judge after fix 3; conditional fix 8.

## foundation-close — FAIL

1. A stone apron runs along the entire front: the plinth's front face stands 0.6-0.9 m in front of the ashlar (dark course under the barrels and ladder foot at (500-1520, 600-800), the doorstep sitting on top of it, the bench on it at right). The concept has barrels and bench on the street at the wall foot. Cause: `_base_box` takes the wall base from the lowest-band vertices, and the two baked barrels (24-segment rings at z 0) plus the bench legs outweigh the wall corners in `_edge(mass=.08)`, so y1 becomes the barrel front. `check_foundations.py` recomputes the same edge and reports `proud_cm 4.0 4.0`, so the gate is blind to it. CODE (shared), fix 1.
2. Same shade as frontage: ashlar luminance 17; the stone coursing is invisible. LEVEL, fix 3.
3. Left lantern at (1375-1395, 60-100) is again a bare glowing box with a 2 px bracket. Fix 5.
4. The boots banner's lower edge is in frame at (1330-1420, 40-120). Fix 2.

## three-quarter — FAIL

1. Two signs on one building: the baked tankard bracket between the upper windows (1300-1360, 300-380) and the boots banner at the door (1270-1300, 430-480). Fix 2.
2. The front apron ends in a stone block at the front-right corner (1500-1620, 580-640) carrying the ivy clump; the ivy was placed at `(x1+10, y1+10)` of the inflated base box, so it sits on the apron 0.8 m in front of the wall corner instead of at the plinth corner. Fix 1 moves both.
3. Lantern cube at (1405-1425, 460-480). Fix 5.
4. Windows are clipped: in `rear.png` 25 % of the window area (560-760, 180-440) is above 600 summed RGB with mean (252,236,179); the lead cames read as thin lines over a flat pale fill, where the concept shows dark amber panes with visible leading. `SB_LitGlass` emissive strength 3 (`build_gilded_tankard.py` line 28) under the fixed-exposure capture. ART (shared), fix 6.
5. Massing matches: 42-degree gable, centred dormer below the ridge, chimney cap 0.5 m over the ridge with its pot (r5 fix 1 is in), jetty on corbels, arched double door with straps and rings, two barrels left, bench right. Plaster and timber are lighter than the concept's grimy cream and near-black oak (`SB_OakTimber dark=(.32,.30,.28)` renders mid-brown) but the design is recognisable here once the sun lights it; not a fail driver.

## rear — PASS

Two shuttered leaded lights, sill and plate rails, plaster flush on the ashlar, no jetty, the roof verge and fascia clean, plinth flush with the wall base (no apron on this side, `rear-base` crop). Window clipping as three-quarter item 4 (fix 6). `README` reports `washing_lines 1`, but no washing line is visible 2.3 m behind the rear wall in this view, in `context-overview-b` or in `context-ground-3`; verify item 7.

## sign-close — FAIL

1. The view frames the dressing sign, not the building's sign: `capture_building.py` targets `ShopSign_<label>` and the pass hung the boots banner. The picture a player gets of "the sign of the Gilded Tankard" is a pair of boots on a blue banner (560-690, 330-620). Fix 2 removes the banner; fix 4 points the view at the baked board, which otherwise falls back to (0, d/2, 2.6 m) — the door arch.
2. Lantern at (380-480, 370-480): 100 x 110 px flat yellow box, cage bars invisible, the cap a 10 px dark line. Fix 5.
3. Everything is in shade (ashlar luminance about 20). Fix 3.

## context-overview-a — FAIL

1. The apron block at the front-right corner under the ivy (1500-1600, 500-540) and the dark apron strip along the front (barrels and bench standing on it). Fix 1.
2. Two signs (tankard bracket at (1380-1410, 310-350); boots banner at the door (1340-1360, 380-410)). Fix 2.
3. The ladder leans on the left gable at mid depth and the L-fence garden sits at the right gable: a tavern with a herb plot and a ladder is plausible; but the ten crop sprigs straddle the fence line (five inside the L, five outside it at (1550-1640, 480-530)), so the plot reads as weeds through a fence. Shared rule, fix 9.

## context-overview-b — PASS

Rear-left from 18 m: slate roof with individual tiles, ridge cap, chimney with pot, dormer peak just visible beyond the ridge (0.5 m below it; expected from this height), plinth flush on the rear and left. The sunlit slate reads light warm grey (tone variant on `SB_Slate dark=(.20,.22,.28)`), darker than the concept's blue-grey only in the shaded front slope; nothing broken.

## context-ground-1 — FAIL

1. Right gable from 20 m: the boots banner hangs off the front corner at (1270-1310, 380-420) beside the baked sign edge-on at (1295-1305, 280-330). Fix 2.
2. Fence L with sprigs on both sides of the rail line at (1400-1560, 500-560). Fix 9.
3. The gable itself is right: chimney against the gable from the ground, plaster triangle, one shuttered light, stone below.

## context-ground-2 — FAIL

1. Front-left looking along the street face: the boots banner at (1660-1720, 390-440) and, behind the ladder, the barrels standing on the apron course (1650-1690, 500-560). Fixes 2, 1.
2. Baked tankard sign edge-on at (1640-1670, 280-340) is correct in position and bracket shape.

## context-ground-3 — PASS

Rear and right gable from 24 m. Chimney full height on the gable, two rear windows, plinth flush, the ladder's rail visible past the far corner. The fence L stands in the open field at (1170-1230, 480-510) with sprigs along it (fix 9, shared, not grounds to fail this view on its own).

## context-ground-4 — FAIL

1. The apron's end block at the front corner of the gable (640-830, 290-370 in the 2x crop; raw about (570-660, 795-835)) stands 0.8 m proud of the wall line beside the fence. Fix 1.
2. Sprigs on both sides of the fence line (raw (470-860, 830-860)). Fix 9.

## Fix list (shared first; each executable without this review)

1. **CODE, shared, Engineer** — `GameClient/ArtSource/Silverbrook/silverbrook_props.py`, `_base_box` (line 526): a base vertex counts only when the mesh rises above it, so baked barrels, benches, tubs and steps never become the footprint edge. Replace
   `low=min(p.z for p in pts);near=[p for p in pts if p.z<=low+band]`
   with
   ```python
   low=min(p.z for p in pts);near=[p for p in pts if p.z<=low+band]
   # ponytail: a base vertex counts only with wall above it (>1.3 m within 20 cm); baked barrels, benches, tubs and steps stay out of the footprint
   cells={(round(p.x/20),round(p.y/20)) for p in pts if p.z>low+130}
   near=[p for p in near if any((round(p.x/20)+i,round(p.y/20)+j) in cells for i in (-1,0,1) for j in (-1,0,1))]
   ```
   Then make the gate use the same function: in `tools/check_foundations.py` replace the two lines
   `X=sorted(q.x for q in near);Y=sorted(q.y for q in near)` and `bx0,bx1,by0,by1=sp._edge(X,True)*hs.x,...` with
   `bx0,bx1,by0,by1=[v*s for v,s in zip(sp._base_box(mesh,25),(hs.x,hs.x,hs.y,hs.y))]` (delete the now-unused `dm`/`pts`/`low`/`near` lines above them). Other buildings: wall corners always have top vertices above them, so their boxes are unchanged unless they also have low baked props, which is the same defect. Re-run `silverbrook_props.py --folder Districts/Dev --terrain Terrain` and `check_foundations.py --expect 1`; expected: the plinth front flush with the ashlar (+4 cm), barrels and bench on the floor, ivy at the wall corner.

2. **CODE, shared, Codex** — `silverbrook_props.py` line 431: never hang `ShopSign_` on a mesh that bakes its sign. Replace
   `if _h(label+'wall')<.4 or folder!='Districts/':  # a dev-map review always has a sign view`
   with
   ```python
   baked_sign=any('SignBoard' in str(s.material_slot_name) for s in mesh.static_materials)  # buildings.md §5: never both signs on one building
   if _h(label+'wall')<.4 or folder!='Districts/':
   ```
   and wrap only the sign spawn: change `s_=spawn('ShopSign_'+label,...)` / `s_.static_mesh_component.set_material(...)` / `counts['signs']+=1` into `if not baked_sign:` + those three lines (ladder and ivy stay). Town effect: every package whose mesh carries a `*SignBoard` slot (the civic packages that bake a sign) loses its second dressing sign where `_h(label+'wall')<.4` hit it, which is the §5 rule they also break; houses unchanged. Dev map: delete `ShopSign_GildedTankard` by re-running the pass (`silverbrook_props.py` clears `TAG` actors first; if it does not, delete the actor in the same run).

3. **LEVEL, Codex** — `tools/Create-BuildingDevMap.py`: light the street face in the review map. After `lighting.apply()` add
   `lighting._one(u.DirectionalLight).set_actor_rotation(u.Rotator(roll=0,pitch=lighting.load_config()['sun.pitch_deg'],yaw=-135),False)  # sun on the +X and +Y faces: frontage, three-quarter and sign views are lit; town yaw 135 leaves the street face in shade`
   and in `docs/blueprints/buildings.md` §6 step 2 append "sun yaw -135 in the dev map (the town's 135 lights only the rear)". The existing map was created with the old script and the tool refuses overwrites: delete `/Game/Frontier/Maps/Dev/GildedTankard/` (`Content/Frontier/Maps/Dev/GildedTankard/`) and rerun README steps 5-9. Acceptance: front plaster luminance >= 90 in `frontage.png`.

4. **CODE, Codex** — baked-sign `sign-close` target. In `build_gilded_tankard.py` add to the `manifest=dict(...)` call `sign_m=[-(lo[0]+hi[0])/2, SY-(lo[1]+hi[1])/2, (4.05+5.2)/2]` (board centre in the exported frame; the Blender export recentres x,y by -(lo+hi)/2 at line 170, so apply the same offset). In `tools/capture_building.py` replace
   `sign = signs[0].get_actor_location() if signs else u.Vector(origin.x, origin.y+d*50, origin.z+260)`
   with
   ```python
   import json
   sm = json.loads((Path(__file__).resolve().parents[1]/'GameClient/ArtSource/Silverbrook'/name/'asset-manifest.json').read_text()).get('sign_m')
   sign = signs[0].get_actor_location() if signs else (origin+u.Vector(sm[0]*100, -sm[1]*100, sm[2]*100) if sm else u.Vector(origin.x, origin.y+d*50, origin.z+260))
   ```
   (the FBX import flips Y, so the Blender -Y front becomes +Y). Rebuild the package (`blender.exe -b --factory-startup -P build_gilded_tankard.py`) so the manifest carries `sign_m`; the mesh is unchanged. Add `sign_m` to §5 of `buildings.md` as a required manifest key for packages that bake a sign.

5. **CODE (kit), shared, Engineer** — `GameClient/ArtSource/Silverbrook/silverbrook_kit.py` `lantern()` lines 345-350: make the cage read and stop the glass clipping. Replace lines 345-350 with
   ```python
   body=top-UP*.27
   G['detail'].box(body,(.12,.12,.22),M['lamp'],0)                                   # glass inset 1.5 cm behind the bars
   for x in (-1,1):
       for y in (-1,1):G['detail'].box(body+Vector((x*.075,y*.075,0)),(.035,.035,.26),M['iron'],0)   # 3.5 cm corner bars (3-4 px at 7 m)
   for dz,s in ((-.14,.22),(.14,.22)):G['detail'].box(body+UP*dz,(s,s,.035),M['iron'],.004)
   G['detail'].prism([body+Vector((-.12,0,.155)),body+Vector((.12,0,.155)),body+Vector((0,0,.30))],(0,1,0),.24,M['iron'])   # cone roof
   ```
   and in every package `M` dict (`build_gilded_tankard.py` line 30 and every other `build_*.py` that calls `lantern(` — `rg -l "lantern\(" GameClient/ArtSource/Silverbrook/*/build_*.py`) change `kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5)` to strength `1.5`. All lantern callers share this function and material and show the same bare cube, so this changes them at their next rebuild; it is the rule they also break. Acceptance: in `sign-close.png` the lantern shows four dark bars and a cone, glass mean below (240,200,120).

6. **ART, shared, Codex** — window clipping: in every package `M` dict change `kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3)` strength `3` to `1.2`; rebuild and re-import this package (`import_silverbrook.py --package-only GildedTankard` rewrites the shared `MI_SB_LitGlass`; commit that resave this time instead of reverting it). Acceptance: `rear.png` window region (560-760, 180-440) has < 2 % of pixels above summed RGB 600 and the lead cames are continuous dark lines. Every lit building shares the material and clips the same way; re-capture the main-street bookmark after to confirm it still passes.

7. **Verify, Engineer** — `HOUSE_DETAILS` reports `washing_lines 1` but no washing line is visible in `rear.png`, `context-overview-b.png` or `context-ground-3.png`, where the rear-yard spot (`-gside*100, y0-230`) should put a 3.7 m line on two posts 2.3 m behind the rear wall. In the editor print `WashingLine_GildedTankard` location, scale, `hidden`/`hidden_in_game` and world bounds; fix whatever puts it out of view (below the floor, zero scale, hidden) and extend `check_details.py` to fail when a counted detail prop's bounds centre is below the terrain or the actor is hidden in game, so a counted-but-invisible prop fails the run.

8. **ART, Codex (conditional on fix 3)** — after the recapture, if the gilt tankard still reads as a flat olive shape in `frontage.png`, change `build_gilded_tankard.py` line 33 `kit.solid('SB_GildedTankard_Gilt',(.62,.45,.18),.4,.85)` to `(.72,.52,.18),.3,.7` (brighter, less mirror-like, so sky reflection no longer greys it) and rebuild/re-import.

9. **CODE, shared, Codex** — `silverbrook_props.py` line 405: keep the crops inside the fence L. Replace `cx,cy=pt(sx+side*(55+(i%2)*85),(i//2-2)*60)` with `cx,cy=pt(sx+side*(55+(i%2)*85),25+(i//2)*25)` (the two sections run along y 0 and the return spans y 0..2*run on the +Y side, so y 25..125 cm is inside the plot). Every town garden currently straddles its fence; same rule.

10. **Recapture, Codex** — after 1-6 and 9: delete the dev map folder, rerun README steps 5-9 (`Create-BuildingDevMap.py`, `silverbrook_props.py --folder Districts/Dev --terrain Terrain`, the four gates, `capture_building.py`, `make_pairs.py`), then a fresh reviewer judges the eleven pairs again. Expected gate lines: `FOUNDATION_CHECK 1 failures 0 ... proud_cm 4.0 4.0` with the plinth now flush with the ashlar, `HOUSE_DETAILS ... signs 0`.
