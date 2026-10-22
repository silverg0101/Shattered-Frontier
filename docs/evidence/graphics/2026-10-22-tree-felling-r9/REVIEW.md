# Tree felling r9 — independent review (2026-10-22)

Reviewer: Fable, no part in the build. Concepts: `GameClient/ArtSource/HarvestOak/MightyOak-Felling-v1/Concept/{sequence,stump,log}`. Captures: `Artifacts/HarvestArt/felling-review-09/{level,slope}` (1920x1080) with `client.log` / `server.log`. Every pair, strip and raw frame was read; x2-x4 crops with a 50 px grid and pixel counts were taken from the raw frames (and from the r8 raw frames for comparison) with a script in the reviewer's scratchpad (not committed). Cut-wood filter as r6-r8: R>170, R-B>90, G-B>45, B<140 (shadowed cut wood fails it; counts are lower bounds, the eye-read is given beside them).

Scope as briefed: bare slab and ground tiling are fixture artifacts; the short forked oak is the shipped art; fall direction and the un-notched log are owner decisions; physics varies per run; one split surface and the prism cutter stay. The bar is the r8 bar. r8 fix 1 is checked for its intent; README item 2 (framing not changed) was not a gate. Passing views are re-checked for regression.

Result: **6 PASS / 2 FAIL.** r8 fix 1 is done and does what it was asked to do: the trench is off the log. The butt is no longer a multi-facet recess (two end-grain planes and one wall on the level map, one plane with two tear pockets on the slope), and the separate cut-wood wedge on the log's top bark in `10-log-close` is gone (414 px -> 11 scattered px). Both log views pass. The two stump views fail on the one item they failed on before, the cut-wood triangles on the log's bark beside the butt in `09`, which are **pixel-identical to r7 and r8** (587 vs 598 vs 584 px level; 321 vs 307 vs 306 px slope) although the trench they were attributed to since r5 is now gone. They are not split geometry; the evidence below points at the upper structure's cut-wood join cap. Fix 1 is a one-line material change with a verification step; it does not touch the split.

Measured facts:

| | level | slope |
|---|---|---|
| FALL_STARTED -> CROWN_YIELDED -> FALL_RESTING (server) | 2.65 s, then 2.73 s | 2.71 s, then 2.60 s |
| `HARVEST_REVIEW_NOTCH_VIEW` / `HARVEST_REVIEW_SUN` | `RadialToViewDeg=40.0`; `P=-30 Y=53.95`; 0 "has to be 'Movable'" lines | same; `Y=54.12`; 0 |
| `TREE_SPLIT_SURFACE` | Attempt 0 **and 1** logged at every split (r8: 0 only); 42.5 / 75.5 / 1.000 / 17.6 | Attempts 0, 1, 2, **3** at every split; 42.0 / 76.0 / 1.000 / 20.0 |
| Warnings `HARVEST_TREE_*_REJECTED` | none | none |
| Notch in `00-notched` | lit quad x 969-985, y 910-943 on the trunk's right third (213 px incl. specks; r8 294) | x 966-994, y 907-949 (146 px; r8 207) |
| Stump top, `09-stump-close` | shelf: one end-grain plane with rings, x 700-1100, y 555-700, nothing on it; rear raised block x 720-1050, top edge y 330 (x 870) to 440 (x 1050); its front wall is one plane with vertical side-grain stripes (r8: a run of facets and a spike to y 375) | shelf y 590-750, rings, clean; rear wall y 450-600; two shards on it: x 800-870 tip y 300, x 960-1030 tip y 380 (r8: one prism, tip 120 px above the crease) |
| Butt face, `09` | disc x 750-1000, y 0-290: end-grain plane y 0-190 (rings centred ~(900,110)), step wall band y 190-235, second end-grain plane x 850-1000, y 235-285. **No multi-facet recess** (r8 had dark facets at 770-960, 190-280) | disc x 735-985, y 55-345: one ringed plane y 55-255, then two shadowed pockets (740-790, 255-310) and (875-990, 255-310) with lit end grain between and below (the tear mirror) |
| Cut-wood on the log's bark beside the butt, `09` (butt faces excluded) | **(650-686, 236-285) 587 px lit** and a shadowed sliver ~(610-640, 110-165) by eye. r8: (650-687, 235-285) 598 px and (626-660, 115-160). r7: 584 px at (649-687, 243-286) | **(636-667, 387-418) 321 px lit** and a sliver ~(505-530, 300-325) by eye. r8: (640-669, 388-417) 307 px. r7: 306 px |
| Butt and bark, `10-log-close` | butt x 1068-1160 (**92 px**), y 355-545: two end-grain planes with a diagonal boundary at y ~420 and a darker wall facet (1115-1160, 505-545); bark band (1160-1300, 340-420): **11 px** scattered, nothing visible at x3 (r8: a 414 px wedge at 1208-1243, 367-405) | butt x 1200-1330 (130 px), y 400-570, faceted with a shadowed recess (1220-1300, 500-560); bark band (1330-1650, 380-620): **0 px** |
| Stump in `10` | in frame x 660-900, y 530-820, shelf and a prism shard visible | in frame x 440-720, y 530-820 |

Why the `09` triangles are not the split (needed for fix 1):

- They did not move by a pixel from r7 through r9 while the collar changed from "trench upper half" (r7/r8) to "pristine, no notch faces" (r9), and while the tear profile changed (attempt 1/3 instead of 0). Split-derived faces would have moved or vanished; the `10` wedge, which was split-derived, did vanish.
- They are not debris: `DebrisLifetimeSeconds=3.0` (`DefaultGame.ini` 92) and the `09` shot is 7.4 s after the last chop (client.log 1880 vs server.log 1731).
- They are not the prism's side walls: `SFHarvestLocalizedGeometry.cpp` 290-303 sizes the cutter from the whole lower trunk's bounds plus `max(10, R)` cm, so no bark is clipped laterally.
- They sit on a plane perpendicular to the trunk axis (both have a straight edge at x 650 / 626 level, 636 / 512 slope) at the collar's top. `UpperRoom` is 42.5 cm, so the lower trunk ends 42.5 cm above the cut: at the `09` scale (~2.1 px/cm at the butt) that is ~90 px from the rear butt face at x ~750, i.e. x ~660. In `08-rest` the same mark sits 20-30 px beside the butt at ~0.5 px/cm: 40-60 cm, the same station.
- `SM_MightyOak_UpperStructure` carries a cut-wood cap in slot 1 across the 1 m join, facing -Z (`GameClient/ArtSource/HarvestOak/MightyOak-v1/tools/Prepare-HarvestMightyOak.py` 250-262, `manifest.json` `join_cap_material_index: 1`, README line 10). The lower trunk's top ring samples the upper tree's radius at 24 angles (`Build-SmoothLowerTrunk.py` 61), so wherever the upper base contour lies outside that 24-gon the cap's rim stands proud of the collar's bark. From the `09` camera (butt side) the -Z cap is front-facing and seen nearly face-on, so a few cm of overhang reads as a 37 x 50 px lit triangle; from the `10` camera (crown side) it is back-face culled, which is why r5/r6 found the join clean in `10` and the triangles only in `09`. r4 fix 2 made the lower trunk's own top cap bark; the upper structure's cap was never changed.

---

## level-sequence — PASS

No regression. Frame 1: lit cut-wood quad 17 x 34 px on the trunk's right third at the notch rows with chip specks beside it. Frames 2-4 as r8: hinge holds through 22 deg with the butt on the stump; at 50 deg the butt is on the slab beside the stump with the crown touching; at rest the trunk lies flat past the stump with the crown crushed.

## slope-sequence — PASS

As level: 28 x 42 px quad at x 966-994, y 907-949; frames 2-4 as r8 (butt beside the stump at 50 deg, trunk downhill at rest, crown crushed).

## level-stump — FAIL

r8 fix 1 intent for the butt met: no stepped recess; the butt is the step the single surface implies (a lofted plane over the footprint, the rear plane behind one wall), and the design reads as before: low chopped shelf in front, raised torn rear. The stump top changed with the retry (rear block to y 330, one planar wall instead of r8's facets and spike); the r7 acceptance allows shards to cross the wall rather than requiring them to, so this is recorded, not gated. One item remains:

1. **Cut-wood triangle on the log's bark beside the butt: (650-686, 236-285) 587 px lit, plus a shadowed sliver at ~(610-640, 110-165).** r8 acceptance (0 px on the bark within 40 cm of the butt in `09`) not met; the pixels are r7's and r8's. Cause re-diagnosed above: the upper structure's slot-1 join cap, not the split.
   Fix type: ENGINEER (art material, one line) with a verification step. Fix 1.

Not gating, for the record: the butt's step wall and the stump's rear wall show side grain (vertical stripes) where the concept shows end grain and fibres; unchanged from r7/r8.

## slope-stump — FAIL

Butt: one ringed plane with two tear pockets below it, no recess; shelf clean; two shards on the rear (tips 300 and 220 px above the crease, taller than r8's one prism). One item remains:

1. **Cut-wood on the log's bark: (636-667, 387-418) 321 px lit and a sliver at ~(505-530, 300-325).** r8: 307 px. Same cause. Fix 1.

## level-log — PASS

r8 item 1 resolved: the separate wedge on the top bark is gone (11 scattered px in the 140 x 80 band where r8 had 414, nothing visible at x3); the butt is two end-grain planes with one wall, not a faceted recess. Everything r8 accepted holds: butt on the ground, trunk flat with its contact shadow, bark continuous into the upper structure, stump in frame with shelf and shard, crown crushed. The butt is 92 px wide (r8 fix 2, framing only, not done; not a gate). Fix 3 carried.

## slope-log — PASS

No regression: 0 cut-wood px on the bark band, stump in frame, butt 130 px wide and on the ground with its tear recess, trunk flat downhill with contact shadow, crown crushed, bark continuous.

## level-frames (motion strip) — PASS

No regression: notch visible from 00, hinge holds through 22 deg with the butt on the stump, breaks by 36 deg, butt on the slab beside the stump by 50 deg and stays, crown contact from ~50 deg, flat by rest. Crown yield 2.65 s after FALL_STARTED, rest 2.73 s after the yield. Nothing floats, no roll.

## slope-frames (motion strip) — PASS

As level; the butt lands beside the stump by 50 deg and the trunk flattens downhill onto the slab; rest 2.60 s after the yield with the crown crushed downhill. No skate, nothing floats.

---

## Fix list

1. **Engineer — make the upper structure's join cap bark, then verify the `09` marks are gone.** Durable fix: `GameClient/ArtSource/HarvestOak/MightyOak-v1/tools/Prepare-HarvestMightyOak.py` ~250-262: give the upper join-cap faces `material_index = 0` (bark; the bark material is object-space triplanar on both meshes, so a bark cap that stands proud of the collar is invisible against it), update `manifest.json` `join_cap_material_index`, re-import with `tools/Update-HarvestOakFellingArt.py`. If the re-export is not wanted this round, the runtime equivalent is one line in `GameClient/Source/ShatteredFrontier/Private/Gathering/SFHarvestFallingTree.cpp` `PrepareVisualSource` after line 139, for the `SM_MightyOak_UpperStructure` part only: `Part->SetMaterial(1, Mesh->GetMaterial(0));` (slot 1 on that mesh is only the join cap, README line 10), plus the same override on the standing node's upper structure component so the 1 px seam at y 900 in `00-notched` goes too. Keep the split code, the surface and the cutter untouched.
   Verification if the marks survive: run the review once with the upper structure part hidden (`SetVisibility(false)`); if the triangles are still there they belong to the collar, in which case log every collar triangle with cut-wood material whose centroid is more than 3 cm from the split surface (position relative to `Cut.Center`) and attach the list to the README.
   Acceptance, both maps: **0 cut-wood px by the filter and nothing by eye in a x4 crop on the log's bark in `09` within 60 cm of the rear butt face** (butt faces excluded); `10` bark bands at or below r9's counts; shelf still clean and the rear still raised.

2. **Engineer — not gating — log the rejection reason per split attempt.** `SFHarvestLocalizedGeometry.cpp` 499: attempt 0 is now rejected at every split on both maps and the slope needs 4 of the 6 profiles; a seed that exhausts them rejects the fall with only a Warning that the review logs do not surface. Add `UE_LOG(LogTemp,Display,TEXT("TREE_SPLIT_PROFILE_REJECTED Seed=%u Attempt=%u %s"),...,*LastFailure)` beside the Verbose accept line so the next round can see whether it is the pristine conservation band or the cap-vertex match that fails.

3. **Engineer — framing only, not a gate, carried from r8: level log butt >= 120 px.** `SFHarvestArtReviewScenario.cpp` `10-log-close` camera, level map only: FOV 50 deg or Side 400 cm, stump kept in frame.

Fix 1 is the gate for both stump views; it changes the log's bark at the join and nothing else, so the two log views are re-checked for no regression next round. No Codex item.

Owner flags carried, not review items: the tree falls away from the notch face (`SFHarvestPresentation.cpp` `TreeSplitFor`); the notch reads as a 17 x 34 px mark at the review distance on the shipped flared trunk; the log is the un-notched collar with the notch volume restored on it (bark intact at the mouth).
