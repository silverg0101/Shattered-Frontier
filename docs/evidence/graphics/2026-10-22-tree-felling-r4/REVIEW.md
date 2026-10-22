# Tree felling r4 — independent review (2026-10-22)

Reviewer: Fable, no part in the build. Concepts: `GameClient/ArtSource/HarvestOak/MightyOak-Felling-v1/Concept/{sequence,stump,log}`. Captures: `Artifacts/HarvestArt/felling-review-04/{level,slope}` (1920x1080) with `client.log` / `server.log`. Pairs and strips in this folder. Every image was read at full resolution; crops (x4-x8) and pixel counts were taken from the raw frames.

Scope applied as briefed: oak species, bark tone and the bare slab are not judged. Repeating ground patches are ignored. Physics varies per run; what is captured is judged. The bar is the r3 bar.

Result: **4 PASS / 4 FAIL**. The r3 gate is cleared: the trunk lands on both maps, the butt leaves the stump and lies beside it, the fall accelerates through its last 30 deg, and the quiet test (not the settle cap) freezes the pose. The stump top has a chopped plane, a level far third and a narrow tear band. What still fails: the notch is not readable in `00-notched` (the one keyframe the concept labels "notched"), and the 1 m join on the log shows cut-wood slivers and a bark break.

Measured facts (from `client.log`, `server.log` and pixel sampling of the raw frames):

| | level | slope |
|---|---|---|
| Rest lean from vertical (`HARVEST_FELLING_REST`) | 98.4 deg (r3: 79.2) | 116.3 deg = 101.3 slab-relative (r3: 95.2) |
| Lean 2 -> 50 deg | 1.94 s | 2.02 s |
| Lean 50 -> 78 deg | **0.59 s** (r3: 1.10) | 0.67 s to 92 deg (the "78" shot landed at 92.2) |
| Crown yield (server) | 2.64 s after FALL_STARTED, Radius=60 | 2.69 s after, Radius=60 |
| FALL_RESTING after yield | 2.79 s (< `MaxSettleSeconds` 4.0: quiet test passed) | 2.79 s (quiet test passed) |
| 78 deg shot -> `HARVEST_FELLING_REST` | 3.64 s | 3.63 s |
| Body origin travel 78 deg shot -> rest | 71 cm | 121 cm |
| Butt at rest (`08-rest`, 1 px/cm) | torn end on the ground; stump right silhouette x~1000, butt left edge x~1035: **35 px of ground between** (r3 acceptance >= 25) | torn end on the slab ~1.2 m downhill of the stump (stump x 930-1000, butt x 1100-1150) |
| Trunk underside (`08-rest`, `10-log-close`) | contact shadow directly under the lower trunk for its whole visible length; fork limbs rise into the crushed crown | same |
| Notch, `00-notched` (rows 880-960, trunk left silhouette) | silhouette x 922-924 above and 923-926 below; rows 915-935 the outline stays at x~923 and the pixels x 923-947 are the notch's inner wall: **426 orange px, mean (68,49,32)**; no background shows through, **bite 0-3 px** | silhouette x 922-923; **58 orange px, mean (101,75,50)**; bite 0 px |
| Stump top, `09-stump-close` | chopped face one sloped plane (x 640-860, y 400-600); far third level (x 1000-1160); three shards in the band x 860-1050, tallest ~155 px (~20 cm); hinge gap a dark hole at (950-1000, 460-500) | plane + level third; shards in x 860-1000, y 430-600; tear crack below the cut plane on the near side at (870-920, 540-610) |
| Join on the log, `10-log-close` | bark pattern break at x~1293 for the full log height (y 500-760); **139 cut-wood px** (R>200, G>150, B<140) in x 1280-1335: slivers at (1290-1305, 505-520), (1280-1290, 625-690), (1300-1330, 705-760); 15 px luminance columns either side 227 / 227 / 223 / 221 (within 8) | pattern break at x~1290; **13 cut-wood px** in x 1270-1330, y 220-640; a yellow fringe along the top of the lower trunk x 1360-1500, y 240-300 |
| Torn butt face, `10-log-close` | 130 px wide (x 1430-1560), ~35 deg off edge-on; stump not in frame | 180 px wide (x 1500-1680); stump not in frame |

---

## level-sequence — FAIL

Frames 2-4 read as the concept: hinge holds through 22 deg with the butt on the stump; at 51 deg the butt is on the ground beside the stump and the crown is touching; at rest the lower trunk is flat with a contact shadow, the butt a stride past the stump, the crown crushed. Frame 1 does not:

1. **The notch is a 25 x 40 px dark patch, mean (68,49,32), in the crown's shadow, with no silhouette bite.** Concept frame 1: a bright wedge taking a third of the trunk's width out of the silhouette. Cause, from the geometry: the review camera sits exactly perpendicular to the fall plane (`SFHarvestArtReviewScenario.cpp` line 330, `-R.Side*1500`), so the notch is viewed edge-on; an 18 cm notch on a ~60 cm trunk whose cutter ends at +-25 cm tangentially (`WoodRadiusCm=25`) is a closed void from that angle: the line of sight through the mouth hits the void's far end wall, never the background. The r3 "bite" acceptance was unreachable from this camera, and the sun flip (line 328-329) puts the notch face at grazing light under the crown. The concept's bite is visible because its camera is ~40 deg off the notch normal and the notch is sunlit.
   Fix type: LEVEL (review camera + review light). See fix 1.

## slope-sequence — FAIL

Frames 2-4 as level (butt on the slab beside the stump at 50 deg; at rest the trunk lies downhill with the crown crushed). Frame 1: 58 orange px, mean (101,75,50), bite 0 px.
Fix type: LEVEL. Fix 1.

## level-stump — PASS

The r3 items are resolved: butt on the ground, not on the stump (its torn end is in the top-left corner of `09`, on the ground, clear of the stump); chopped third is a single sloped plane with no facet taller than ~3 cm; far third level; the tear is three shards of ~20 cm in a band ~0.4 R wide; grain runs across the end and along the shard faces; nothing floats or sinks; cut-wood and bark are the shipped materials. Remaining difference, secondary: the shards are three flat triangles, not fibres (boolean grid limit, accepted in r3). Optional fix 5.

## slope-stump — PASS

Same read; the tear crack running 25 cm below the cut plane on the near side reads as fibres pulled out of the stump. Stump seated on the slab on both the uphill and downhill sides.

## level-log — FAIL

The butt is on the ground and the torn face is visible at 130 px (r3 fix 6 intent met). The lower trunk lies flat with its contact shadow. Then, in order of visibility:

1. **Cut-wood slivers at the 1 m join.** 139 pixels of cut-wood colour in a 55 px band around x~1293: a sliver along the top edge (1290-1305, 505-520), a wedge on the near face (1280-1290, 625-690) and a 30 x 55 px triangle on the underside (1300-1330, 705-760). `Build-SmoothLowerTrunk.py` line 107 gives both caps slot 1 (cut-wood); the top cap's rim stands proud of the upper structure's base ring, so it shows as yellow from any angle that sees the join's silhouette. This is new in r4 (the r3 seam was a tone step only).
   Fix type: ART. See fix 2.
2. **Bark pattern break at the join, full log height.** The luminance step from r3 is gone (227 vs 223), but the crevice pattern restarts at x~1293 with a visible vertical line. Builder's deviation (per-column UV copy stretched the bark into stripes) explains why the r3 instruction was not followed; it does not meet the item's intent, which was no visible join.
   Fix type: ART. See fix 3.
3. Secondary: the stump is not in frame (r3 fix 6 acceptance asked for it; the concept log image has the stump at the left). Fix type: LEVEL. Fix 4.

## slope-log — FAIL

1. Cut-wood slivers at the join: 13 px in the band plus a yellow fringe along the top of the lower trunk (x 1360-1500, y 240-300) where the top cap's rim shows above the bark silhouette. Fix 2.
2. Bark pattern break at x~1290. Fix 3.
3. Secondary: stump not in frame. Fix 4.

## level-frames (motion strip) — PASS

Direction: correct. Hinge: holds through 22 deg with the butt on the stump; breaks at 25 deg (`HingeBreakDegrees`), and in `04` (36.9 deg) the butt is 60 cm forward of the stump and ~50 cm up, mid-kick; by `05` (51.4 deg) it is on the ground beside the stump and stays there. Speed: 2 -> 50 deg in 1.94 s, 50 -> 78 in 0.59 s, accelerating to the end. Ground contact: crown from ~50 deg, trunk down by rest. Rest: quiet test, 2.79 s after the yield; the crown is crushed to ~3 m and the lower trunk lies flat. Reads as a weighted fall.

## slope-frames (motion strip) — PASS

Direction and hinge as level; 2 -> 50 deg in 2.02 s, 50 -> 92 in 0.67 s. The butt lands on the slab beside the stump at 50 deg and the trunk slides ~1.2 m downhill while rotating its last 24 deg onto the slab, which reads correctly on a 15 deg slope. Rest by the quiet test at 101 deg slab-relative with the crown crushed downhill. No roll, no skate.

---

## Fix list (shared fixes first)

1. **Engineer — LEVEL (review script + review light) — frame the notch the way the concept does.** The edge-on sequence camera cannot show a 30 %-diameter notch; the concept camera is ~40 deg off the notch normal on the sunlit side. `GameClient/Source/ShatteredFrontier/Private/Tests/SFHarvestArtReviewScenario.cpp` line 330, replace the view with one swung 35 deg toward the notch side for the whole sequence (keyframes 00-08 keep one camera, as the concept does):
   ```cpp
   const FVector SeqDir=(-R.Side*FMath::Cos(FMath::DegreesToRadians(35.f))-R.FallDir*FMath::Sin(FMath::DegreesToRadians(35.f))).GetSafeNormal();
   View(R.Base+FVector(0,0,450)+SeqDir*1500,R.Base+FVector(0,0,450),65); R.bNotched=true;
   ```
   and light the cut face: in the block at lines 170-171 that sets the sun's intensity for the review, also set its rotation so it comes from the camera side at 30 deg elevation (`It->SetActorRotation((-SeqDir).Rotation()+FRotator(-30,0,0))`, computed after `SeqDir` is known, i.e. move the intensity block into the `R.Completed==2` branch or store the light and rotate it there). Drop the `Side` flip at lines 328-329 (the sun now follows the camera). The fall still crosses the frame (cos 35 = 0.82 of its width) and lands toward the far side, as in the concept.
   Acceptance: in `00-notched` on both maps the notch shows >= 1200 px of cut-wood colour with mean R >= 150, and the near silhouette shows a V >= 10 px deep at the notch rows; keyframes 03/05/08 still show the butt on the stump, the butt on the ground, and the trunk flat.

2. **Codex — ART — kill the cut-wood rim at the 1 m join.** `GameClient/ArtSource/HarvestOak/MightyOak-Felling-v1/tools/Build-SmoothLowerTrunk.py` line ~107: give the TOP cap `material_index = 0` (bark), keep slot 1 only on the bottom cap (the only cap a player can see is the one the boolean exposes, and the boolean assigns its own cut-wood faces). Then make the top ring coincide with the upper structure: read `SM_MightyOak_UpperStructure`'s lowest ring (vertices within 2 mm of the join height) and set the lower trunk's top ring vertices to those positions verbatim (same count and angular order as `rings[-1]`), so no rim stands proud. Re-import via `tools/Update-HarvestOakFellingArt.py`; `ShatteredFrontier.Gathering.{TreeSplit,LocalizedGeometry}` must pass.
   Acceptance: zero pixels with R>200, G>150, B<140 inside a 60 px band around the join in `10-log-close` on both maps, and no yellow fringe along the lower trunk's top silhouette.

3. **Codex — ART — continue the bark pattern across the join without stripes.** Keep cylindrical UVs (the builder's stripe finding stands) but fit them to the upper mesh instead of copying per column: measure the upper structure's lowest ring UVs; least-squares fit `U = a*angle + b` and `V = c*z + d` to that ring (one affine fit, not 24 independent columns); apply the fit to every lower-trunk ring. If the upper ring's U is not monotonic in angle (so an affine fit cannot match it within 0.02 UV), switch `M_MightyOak_Bark` to object-space triplanar bark on both meshes in `tools/Update-HarvestOakFellingArt.py` (the bark normal/roughness maps are already shared) so the pattern is independent of the UV seam. Same tests as fix 2.
   Acceptance: at the `10-log-close` camera no vertical edge at the join: horizontal-gradient energy in the 6 px column at the join <= 1.5x the mean of the 6 px columns 40 px either side, over the log's full height.

4. **Engineer — LEVEL (review script) — stump in the log shot.** `SFHarvestArtReviewScenario.cpp` line 368: `View(Butt-R.FallDir*420+R.Side*260+FVector(0,0,140),Butt+Rest.GetUnitAxis(EAxis::Z)*220,50);` -> `View(Butt-R.FallDir*150+R.Side*520+FVector(0,0,160),Butt+R.FallDir*160+FVector(0,0,40),55);` (further to the side so the stump, one metre behind the butt, enters the frame edge while the torn face stays >= 120 px wide).
   Acceptance: stump top visible in `10-log-close` on both maps; torn face >= 120 px wide.

5. **Codex — ART — optional, not a gate: fibre read on the tear.** Author a 3-card alpha splinter tuft (`T_MightyOak_Splinter_BaseColor`, cards 25 x 12 cm, masked) and have `SFHarvestPresentation.cpp` place one at the tear band centre on the stump top and its mirror on the collar, aligned to `FallDirection`. This adds fibres without touching the boolean grid or the notch cutter. Skip if the owner accepts the three-shard tear.

Design note for the owner, not a fix: the live tree falls away from the notch face (`SFHarvestPresentation.cpp` line 70, `FallDirection = -Cut.Normal`), while the concept, like real felling, drops the tree toward the notch with the woodsman on the far side. The GDD row says fall direction must be reviewed before gameplay use; this is that review's flag.

Fix 1 is the gate for the sequence views, fixes 2-3 for the log views. Fixes 1 and 4 change the review script only; 2 and 3 rebuild an existing asset; 5 is optional.
