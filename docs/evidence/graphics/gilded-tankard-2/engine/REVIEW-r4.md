# The Gilded Tankard — Unreal stage review r4 (dev map `GildedTankard_Review`)

Judge: independent art director (did not build this). Inputs: concept `docs/Game-Design/concept-art/Silverbrook/The Gilded Tankard.png`; the eleven raw `<view>.png` (1920x1080) and their `-compare.png` pairs; `README.md`; `REVIEW-r1..r3.md`; `build_gilded_tankard.py` (lines 33, 51-52, 97-110, 133-159, 208-211), `asset-manifest.json`, `silverbrook_kit.py` (`Geo.poly` 205-210, `vessel` 239-244, `object` 245-255, `solid` 170-175), `silverbrook_props.py` (`dress_footings`, ivy 352-456); `docs/blueprints/buildings.md` §8/§18. Pixel coordinates are in the raw captures (x0-x1, y0-y1); colours are 0-255 sRGB means of the named region, luminance Rec.709.

Result: **9 PASS / 2 FAIL.** The three r3 defects are gone: the external stack now stands on a footing of the plinth's stone in every view that shows its base (three-quarter 1313-1397 x 950-990; overview-a 1175-1232 x 735-760; ground-1 1180-1273 x 840-880; ground-3 560-585 x 830-870; ground-4 790-860 x 830-870: a dark jointed course, no void, no pixel darker than the plinth beside it), the stoop is three equal risers of the plinth's rubble up to the door sill (`foundation-close` risers 55/50/50 px, top tread visible, nothing brighter than the ashlar), and the tankard is a turned vessel with a lit side and a shadow side (`sign-close` row y 450: 131 at x 770 rising to 212 at x 950). The stack's dark faces in `ground-1/3/4` and `rear` are shadow, not a shading fault: 48 vs 44, 38 vs 37, 38 vs 44 against the shadowed rear wall in the same frame.

One defect remains, in the building, none in the dressing:

- A. **The tankard is lemon, not gilt, and faceted.** `build_gilded_tankard.py` line 157 builds it with `Geo.vessel` at the default 24 segments; `Geo.poly` gives every quad its own four vertices and `Geo.object` never welds, so `smooth=True` has no shared vertex to average over and each face shades flat: in `sign-close` the body is 12 visible vertical bands about 24 px wide with luminance steps of 23, 17, 9 and 6 levels across 2 px at x 780, 802, 834, 874. The lit face (236,210,123) is 1.39x the plaster beside it (181,146,105); in `frontage` 191 vs 139 (1.37x), in `three-quarter` (223,195,113) vs (124,96,61) (1.97x). In the concept the tankard body (126,97,63) is the same luminance as the plaster (116,97,78) and more orange (R/G 1.30 against the live 1.12). It is the brightest surface on the facade after the lit glass and reads as a yellow plastic mug on a dark board. The material (line 33, `(.90,.66,.22)`, rough .3, metal .6) is 40 % bright-yellow diffuse under direct sun; r2 showed that raising metallic only mirrors the sky and striped floor (olive), so the fix is a darker, more orange base at the same metallic, plus enough segments that the facets vanish.

Verified against the code before grading: two front windows at x +-2.6, sign, dormer and door at x 0; the right-gable ground-storey light at model y -1.675 is the concept's side-wall light (r3 fix 4) and is clear of the stack (y 1.65-2.75); the stack footing is the manifest `footings_cm [[434.3, 524.3, -313.2, -203.2]]` consumed by `dress_footings` (opt-in; no other manifest has the key). Neither the plant at the front-right corner (the `IvyClump` prop at a plinth corner, `silverbrook_props.py` 454-456, 1.2 m, on the ground) nor the stoop's leftward stagger in `foundation-close` (perspective: the wall recedes to the right, so the treads step toward the camera down-left) is a defect.

## frontage — FAIL

1. Emblem (918-980, 110-220): lit face (217,191,111), luminance 191 against the plaster (166,135,98) at 139: 1.37x; the concept's tankard equals its plaster. Hue R/G 1.14 against the concept's 1.30. It reads as a lemon-yellow cut-out on the board, the brightest opaque surface in the frame. Defect A. ART, fixes 1-2.
2. Right as built: ashlar ground storey with the 65 cm plinth course under it (1500-1900, 870-910), dressed round arch with quoins, double door with two straps per leaf (y 650, 800) and ring plates mid-leaf (y 735), caged lanterns either side (860-920, 510-590; 1380-1440, 500-590) with amber glass, jetty beam on corbels, oak frame, shuttered lit leaded lights, bracket with scroll ring (1000-1060, 90-130), three stoop risers (990-1500, 870-990) of the plinth's rubble, barrels left (745-915, 780-940) and bench right (1300-1920, 980-1080) on the ground, ivy clump at the far corner.

## foundation-close — PASS

Three risers of equal height: tread edges at y 660, 715, 765, floor 815 (55/50/50 px, 21-23 cm), top tread at the door sill (1000-1500, 640-700), `Stoop0` visible, all of it the plinth's dark rubble (no surface brighter than the ashlar mean (98,81,61)); the plinth course runs continuously under the wall at the stoop's top-tread height; barrels (650-890, 540-760) and bench (1430-1920, 740-1000) stand on the floor at the wall foot; the door leaves end on the sill with no gap; lanterns are cages with a cap and amber glass.

## three-quarter — PASS

Massing, 42-degree gable, dormer centred under the ridge, stack on the right gable with cap and pot rising clear of the ridge, jetty on corbels, eave corbel row, two front and one gable shuttered lights, the new ground-storey light on the right gable (1120-1175, 745-840) as in the concept's 3/4 view, arched door with three stoop risers (700-830, 930-1000), barrels left, bench right, lanterns either side, ivy clump at the front-right plinth corner (1020-1110, 960-1060) on the ground, fence L (1240-1560, 960-1080) with its sprigs inside, posts on the floor. The stack now meets a dark jointed course of the plinth's height (1313-1397, 950-990) with no void. Noted, not failing this view: the emblem (670-710, 540-600) at (223,195,113) is 1.97x the plaster beside it; defect A, fixes 1-2.

## rear — PASS

Two shuttered lit lights with sill and plate rails, plaster flush on the ashlar, plinth course under the wall (520-1640, 900-950), washing line on two braced posts (760-1300, 720-1000) with five garments clear of the wall, ladder against the far corner (1640-1720, 640-940) with both feet on the floor, fence L past the right corner (390-500, 820-890). The stack is edge-on at the left (490-540, 60-940) and its strip (41,38,33) matches the shadowed wall beside it (47,44,36); the r3 grass notch under it is gone.

## sign-close — FAIL

1. Body faceting (760-1050, 160-720): 12 flat vertical bands about 24 px wide; along y 450 the luminance steps 23, 17, 9 and 6 levels within 2 px at x 780, 802, 834 and 874, and the lid and foot rings (780-1040, 150-200 and 670-710) show the same bands. A 24-sided flat-shaded cylinder at this size is a low-poly placeholder, not a turned gilt vessel. Defect A. ART, fix 1.
2. Body colour: lit face (236,210,123), luminance 209 against the plaster (181,146,105) at 150 (1.39x); R/G 1.12 against the concept's 1.30. The concept tankard is a muted gold with a highlight and brown shadow side; this is lemon. Defect A. ART, fix 2.
3. Concept element not built: the concept tankard has a domed hinged lid; the live top is a flat 7 cm ring (780-1040, 150-200). Not a breakage; a player still reads a tankard. ART, fix 3 (lowest priority).
4. Right as built: shield board 0.8 x 1.15 m of dark planks hung on two drop rods, arm with strut and scroll ring, wall plate, foot ring, D handle meeting the body, vessel in relief on both faces casting its shadow on the board, window with open shutters behind, sill rail and jetty beam. Nothing clips.

## context-overview-a — PASS

Front-right from above: slate roof with ridge caps, dormer, stack with pot, the stack's base on a dark course continuous with the plinth (1175-1232, 735-760; no void, floor not visible past it), jetty, door with three risers (800-880, 725-770), barrels, bench, ivy clump, fence L with the sprigs inside, nothing mirrored or in rows, nothing floating. The emblem (770-800, 510-560) is 30 px of pale yellow; fixes 1-2 cover it.

## context-overview-b — PASS

Rear-left from above: roof, stack rising through the rear slope with clean slate edges against it, dormer peak beyond the ridge, two lit rear windows, one gable window, plinth course under both visible walls, washing line and ladder in the rear shadow, the front jetty beam end past the far corner (1240-1270, 550-580) where the concept's beam also projects. The stack's base is hidden behind the house.

## context-ground-1 — PASS

Right gable from the right: full-height stack against the gable on its own dark footing course (1180-1273, 840-880) continuous with the plinth under the gable wall, plaster triangle, one shuttered upper light clear of the stack, the small lit ground-storey light (895-960, 625-715) in the ashlar near the front corner, sign and lantern edge-on at the front corner, barrels and ivy clump at the corner, fence L (860-1120, 820-950) with the sprigs inside, washing line beyond. The stack's rear face (32,30,24) is in shadow with the sun on the far side; nothing is open under it.

## context-ground-2 — PASS

Left gable from the front-left: one shuttered light, posts and rails, ashlar below, plinth course flush, ladder at mid depth (770-850, 680-1030) with both feet on the floor and its head on the wall, washing line at the rear-left with braced posts, barrels at the front corner, sign (tankard edge-on, 1300-1340, 450-540) and lantern (1220-1260, 650-710) seen past the corner on the front wall. The stack is on the far gable and out of frame.

## context-ground-3 — PASS

Rear from the right: stack full height over the ridge on its footing (560-585, 830-870: (35,31,25) against the shadowed rear plinth (26,24,20); no void), two rear windows, washing line, ladder past the far corner, fence L with sprigs at the left (440-550, 780-840).

## context-ground-4 — PASS

Stack gable from the rear-right: stack on its footing course (790-860, 830-870), gable window, the small lit ground-storey light near the front corner (670-700, 640-710), jetty beam end past the front corner (605-660, 545-600), fence L with sprigs, ivy clump, washing line at the rear with braced posts on the floor.

## Noted, not graded here

- Surface treatment (plaster trowel arcs, bright crisp ashlar, mid-brown timber against the concept's grime and near-black oak): §10 defers to GFX-03 §2, as every WP6 review has; `kit.solid()` palettes are retired by GFX-03 §2.4.
- Lit windows under a noon sun: GFX-03 §2.
- The `IvyClump` reads as a hop-like shrub standing at the corner rather than ivy on the wall: accepted at 3 m in `props-and-dressing.md`.

## Fix list (each executable without this review; shared first — none shared)

1. **ART, package, Codex** — tankard body smooth enough to read as turned metal. `GameClient/ArtSource/Silverbrook/GildedTankard/build_gilded_tankard.py` line 157: replace
   ```python
   G['detail'].vessel((-.06,SY,4.40),[(0,0),(.23,0),(.23,.07),(.21,.07),(.19,.65),(.22,.65),(.22,.72),(0,.72)],M['gilt'])   # round gilt tankard through the board (relief on both faces)
   ```
   with
   ```python
   G['detail'].vessel((-.06,SY,4.40),[(0,0),(.23,0),(.23,.07),(.21,.07),(.19,.65),(.22,.65),(.22,.72),(0,.72)],M['gilt'],segments=96)   # round gilt tankard through the board (relief on both faces); 96 sides: Geo.poly never shares vertices, so smooth=True cannot smooth and each side shades flat
   ```
   Checked: `vessel(self,centre,profile,mat,segments=24)` (`silverbrook_kit.py` line 239); 8 profile spans x 96 = 768 quads, 1,536 triangles, inside the 1.0 fallback. Kit unchanged, so every other package's barrels and pots are untouched. Acceptance: in `sign-close.png` along y 450 no luminance step of 8 or more levels between adjacent 2 px samples inside the body (x 775-1025).
2. **ART, package, Codex** — gilt that sits at the plaster's brightness, as drawn. Same file line 33: replace
   ```python
       gilt=kit.solid('SB_GildedTankard_Gilt',(.90,.66,.22),.3,.6))
   ```
   with
   ```python
       gilt=kit.solid('SB_GildedTankard_Gilt',(.60,.38,.09),.3,.6))   # darker, more orange base at the same part-diffuse metallic (r2: higher metal mirrors the floor and reads olive); the concept tankard is as bright as its plaster, the .90 base was 1.4x
   ```
   Package-local material, nothing shared. Metallic stays .6 and roughness .3. Acceptance: in `sign-close.png` the body lit-face mean (880-940, 300-600) has luminance between 0.9x and 1.15x the plaster at (1400-1800, 100-500) and R/G >= 1.2; in `frontage.png` the emblem (918-980, 110-220) is at most 1.15x the plaster at (1050-1180, 120-220). If the ratio is still above 1.15 after the rebuild, scale the three base components down in proportion (never raise metallic).
3. **ART, package, Codex** — domed lid, as drawn (lowest priority; concept element, not a breakage). With fix 1, use the profile
   ```python
   [(0,0),(.23,0),(.23,.07),(.21,.07),(.19,.65),(.22,.65),(.22,.72),(.18,.76),(.10,.79),(0,.80)]
   ```
   Checked: top at z 5.20 = the board's top edge (`board` line 152, z 5.2), x -.28..+.16 clear of the drop rods at +-.32 (line 151) and under the arm at AZ 5.45; the handle (HO top 5.03) is unchanged. Acceptance: `sign-close.png` shows a rounded cap above the lip ring instead of a flat top.
4. **Rebuild, re-import, recapture, Codex** — after 1-3: `blender.exe -b --factory-startup -P build_gilded_tankard.py` (expect `GILDEDTANKARD_BUILT`, bounds unchanged 10.585 x 9.11 x 12.07); README step 4 (`import_silverbrook.py --package-only GildedTankard`, revert resaved shared `MI_SB_*` with `git checkout`); no map recreation (no part pivot changes, `Door1` unchanged); step 7 gates with the same lines (`DEFAULT_MATERIAL_CHECK visible_in_town 26 default_or_graybox 0 []`); steps 8-9; a fresh reviewer judges the eleven pairs.
