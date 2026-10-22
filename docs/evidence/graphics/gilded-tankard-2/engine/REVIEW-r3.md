# The Gilded Tankard — Unreal stage review r3 (dev map `GildedTankard_Review`)

Judge: independent art director (did not build this). Inputs: concept `docs/Game-Design/concept-art/Silverbrook/The Gilded Tankard.png`; the eleven raw `<view>.png` (1920x1080) and their `-compare.png` pairs; `README.md`; `REVIEW-r1.md`, `REVIEW-r2.md`; `build_gilded_tankard.py`, `asset-manifest.json`, `silverbrook_kit.py` (`frame`, `wall`, `door`, `vessel`, `Geo.box`), `silverbrook_props.py` (`_base_cm`, `_base_box`, `dress_footings`), `tools/check_foundations.py`, `tools/capture_building.py`; `docs/blueprints/buildings.md` §5/§8/§10. Pixel coordinates are in the raw captures (x0-x1, y0-y1); colours are 0-255 sRGB means of the named region.

Result: **4 PASS / 7 FAIL.** Three defects, all in the building, none in the dressing:

- A. **The chimney floats.** The stack is an external breast on the right gable (`XC=W2+.44`, box 0.9 x 1.1 m from model z 0), but the manifest `base_cm` is the ashlar wall base only (x max 435.3 cm; the stack spans 434.3-524.3 cm), so the footing pass lifts the house 65 cm onto a plinth that ends 5 cm into the stack. The stack's bottom face hangs 65 cm above the floor over a black void. Visible in six views. `check_foundations.py` cannot see it (it compares the plinth with the same `base_cm`), and `check_ground.py` only checks dressing.
- B. **A double-height top step.** The r2 fix narrowed the baked doorstep (`build_gilded_tankard.py` line 141) to 1.5 m, but it still sits on top of the engine stoop: the stoop's top tread is at the wall base (`dress_footings` line 645, `top=wall_base-1`), the baked step is 0-45 cm out from the wall at wall base +0..20 cm, so it buries `Stoop0` (4-44 cm out, top at -1 cm) and the last riser is 42.7 cm (21 cm elsewhere). It is also a single bright jointless block: §10 "smeared pale pier block".
- C. **The tankard is a flat yellow cut-out.** Both emblem faces are 2 cm flat plates on the board; a flat plate under a uniform sky shades uniformly, so the r2 material change gave a brighter flat colour, not gilt. The body reads the same value at every point (spread 4 levels), while the barrels in the same light show a 2:1 lit-to-shadow roll-off.

Verified against the code before grading: two front windows at x +-2.6, sign, dormer and door at x 0 (lines 59, 129, 132, 143-160); the third "window" at the right edge of `frontage` is the gable light past the corner, and the sign over the left window in `frontage`/`three-quarter` is the 1.2 m projection seen from front-right (`SY=-D2-1.2`). Neither is a defect.

## frontage — FAIL

1. Doorstep slab (1000-1310, 830-880 in `frontage`; measured in `foundation-close` below): a bright, jointless 1.5 x 0.45 x 0.2 m block on top of the stoop. From this player's-eye view it reads as a pale bench top across the doorway. Defect B. ART, fix 2.
2. Emblem (900-980, 90-230): flat lemon (217,189,102) with no shading across the body. Defect C. ART, fix 3.
3. Right as built: ashlar ground storey, dressed round arch with quoins, double door with two straps per leaf and ring plates, caged lanterns either side (850-890, 510-590; 1360-1410, 500-590) with amber glass, jetty beam on corbels, oak frame, shuttered lit leaded lights, barrels left on the ground, bench right. The plinth course is visible under the wall (1500-1900, 920-1000).

## foundation-close — FAIL

1. Top riser: the climb from the `Stoop1` tread (y 705) to the slab top (y 607) is 98 px; the two lower risers are 55 and 50 px (y 705-760, 760-810). In metres: 42.7 cm against 21.7 cm. `Stoop0` is entirely under the slab (its tread is never visible), so the stoop reads as three risers of which the top one is twice the others. Defect B. ART, fix 2.
2. Slab material: x 1000-1310, y 600-670 is one stone with no joint across 310 px and a mean of (152,130,103); the ashlar wall beside it is (104,86,64) with joints every 40-90 px, the stoop treads (97,80,60). It is the brightest surface in the frame. Defect B, same fix.
3. Right as built: the 65 cm course under the wall is a darker matte rubble distinct from the ashlar; the three engine risers land on the floor; barrels (630-880, 540-760) and bench (1400-1920, 740-1000) stand on the ground at the wall foot; lanterns are cages with a cap.

## three-quarter — FAIL

1. Chimney base (1313-1397, 950-990): a black rectangle 84 x 40 px under the stack where the ground should meet the stone; the plinth (dark course, 620-1320, 940-1000) stops at the gable wall and the stack's underside is open 65 cm above the floor. Defect A. CODE + manifest, fix 1.
2. Slab (700-870, 920-940) pale strip over the stoop and the flat emblem (670-710, 540-600): defects B and C, small at this distance.
3. Right as built: massing, 42-degree gable, dormer centred under the ridge, stack on the right gable with cap and pot, jetty on corbels, eave corbel row, two front and one gable shuttered lights, arched door, barrels left, bench right, lanterns either side, ivy at the front-right plinth corner (1030-1110, 960-1060), fence L (1240-1560, 960-1080) with its sprigs inside.
4. Concept element not built: the concept's 3/4 view has a small lit ground-storey window on the right side wall near the front corner (concept 1440-1470, 770-810). The live right gable ground storey is blank ashlar (1050-1300, 650-940). Not a breakage; a player would still know the building. ART, fix 4.

## rear — PASS

Two shuttered lit lights with sill and plate rails, plaster flush on the ashlar, plinth flush, washing line on two posts behind the wall (760-1300, 720-1000) with five garments, ladder on the far corner (1640-1720, 640-940), fence L past the right corner (390-500, 820-890). Glass strength 1.2: cames continuous over an even amber fill. The stack is edge-on at the left (490-530, 60-900); its open underside shows only as a 10 x 15 px grass notch at (493-503, 883-897), which fix 1 removes.

## sign-close — FAIL

1. Tankard body (760-1050, 160-720): means top (217,189,102), middle (219,190,102), bottom (218,188,98), left (218,189,100), right (219,190,101). A 0.40 x 0.58 m "gilt" body with a 4-level spread is a painted cut-out, not metal; the concept tankard is a rounded gold vessel with a highlight and a shadow side. Cause: line 160 extrudes flat 2 cm prisms both sides of the board; the 0.6 metallic/0.3 roughness material has nothing to reflect but a uniform sky. Defect C. ART, fix 3.
2. Right as built: shield board 0.8 x 1.15 m of dark planks hung on two drop rods, arm with strut and scroll ring, wall plate, lid/foot/D-handle outline, window with open shutters behind, sill rail and jetty beam. Nothing clips.

## context-overview-a — FAIL

1. Chimney base (1175-1232, 735-760): the stack ends in the air with a black void 57 x 25 px beneath it, the striped floor visible past it; the plinth under the gable wall ends flush with the wall. From above this is the most obvious broken thing in the frame. Defect A, fix 1.
2. Otherwise a coherent place: slate roof with ridge caps, dormer, stack with pot, jetty, door with stoop, barrels, bench, ivy, fence L with the sprigs inside, nothing mirrored or in rows.

## context-overview-b — PASS

Rear-left from above: roof, stack rising through the rear slope with clean slate edges against it (crop checked, 620-820, 250-420), dormer peak beyond the ridge, two lit rear windows, one gable window, plinth flush, washing line and ladder in the rear shadow. The stack's base is hidden behind the house in this view.

## context-ground-1 — FAIL

1. Chimney base (1167-1273, 830-867): the plinth's dark course runs under the gable wall to x 1180 and stops; under the stack is a flat black rectangle 106 x 37 px down to the floor. Defect A, fix 1.
2. Otherwise right: full-height stack against the gable, plaster triangle, one shuttered light clear of the stack, ashlar below, sign and lantern edge-on at the front corner, fence L (860-1120, 820-950) with the sprigs inside, ivy at the corner, washing line beyond.

## context-ground-2 — PASS

Left gable from the front-left: one shuttered light, posts and rails, ashlar below, plinth flush, ladder at mid depth (780-850, 690-1020) with both feet on the floor, washing line at the rear-left, barrels at the front corner, sign and lantern edge-on. The stack is on the far gable and out of frame.

## context-ground-3 — FAIL

1. Chimney base (585-670, 820-870): the stack's stone ends above the floor with a dark cavity beneath it where the wall beside it has its plinth course. Defect A, fix 1.
2. Otherwise right: stack full height over the ridge, two rear windows, washing line, fence L at the stack side, ladder past the far corner.

## context-ground-4 — FAIL

1. Chimney base (797-860, 830-873): the plinth course under the gable wall stops at x 797 and the stack's underside is open to the floor. Defect A, fix 1.
2. Otherwise right: stack gable, gable window, fence L with sprigs inside, washing line at the rear.

## Noted, not graded here

- Surface treatment (plaster trowel arcs, bright crisp ashlar, mid-brown timber against the concept's grime and near-black oak): §10 defers to GFX-03 §2, as every WP6 review has.
- Lit windows under a noon sun: GFX-03 §2.
- Door rings mid-leaf: r5 Blender fix 3, accepted there.

## Fix list (each executable without this review; shared first)

1. **CODE, shared, opt-in, Engineer** — footing under parts that stand on the ground outside the wall base. `GameClient/ArtSource/Silverbrook/silverbrook_props.py`:
   a. After `_BASE_CM={}` (line 538) add
   ```python
   def _footings_cm():
       """Opt-in extra footings from package manifests (`footings_cm`: [[x0,x1,y0,y1], ...] mesh-local cm) for
       stacks and bays that stand on the ground outside the wall base (the Gilded Tankard's external chimney)."""
       if '_' not in _FOOTINGS_CM:_FOOTINGS_CM['_']={m['unreal_mesh']:m['footings_cm'] for p in ROOT.glob('*/asset-manifest.json')
                                                      for m in [json.loads(p.read_text(encoding='utf-8'))] if 'footings_cm' in m}
       return _FOOTINGS_CM['_']
   _FOOTINGS_CM={}
   ```
   b. In `dress_footings`, after `plinth.static_mesh_component.set_material(0,m)` (line 630) and before the `# Stoop:` comment, add
   ```python
           # Opt-in footings (manifest `footings_cm`) under parts that stand on the ground outside the wall base: same stone, top at the wall base.
           for old in [x for x in by_label.values() if x.get_actor_label().startswith(building.get_actor_label()+'_Footing')]:A.destroy_actor(old)
           for k,(fx0,fx1,fy0,fy1) in enumerate(_footings_cm().get(mesh.get_name(),[])):
               fw=(fx1-fx0+2*pad)*s.x;fd=(fy1-fy0+2*pad)*s.y;fcx=(fx0+fx1)/2*s.x;fcy=(fy0+fy1)/2*s.y
               fpx,fpy=loc.x+co*fcx-si*fcy,loc.y+si*fcx+co*fcy;fb=ground(fpx,fpy)-val(f['burial_cm'])
               ft=A.spawn_actor_from_class(u.StaticMeshActor,u.Vector(fpx,fpy,(fb+wall_base)/2),building.get_actor_rotation())
               ft.static_mesh_component.set_static_mesh(cube);ft.static_mesh_component.set_material(0,m)
               ft.set_actor_scale3d(u.Vector(fw/100,fd/100,(wall_base-fb)/100))
               ft.set_actor_label(f'{building.get_actor_label()}_Footing{k}');ft.set_folder_path(building.get_folder_path())
   ```
   Opt-in: no other manifest has `footings_cm`, so every other building's output is unchanged. `check_foundations.py` reads `_Plinth` only and is unaffected.
   c. **ART, package, Codex** — declare the stack footing. `GameClient/ArtSource/Silverbrook/GildedTankard/build_gilded_tankard.py` line 210: after the `base_cm=...` entry add the manifest key
   ```python
       footings_cm=[[round(100*(XC-.45-(lo[0]+hi[0])/2),1),round(100*(XC+.45-(lo[0]+hi[0])/2),1),round(-100*(YC+.55-(lo[1]+hi[1])/2),1),round(-100*(YC-.55-(lo[1]+hi[1])/2),1)]],   # external stack (0.9 x 1.1 m from the ground) stands outside the wall base: its own footing in dress_footings
   ```
   Expected manifest values `[[434.3, 524.3, -313.2, -203.2]]` (x = model x - 24.7; y = -(model y + 38.2), the import flips Y). Acceptance: `HOUSE_FOUNDATIONS 1`; in `three-quarter.png` and `context-overview-a.png` the stack meets a dark stone course of the plinth's height with no void (no pixel darker than the plinth course under the stack); `DEFAULT_MATERIAL_CHECK visible_in_town 26 default_or_graybox 0 []` (one more actor); the other gate lines unchanged.
2. **ART, package, Codex** — door sill at the wall base, no baked step. `build_gilded_tankard.py`:
   a. Line 52: replace `XDOOR,DWID,DS,STEP=0.0,2.6,1.5,.2   # double door: width, spring above the step (head 3.2 m, 0.34 m of ashlar under the jetty), step top` with `XDOOR,DWID,DS,STEP=0.0,2.6,1.7,0.0   # double door: width, spring, sill at the wall base (head 3.0 m); the engine stoop (silverbrook_props.dress_footings) climbs to the wall base, so no baked step`. The opening `(0, 2.6, 0, 3.0)` and arch spring 1.7 m keep the crown at 3.0 m; straps, ring and quoins are all `STEP+...` and move down with the sill.
   b. Line 140: replace `DL.box((0,-DF+.155,STEP+1.38),(.06,.02,2.72),M['timber'],.004,grain=True,band=band())   # meeting stile on the leaf: z 0.22..2.94 under the 3.0 m crown` with `DL.box((0,-DF+.155,STEP+1.48),(.06,.02,2.92),M['timber'],.004,grain=True,band=band())   # meeting stile on the leaf: z 0.02..2.94 under the 3.0 m crown`.
   c. Delete line 141 (`G['stone'].box((XDOOR,-DF-.225,STEP/2),(1.5,.45,STEP),M['ashlar'],.015,rect=kit.rng.choice(STONES))`).
   `base_cm` (wall face `-DF`) and `sign_m` are unchanged; the `Door1` part pivot z becomes 0.0, so the dev map must be recreated (fix 5). Acceptance: in `foundation-close.png` three risers of equal height (within 10 px) from the floor to the door sill, the top tread `Stoop0` visible, no surface in the stoop brighter than the ashlar wall mean.
3. **ART, package, Codex** — tankard body as a turned vessel so it shades like metal. `build_gilded_tankard.py` lines 154-160: replace
   ```python
   body=[(-.27,4.47),(.13,4.47),(.13,5.05),(-.27,5.05)]   # tankard: 0.40 x 0.58 body (concept body 53 % of the shield width), open D handle abutting at x .13
   lid=[(-.30,5.05),(.15,5.05),(.15,5.12),(-.30,5.12)];foot=[(-.30,4.40),(.15,4.40),(.15,4.47),(-.30,4.47)]
   HO=[(.13,4.55),(.22,4.55),(.31,4.65),(.31,4.93),(.22,5.03),(.13,5.03)];HI=[(.13,4.61),(.19,4.61),(.25,4.67),(.25,4.91),(.19,4.97),(.13,4.97)]   # D handle outer/inner rims
   # five convex quads: Unreal's FBX import fan-triangulates n-gons from their first vertex, which breaks a concave outline
   handle=[[HO[k],HO[k+1],HI[k+1],HI[k]] for k in range(5)]
   for poly in (body,lid,foot,*handle):
       for dy in (-.035,.035):G['detail'].prism([(x,SY+dy,z) for x,z in poly],(0,1,0),.02,M['gilt'])
   ```
   with
   ```python
   # tankard: a turned gilt vessel through the board (both faces), body r .19-.21 tapering up, foot and lid rings; a flat plate shaded flat under the sky
   G['detail'].vessel((-.06,SY,4.40),[(0,0),(.23,0),(.23,.07),(.21,.07),(.19,.65),(.22,.65),(.22,.72),(0,.72)],M['gilt'])
   HO=[(.11,4.55),(.22,4.55),(.31,4.65),(.31,4.93),(.22,5.03),(.11,5.03)];HI=[(.11,4.61),(.19,4.61),(.25,4.67),(.25,4.91),(.19,4.97),(.11,4.97)]   # D handle outer/inner rims, 2-4 cm into the body
   # five convex quads: Unreal's FBX import fan-triangulates n-gons from their first vertex, which breaks a concave outline
   handle=[[HO[k],HO[k+1],HI[k+1],HI[k]] for k in range(5)]
   for poly in handle:
       for dy in (-.035,.035):G['detail'].prism([(x,SY+dy,z) for x,z in poly],(0,1,0),.02,M['gilt'])
   ```
   Checked: the vessel (axis Z, `Geo.vessel` line 239) spans x -.29..+.17 and z 4.40..5.12, inside the board outline (x +-.4, z 4.05..5.2) and clear of the drop rods at x +-.32; it projects 0.2 m from each board face like a carved pub sign. The D handle at x .11 overlaps the body (edge x .15 at z 4.47, .13 at z 5.05). Material unchanged (`SB_GildedTankard_Gilt`, package-local). Acceptance: in `sign-close.png` the body's lit half and shadow half differ by >= 40 levels of luminance with a specular highlight band; `frontage.png` emblem no longer one flat value.
4. **ART, package, Codex** — ground-storey window on the right side wall, as drawn (lowest priority; concept element, not a breakage). `build_gilded_tankard.py` line 102: replace `wall(G['stone'],M['stone'],(s*W2,JET/2,0),(s,0,0),D2-JET/2,STONE_TOP,[])` with `wall(G['stone'],M['stone'],(s*W2,JET/2,0),(s,0,0),D2-JET/2,STONE_TOP,[(-1.9,.88,1.51,2.59)] if s==1 else [])   # right gable: small taproom light 1.5 m from the front corner, as drawn` and after line 131 add `window(G,M,(W2,JET/2-1.9,1.6),(1,0,0),.7,.9,shutters=False,band=band())   # right-gable ground-storey light (concept 3/4 view)`. Checked: `frame((W2,JET/2,0),(1,0,0))` has `a` along +y, so a = -1.9 is model y -1.675 (1.5 m behind the front face at -3.15), clear of the stack (y 1.65..2.75) and the front lantern; opening = glazed 0.7 x 0.9 plus the 9 cm frame. Acceptance: `context-ground-1.png` shows one small lit light in the ashlar of the right gable near the front corner.
5. **Rebuild, re-import, recapture, Codex** — after 1-4: `blender.exe -b --factory-startup -P build_gilded_tankard.py` (expect `GILDEDTANKARD_BUILT`, bounds unchanged 10.585 x 8.925 x 12.07); README step 4 (`import_silverbrook.py --package-only GildedTankard`, revert resaved shared `MI_SB_*`); delete `/Game/Frontier/Maps/Dev/GildedTankard/` and rerun step 5 (`Create-BuildingDevMap.py GildedTankard`, so `place_parts.attach` uses the new `Door1` pivot); step 6; step 7 gates with `DEFAULT_MATERIAL_CHECK visible_in_town 26`, the other lines unchanged, and report rather than silence any `GROUP_DETAIL_OVERLAPS` naming `_Footing0`; steps 8-9; a fresh reviewer judges the eleven pairs.
