# WP6e review — Silverbrook town, pass 5 (WP6d fix list)

Reviewer: independent (Fable), did not build the work. Judged per `docs/visual-review.md` against `Silverbrook-Design-Board-v1.webp`, `UI/v5/gameplay-hud.png` and the owner's screenshots 16, 18 and 19. Commit bdc74a4. Every capture read at full resolution plus 2-5x crops; luminance and hue measured on `chimney-smoke.png`, `main-street.png` and `market.png` (sRGB, L = .2126 R + .7152 G + .0722 B). Code read: the bdc74a4 diff (`silverbrook_props.py`, `silverbrook-props.json`, `build_silverbrook.py`, `paint_street_edges.py`, `build_materials.py`, `make_chimney_smoke.py`, `build_prophaybales.py`, `make_straw_scan.py`), `capture_game.py`, `check_details.py`, `apply_dressing.py`, and `NatureFamilies/import_ground.py` (the M_GN_Ground blend order, for the market seam).

**Result: 3 PASS / 2 FAIL.** The street is painted to the kerb, the hay is straw, every bay has its planter, the capture is unblocked and the smoke is no longer white light. Two things remain: the smoke taper is upside down (a dense slab at the pot, a thread at the top), and a grass band now runs along the market plaza's edge where the cobble ramp meets the earth band.

## Owner complaints

| Complaint | Status | Evidence |
|---|---|---|
| 16.webp: thin glossy plank foundation | **Fixed** (unchanged) | `foundation-close.png`: matte rubble course across x 0-1230, y 520-900, three steps at x 0-560; `foundation-check.json` 312 houses, 0 failures, 65 cm minimum plinth. |
| 18/19.webp: coloured lines, 3D labels, "Tt" sprites | **Fixed** (unchanged) | No line, label or sprite in any capture; 126 debug actors hidden, 8,723 visible meshes, 0 default/graybox slots. (The cyan strip in `house-frontage.png` x 1760-1920, y 715-740 and `chimney-smoke.png` x 0-520, y 550-600 is the brook's water plane, `MI_GN_River`, not a debug line; see Deferred.) |
| 17.webp / pass-1: bare strip, one cart, identical rows | **Fixed** (unchanged) | `main-street.png`: both margins dressed for the visible length, five house tones, mixed packages, no green on the carriageway. |

## WP6d fix list — status

| # | Item | Status |
|---|---|---|
| 1 | Smoke darker than the sky, soft-edged, tapered, chimney in frame | **Half.** 1a/1b (lit, base colour .42) resolved: the plume is neutral tan (R-B -1 vs sky -15) and iso-luminant with the sky (row 30: plume L 170 vs sky 168/169; row 80: 164 vs 163/164). It no longer reads as light. The "15 % darker" number is withdrawn: the HUD v5 concept's own chimney smoke above the mill (`main-street-hud-compare.png` left panel, x 765-800, y 90-140) is lighter than its sky; neutral and within 10 % of the sky is the right target. 1c (edge columns zero) resolved: no card border visible. 1e resolved: the stack top is in frame at x 860-940, y 580-760 under the plume. 1f resolved: the far rows are faint tan threads, not white pickets (x 1598-1614 and 1772-1792, y 260-370: R-B +6 to +15 against the sky's -3 to -8, L within 2-7 of the sky at y 280). **1d (taper) is inverted**, see chimney-smoke 1. |
| 2 | Streets and the market square painted at full weight | **Resolved on the street, new seam on the plaza.** `main-street.png`: every 100 x 100 block in y 620-1080 has G-R between -13 and -51 (acceptance was <= 8); the carriageway is cobble to both kerbs. `market.png`: the square is cobble, the 10 m earth band reads as packed earth, the firewood stores stand on earth; but a yellow-green band runs along the plaza ramp, see market 1. |
| 3 | Sill planters on every bay package | **Resolved.** `house-frontage.png` x 780-1080, y 660-750: terracotta trough on the Townhouse1 bay's front face (front face x 790-1175; the planter is centred on it, the cants at x 740-790 and 1250-1445 are clear), eight pink umbels, trailing stems. `foundation-close.png` x 650-1120, y 0-130: the same trough from below. `check_details.py`: 140 of 140. |
| 4 | Props do not collide, captures not blocked | **Resolved.** `foundation-close.png`: plinth, three steps, door jamb, stoop tub (x 240-370, y 320-520), bay apron, flower tub (x 1230-1500, y 570-1000) and ivy, nothing within 1.5 m; no tub geometry inside any barrel; 0 group/detail overlaps. |
| 5 | Hay bales read as straw | **Resolved.** `main-street.png` x 190-420, y 570-760 and x 1510-1730, y 560-740: golden cut-face texture with visible stalk grain, dark 12 mm twine, loose stalks 0.12-0.25 m standing out of the faces and ends, shadowed gap between the bottom bales. No face reads as a flat box. |
| 6 | Chimney view shows the chimney | **Resolved** (folded into 1e). |
| 7 | Side-window void on Townhouse1 | **Closed, accepted.** See deviations. |

## Builder's deviations

1. **Sill coordinates re-traced on the imported meshes** (import recentres each mesh on its bounds; Townhouse1 +26 cm x, -18 cm y) — **accepted**. The capture shows the planter's back against the sill line of the bay's front face, not floating in front of it (`house-frontage.png` x 780-1080: the trough's top edge at y 660 meets the lattice bottom at y 690; the apron behind it is in its shadow). The trace gate passes on all 140.
2. **Sight-line assert excludes the framed house's own details; frontage eyes 1 m closer, FOV 65 -> 72.4 and 48 -> 54.1** — **accepted**. The subject's planter, tubs, sign and ivy are what the view is for; the frame still holds the door jamb, stoop, tub, bay, sign, lantern and the corner, and `StreetGroup_SouthStreet_068_L` is behind the camera. The wider FOV adds no visible distortion at the bay.
3. **Fix 7 closed with no change (grazing angle only)** — **accepted, verified against the image.** `house-frontage.png` x 1710-1750, y 400-690: the side opening shows a timber frame, a lit reveal face on its near side (x 1715-1735) and a dark inner plane; at this angle (the side wall is ~80 degrees to the view) a glass plane 13 cm behind the wall face projects to under 3 px, so no lattice is expected. The trace evidence (hit 12.5-14 cm behind the face on both sides, at local y -100) is consistent with the glazing being present. Not a void.
4. **`build_silverbrook.py` paint change applies on the next full build; the open map repainted with `paint_street_edges.py`** — **accepted**. The diff shows the same fields in both (ramp one grid step, 1 to the edge, plaza cobble plus a 10 m earth band, lanes one vertex column), so the next rebuild reproduces this paint. Fix 2 below changes both again.
5. Texture zero ramp offset half a pixel; material also zeroes the card's own U edges because the panner drifts U — **accepted**, no card border in any capture.

## main-street — PASS

Matches the board's TOWN STREETS tile in layout and treatment apart from the deferred items: cobble carriageway to both kerbs with no green (every 100 x 100 block G-R <= -13), barrels, crate, jug on a barrel, hay stacks reading as straw, sacks, bench, hay cart, ladder, bracket signs, washing line, five house tones, lamps. The chimney threads above the right roofs (x 807-817, y 345-405; x 1152-1162, y 330-370) are 10-15 px wide but tan and no brighter than the sky (L 172-176 vs 175-180, R-B +3 to +11 vs -8 to -14): they read as faint smoke at 40-60 m, and they widen once fix 1 is in. Lamps lit at noon and the far faceted mountains are deferred (GFX-02 J5, J1b).

## market — FAIL

1. **What's wrong:** A yellow-green band 5 m wide runs diagonally along the plaza's edge between the cobble and the earth band: from x 960-1080, y 1010-1080 up to x 1320-1440, y 770-890. Greenness G-(R+B)/2 measured in 60 x 60 blocks is 10-21 along the band (peak 21.5 at x 960-1020, y 1010) against 1.6 on the cobble and 1.2 on the earth; the band's mean colour is (162,138,83) where the earth is (119,89,56). It reads as a mossy lawn edge around a paved square.
   **Root cause:** `M_GN_Ground` (`NatureFamilies/import_ground.py` 160-165) lerps the cobble in by G first and the earth in by B second, and the paint keeps earth out of the cobble (`B = f * (1 - G)`, `build_silverbrook.py` 765/778, `paint_street_edges.py` 97). On the one-step ramp G passes through 0.45-0.6 where the cobble weight is 0.25-1 and B is 0.4-0.55, so the earth weight is 0-0.75: whatever neither covers is the forest-floor grass. Every street ramp has the same seam; it shows here because the plaza ramp runs across the frame 8 m away.
   **Fix type:** CODE + LEVEL. **Instruction:** fix 2.

Resolved here: no needle above the central chimney (x 1380-1400, y 285-335 is within 2 % of the sky), the square is cobble, the earth band is packed earth with no lawn, eight stalls, goods, grindstone, firewood racks. The left house's orange window glow at noon is deferred (GFX-02 J5).

## house-frontage — PASS

The Townhouse1 package reads as the board's cottage elevation: ashlar plinth, round-headed door, lantern, bracket sign, canted bay with leaded lights, sill planter across the bay's front face with pink umbels and trailing stems, stoop tub with red geraniums, garden fence at the corner. Side window accepted as a grazing-angle view of a recessed light (deviation 3). The road embankment at x 0-400, y 690-800 is deferred (GFX-02 J2); the brook's flat cyan plane at x 1760-1920, y 715-740 is outside this pass (see Deferred).

## foundation-close — PASS (regression cleared)

Plinth with the rubble course 4 cm proud, three steps, door jamb, stoop tub with blooms, bay apron with its gothic panels, sill planter underside, flower tub and ivy at the corner, cobble to the plinth. Nothing within 1.5 m of the eye; no geometry intersects (the ivy's leaf cards hang in front of the tub rim at x 1500-1540, y 700-830, which is how ivy drapes, not a clash). The geranium blooms at 1.5 m are flat five-petal discs (x 1290-1460, y 575-640): noted for GFX-03 prop polish, not failed, since the prop was accepted at the 3 m it is designed for.

## chimney-smoke — FAIL

1. **What's wrong:** The plume is widest and densest at the pot and thinnest at the top, the opposite of the fix and of any chimney plume. Measured (columns with R-B more than 8 above the sky's): row 30 x 885-1002 (117 px), row 80 x 910-981 (71 px), row 260 x 914-987 (73 px); under the card's lower third the slate roofs at x 820-980, y 470-560 read L 157-161 against 78-90 for the same slate at x 1060-1220, i.e. a near-opaque tan slab the width of the card (2.2 m = 290 px at 14 m) sits on the roofs, and the field behind the card at x 700-860, y 380-440 is +5 to +9 L warmer than the open field. The card's V = 0 is at the pot, so `lerp(.7, 1.6, V)` squeezes the top and spreads the bottom.
   **Fix type:** CODE. **Instruction:** fix 1a.
2. **What's wrong:** The column's two edges run straight and parallel for the full 700 px from the frame top to the roofs (`cs-plume` crop): the texture's alpha is `profile * (.35 + .75 n)`, never below 35 % of the Gaussian profile, so there are no gaps or puffs, and the 0.18 sway is 40 px at this distance. Acceptance was "no straight edge longer than 100 px".
   **Fix type:** CODE. **Instruction:** fix 1b/1c.
3. **What's wrong:** At opacity 0.7 the lit smoke doubles the brightness of whatever is behind the card (roofs 78-90 -> 157-161). A plume at 14 m should veil the roof, not replace it.
   **Fix type:** CODE. **Instruction:** fix 1d.

Resolved here: plume neutral tan, within 2 % of the sky's luminance (criterion revised, see fix-list status 1); card edges invisible; stack top in frame under the plume; far rows faded to threads. Far white faceted mountains deferred (GFX-02 J1b/J2).

## Deferred to GFX-02 / GFX-03 (seen, not failed here)

- Flat cobble and lawn tiles without relief; the cobble paint stretched down the bank in `house-frontage.png` x 0-400, y 690-800 — GFX-02 J2.
- Lamps and windows lit at noon (`main-street.png` lamps, `market.png` left house x 1270-1330, y 380-500) — GFX-02 J5.
- Far mountains: white faceted shapes (`main-street`, `chimney-smoke`), smooth domes (`market`) — GFX-02 J1b/J2.
- **Water surface (new line needed):** the brook renders as a flat, opaque cyan plane with no reflection, depth tint or bank edge (`house-frontage.png` x 1760-1920, y 715-740; `chimney-smoke.png` x 0-520, y 550-600, `MI_GN_River`). Zone-wide terrain material, not part of the town fix list; add it to GFX-02 as "water: reflection, depth tint, bank foam", with the board's river (right panel, x 1500-1750, y 450-650) as the reference.
- Ashlar tiles repeat, plaster is flat, no corner wrap — GFX-03 1-2.
- `SM_MarketStall` stand-in with flat striped cloth — GFX-03 gap 7.
- No decals, no per-instance weathering — GFX-03 gaps 5-6.
- Flower and stoop tub blooms are flat discs under 2 m (`foundation-close.png` x 1290-1460, y 575-640) — GFX-03 prop polish: `PropFlowerTub` umbels as 12-16 florets on a dome, not five petals on a disc.

## Fix list (shared first)

1. **Smoke plume narrow at the pot, wide and broken into puffs at the top, veiling not covering** — Engineer, CODE. Covers chimney-smoke 1-3 (and widens the main-street threads).
   a. `GameClient/ArtSource/VFX/StreetLamp/build_materials.py` `build()`: the card's V = 0 is at the pot (the capture proves it: 71-117 px at the top of the frame, a 290 px slab at the roofs), so swap the taper ends:
      `_link(_constant(smoke, .7), widen, 'A'); _link(_constant(smoke, 1.6), widen, 'B')` -> `_link(_constant(smoke, 1.6), widen, 'A'); _link(_constant(smoke, .7), widen, 'B')`
      and correct the comment to "U is squeezed x1.6 at the pot (V = 0) and x0.7 at the top (V = 1)". Then check the rise direction once in PIE: if the puffs travel toward the pot, set `chimney_smoke.pan_v` in `silverbrook-props.json` to `-0.12` (the declared range allows it) and note the sign in the comment. Do not restate the V orientation from the spawn roll; take it from the capture.
   b. Fade the top so the plume disperses instead of ending at the card's sine edge: after `widen`, add
      ```python
      thin = _node(smoke, u.MaterialExpressionLinearInterpolate)
      _link(_constant(smoke, 1.), thin, 'A'); _link(_constant(smoke, .45), thin, 'B'); _link(vs, thin, 'Alpha')   # full at the pot, 45 % at the top
      ```
      and multiply `thin` into the final `MP_OPACITY` product beside `far` and `sides`.
   c. `make_chimney_smoke.py`: puffs with real gaps and more drift. `alpha = np.clip(profile * (.35 + .75 * n), 0, 1) * 1.0` -> `alpha = np.clip(profile * np.clip((n - .3) / .7, 0, 1) * 1.4, 0, 1)`; `sway = (periodic_noise(3)[:, :1] - .5) * .18` -> `* .30`. Keep the edge-zero line and the assert (max alpha still reaches 1 where n = 1). Rerun, reimport `T_ChimneySmoke`.
   d. `silverbrook-props.json` `chimney_smoke.opacity`: `0.7` -> `0.5`.
   Acceptance (`chimney-smoke.png`, plume = columns whose R-B exceeds the sky's by 8): width at y 30 >= 1.8 x the width 60 px above the stack top (x 860-940, y 580); mean plume L within 10 % of the sky 50 px either side at rows 30, 80 and 260; the plume's left and right edge x, taken per row, has a standard deviation >= 8 px over every 100-row window from y 0 to 500; slate under the card at y 470-560 keeps L <= 1.5 x the clear slate at x 1060-1220; the stack top stays in frame. `main-street.png`: threads above the right roofs >= 20 px wide at y 345-405 and no brighter than the sky.
2. **Cobble fades into earth, never grass** — Engineer, CODE (material) + LEVEL (paint). Covers market 1; removes the same seam from every street ramp.
   a. `GameClient/ArtSource/Greenlands/NatureFamilies/import_ground.py` `M_GN_Ground`, line 160: blend the earth in first and the cobble on top, so the cobble's soft edge lands on earth:
      `for ch, (c_, n_, r_) in ((('G', 0, 1, 0), (rc, rn, rr)), (('B', 0, 0, 1), (tc, tn, tr))):` -> `for ch, (c_, n_, r_) in ((('B', 0, 0, 1), (tc, tn, tr)), (('G', 0, 1, 0), (rc, rn, rr))):`
      Update the comment at line 142 ("earth under the cobble; cobble wins where G is high"). Run it in the editor (`tools/Run-InEditor.py`) to rebuild `MI_GN_Ground`.
   b. Paint earth under the cobble, not only outside it. `build_silverbrook.py` lines 765 and 778: `ROAD_B[_k] = max(ROAD_B.get(_k, 0.0), _f * (1.0 - ROAD_G.get(_k, 0.0)))` -> `ROAD_B[_k] = max(ROAD_B.get(_k, 0.0), _f)` (both places; the earth field is 1 inside the cobble, which is now harmless). `paint_street_edges.py` line 97: `nb = b.get(key, 0.0) * (1.0 - ng)` -> `nb = max(c.b, b.get(key, 0.0))`, and drop the "earth only outside the cobble" clause from the docstring. Repaint the open map with `paint_street_edges.py`, rerun `check_ground.py`.
   Acceptance: `market.png` y 650-1080 has no 60 x 60 block with G-(R+B)/2 above 6 (today 10-21 along x 960-1440, y 770-1080; cobble 1.6, earth 1.2); `main-street.png` keeps every 100 x 100 block in y 620-1080 at G-R <= 8; trails outside the wall unchanged (spot-check one trail crossing in the editor: dirt, not cobble, where G = 0).

Resubmit with `chimney-smoke`, `market` and `main-street` recaptured (the smoke change touches the street view); `house-frontage` and `foundation-close` stand unless the dressing is rerun.
