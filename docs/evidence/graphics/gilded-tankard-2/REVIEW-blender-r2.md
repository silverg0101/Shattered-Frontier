# The Gilded Tankard — Blender stage review r2

Judge: independent art director (did not build this). Inputs: concept `docs/Game-Design/concept-art/Silverbrook/The Gilded Tankard.png`; renders `GildedTankard-Beauty/Front/Side/Rear.png` (1100 px); script `build_gilded_tankard.py` read for exact values. Ortho views are 75.3 px/m (ortho_scale 14.6 over 1100 px); ground sits at y≈1077.

Result: 0 PASS / 5 FAIL. The model is a hipped-roof house with a sign; the concept is a gabled, jettied, dormered alehouse. The roof form alone makes every view fail the "same design" test.

Distinct from an ordinary house? Partly. The wide arched double door, hanging board, lanterns, barrels and bench say "inn", but the gilt emblem reads as a blob (not a tankard) and the jetty/dormer that give the concept its silhouette are missing. After fixes 1-4 it will read as the Tankard; before them it reads as "generic stone-and-plaster house with a sign".

## Beauty — FAIL

1. Roof is hipped (four slopes; left end is a slate triangle). Concept 3/4 view is a gable roof: ridge runs the full width, vertical gable ends, chimney stack on the right gable wall. CODE, fix 1.
2. No jetty. The concept upper storey oversails the stone by ~0.5 m on a corbelled beam; here the plaster is flush with the ashlar and the corbels at the storey line hold up nothing. CODE, fix 2.
3. Left wall (side) upper storey is bare plaster: no corner posts, no mid post, no sill rail. Concept 3/4 shows framing on the side wall too. CODE, fix 3.
4. No front dormer. Concept has a small gabled dormer with a lit leaded light centred on the front slope. CODE, fix 4.
5. Chimney is a thin stack on the rear slope with two pots; concept is a full-height stone stack on the right gable wall, one round pot. CODE, fix 5.
6. Sign emblem is an ochre polygon ~0.5 x 1.0 m with a solid bump; it does not read as a tankard at any distance. CODE, fix 6.
7. Barrels interpenetrate: centres (-3.0,-D2-.8) and (-2.25,-D2-.6) are 0.776 m apart, max diameter 0.80 m, so they intersect by 2.4 cm at the bilge. CODE, fix 7.

## Front — FAIL

1. Roof silhouette is a trapezoid: eaves 170-935 px (10.2 m), ridge 290-810 px (6.9 m). Concept front elevation is a full-width rectangle (ridge as wide as the eaves). CODE, fix 1.
2. Chimney at x 810-885 px sits inside the roof outline on the right-rear; concept chimney rises at the right edge, outside the wall line, from ground to above the ridge. CODE, fix 5.
3. Emblem at 540-580 x 670-750 px is a notched block. Concept: cylinder body with an open D handle. CODE, fix 6.
4. Door has no strap hinges and one 7 cm ring. Concept: two long straps per leaf and a pair of rings at mid height. The kit's ledges are 1.2 cm deep and invisible at this scale. CODE, fix 8.
5. No dormer on the front slope. CODE, fix 4.
6. Storey line: beam present, corbels present, but no overhang (plaster face and ashlar face both at y=-D2). CODE, fix 2.
7. Framing: posts at ±1.1 and ±4.25 m only; concept also carries a mid-rail at window-head height. Minor; covered by fix 3's rails.
8. Framing (capture): ground line touches the bottom edge (step top at y≈1062, ground 1077). Not broken, but leave margin. CODE, fix 9.

## Side — FAIL

1. The whole triangle above the eaves (220-820 px wide, apex y=245) is slate. Concept: a plaster gable with the roof edge as a barge board; stone chimney standing against this wall. CODE, fixes 1 and 5.
2. Slate on the hip triangle is tiled at a different scale and direction from the front slope (tiles ~2x larger, courses reading as vertical planks). Disappears with fix 1; if the hip is kept, re-run tile_plane on the side planes with the same TILE and toward vector convention as the front.
3. Upper storey is a blank plaster panel 540 x 250 px around one window: no posts, no sill rail, no eaves corbels. CODE, fix 3.
4. No jetty step at the storey line (stone and plaster flush at x=±W2). Front-only jetty is the smallest acceptable fix (fix 2); a side jetty is not required.
5. Chimney stack rises from behind the roof at x 375-440 px (rear slope); concept stack is on this gable wall. CODE, fix 5.
6. Right-hand barrel is cut by the frame edge (x 810-880, y 990-1077). Capture only; fix 9.

## Rear — FAIL

Inferred view, judged only for consistency and breakage.

1. Hip at the right (lighter tile strip x 800-930 px) contradicts the gable concept. CODE, fix 1.
2. Upper storey has no framing: two windows in blank plaster, one eaves rail. CODE, fix 3.
3. Render is in full shadow (sun at -35° yaw lights the front/left); the ashlar reads blue-black and cannot be judged. Capture: add a fill or rotate the sun for the Rear camera. Fix 9.
4. Chimney at x 215-295 px on the rear-left. Moves with fix 5.

## Concept-to-model — FAIL

| Element | Concept | Model | Verdict |
|---|---|---|---|
| Roof | Gable, ridge full width, ~42° | Hip, ridge 6.8 m of 10 m | Wrong form |
| Jetty | ~0.5 m oversail on corbels | Flush, decorative corbels | Missing |
| Dormer | 1 small gabled, lit, centred | None | Missing |
| Chimney | Full-height stone stack on right gable, 1 pot | Short stack on rear slope, 2 pots | Wrong place |
| Framing | Posts, rails, braces all walls | Front posts only | Incomplete |
| Door | Double, 4 straps, 2 rings | Single leaf, no straps, 1 ring | Under-detailed |
| Sign | Scroll bracket, shield, gilt tankard | L bracket, shield, blob | Emblem unreadable |
| Windows | 2 shuttered leaded front, 1 side | Same | OK |
| Lanterns | 2 at arch springing | 2 at z 2.55 | OK |
| Barrels / bench | 2 left, bench right | Same, barrels intersect, no hoops | Fix 7; hoops accepted |
| Proportions | stone : upper : roof ≈ 0.43 : 0.41 : 0.25 of width | 0.40 : 0.43 : 0.39 | Roof tall; acceptable at 42° once gabled |
| Materials | — | Scans, no defaults | OK |

Nothing floats, no placeholder materials, no visible z-fighting. The failures are form and missing elements, not breakage.

## Fix list

All CODE fixes are in `GameClient/ArtSource/Silverbrook/GildedTankard/build_gilded_tankard.py` (module level, no functions except where named). Line numbers refer to the current file.

1. Gable roof. Line 46: `EX,EY,RX=W2+.4,D2+.4,W2+.4`. Line 47: `KF,KS=(RIDGE-EAVE)/EY,1e9`. Delete lines 67-68 (the two side planes). Replace lines 74-75 (hip caps) with barge boards: `for sx in (-1,1):\n    for sy in (-1,1):G['timber'].beam((sx*(EX+.02),sy*EY,EAVE-.07),(sx*(EX+.02),0,RIDGE-.07),.05,.24,M['timber'],band=band())`. Delete line 78 (horizontal side eave beams). Line 89: change `s*(W2-.15)` to `s*W2-(.3 if s>0 else 0)` so both gable prisms sit flush inside the wall face (currently one is 0.15 m proud, the other 0.15 m recessed).
2. Front jetty. After line 48 add `JET=.45;DF=D2-JET`. Line 81: `(0,-DF,0)`. Line 86: `wall(G['stone'],M['stone'],(s*W2,JET/2,0),(s,0,0),D2-JET/2,STONE_TOP,[])`. Line 100: `G['timber'].box((x,-D2+.1,STONE_TOP-.22),(.2,.7,.44),...)` (corbels now bridge stone face to beam). After line 100 add the soffit: `G['timber'].box((0,-D2+JET/2,STONE_TOP-.03),(2*W2+.4,JET,.06),M['timber'],.01,grain=True,band=band())`. Line 109: `(XDOOR,-DF,STEP)`. Line 110: `-DF-.225`. Line 122: `BY=-DF-.55`. Line 125: `(-3.05,-DF-.8),(-2.2,-DF-.6)`. Line 126: `(x,-DF-.42,2.55),arm=((x,-DF-.09,2.55),(0,-1,0))`.
3. Framing on all walls. After line 98 add: `for x in (-4.25,-1.1,1.1,4.25):post(x,D2+.06,STONE_TOP,PLATE-.1,True)` and `for s in (-1,1):\n    for yy in (-3.3,-1.2,1.2,3.3):post(s*(W2+.06),yy,STONE_TOP,PLATE-.1,False)`. After line 104 add sill rails: `rail((-W2,D2+.04,WZ-.17),(W2,D2+.04,WZ-.17),.12)` and `for s in (-1,1):rail((s*(W2+.04),-D2,WZ-.17),(s*(W2+.04),D2,WZ-.17),.12)`.
4. Front dormer. After line 62 add `DY0=-EY+1.2;DZ0=EAVE+1.2*KF;in_dormer=lambda p:abs(p.x)<.74 and DY0-.05<p.y<DY0+1.3`. Line 65: covered list `[in_dormer]`. After line 75 add:
   `G['plaster'].box((0,DY0+.65,DZ0+.9),(1.5,1.3,1.8),M['plaster'],.01)`
   `G['plaster'].prism([(-.85,DY0,DZ0+1.75),(.85,DY0,DZ0+1.75),(0,DY0,DZ0+2.35)],(0,1,0),1.3,M['plaster'])`
   `plane((-.85,DY0+1.4,DZ0+1.75),(0,-1,0),(.85,0,.6),math.hypot(.85,.6),lambda t:0,lambda t:1.5,[])`
   `plane((.85,DY0-.1,DZ0+1.75),(0,1,0),(-.85,0,.6),math.hypot(.85,.6),lambda t:0,lambda t:1.5,[])`
   `capped((0,DY0-.1,DZ0+2.41),(0,DY0+1.4,DZ0+2.41),3)`
   `window(G,M,(0,DY0,DZ0+.35),(0,-1,0),.8,1.0,shutters=False,wall_th=.2,band=band())`
   Target: dormer 1.5 m wide, front face 1.2 m up the slope, ridge 2.35 m above its base (buried into the main slope at the back).
5. Chimney on the right gable. Line 50: `XC,YC=W2+.45,-1.0`. Line 91: `G['stone'].box((XC,YC,6.6),(.9,1.1,13.2),M['stone'],.01)`. Line 92: z `13.25`. Line 93: `for dx in (0,):` and z `13.3`. Line 65 covered list becomes `[in_dormer,in_stack]`; line 66 covered list becomes `[]`. (Top of pot 13.77 m < slot 14; width 11.0 m < 14.)
6. Tankard emblem. Replace lines 119-120 with:
   `body=[(-.24,4.35),(.12,4.35),(.12,5.32),(-.24,5.32)]`
   `handle=[(.08,4.52),(.24,4.52),(.36,4.66),(.36,5.04),(.24,5.18),(.08,5.18),(.08,5.10),(.20,5.10),(.28,5.02),(.28,4.68),(.20,4.60),(.08,4.60)]`
   `for poly in (body,handle):\n    for dy in (-.035,.035):G['detail'].prism([(x,SY+dy,z) for x,z in poly],(0,1,0),.02,M['gilt'])`
   Result: 0.36 m body, open 0.08 m-thick D handle; still flat, as accepted.
7. Barrel overlap. Line 125 centres `(-3.05,...)` and `(-2.2,...)` (included in fix 2's line edit); separation becomes 0.873 m > 0.80 m diameter.
8. Door ironwork. After line 109 add: `DL=G[f"{G['detail'].name}_Door1"]` then `for s in (-1,1):\n    for z in (STEP+.6,STEP+DS-.2):DL.box((s*(DWID/2-.55),-DF+.155,z),(1.0,.01,.12),M['iron'],.004)` and a second ring mirrored: `DL.box((-DWID/2*.55,-DF+.155,1.06),(.07,.01,.07),M['iron'],.004)`. Straps live on the leaf mesh so they travel with the door part. (If fix 2 is not applied, use `-D2+.155`.)
9. Capture. Line 149: ortho cameras z `6.6`, ortho_scale `15.4` (ground margin, barrels uncut). Line 143: for the Rear render raise `bg.inputs['Strength']` to `.9` or add a second sun at yaw 145° with energy 2.0 so the rear ashlar is readable.

Re-render all four views after fixes 1-8 and resubmit; expect Front and Beauty to carry the decision.
