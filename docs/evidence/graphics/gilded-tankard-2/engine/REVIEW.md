# The Gilded Tankard — Unreal stage review r5 (dev map `GildedTankard_Review`)

Judge: independent art director (did not build this). Inputs: concept `docs/Game-Design/concept-art/Silverbrook/The Gilded Tankard.png`; the eleven raw `<view>.png` (1920x1080) and their `-compare.png` pairs; `README.md`; `REVIEW-r4.md` (for the acceptance lines only); `build_gilded_tankard.py` (lines 20-33, 51-55, 97-110, 119-121, 129-159, 163-171), `asset-manifest.json`, `silverbrook_kit.py` (`solid` 170-175, `vessel` 239-244, `door` 314-326, `lantern` 337-358), `silverbrook_props.py` (`dress_footings` stoop 646-664, details 352-477), `docs/blueprints/buildings.md` §5/§6/§8, `props-and-dressing.md` §B. Pixel boxes are in the raw captures (x0, y0, x1, y1); colours are 0-255 sRGB means, luminance Rec.709.

Result: **11 PASS / 0 FAIL.**

The r4 defect A is closed: the tankard body is a smooth turned vessel (along `sign-close` row y 450 the luminance runs 110, 134, 149, 156, 164, 171, 176, 179, 181, 180, 175, 168, 158, 139 across x 780-1040 with no step of 8 or more levels inside the body; the only steps are its silhouette at x 764-766 and 1044) with a domed lid above the lip ring, and its colour sits at the plaster's brightness in the two framing views: `frontage` emblem (918-980, 110-220) (181,143,60) lum 145 against plaster (1050-1180, 120-220) lum 139 = 1.04x; `three-quarter` emblem (680-705, 540-600) lum 160 against plaster (730-800, 480-560) lum 153 = 1.05x; R/G 1.24-1.26 in all three views (concept 1.29). In `sign-close` the lit face (880-940, 300-600) (219,177,68) lum 178 is 1.18x the plaster (1400-1800, 100-500) lum 151, 0.03 over the r4 fix-2 band; at 3 m the emblem still reads as a gilt tankard, not a lemon cut-out, so it is noted below and not failed.

Checked for floating and breakage outside the plinth, against the code: lantern arms root in the ashlar face at `-DF-.03` and the lantern body hangs 0.42 m out (line 171); sign plate and arm root in the plaster face at `-D2-.03` (144-145), the board hangs on two rods from the arm (150-151); eave corbels (82) span y -3.97..-3.57 against the plaster face at -3.6; jetty corbels (120) run from the ashlar face to the beam; the stack (108) stands on its manifest footing; the dormer sits on the front slope with its ridge at DR below RIDGE. Nothing is floating, mirrored, default-material or placeholder in any frame.

## frontage — PASS

Ashlar ground storey on the 65 cm plinth course (1500-1900, 870-910, dark rubble under the lighter ashlar at lum 56 vs 87), dressed round arch with voussoirs and jamb quoins, round-headed double door filling the arch (the dark field above the straps at 1000-1300, 480-640 is the jetty's shadow, not a void: the leaf planks continue through it), two straps per leaf (y 650, 800) and ring plates mid-leaf, caged lanterns either side (865-915, 525-595; 1380-1440, 500-590) with amber glass, jetty beam on four corbels and a plank soffit, oak posts and rails, two shuttered lit leaded lights at x +-2.6, the sign bracket (arm, strut, scroll ring, wall plate; 950-1090, 60-130) with the shield board and gilt tankard hanging perpendicular to the wall in front of the left window (perspective; it is 1.2 m out on the facade centre line, line 55), three stoop risers of plinth rubble (930-1310, 870-1030), barrels left (745-915, 780-940) and bench right (1300-1920, 980-1080) on the floor, ivy clump leaves at the far corner. Same layout, proportions and elements as the concept elevation.

## foundation-close — PASS

Three equal risers (tread edges y 660, 715, 765, floor 815: 55/50/50 px) of the plinth's rubble up to the door sill, top tread the door's full width (1050-1420), lower treads 40 cm further toward the street each (`dress_footings` 658: `off=face+20+40*k`; the leftward stagger is the camera at +0.55 W looking along the wall); stoop top (89,73,54) lum 75 and plinth (68,54,39) lum 56, both darker than the ashlar (102,85,64) lum 87, matte. Door leaves end on the sill with no gap; ring plates at y 450-480; straps at y 320 and 540. Barrels (650-890, 540-760) and bench (1430-1920, 740-1000) stand on the floor at the wall foot. Lanterns are cages with a cone cap and amber glass (820-880, 170-260; 1560-1640, 60-170).

## three-quarter — PASS

Massing as the concept 3/4: 42-degree slate gable roof with ridge caps, centred gabled dormer with a lit leaded light, full-height stone stack on the right gable with cap and pot clear of the ridge (1290-1420, 40-120), jetty beam on corbels with the eave corbel row above, oak-framed plaster storey with two front and one gable shuttered lights, ashlar ground storey on the plinth course, small lit ground-storey light on the right wall (1120-1175, 745-840) as drawn, arched door with the three stoop risers (700-830, 930-1000), barrels left (580-690, 890-990), bench right (850-980, 980-1030), lanterns either side, hanging sign (680-775, 505-600) perpendicular to the facade, ivy clump at the front-right plinth corner (1020-1110, 960-1060) on the floor, garden fence L (1230-1570, 950-1080) with its crop sprigs inside and posts on the floor. The stack base meets the dark footing course (1313-1397, 950-990), no void.

## rear — PASS

Two shuttered lit lights with sill and plate rails, plaster flush on the ashlar, plinth course under the wall (520-1640, 900-950), washing line on two braced posts (760-1300, 720-1000) with five garments, ladder edge-on against the far (left) side wall (1640-1760, 640-940) with its feet on the floor, fence L past the right corner (390-500, 820-890). The stack is edge-on at the left (490-540, 60-940). The dark wedge under the wall (490-760, 900-1080) is the building's cast shadow on the floor (sun from the street face), not geometry.

## sign-close — PASS

Shield board 0.8 x 1.15 m of dark planks hung on two drop rods under the arm, wall plate and strut at the right, open shutters and the leaded light behind at the left, sill rail and jetty beam below. Tankard: smooth body (no facet step >= 8 levels along y 450), lip ring (760-1050, 150-200), foot ring (760-1050, 670-710), domed lid (820-1040, 90-150), D handle (1040-1180, 220-610) meeting the body, relief on this face casting its own shadow on the board. Lit face (219,177,68) lum 178, shadow side (174,133,50) lum 135, R/G 1.24-1.31. Nothing clips.

Noted, not failing: the lit face is 1.18x the plaster here (band in r4 fix 2 was 0.9-1.15x) and the body is more saturated than the drawn brass (B/R 0.31 against the concept's 0.51 at (488-520, 495-560)). At the frontage and three-quarter distances the emblem is at 1.04-1.05x the plaster, which is what a player sees.

## context-overview-a — PASS

Front-right from above: slate roof with ridge caps and the dormer on the front slope under the main ridge, stack with pot on its footing course (1175-1232, 735-760), jetty, door with the three risers (800-880, 725-770), barrels, bench, ivy clump, fence L with sprigs inside (1110-1280, 760-910). Nothing mirrored, in rows, or floating. The emblem (770-800, 510-560) is a 30 px gilt mark on the dark board.

## context-overview-b — PASS

Rear-left from above: roof, stack rising through the rear slope with clean slate edges against it, the dormer's gable peak beyond the ridge (1030-1080, 165-200: its barge boards, as a dormer on the far slope shows from above), two lit rear windows, one gable window, plinth course under both visible walls, washing line (690-800, 740-900) and ladder (1090-1150, 690-830) in the rear shadow, the front jetty beam end past the far corner (1240-1270, 550-580).

## context-ground-1 — PASS

Right gable from the right: full-height stack against the gable on its dark footing course (1180-1273, 840-880) continuous with the plinth, plaster gable triangle, one shuttered upper light clear of the stack, the small lit ground-storey light (895-960, 625-715) near the front corner, sign (670-700, 460-540) and lantern (750-780, 640-670) seen past the front corner, barrels (715-770, 760-830) and ivy clump (760-840, 810-900) at the corner, fence L (860-1120, 820-950) with sprigs inside, washing line beyond (1320-1380, 700-830). The dormer's side timber shows above the front eave (800-830, 150-190).

## context-ground-2 — PASS

Left gable from the left: one shuttered light, posts and rails, ashlar below, plinth course flush, ladder at mid depth (770-850, 680-1030) face-on with both feet on the floor and its head on the ashlar, washing line at the rear-left (310-440, 740-990) with braced posts, barrels at the front corner (1230-1340, 870-1000), sign edge-on with the tankard through it (1300-1340, 450-540) and lantern (1220-1260, 650-710) past the corner on the front wall. The jetty shows as the 0.45 m oversail at the front corner (1210-1290, 520-590).

## context-ground-3 — PASS

Rear from the right: stack full height over the ridge on its footing (560-585, 830-870), two shuttered rear windows, washing line (790-1120, 700-920), ladder edge-on past the far corner (1290-1340, 690-860), fence L with sprigs at the left (440-550, 780-840). Plinth course under the rear wall (560-1300, 830-880).

## context-ground-4 — PASS

Stack gable from the rear-right: stack on its footing course (790-860, 830-870), gable window, the small lit ground-storey light near the front corner (670-700, 640-710), jetty beam end past the front corner (605-660, 545-600), fence L (360-720, 790-880) with sprigs, ivy clump (600-650, 770-850), washing line at the rear (1210-1400, 690-900) with braced posts on the floor.

## Noted, not graded here

- Wall lanterns are smaller than drawn: glass body 0.22 m (`kit.lantern` cage, line 348) against about 0.55 m in the concept; in `frontage` the lantern is 0.18 of the door height, in the concept 0.29. They read as lanterns at every distance; if the owner wants the drawn size, add an opt-in `scale=1.0` factor to `kit.lantern(...)` multiplying the body, bar, ring and roof dimensions (default unchanged, every other package identical) and pass `scale=2.0` at `build_gilded_tankard.py` line 171. Not a fail.
- Tankard saturation and the 1.18x `sign-close` ratio (above). Not a fail.
- Bracket arm at z 5.45 (upper-window mid-height, as drawn) against `buildings.md` §5's 3.45-3.75 m: the concept wins; the blueprint row should say "or at the upper-window mid-height when the concept draws it there".
- Surface treatment (plaster trowel arcs, crisp pale ashlar, mid-brown oak against the concept's grime and near-black timber; lit windows under a noon sun): GFX-03 §2, as in every WP6 review.
- Dev-map floor stripes and the floor edge: expected, not graded.

## Fix list

(empty — all eleven views pass)
