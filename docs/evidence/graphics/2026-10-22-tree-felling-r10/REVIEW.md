# Tree felling r10 — independent review (2026-10-22)

Reviewer: Fable, no part in the build. Concepts: `GameClient/ArtSource/HarvestOak/MightyOak-Felling-v1/Concept/{sequence,stump,log}`. Captures: `Artifacts/HarvestArt/felling-review-10/{level,slope}` (1920x1080) with `client.log` / `server.log` / `results.json`. Every pair, strip and raw frame was read; x2-x6 crops with a 50 px grid, pixel counts and masks were taken from the raw frames (and the r9 raw frames for comparison) with a script in the reviewer's scratchpad (not committed). Cut-wood filter as r6-r9: R>170, R-B>90, G-B>45, B<140.

Scope as briefed: bare slab and ground tiling are fixture artifacts; the short forked oak is the shipped art; fall direction and the un-notched log are owner decisions; physics varies per run; one split surface and the prism cutter stay. The bar is the r9 bar. r9 fix 1 is checked against its acceptance; fixes 2 and 3 were not gates. Passing views are re-checked for regression.

Result: **8 PASS / 0 FAIL.** r9 fix 1 (upper structure join cap given the bark material at runtime, README item 1) removes the only open item: the cut-wood triangles on the log's bark beside the butt in `09` are gone on both maps, **587 -> 0 px (level) and 321 -> 0 px (slope)** in the r9 boxes, and 0 px on the bark anywhere left of the butt by mask (the only filter hits in the wider boxes are the butt disc's own rim column at x 720-740 slope, confirmed on the mask image). This also confirms r9's diagnosis: the marks were the join cap, not the split. r9 fix 2 is done (`TREE_SPLIT_PROFILE_REJECTED` lines appear on both maps with reasons). No regression in the six views that passed in r9.

Measured facts:

| | level | slope |
|---|---|---|
| FALL_STARTED -> CROWN_YIELDED -> FALL_RESTING (server) | 2.66 s, then 2.70 s | 2.65 s, then 2.76 s |
| `HARVEST_REVIEW_NOTCH_VIEW` / `HARVEST_REVIEW_SUN` / "has to be 'Movable'" | `RadialToViewDeg=40.0`; `P=-30 Y=53.95`; 0 | same; `Y=54.12`; 0 |
| `TREE_SPLIT_SURFACE` / rejections | attempt 0 rejected ("Complementary torn cap vertices did not match"), attempt 1 accepted; 42.5 / 75.5 / 1.000 / 17.6 | attempts 0 and 2 rejected (cap vertices), attempt 1 rejected (`closed=0 ... signed=-37444`), attempt 3 accepted; 42.0 / 76.0 / 1.000 / 20.0 |
| Warnings `HARVEST_TREE_*_REJECTED` | none | none |
| Notch in `00-notched` | lit quad ~17 x 34 px at x 969-985, y 910-943 on the trunk's right third, chip specks beside it (253 px incl. specks; r9 213) | quad x 966-994, y 907-946 (221 px; r9 146) |
| Cut-wood on the log's bark beside the butt, `09` (r9 boxes) | **(640-700, 220-300): 0 px** (r9 587); (600-650, 100-175): 0 px; whole bark left of the butt (560-750, 60-330): 2 px at the box corner (ground) | **(625-680, 375-430): 0 px** (r9 321); (495-540, 290-335): 6 px (ground speck, r9 3); bark (400-730, 60-600): every hit is in the butt's rim column x 720-740 or the stump at y 500-600 |
| Butt face, `09` | disc x 750-1100, y 0-290: ringed end-grain plane y 0-190, shadowed wall band y 190-235 (now dark cut-wood tone; r9 pale side grain), lower end-grain plane y 235-290 with its contact shadow; no multi-facet recess | disc x 735-985, y 55-345: one ringed plane y 55-255, two shadowed tear pockets (740-790 and 875-990, y 255-320), lit end grain between and below |
| Stump top, `09` | shelf: one ringed end-grain plane x 700-1100, y 555-700, clean; rear raised block top edge y 330 (x 1030) to 400 (x 720), front wall one plane with vertical side grain (as r9) | shelf y 590-750, ringed, clean; rear wall y 450-600 with three prism shards, tips at (780-880, y 450) and (960-1070, y 490) |
| Butt and bark, `10-log-close` | butt x 1053-1176 (disc ~95 px plus lower facet), y 354-541: two end-grain planes, diagonal boundary y ~420, dark wall facet 1115-1176, 505-541; bark band (1160-1300, 340-420): **5 px** at the disc edge (r9 11) | butt x 1223-1339, y 391-574, ringed plane with two dark tear pockets at (1240-1300, 500-560); bark band (1330-1650, 380-620): all 1691 hits are the disc's own rim at x 1330-1360 (mask), **0 on bark** |
| Stump in `10` | in frame x 660-900, y 530-820, shelf and shard visible | in frame x 390-660, y 540-820 |

---

## level-sequence — PASS

No regression. Frame 1: lit cut-wood quad on the trunk's right third at the notch rows with chip specks. Hinge holds through 22 deg with the butt on the stump; at 50 deg the butt is on the slab beside the stump with the crown touching; at rest the trunk lies flat past the stump with the crown crushed.

## slope-sequence — PASS

As level: 28 x 39 px quad at x 966-994, y 907-946; butt beside the stump at 50 deg, trunk downhill at rest, crown crushed.

## level-stump — PASS

r9 item 1 resolved: 0 cut-wood px on the log's bark within the r9 boxes and nothing by eye at x3/x6 in the 60 cm band left of the rear butt face; `10` bark band at 5 px (r9 11); shelf clean, rear still raised. The butt is a lofted step (plane, one wall, plane), the stump is a low chopped shelf with a raised torn rear, as the concept's composition.

Not gating, for the record: at x6 a pale bark-textured sliver ~65 x 27 px stands proud of the butt's right rim at (1083-1148, 180-207), seen against the shadowed ground. It is the bark-material cap rim r9 fix 1 predicted would be hidden against the collar; here it overhangs the butt silhouette. It reads as a bark lip at the cut, not as a broken face. Durable removal is fix 1 below. Also unchanged and not gating: the step wall and the stump's rear wall show side grain where the concept shows fibres.

## slope-stump — PASS

r9 item 1 resolved: 0 px in the r9 box, every remaining filter hit in the wider box is the butt disc's own rim column or the stump (mask checked); x6 crop of the butt's right rim is clean. Butt: one ringed plane with two tear pockets; shelf clean; three prism shards on the rear.

## level-log — PASS

No regression: butt on the ground, trunk flat with its contact shadow, bark continuous into the upper structure, stump in frame with shelf and shard, crown crushed; bark band 5 px (r9 11). Butt ~95 px disc (r9 fix 3, framing only, not done; not a gate).

## slope-log — PASS

No regression: 0 cut-wood px on bark, stump in frame, butt 116 px wide on the ground with its tear pockets, trunk flat downhill with contact shadow, crown crushed, bark continuous.

## level-frames (motion strip) — PASS

No regression: notch visible from 00; lean 2.1 / 10.1 / 22.2 / 36.2 deg progressive with the butt on the stump through 22 deg; breaks by 36; butt on the slab beside the stump by 50 and stays; crown contact from ~50; flat by rest (98.4 deg). Crown yield 2.66 s after FALL_STARTED, rest 2.70 s after the yield. Nothing floats, no roll.

## slope-frames (motion strip) — PASS

The repeated run (README) is continuous: 2.1 / 10.1 / 22.0 / 37.0 deg, then the butt lands beside the stump by 50 and the trunk flattens downhill (rest at 116.3 deg); rest 2.76 s after the yield with the crown crushed downhill. No skate, nothing floats.

---

## Fix list

None gating. Carried, not gates:

1. **Engineer — durable version of r9 fix 1.** The runtime `SetMaterial(1, bark)` in `ASFHarvestFallingTree::PrepareVisualSource` is the fix; make it survive a re-import: `GameClient/ArtSource/HarvestOak/MightyOak-v1/tools/Prepare-HarvestMightyOak.py` ~250-262 give the join-cap faces `material_index = 0`, update `manifest.json` `join_cap_material_index`, re-import with `tools/Update-HarvestOakFellingArt.py`. While there, inset the cap polygon 2-3 cm inside the lower trunk's 24-gon so no rim stands proud of the collar (removes the level `09` bark sliver at (1083-1148, 180-207)). Keep the split code, surface and cutter untouched.
2. **Engineer — framing only, carried from r8/r9: level `10-log-close` butt >= 120 px.** `SFHarvestArtReviewScenario.cpp`, level map only: FOV 50 deg or Side 400 cm, stump kept in frame.
3. **Engineer — not gating: slope attempt 1 rejects on `closed=0`.** The seed's second profile produces an open result (`signed=-37444`); with attempts 0 and 2 also rejected the slope relies on attempt 3 of 6. If a future seed exhausts the profiles the fall will reject with only a Warning. Log the chosen attempt index beside `HARVEST_TREE_FALL_STARTED` so the review can see the margin per run.

Owner flags carried, not review items: the tree falls away from the notch face (`SFHarvestPresentation.cpp` `TreeSplitFor`); the notch reads as a 17 x 34 px mark at the review distance on the shipped flared trunk; the log is the un-notched collar with the notch volume restored on it (bark intact at the mouth).
