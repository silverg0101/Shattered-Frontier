# Tree felling r8 — independent review (2026-10-22)

Reviewer: Fable, no part in the build. Concepts: `GameClient/ArtSource/HarvestOak/MightyOak-Felling-v1/Concept/{sequence,stump,log}`. Captures: `Artifacts/HarvestArt/felling-review-08/{level,slope}` (1920x1080) with `client.log` / `server.log`. Every pair, strip and raw frame was read; x2-x4 crops with a 50 px grid and pixel counts were taken from the raw frames with a script in the reviewer's scratchpad (not committed). Cut-wood filter as r6/r7: R>170, R-B>90, G-B>45, B<140 (shadowed cut wood fails it; counts are lower bounds and the eye-read is given beside them).

Scope as briefed: bare slab and ground tiling are fixture artifacts; the short forked oak is the shipped art; fall direction is the owner's call; physics varies per run. The bar is the r7 bar. r7 fixes are checked for their intent; the builder's documented deviation (README item 1) is checked against the intent of the item it answers. Passing views are re-checked for regression.

Result: **5 PASS / 3 FAIL**, the same five and three as r7. r7 fixes 2-4 are done and hold. r7 fix 1 was not done: the README's deviation (two-surface split rejected by the boolean, lateral-only footprint left a V pit) explains why that *mechanism* is out, but the item's intent (no trench on the log, no cut wood on the log's bark) is still unmet, with the same pixels as r7. The three views fail on that one item only. A fix that stays inside the new constraints (one split surface, prism cutter) is given below; it does not touch the stump geometry at all.

Measured facts:

| | level | slope |
|---|---|---|
| FALL_STARTED -> CROWN_YIELDED -> FALL_RESTING (server) | 2.62 s, then 2.73 s | 2.61 s, then 2.55 s |
| `HARVEST_REVIEW_NOTCH_VIEW` | `RadialToViewDeg=40.0` (no `HitCm`; r7 fix 3 done) | same |
| `HARVEST_REVIEW_SUN` | `P=-30 Y=53.95`; 0 "has to be 'Movable'" lines | `Y=54.12`; 0 |
| `TREE_SPLIT_SURFACE` | Attempt=0, UpperRoom 42.5, LowerRoom 75.5, TearScale 1.000, Amplitude 17.6 | Attempt=0, 42.0 / 76.0 / 1.000 / 20.0 |
| Notch in `00-notched` (trunk x 920-990 at rows 900-960) | lit cut-wood quad **x 969-985, y 910-945** on the trunk face; 294 px in the notch rows incl. chip specks at x 1000-1035 | quad x 966-985, y 905-949; 207 px incl. specks |
| Stump top, `09-stump-close` | front shelf: flat end-grain plane with rings, x 700-1150, y 540-660, no shard on it; crease at y ~540; rear raised, top edge y ~400 (step 140 px); step wall x 750-1000 is a run of 50-100 px triangular facets, not one plane; a spike at x 700-760 whose tip is at **y 375 (165 px above the crease)** | shelf y 545-660 with rings, clean; rear top edge y ~400; a prism x 735-830, tip **y 425 (120 px above the crease)**; right of it the step wall x 850-1100 (250 px) is one smooth plane ~140 px tall |
| Cut-wood on the log's bark beside the butt, `09` (butt face at x >= 745 level / 640 slope) | **two triangles**: (650-687, 235-285) **598 px** lit and (626-660, 115-160) shadowed ~35 x 45 by eye; r7: 584 px | **two**: (640-669, 388-417) **307 px** lit and (600-640, 290-340) shadowed ~40 x 50 by eye; r7: 306 px |
| Butt face, `09` | end-grain disc x 750-1000, y 0-290 with a **stepped recess** in its lower part: dark facets (770-960, 190-280) | disc x 640-800, y 20-350 with the same recess (720-820, 270-350) |
| Butt face, `10-log-close` | x 1070-1160 (**~90 px wide**), y 370-520, stepped; plus a **separate** cut-wood wedge on the log's top bark at **(1208-1243, 367-405), 414 px**, ~45 px of bark from the butt rim; r7: 268 px at (1163-1192, 403-431) | x 1200-1330 (130 px), y 400-570, faceted with a shadowed recess (1220-1300, 500-560); 93 px at (1373-1382, 472-485) contiguous with the rim (r7: 86 px, accepted) |
| Stump in `10` | in frame, centre-left (x 660-900, y 530-820), shelf and spike visible | in frame (x 440-720, y 530-820) |

What the geometry is now (`SFHarvestLocalizedGeometry.cpp` 336-348, and `SFHarvestPresentation.cpp` 264 / 311 / 395), needed for the fix:

- `SplitTree(Source, ...)` receives the **notched** trunk (`Staged` / `Runtime->Current`) and cuts both parts from it: `Stump = Source - SplitVolume` (line 455), `Collar = Source ∩ SplitVolume` (line 456). `Plane = -(0.6R+6)*InFootprint + ...` (345) drops the surface below the trench floor over the front band, so the stump keeps nothing of the trench (that is the clean shelf, good) and the log keeps **all** of it: roof, back walls and ends become the butt's stepped recess, and where roof and walls meet the bark at the mouth they are the cut-wood triangles 13-25 cm from the butt. Same cause as r6/r7.
- `Runtime->Base` (`SFHarvestPresentation.cpp` 98, 151) is the pristine lower trunk and is alive at all three call sites (`ApplyState` rebuilds `Staged` from it at 297).
- The tear band is now behind the step (340-341) and masked out of the footprint (348): that is the clean shelf and the shards on the rear level in `09`. r7 fix 2 done.

---

## level-sequence — PASS

No regression. Frame 1: lit cut-wood quad 17 x 35 px on the trunk's right third at the notch rows with chip specks beside it (294 px, r7 210 incl. specks). Frames 2-4 as r7: hinge holds through 22 deg with the butt on the stump; at 50 deg the butt is on the slab beside the stump with the crown touching; at rest the trunk lies flat past the stump with the crown crushed.

## slope-sequence — PASS

As level: 19 x 44 px quad at x 966-985, y 905-949 (207 px incl. specks); frames 2-4 as r7 (butt beside the stump at 50 deg, trunk downhill at rest, crown crushed).

## level-stump — FAIL

r7 item 2 is resolved: the shelf is a clean end-grain plane with no shard on it; the step wall is a run of triangular facets, not one unbroken plane; the tallest standing shard (spike x 700-760) tips at y 375, 165 px above the crease. The design reads: a low chopped shelf at the front, a raised broken rear. One item remains, unchanged from r6/r7:

1. **Cut-wood triangles on the log's bark 13-25 cm from the butt: (650-687, 235-285) 598 px lit and (626-660, 115-160) shadowed; the butt itself carries the trench as a stepped recess (770-960, 190-280).** r6 item 3 / r7 item 1 acceptance (0 px on the bark, butt one flat face over the footprint) not met; the pixels are r7's (584 px). The README deviation rules out the two-surface *mechanism*; it does not meet the item's intent.
   Fix type: ENGINEER (collar source). Fix 1.

Not gating, for the record: the step wall and the butt show side grain (vertical / horizontal stripes) where the concept shows end grain and fibres; the triplanar pick on near-vertical cut faces. Unchanged from r7.

## slope-stump — FAIL

r7 item 2 resolved by the letter: shelf clean; prism x 735-830 tips 120 px above the crease; the step wall is broken by that prism at the left. (The remaining wall x 850-1100 is still one smooth plane 250 px wide; recorded, not gating, the r7 acceptance allows shards to cross it rather than requiring them to.) One item remains:

1. **Cut-wood on the log's bark: (640-669, 388-417) 307 px lit and (600-640, 290-340) shadowed; stepped recess on the butt (720-820, 270-350).** r7: 306 px. Fix 1.

## level-log — FAIL

Everything r7 accepted holds: butt on the ground, trunk flat with its contact shadow, bark continuous into the upper structure, stump in frame (shelf and spike visible at x 660-900), crown crushed. One item, the same as r7's:

1. **A separate cut-wood wedge on the log's top bark at (1208-1243, 367-405), 414 px, ~45 px of bark between it and the butt rim; the butt (x 1070-1160) is stepped, not one face.** The trench roof's edge at the mouth. Fix 1.

The butt is ~90 px wide; r7 fix 4 (framing, not a gate) moved the eye for both maps and the level butt is still under 120 px. Not gating; see fix 2.

## slope-log — PASS

No regression: stump in frame, butt 130 px wide and on the ground, trunk flat downhill with contact shadow, crown crushed, bark continuous. The 93 px patch at (1373-1382, 472-485) is contiguous with the rim, the same lower-lobe read r7 accepted (86 px). The butt changes with fix 1; re-check then.

## level-frames (motion strip) — PASS

No regression: notch visible from 00, hinge holds through 22 deg with the butt on the stump, breaks by 36 deg, butt on the slab beside the stump by 50 deg and stays, crown contact from ~50 deg, flat by rest. Crown yield 2.62 s after FALL_STARTED, rest 2.73 s after the yield. Nothing floats, no roll.

## slope-frames (motion strip) — PASS

As level; the butt lands beside the stump by 50 deg and the trunk slides downhill while flattening onto the slab; rest 2.55 s after the yield with the crown crushed downhill. No skate, nothing floats.

---

## Fix list

1. **Engineer — cut the collar from the pristine trunk, the stump from the notched one. One split surface, prism cutter untouched, stump geometry unchanged.** `GameClient/Source/ShatteredFrontier/Private/Gathering/SFHarvestLocalizedGeometry.cpp` / `.h` line 41 and `SFHarvestPresentation.cpp` 264, 311, 395.
   - Add a parameter `const FDynamicMesh3& PristineLowerTrunk` to `SplitTree` / `TrySplitTree`. Prepare it exactly as `Prepared` is (copy, attributes, material ID, orientation flip at 400-405) into `PreparedPristine`. Line 455 stays `Stump = Prepared - SplitVolume`. Line 456 becomes `Collar = PreparedPristine ∩ SplitVolume` (the `BuildPart` lambda takes the source mesh as an argument). The notch lies entirely inside `SplitVolume` over the footprint, so the collar is the same solid the log has today with the trench filled in: its butt over the footprint is one lofted face and its bark at the mouth is the pristine bark. The pristine trunk is the simpler mesh of the two for the boolean.
   - Pass `Runtime->Base` at all three call sites (it is already in scope at each). Tests `SFHarvestTreeSplitTests.cpp` 87/105/109 pass the un-notched source they build the notched one from.
   - Conservation check (457-461): compare against `Volume(PristineLowerTrunk)`, not `Volume(Source)`: pristine = stump + collar exactly when no notch face crosses the surface. Cap-vertex match (462-467): keep as is; both caps are the same surface through the same bark outline when the notch is wholly above it, so the check is the proof that no trench crosses the split.
   - For the check to hold the drop must cover the whole trench with margin. Today (line 344) the lateral footprint is full only to |U| < 0.9 R and the W ramp reaches 1 only at W = -1.3 D + 0.25 R, so for shallow notches (D < ~1.1 R) the apex and the trench ends sit above the surface (the "flat dark wedges" r7 saw on the shelf). Change to: lateral full for |U| <= 1.2 R, ramp to 0 by 1.4 R; W full for W >= -(D + 0.1 R), ramp to 0 by W = -(D + 0.35 R). Keep `BandCentre` and the `(1 - InFootprint)` tear mask as they are; the mask already handles the overlap.
   - If the pristine intersection is rejected on some profile, log it and fall back to the notched source for that profile only (today's result), not globally. `ShatteredFrontier.Gathering.{TreeSplit,LocalizedGeometry,LocalizedImportedContact,FellingAuthority}` must pass; the TreeSplit assertion r6 asked for still applies (no collar vertex inside the footprint below `Cut.Center.Z + 0.5 R`).
   Acceptance, both maps: **0 cut-wood px on the log's bark within 40 cm of the butt in `09` and `10`** by the filter and by eye in a x4 crop (the butt face excluded); the butt over the footprint is one lofted face with the tear mirror, not a stepped recess; the stump top in `09` unchanged from r8 (shelf clean, shard tips at the r8 heights ±15 px, physics variance aside).

2. **Engineer — framing only, not a gate: level log butt >= 120 px.** `SFHarvestArtReviewScenario.cpp` `10-log-close` camera. The r7 fix moved both maps to Side 500 cm and the level butt is still 90 px (the slope butt is 130). On the level map only, narrow the FOV to 50 deg or move the eye to Side 400 cm; keep the stump in frame.

Fix 1 is the gate for the stump views and the level log view; it changes the butt only, so all four close-up views are re-checked next round for the butt and the stump views for no regression. No Codex item this round.

Owner flags carried, not review items: the tree still falls away from the notch face (`SFHarvestPresentation.cpp` `TreeSplitFor`); the notch mouth reads as a 17 x 35 px mark at the review distance on the shipped flared trunk (a notch-size decision, cutter 2 R x 1.2 R). New, for the owner: after fix 1 the log is the un-notched collar, i.e. the notch volume is restored on the log (bark intact at the mouth); that is the concept's read (the chopped face belongs to the stump, the butt is a torn disc) but it is a design choice to confirm.
