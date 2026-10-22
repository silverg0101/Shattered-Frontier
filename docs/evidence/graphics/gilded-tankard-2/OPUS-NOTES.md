# Gilded Tankard: Opus takeover notes

Values and methods that the blueprints (`docs/blueprints/buildings.md`, `README.md`) got wrong or left out, as settled in this pass. `B` = `GameClient/ArtSource/Silverbrook/GildedTankard/build_gilded_tankard.py`, `K` = `GameClient/ArtSource/Silverbrook/silverbrook_kit.py`. Line numbers are at the Blender-stage PASS commit.

## Review log
| Round | File | Verdict | Implemented |
|---|---|---|---|
| r1, r2 (Haiku builder) | `review/REVIEW-blender.md`, `-r2.md` | 0/5 PASS | r2 fixes 1-9, with the corrections below |
| r3 | `review/REVIEW-blender-r3.md` | 4 PASS / 1 FAIL (chimney inside the right-side window) | fixes 1-6 |
| r4 | `review/REVIEW-blender-r4.md` | 5 PASS / 1 FAIL (Front: two strap systems on the door) | fixes 1-5 (fix 1 needs the kit opt-in) |
| r5 | `review/REVIEW-blender-r5.md` | 6 PASS / 0 FAIL | non-blocking items 1, 2 and 4 applied after the PASS; item 3 (true two-leaf door, needs `K` `door(leaves=2)` plus `split_doors` hinge flag) deferred |

## Roof (gable) and ridge formula
- Gable on the Blacksmith convention: `EX,EY,RX=W2+.4,D2+.4,W2+.4` (B:46). The ridge half-length `RX` equals `EX` for a gable; `KF=(RIDGE-EAVE)/EY`, `KS=1e9` (B:47) so `main_z` (B:56) has no hip slope inside |x| <= RX.
- The blueprint's `RIDGE = EAVE + (W2+.4)*tan(pitch)` is wrong for a gable whose ridge runs along x. The rise is over the **front-to-back** half-depth: `RIDGE = EAVE + EY*tan(pitch)`. Here `EAVE 7.4`, `EY 4.0`, 42 deg -> `RIDGE 11.0` (B:45). The blueprint's "`RX = W2 + .4`" line is the eave overhang (`EX`), not the ridge half-length.
- Only two `plane()` calls (front and rear, B:71-72); delete the side planes. Gable ends get barge boards `beam((sx*(EX+.02),sy*EY,EAVE-.07),(sx*(EX+.02),0,RIDGE-.07),.05,.24)` (B:78-79) instead of hip caps.
- Gable triangle: `prism` at `s*(W2-.15)` thickness .3 (B:105) is already flush with the `wall()` face (`Geo.prism` is centred on its points; `wall()` builds inward from the outer face). The r2 instruction `s*W2-(.3 if s>0 else 0)` would have made one gable 15 cm proud and the other recessed; not applied.
- Roof share: the reviewer accepted 0.36 of the width against the concept's 0.22, because the §3 depth floor (D2 >= 0.6 x SLOT d / 2) plus pitch >= 40 deg forces a rise >= 3.36 m. Write that into §3 as an expected deviation for narrow-fronted, deep concepts.

## Footprint ratios
- `W2,D2=4.6,3.6` (B:44): 0.66 and 0.60 of SLOT 14 x 12. The §3 example "Tankard 4.5/3.2" (0.53 deep) does not exist and contradicts the 0.6-0.8 rule; delete it. Final bounds 10.585 x 8.925 x 12.07 m, 51.6 k triangles.

## Two-storey fronts
- Storeys: ashlar `STONE_TOP 3.7` (B:49); plaster storey to `PLATE=main_z(0,-D2)-.04` = 7.72 (B:57).
- Upper windows: `WIN,WH,WZ=1.15,1.6,4.6` (B:53), sill 0.9 m above `STONE_TOP`. §3's "sill 1.0-1.3 m" applies to the ground storey only; add "upper storey: sill = STONE_TOP + 0.9, plaster opening `WO` relative to STONE_TOP" (B:54).
- Door: `DS` spring 1.5 (B:52), not §3's 2.0. Rule: the voussoir top `STEP+DS+DWID/2+.30` must leave >= 0.3 m of ashlar under the jetty soffit (`STONE_TOP-.06`); with DS 1.8 it left 4 cm (r3 fix 4).
- Sign on a two-storey front (r3 fix 2, B:143-157): arm at **z 5.45** (upper-window mid-height), 1.2 m long; wall plate 6x10x100 cm centred 0.4 m below the arm; strut from 0.85 m below the arm with its foot 5 cm **inside** the plaster face (`-D2+.05`), never ending at the face (it floated); 8-segment scroll r .17 centred 0.5 m out and 0.2 m below the arm; board 0.8 x 1.15 m shield bottom 4.05. §5's "3.45-3.75 m, above the door head, below the upper sills" is only for one-storey fronts; on two storeys the band between the door head and the upper sills is the jetty.
- Emblem (B:154-157): body + lid + foot + open D handle as separate abutting polygons (never overlapping, or the coplanar faces fight), each a 2 cm prism at `SY +- .035`. Body ~50 % of the shield width (r5 item 2); a solid silhouette with the handle reads as a blob, a narrow body reads as a letter.

## Jetty, corbels, framing
- Jetty (B:48, r2 fix 2): `JET=.45; DF=D2-JET`. Only the **front ashlar** moves back to `-DF`; the plaster storey stays at `-D2`. Side stone walls `wall(...,(s*W2,JET/2,0),(s,0,0),D2-JET/2,...)`. Everything at street level uses `-DF`: door, step, bench, barrels, lanterns (B:132-160).
- Jetty beam 0.36 x 0.30 at `STONE_TOP+.15`, `y=-D2-.18` (B:119); corbels `box((x,-D2+.1,STONE_TOP-.22),(.2,.7,.44))` reach from the ashlar face to under the beam (B:120); x at +-4.5 (corners) and +-2.6 (clear of the arch: radius + 0.3 voussoir < 2.6-0.1); soffit `box((0,-D2+JET/2,STONE_TOP-.03),(2*W2+.4,JET,.06))` closes the underside (B:121).
- Framing on all four walls (B:115-127): posts 0.2 x 0.13 from `STONE_TOP` to `PLATE-.1`; front and rear at x +-4.5 (corner) and +-1.1; sides at y +-3.5 and +-1.5. Check posts against open shutters: a window of width w puts shutters out to `w/2+t+sw` = 1.345 m for w 1.15 (K:303-305), so the r2 side posts at +-1.2 sat on the shutters. Sill rail 0.13 x 0.12 at `WZ-.17` and plate rail at `PLATE-.3` on every wall.
- Eave corbels must touch the fascia: `box((x,-D2-.17,EAVE-.33),(.16,.4,.3))` (B:82); at `EAVE-.36` a 2 cm sky slit showed. Keep their x 0.2 m clear of the posts (+-1.5, not +-1.3).

## Dormer recipe (centred front dormer on a gable)
- Use the Blacksmith dormer (Blacksmith `build_blacksmith.py` lines 128-141), not a solid box: B:59-61 `YD=-2.8; DB=main_z(0,YD)-.1; DH,DW=.82,.7; DE=DB+1.35; DR=DE+.75`; `in_dormer` covers main-slope slates under the dormer roof; the dormer planes use `[under_main]` and run back to `YR=-(RIDGE-DR)/KF`, where the dormer ridge meets the main slope; front `wall()` with a window opening, gable prism, cheek prisms back to `ym`, posts and bressumer (B:83-96). The r2 box version left the dormer ridge 1.1 m above the main slope at its back and buried the glass inside solid plaster.
- Window 0.6 x 0.8 (concept: about a third of the dormer width), `shutters=False`, `wall_th=.14`.

## Chimney on a gable wall
- Stack against the gable wall, 1 cm into it: `XC=W2+.44`, on the **rear** half `YC=2.2` (B:51) so it clears the side window and the post at y 1.5; `in_stack` on the rear plane only (B:63, B:72), y half-width .6 for a 1.1 m stack. Top `CT=RIDGE+.5`, one pot (B:107-110). §3's "stack 0.9x0.6 m, two pots" is for ridge stacks only.

## Door ironwork and the kit opt-in
- `K` `door(..., straps=True)` (K:314, K:321): `straps=False` drops the kit's two full-width ledges so a package can lay per-leaf straps. Default unchanged. Verified by rebuilding the Blacksmith: bounds `[12.391, 9.055, 13.39]`, 80 160 triangles, every mesh count identical before and after (Masonry 1380, Plaster 844, Timber 4236, Roof 62008, Details 11416, Door1 276); the rebuilt outputs were reverted with `git checkout`.
- Package ironwork goes on the leaf mesh `G[f"{G['detail'].name}_Door1"]` so it travels with the door part (B:133-140): straps 1.0 x .12 at `|x| = DWID/2-.55`, z `STEP+.55` and `STEP+1.65`, `y=-DF+.16`; second ring mirrored; meeting stile 6 x 2 cm at x 0.

## Ashlar scale knob
- No kit parameter: the package multiplies the kit's measured-size table, `kit.SCALE['SB_Ashlar']*=1.6` (B:35), before any geometry. Safe because each build is its own Blender process; it changes only this package's UV scale for `SB_Ashlar`. The blueprint should name it as the per-package knob (`kit.SCALE[<material name>] *= f`, set right after `M`).

## Review captures
- Ortho cameras z 6.6, `ortho_scale 15.4` (B: `cam(...)` line near the end) keep the ground and barrels inside the frame. Add a fifth view `SideR` (`cam('SideR',(40,0,6.6),(90,0,90),15.4)`): the r3 blocker (chimney through the right window) was invisible in the four standard views. `review_gilded_tankard.py` turns the sun to yaw 145 deg for Rear and SideR (otherwise in full shadow).

## Registration (missing_assets.py)
- See the Unreal-stage section below (filled in when the dev map and README row are done).
