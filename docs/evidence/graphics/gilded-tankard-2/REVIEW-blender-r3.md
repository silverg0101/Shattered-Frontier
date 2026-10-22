# The Gilded Tankard — Blender stage review r3

Judge: independent art director (did not build this). Inputs: concept `concept/GildedTankard.png` (front elevation left, 3/4 right); renders `GildedTankard-Beauty/Front/Side/Rear.png` (1100 px; ortho 71.4 px/m, camera z 6.6, ground at y≈1021); `build_gilded_tankard.py` read for exact values; `silverbrook_kit.py` for helper conventions; `docs/blueprints/buildings.md` §3/§5. Concept measured at 120 px/m (door head 2.2 m): building 6.5 m wide, stone 2.75, upper 2.3, roof 1.4; model 10.0 (eaves) x 3.7 / 3.7 / 3.6.

Result: 4 PASS / 1 FAIL. The four captures read as the concept (gable roof, jetty on corbels, centred dormer, right-gable chimney, two shuttered leaded windows, hanging tankard board, arched double door, lanterns, barrels, bench). The model fails overall: the chimney stack is placed on the front half of the right gable and swallows the right-side window's jamb, sill and left shutter, a clash that none of the four cameras can see. The right side must be captured next round.

## Beauty — PASS

1. Front corners have no corner post: front posts stop at x=±4.25 (0.35 m short of the corner), side posts at y=±3.3 (0.3 m short). The concept 3/4 shows a corner post over the corbel at the front-left corner. CODE, fix 3.
2. Sign bracket is a plain L with a strut; the concept's bracket is a scrolled iron arm, and §5 asks for an 8-segment scroll r 0.17 under the arm and a 6x10x100 cm wall plate (model plate is a 12 cm cube). CODE, fix 2.
3. Strut foot floats: `beam((0,-D2-.06,5.4),...)` ends 6 cm outside the plaster face at x=0 where there is no post (hidden behind the post silhouette in Side, open in 3D). CODE, fix 2.
4. Board bottom (z 4.0) sits exactly on the jetty beam top (4.0): in elevation the tip reads as resting on the beam. CODE, fix 2 (bottom 4.05, board shortened to spec).

## Front — PASS

1. Arch crowds the jetty: voussoir extrados top = STEP+DS+DWID/2+.30 = 3.60 m, jetty soffit bottom = STONE_TOP-.06 = 3.64 m, a 4 cm band (y 757-770 px). Concept keeps 0.25 m of stone above the arch on a 2.75 m storey (9 %); model is 1 %. CODE, fix 4 (DS 1.5 gives 0.34 m, 9 %).
2. Sign board is 1.04 x 1.65 m (x 513-587, y 618-736 px); §5 caps the board at 0.8 x 1.15 m and puts the arm at 3.45-3.75 m. Proportionally the board matches the concept (0.10 x 0.17 of width), so the size is a spec violation, not a recognition one; the arm stays at upper-window mid-height as drawn in the concept. CODE, fix 2.
3. Lantern wall plates float 5 cm: `lantern()` centres its 8 cm plate on the arm base, base is at -DF-.09, ashlar face at -DF. Same convention as Townhouse1/Blacksmith; package-local fix. CODE, fix 5.
4. Chimney reads as a stepped stack (full 0.9 m above z 9.6, a 0.45 m strip in shadow below) because the 0.4 m gable overhang passes in front of it. The concept front elevation also has the stack inside the roof outline, so this is accepted; it changes anyway with fix 1.
5. Proportions: eave at 67 % of total height vs concept 79 % (roof 0.36 of width vs 0.22). The spec floor (D2 >= 0.6 x 12/2 = 3.6, pitch >= 40 deg) forces a 3.36 m rise minimum; the 3.6 m built is within 7 % of that floor. Accepted, no fix.

## Side — PASS

Left gable, inferred. Framing, one shuttered window at y=0, flush stone storey, front jetty step with beam end and soffit, sign and lanterns in profile, two barrels: consistent with Front and Beauty. Dormer ridge (z 10.48, y -2.98..-0.58) shows above the gable line from |y|>0.58 and its cheek from |y|>1.41: correct for a gabled dormer on a 42 deg slope. Gable prisms are flush on both sides (`s*(W2-.15)`, 0.3 thick, centred). Ridge-cap end faces show as a 0.32 m tab at the apex: accepted.

No fix from this view. The right side (chimney side) is not captured by any view; fix 6 adds it.

## Rear — PASS

Inferred; judged for consistency and breakage. Two shuttered windows at ±2.6, posts at ±1.1/±4.25, sill and top rails, rear eave fascia, chimney at the left (= +x), no dormer, jetty beam stub (0.2 m past the gable) and side posts visible at the right edge: all consistent. Nothing floats or clips.

Capture: `review_gilded_tankard.py` raises the world strength to .9 for Rear but the sun (yaw -35 deg) still leaves the whole wall in shadow; the ashlar reads blue-black until brightened 2.6x. CODE, fix 6 (flip the sun yaw for Rear and SideR).

## Concept-to-model — FAIL

| Element | Concept | Model | Verdict |
|---|---|---|---|
| Roof | gable, ridge full width, dormer centred | gable, 42 deg, dormer at y -2.8 | OK |
| Jetty | ~0.5 m on corbels, front only | 0.45 m, 4 corbels, soffit | OK |
| Chimney | 3/4 view: stack on the right gable at the REAR corner; right wall window on the front half | stack at YC=-1.0 (y -1.55..-0.45, x 4.59..5.49); right window at y=0 with shutters to |y| 1.345 | CLASH: window jamb (y -.665..-.575, x to 4.64), sill (to y -.725, x to 4.72) and the whole -y shutter (y -1.345..-.68, x 4.61..4.65) are inside the stone. Fix 1 |
| Framing | corner posts, posts flanking the sign | posts at ±1.1 and ±4.25, sides ±1.5/±3.3; no corner posts | Fix 3 |
| Door | double, straps, two rings, stone band above the arch | 2 straps per leaf, 2 rings at z 1.25; 4 cm band | Fix 4 |
| Sign | scroll bracket, shield, gilt tankard | L arm + strut, shield 1.04x1.65, tankard reads | Fix 2 |
| Windows | 2 front shuttered, 1 each side | same | OK |
| Lanterns | either side of the door | z 2.55, plates 5 cm off the wall | Fix 5 |
| Barrels / bench | 2 left, bench right | same; barrels 0.873 m apart (no overlap) | OK |
| Materials | — | kit scans and solids, no defaults, no z-fighting seen | OK |

## Fix list

All CODE fixes are in `GameClient/ArtSource/Silverbrook/GildedTankard/build_gilded_tankard.py` unless stated; line numbers refer to the current file. Rebuild, run `review_gilded_tankard.py`, and submit five views (Beauty, Front, Side, SideR, Rear).

1. **Chimney to the rear of the right gable** (clears the right-side window; matches the 3/4 view). Codex.
   Line 51: `XC,YC=W2+.44,2.2                # chimney stack: full height against the right gable wall, rear half (clear of the side window, |y| <= 1.35)`
   Line 71: covered list `[in_dormer]` (was `[in_dormer,in_stack]`).
   Line 72: covered list `[in_stack]` (was `[]`).
   Check: stack y 1.65..2.75; side post at yy=1.5 ends at y 1.6 (5 cm clear), post at 3.3/3.5 clear; side rails pass through the stone (hidden); rear wall ends at y 3.6; ridge caps at y 0 clear.

2. **Sign to §5: scroll bracket, 0.8 x 1.15 board, arm at window mid-height, strut and arm embedded.** Codex.
   Replace lines 141-151 with:
   ```python
   AZ=5.45   # arm: upper-window mid-height, as drawn; plate 6x10x100, scroll r .17 (Golden Stag Inn lines 219-225)
   G['detail'].box((0,-D2-.03,AZ-.4),(.1,.06,1.0),M['iron'],.004)
   G['detail'].beam((0,-D2+.05,AZ),(0,SY-.02,AZ),.05,.05,M['iron'],grain=False)
   G['detail'].beam((0,-D2+.05,AZ-.85),(0,-D2-.95,AZ-.02),.035,.035,M['iron'],grain=False)
   for k in range(8):   # scroll under the arm, in the arm's vertical plane
       t0,t1=math.tau*k/8,math.tau*(k+1)/8;c=Vector((0,-D2-.5,AZ-.2));r=.17
       G['detail'].beam(c+Vector((0,r*math.cos(t0),r*math.sin(t0))),c+Vector((0,r*math.cos(t1),r*math.sin(t1))),.02,.02,M['iron'],grain=False)
   G['detail'].beam((-.32,SY,AZ),(.32,SY,AZ),.03,.03,M['iron'],grain=False)
   for x in (-.32,.32):G['detail'].beam((x,SY,AZ-.03),(x,SY,5.19),.02,.02,M['iron'],grain=False)
   board=[(-.4,5.2),(.4,5.2),(.4,4.45),(0,4.05),(-.4,4.45)]   # 0.8 x 1.15 m shield, bottom 3.85 m above the step
   G['detail'].prism([(x,SY,z) for x,z in board],(0,1,0),.05,M['sign'])
   body=[(-.17,4.35),(.09,4.35),(.09,5.05),(-.17,5.05)]   # tankard: 0.26 x 0.70 body, open D handle abutting at x .09
   handle=[(.09,4.47),(.18,4.47),(.27,4.57),(.27,4.85),(.18,4.95),(.09,4.95),(.09,4.89),(.15,4.89),(.21,4.83),(.21,4.59),(.15,4.53),(.09,4.53)]
   for poly in (body,handle):
       for dy in (-.035,.035):G['detail'].prism([(x,SY+dy,z) for x,z in poly],(0,1,0),.02,M['gilt'])
   ```
   Check: plate z 4.55..5.55 clears the sill rail (4.37..4.49); strut foot at y -3.55 is inside the plaster, passes over the rail (z 4.73 at y -3.705); scroll rests on the strut (ring bottom 5.07, strut top 5.08 at y -4.1) and touches the arm; emblem inside the shield (half-width 0.3 at z 4.35, body x -.17..0.09); arm 1.2 m, posts at ±1.1 clear.

3. **Corner posts and corner corbels.** Codex.
   Line 114: `for x in (-4.5,-1.1,1.1,4.5):post(x,y,STONE_TOP,PLATE-.1,True)`
   Line 115: `for x in (-4.5,-1.1,1.1,4.5):post(x,D2+.06,STONE_TOP,PLATE-.1,True)   # rear posts`
   Line 117: `for yy in (-3.5,-1.5,1.5,3.5):post(s*(W2+.06),yy,STONE_TOP,PLATE-.1,False)   # side posts, clear of the side shutters (|y| <= 1.35)`
   Line 119: `for x in (-4.5,-2.6,2.6,4.5):G['timber'].box((x,-D2+.1,STONE_TOP-.22),(.2,.7,.44),M['timber'],.012,grain=True,band=band())`
   Check: front post x 4.4..4.6 and side post y -3.6..-3.4 meet at the corner (5 mm shared volume, hidden); front shutters reach |x| 3.945; corbel at x 4.4..4.6 ends at the side stone wall face y -3.15.

4. **Stone band above the arch.** Codex.
   Line 52: `XDOOR,DWID,DS,STEP=0.0,2.6,1.5,.2   # double door: width, spring above the step (head 3.2 m, 0.34 m of ashlar under the jetty), step top`
   Straps (`STEP+DS-.2`), opening tuple and `door()` follow automatically.

5. **Lantern plates flush with the ashlar.** Codex.
   Line 157: `for x in (-2.3,2.3):lantern(G,M,(x,-DF-.42,2.55),arm=((x,-DF-.03,2.55),(0,-1,0)))`

6. **Capture: right-side view and lit rear.** Codex.
   Line 180: append `;cam('SideR',(40,0,6.6),(90,0,90),15.4)`.
   `review_gilded_tankard.py` line 5: `import bpy,json,math`; line 9: `for name in ['Beauty','Front','Side','SideR','Rear']:`; add inside the loop before the render: `bpy.data.objects['Sun'].rotation_euler[2]=math.radians(145 if name in ('Rear','SideR') else -35)`.
   Next review judges `GildedTankard-SideR.png` for the chimney/window relationship (window at y 0 with both shutters open, stack behind it at y 1.65..2.75).
