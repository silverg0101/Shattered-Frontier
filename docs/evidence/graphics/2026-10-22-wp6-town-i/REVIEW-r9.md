# WP6i round-9 review — Silverbrook town, pass 9 r9 (REVIEW-r8.md fix list 1-5)

Reviewer: independent (Fable), did not build the work. Judged per `docs/visual-review.md` against `Silverbrook-Design-Board-v1.webp` and `UI/v5/gameplay-hud.png` (mill plume x 1330-1360, y 100-170), the r8 fix list and its acceptances, the r1-r8 and WP6h standing acceptances, and the owner's four reports. Commit 014471a4. Every capture read at full resolution plus 2-6x crops and pixel measurements (`py -3`, PIL/numpy/scipy): `check_dusk_blobs.py` run on the r9 frames and again on the r8 frames pulled from `b9fc1b5c`; warm / hot pixel rings, cap-versus-pane luminance, wall-band columns and the side-b warm-blob map on the three dusk views; the noon panes at the same boxes; `measure_plume.py` re-run from a scratch copy against `chimney-smoke.png` / `-ref.png` / `motion-00..13.png`; the r9 diff of `lamp_fx.py`, `check_lights.py` and `check_dusk_blobs.py` read in full; `git log` on `dusk-panes.json`; `silverbrook_kit.lantern` (lines 337-358) for the lantern's parts. The editor MCP was down, so `LAMP_MATCH` / `LAMP_FX 731` / `LIGHT_CHECK 731` are the README's numbers, checked for consistency with the frames, not reproduced.

**Result: 17 PASS / 2 FAIL.** Bookmarks 5 / 0, dusk lamp views 1 / 2, context views 11 / 0.

Fixed this round and holding: **the building lanterns light their walls.** Frontage: the ashlar beside the bracket lantern carries a warm pool with the bracket's shadow across it (brightest 25-px column x 500, L 134, 114 px from the pane centre; the pool x 430-560, y 280-500 reads R - B +29; 10,690 warm px in the 150-px ring against 61 in r8). Townhouse1 (`context-lamp-wall-dusk`): the stones beside the door lantern x 520-700, y 900-1080 read median L 105, R - B +102, the arch voussoirs and the jetty soffit catch it, 19,645 warm px (r8: 34). The street-lamp cards are untouched (side-b far lamp: four panes, flame visible, no blob). The commit is clean: only `MI_SB_LanternGlow` and the map differ among the binaries. The plume is unchanged and inside every band (core L/ref 0.972 / 1.021 / 1.067, dense puffs R - B -4.1, hill 0.963, sky rows 0.976, growth 2.52, 0 thin rows in 15 frames, MAD 1.2 / 1.45 — reproduced). The r8 gate reproduces on the r8 frames (warm 61 / 745 / 34, exit 1).

Not met: **the light was put at the lantern's cap, not at its pane.** The README's own `LAMP_MATCH` lines say it: on all three houses the matched piece is 10 x 20 x 10 cm centred 19 cm *above* the json socket (Townhouse1 json z 228 -> "pane" z 247; Townhouse4 198 -> 217; Townhouse2 203 -> 222). That is the roof prism of `silverbrook_kit.lantern` (line 358: body + 0.14..0.24 m, 20 wide), never the 15 x 15 x 24 pane that sits *at* the socket (line 354, `body`), and the code then places the light at that piece's centre + 14 cm outward (lines 125-130). So every building light hangs level with the cap, 4 cm outside its edge, and the cap's outward slope — black iron — is blown to the colour of the glass: `context-lamp-wall-dusk` cap x 470-505, y 942-976 median L 143, p90 227, max 236 against the pane's 240; `house-frontage-dusk` cap p90 158, max 244 with 15 px over 235. The gate's own `DUSK_BLOBS` reports one outside blob per view (the cap, 346 px L 220 / 70 px L 226) and `hot_px` 6 / 15, exit 1; the README lists them as open. A player on the stoop sees a lantern whose iron roof glows like the glass (`cap5x`, `cap6x`). The fix is one line: the light goes at the socket (which is the pane centre by construction of `extract_sockets.py`), the piece match stays as the existence check.

Also not met in the commit: `dusk-panes.json` was **not changed** (last touched in `b9fc1b5c`; the side-b window box is still `[1700, 1920, 0, 1080]`), although README round 9 item 3 says "side-b uses the far lamp panes and four far windows instead of the blanket box". The `DUSK_WASH` line and bands were added to `check_dusk_blobs.py` as asked. `vfx.md` line 17 still says lights come from the `*Glow` slot scan, that the json points "were 2-20 m off", and `LIGHT_CHECK lights 795 ... shadowed 444`: three statements the r9 code contradicts.

## Owner reports

| Report | Status | Evidence |
|---|---|---|
| Floating houses | Fixed (WP6h), holds | `foundation-close`, `context-house-side-a/b/rear`: plinth under every wall, buried, no gap; assets unchanged. |
| Orange light with no visible source | Fixed on the frontage house and Townhouse1/2/4; one residue | Pools sit beside their lanterns with the bracket's shadow in them (frontage column x 500 vs pane 386; lamp-wall wall R - B +102). Residue: `LAMP_NOMATCH` on `SM_SilverbrookWeaponsVendor`'s two sockets (nearest piece 208 cm) — two lit panes per instance with no light, the r8 inverse on one building type (fix 4). |
| Street clutter in mirrored rows on an empty approach road | Fixed (r1), holds | `context-road-approach`: groups only in front of houses, no twin across the road, irregular gaps. |
| White street | Holds | Tan cobbles kerb to kerb in every street view. |
| Lamps lit at noon (J5) | Holds | Noon panes at the dusk boxes: side-b L 72, lamp-wall 87, frontage 138 (sunlit cream glass, no emission); every lantern in the noon views dark. |

## Round-8 fix items 1-5: met?

| Item | Verdict | Measured (this review) |
|---|---|---|
| 1 Building lantern chambers from the committed socket, light outside the opaque pane | **Partly**: lights placed, walls lit, but at the cap, not the pane | `LAMP_MATCH` dist 20 / size [10, 20, 10] on all three (acceptance expected about [15, 15, 24] at the socket); warm px 10,690 / 855 / 19,645 against bands 3,000 / 2,000 / 3,000; hot px 15 / 0 / 6 (band 0); `DUSK_BLOBS outside` 1 / 0 / 1 (band 0); side-b x 1250-1455 warm 158 (band 1,000 — unmeetable, see side-b); panes 240-241; cards gated to `StreetLamp_` (lines 283-288); `LAMP_FX` 731 (README; every json socket now lights, the 524 estimate assumed only the three townhouses were added); `vfx.md` not updated. Memory deviation (local 120 cm box instead of a full slot split) accepted: it is the method the review's own constraint demanded. `LAMP_NOMATCH` instead of `assert` accepted; its two hits are fix 4. |
| 2 `check_lights.py` independent of the wrong slot | **Met** | `glow_boxes(mesh, near)`: building = lantern-sized (8-35) components of any material within 40 cm of the light, street lamp = `*Glow` slots; threshold 20; `LIGHT_CHECK lights 731 on_glass 731 orphans 0 in_wall 0` (README). Note: with the light at cap height the gate passes on the cap piece, and after fix 1 it will pass on the same piece at 14-17 cm; still inside 20. |
| 3 Dusk gate: name the far-lamp panes, add `DUSK_WASH` | **Half**: script yes, json no | `DUSK_WASH` added with the r8 bands, exits 1 on r8 frames (61 / 745 / 34 reproduced) and on r9 (hot and side-b). `dusk-panes.json` unchanged in the commit; README claims otherwise. The `DUSK_ORANGE` test (R > B + 80, L > 150) now flags the lit stone itself (frontage 5 blobs, lamp-wall 10, all L 152-204 wall pixels): the r7 definition cannot coexist with a lit wall and needs the L bound raised (fix 2). |
| 4 Lamp family | Owner decision, open | Unchanged. |
| 5 Resubmit | Met in form | All views recaptured, ref and 14 motion frames, perf pair (1440p: chimney on - off -0.21 ms, street -0.03 ms at 731 lights; GT 0.09-0.12 ms, workers 0.32-0.36), only the two listed binaries differ. README carries `LAMP_MATCH`, counts and the wash numbers, and marks the out-of-band values open. |

## main-street — PASS

Unchanged street: tan cobbles kerb to kerb, five house tones, barrels, crates, hay cart, bench, ladder, signs; lamps and windows dark at noon; weeds at the kerb; nothing floating. Far threads pale on the sky. Seen, not failed: the white faceted range (J1b, deferred).

## market — PASS

Stalls with awnings, firewood sheds, ladder, sacks, mixed house tones, cobbles with earth margins, far houses on plinths, faint chimney thread. Stall stand-in: GFX-03, deferred. Unchanged.

## house-frontage — PASS

Bay window with planter, boot sign, bracket lantern dark at noon (pane L 138, sunlit cream), stoop with tub, ivy, plinth; unchanged. Seen, not failed: the kerb-high cracked-mud bank at x 0-400, y 600-820 (J2, deferred).

## foundation-close — PASS

Rubble plinth under the ashlar on both faces of the corner, flush, buried; tub and planter on the stoop. Unchanged.

## chimney-smoke — PASS

Re-measured on `chimney-smoke.png`, `-ref.png`, `motion-00..13.png` (scratch copy of `measure_plume.py`; numbers equal the committed `plume-measure.json`).

| Acceptance (r7 fix 4 / r8) | r9 | Met |
|---|---|---|
| densest puffs R - B <= 5 and >= -12 | -4.1 | Yes |
| core L/ref p5 >= 0.90, p50 and mean 1.00-1.15 | 0.972 / 1.021 / 1.067 | Yes |
| hill (y 60-300) p50 0.95-1.15 | 0.963 | Yes |
| sky rows (y < 60) p50 0.90-1.05 | 0.976 | Yes |
| growth >= 2.0 at y 300 | 260 / 103 = 2.52 | Yes |
| 0 thin rows, 15 frames | 0 | Yes |
| MAD >= 1.0 | 1.2 min, 1.45 mean | Yes |

Frames 00 / 07 / 13 differ in puff placement; a soft pale-grey wisp leaves the pot, widens over the ridge and thins over the hazy hill, the same weight as the HUD wisp. No plume item.

## house-frontage-dusk — FAIL

Bay panes lit, the stoop, tub and planter readable (bottom quarter 9.4), moon shadow across the field, far cottages with lit windows, open sky at x 0-250 L 108 (toward the sun, band 10-120; r8 106), no orange blob off the wall. New and right: the warm pool on the ashlar x 430-560 with the bracket's shadow through it, the arch stones warm, the pool brightest 114 px from the pane.

1. **What's wrong:** the lantern's iron roof glows. Cap x 362-412, y 350-374: p90 L 158, max 244, 15 px over 235 at x 371-374, y 358-363 — the top-left slope of the cap is the colour of the glass (`cap6x`); the pane itself is L 241. The light sits 19 cm above the pane centre (README `LAMP_MATCH`: json z 228 -> piece z 247 on this kit lantern; frontage house same code path), level with the cap and 4 cm outside its edge, so the cap's outward slope is lit at point-blank range. `DUSK_BLOBS outside 1 (369, 378, 356, 371, 70 px, L 226)`, `hot_px 15`; acceptance was 0 / 0. **Fix type:** CODE. **Exact instruction:** fix 1.

## context-house-side-b-dusk — PASS

Window glass lit, bottom quarter 18.3, the bench and clothes line cast moon shadows, the far street lamp's four panes lit with a flame, the far house's windows and door lanterns lit, sky x 0-250 L 82 (band 10-60 away from the sun; r8 82 — the standing r7 reading, not re-scored); no blob, no orange, no hot pixel. The corner lantern (x 1455-1500, y 228-276, L 240) hangs under the jetty on the front face, which faces away from this eye; what it lights that the eye can see is the jetty soffit directly above it (x 1487-1499, y 161-167, L 164, R - B +40; a second glint at x 1482-1500, y 160-167) and the top riser of the stoop against the corner post (`stoop3x`), both warm. Nothing of the side wall (x 1250-1440: L 17.6, R - B -35) can be reached by a light round the corner with `cast_shadows` on, so the r8 band "2,000 warm px, 1,000 in x 1250-1455" was not a thing this geometry can show; the 855 / 158 counted are the far house's windows plus the soffit. The view reads as a place: a lit lantern, a glint on the beam above it, dark stone on the moon side. Fix 2 sets side-b's band to what this eye can see; the cap defect of fix 1 is not visible from here (the cap is seen from behind, dark).

## context-lamp-wall-dusk — FAIL

Townhouse1 (`TheCommons_townhouse_037`): gable, jetty, lit upper and bay windows, stag sign, sky 54, bottom quarter 31.3. New and right: the door lantern lights the ashlar — x 520-700, y 900-1080 median L 105, R - B +102, the voussoirs and the soffit warm, the bracket arm's shadow on the wall, 19,645 warm px.

1. **What's wrong:** the lantern's cap and bars glow like the glass. Cap x 470-505, y 942-976: median L 143, p90 227, max 236; the vertical iron bars' outer faces orange (`cap5x`): the lantern reads as one orange block with a pyramid top, not glass in an iron frame. `DUSK_BLOBS outside 1 (476, 499, 947, 973, 346 px, L 220)`, `hot_px 6` at x 485-487, y 955-957. Same cause as the frontage: the light at the cap's height (json z 228 -> piece z 247). **Fix type:** CODE. **Exact instruction:** fix 1.
2. **What's wrong (gate, not the frame):** `DUSK_ORANGE outside 10`: every one is wall stone at L 152-204 inside the pool (largest 593-637, 1036-1070, 631 px, L 164). The r7 "orange dash" test fires on the lit wall it was meant to protect. **Fix type:** CODE. **Exact instruction:** fix 2.

## Context views

### context-overview-se — PASS

Eye 230 m up: the whole town, forest east and south-east, roads out of each gate, wall extremes in frame; no claim-pad slabs. Unchanged. Seen, not failed (deferred): lake causeway, straight canal and cut banks, flat cyan water, white faceted ring (J1b, GFX-02).

### context-overview-nw — PASS

Westfields meadow, roads, river and bridges fill the bottom; the smooth white dome at x 0-260, y 0-210 is at its 1.2 km ring (J1b). Unchanged.

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

Plinth stepping to the stoop base; stag sign, bracket lantern (pane L 72, in shade) and iron lamp dark; forecourt and street clear of stones. Unchanged.

### context-house-side-rear — PASS

Rear plinth following the slope, firewood shelter, fence, lamps dark. Unchanged.

### context-lamp-wall — PASS

Townhouse1 gable in sun: stag sign, bracket lantern dark at noon (pane L 87, cream in sun), no wash, windows dark, sill plaster plain. Same house as the dusk frame.

### context-road-approach — PASS

Both sides fronted by houses, groups irregular, no twin across the road, timber-post lamps dark, no stones on the carriageway; chimney threads pale on the sky. Unchanged.

## Deferred (seen, not failed; GFX-02 / GFX-03 / owner, unchanged)

Bank texture streaks and bedrock faces on kerb-high banks (J2); the white faceted mountain ring and the far domes (J1b), 12 m cobble roads outside the wall (J1b); flat cyan water, lake causeway, canal and its cut banks, the zigzag fence lines on the Westfields field (GFX-02 world); ashlar repeats, plaster, stall stand-in, low-poly trees and shrubs, the same shrub mesh repeated (GFX-03); townsfolk mannequins (off for the captures); street-lamp family (r1 fix 9, owner); the claim pad's final form (DEC-03 levelling).

## Fix list (shared first)

1. **Light at the pane centre, not at the matched piece** — Engineer, CODE, `GameClient/ArtSource/Greenlands/lamp_fx.py` `glass_sockets`. The json socket *is* the pane centre (`extract_sockets.py`: the centre of the `*LanternGlow` island; `silverbrook_kit.lantern` line 354 puts the 15 x 15 x 24 pane at `body`, the roof prism at `body + 0.14..0.24`, line 358). The nearest lantern-sized piece is only the proof a lantern is there. Replace lines 125-131:
   ```python
               c = [(b[i] + b[i + 3]) / 2 for i in range(3)] if b else None
               u.log(f'LAMP_MATCH {name} json {[round(v) for v in t]} pane {c and [round(v) for v in c]} size {b and [round(b[i + 3] - b[i]) for i in range(3)]} dist {round(d)}')
               if d > 40:   # no kit lantern there (e.g. SM_SilverbrookWeaponsVendor's z 112 socket, nearest piece 208 cm): no light from nowhere
                   u.log(f'LAMP_NOMATCH {name} json {[round(v) for v in t]}'); continue
               a = max(range(2), key=lambda i: abs(c[i] - centre[i]) / half[i])   # the horizontal axis the lantern overhangs on
               c[a] += PANE_CLEAR if c[a] >= centre[a] else -PANE_CLEAR
               out.append(c)
   ```
   with
   ```python
               u.log(f'LAMP_MATCH {name} json {[round(v) for v in t]} piece {b and [round((b[i] + b[i + 3]) / 2) for i in range(3)]} size {b and [round(b[i + 3] - b[i]) for i in range(3)]} dist {round(d)}')
               u.log(f'LAMP_PIECES {name} ' + ' '.join(f'{[round((q[i] + q[i + 3]) / 2 - t[i]) for i in range(3)]}:{[round(q[i + 3] - q[i]) for i in range(3)]}' for q in pieces(sub)))
               if d > 40:   # no kit lantern there (e.g. SM_SilverbrookWeaponsVendor's z 112 socket, nearest piece 208 cm): no light from nowhere
                   u.log(f'LAMP_NOMATCH {name} json {[round(v) for v in t]}'); continue
               c = list(t)   # the socket is the pane centre (extract_sockets: the *LanternGlow island centre); WP6i r9 lit the cap by using the matched piece
               a = max(range(2), key=lambda i: abs(c[i] - centre[i]) / half[i])   # the horizontal axis the lantern overhangs on
               c[a] += PANE_CLEAR if c[a] >= centre[a] else -PANE_CLEAR
               out.append(c)
   ```
   (`pieces(sub)` is recomputed once more per socket for the log; the sub-mesh is small.) Update the docstring sentence "confirmed by a lantern-sized connected piece ... within 40 cm" to "... the light goes at the socket; a lantern-sized piece within 40 cm only confirms a lantern is there". No change to `_point_light`, `PANE_CLEAR`, `LANTERN_AXES` or `check_lights.py` (the light moves 19 cm down and 5 cm along the wall; its gap to the matched piece becomes about 14-17 cm, under the gate's 20; if `LIGHT_CHECK` orphans any building light, report the `LAMP_PIECES` line for it rather than raising the threshold). With the light at the pane's mid-height the cap's outward slope (plane through 10 cm out / +14 cm up rising inward) lies above and behind the light, so it stays dark iron. **Acceptance:** `LAMP_MATCH` lines for `SM_SilverbrookTownhouse1`, `2`, `4` and the frontage mesh with `dist <= 40`; `LAMP_PIECES` lines in the README for the same four; `house-frontage-dusk` cap x 362-412, y 350-374 p90 L <= 120 and `context-lamp-wall-dusk` cap x 470-505, y 942-976 p90 L <= 120 (a dark iron roof over a L >= 200 pane); `check_dusk_blobs.py` exit 0 with the fix-2 definitions (`DUSK_BLOBS outside 0`, `hot_px 0` on all three; warm >= 3,000 / 50 / 3,000); the wall pools stay: frontage x 430-560, y 280-500 median R - B >= +20, lamp-wall x 520-700, y 900-1080 median L >= 80 and R - B >= +60; `LIGHT_CHECK lights N on_glass N orphans 0 in_wall 0` with N = `LAMP_FX`'s count.
2. **Dusk gate definitions** — Engineer, CODE, `docs/evidence/graphics/2026-10-22-wp6-town-i/check_dusk_blobs.py`. (a) The orange test was written for a flame card on a wall and now fires on lit stone: change `(a[..., 0] > a[..., 2] + 80) & (L > 150)` to `(a[..., 0] > a[..., 2] + 80) & (L > 200)` and the docstring line "R > B + 80 and L > 150" to match (the r7 dash read L > 200; lit ashlar peaks at L 204). (b) `warm` must exclude every pane and window box, not just the subject lantern: build `ring` with `~inside(boxes)` where `inside` is the same "box grown by 10 px" test as `outside()`, applied to `p['lantern'] + p['windows']`. (c) `WARM = {'house-frontage-dusk': 3000, 'context-lamp-wall-dusk': 3000, 'context-house-side-b-dusk': 50}` with a comment: side-b's lantern lights a face the eye cannot see; 50 is the soffit glint and the stoop riser. Drop the `warm_x1250_1455` column. Add `cap` boxes to `dusk-panes.json` (fix 3) and print `DUSK_CAP <view> p90 <L>` for each, failing when p90 > 120. **Acceptance:** exit 1 on the r9 frames (caps, hot px), exit 0 on the fix-1 recaptures; the orange test still fails a synthetic L 230 orange patch painted on an r9 frame (state the test in the README).
3. **`dusk-panes.json`: the boxes r8 fix 3 asked for, not committed in r9** — Codex, LEVEL, `docs/evidence/graphics/2026-10-22-wp6-town-i/dusk-panes.json`. In `context-house-side-b-dusk` replace the window box `[1700, 1920, 0, 1080]` with the far street lamp's panes `[1717, 1735, 344, 372]`, `[1785, 1803, 344, 372]` and the far house's windows `[1455, 1475, 300, 321]`, `[1440, 1449, 345, 370]`, `[1479, 1495, 347, 371]`, `[1550, 1561, 345, 367]` (measured on the r9 frame; none wider than 120 or taller than 150). Add a `cap` key per view: frontage `[[362, 412, 350, 374]]`, lamp-wall `[[470, 505, 942, 976]]`, side-b `[[1455, 1500, 205, 228]]`. Update `_about`. **Acceptance:** the committed file differs from `b9fc1b5c`; `check_dusk_blobs.py` reads the caps.
4. **`SM_SilverbrookWeaponsVendor`: two lit panes with no light** — Engineer, CODE/LEVEL. `LAMP_NOMATCH` at json (-46, 25, 112) and (256, 13, 248), nearest lantern-sized piece 208 cm: on that mesh the json does not sit at its glass, which is the owner's "lit source with no light" on every instance of that building. Run `py -3 GameClient/ArtSource/VFX/StreetLamp/extract_sockets.py` and diff its WeaponsVendor entries against the committed json; if equal, the imported mesh differs from the GLB (log `LAMP_PIECES` with the box widened to 250 cm for those two sockets, move each json point to the lantern-sized piece it reports, in Blender metres with Y un-mirrored, or delete the socket when no such piece exists and note that the pane's glow is then a window, not a lantern). **Acceptance:** `LAMP_NOMATCH` absent from the README's log; `LAMP_FX` count = 731 + the vendor instances x 2 (or the socket removed and the count explained).
5. **`docs/blueprints/vfx.md` line 17 describes the r7 code** — Codex, docs. Replace "Light and flame positions come from each imported mesh's own `*Glow` material pieces (... the JSON now only lists eligible meshes)" with: "Street lamps: light and cards at the connected pieces of the mesh's `*Glow` slots merged within 60 cm. Buildings: the light at the committed `lamp-sockets.json` point (Blender metres, Y mirrored on import: (x, -y, z) x 100), which is the pane centre, pushed `PANE_CLEAR` 14 cm outward of the opaque emissive pane along the overhang axis, only when a lantern-sized (8-35 cm) piece lies within 40 cm (`LAMP_MATCH`; otherwise `LAMP_NOMATCH`, no light); building lanterns get no flame, halo or smoke cards (the pane is the flame)." Replace "(444 of 795)" and the `LIGHT_CHECK lights 795 ... shadowed 444` gate line with the r10 counts from fix 1's README. **Acceptance:** no "2-20 m" / "795" in `vfx.md`.
6. **Lamp family** — owner decision (r1 fix 9), unchanged. Listed, not scored.
7. **Resubmit** — Engineer. `lamp_fx.apply()` + `check_lights.py` (fix 1), `check_dusk_blobs.py` with the new json (fixes 2-3), then the capture run with the r4 fix-2 command line (townsfolk off): the three dusk views at minimum; noon bookmarks and context views only if any asset other than the map's lamp actors changes. Revert every `.uasset` the editor resaves except `MI_SB_LanternGlow`. README: the `LAMP_MATCH` / `LAMP_PIECES` / `LAMP_NOMATCH` lines, `LAMP_FX`, `LIGHT_CHECK`, the full `DUSK_*` output and the cap p90 values against fix 1's bands. A value outside a band is a known open item, not a pass; a file the README says changed must be in the commit.
