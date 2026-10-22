# The Gilded Tankard — Blender stage review r5

Judge: independent art director (did not build this). Inputs: concept `concept/GildedTankard.png` (front elevation left, 3/4 right; measured 858 px ground-to-ridge, door head 2.2 m); renders `GildedTankard-Beauty/Front/Side/SideR/Rear.png` (1100 px; ortho 71.4 px/m, camera z 6.6, ground at image y 1021; the export recentres the model x −0.245 m, y +0.40 m against the script values); `build_gilded_tankard.py` and `silverbrook_kit.py` read for exact values; `docs/blueprints/buildings.md` §3/§5; `asset-manifest.json` (bounds 10.585 x 8.925 x 13.77 m in the 14 x 12 x 14 slot, 51 616 tris, one Door1 part 2.6 m wide) and `exchange-review.json` (GLB/FBX 6 meshes + the leaf, no missing images).

Result: 6 PASS / 0 FAIL. The r4 fix list is in: one strap set per leaf, flush, at 0.55/1.65 m with a centre cover strip; hooped barrels; lid and foot on the tankard; eave corbels into the fascia. Every view reads as the concept and nothing is broken (no clipping, floating, defaults or z-fighting found; every joint below checked against the code). Remaining differences are proportion and part-count items, listed most visible first.

## Beauty — PASS

1. Chimney stack is 2.2 m above the ridge; the concept's cap sits 0.4–0.5 m above it. Stone top z 13.3 (line 108) vs ridge cap 11.12; in the capture the stack rises 154 px over the ridge on a 792 px ground-to-ridge rise (19 %), the concept 38 px on 858 (4 %). The 3/4 view reads as a tower stack, not the squat gable stack drawn. CODE, fix 1.
2. Tankard emblem is a 0.26 m wide body on a 0.8 m shield (33 %); the concept body is 53 % of the shield width (handle included 74 %). At Beauty distance it reads as a tall "I" with a D handle. CODE (ART), fix 2.
3. Door reads as two leaves (cover strip at x 0, straps per leaf) but rings sit mid-leaf at |x| 0.715; the concept has both rings beside the joint. CODE, fix 3 (rings move with the two-leaf split).

## Front — PASS

1. Chimney height as Beauty 1 (image x 860–924, top row 75 vs ridge row 229). Fix 1.
2. Emblem width as Beauty 2 (shield x 495–552 px, body 520–535). Fix 2.
3. Door: leaf is one prism (manifest Door1 width 2.6, hinge x −1.3) with a cover strip for a joint; it opens as one 2.6 m slab. The concept is a pair. Visually fine closed; wrong the moment it opens in-engine. CODE (kit opt-in), fix 3.
4. Eave corbels at x ±1.3 (1.22–1.38) sit 2 cm from the posts at ±1.1 (1.0–1.2) and read as a notch on the post (image x 455–470 and 630–645, y 495–515). The other five are free-standing. CODE, fix 4.
5. Measured and clean: door straps z 0.75 and 1.85 inside the leaf crown (2.26 at |x| 1.25), 1 cm rear face on the leaf (y −2.995..−2.985); cover strip x ±0.03 clear of the straps (|x| 0.25–1.25) and ring plates (|x| 0.68–0.75); arch extrados 3.30 under the soffit 3.64; shield 0.8 x 1.15, bottom 3.85 above the step, rods 1 cm into the board; sign plate z 4.55–5.55 flush on the plaster, strut passes the sill rail (4.37–4.49) at z 4.73; scroll ring touches the strut (both iron, as the Stag); lantern plates 1 cm into the ashlar; bench legs inside the seat; barrels 0.873 m apart (max r 0.408), 9 cm clear of the step; nothing floats.

## Side — PASS

Left gable, inferred. Chimney behind the gable at model y 1.65–2.75 (image x 326–405) with its pot; dormer roof (DE 9.73–DR 10.48) and plaster cheek above the slope at x 575–740; one shuttered light at y 0, posts at ±1.5/±3.5, sill and top rails; ashlar flush with the plaster face, 0.45 m jetty step with beam stub (x −4.8), soffit and corbel block; fascia end + eave corbel block under the roof edge at the front corner (corbel y −3.97..−3.57 is 37 cm proud of the gable face, expected); sign in profile (plate, arm, strut, octagon, board edge-on with gilt both faces); lanterns, both barrels. Ridge-cap end tab at the apex as r3/r4. Only the chimney height (fix 1) shows here: stack top 2.2 m over the apex.

## SideR — PASS

Right gable, chimney side, street front on the left. Stack image x 695–773 (y 1.65–2.75), window at y 0 with shutters open to |y| 1.345 (x 481–673), side post y 1.4–1.6 (x 677–691): 5.5 cm shutter-to-post, 5 cm post-to-stack, nothing inside the stone. Side rails pass through the stack hidden (x 4.575–4.705 inside the wall 4.6 + stack 4.59–5.49); barge board enters and leaves the stack at z 8.46–9.45 hidden inside it. Stack shadow across the window is lighting. Front corner: jetty beam stub, soffit, corbel; the 5 cm proud quoins and step read as the thin dark line down the front edge (sun at yaw 145). Barrel and lantern at the front. Fix 1 only.

## Rear — PASS

Inferred; judged for consistency and breakage. Two shuttered lights at ±2.6, posts ±1.1/±4.5, sill and top rails, rear fascia to x ±5.0, no dormer, plaster flush on the ashlar (no rear jetty). Stack at the left full height outside the roof (x 4.75–5.24 after recentring) and above z 8.5 behind the slope, tiles omitted inside `in_stack`. Right edge: side post strip (x 4.6–4.725) beside the corner post, jetty beam end (x 4.725–4.8, z 3.7–4.0) as the small dark block at image x 905, y 735–760 — unlit end grain, not a hole. Fix 1 only.

## Concept-to-model — PASS

Concept front elevation 732 px wide on an 858 px ground-to-ridge rise (0.85); model ground storey 9.2 m on 11.0 m (0.84). Storeys 3.7 / 4.0 / roof 3.6 at 42 deg against the concept's 2.8 / 2.5 / 1.55 (door units): accepted r3/r4 under §3's 40 deg floor and the "Tankard 4.5/3.2" row, unchanged.

| Element | Concept | Model | Verdict |
|---|---|---|---|
| Roof | gable, ridge full width, centred dormer | gable 42 deg, dormer y −2.8, caps, barge boards | OK |
| Chimney | right gable from the ground, cap 0.4–0.5 m over the ridge, one pot | x 4.59–5.49, y 1.65–2.75, cap 2.2 m over the ridge, one pot | Fix 1 |
| Jetty | ~0.5 m on corbels, front only | 0.45 m, corbels ±4.5/±2.6, beam, soffit | OK |
| Door | pair, straps per leaf, rings at the joint | one 2.6 m leaf with cover strip, straps per leaf, rings mid-leaf | Fix 3 |
| Sign | scroll bracket, shield, wide gilt tankard with lid | §5 bracket, 0.8 x 1.15 shield, slender tankard with lid/foot | Fix 2 |
| Windows | 2 front, 1 each side, shuttered, lit | same, plus 2 rear | OK |
| Lanterns | either side of the door, body ~1.8 m over the step | z 2.55 top, body 2.08 over the step (§3 range) | OK |
| Barrels / bench | 2 hooped barrels left, bench right | same | OK |
| Materials | — | kit scans and solids, no defaults | OK |

## Fix list

All in `GameClient/ArtSource/Silverbrook/GildedTankard/build_gilded_tankard.py` unless stated; line numbers refer to the current file. Rebuild, run `review_gilded_tankard.py`, submit the five views.

1. **Chimney cap 0.5 m over the ridge.** Codex.
   Replace lines 107–109 with:
   ```python
   CT=RIDGE+.5   # stack top 0.5 m over the ridge, as drawn (concept cap 38 px over an 858 px ground-to-ridge rise)
   G['stone'].box((XC,YC,CT/2),(.9,1.1,CT),M['stone'],.01)
   G['stone'].box((XC,YC,CT+.05),(1.0,1.2,.1),M['stone'],.015,rect=kit.rng.choice(STONES))
   G['detail'].vessel((XC,YC,CT+.1),[(.12,0),(.115,.34),(.125,.43),(.14,.47),(.11,.47),(.095,.41),(0,.41)],M['pot'])
   ```
   Check: stack 0–11.5, cap 11.5–11.6, pot top 12.07 (model height 12.07 < 14); cap clears the ridge cap (11.12) by 0.38 m and the roof where the stack emerges (z 8.5–9.5) by 2 m; barge board inside the stack at z 8.46–9.45 stays hidden; `in_stack` unchanged.

2. **Tankard emblem to the concept's width.** Codex.
   Replace lines 153–155 with:
   ```python
   body=[(-.27,4.47),(.13,4.47),(.13,5.05),(-.27,5.05)]   # tankard: 0.40 x 0.58 body (concept body 53 % of the shield width), open D handle abutting at x .13
   lid=[(-.30,5.05),(.15,5.05),(.15,5.12),(-.30,5.12)];foot=[(-.30,4.40),(.15,4.40),(.15,4.47),(-.30,4.47)]
   handle=[(.13,4.55),(.22,4.55),(.31,4.65),(.31,4.93),(.22,5.03),(.13,5.03),(.13,4.97),(.19,4.97),(.25,4.91),(.25,4.67),(.19,4.61),(.13,4.61)]
   ```
   Lines 156–157 (the loop) unchanged. Check: shield half-width is 0.4 above z 4.45 and (z − 4.05) below it; foot bottom 4.40 → 0.35 ≥ 0.30; handle outer 0.31 ≤ 0.4; lid top 5.12 < 5.2; polygons abut, none overlap. Emblem 0.61 x 0.72 m (concept 0.74 x 0.61 of the shield).

3. **Two door leaves, rings at the joint.** Engineer (kit opt-in; default path byte-identical for every other building; the Gatehouse builds its own leaves and is untouched).
   `GameClient/ArtSource/Silverbrook/silverbrook_kit.py` lines 314–326 →
   ```python
   def door(G,M,c,o,w,spring,stone_rects,wall_th=.3,band=None,straps=True,leaves=1):
       """Round-headed ledged plank door in a dressed-stone surround (jamb quoins + voussoirs).
       leaves=2: two half-leaves meeting at a=0, each hinged on its own jamb; they carry no kit ironwork (the package lays it)."""
       P,R=frame(c,o);rad=w/2
       arc=lambda t0,t1,n:[P(rad*math.cos(t),-.2,spring+rad*math.sin(t)) for t in np.linspace(t0,t1,n)]
       if leaves==1:outlines=[([P(-rad,-.2,0),P(rad,-.2,0)]+arc(0,math.pi,17),-1)]
       else:outlines=[([P(-rad,-.2,0),P(0,-.2,0)]+arc(math.pi/2,math.pi,9),-1),([P(0,-.2,0),P(rad,-.2,0)]+arc(0,math.pi/2,9),1)]
       for outline,hinge in outlines:
           # The leaf (boards, straps, ring) is its own mesh so it can open: hinge on the left jamb seen from outside (hinge=1: the right jamb).
           D=Geo(f"{G['detail'].name}_Door{len(DOORS)+1}",G['detail'].col);G[D.name]=D
           D.prism(outline,o,.07,M['door'])
           if leaves==1:
               for z in ((.35,spring-.1) if straps else ()):   # straps=False: the package lays its own per-leaf straps
                   D.box(P(-rad*.15,-.15,z),(w*.8,.012,.05),M['iron'],.004,R)
               for k in range(10):
                   t=math.tau*k/10;D.box(P(rad*.55+.06*math.cos(t),-.13,1.0+.06*math.sin(t)),(.04,.014,.014),M['iron'],0,R@Matrix.Rotation(t,3,'Y'))
               D.box(P(rad*.55,-.155,1.06),(.07,.01,.07),M['iron'],.004,R)
           DOORS.append(dict(geo=D,r=(R@Vector((1,0,0))).normalized(),o=Vector(o).normalized(),width=w/leaves,hinge=hinge))
   ```
   (lines 327–335, the quoins and voussoirs, unchanged.) Kit lines 161–165 →
   ```python
       for k,d in enumerate(DOORS,1):
           obj=bpy.data.objects[d['geo'].name];vs=[obj.matrix_world@v.co for v in obj.data.vertices];r,o=d['r'],d['o'];h=d.get('hinge',-1)
           # Hinge: the leaf's bottom corner at the -r jamb (+r when hinge=1) on its inner (-o) face; frame axes are orthonormal.
           pivot=r*(max if h>0 else min)(v.dot(r) for v in vs)+o*min(v.dot(o) for v in vs)+UP*min(v.z for v in vs)
           parts.append((obj,f'Door{k}',pivot,(0,0,1),'swing',-h*(90 if UP.cross(r).dot(o)<0 else -90),'closed'))  # opens inward (-o)
   ```
   Package lines 131–139 →
   ```python
   door(G,M,(XDOOR,-DF,STEP),(0,-1,0),DWID,DS,STONES,band=band(),straps=False,leaves=2)
   R0=frame((0,0,0),(0,-1,0))[1]
   for n,s in ((1,-1),(2,1)):   # Door1 left leaf, Door2 right leaf: ironwork on the leaf mesh so it travels with the part
       DL=G[f"{G['detail'].name}_Door{n}"]
       for z in (STEP+.55,STEP+1.65):DL.box((s*(DWID/2-.55),-DF+.16,z),(1.0,.01,.12),M['iron'],.004)
       for k in range(10):   # ring beside the meeting joint, as drawn
           t=math.tau*k/10;DL.box((s*.2+.06*math.cos(t),-DF+.13,STEP+1.0+.06*math.sin(t)),(.04,.014,.014),M['iron'],0,R0@Matrix.Rotation(t,3,'Y'))
       DL.box((s*.2,-DF+.155,STEP+1.06),(.07,.01,.07),M['iron'],.004)
       DL.box((s*.015,-DF+.155,STEP+1.38),(.03,.02,2.72),M['timber'],.004,grain=True,band=band())   # cover strip over the joint, half on each leaf
   ```
   Check: Door1 pivot x −1.3 travel +90, Door2 pivot x +1.3 travel −90, both open inward; `_vestibules` groups the coplanar pair into one entry room (same d, same z0, spans merge); ring plates |x| 0.165–0.235 and rings 0.14–0.26 clear the strip (|x| ≤ 0.03) and the straps (z 0.69–0.81 / 1.79–1.91 vs ring 0.94–1.10); straps end at |x| 1.25 inside the leaf crown; the strap boxes stay inside |x| 1.3 so the pivot is the leaf edge. Manifest gets Door1 and Door2 (width 1.3 each); update the docstring (line 5 "double door") and the README part list.

4. **Inner eave corbels off the posts.** Codex.
   Line 82: `for x in (-4.0,-2.6,-1.5,0,1.5,2.6,4.0):G['timber'].box((x,-D2-.17,EAVE-.33),(.16,.4,.3),M['timber'],.012,grain=True,band=band())   # eave corbels under the fascia`
   Check: corbel x 1.42–1.58 is 22 cm from the post (1.0–1.2) and 1.1 m from the next corbel; nothing else at z 6.92–7.22.
