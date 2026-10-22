# WP6i round-8 review — Silverbrook town, pass 9 r8 (REVIEW-r7.md fix list 1-6)

Reviewer: independent (Fable), did not build the work. Judged per `docs/visual-review.md` against `Silverbrook-Design-Board-v1.webp` and `UI/v5/gameplay-hud.png` (mill plume x 1330-1360, y 100-170: 144/143/152, R - B -8), the r7 fix list and its acceptances, the r1-r7 and WP6h standing acceptances, and the owner's four reports. Commit b9fc1b5c. Every capture read at full resolution plus 3-4x crops and pixel measurements (`py -3`, PIL/numpy/scipy): connected bright-blob and orange-blob search in the three dusk views (r8 and the r7 frames side by side), wall-band column medians, warm-pixel counts within 150 px of each lantern pane, sky / bottom-quarter medians, plume L/ref by band on `chimney-smoke.png` against `-ref.png`, densest-puff colour, far threads in `main-street`, motion frames 0 / 6 / 13, and every `.uasset` blob in the commit compared with `af48e04f` by `git ls-tree`. README numbers were checked, not trusted; the lantern geometry was checked against the Blender builders (`build_townhouse1/2/4.py`, `silverbrook_kit.lantern`) and `lamp-sockets.json`.

**Result: 16 PASS / 3 FAIL.** Bookmarks 5 / 0, dusk lamp views 0 / 3, context views 11 / 0.

Fixed this round and holding: **the orange dash is gone** (side-b x 1379-1429, y 212-225 now reads wall L 17, R - B -37; no orange blob outside a pane in any dusk view; `check_dusk_blobs.py` prints `outside 0` for all three and I reproduce it), **the plume is at the concept's hue** (densest puffs 140/138/145, R - B -4.7; lit colour with the 20 % field removed 161/160/169, R - B -7.8 against the concept's -8; core p5 / p50 / mean 0.929 / 1.038 / 1.081; hill 0.967; sky rows 0.966; roofs p50 1.091, at the bound as r7; motion frames differ, no thin rows), **the sky band is written by direction** (r8 sky medians: frontage 106 toward the sun, side-b 82 / 19, lamp-wall 53 — all inside the new bands; steps and bottom quarters 8.7 / 17.4 / 20.5), and **the commit is clean** (1,652 of 1,654 `.uasset` / `.umap` blobs identical to `af48e04f`; only `MI_SB_LanternGlow` and the map differ).

Not met: **fix 1 was not implemented, the lights were deleted.** The r7 acceptance for side-b was "the wall band y 100-600, x 900-1450 has its brightest column within 40 px of the lantern's centre column (1477)". In r8 the brightest 25-px column is **x 925** (L 31), 552 px away; every column reads L 17-31 with R - B -19 to -55: the wall is lit by the moon only. Within 150 px of the pane, outside it, **745 px** are warm (R >= B + 10, L >= 30) against 16,274 in r7 — and those 745 are the far house's lit windows. The lamp-wall lantern (Townhouse1, pane x 446-505, y 976-1047) sits 10 cm from its ashlar: **34 warm px** around it (r7: 36). The frontage lantern (x 364-408, y 374-425): **61** (r7: 63). Three lit lanterns, three walls with no light on them: the owner's "orange light with no visible source" has become **a visible source with no light**, on every building lantern in the town (`LAMP_FX` 392 lights, `LIGHT_CHECK on_glass 392`, and none of them shows on a wall).

Why, from the code and the builders, not the README: (a) The README says "these houses have no lantern of their own; the glow is the hanging sign". False. `build_townhouse2.py` line 250 `lantern(G,M,(XDOOR-.75,-D2-.42,2.3),arm=...)`, `build_townhouse4.py` line 273, `build_townhouse1.py` line 171 each place `silverbrook_kit.lantern`: a **15 x 15 x 24 cm `SB_LanternGlow` box** (kit line 354) at body z 2.03 / 1.98 / 2.28 m, exactly the one `lamp-sockets.json` point per mesh (Townhouse2 (-1.777, -2.678, 2.03), Townhouse4 (-2.222, -2.67, 1.98), Townhouse1 (-2.16, -3.1, 2.28)), and exactly the pane that glows L 240 in `context-house-side-b-dusk`, `context-lamp-wall-dusk` and the noon frames. The banner is `SB_Townhouse2_Banner` (denim), a 52 x 3 x 98 cm shield prism at z 1.99 (`sb` polygon, line 283) — the piece the `LAMP_PIECE` log reports under the *glow* slot, with the banner's own pointed-bottom "feet". So `glass_sockets` reads the **wrong material ID** on the merged building meshes: the dynamic-mesh IDs from `split_mesh_by_material_i_ds` are off by one against `mesh.static_materials` (the Details object's slot list runs `..., SB_LanternGlow, SB_Townhouse2_Banner, ...`; Townhouse1 returned a 204 x 19 x 2 sill strip the same way). The r8 "drop any chamber >= 60 cm" rule then discards the banner, and the real lantern was never found. `check_lights.py` uses the same `glow_boxes` scan, so it certified the r6/r7 banner-and-sill lights as `on_glass` and now certifies the 392 remaining lights the same way: it is not independent of the defect. (b) The committed GLB sockets are not "2-20 m off": they are Blender coordinates (extract_sockets.py line 3) and UE's FBX import mirrors Y, so the building points were compared un-mirrored (street lamps sit at y = 0, which is why they "matched to 0 cm"). The banner chamber at (-316, 156, 199) is the mirrored Blender banner (SX, +1.7, 1.99) to within 12 cm. (c) The lanterns that do have a light (the frontage house, every cottage) put it **at the centre of an opaque 15 cm emissive box with `cast_shadows` on**: nothing leaves the box, which is why the frontage wall beside its lantern reads L 23 in r6, r7 and r8 alike. The r6 instruction "light at the glass centre" was right for the translucent street-lamp glass and wrong for the kit's opaque panes; the 2 x 5 flame, halo and smoke cards per building lantern are inside the same box and invisible (the pane is a flat L 240 slab, as r7 noted). Fix 1 below puts a correct chamber under every building lantern and moves its light 14 cm clear of the pane.

## Owner reports

| Report | Status | Evidence |
|---|---|---|
| Floating houses | Fixed (WP6h), holds | `foundation-close`, `context-house-side-a/b/rear`: plinth under every wall, buried, no gap; assets unchanged. |
| Orange light with no visible source | **Open, inverted: every building lantern is a lit pane that lights nothing** | Side-b wall band brightest column x 925 vs lantern 1477; 745 / 34 / 61 warm px around the three lanterns (r7 side-b 16,274 from the misplaced light). Fix 1. |
| Street clutter in mirrored rows on an empty approach road | Fixed (r1), holds | `context-road-approach`: groups only in front of houses, no twin across the road, irregular gaps. |
| White street | Holds | Tan cobbles kerb to kerb in every street view. |
| Lamps lit at noon (J5) | Fixed, holds | Every noon lantern dark (`context-lamp-wall`, `context-house-side-a/b/rear`, `house-frontage`, `context-road-approach`). |

## Round-7 fix items 1-6: met?

| Item | Verdict | Measured (this review) |
|---|---|---|
| 1 Sign-sized glow pieces never join a lantern chamber | **Not met** | The orange dash is gone (no orange blob outside a pane in x 1300-1450; none anywhere outside the panes), but by removing the light, not placing it: side-b wall band brightest column x 925 (acceptance: within 40 px of 1477); warm px within 150 px of the pane 745 / 34 / 61 (side-b / lamp-wall / frontage). Panes 240-241 (met); within 1 m outside the pane 0 px > 235 (met); `orphans 0 in_wall 0`, 392 lights in the README (reported, but by a gate that reads the same wrong slot). The `LAMP_PIECE` lines were put in the README as asked; they show the banner and the sill, never the 15 x 15 x 24 pane, which is the defect. |
| 2 Dusk capture gate | Met in form, one box is a blanket | `check_dusk_blobs.py` + `dusk-panes.json`: reproduces `outside 0` / `orange outside 0` on all three views. But side-b's "window" box `[1700, 1920, 0, 1080]` is the whole right 220 px of the frame; what sits there is the far iron street lamp's two panes at x 1718-1734 / 1786-1803, y 345-372. Fix 3 names them. |
| 3 Night-row sky band by direction | **Met** | `lighting-and-atmosphere.md` line 40 carries the two bands and the steps clause; r8 frontage sky 106 (toward the sun, band 10-120), side-b 82 / 19, lamp-wall 53 (band 10-60); bottom quarters 8.7 / 17.4 / 20.5; `capture_game.py` comment updated. No capture changed. |
| 4 Cool the plume one more step | **Met** | Tint [0.25, 0.31, 0.43] in `silverbrook-props.json` and `vfx.md`; densest puffs R - B -4.7 (band <= 5, >= -12); core 0.929 / 1.038 / 1.081; hill 0.967; sky rows 0.966; roofs 1.091 (at the bound, as r7's 1.098); growth 2.35 by the r6 definition; 0 thin rows in 15 frames; MAD 1.13-1.36; up 69.6 px/s; main-street thread x 790-830 L/sky 0.92 with B > R (157/168/183 on 153/165/182). |
| 5 Lamp family | Owner decision, open | Unchanged. |
| 6 Resubmit | Met in form | All views recaptured, ref and 14 motion frames, perf pair (1440p: GT 0.119-0.159 ms <= 0.2, workers 0.30-0.37 <= 0.6, chimney on - off -0.24 ms, street +0.72 with one 5.39 ms outlier frame; on-1 - off-1 -0.22), only the listed assets differ from `af48e04f`. |

## main-street — PASS

Unchanged street (no asset in the view changed): tan cobbles kerb to kerb, five house tones, barrels, crates, hay cart, bench, ladder, signs, lamps and windows dark at noon, weeds at the kerb, nothing floating. Far thread x 790-830: 157/168/183 on sky 153/165/182, a pale cool ribbon; x 1395-1418: 162/163/172 on 170/176/186, a mid-grey ribbon against the white range. Seen, not failed: the white faceted range (J1b, deferred).

## market — PASS

Stalls with awnings, firewood sheds, ladder, sacks, mixed house tones, cobbles with earth margins, far houses on plinths; the far chimney thread at x 1635-1665 is a faint grey ribbon. Stall stand-in: GFX-03, deferred. Unchanged from r7.

## house-frontage — PASS

Bay window with planter, boot sign, bracket lantern dark at noon, stoop with tub, ivy, plinth; unchanged. Seen, not failed: the kerb-high cracked-mud bank at x 0-400, y 600-820 (J2, deferred).

## foundation-close — PASS

Rubble plinth under the ashlar on both faces of the corner, flush, buried; tub and planter on the stoop. Unchanged.

## chimney-smoke — PASS

Measured on `chimney-smoke.png`, `-ref.png` and `motion-00/06/13.png` (pots x 885-920, y 575-620).

| Acceptance (r7 fix 4) | r8 | Met |
|---|---|---|
| densest puffs R - B <= 5 and >= -12 | -4.7 (140/138/145 on a 55/50/48 field; lit colour 161/160/169, -7.8; concept -8) | Yes |
| core L/ref p5 >= 0.90, p50 and mean 1.00-1.15 | 0.929 / 1.038 / 1.081 | Yes |
| hill (y 60-300) p50 0.95-1.15 | 0.967 | Yes |
| sky rows (y < 60) p50 0.90-1.05 | 0.966 | Yes |
| roofs (y 300-380) p50 >= 1.1 | 1.091 | At the bound (r7 1.098) |
| growth >= 2.0 at y 300 | 155 / 66 = 2.35 (r6 definition) | Yes |
| 0 thin rows, 15 frames | 0 | Yes |
| MAD >= 1.0 | 1.13 min, 1.27 mean | Yes |
| up >= 20 px/s, drift <= 60 | 69.6 up, 3.0 drift | Yes |
| far threads p10 >= 0.87, B >= R | x 790-830 0.92, B > R | Yes |

What a player sees (`cs8_pair`, `motion_trio`): a soft pale-grey wisp leaves the pot, widens over the ridge and thins into a veil over the hazy hill and the far row, different in every frame, the same weight and value as the HUD wisp. No remaining plume item.

## house-frontage-dusk — FAIL

Bay panes lit, the stoop, tub and planter readable (bottom quarter 8.7), moon shadow across the field, far cottages with lit windows, sky 106 toward the sun; no bright blob outside the panes.

1. **What's wrong:** the bracket lantern at x 364-408, y 374-425 glows L 240 and lights nothing. The ashlar beside it (x 410-520, y 360-460) reads median L 23.5, R - B -18.5, the bracket and arch below it L 20-30, the same as in r6 and r7; within 150 px of the pane, outside it, 61 px are warm (R >= B + 10, L >= 30). A player on the stoop sees a lit lantern and a black wall and dark steps under it (`hf8_lantern_4x`). The light exists (`LIGHT_CHECK on_glass`) but sits at the centre of the opaque 15 x 15 x 24 cm emissive pane box with `cast_shadows` on, so it is sealed inside. Same state as r7, which checked only for blobs; the r8 `LAMP_PIECE` evidence shows why it has never worked. **Fix type:** CODE. **Exact instruction:** fix 1.

## context-house-side-b-dusk — FAIL

Window glass lit, bottom quarter 17.4, the bench and clothes line cast moon shadows, the far street lamp and windows lit, nothing clipped; the r7 orange dash is gone (x 1379-1429, y 212-225 reads 17 / -37).

1. **What's wrong:** the corner lantern at x 1455-1500, y 228-276 glows L 240 with no light on the ashlar corner beside it (x 1250-1450, y 100-600: L 8-25, R - B -19 to -41) nor on the stoop below. The wall band y 100-600, x 900-1450 has its brightest 25-px column at x 925 (L 31), 552 px from the lantern's column 1477 (r7 acceptance: within 40 px); warm pixels within 150 px of the pane 745, all of them the far house's windows (r7: 16,274 from the misplaced light). `SM_SilverbrookTownhouse2` has no light at all now: its only `LAMP_SOCKET` was the banner, dropped by the r8 >= 60 cm rule, and its real pane (the 15 x 15 x 24 box at mesh (-178, +268, 203) after the Y mirror) was never scanned (`sb8_lantern_3x` against `sb7_lantern_3x`). **Fix type:** CODE. **Exact instruction:** fix 1.

## context-lamp-wall-dusk — FAIL

Townhouse1 (`TheCommons_townhouse_037`), the same house as the noon frame: gable, jetty, lit upper and bay windows, the stag sign and the door lantern (x 446-505, y 976-1047, L 240), sky 53, bottom quarter 20.5, no bright blob outside the panes, sill plaster plain.

1. **What's wrong:** the lantern hangs 10 cm off the ashlar and the stones around it are cold: x 520-700, y 900-1080 median L 28.4, R - B -28; x 440-520, y 870-965 L 17.8, R - B -23; 34 warm px within 150 px of the pane (r7: 36). The lantern lights nothing (`lw8_lantern_3x`). `SM_SilverbrookTownhouse1` has no light: its only `LAMP_SOCKET` was the 204 x 19 x 2 sill strip, and the pane at mesh (-216, +310, 228) was never scanned. **Fix type:** CODE. **Exact instruction:** fix 1.

## Context views

### context-overview-se — PASS

Eye 230 m up: the whole town, forest east and south-east, roads out of each gate, wall extremes inside the frame; no claim-pad slabs. Unchanged. Seen, not failed (deferred): the lake causeway, the straight canal and its cut banks, flat cyan water, the white faceted mountain ring (J1b, GFX-02).

### context-overview-nw — PASS

The Westfields meadow, roads, river and bridges fill the bottom; the smooth white dome at x 0-260, y 0-210 is at its 1.2 km ring (J1b). Unchanged.

### context-road-silvermere — PASS

The V cutting, pines on both crests, shrubs and tufts down both banks, cobbles only on the carriageway, dark earth verges. Unchanged.

### context-road-everwood — PASS

Wide cobbled road into the pines, irregular scrub, dark rocks on the left margin, tufts; no stones on the carriageway. Lollipop broadleaf and the white range: GFX-03 / J1b.

### context-road-valemere — PASS

Trees, shrubs, the boulder on the verge, striped fields at the left; no stone sets on the road. Unchanged.

### context-town-edge — PASS

Wall segments and towers stepping with the ground, the far wall and the road out; nothing floats, no lamps on the open road. Unchanged.

### context-house-side-a — PASS

Side wall on its plinth, fence, garden bed, iron lamp dark at noon. Unchanged.

### context-house-side-b — PASS

Plinth stepping to the stoop base; stag sign, bracket lantern and iron lamp dark; forecourt and street clear of stones. The noon frame of the dusk subject: the lantern is a bracket lantern on the front face beyond the corner, 45 cm from the banner (`sb8_noon_lantern_3x`).

### context-house-side-rear — PASS

Rear plinth following the slope, firewood shelter, fence, lamps dark. Unchanged.

### context-lamp-wall — PASS

Townhouse1 gable in sun: stag sign, bracket lantern dark at noon, no wash, windows dark, sill plaster plain. Same house as the dusk frame.

### context-road-approach — PASS

Both sides fronted by houses, groups irregular, no twin across the road, timber-post lamps dark, no stones on the carriageway; chimney threads pale on the sky. Unchanged.

## Deferred (seen, not failed; GFX-02 / GFX-03 / owner, unchanged)

Bank texture streaks and bedrock faces on kerb-high banks (J2); the white faceted mountain ring and the far domes (J1b), 12 m cobble roads outside the wall (J1b); flat cyan water, lake causeway, canal and its cut banks, the zigzag fence lines on the Westfields field (GFX-02 world); ashlar repeats, plaster, stall stand-in, low-poly trees and shrubs, the same shrub mesh repeated (GFX-03); townsfolk mannequins (off for the captures); street-lamp family (r1 fix 9, owner); the claim pad's final form (DEC-03 levelling).

## Fix list (shared first)

1. **Building lantern chambers from the committed socket, light outside the opaque pane** — Engineer, CODE, `GameClient/ArtSource/Greenlands/lamp_fx.py`. Replace `glass_sockets` (lines 63-113) with:
   ```python
   LANTERN_AXES = (8., 35.)   # a kit lantern pane is a 15 x 15 x 24 cm box (silverbrook_kit.lantern; the cage variant 12 x 12 x 22)
   PANE_CLEAR = 14.   # cm outward from the pane centre: the 6 cm light source clears the opaque 7.5 cm half-box (WP6i r8 fix 1)


   def glass_sockets(mesh, u, sockets=None):
       """Mesh-space cm light positions. Street lamps: the connected pieces of the *Glow slots merged within 60 cm (translucent
       M_LampGlass; GLB sockets matched to 0 cm). Buildings: the committed lamp-sockets.json point (Blender metres; UE's FBX
       import mirrors Y, so (x, -y, z) x 100) confirmed by a lantern-sized connected piece of ANY material slot within 40 cm.
       The dynamic-mesh material IDs of the merged building meshes do not line up with static_materials (WP6i r8: the 'glow'
       ID returned Townhouse2/4's banner and Townhouse1's sill), so the slot scan is not used on buildings. The kit pane is an
       opaque emissive box, so the light sits PANE_CLEAR cm outward of it along the axis the lantern overhangs the house on."""
       name = mesh.get_name()
       if name in _GLASS:
           return _GLASS[name]
       street = name.startswith('SM_SilverbrookStreetLamp')
       glow = [i for i, slot in enumerate(mesh.static_materials) if _lantern(slot)]
       dm = u.DynamicMesh()
       dm, _ = u.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
           mesh, dm, u.GeometryScriptCopyMeshFromAssetOptions(), u.GeometryScriptMeshReadLOD())
       parts = u.GeometryScript_MeshDecomposition.split_mesh_by_material_i_ds(dm, mesh_pool=u.DynamicMeshPool())
       meshes = next(x for x in parts if isinstance(x, u.Array) and len(x) and isinstance(x[0], u.DynamicMesh))
       ids = next(x for x in parts if isinstance(x, u.Array) and len(x) and isinstance(x[0], int))
       boxes = []
       for piece, mid in zip(meshes, ids):
           if street and mid not in glow:
               continue
           comps = u.GeometryScript_MeshDecomposition.split_mesh_by_components(piece, mesh_pool=u.DynamicMeshPool())
           for comp in next(x for x in comps if isinstance(x, u.Array)):
               bb = u.GeometryScript_MeshQueries.get_mesh_bounding_box(comp)
               b = [bb.min.x, bb.min.y, bb.min.z, bb.max.x, bb.max.y, bb.max.z]
               if street or all(LANTERN_AXES[0] <= b[i + 3] - b[i] <= LANTERN_AXES[1] for i in range(3)):
                   boxes.append(b)
       out = []
       if street:
           merged = []
           for b in boxes:   # union boxes whose gap is under 60 cm into one chamber
               for m in merged:
                   if all(max(m[i], b[i]) - min(m[i + 3], b[i + 3]) < 60 for i in range(3)):
                       m[:] = [min(m[i], b[i]) for i in range(3)] + [max(m[i + 3], b[i + 3]) for i in range(3)]
                       break
               else:
                   merged.append(list(b))
           out = [[(m[i] + m[i + 3]) / 2 for i in range(3)] for m in merged]
       else:
           box = mesh.get_bounding_box()
           centre = [(box.min.x + box.max.x) / 2, (box.min.y + box.max.y) / 2]
           half = [max((box.max.x - box.min.x) / 2, 1), max((box.max.y - box.min.y) / 2, 1)]
           for p in (sockets or load_sockets())[name]:
               t = [p[0] * 100, -p[1] * 100, p[2] * 100]
               near = sorted(((sum(((b[i] + b[i + 3]) / 2 - t[i]) ** 2 for i in range(3)) ** .5, b) for b in boxes), key=lambda n: n[0])
               d, b = near[0] if near else (1e9, None)
               c = [(b[i] + b[i + 3]) / 2 for i in range(3)] if b else None
               u.log(f'LAMP_MATCH {name} json {[round(v) for v in t]} pane {c and [round(v) for v in c]} size {b and [round(b[i + 3] - b[i]) for i in range(3)]} dist {round(d)}')
               assert d <= 40, f'{name}: no lantern-sized piece within 40 cm of lamp-sockets.json {p} (mirrored Y); check the import axes'
               a = max(range(2), key=lambda i: abs(c[i] - centre[i]) / half[i])   # the horizontal axis the lantern overhangs on
               c[a] += PANE_CLEAR if c[a] >= centre[a] else -PANE_CLEAR
               out.append(c)
       _GLASS[name] = out
       return out
   ```
   Line 122: `for xyz in glass_sockets(mesh, u, sockets):`. Lines 262-266 (the five card instances): wrap in `if label.startswith('StreetLamp_'):` — the kit pane is an opaque emissive slab, so flame, halo and smoke cards inside or in front of it are invisible or a floating flame (r5-r7); the pane is the flame. Keep `_point_light` as is (27 lm wall factor, 6 cm source, `cast_shadows` True, 8 m reach): at 47 cm from the wall that is 10 lux against the 5 lux moon, a visible pool without clipping. Update the module docstring (lines 3-5: "Uses committed lamp-glass sockets" is true again) and `docs/blueprints/lighting-and-atmosphere.md` / `dependency-map.md` lines that say the GLB sockets are "2-20 m off". **Acceptance:** `LAMP_MATCH` lines in the README for `SM_SilverbrookTownhouse1`, `2`, `4` and the frontage house's mesh, each with `dist <= 40` and `size` within 8-35 on every axis (expected about [15, 15, 24]); `LAMP_FX` lights back to about 524 (392 + the 132 Townhouse1/2/4 lanterns); `check_dusk_blobs.py` `outside 0` and `orange outside 0` on all three dusk views (no blob >= 20 px with L > 200 outside the pane boxes, none orange); within 150 px of each lantern pane's centre and outside the pane bbox grown by 10 px: `context-lamp-wall-dusk` >= 3,000 px and `house-frontage-dusk` >= 3,000 px with R >= B + 10 and L >= 30, `context-house-side-b-dusk` >= 2,000 px (the corner is grazed) of which >= 1,000 lie in x 1250-1455 (the house, not the far windows); no pixel > 235 in those rings; every lantern pane >= 200; `LIGHT_CHECK ... orphans 0 in_wall 0` from the fix-2 gate.
2. **`check_lights.py` reads the same wrong slot** — Engineer, CODE, `GameClient/ArtSource/Greenlands/check_lights.py`. Replace `glow_boxes` (lines 17-31) so that for a building mesh (name not starting `SM_SilverbrookStreetLamp`) it keeps every connected component of every material ID whose three sizes are within 8-35 cm (the lantern pane rule, duplicated on purpose: the gate stays independent of `lamp_fx`), and for a street lamp the *Glow-slot pieces as now. Replace lines 50-56 (the `near` / `g > 3` block) with: `g = min(gap(local, b) for b in glow_boxes(mesh)) if glow_boxes(mesh) else 1e9; best = min(best, g)` and line 57's threshold `35` with `20` (a light 14 cm outside a pane face is 6.5 cm from the box; 20 leaves room for the cage variant). Docstring line 4-5: "sit within 20 cm of a lantern-sized piece of that actor's mesh". **Acceptance:** `LIGHT_CHECK lights N on_glass N orphans 0 in_wall 0 unshadowed_wall 0` with N equal to `LAMP_FX`'s count, in the README.
3. **Dusk gate: name the far-lamp panes** — Codex, LEVEL, `docs/evidence/graphics/2026-10-22-wp6-town-i/dusk-panes.json`. In `context-house-side-b-dusk`, replace the `[1700, 1920, 0, 1080]` window box with the far street lamp's two panes `[1716, 1736, 343, 374]`, `[1784, 1805, 343, 374]` and the far houses' lit windows read off the frame (each window its own box, no box wider than 120 px or taller than 150 px); add to `check_dusk_blobs.py` a `DUSK_WASH <view> warm_px <n> hot_px <m>` line per lantern box: pixels within 150 px of the pane centre, outside the pane bbox grown by 10 px, with R >= B + 10 and L >= 30 (warm) and L > 235 (hot); exit 1 when warm < the fix-1 counts or hot > 0. **Acceptance:** the script fails on the r8 frames (warm 745 / 34 / 61) and passes on the fix-1 recaptures.
4. **Lamp family** — owner decision (r1 fix 9), unchanged. Listed, not scored.
5. **Resubmit** — Engineer. `lamp_fx.apply()` + `check_lights.py` (fixes 1-2), `check_dusk_blobs.py` (fix 3), then the capture run with the r4 fix-2 command line (townsfolk off): the three dusk views at minimum; the noon bookmarks and context views again only if any asset other than the map's lamp actors changes. Revert every `.uasset` the editor resaves except `MI_SB_LanternGlow`; list in the README the `LAMP_MATCH` lines, `LAMP_FX`, `LIGHT_CHECK`, `DUSK_BLOBS` / `DUSK_ORANGE` / `DUSK_WASH` lines and the warm-pixel counts against the fix-1 bands. A value outside a band is a known open item, not a pass.
