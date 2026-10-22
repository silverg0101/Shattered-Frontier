# WP6f review — Silverbrook town, pass 6 (WP6e fix list)

Reviewer: independent (Fable), did not build the work. Judged per `docs/visual-review.md` against `Silverbrook-Design-Board-v1.webp`, `UI/v5/gameplay-hud.png` and the owner's screenshots 16, 18 and 19. Commit 8cfddf5. Every capture read at full resolution plus 2-4x crops; luminance (L = .2126 R + .7152 G + .0722 B) and R-B / G-(R+B)/2 measured on `chimney-smoke.png`, `market.png` and `main-street.png`, with the WP6e captures measured the same way for comparison and e-to-f pixel diffs on the three views that passed before. Code read: the 8cfddf5 diff (`build_materials.py`, `make_chimney_smoke.py`, `silverbrook-props.json`, `import_ground.py`, `build_silverbrook.py`, `paint_street_edges.py`), `silverbrook_props.py` (card spawn: 2.2 x 6 m plane, roll -90, bottom 20 cm above the pot, two cards at yaw +35 / +125), `capture_game.py` (eye 14 m out at card-centre height, FOV 55: the card spans y -66 to 734, 290 px wide face-on), `apply_dressing.py`. The committed `T_ChimneySmoke.png` was run through the material's opacity chain (taper, thin, sine end fade, sides, opacity .5) to check the capture against the design.

**Result: 4 PASS / 1 FAIL.** The market seam is gone and the three earlier passes hold with no regression. The chimney plume is still wrong way up: 60-90 px above the pots it spans 229-275 px (1.7-2.1 m), at the frame top 124-140 px, and its densest part is mid-card. The WP6e fix was implemented exactly as written; the taper swap is right, but the sine end fade (0.56 at the pots, 1.0 mid-card, 0.31 near the top) multiplied by `thin` and opacity .5 leaves the pot end at alpha <= 0.10 and the top at <= 0.08 with a 0.36 bulge in between, and the taper's x1.6 U scale samples past the texture's edges, so the wrapped neighbour column adds a faint 85 px band at the card edge near the pot.

## Owner complaints

| Complaint | Status | Evidence |
|---|---|---|
| 16.webp: thin glossy plank foundation | **Fixed** (unchanged) | `foundation-close.png`: matte rubble course x 0-1230, y 520-900, three steps; `foundation-check.json` 312 houses, 0 failures, 65 cm minimum plinth. |
| 18/19.webp: coloured lines, 3D labels, "Tt" sprites | **Fixed** (unchanged) | No line, label or sprite in any capture; 126 debug actors hidden, 8,723 visible meshes, 0 default/graybox. The pale zigzag ribbon across the field in `chimney-smoke.png` x 0-700, y 500-570 is the brook's water plane at a grazing angle (it continues into the cyan strip at x 0-50, y 590-610), present in WP6e too; see Deferred. |
| 17.webp / pass-1: bare strip, one cart, identical rows | **Fixed** (unchanged) | `main-street.png`: both margins dressed, five house tones, mixed packages, no green on the carriageway (worst 100 x 100 block G-R = -14.2). |

## WP6e fix list — status

| # | Item | Status |
|---|---|---|
| 1a | Taper ends swapped; rise direction measured, `pan_v` -0.12 | **Done as specified, acceptance not met.** The puffs now rise (builder's frame test, consistent with the panner sign). Plume span (columns whose R-B exceeds the background interpolated across x 820-1070 by 4): y 10: 869-999 (124 px), y 30: 874-1016 (140), y 60: 889-1025 (137). Over the field 60-90 px above the pots (L excess over the same-row field at x 560-700, fitted linearly): y 460: 772-999 (165 px, 229 columns > 3 at y 490 spanning 725-999), y 520: 750-955. Acceptance was top >= 1.8 x pot; measured 0.5-0.6 x. |
| 1b | `thin` 1 -> .45 along V | **Done.** Material diff confirmed (M_LampSmoke.uasset 44,125 -> 48,061 bytes). Its product with the sine end fade is what moves the density mid-card: simulated alpha max 0.10 at V .19 (pot top), 0.17 at .3, 0.36 at .5, 0.24 at .7, 0.08 at .9. |
| 1c | Puff texture with gaps, sway .30 | **Done.** Committed `T_ChimneySmoke.png`: alpha max 1.0, mean 0.30, column FWHM 94 px of 256; edge columns zero. The gaps do not read at 14 m because the composite alpha never exceeds 0.36. |
| 1d | `chimney_smoke.opacity` .7 -> .5 | **Done** (`silverbrook-props.json`, MI_SB_ChimneySmoke rebuilt by `dress_all`). |
| 2a | `M_GN_Ground` earth first, cobble on top | **Resolved.** `market.png` y 650-1080, x >= 960: no ground block (60 x 60) with G-(R+B)/2 above 5.5 (acceptance <= 6; the 5.9-7.4 blocks at x 960-1020 and 1740-1800, y 650-770 straddle the two house plinths, not ground). Cobble 2.0, earth 2.9; WP6e band was 18.3-21.5 with cobble 6.4 and earth 12.7. |
| 2b | Earth painted under the cobble, both scripts | **Resolved.** `paint_street_edges.py`: 11,899 vertices, 312 door paths, 312 lot lanes; `main-street.png` every 100 x 100 block in y 620-1080 has G-R <= -14. Trail spot-check accepted on the builder's vertex-colour evidence (1,465 trail vertices with G = 0, B > 0.9; 684 beyond 400 m), not re-run here: the editor was closed. |

## main-street — PASS

Matches the board's TOWN STREETS tile apart from the deferred items: cobble carriageway to both kerbs (every 100 x 100 block in y 620-1080 G-R between -14 and -52), barrels, crate, jug, straw hay stacks with loose stalks, sacks, bench, hay cart, ladder, bracket signs, washing line, five house tones, lamps. No regression against WP6e (mean absolute pixel difference 1.5; the largest 120 x 120 block, 5.9 at x 240-360, y 600-720, is the hay stalks). The WP6e threads above the right roofs (x 807-817, x 1152-1162) are now below measurement (peak R-B excess 2.4 at y 345 in x 740-900; 3 columns at y 330-345 in x 1080-1230, which are the gable edge): chimney smoke at 40-60 m is invisible from the street. Not failed here, since the street tile is judged on the street; fix 1's acceptance re-checks it.

## market — PASS

The yellow-green band is gone: along the former seam (x 960-1440, y 770-1080) the 60 x 60 blocks read 1-5 (cobble 2.0, earth 2.9), and the 2-4x crop shows cobble dissolving into packed earth with a ragged, noise-driven edge, no lawn. Square cobbled, stalls on it, the 10 m earth band in the foreground with the firewood stores on earth, eight stalls, goods, grindstone, barrels. Left house's orange window glow at noon and the smooth far domes are deferred (GFX-02 J5, J1b).

## house-frontage — PASS (no regression)

Townhouse1 package as in WP6e: ashlar plinth, round-headed door, lantern, bracket sign, canted bay with leaded lights, terracotta sill planter across the bay's front face (x 780-1080, y 660-750) with pink umbels and trailing stems, stoop tub with red geraniums, garden fence at the corner. e-to-f difference 2.1 mean; the largest blocks (4-6 at x 0-360, y 720-960) are the street margin outside the kerb, which now reads as packed earth where it was a cobble/earth mix: the designed margin (`STREET_EARTH_EXTRA_M`), not a defect. The cobble paint stretched down the bank (x 0-400, y 690-800) and the flat cyan brook at x 1760-1920, y 715-740 stay deferred.

## foundation-close — PASS (no regression)

Plinth with the rubble course proud, three steps, door jamb, stoop tub, bay apron with gothic panels, sill planter underside, flower tub and ivy at the corner, cobble to the plinth. e-to-f difference 2.5 mean, concentrated in x 1560-1680, y 480-960: the ivy cards (wind), nothing else moved. Geranium discs under 2 m remain a GFX-03 prop-polish note.

## chimney-smoke — FAIL

1. **What's wrong:** The plume is widest and faintest at the pots and dense mid-frame, the inverse of a chimney plume. Measured on `chimney-smoke.png` (stack x 860-940, pots at y 580): 60-90 px above the pots the veil over the field spans x 725-999 at y 490 (229 columns with L excess > 3 over the same-row field, max +21.8) and x 772-999 at y 460; at y 430 the plume FWHM is 146 px and the peak R-B excess 28.7; it then pinches to 49-63 px at y 150-260 (peak excess 11-22) and opens to a faint 124-140 px haze at y 10-60 with a peak excess of only 6.6-7.3 (the sky's own R-B is -12 to -20, so the top is a tint, not smoke). The 3x crop of x 700-1240, y 380-640 shows a soft fog sitting on the field and the roof behind the stack rather than a column leaving the pots. Luminance is right (plume L / sky L = 1.005, 1.000 and 1.023 at rows 30, 80, 260; acceptance within 10 %), the card edges are invisible and the stack top is in frame.
   **Root cause (from the material graph and the committed texture, not assumed):** the opacity is `tex * .5 * thin(V) * sin(pi V) * sides`, and the camera sees V = .19 at the pots and V = .92 at the frame top. `sin(pi V)` is 0.56 at the pots and 0.31 at the top, so with `thin` the composite alpha is <= 0.10 at the pots, 0.36 at V = .5 (y 330) and <= 0.08 at the top: the x1.6 -> x0.7 taper is real (core 91 px at the pot, 207 px at the top in card space) but the fade product hides both ends, and a Gaussian column read against a threshold narrows as it fades, which is why the top measures 124-140 px, not 207. Second, the x1.6 U scale at the pot samples U in [-0.3, 1.3]; the texture's address mode is Wrap (the import sets only compression and sRGB), so the neighbouring tile's column tail (alpha 0.04-0.10) appears as an extra 85 px band at the card edge for V < .35 (simulated span 58-277 of 290 wrapped vs 58-192 clamped at V = .19). With two crossed cards per stack (yaw +35 / +125, both past the ViewFade threshold from this eye) the base reads 1.7-2.1 m wide.
   **Fix type:** CODE. **Instruction:** fix 1.

Resolved here: lit neutral plume within 2.3 % of the sky's luminance, no card border, stack top in frame under the plume, far rows faded. The dark blue-grey wedge in the sky at x 560-640, y 0-50 is a far faceted peak behind the near mountain (also in WP6e), deferred with the mountains.

## Deferred to GFX-02 / GFX-03 (seen, not failed here)

- Flat cobble and lawn tiles without relief; cobble paint stretched down the bank in `house-frontage.png` x 0-400, y 690-800 — GFX-02 J2.
- Lamps and windows lit at noon (`main-street.png` lamps, `market.png` left house x 1270-1330, y 380-500) — GFX-02 J5.
- Far mountains: white faceted shapes and the dark wedge at `chimney-smoke.png` x 560-640, y 0-50; smooth domes in `market.png` — GFX-02 J1b/J2.
- Water surface: flat opaque cyan plane, and at a grazing angle a pale zigzag ribbon with polygonal bank edges (`chimney-smoke.png` x 0-700, y 500-610; `house-frontage.png` x 1760-1920, y 715-740) — GFX-02 "water: reflection, depth tint, bank foam".
- Ashlar tiles repeat, plaster flat, no corner wrap — GFX-03 1-2. `SM_MarketStall` stand-in — GFX-03 gap 7. No decals or per-instance weathering — GFX-03 gaps 5-6. Flat-disc geranium and umbel blooms under 2 m — GFX-03 prop polish.

## Fix list (shared first)

1. **Chimney plume densest at the pots, dispersing upward, no wrapped column at the card edge** — Engineer, CODE. Covers chimney-smoke 1 and restores the 40-60 m threads in main-street.
   a. `GameClient/ArtSource/VFX/StreetLamp/build_materials.py` `build()`: replace the symmetric sine end fade with a foot ramp that only hides the card's bottom edge and a head ramp over the top fifth; `thin` already carries the dispersal. Replace
      ```python
      ends = _node(smoke, u.MaterialExpressionSine, period=2.0); _link(v, ends, '')   # 0 at both ends of the card, 1 mid-way
      ```
      with
      ```python
      foot = _node(smoke, u.MaterialExpressionSaturate); _link(_mul(smoke, v, _constant(smoke, 8.)), foot, '')   # 0 at the card's bottom edge, 1 from V = .125 (75 cm): hides the cut, keeps the pot end dense
      head = _node(smoke, u.MaterialExpressionSaturate); _link(_mul(smoke, _sub(smoke, _constant(smoke, 1.), v), _constant(smoke, 5.)), head, '')   # 1 to V = .8, 0 at the top edge
      ends = _mul(smoke, foot, head)   # WP6f review fix 1a: densest at the pot; `thin` does the dispersal
      ```
      Leave `fade = lerp(1, ends, EdgeFade)` and `edge_fade` 1.0 as they are. Expected composite alpha (texture at 1): 0.45 at the pots (V .19), 0.36 at V .5, 0.28 at V .8, 0 at the top edge.
   b. `_texture()`: clamp U on the plume texture so the taper's out-of-range samples return the zero edge column instead of the next tile. After `tex.set_editor_property('srgb', True)` add
      ```python
      if name == 'T_ChimneySmoke':   # WP6f review fix 1b: the taper samples U past 0..1 at the pot; wrapped, the next tile's column shows at the card edge
          tex.set_editor_property('address_x', u.TextureAddress.TA_CLAMP)
      ```
      (V stays Wrap: the panner needs the periodic rows.) Rerun `build()` in the editor (`apply_dressing.py` already calls it), then `dress_all` so `MI_SB_ChimneySmoke` is re-saved against the rebuilt parent.
   Acceptance (`chimney-smoke.png`, same eye; plume = columns whose 7-row-averaged R-B exceeds the background interpolated linearly between x 820 and x 1070 by 4; over the field at y >= 440 use the L excess > 3 over the same-row field at x 560-700 fitted linearly): at y 520 the plume lies within x 820-1000 and spans <= 140 px; at y 150 it spans >= 1.5 x the y 520 span; peak R-B excess >= 12 at y 400-520 and >= 8 at y 100; no row in y 460-520 has more than 160 columns of field brightening; plume L within 10 % of the sky at rows 30, 80, 260; stack top in frame. `main-street.png`: above the right roofs (x 790-830 or x 1085-1125, y 300-400) at least one row shows >= 15 columns with R-B excess >= 5 over the sky interpolated across the window, i.e. a chimney thread is visible at 40-60 m.

Resubmit with `chimney-smoke` and `main-street` recaptured; `market`, `house-frontage` and `foundation-close` stand unless the dressing is rerun.
