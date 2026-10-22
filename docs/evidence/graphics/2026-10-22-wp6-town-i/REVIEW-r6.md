# WP6i round-6 review — Silverbrook town, pass 9 r6 (REVIEW-r5.md fix list 1-5)

Reviewer: independent (Fable), did not build the work. Judged per `docs/visual-review.md` against `Silverbrook-Design-Board-v1.webp` and `UI/v5/gameplay-hud.png` (mill plume x 1330-1360, y 100-170: a pale wisp, RGB 144/143/152, lighter than the trees behind it), the r5 fix list and its acceptances, the r1-r5 and WP6h standing acceptances, and the owner's four reports. Commit 6c3ca168. Every capture read at full resolution plus 2-4x crops and pixel measurements (`py -3`, PIL/numpy): plume L/ref by band on `chimney-smoke.png` against `-ref.png`, densest-puff colour, far-thread colour against the sky beside it in `main-street`, `market` and `-approach`, per-frame continuity and mean abs diff over the 14 motion frames, dusk step / bottom-quarter / sky medians, bright-blob search in the three dusk views, and r6 noon captures diffed against the r5 blobs (`af48e04f`) to check the 30-odd resaved `.uasset` files in the commit. README numbers were checked, not trusted; where they differ it is stated.

**Result: 16 PASS / 3 FAIL.** Bookmarks 5 / 0, dusk lamp views 0 / 3, context views 11 / 0.

Fixed this round and holding: **the plume is no longer soot** (core L/ref p5 / p50 / mean 0.915 / 1.037 / 1.024, hill p50 1.032, sky rows p50 0.975; the wisp at the pot is pale, `cs_pot_3x`; every far thread is at 0.90-0.96 of the sky beside it with B > R, where r5 had 0.58-0.66), **the base is thin and grows** (y 550 span 64 px, y 300 span 128 px: 2.0x), **the sill chamber gets no lamp** (`lamp_fx.py` 100: `size[2] < 5`; lights 589 -> 524, `orphans 0`; the rule is a geometry rule, not a per-actor case), **the sky light is movable** (`lighting.py` 114-116, `WP6_NIGHT_ROW ... sky_mobility Movable`), and the r5 passes hold (no townsfolk in any plume frame, claim pads hidden, domes out of the overviews, plinths, frontage rule, no stones on paved ground, no flame at noon, no lamps on the open road). The r6 noon captures differ from r5 by mean |dL| 0.17-0.83 with no change in mean L, so the resaved materials in the commit changed nothing visible.

Not met: (1) **the frontage stoop is still black at dusk** — steps x 100-700, y 880-1080 median 5.4 (>= 6), bottom quarter 5.6 (>= 8), 45 % of the frame under L 8; sky 3.0 -> 6.0 with a movable sky light moved the steps 4.3 -> 5.4, so the night sky the sky light captures is simply dark and the lever is the moon, not the sky light (fix 2). (2) **A flame burns beside the lantern, not in it** — in both lantern dusk views an orange dash (L 233 / 243, 50 / 63 px long, 12 / 29 px tall) sits on the wall 30-40 cm beside and above the lantern glass with no body under it, the glass itself shows no flame or halo, and the wall wash is brightest 120 px to the lantern's left: the lamp FX origin (cards and point light) is off the glass. It was in the r5 captures too (same bbox) and was missed there; it is the `vfx.md` §1 known failure "flames outside the lantern" and the owner's "orange light with no visible source" at small scale (fix 1).

## Owner reports

| Report | Status | Evidence |
|---|---|---|
| Floating houses | Fixed (WP6h), holds | `foundation-close`, `context-house-side-a/b/rear`: plinth under every wall, buried, no gap; `DRESSING_GROUND max_positive_gap_cm 0.01`. |
| Orange light with no visible source | Fixed at the sill (code); **recurs as a flame card off its lantern** | `context-house-side-b-dusk` x 1379-1428, y 212-224 and `context-lamp-wall-dusk` x 936-999, y 525-554: an orange dash on bare ashlar beside the lantern; the lantern pane is a flat yellow slab with no flame inside. |
| Street clutter in mirrored rows on an empty approach road | Fixed (r1), holds | `context-road-approach.png`: groups only in front of houses, no twin across the road, irregular gaps. |
| White street | Holds | Tan cobbles kerb to kerb in every street view; `main-street` unchanged from r5 (mean |d| 0.28). |
| Lamps lit at noon (J5) | Fixed, holds | Every noon lantern dark (`context-lamp-wall` new subject `TheCommons_townhouse_099`: lantern dark, no wash). |

## Round-5 fix items 1-5: met?

| Item | Verdict | Measured (this review) |
|---|---|---|
| 1 Pale plume: tint calibrated, start 0.5 | **Met by eye; three bands open** | Core (y 380-540, x 880-1060) p5 0.915 (>= 0.90), p50 1.037, mean 1.024 (1.00-1.15): met. Hill (y 60-300) p50 1.032 (0.95-1.15): met. Sky rows p50 0.975 (0.90-1.05; 7 px only): met. Roofs (y 300-380) p50 1.152 (>= 1.2): short. Field plume R - B 30.8 (<= 8): not met, and not meetable through semi-transparent smoke over a 160/147/116 field; the meetable version is the densest puffs (top 3 % of L - L_ref in y 560-760, x 880-1020): 146/131/126, **R - B +20** where the concept wisp is -8 (fix 3). Far threads: `main-street` x 1385-1425 p10 0.900 (>= 0.90, B > R: met), x 555-575 no visible thread (`ms_left_4x` is clean sky); `market` x 1635-1665 p10 0.882 (short by 0.02), `-approach` x 530-580 p10 0.926, x 1440-1460 0.952: met. y 550 span 64 (<= 140): met; growth 2.0x at y 300 (the README's 0.11 measures y 150, where the puffs have faded to alpha 0; the metric, not the plume, is wrong). Motion 45-69 px/s up (the 457 px/s is the 0.05 s frame pair): met; consecutive mean abs diff 1.2-1.7 (>= 2 not met: the band was set for a dark plume and scales with contrast; the trio `motion-00/06/13` shows distinct puff shapes). Continuity: 0-3 empty 5-px rows per frame at the roof crossing where the pale plume matches the slate at |dL| < 4; by eye the wisp runs from the pot over the ridge into the sky in every frame. |
| 2 Night row: movable sky light, 3.0 -> 6.0 | **Not met for the frontage** | `SKY_MOBILITY MOVABLE`, `recapture_sky()` in the capture. Steps median 5.4 (>= 6), bottom quarter 5.6 (>= 8), x 0-700 / y 700-1080 18.5 (met); side-b bottom quarter 12.4, lamp-wall 14.6 (met); clip within 1 m 0.0 / 0.0 / 0.16 % (met); panes 238 / 233 / 243; window glass 115-117 (met). The sky light is reaching the scene (+1.1 L for 2x), but the captured night sky is too dark for it to matter. |
| 3 No light at the lamp-wall sill | **Met in code; not verifiable in the capture** | `size[2] < 5` rule with the logged box in the comment; 589 -> 524 lights, `orphans 0`. The seeded `context-lamp-wall` pick changed with the light list (`TheCommons_townhouse_099`, SM_SilverbrookTownhouse4), so no Townhouse1 sill is in any r6 frame and the acceptance region (x 760-1180, y 590-690 of the r5 subject) cannot be measured. Fix 4 pins the subject. |
| 4 Lamp family | Owner decision, open | Unchanged. |
| 5 Resubmit | Met in form; one process miss | All views, ref, motion set, perf pair (Niagara GT 0.08-0.16 ms, workers 0.29-0.37 ms, on - off +0.76 / -0.06 ms: the +0.76 is one off-1 outlier at 4.36 ms, on-2 - off-2 is +0.16), gates listed. The commit also resaved 33 `.uasset` files outside the fix list (slate / plaster / foundation / window / lamp materials, the LUT, backdrop materials, dome and mountain-ring meshes, lamp textures); none changed a pixel (noon diffs above) but "fix only the list" means they do not belong in the change (fix 5). |

## main-street — PASS

Same street as r5 (mean |d| 0.28): cobbles kerb to kerb, five house tones, barrels, crates, hay cart, bench, ladder, signs, lamps and windows dark at noon, weeds at the kerb, nothing floating. The far chimney threads are now pale: x 1385-1425 RGB 173/171/176 on sky 175/179/188 (p10 0.90, p50 0.94, `ms_right_4x`), the left chimney at x 555-575 shows no thread at all, and the thread above the left roofs at x 790-830 is 14 columns at 0.92 of the sky. Seen, not failed: the threads are neutral grey on a blue sky and so read faintly warm (R - B +9 relative to the sky; the concept's wisp is +15 relative to its blue-green trees, so this matches the concept), and the white faceted range (J1b, deferred).

## market — PASS

Stalls with awnings, firewood sheds, ladder, sacks, mixed house tones, cobbles with earth margins, far houses on their plinths (stall stand-in: GFX-03, deferred). The chimney thread at x 1635-1665, y 150-290 is 139/146/158 on sky 134/149/165: p50 1.0, p10 0.88 (0.02 under the band; at 1x a faint grey ribbon, `mk_thread_4x` exaggerates it). Not failed on that margin.

## house-frontage — PASS

Bay window with planter, boot sign, bracket lantern dark at noon, stoop with tub, ivy, plinth; unchanged (mean |d| 0.27). Seen, not failed: the kerb-high cracked-mud bank at x 0-400, y 600-820 (J2, deferred).

## foundation-close — PASS

Rubble plinth under the ashlar on both faces of the corner, flush, buried; tub and planter on the stoop. Unchanged (mean |d| 0.17).

## chimney-smoke — PASS

Measured on `chimney-smoke.png`, `-ref.png` and `motion-00..13.png` (pots x 885-920, y 575-620).

| Acceptance (r5 fix 1) | r6 | Met |
|---|---|---|
| core L/ref p5 >= 0.90, p50 and mean 1.00-1.15 | 0.915 / 1.037 / 1.024 | Yes |
| hill (y 60-300) p50 0.95-1.15 | 1.032 (plume 155/150/154 on 143/150/162) | Yes |
| sky rows (y < 60) p50 0.90-1.05 | 0.975 | Yes |
| roofs (y 300-380) p50 >= 1.2 | 1.152 | Short |
| field plume mean R - B <= 8 | 30.8 (ref field itself 44.6) | Not meetable (see item 1) |
| densest puffs R - B (concept -8) | +20 (146/131/126 at dL 84) | Open (fix 3) |
| y 550 span <= 140, growth >= 2.0 | 64; 2.0 at y 300 (y 150 is past the alpha fade) | Yes |
| continuity, 15 frames | 0-3 empty 5-px rows per frame at the slate crossing (|dL| < 4 where plume L = slate L); continuous by eye | Yes by eye |
| motion >= 20 px/s up, MAD >= 2 | 45-69 px/s; MAD 1.2-1.7 | Motion yes; MAD band obsolete at parity luminance |
| people boxes: no mannequin | none | Yes |

What a player sees (`cs_pot_3x`, `cs_top_3x`, `motion_trio`): a soft pale wisp leaves the pot, widens over the ridge and thins into a translucent veil over the hazy hill, changing shape frame to frame. The one measurable difference from the HUD wisp is hue: the densest puffs are sun-warm (168/151/146 lit colour once the 20 % background is removed; R - B +20) where the concept's are cool (144/143/152, R - B -8). **Fix type:** LEVEL. **Exact instruction:** fix 3 (one tint value; luminance unchanged).

## house-frontage-dusk — FAIL

Pane x 365-415, y 365-425 L 238 (met). Bay panes 117 (met). Clip 0.0 % (met). Open street at the left moonlit (L 25). The house's own shadow side is black: steps x 100-700, y 880-1080 median 5.4, bottom quarter 5.6, planter and bay base under L 8, 45 % of the frame under L 8 (`hf_dusk_steps_2x`, `hf_dusk_boost`: the steps are a black mass at x3).

1. **What's wrong:** the stoop under the house's own lantern and lit bay window reads as a black block from y 880 down; the r5 acceptance (steps >= 6, bottom quarter >= 8) is not met. Sky light 3.0 -> 6.0 (movable, recaptured) moved the steps 4.3 -> 5.4: the sky light works, but the sky it captures at sun -12 deg / moon 2.5 lux is dark, so its full contribution at 6.0 is about 2 L. The r4 probe gives the lever: open ground L 7 / 11 / 15 / 36 at moon 1 / 1.5 / 2 / 5 lux; the steps are lit by Lumen bounce off that ground, which scales with it. **Fix type:** LEVEL + CODE. **Exact instruction:** fix 2.

## context-house-side-b-dusk — FAIL

Pane 233 (met); window glass 115 (met); bottom quarter 12.4 (met); the bench casts a moon shadow; the sign's shadow on the wash is right (the lantern is beside the sign).

1. **What's wrong:** an orange dash, x 1379-1428, y 212-224 (50 x 12 px, L max 233, mean RGB 241/184/80), floats on the ashlar 30 cm left of and 12 cm above the lantern glass (x 1455-1500, y 225-280), with no bracket, glass or flame under it (`sb_streak_ctx`); the lantern pane is a uniform yellow slab with no flame or halo inside it; the wall wash is brightest at x 1355, 120 px (about 40 cm) left of the lantern. The flame / halo cards and the point light of this lantern are placed beside the lantern, not at its glass. Present in the r5 capture at the same bbox (missed then). **Fix type:** CODE. **Exact instruction:** fix 1.

## context-lamp-wall-dusk — FAIL

New subject (`TheCommons_townhouse_099`, SM_SilverbrookTownhouse4, gable with sign, door lantern, lit upper windows). Lantern x 1130-1190, y 560-660 L 243; within 1 m 0.16 % > 235 (met); sky 19 (met); bottom quarter 14.6 (met); window glass 115 (met). The night chimney column at x 1690-1720 is right at night.

1. **What's wrong:** the same defect as side-b: an orange dash x 936-999, y 525-554 (63 x 29 px, L max 243, RGB 243/177/65) on the wall at the sign's right edge, 1.5 m left of and 10 cm above the door lantern (`lw_dusk_streak_3x`); the lantern glass shows no flame or halo; the bracket arm from wall to lantern is dark while the dash is the brightest thing on the wall. **Fix type:** CODE. **Exact instruction:** fix 1.
2. The r5 fix-3 acceptance (no orange panel under a Townhouse1 first-floor sill) cannot be measured here: no Townhouse1 is in frame. **Fix type:** CODE (capture). **Exact instruction:** fix 4.

## Context views

### context-overview-se — PASS

Eye 230 m up, 848 m out: the whole town, forest east and south-east, roads out of each gate, wall extremes inside the frame; no claim-pad slabs (mean |d| from r5 0.56). Seen, not failed (deferred, unchanged): the lake causeway, the straight canal and its cut banks, flat cyan water, the white faceted mountain ring (J1b, GFX-02).

### context-overview-nw — PASS

The r5 frame: the Westfields meadow, roads, river and bridges fill the bottom; the near dome is gone; the smooth white dome at x 0-260, y 0-210 is at its 1.2 km ring (J1b).

### context-road-silvermere — PASS

The V cutting, pines on both crests, shrubs and tufts down both banks, cobbles only on the carriageway, dark earth verges. Unchanged.

### context-road-everwood — PASS

Wide cobbled road into the pines, irregular scrub, dark rocks on the left margin, tufts; no stones on the carriageway (mean |d| 0.83, mean L equal). Lollipop broadleaf and the white range: GFX-03 / J1b.

### context-road-valemere — PASS

Trees, shrubs, the boulder on the verge, striped fields at the left; no stone sets on the road. Unchanged.

### context-town-edge — PASS

Wall segments and towers stepping with the ground, the far wall and the road out; nothing floats, no lamps on the open road. Unchanged.

### context-house-side-a — PASS

Side wall on its plinth, fence, garden bed, iron lamp dark at noon; sky and a distant range top right (mean |d| 0.47). Unchanged.

### context-house-side-b — PASS

Plinth stepping to the stoop base; bracket lantern and iron lamp dark; forecourt and street clear of stones. Unchanged.

### context-house-side-rear — PASS

Rear plinth following the slope, firewood shelter, fence, lamps dark. Unchanged.

### context-lamp-wall — PASS

New subject: gable facade in sun, stag sign, bracket lantern dark at noon, no wash, windows dark, haystack and tub at the door; no light with no source.

### context-road-approach — PASS

The owner's spot holds: both sides fronted by houses, groups irregular, no twin across the road, timber-post lamp dark, no stones on the carriageway. The chimney threads are now pale: x 530-580, y 50-250 RGB 140/148/161 on sky 140/157/174 (p10 0.926, p50 0.961), x 1440-1460 p10 0.952 (`appr_4x`). The r5 colour fail is cleared.

## Deferred (seen, not failed; GFX-02 / GFX-03 / owner, unchanged)

Bank texture streaks and bedrock faces on kerb-high banks (J2); the white faceted mountain ring and the far domes (J1b), 12 m cobble roads outside the wall (J1b); flat cyan water, lake causeway, canal and its cut banks, the zigzag fence lines on the Westfields field (GFX-02 world); ashlar repeats, plaster, stall stand-in, low-poly trees and shrubs, the same shrub mesh repeated (GFX-03); townsfolk mannequins (off for the captures); street-lamp family (r1 fix 9, owner); the claim pad's final form (DEC-03 levelling).

## Fix list (shared first)

1. **Lantern FX at the glass, not beside it** — Engineer, CODE, `GameClient/ArtSource/Greenlands/lamp_fx.py` `glass_sockets()`. The bracket-lantern cards and light sit 30-40 cm beside the glass in both dusk views. (a) Diagnose from the existing `LAMP_SOCKET` log for the two dusk subjects (`context-lamp-wall-dusk`: SM_SilverbrookTownhouse4; `context-house-side-b-dusk`: log its mesh the same way, `WP6_SIDE_B_SUBJECT`, in `capture_game.py` where the view is added): the lantern chamber's box and the final `c` that `out.append(c)` receives. (b) The thin-panel rule is the only code that moves a light off its chamber, and it moves it along the pane's thin axis "away from the mesh origin", which for a lantern pane on a corner or gable wall is along the wall. Replace lines 104-107
   ```python
            thin = min(range(2), key=lambda i: size[i])
            if size[thin] < 5:   # wall-mounted glow panel: light in front of it
                c[thin] += 30 if c[thin] >= 0 else -30
            out.append(c)
   ```
   with
   ```python
            thin = min(range(2), key=lambda i: size[i])
            if size[thin] < 5 and max(size) >= 60:   # a flush glow panel (window-sized): light 30 cm in front of it, outward from the mesh bounds centre
                c[thin] += 30 if c[thin] >= centre[thin] else -30
            out.append(c)   # a lantern pane (every axis under 60 cm) keeps its cards and light at the glass centre (WP6i r6 fix 1)
   ```
   with `box = mesh.get_bounding_box(); centre = [(box.min.x + box.max.x) / 2, (box.min.y + box.max.y) / 2]` computed once after `name = mesh.get_name()`. If the log shows the lantern chamber is not thin but wide (two pieces merged across the bracket), lower the merge gap on line 90 from `< 60` to `< 20` instead; in either case the rule stays a geometry rule. (c) Extend the `vfx.md` §1 step-5 offsets gate to building lanterns: `five-lamp-offsets.json` gains five townhouse lanterns (one per `SM_SilverbrookTownhouse1..4` plus the side-b subject), each flame / halo origin within 3 cm of its glass piece's centre, and `check_lights.py` fails on any offset over 3 cm for a chamber under 60 cm. (d) Rerun `lamp_fx.apply()`, `check_lights.py` (`orphans 0`, `in_wall 0`), recapture the three dusk views. **Acceptance:** in `context-house-side-b-dusk` and `context-lamp-wall-dusk` no connected blob of >= 20 px with L > 200 outside the lantern pane and window panes (the flame and halo count as inside when their bbox lies within the pane's bbox grown by 10 px); the flame card is visible inside the glass (a column of >= 8 px with L > 245 inside the pane bbox); the wall wash's brightest column within 40 px of the lantern's centre column; within 1 m of the lantern <= 1 % > 235; pane >= 200; the offsets gate file and `LAMP_SOCKET` lines for both subjects in the README.
2. **Night row: moon 5 lux** — Engineer, LEVEL + CODE. `capture_game.py` line 66: `NIGHT = {'sun_pitch': 12., 'sky': 6.0, 'moon_lux': 2.5}` -> **`'moon_lux': 5.0`** (sky stays 6.0, movable). `docs/blueprints/lighting-and-atmosphere.md` line 40: "moon 5 lux at pitch -35 (2.5 lux left the frontage steps at L 5; the sky light at 6.0 adds about 2 L)". Recapture the three dusk views. If `house-frontage-dusk` steps x 100-700, y 880-1080 median is still < 6, set `'sky': 8.0` (the `sun.sky_light_intensity` bound) and recapture once; log both step medians in the README. **Acceptance**, all three dusk views: sky top 60 rows (sky pixels, x 0-330 for the frontage) L 10-60; bottom-quarter median >= 8; `house-frontage-dusk` x 0-700, y 700-1080 median >= 8 and steps x 100-700, y 880-1080 median >= 6; within 1 m of any lantern, outside the pane, <= 1 % > 235; panes >= 200; window glass 100-220.
3. **Cool the plume's own colour** — Codex, LEVEL. `GameClient/ArtSource/Silverbrook/silverbrook-props.json` line 383 `chimney_smoke.tint.value` `[0.31, 0.31, 0.36]` -> **`[0.28, 0.31, 0.40]`** (same luminance 0.31; the lit colour measured 168/151/146 at tint 0.31/0.31/0.36, so these ratios land it on the concept's 144/143/152). Push with `apply_dressing.py`; nothing else changes. `docs/blueprints/vfx.md` line 27: the tint; line 31: replace "field plume R - B <= 8" with "densest puffs (top 3 % of L - L_ref in y 560-760, x 880-1020) R - B <= 5", "growth: span at y 300 >= 2.0 x the y 550 span and <= 450" (not y 150), "consecutive mean abs diff >= 1.0 in x 760-1200, y 100-760", and "continuity counted with |RGB - RGB_ref| sum >= 8 (not |dL| >= 4), no 5-px row with < 5 plume columns in y 150-560, x 880-1080". `measure_plume.py`: the same three metric changes (growth row, dense-puff colour, RGB continuity). Recapture `chimney-smoke`, `-ref`, the motion set. **Acceptance:** the r5 core / hill / sky bands as now, roofs p50 >= 1.1, densest puffs R - B <= 5, far threads p10 >= 0.88 with B >= R, the growth / MAD / continuity bands above, motion >= 20 px/s.
4. **Pin the lamp-wall subject** — Engineer, CODE, `capture_game.py` lines 141-147. The seeded pick iterates the light list, so any lamp change moves the subject and the r5 fix-3 acceptance has no frame. Sample instead over `sorted(h for h in houses if h.static_mesh_component.static_mesh.get_name() == 'SM_SilverbrookTownhouse1', key=lambda a: a.get_actor_label())` with the same `rng`, take its first bracket-lantern light (owner match) and keep the `clear()` test; log `WP6_LAMP_WALL_SUBJECT`. **Acceptance:** `context-lamp-wall-dusk` shows a Townhouse1 wall; the plaster panel between the first-floor sill and the jetty beam mean L <= 25 and no pixel > 150 outside the lantern and window panes; the sill has no orange streak (mean R - B <= 20); lantern pane and pool as fix 1.
5. **Commit only the list** — Engineer. Revert the 33 resaved `.uasset` files that no fix touched (the five `MI_SB_Slate_HouseTone*`, five `MI_SB_LimePlaster_HouseTone*`, `M_SB_FoundationStone`, `M_SB_WindowGlow`, `MI_SB_LitGlass`, `MI_SB_GildedTankard_*`, `MI_SB_ArmorVendor_Glow`, `MI_SB_WeaponsVendor_Glow`, `MI_SB_Blacksmith_ForgeFire`, `MI_SB_StreetLamp*_Glow`, `MI_SB_TempleOfTheDawn_BlueGlass`, `MI_SB_Watchtower_Flame`, `T_Grade_Silverbrook_LUT`, `M_Backdrop_*`, `SM_GraniteDomeFar_00/01`, `SM_MountainRing`, `MI_LampIron`, `M_Lamp*`, `T_Lamp*`, `T_ChimneyPuffs`) to their `af48e04f` blobs unless a fix above rebuilds them (`M_ChimneySmokeSprite` is rebuilt by nothing here; `MI_SB_LanternGlow` by `lamp_fx.apply()`), and list in the README which assets each fix rebuilt. The map package stays under 100 MB (98.3 MB now).
6. **Lamp family** — owner decision (r1 fix 9), unchanged. Listed, not scored.
7. **Resubmit** — Engineer. `lamp_fx.apply()` + `check_lights.py` + offsets gate (fix 1), `apply_dressing.py` (fix 3), all gates, then the capture run with the r4 fix-2 command line (townsfolk off): the five views, `chimney-smoke-ref`, the 14 motion frames, the eleven context views, the three dusk views (fixes 2 and 4), `measure_plume.py` with the fix-3 metrics, the 1440p perf pair. Report the README numbers against the bands; a value outside a band is a known open item, not a pass.
