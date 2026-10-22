# The Gilded Tankard — Unreal stage review r2 (dev map `GildedTankard_Review`)

Judge: independent art director (did not build this). Inputs: concept `docs/Game-Design/concept-art/Silverbrook/The Gilded Tankard.png`; the eleven raw `<view>.png` (1920x1080) and their `-compare.png` pairs; `README.md`; `REVIEW-r1.md`; `build_gilded_tankard.py`, `silverbrook_kit.py` (`Geo.box`, `solid`), `silverbrook_props.py` (`dress_house_details`, `dress_footings`), `tools/capture_building.py`; `docs/blueprints/buildings.md` §5/§8/§10. Pixel coordinates are in the raw captures; colours are 0-255 means of the named region.

Result: **8 PASS / 3 FAIL.** The r1 list is in: street face lit, one sign, flush plinth, caged lanterns, no window clipping, washing line visible, crops inside the fence. Two defects remain, both in the package mesh: the baked doorstep became a 3.4 m stone shelf when the footing pass lifted the house onto its plinth, and the gilt tankard is not gilt now that it is lit (r1 fix 8's condition is met).

Layout verified against the code before judging: two front windows at x ±2.6, sign, dormer and door at x 0 (`build_gilded_tankard.py` lines 59, 129, 132, 143-160). The third "window" at the right edge of `frontage` (1830-1920, 0-180) is the right-gable light past the corner, and the sign appearing over the left window in `frontage` (880-1000, 60-260) and `three-quarter` (660-720, 540-600) is parallax of a sign plane 1.2 m proud of the wall (`SY=-D2-1.2`) seen from a front-right camera. Neither is a defect.

## frontage — FAIL

1. A pale stone shelf crosses the doorway at (910-1300, 830-950): 3.4 m wide, 0.45 m deep, 0.2 m thick, uniform (205,197,185) with no joints, sitting on the plinth top with the 1.5 m stoop (915-1180) under its middle only. From the street it reads as a bench top in front of the door. Cause: line 141 bakes the Blender-stage ground step `(DWID+.8,.45,STEP)` at model z 0; `dress_footings` lifts the house 65 cm and builds a 1.5 m stoop (`set_actor_scale3d(... 1.5 ...)`, line 648), so the baked step is now a shelf at z 65-85 cm, 0.95 m of it cantilevered each side. ART, fix 1.
2. Tankard emblem at (900-980, 90-230) is a flat olive (170,160,100) cut-out on the shield; the concept's is bright gold with a highlight. Fix 2.
3. Matches: ashlar ground storey with the dressed round arch and quoins, double door with straps and ring plates, caged lanterns either side (850-890, 510-580; 1360-1410, 500-590) with amber glass and a dark cap, jetty beam on corbels, oak frame and lit leaded lights above, two hooped barrels left of the door on the ground, bench right.
4. Note, not graded: the view is a 5.3 m player's-eye framing (`capture_building.py` line 28) that cuts the roof, dormer and chimney; the elevation was judged in `three-quarter`.

## foundation-close — FAIL

1. The shelf of frontage 1, close: slab x 890-1480, top face y 600-660, front face 650-700; stoop treads x 900-1270 with three risers down to y 840. The right 1270-1480 (about 0.95 m) and the left 890-900 overhang the plinth face with only shadow beneath. The slab's smooth, jointless face is the §10 "smeared pale pier block" look (a single `STONES` rect stretched over 3.4 m). Fix 1.
2. Plinth and stoop themselves are right: the 65 cm course is a darker matte rubble distinct from the ashlar (course change at y about 790 under the bench), the stoop's three equal risers land on the floor, barrels (630-880, 540-760) and bench (1400-1920, 740-1000) stand on the ground at the wall foot as drawn. Lanterns read as cages with a cap.

## three-quarter — PASS

Massing, 42-degree gable, centred dormer under the ridge, full-height chimney on the right gable with cap and pot, jetty on corbels, eave corbel row, two shuttered lit windows on the front and one on each gable, arched double door, barrels left, bench right, lanterns either side. Nothing floats or clips: the fence L (1240-1560, 960-1080) holds its sprigs inside, the ivy clump stands at the front-right plinth corner (1030-1110, 960-1060). Visible here but small: the olive emblem (660-700, 540-600, fix 2) and the pale slab strip over the stoop (700-870, 920-940, fix 1).

## rear — PASS

Two shuttered lit lights with sill and plate rails, plaster flush on the ashlar, plinth flush, washing line on two posts 2.3 m behind the wall (760-1300, 720-1000) with five garments, ladder leaning on the left gable's rear corner (1640-1720, 640-940), fence L seen past the right corner (390-500, 820-890). No clipping at glass strength 1.2: the lead cames are continuous dark lines over an even amber fill.

## sign-close — FAIL

1. The tankard is not gilt: body (760-1050, 160-720) lit face about (178,150,72), flat across its whole width with no specular gradient; the concept tankard is bright gold with a highlight and shadow side. `line 33` `kit.solid('SB_GildedTankard_Gilt',(.62,.45,.18),.4,.85)`: at metallic .85 the base colour only tints a reflection of sky and striped floor, so the face is a dull mustard from every angle. ART, fix 2.
2. Right as built: shield board 0.8 x 1.15 m of dark planks, lid and foot on the tankard, open D handle, arm with strut and scroll ring, two drop rods into the board, window with shutters behind, sill rail and jetty beam. Nothing clips.

## context-overview-a — PASS

Front-right from above: slate roof with individual tiles and ridge caps, dormer, chimney with pot, jetty, door with stoop, barrels, bench, ivy at the front-right corner, fence L with the sprigs inside it, nothing mirrored or in rows. The slab (800-880, 730-745) is a pale strip at this distance (fix 1).

## context-overview-b — PASS

Rear-left from above: roof, chimney, dormer peak beyond the ridge, two lit rear windows, one gable window, plinth flush, washing line in the rear shadow. Nothing broken.

## context-ground-1 — PASS

Right gable from 20 m: chimney from the ground against the gable, plaster triangle, one shuttered light clear of the stack, ashlar below, sign and lantern edge-on at the front corner, fence L (860-1120, 820-950) with the sprigs inside, ivy clump at the corner, washing line beyond. A tavern with a herb plot, barrels, a bench and a yard line is a coherent place.

## context-ground-2 — PASS

Left gable from the front-left: one shuttered light, posts and rails, ashlar below, ladder at mid depth (780-850, 690-1020), washing line at the rear-left, barrels at the front corner, sign and lantern edge-on. Nothing floats.

## context-ground-3 — PASS

Rear and right gable from 24 m: chimney full height, two rear windows, washing line, fence L at the chimney side, ladder past the far corner, plinth flush all round.

## context-ground-4 — PASS

Front-right from the right: chimney gable, gable window, fence L with sprigs inside, ivy at the plinth corner, washing line at the rear. Nothing broken.

## Noted, not graded here

- Surface treatment: plaster shows 30-40 cm trowel arcs (`sign-close` 1320-1920, 0-600) on an even cream, ashlar is bright with crisp joints, timber mid-brown; the concept is grimy plaster, weathered rubble, near-black oak. §10 defers this to GFX-03 §2 ("not per-house patches"), as every WP6 review has.
- Door rings at |x| 0.715 (mid-leaf) instead of beside the joint: r5 Blender fix 3, accepted there.

## Fix list (each executable without this review)

1. **ART, package, Codex** — `GameClient/ArtSource/Silverbrook/GildedTankard/build_gilded_tankard.py` line 141: make the baked threshold stone the width of the engine stoop. Replace
   `G['stone'].box((XDOOR,-DF-.225,STEP/2),(DWID+.8,.45,STEP),M['ashlar'],.015,rect=kit.rng.choice(STONES))`
   with
   `G['stone'].box((XDOOR,-DF-.225,STEP/2),(1.5,.45,STEP),M['ashlar'],.015,rect=kit.rng.choice(STONES))   # threshold stone the width of the engine stoop (silverbrook_props.dress_footings, 1.5 m); wider reads as a shelf once the footing pass lifts the house onto its plinth`
   Package only; the bounds (chimney x, sign y) and `base_cm` are unchanged, so the door part and stoop placement do not move. Rebuild (`blender.exe -b --factory-startup -P build_gilded_tankard.py`), re-import (`Run-InEditor.py import_silverbrook.py --package-only GildedTankard`). Acceptance: in `foundation-close.png` the threshold stone's ends coincide with the top tread's ends (no slab beyond x of the stoop).
2. **ART, package, Codex** — same file line 33: gilt that reads as gold under the dev-map sun. Replace
   `gilt=kit.solid('SB_GildedTankard_Gilt',(.62,.45,.18),.4,.85))`
   with
   `gilt=kit.solid('SB_GildedTankard_Gilt',(.90,.66,.22),.3,.6))   # bright warm base, part-diffuse: at metal .85 the emblem only mirrored the sky and read olive`
   Package-local material (`SB_GildedTankard_Gilt`), nothing shared. Rebuild and re-import with fix 1. Acceptance: `sign-close.png` tankard body mean >= (200,160,70) with a visible light-to-dark gradient across the body; `frontage.png` emblem no longer olive.
3. **Recapture, Codex** — after 1-2: README steps 4 (re-import), 6 (`silverbrook_props.py --folder Districts/Dev --terrain Terrain`), 7 (four gates, expected lines unchanged), 8 (`capture_building.py`), 9 (`make_pairs.py`); then a fresh reviewer judges the eleven pairs.
