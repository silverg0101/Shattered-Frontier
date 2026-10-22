# Tree felling r7 — independent review (2026-10-22)

Reviewer: Fable, no part in the build. Concepts: `GameClient/ArtSource/HarvestOak/MightyOak-Felling-v1/Concept/{sequence,stump,log}`. Captures: `Artifacts/HarvestArt/felling-review-07/{level,slope}` (1920x1080) with `client.log` / `server.log`. Every pair, strip and raw frame was read; crops (x2-x4, 50 px grid) and pixel counts were taken from the raw frames with a script in the reviewer's scratchpad (not committed). Cut-wood colour as r6: R>170, R-B>90, G-B>45, B<140 (shadowed cut wood fails this filter, so counts below are lower bounds; the eye-read in the crops is also given).

Scope as briefed: oak species, bark tone, bare slab and ground tiling are not judged; fall direction is the owner's call; physics varies per run. The bar is the r6 bar. r6 fixes are checked for their intent; previously passing views are re-checked for regression.

Result: **5 PASS / 3 FAIL.** The sequence views pass for the first time: the review camera now looks into the notch mouth and `00-notched` shows a lit cut-wood patch on the trunk on both maps. The stump views still fail: the r6 trench pocket and its walls are gone, but the cut-wood triangles on the log's bark beside the butt are still there (r6 item 3, now 586 / 322 px, was 669 / 399), and the tear band is now a smooth side-grain wall with no shard taller than 100 px. The level log view regresses on the same triangles. Motion strips hold.

Measured facts:

| | level | slope |
|---|---|---|
| FALL_STARTED -> CROWN_YIELDED -> FALL_RESTING (server) | 2.61 s, then 2.75 s | 2.61 s, then 2.59 s |
| `HARVEST_REVIEW_NOTCH_VIEW` (`client.log` 1841) | `RadialToViewDeg=40.0 HitCm=20.8` | `40.0 / 21.4` |
| `HARVEST_REVIEW_SUN` | `P=-30 Y=53.95`; 0 "has to be 'Movable'" lines | `Y=54.12`; 0 |
| `TREE_SPLIT_SURFACE` | Attempt=0, UpperRoom 42.5, LowerRoom 75.5, TearScale 1.000, Amplitude 17.6 | Attempt=0, 42.0 / 76.0 / 1.000 / 20.0 |
| Notch in `00-notched` (trunk x 922-990 at rows 900-960) | lit cut-wood quad **x 969-985, y 910-943** (17 x 33 px) on the trunk face, 210 px strict (288 incl. chip specks at x 1000-1034); interior lit, shadowed floor strip at y 937-943 | **x 966-985, y 907-941**, 136 px strict (196 incl. specks) |
| Right silhouette at the notch rows, `00` | x 973 (row 900) to 987 (row 965), monotonic: **no bite** | same, 973 to 987 |
| Stump top, `09-stump-close` | far rim x 715-1150 at y 380-400; a **smooth side-grain wall y 400-535** (135 px tall, 435 px wide, vertical texture stripes, no relief); crease at y ~535; an end-grain **shelf y 535-620** (near rim at y 620-640); on the shelf a prism x 700-750, y 550-620 (**70 px** tall) and a flat dark wedge x 960-1100, y 583-617 (34 px); rear level surface visible only as a 15 px band at y 385-400 | wall y 395-540; shelf y 540-660; prisms x 743-820 (**100 px**, y 540-640) and x 847-897 (90 px); wedges 17 and 37 px |
| Cut-wood on the log's bark beside the butt, `09` (butt face at x >= 740) | **two triangles**: (649-687, 243-286) 584 px strict and (615-635, 117-172) shadowed, ~20 x 55 px by eye | **three**: (639-669, 387-417) 306 px, (609-620, 297-335) ~12 x 40 shadowed, (710-720, 400-440) ~10 x 40 |
| Butt face, `10-log-close` | x 1040-1130 (**~90 px wide**), y 394-548: flat stepped facets with a diamond; plus a **separate** cut-wood triangle on the log's top bark at (1163-1192, 403-431), 268 px, 35 px of bark between it and the butt | x 1159-1312 (**153 px**), y 426-587, faceted with one shadowed recess; 86 px at (1303-1313, 488-500) contiguous with the butt rim |
| Stump in `10` | in frame, centre-left (x 735-900, y 545-750) | **in frame** (x 550-750, y 510-750); r6 fix 3 met |

What the geometry is now (`SFHarvestLocalizedGeometry.cpp` 340-348, needed for the fixes):

- One split surface still serves both parts. `Plane = -(0.6R+6) * InFootprint + ...` (line 345) drops it below the notch floor over the footprint, so the stump keeps nothing of the trench (good) and **the log keeps all of it**: the trench's roof, back walls and ends are now the butt's stepped facets, and where the roof and walls meet the bark at the mouth they show as the cut-wood triangles on the log's bark, 13-25 cm from the butt, on both maps. The r6 fix 2 intent ("no part of the trench remains on the collar") is not met by this deviation; the README's rejected alternative was a different *cutter*, not the second lofted surface r6 asked for, which was never tried.
- `Band` (line 340) is centred on the contact: 1 for W in [-0.4R, 0], 0.03 beyond W = -0.65R. The footprint covers W > -1.3 D = -26 cm, so the whole tear band lies **on the dropped shelf** and the rear of the stump (behind the step) gets Band 0.03, i.e. a flat plane. That is the smooth wall + flat rear in `09`; the only relief is the few prisms on the shelf.

---

## level-sequence — PASS

Frame 1: the notch now reads as a cut — a lit cut-wood quad 17 x 33 px on the trunk's right third at the notch rows, interior and shadowed floor visible, chip specks beside it. r6 item 1 (no patch on the lit trunk) is resolved. Frames 2-4 as r6: hinge holds through 22 deg with the butt on the stump; at 50 deg the butt is on the ground beside the stump with the crown touching; at rest the trunk lies flat past the stump with the crown crushed.

Recorded, not gating: the r4 numbers (>= 1200 px, V >= 10 px in the silhouette) cannot be met at the 40 deg camera r6 itself specified — the mouth's tangential half-angle on this flared base is ~30-39 deg, so no bite reaches the silhouette and the visible mouth is 20-30 px wide; r6's own estimate for a correct camera was ~25 x 30 px, which this is. Those numbers are retired. Whether the notch should be larger on the shipped trunk is a design call, not a review item. `HitCm=20.8` does not meet the r6 gate (<= 10); the trace stops ~the notch depth short of the cut point on both maps, so it is hitting the trunk's collision, not proving the view. The pixels prove it; the gate should go (fix 3).

## slope-sequence — PASS

As level: 19 x 34 px lit patch at x 966-985, y 907-941; frames 2-4 as r6 (butt on the slab beside the stump at 50 deg, trunk downhill at rest, crown crushed).

## level-stump — FAIL

The pocket, its 115 px vertical wall and the 185 px slab of r6 are gone; the chopped side is one open shelf from the near rim to a crease 135 px below the far rim, with a single step behind it (r6 fix 2 acceptance for the plane and the pocket is met). Two items remain, one unchanged from r6 and one regression:

1. **Cut-wood triangles on the log's bark 13-25 cm from the butt: two, (649-687, 243-286) 584 px strict and (615-635, 117-172) shadowed.** r6 item 3 acceptance (0 px) not met; r6 had 669 px. Cause above: the single dropped surface hands the trench to the log.
   Fix type: ENGINEER (split surface). Fix 1.
2. **No splinter band: the step is a smooth plane 135 x 435 px with side-grain texture stripes and no relief; tallest standing shard 70 px (the prism at x 700-750), on the shelf.** r5's "tallest shard >= 110 px", recorded as met in r6 by what turned out to be trench walls, is now unmet, and the concept's defining feature on the stump (a band of standing fibres between the chopped plane and the far side) is absent. Cause above: `Band` sits inside the footprint.
   Fix type: ENGINEER (tear band). Fix 2.

Not gating, for the record: the step wall and the butt render with side grain (vertical stripes) where the concept shows end grain and fibres; the triplanar pick on a near-vertical cut surface. Re-check after fix 2 moves the relief.

## slope-stump — FAIL

Same read: three triangles on the log's bark ((639-669, 387-417) 306 px, (609-620, 297-335), (710-720, 400-440)); smooth wall y 395-540; tallest shard 100 px (x 743-820). Fixes 1 and 2.

## level-log — FAIL

Regression on the r6 acceptance "0 cut-wood px on the log's bark within 40 cm of the butt in 10": a cut-wood triangle 30 x 28 px (268 px) sits on the log's top bark at (1163-1192, 403-431), 35 px of bark from the butt face — the trench's roof edge, same cause as level-stump item 1; the r6 camera hid the log's top, the r7 camera (fix 3) shows it. Everything else holds: butt on the ground, trunk flat with its contact shadow, bark continuous into the upper structure, stump top in frame, crown crushed. The butt is ~90 px wide, under the 120 px r6 asked fix 3 to keep (framing only, not gating). Fix 1; fix 4 for the width.

## slope-log — PASS

No regression: stump now in frame (r6 fix 3 met), butt 153 px wide and on the ground, trunk flat downhill with contact shadow, crown crushed, bark continuous. The 86 px patch at (1303-1313, 488-500) is contiguous with the butt rim, within the r5-accepted lower-lobe read. The butt will change with fix 1; re-check then.

## level-frames (motion strip) — PASS

No regression: notch visible from 00 (new), hinge holds through 22 deg with the butt on the stump, breaks by 36 deg, butt on the ground beside the stump by 50 deg and stays, crown contact from ~50 deg, flat by rest. Crown yield 2.61 s after FALL_STARTED, rest 2.75 s after the yield.

## slope-frames (motion strip) — PASS

As level; the butt lands on the slab beside the stump by 50 deg and the trunk slides downhill while rotating onto the slab; rest 2.59 s after the yield with the crown crushed downhill. No roll, no skate, nothing floats.

---

## Fix list

1. **Engineer — split surface: build the second (collar) surface r6 fix 2 asked for; the dropped surface stays for the stump.** `GameClient/Source/ShatteredFrontier/Private/Gathering/SFHarvestLocalizedGeometry.cpp` lines 340-348 and the loft that follows. Keep `InFootprint` and the current `Plane` as `StumpDelta`. Add `CollarDelta = (0.6*Cut.Radius+2.0)*InFootprint + (the same non-footprint terms: 0.025*FromNotch.Dot(U) - 0.04*W - 0.45*Chop)`, clamped to `UpperRoom-Rise-1.0` (42 cm leaves room for 15 cm). Tear applies to both with the sign as now. Loft two `SplitVolume`s and cut `OutStump` with the stump one, `OutUpperCollar` with the collar one. Over the footprint the butt then lies at the roof, so no roof, wall or end of the trench survives on the log and nothing meets the bark at the mouth. `ShatteredFrontier.Gathering.{TreeSplit,LocalizedGeometry,LocalizedImportedContact,FellingAuthority}` must pass; add the TreeSplit assertion r6 asked for (no collar vertex inside the footprint below `Cut.Center.Z+0.5*Cut.Radius`, no stump vertex inside it above `Cut.Center.Z-0.5*Cut.Radius`). If the preflight rejects the collar volume on some profile, log it and fall back to the current single surface for that profile only, not globally.
   Acceptance, both maps: **0 cut-wood px on the log's bark within 40 cm of the butt in `09` and `10`** by the r6 filter and by eye in a x4 crop (the butt face excluded); the butt over the footprint is one flat face, not stepped facets.

2. **Engineer — tear band behind the step, not on the shelf.** Same file, line 340. Centre the band 0.45 R behind the notch apex: `Band = max(0.03, 1 - clamp((|W + Cut.Depth + 0.45*Cut.Radius| - 0.2*Cut.Radius) / (0.25*Cut.Radius), 0, 1))`, and multiply `Tear` by `(1 - InFootprint)` so the shelf stays a clean plane. Keep Amplitude (17.6-20 cm -> >= 110 px at the `09` camera), the 3x3 smoothing and the TearScale log. The shards then stand on the stump's rear level, above the step, and their mirror image tears the log's butt.
   Acceptance at the `09-stump-close` camera, both maps: tallest standing shard >= 110 px and located above the shelf crease; the shelf carries no shard; the step wall is no longer one unbroken plane >= 100 px tall across its full width (shards may cross it). Log views re-checked for the butt.

3. **Engineer — review driver, not a gate: drop `HitCm`.** `SFHarvestArtReviewScenario.cpp` 332-335. The visibility trace hits the trunk's collision and cannot prove the view; delete the trace and the `HitCm` field, keep `RadialToViewDeg`. The notch gate is the capture itself: `00-notched` shows >= 150 cut-wood px (r6 filter) on the trunk face at the notch rows on both maps (r7: 210 / 136).

4. **Engineer — framing only, not a gate: level log butt >= 120 px.** `SFHarvestArtReviewScenario.cpp` line 375 area: move the `10-log-close` eye ~25 % closer along the same bearing (or narrow the FOV to 50 deg) on the level map only if the stump stays in frame; the slope framing is right (153 px).

Fixes 1 and 2 are the gate for the stump views and the level log view; they change the stump top and the butt together, so all four close-up views are re-checked next round. No Codex item this round.

Owner flag carried: the tree still falls away from the notch face (`SFHarvestPresentation.cpp` `TreeSplitFor`); not a review item. New flag for the owner, not a review item: on the shipped flared trunk the notch mouth reads as a 17 x 33 px mark at the review distance; if a bigger bite is wanted it is a notch-size decision (cutter 2 R x 1.2 R), not a camera or lighting fix.
