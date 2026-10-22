# Tree felling r6 — independent review (2026-10-22)

Reviewer: Fable, no part in the build. Concepts: `GameClient/ArtSource/HarvestOak/MightyOak-Felling-v1/Concept/{sequence,stump,log}`. Captures: `Artifacts/HarvestArt/felling-review-06/{level,slope}` (1920x1080) with `client.log` / `server.log`. Every pair, strip and raw frame was read; crops (x2-x5, with a 50 px grid) and pixel counts were taken from the raw frames with a script in the reviewer's scratchpad (not committed). Cut-wood colour below: R>170, R-B>90, G-B>45, B<140 (sunlit cut wood in `09` is ~(235,185,110); lit bark ~(240,222,196) is excluded).

Scope as briefed: oak species, bark tone, bare slab and ground tiling are not judged; fall direction is the owner's call; physics varies per run. The bar is the r5 bar; r5 fixes are checked for their intent and passing views for regression.

Result: **4 PASS / 4 FAIL**, the same four. Log views and motion strips hold. The sequence views fail on the same item as r3-r5 (no notch in `00-notched`): the sun now moves (fix 1 done, log confirms) but the notch mouth is still edge-on at the right silhouette. The stump views fail on the same read as r5 (level disc with a square-walled pocket and a few prisms; cut-wood triangles on the log's bark). Reading the code against the captures shows why r5's fixes 2 and 3 changed nothing visible: the "socket" is the notch cavity itself, and the 0.6 R lift was applied outside the trunk.

Measured facts:

| | level | slope |
|---|---|---|
| FALL_STARTED -> CROWN_YIELDED -> FALL_RESTING (server) | 2.64 s, then 2.74 s (quiet test, < 4.0) | 2.64 s, then 2.56 s |
| Review sun (`client.log` 1841) | `HARVEST_REVIEW_SUN Rot=P=-30 Y=79.65`; **0** "has to be 'Movable'" lines | `Y=80.33`; 0 |
| `TREE_SPLIT_SURFACE` | UpperRoom 42.5, LowerRoom 75.5, TearScale 1.000, Amplitude 17.6 (R = 22) | 42.0 / 76.0 / 1.000 / 20.0 (R = 25) |
| Notch in `00-notched` (trunk x 905-995 at rows 900-980, 5x crop) | **28 cut-wood px**, all at x 984-988, y 935-941, on the right silhouette; no patch on the lit right third of the trunk; bite 0 px | **17 px** at x 985-988, y 930-936; bite 0 px |
| Chip burst in `00` | 51 px of 3-8 px orange specks at x 1013-1049, y 909-969, i.e. 20-55 px *outside* the right silhouette at notch height (strike 2's chips, one tick after commit) | 53 px at x 1013-1042, y 913-939 |
| `03-lean-22` | cut opens at the right silhouette, x 955-990, y 925-960 (as r5) | same |
| Stump top, `09-stump-close` | rim x 625-1105, y 375-430: near rim (x 650-750) y 400-430, far rim y 375-385, **one ellipse, no crease**; a square pocket x 760-1000, y 400-520 with a shadowed vertical wall x 760-800, **y 405-520 (115 px)** and a flat floor; two pyramids x 800-865 and 865-950; a slab x 990-1060, y 380-565 whose right edge is a **straight vertical 185 px** line; the pocket's mouth is open on the stump's right flank below the rim, x 1000-1060, y 500-565 | rim x 630-1100, y 380-400, level; vertical wall x 800-810, y 390-480 (90 px); pyramid x 800-860; slab x 1000-1060, y 355-560 (vertical right edge **205 px**); side mouth x 1000-1055, y 480-560 |
| Cut-wood on the log's bark beside the butt, `09` | **two triangles** inside (444-509, 130-294): ~65 x 50 and ~40 x 45 px, 669 px by the strict filter (r5: three, 1995 px by the looser filter) | two triangles inside (372-432, 323-437), 399 px (r5: 475) |
| Torn butt face, `10-log-close` | ~175 px wide (x 413-587); a flat disc with a Z-shaped recessed channel; 0 cut-wood px on the bark beside it | ~145 px (x 430-573); faceted cluster with one spike; a 10 x 12 px sliver under the butt at (403,845) as r5 |
| Stump in `10` | in frame, bottom-left (x 95-370, y 820-1080) | **not in frame** (fix 4 not met; camera code at line 375 is as instructed, the aim point `Butt+FallDir*200` is downhill of the butt) |
| Bark fissures on the stump body, `09` | vertical fissure relief visible at x 700-1150, y 500-1000 (r5: plaster); fix 5 met by inspection, no gate | same |

What the geometry actually is (from `SFHarvestLocalizedGeometry.cpp`, needed to write fixes that work):

- The TreeNotch cutter (lines 158-161) is a prism whose V-shaped cross-section lies in the tangent/depth plane and is extruded **vertically** +-0.6 R: a horizontal trench 2 R wide, 1.2 R tall, with a level floor at contact-0.6 R, a level roof at contact+0.6 R and two angled back walls meeting at depth D.
- The split surface (line 362) sits at `Cut.Center.Z + Delta`, i.e. through the *middle* of that trench. So the stump keeps the trench's lower half (floor + 0.6 R of back wall = the "square-walled socket with a flat floor" of r5 and r6) and the log keeps the upper half (roof + upper walls = the cut-wood triangles on the collar's bark). r5's cause for item 2 (independent per-cell noise) was wrong; smoothing the noise (done, TearScale 1.000) could not remove walls that belong to the notch.
- `W` (line 337) is the signed distance from the contact along the outward normal, so `W > 0` is **outside the bark**. `Lift` (line 343, `clamp(W/0.3R)`) and `Chop` (line 340, `W-0.3R`) are therefore ~0 everywhere inside a round trunk; the r6 "split rises 0.6 R on the chopped side" is a no-op, which is why r6's stump top is the same level disc as r5's and the log's triangles are still there. `UpperRoom` 42 cm leaves room for +0.6 R (15 cm); `LowerRoom` 76 cm for -0.6 R.

---

## level-sequence — FAIL

Frames 2-4 read as the concept and as r5: hinge holds through 22 deg with the butt on the stump; at 50 deg the butt is on the ground beside the stump with the crown touching; at rest the trunk lies flat past the stump, the crown crushed. Frame 1:

1. **No notch in `00-notched`: 28 cut-wood px, all on the right silhouette (x 984-988), no bite, no patch on the lit trunk.** r5 fix 1 is done (sun movable, logged, no warning) and the trunk's right third is now sunlit, so light is no longer the cause. The camera code (`SFHarvestArtReviewScenario.cpp` 324-337) puts the camera 55 deg off the cut normal on the notch side, which would show the mouth (2 R = 44 cm wide on a ~70 cm trunk, tangential half-angle ~39 deg) as a ~25 x 30 px cut-wood patch at x 960-995, y 930-960. The frame shows bark there, and both the sliver and strike 2's chip burst (x 1013-1049) sit on or beyond the silhouette: as rendered, the mouth is >= 85 deg off the view axis, ~30-35 deg further round than `State.Cuts.Last().Normal` says. The cut normal is a facet normal of a 576-triangle trunk (`HARVEST_PRESENTATION_READY Triangles=576`), not the radial direction of the mouth.
   Fix type: LEVEL (review camera). Fix 1.

## slope-sequence — FAIL

Frames 2-4 as level (butt on the slab beside the stump at 50 deg; at rest the trunk lies downhill with the crown crushed). Frame 1: 17 px at the right silhouette, bite 0. Fix 1.

## level-stump — FAIL

The butt is on the ground, the stump is seated, the materials are the shipped ones, the bark now has fissure relief (r5 fix 5 met). What a player sees on top is unchanged from r5: a level sawn disc with a square pocket and a few crystal prisms, not the concept's open axe-cut plane rising into a splinter band.

1. **Level disc, no chopped plane, no crease.** Near rim y 400-430 and far rim y 375-385 lie on one ellipse. r5 fix 2 intent (a chopped plane meeting the tear band at a crease) not met; the r6 lift acted outside the trunk (see above).
   Fix type: ENGINEER (split surface). Fix 2.
2. **The pocket is the lower half of the notch trench:** a flat floor at contact-0.6 R and vertical cut-wood walls 115 px tall (x 760-800, y 405-520), open on the stump's right flank below the rim (x 1000-1060, y 500-565). r5 fix 2's "no vertical cut-wood edge >= 60 px" not met; the 185 px vertical right edge of the slab at x 1055 is the trench's end wall, not a shard.
   Fix type: ENGINEER (split surface). Fix 2.
3. **Cut-wood triangles on the log's bark, 10-25 cm from the butt: two, 669 px (strict), inside (444-509, 130-294).** The trench's roof and upper walls, left on the collar because the split passes through the trench's middle. r5 fix 3 acceptance (0 px) not met; its implementation was a no-op.
   Fix type: ENGINEER (split surface). Fix 2.

Shards: the two pyramids and the slab are 110-185 px tall (r5's >= 110 px met) and planar; fibres remain optional as in r4.

## slope-stump — FAIL

Same read: level rim y 380-400, vertical wall 90 px (x 800-810), slab edge 205 px, side mouth x 1000-1055, y 480-560; two cut-wood triangles on the log's bark inside (372-432, 323-437), 399 px. Fix 2.

## level-log — PASS

No regression from r5: 0 cut-wood px on the bark beside the butt in this shot, no edge or pattern break at the 1 m join, bark continuous from the butt into the upper structure, stump top in frame bottom-left, butt on the ground with the trunk flat and its contact shadow, crown crushed, torn face ~175 px. The butt face is a flat disc with a recessed Z-shaped channel (the notch trench's upper half seen end-on), within the r4-accepted "flat faceted butt" state. Fix 2 will change this face (it becomes the roof level over the notch footprint plus the torn band); re-check it then.

## slope-log — PASS

Join clean, bark continuous, torn face ~145 px (>= 120), trunk flat downhill with contact shadow, crown crushed; the 10 x 12 px sliver under the butt is the r5-accepted lower lobe. Remaining difference, not a player-visible defect: the stump is still outside the frame on this map although the camera code at line 375 is as r5 instructed; the aim point is 2 m downhill of the butt, away from the stump. Fix 3 (framing only).

## level-frames (motion strip) — PASS

No regression: hinge holds through 22 deg with the butt on the stump; breaks by 36 deg with the butt kicked forward; by 50 deg the butt is on the ground beside the stump and stays; crown contact from ~50 deg; flat by rest. Crown yield 2.64 s after FALL_STARTED, rest 2.74 s after the yield by the quiet test. Notch invisible in 00-02 (sequence item), visible from 03.

## slope-frames (motion strip) — PASS

As level; the butt lands on the slab beside the stump by 50 deg and the trunk slides downhill while rotating onto the slab; rest 2.56 s after the yield with the crown crushed downhill. No roll, no skate, nothing floats.

---

## Fix list

1. **Engineer — LEVEL (review camera) — frame the notch by its radial direction, 40 deg off, and prove it in the log.** `GameClient/Source/ShatteredFrontier/Private/Tests/SFHarvestArtReviewScenario.cpp` lines 324-337. Keep `R.FallDir` from the cut normal (it must match the physics). Add the mouth direction from the trunk axis: `const FVector CutWorld=Wood->HarvestVisuals->...TransformPosition(State.Cuts.Last().Point)` (the presentation's `Runtime->Transform`; expose a const getter if needed) and `const FVector NotchDir=FVector(CutWorld.X-Wood->GetActorLocation().X,CutWorld.Y-Wood->GetActorLocation().Y,0).GetSafeNormal();` then `SeqDir=(NotchDir*cos(40 deg)-R.Side*sin(40 deg)).GetSafeNormal()` (the `-Side` term keeps the fall crossing the frame as now). Keep the sun as r6 (`(-SeqDir).Rotation()+FRotator(-30,0,0)`). Log, once, `HARVEST_REVIEW_NOTCH_VIEW NormalDeg=%.1f RadialDeg=%.1f Hit=%.1fcm` where the two angles are between `SeqDir` and the horizontal cut normal / `NotchDir`, and `Hit` is the distance from `CutWorld` at which a `LineTraceSingleByChannel(ECC_Visibility)` from the camera eye toward `CutWorld` stops (ignore the pawn). If `RadialDeg` is not within 40 +- 2 or `Hit` > 10 cm, the camera is not looking into the mouth and the run must say so before any capture is reviewed.
   Acceptance (r4/r5 unchanged): in `00-notched` on both maps the notch shows >= 1200 px of cut-wood colour with mean R >= 150 and a V >= 10 px deep in the right silhouette at the notch rows; `client.log` carries the `HARVEST_REVIEW_NOTCH_VIEW` line with `Hit` <= 10; keyframes 03/05/08 still show the hinge, the butt beside the stump and the flat rest.

2. **Engineer — split surface: the stump keeps the open notch floor, the log takes a clean butt at the roof.** `GameClient/Source/ShatteredFrontier/Private/Gathering/SFHarvestLocalizedGeometry.cpp` lines 330-364. Delete `Lift` and `Chop` (lines 340-344; `Plane=0.025*FromNotch.Dot(U)-0.04*W`). Add a notch-footprint weight per cell, inside the trunk: `const double T=FromNotch.Dot(FVector3d::UnitZ().Cross(WorkingFace));` (tangential offset) and `Footprint=clamp((W+Cut.Depth+0.3*Cut.Radius)/(0.3*Cut.Radius),0,1)*clamp((1.1*Cut.Radius-|T|)/(0.2*Cut.Radius),0,1)` (1 over the trench, 0 behind its apex line and beyond its ends, blended over 0.3 R / 0.2 R). Build **two** lower surfaces: `StumpDelta=Delta*(1-Footprint)-0.6*Cut.Radius*Footprint` and `CollarDelta=Delta*(1-Footprint)+0.6*Cut.Radius*Footprint` (the tear term applies to both with `1-Footprint`), each clamped to the rooms, and loft two `SplitVolume`s, one to cut `OutStump` and one to cut `OutUpperCollar`, instead of sharing one. Result: on the stump, the notch's floor is the top surface on the notch side (open to the sky, no roof, no pocket), stepping up at the apex line into the tear band and the level far side; on the log, the butt over the notch footprint is at the roof so no part of the trench remains on the collar. Keep the 3x3 smoothing and TearScale logging. `ShatteredFrontier.Gathering.{TreeSplit,LocalizedGeometry,LocalizedImportedContact,FellingAuthority}` must pass; add one TreeSplit assertion that no stump vertex inside the notch footprint lies above `Cut.Center.Z-0.5*Cut.Radius` and no collar vertex inside it lies below `Cut.Center.Z+0.5*Cut.Radius`.
   Acceptance at the `09-stump-close` camera, both maps: the chopped plane is one open surface from the near rim to a crease >= 50 px below the far-rim ellipse; no flat-floored pocket and no cut-wood wall in the tear band whose silhouette is a straight vertical edge >= 60 px (the single step at the apex line is allowed); tallest shard >= 110 px; **0 cut-wood px on the log's bark within 40 cm of the butt** in `09` and `10` (the torn face itself excluded). Log views re-checked for the butt face.

3. **Engineer — LEVEL (review script), framing only, not a gate — stump in the slope log shot.** `SFHarvestArtReviewScenario.cpp` line 375: aim between butt and stump, `View(Butt-R.FallDir*120+R.Side*620+FVector(0,0,170),(Butt+R.Base)*0.5+FVector(0,0,40),60);` and widen to 70 deg only if the stump top is still cut off. Keep the torn face >= 120 px wide.
   Acceptance: stump top visible in `10-log-close` on both maps.

Fixes 1 and 3 change the review script only; fix 2 changes the split surface (stump top and log butt change together). Fix 1 is the gate for the sequence views, fix 2 for the stump views. No Codex item this round (r5 fix 5 is met).

Owner flag carried: the tree still falls away from the notch face (`SFHarvestPresentation.cpp` `TreeSplitFor`), unlike real felling; not a review item.
