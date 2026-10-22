# Gilded Tankard: Opus takeover notes

Values and methods the blueprints (`docs/blueprints/buildings.md`, `README.md`) got wrong or left out, as settled in this pass. `B` = `GameClient/ArtSource/Silverbrook/GildedTankard/build_gilded_tankard.py`, `K` = `GameClient/ArtSource/Silverbrook/silverbrook_kit.py`, `P` = `GameClient/ArtSource/Silverbrook/silverbrook_props.py`, `I` = `GameClient/ArtSource/Silverbrook/import_silverbrook.py`, `M` = `GameClient/ArtSource/Greenlands/missing_assets.py`. Line numbers are at the final commit.

## Review log
| Stage, round | File | Verdict | Implemented |
|---|---|---|---|
| Blender r1, r2 (Haiku builder) | `GildedTankard/review/REVIEW-blender.md`, `-r2.md` | 0/5 PASS | r2 fixes 1-9, with the corrections below |
| Blender r3 | `review/REVIEW-blender-r3.md` | 4/1 (chimney inside the right window) | all 6 |
| Blender r4 | `review/REVIEW-blender-r4.md` | 5/1 (two strap systems on the door) | all 5 |
| Blender r5 | `review/REVIEW-blender-r5.md` | **6/0 PASS** | polish items 1, 2, 4 applied; 3 (true two-leaf door) deferred |
| Engine r1 | `engine/REVIEW-r1.md` | 3/8 | 2-7, 9; fix 1 replaced (see Foundations); 8 conditional, not needed |
| Engine r2 | `engine/REVIEW-r2.md` | 8/3 | both |
| Engine r3 | `engine/REVIEW-r3.md` | 4/7 (chimney floats, double riser, flat emblem) | all 4 |
| Engine r4 | `engine/REVIEW-r4.md` | 9/2 (faceted lemon emblem) | all 3 |
| Engine r5 | `engine/REVIEW.md` | **11/0 PASS** | empty list |

## Roof and ridge (gable)
- Gable: `EX,EY,RX=W2+.4,D2+.4,W2+.4` (B:46); `KF=(RIDGE-EAVE)/EY`, `KS=1e9` so `main_z` has no hip slope inside |x| <= RX. Only front and rear `plane()` calls; gable ends get barge boards `beam((sx*(EX+.02),sy*EY,EAVE-.07),(sx*(EX+.02),0,RIDGE-.07),.05,.24)` (B:79).
- Formula fix: for a ridge along x the rise is over the half-**depth**: `RIDGE = EAVE + EY*tan(pitch)` (7.4 + 4.0 x tan 42 deg = 11.0, B:45). §3's `EAVE + (W2+.4)*tan(pitch)` is wrong, and its "`RX = W2 + .4`" is the eave overhang `EX`, not the ridge half-length.
- Gable triangle `prism` at `s*(W2-.15)`, thickness .3 (B:105), is flush with the `wall()` face: `Geo.prism` is centred on its points, and `wall()` builds inward from the outer face. The r2 suggestion `s*W2-(.3 if s>0 else 0)` would have made one gable 15 cm proud.
- Accepted deviation: the roof is 0.36 of the width against the concept's 0.22, because the §3 depth floor (D2 >= 0.6 x d/2) plus pitch >= 40 deg forces a rise >= 3.36 m.

## Footprint and storeys
- `W2,D2=4.6,3.6`: 0.66 / 0.60 of SLOT 14 x 12. The §3 "Tankard 4.5/3.2, 34 k, 2.6/2.0/0.2" examples never existed; final bounds are 10.585 x 9.11 x 12.07 m, 53.1 k triangles.
- Ashlar `STONE_TOP 3.7`; upper windows `WIN,WH,WZ=1.15,1.6,4.6` (B:53), with the sill 0.9 m above `STONE_TOP`. §3's "sill 1.0-1.3 m" applies to the ground storey only.
- Door `DS 1.7, STEP 0` (B:52): the crown is 3.0 m and leaves 0.34 m of ashlar under the jetty soffit (rule: voussoir top `STEP+DS+DWID/2+.30` at least 0.3 m below `STONE_TOP-.06`). **Bake no doorstep**: the footing pass builds the stoop to the wall base, and a baked step doubles the top riser (engine r3).
- Sign on a two-storey front (B:143-157): arm at z 5.45 (upper-window mid-height), 1.2 m long; wall plate 6x10x100 cm, 0.4 m below the arm; strut foot 5 cm inside the plaster (`-D2+.05`); 8-segment scroll r .17; shield 0.8 x 1.15 m with its bottom at 4.05. §5's 3.45-3.75 m bracket applies to one-storey fronts only.
- Emblem: a round `vessel` through the board (relief on both faces), `segments=96` because `Geo.object` never welds and `smooth=True` alone leaves facets; domed lid; D handle as five convex quads (B:156-157). Gilt `kit.solid((.60,.38,.09),.3,.6)` (r2's `.85` metallic mirrored the sky and read olive, `(.90,.66,.22)` read lemon).

## Jetty, corbels, framing
- `JET=.45; DF=D2-JET` (B:48). Only the front ashlar moves back to `-DF`; the side stone walls are `wall(...,(s*W2,JET/2,0),(s,0,0),D2-JET/2,...)` (B:102). Street-level items use `-DF`.
- Jetty beam 0.36 x 0.30 at `STONE_TOP+.15`, `y=-D2-.18` (B:119); corbels `box((x,-D2+.1,STONE_TOP-.22),(.2,.7,.44))` at x +-4.5 / +-2.6 (B:120); soffit `box((0,-D2+JET/2,STONE_TOP-.03),(2*W2+.4,JET,.06))` (B:121).
- Framing on all walls (B:115-127): posts 0.2 x 0.13, front/rear x +-4.5 (corners) and +-1.1, sides y +-3.5 and +-1.5. Keep posts clear of open shutters, which reach `w/2+t+sw` = 1.345 m for w 1.15 (K window). Eave corbels must reach the fascia (`EAVE-.33`, B:82) and stay 0.2 m clear of posts.

## Dormer (centred, on a gable)
- Use the Blacksmith recipe (`build_blacksmith.py` 128-141), not a solid box: `YD=-2.8; DB=main_z(0,YD)-.1; DH,DW=.82,.7; DE=DB+1.35; DR=DE+.75` (B:59); `in_dormer` covers main slates; the dormer planes use `[under_main]` and run back to `YR=-(RIDGE-DR)/KF` (B:83-96). Window 0.6 x 0.8, `wall_th=.14`, no shutters.

## Chimney on a gable wall
- `XC=W2+.44` (1 cm into the wall), `YC=2.2` on the rear half, clear of the side window (B:51). `in_stack` y half-width .6, on the rear plane only. Top `CT=RIDGE+.5`, one pot (B:107-110). §3's "0.9 x 0.6, two pots" is for ridge stacks only.
- An external stack stands outside the wall-base footing, so it needs its own footing: manifest `footings_cm` (B:209), built by `P:636` as `<House>_Footing<k>`.

## Shared-code changes (all opt-in; other buildings unchanged)
- `K:314/321` `door(..., straps=True)`: `straps=False` drops the kit ledges so a package lays per-leaf straps on `G[f"{G['detail'].name}_Door1"]` (B:133-141).
- `K:337/347` `lantern(..., cage=True)`: 3.5 cm bars, inset glass, cone roof.
- Blacksmith regression after each kit change: bounds `[12.391, 9.055, 13.39]`, 80 160 triangles, every mesh identical; outputs reverted.
- Package-local materials instead of shared ones: `SB_GildedTankard_LitGlass` strength 1.2 and `SB_GildedTankard_LanternGlow` 1.5 (B:29-30), because changing `SB_LitGlass` would rewrite the shared `MI_SB_LitGlass`.
- `I:159` `nanite_fallback_percent` (manifest): `-game -RenderOffscreen` captures draw the Nanite **fallback**, and the Auto fallback (15 k of 51 k triangles) erased the flat emblem. 1.0 keeps the full mesh. Check other packages' small details in captures for the same loss.

## Foundations and street props
- The footing pass lifts the house 65 cm, so anything baked at z 0 outside the walls floats. Barrels and bench are separate `static` parts (B:160-170, exported with `kit.export_parts` at each group's base centre, B:184-187); `place_parts.attach` places them, and `P:391` drops `static` parts to the terrain. All house loops skip `static` actors.
- Manifest `base_cm` (B:210): the ashlar wall-base box after import (x as built, y negated, both minus the Blender recentring); `_base_box` uses it when present (P:532-544). Verified by tracing the imported faces (0.1 cm). The r1 "wall above within 20 cm" filter was rejected: it left this box unchanged (the eaves overhang the barrels) and moved the Adventurers' Guild, both vendors and the Granary.
- `tools/check_foundations.py` now calls `_base_box` and reloads `silverbrook_props`, because the editor keeps modules between Run-InEditor calls (a stale cache gave `proud_cm 649`).
- New `tools/check_ground.py --terrain <label>` (the WP6 copy hard-codes `SM_Silverbrook_Terrain`); it also checks `static` parts.
- Dev-map gate lines at PASS: `FOUNDATION_CHECK 1 failures 0 min_visible_cm 65.0 max_abs_gap_cm 0.0 proud_cm 4.0 4.0`, `SILL_PLANTERS 0 expected 0 GROUP_DETAIL_OVERLAPS 0 []`, `HIDDEN_IN_GAME_CHECK checked 0 visible_in_game 0 []`, `DEFAULT_MATERIAL_CHECK visible_in_town 26 default_or_graybox 0 []`, `DRESSING_GROUND {'checked': 18, 'floating': 0, ...}`.

## Dressing and dev map
- `P:435` `baked_sign`: no `ShopSign_` on a mesh with a `*SignBoard` slot (§5 "never both").
- `P:409` crops at y 25..125 cm, inside the fence L.
- `tools/Create-BuildingDevMap.py`: review sun yaw -135 (the town's 135 shades the +Y street face).
- Save the level explicitly after `silverbrook_props.py` (`LevelEditorSubsystem.save_current_level()`): the r1 washing line was counted but missing from the capture.
- To redo a dev map, close the editor and delete `Content/Frontier/Maps/Dev/<Name>/`: `EditorAssetLibrary.delete_asset` returned True while the file stayed on disk.
- `tools/capture_building.py`: six context views (two overviews, four seeded ground spots) per visual-review.md, and `sign-close` aims at manifest `sign_m` (B:211, Y negated on import) when no dressing sign exists.

## Capture and review hygiene
- Blender review: add `SideR` (`cam('SideR',(40,0,6.6),(90,0,90),15.4)`), ortho z 6.6 and scale 15.4; turn the sun to yaw 145 for Rear and SideR (`review_gilded_tankard.py`). The r3 blocker was invisible in the four standard views.
- Run git checkouts of shared `MI_SB_*` only after the editor PID has fully exited (wait ~15 s), or `unlink ... Invalid argument` leaves two files modified.

## Registration (missing_assets.py)
- A new kind must be in `LAYOUT_TYPES` (`'tavern': 'The Gilded Tankard'`, M:35) or `DEFINITION_TYPES`, plus `building_types.tavern` in `silverbrook-town-layout.json`, or the regenerated concept README drops its row. `_core()` (M:56) matches package folders to display names without a leading "The", which also fixed the Golden Stag row. Asset ID becomes `BLD-GreenlandsTheGildedTankard-01`. Running the script also rewrites batch 42 and `asset-catalog.md`; those were reverted here because the change was unrelated (three vegetation rows had landed).
- Town placement (§7: a `tavern` lot, look profile `have`, `KIND_BUILDING`) is the main editor lane's job and is not done.
