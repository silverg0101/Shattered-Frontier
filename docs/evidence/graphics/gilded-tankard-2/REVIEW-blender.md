# The Gilded Tankard — Blender-stage review (independent judge)

Concept: `docs/Game-Design/concept-art/Silverbrook/The Gilded Tankard.png` (front elevation + 3/4).
Renders: `GildedTankard-{Beauty,Front,Side,Rear}.png`, 1100 px, Eevee. Front/Side/Rear are ortho at 14.6 m → 75.3 px/m.
Accepted as stated: no front dormer, flat tankard silhouette, barrels without hoops, inferred side/rear.

Measurements (concept front elevation, building width 755 px = 9.2 m → 82 px/m):

| Quantity | Concept | Model (Front.png) |
|---|---|---|
| Wall height (ground → eave) ÷ width | 658/755 = 0.87 (≈ 8.0 m) | 430/715 = 0.60 (5.7 m; EAVE=6.0) |
| Roof height ÷ wall height | 187/658 = 0.28 (3/4 view 0.54) | 373/430 = 0.87 (RIDGE−EAVE = 4.7) |
| Ridge length ÷ eave width | 630/820 = 0.77 | 325/770 = 0.42 (RX=2.0 of EX=5.0) |
| Stone storey ÷ wall height | 330/658 = 0.50 | 265/430 = 0.62 (STONE_TOP=3.6 of 6.0) |
| Upper window w × h | 95×135 px = 1.15 × 1.65 m | 60×90 px = 0.80 × 1.20 m |
| Sign board w × h | 90×140 px = 1.05 × 1.65 m, bottom at storey line | 0.80 × 1.00 m, straddles storey line (3.2–4.2 vs STONE_TOP 3.6) |
| Jetty / corbels under upper storey | yes, 7 corbels + 0.3 m beam | none (0.2 m flush rail) |
| Upper-storey posts (front) | 7 verticals | 2 (corners only) |

## Beauty — FAIL
Reads as a plain stone-and-plaster house with an oversized hip roof. Not recognisable as the concept: the roof is ~half the silhouette (concept ~1/3), there is no jetty, the upper storey is bare plaster between two corner posts, and the sign is a 0.8 m dark shingle with a yellow smear, not a shield board with a gilt tankard. Barrels, bench, lanterns and the arched double door are present and read correctly. Nothing floats; no default materials.

1. Roof dominates: ridge 4.7 m above a 6.0 m wall; walls squat (0.60 of width vs 0.87). CODE, fix 1.
2. Ridge too short (42 % of eave width vs 77 %): the hips read as a pyramid. CODE, fix 2.
3. No jetty beam or corbels at the storey line. CODE, fix 3.
4. Upper storey framing too sparse (2 posts, no sill rail). CODE, fix 4.
5. Sign board ~1/2 the concept's area and hung ~1.1 m too low. CODE, fix 5.
6. Slate carries orange-brown lichen on ~50 % of tiles; concept slate is blue-grey with faint variation. CODE, fix 6.
7. Shutters are bleached pale planks (M['shutter']=M['door']); concept shutters are dark oak like the frame. CODE, fix 7.

## Front — FAIL
Layout matches (door centred under round arch, two windows, lanterns at ±2.3 m, barrels left, bench right, chimney right-rear) but proportions do not: wall 5.7 m vs ~8 m, roof 0.87 of wall vs 0.28–0.54, windows 0.8 m vs 1.15 m wide, stone storey 62 % vs 50 %. Sign board overlaps the arch crown in elevation (board 3.2–4.2 m, arch top 3.5 m). A dark blob sits on the right door leaf (x 590–640, y 850–900 px): it is the sign board's sun shadow, which lands on the door because the sign hangs too low; fix 5 moves it onto the plaster. The right barrel (x −2.0 m, r 0.4) clips the step box corner (step x ±1.7, y to −D2−0.45) by ~0.1 m; the bench's left leg (x 1.61–1.69, y −4.33..−3.97) clips the same step by ~0.08 m. Stone blocks are ~0.33 × 0.16 m coursed rubble vs ~0.55 × 0.35 m rough ashlar.

1. Proportions (wall height, roof height, stone split, window size). CODE, fix 1.
2. Ridge length. CODE, fix 2.
3. Jetty missing. CODE, fix 3.
4. Framing sparse. CODE, fix 4.
5. Sign size/height and the shadow on the door. CODE, fix 5.
6. Lichen on slate. CODE, fix 6.
7. Shutter and door tone (door is mid-tan with hairline straps; concept is dark oak with heavy black straps). CODE, fix 7.
8. Barrel and bench clip the step. CODE, fix 9.
9. Stone block scale. CODE, fix 8.

## Side — FAIL
Inferred view; nothing view-specific is broken (chimney, side window, projecting sign and lantern all sit correctly, barrel stands on the ground). Fails on the shared proportions only: the hip end is a 373 px slate triangle over a 440 px wall, so the roof is taller than it is in the concept 3/4 (where the hip end is ~0.54 of the wall). Fixes 1 and 2 cover it.

## Rear — FAIL
Inferred view; rendered in shadow, nothing broken (two windows, chimney on the back slope, side-window shutters visible edge-on at the corners). Fails on the shared proportions (roof/wall 0.85, ridge 42 %). Fixes 1 and 2 cover it. The chimney top is only ~0.3 m above the ridge (concept ~0.9 m + pot); fix 10.

## Concept-to-model — FAIL
A player would say "stone-and-plaster house" not "the alehouse from the concept". What is right: two storeys, ashlar below / plaster above, wide round-arched double door with dressed surround and step, two shuttered leaded windows, hanging sign on an iron bracket, two lanterns, two barrels, bench, slate roof with stone chimney and clay pots. What breaks recognition: squat walls under a roof almost as tall as the walls, no jetty, bare upper storey, a sign too small and too low to read as a tankard board, rust-blotched slate, pale shutters and door. Distinct from an ordinary house only by the arch and the sign; the sign must be the size the concept gives it to carry that.

## Fix list (deduplicated, shared fixes first; all in build_gilded_tankard.py unless noted)

1. Proportions — line 44: `EAVE,RIDGE=7.4,11.0`; line 47: `STONE_TOP=3.7`; line 50: `XDOOR,DWID,DS,STEP=0.0,2.6,1.8,.2` (arch top 3.3 m, voussoir ring stays under the jetty); line 51: `WIN,WH,WZ=1.15,1.6,4.6`. Result: wall 7.4 m (0.80 of width), roof 3.6 m (0.49 of wall, 42° front pitch), stone 50 %, windows 1.15 × 1.6 at 4.6–6.2 m. Total height 12.6 m, inside the 14 m slot.
2. Ridge length — line 45: `EX,EY,RX=W2+.4,D2+.4,3.4` (ridge 6.8 m = 68 % of eave width; hip edge ≈ 71° as in the elevation).
3. Jetty — replace line 97 with:
   `G['timber'].beam((-W2-.2,-D2-.18,STONE_TOP+.15),(W2+.2,-D2-.18,STONE_TOP+.15),.36,.3,M['timber'],band=band())`
   and add after it:
   `for x in (-4.0,-2.6,2.6,4.0):G['timber'].box((x,-D2-.2,STONE_TOP-.22),(.2,.4,.44),M['timber'],.012,grain=True,band=band())`
   (corbels kept clear of the arch voussoirs at |x| < 1.7).
4. Framing — line 96: `for x in (-4.25,-1.1,1.1,4.25):post(x,y,STONE_TOP,PLATE-.1,True)` (inner posts flank the sign bracket, clear of the open shutters whose inner edge is at |x| ≈ 1.35); add after line 98: `rail((-W2,-D2-.04,WZ-.17),(W2,-D2-.04,WZ-.17),.12)` (sill rail 2 cm under the shutter bottoms).
5. Sign — lines 108–112: replace every `4.45` with `5.9`, `3.95` with `5.4`, `4.2` with `5.65`. Line 113: `board=[(-.52,5.65),(.52,5.65),(.52,4.6),(0,4.0),(-.52,4.6)]` (1.04 × 1.65 m, bottom 0.3 m above the jetty beam). Line 115: `tank=[(-.2,4.35),(.2,4.35),(.2,4.5),(.38,4.5),(.44,4.7),(.44,5.0),(.38,5.2),(.2,5.2),(.2,5.35),(-.2,5.35)]` (tankard 0.64 × 1.0 m).
6. Slate lichen — line 57: `def lichen(p):return kit.rng.uniform(.15,.45) if kit.rng.random()<.12 else 0`.
7. Oak tone — line 25: `door=kit.scan('SB_DoorBoards','old_planks_02',dark=(.22,.17,.12),aged=.5),`; line 34: `M['shutter']=M['timber']` (keep `M['ashlar']=M['stone'];M['planks']=M['timber']`). Door straps (kit line 322, 0.05 m tall) are left to the kit; if still invisible after re-render, raise strap height there to `.09` (kit change, separate commit).
8. Stone block scale — add after line 34: `kit.SCALE['SB_Ashlar']*=1.6` (blocks ≈ 0.53 × 0.26 m).
9. Street clips — line 119: `(2.7,BY,.45)`; line 120: `for lx in (2.7-.75,2.7+.75)`; line 121: `for bx,by in ((-3.0,-D2-.8),(-2.25,-D2-.6))`.
10. Chimney after the roof change — line 89: `(XC,YC,10.0),(.9,.7,4.0)`; line 90: `(XC,YC,12.05)`; line 91: `(XC+dx,YC,12.1)` (stack 8.0–12.0 m, 1.0 m above the ridge; pots top 12.57 < 14 m).
11. Eave corbels (concept shows a row under the front fascia) — add after line 77: `for x in (-4.0,-2.6,-1.3,0,1.3,2.6,4.0):G['timber'].box((x,-D2-.2,EAVE-.36),(.16,.4,.3),M['timber'],.012,grain=True,band=band())`.
12. Chimney position (concept stack is at the right side end, not mid back slope) — line 49: `XC,YC=3.9,1.5` (stack 3.45–4.35 m stays inside W2=4.6; `in_stack` follows). Lowest priority.

Re-render all four views after fixes 1–10 and resubmit for a fresh judge.
