# The Gilded Tankard — Blender stage review r4

Judge: independent art director (did not build this). Inputs: concept `concept/GildedTankard.png` (front elevation left, 3/4 right); renders `GildedTankard-Beauty/Front/Side/SideR/Rear.png` (1100 px; ortho 71.4 px/m, camera z 6.6, ground at y≈1021; the export recentres the model, x −0.25 m and y +0.38 m against the script values); `build_gilded_tankard.py` and `silverbrook_kit.py` read for exact values; `docs/blueprints/buildings.md` §3/§5; `asset-manifest.json` (bounds 10.585 x 8.925 x 13.77 m in the 14 x 12 x 14 slot, 51 308 tris, Door1 part) and `exchange-review.json` (FBX textures resolve, GLB/FBX 6 meshes + the door leaf).

Result: 5 PASS / 1 FAIL. All five captures read as the concept: gable slate roof, centred front dormer, full-height stack on the right gable now behind the right-side window (r3's clash is gone), 0.45 m jetty on corbels with corner posts, two shuttered leaded lights front and rear, one each side, arched double door in a dressed surround, scroll-bracket shield with a gilt tankard, lanterns, barrels, bench. Front fails on the door: two strap systems overlap on the leaf (the kit's full-width straps plus the package's per-leaf straps) and the "double" door has no meeting joint.

## Beauty — PASS

1. Barrels have no hoops (`vessel` of `old_planks`, lines 160): they read as planked tubs; the concept barrels carry three iron bands each. CODE, fix 3.
2. Tankard emblem is a plain 0.26 x 0.70 rectangle with a D handle; it reads as a letter P at this distance. The concept tankard has a lid and a foot. CODE (ART), fix 4.
3. Door reads as one 2.6 m plank slab with two rings; see Front. Fix 1.

## Front — FAIL

1. Door ironwork is doubled. `kit.door` (kit lines 321-322) lays two 2.08 m straps across the whole leaf at z 0.55 and 1.60 (STEP+.35, STEP+spring−.1), 5 cm tall, centred at x −0.195 so they cross the centre line; the package adds four 1.0 m per-leaf straps 12 cm tall at z 0.80 and 1.50 (line 134). In the render (x 440-660, y 880-1000) this reads as four strap rows, the thin kit pair offset 1.5 cm above and 25 cm below the thick pair and running across the leaf split. CODE, fix 1 (kit opt-in `straps=False`).
2. No meeting stile: the leaf is one prism; the planks run continuously across x=0 (zoom at 5x shows no joint). The concept has a clear centre joint with a cover strip. CODE, fix 1 (astragal on the leaf mesh so it travels with Door1).
3. Package straps float 5 mm off the leaf face (strap rear y −2.99, leaf face y −2.985). Invisible here, a shadow line in-engine. CODE, fix 1 (`-DF+.16`).
4. Eave corbels (line 82) stop 2 cm under the fascia: corbel top z 7.19 (EAVE−.36+.15), fascia bottom 7.21 (EAVE−.07−.12). Hidden in this elevation, visible from the side (Side, fascia end at x 790-800, y 490-500: sky between the fascia and the block). CODE, fix 5.
5. Everything else measured: arch extrados 3.30 m under the soffit at 3.64 (0.34 m of ashlar, 9 %, matches the concept); shield 0.8 x 1.15 m, bottom 3.85 m above the step, plate 6 x 10 x 100 cm at z 4.55-5.55 clear of the sill rail (4.37-4.49); strut foot inside the plaster at y −3.55, passes the rail at z 4.73; scroll ring 5.08-5.42 touches the arm; lantern plates on the ashlar face. Nothing floats.

## Side — PASS

Left gable, inferred. Chimney rises behind the gable at image x 326-405 (model y 1.65-2.75) with its pot; dormer ridge and cheek show above the slope at x 575-740 (gabled dormer on a 42 deg slope); one shuttered light at y=0 with posts at ±1.5/±3.5 and both rails; stone storey flush, 0.45 m jetty step with beam stub, soffit and corbel block; sign in profile (plate, arm, strut, octagonal scroll, board edge-on); lanterns and both barrels; bench hidden behind the barrels. Ridge-cap end face as a 0.32 m tab at the apex, as in r3. Only fix from this view: the fascia/corbel slit (fix 5).

## SideR — PASS

Right gable, chimney side, street front on the left. Stack y 1.65-2.75 (image x 695-773), window at y 0 with both shutters open to |y| 1.345 (image x 481-673), side post at y 1.4-1.6 between them (image x 677-691): 5.5 cm shutter-to-post, 5 cm post-to-stack, nothing inside the stone. Side rails pass through the stack hidden (rail x 4.575-4.705 inside wall 4.6 + stack 4.59-5.49). Barge board enters and leaves the stack at z 8.95: reads as a stack built around the verge. Stack shadow across the window is lighting, not geometry. Jetty beam stub, soffit and corbel at the front corner; door jamb quoins and voussoirs proud 5 cm of the ashlar show as the small steps along the front edge. No fix.

## Rear — PASS

Inferred; judged for consistency and breakage. Two shuttered lights at ±2.6, posts at ±1.1 and ±4.5, sill and top rails, rear fascia to x ±5.0, no dormer. Stack at the left: visible full height outside the roof (x 4.75-5.24 after recentring) and above z 8.5 behind the slope (x 4.34-4.75), tiles omitted inside `in_stack`. Side post at y 3.5 flush with the rear face shows as a 12.5 cm strip beside the corner post; jetty beam end (0.2 m past the gable) shows as the dark block at x 905, z 3.7-4.0. Sun flipped to yaw 145 for this view: the ashlar reads. No fix.

## Concept-to-model — PASS

Concept measured at 127 px/m on the front elevation (door head 2.2 m): 5.9-6.5 m wide, stone 2.8, upper 2.5, roof 1.55 (roof/width 0.26). Model: 9.2 m (10.0 at the eaves), 3.7 / 4.0 / 3.6 (0.36). The width follows §3's 0.6-0.8 of the 14 m slot and the "Tankard 4.5/3.2" example; the roof follows the 40 deg pitch floor. Both accepted in r3 and unchanged.

| Element | Concept | Model | Verdict |
|---|---|---|---|
| Roof | gable, ridge full width, dormer centred | gable 42 deg, dormer at y −2.8, caps, barge boards | OK |
| Jetty | ~0.5 m on corbels, front only, corner posts | 0.45 m, corbels at ±4.5/±2.6, posts ±1.1/±4.5 and corner pairs | OK |
| Chimney | right gable, rear corner, one pot | x 4.59-5.49, y 1.65-2.75, cap, one pot; window clear | OK |
| Door | double, straps per leaf, two rings, centre joint | one 2.6 m leaf, rings both sides, kit straps across + per-leaf straps, no joint | Fix 1 |
| Sign | scroll bracket, shield, gilt tankard with lid | §5 bracket, 0.8 x 1.15 shield, rectangle-and-handle emblem | Fix 4 |
| Windows | 2 front, 1 each side, shuttered, lit | same, plus 2 rear | OK |
| Lanterns | either side of the door | z 2.55, plates on the ashlar | OK |
| Barrels / bench | 2 hooped barrels left, bench right | 2 plain barrels (0.873 m apart), bench | Fix 3 |
| Materials | — | kit scans and solids, no defaults, no z-fighting seen | OK |

## Fix list

All in `GameClient/ArtSource/Silverbrook/GildedTankard/build_gilded_tankard.py` unless stated; line numbers refer to the current file. Rebuild, run `review_gilded_tankard.py`, submit the five views.

1. **Door: single strap set, flush, with a meeting stile.** Codex.
   `GameClient/ArtSource/Silverbrook/silverbrook_kit.py` line 314: `def door(G,M,c,o,w,spring,stone_rects,wall_th=.3,band=None,straps=True):`
   kit line 321: `for z in ((.35,spring-.1) if straps else ()):` (default keeps every other building unchanged).
   Line 131: `door(G,M,(XDOOR,-DF,STEP),(0,-1,0),DWID,DS,STONES,band=band(),straps=False)`
   Line 134: `for z in (STEP+.6,STEP+DS-.2):DL.box((s*(DWID/2-.55),-DF+.16,z),(1.0,.01,.12),M['iron'],.004)`
   After line 138 add: `DL.box((0,-DF+.155,STEP+1.38),(.06,.02,2.72),M['timber'],.004,grain=True,band=band())   # meeting stile on the leaf: z 0.22..2.94 under the 3.0 m crown`
   Check: leaf outer face y −2.985; straps y −2.995..−2.985 (touching); stile y −3.005..−2.985, x ±0.03 clear of the straps (|x| 0.25..1.25) and rings (|x| 0.715); nothing changes the hinge (min along r unchanged).

2. **Door strap rows match the concept heights.** Codex, optional with 1.
   Line 134 z tuple: `(STEP+.55,STEP+1.65)` (concept straps 0.55 and 1.65 m above the step; at |x| 1.25 the leaf crown is z 1.86, so the upper strap stays inside the leaf).

3. **Barrel hoops.** Codex.
   Replace line 160 with:
   ```python
   for bx,by in ((-3.05,-DF-.8),(-2.2,-DF-.6)):
       G['timber'].vessel((bx,by,0),[(.28,0),(.36,.25),(.40,.55),(.36,.85),(.28,1.1),(0,1.1)],M['timber'])
       for hoop in ([(.321,.09),(.340,.15)],[(.408,.52),(.408,.58)],[(.340,.95),(.321,1.01)]):G['detail'].vessel((bx,by,0),hoop,M['iron'])   # three iron bands, 12 mm proud of the staves
   ```
   Check: stave radius 0.309/0.328 at z 0.09/0.15, 0.396 at 0.52/0.58, 0.328/0.309 at 0.95/1.01; hoops 12 mm outside each; barrels 0.873 m apart, max radius 0.408 + 0.408 = 0.816, no contact.

4. **Tankard emblem: lid and foot.** Codex.
   Replace lines 152-155 with:
   ```python
   body=[(-.17,4.37),(.09,4.37),(.09,5.05),(-.17,5.05)]   # tankard: 0.26 x 0.68 body, open D handle abutting at x .09
   lid=[(-.2,5.05),(.11,5.05),(.11,5.12),(-.2,5.12)];foot=[(-.2,4.30),(.11,4.30),(.11,4.37),(-.2,4.37)]
   handle=[(.09,4.47),(.18,4.47),(.27,4.57),(.27,4.85),(.18,4.95),(.09,4.95),(.09,4.89),(.15,4.89),(.21,4.83),(.21,4.59),(.15,4.53),(.09,4.53)]
   for poly in (body,lid,foot,handle):
       for dy in (-.035,.035):G['detail'].prism([(x,SY+dy,z) for x,z in poly],(0,1,0),.02,M['gilt'])
   ```
   Check: polygons abut, none overlap (no coplanar fighting); shield half-width at z 4.30 is 0.25 (foot x −0.2..0.11 inside), top 5.2 above the lid at 5.12.

5. **Eave corbels into the fascia and the plaster.** Codex.
   Line 82: `for x in (-4.0,-2.6,-1.3,0,1.3,2.6,4.0):G['timber'].box((x,-D2-.17,EAVE-.33),(.16,.4,.3),M['timber'],.012,grain=True,band=band())   # eave corbels under the fascia`
   Check: top z 7.22 is 1 cm inside the fascia (7.21-7.45); y −3.97..−3.57, 3 cm into the plaster, 2.5 cm behind the fascia rear (−3.995); x 1.22-1.38 clears the post at 1.0-1.2.
