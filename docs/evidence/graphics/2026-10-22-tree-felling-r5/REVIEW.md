# Tree felling r5 — independent review (2026-10-22)

Reviewer: Fable, no part in the build. Concepts: `GameClient/ArtSource/HarvestOak/MightyOak-Felling-v1/Concept/{sequence,stump,log}`. Captures: `Artifacts/HarvestArt/felling-review-05/{level,slope}` (1920x1080) with `client.log` / `server.log`. Every pair, strip and raw frame was read; crops (x2-x4) and pixel counts were taken from the raw frames with a script in the reviewer's scratchpad (not committed). Cut-wood colour below means R>190, G>140, B<150, R-B>70 (sunlit cut wood in `09` is ~(240,200,120)).

Scope as briefed: oak species, bark tone, bare slab and ground tiling are not judged; fall direction is the owner's call; physics varies per run. The bar is the r4 bar. r4 fixes are checked for their intent; passing views are re-checked for regression.

Result: **4 PASS / 4 FAIL**. The motion strips hold and both log views now pass (no cut-wood rim and no pattern break at the 1 m join). The sequence views still fail on the same item as r4: the notch is invisible in `00-notched`, and the cause is in the log: the review sun never moved. The stump views regress: the chopped third and the 20 cm shards of r4 are gone, the top is a level disc with a square-walled socket, and cut-wood triangles show on the log's bark beside it.

Measured facts:

| | level | slope |
|---|---|---|
| FALL_STARTED -> CROWN_YIELDED -> FALL_RESTING (server) | 2.63 s, then 2.78 s (quiet test, < 4.0) | 2.66 s, then 2.55 s |
| Sun rotation (`client.log` 1842) | `PIE: Warning: Mobility of ... DirectionalLight_0 : LightComponent0 has to be 'Movable' if you'd like to move.` — the `SetActorRotation` at `SFHarvestArtReviewScenario.cpp:330` did nothing | same line in both runs |
| `00-notched`, trunk base (x 820-1120, y 760-1030, x4 crop) | trunk in the crown's shadow, mean (53,43,35) left / (76,61,47) right; ground shadow falls toward the camera; **0 cut-wood px**, no silhouette bite at rows 900-980 | same, **0 px**, no bite |
| `01`, `02` cut-wood px at the base | 0 / 0 | 0 / 0 |
| `03-lean-22` cut-wood px at the base | 335 (the opened cut at the right silhouette, x 955-985, y 925-955) | 153 |
| Join on the log, `10-log-close` (band 50 px wide along the trunk axis) | no column at the join (x ~650-700) in the top five gradient columns; strongest columns are shadow edges at x 830/834, ratio 1.62-1.94 to their neighbours | same; strongest at x 821/829 (leaf shadow), 1.54-1.82 |
| Cut-wood px within 30 px of the join | 0 | 0 |
| Torn butt face, `10` | 175 px wide (x 415-590) | 180 px (x 410-590) |
| Stump in `10` | top of the stump in the bottom-left corner (x 0-270, y 940-1080) | **not in frame** |
| Stump top, `09-stump-close` | near rim and far rim lie on one ellipse (no crease, no sloped third); a square-walled socket at x 810-960, y 420-520 with vertical faces ~80 px tall and a flat floor; one shard ~100 px (~12 cm) at x 980-1050. r4: sloped plane with a crease, far third level, tallest shard 155 px (~20 cm) | level disc; a rectangular trough across the middle x 700-1080, y 410-540; one shard ~40 px |
| Cut-wood on the log's bark beside the stump, `09` | **1995 px**: three triangles at (445-460, 125-170), (480-520, 240-290) and a sliver (400-520, 290-320), all within ~25 cm of the butt on the notch side | **475 px** at (370-430, 320-360) and (405-435, 410-440); 427 px under the butt (100-250, 470-620) |
| Bark on the stump body, `09` (level box x 750-1100, y 650-900) | mean (240,222,196), luminance std **12.5** (r4: (234,213,184), std 20.3) | std 20.5 (r4 box included bark edges, not comparable) |
| Butt at rest, `08-rest` | torn end ~1 m left of the stump, ground between (log right edge x ~820, stump left edge x ~925) | butt > 1.2 m downhill, out of the base crop |

---

## level-sequence — FAIL

Frames 2-4 read as the concept and as r4: hinge holds through 22 deg with the butt on the stump; at 50 deg the butt is on the ground beside the stump with the crown touching; at rest the trunk lies flat, the butt a stride past the stump, the crown crushed. The camera swing is in (the fall now crosses the frame toward the far side, like the concept). Frame 1 is unchanged from r4:

1. **No notch in `00-notched`: 0 cut-wood px, no silhouette bite, trunk base at (53,43,35) in the crown's shadow.** The r4 fix moved the camera but the sun stayed where it was: `client.log` line 1842 says the directional light is not Movable, so `SetActorRotation` at line 330 was refused and the shadow still falls toward the camera. The notch side is unlit, so a 50 x 30 cm cavity in shadow is indistinguishable from bark in shadow. The concept's frame 1 is a sunlit wedge taking a third of the trunk's width.
   Fix type: LEVEL (review light). Fix 1.

## slope-sequence — FAIL

Frames 2-4 as level (butt on the slab beside the stump at 50 deg; at rest the trunk lies downhill with the crown crushed). Frame 1: 0 cut-wood px, no bite, same cause. Fix 1.

## level-stump — FAIL (regression from r4 PASS)

The butt is on the ground, the stump is seated, cut-wood and bark are the shipped materials, and the chopped face's far third is level. What a player now sees on the top is a sawn-flat disc with a square hole:

1. **The chopped third is gone.** In r4 the near third was one sloped plane with a crease against the tear band (a 7-8 cm drop at the near rim). In r5 the near rim and far rim lie on one ellipse: no crease, no drop. `SFHarvestLocalizedGeometry.cpp:329` gives the chopped side `-0.45*Chop` (up to ~9 cm at W = R), scaled by `HeightScale` (line 332), so either the clamp fired or the chopped side is not where the `09` camera looks.
   Fix type: ENGINEER (split surface). Fix 2.
2. **The tear band reads as a mortise.** A socket at x 810-960, y 420-520 with two vertical cut-wood walls ~80 px tall and a flat floor, and one ~12 cm shard. r4 had three ~20 cm shards and a 50 x 40 px hole. Cause: `Noise[ID]` is independent per cell on a 17-grid (~4 cm cells), so adjacent cells differ by up to 2 x Amplitude and the lofted faces between them are vertical walls.
   Fix type: ENGINEER (split surface). Fix 2.
3. **Cut-wood triangles on the log's bark, 10-25 cm from the butt, notch side: 1995 px in this shot.** Three sunlit wood triangles on the bark of the fallen log (top-left of `09`). They are the upper face of the notch prism (`SFHarvestLocalizedGeometry.cpp:158-161`: the cutter is +-0.6 R tall about the contact) left on the log because the split surface passes through the contact, the middle of the notch, so the top half of the wedge is removed from the log's collar. Real felling, and the concept log image, leave the whole notch on the stump and a torn, round butt on the log.
   Fix type: ENGINEER. Fix 3.

Secondary (not a gate, bark tone is out of scope): the triplanar bark lost its fissure contrast, luminance std 12.5 vs 20.3 in r4 at the same camera; the stump body reads as plaster. Fix 5.

## slope-stump — FAIL (regression from r4 PASS)

Same read: level disc, no crease, a rectangular trough across the middle (x 700-1080, y 410-540), one ~40 px shard; 475 px of cut-wood triangles on the log's bark beside it and 427 px under the butt. Fixes 2, 3.

## level-log — PASS

r4 items resolved: 0 cut-wood px within 30 px of the join and no yellow fringe on the lower trunk's top silhouette (fix 2 intent met: the top cap is bark and the rim no longer stands proud); no vertical edge at the join, the bark pattern is continuous from the butt into the upper structure (fix 3 intent met by the documented triplanar fallback); the stump top is in frame (bottom-left corner, cropped by the frame edge, fix 4 acceptance met on this map). The butt is on the ground, the torn face 175 px wide, the trunk flat with its contact shadow, the crown crushed. The flat faceted butt face is the r4-accepted state (fibres were optional).

## slope-log — PASS

Join: 0 cut-wood px, no edge, continuous bark. Torn face 180 px. Trunk flat downhill with contact shadow. Remaining difference, not a player-visible defect: the stump is not in frame on this map (r4 fix 4 asked for both maps; the camera offset `-R.FallDir*150+R.Side*520` puts the stump, 1.2 m downhill of the butt here, outside the 55 deg field). A 35 x 30 px cut-wood wedge shows under the butt at (410-445, 830-860): the torn face's lower lobe wrapping under the bark. Fix 4 (framing only).

## level-frames (motion strip) — PASS

No regression from r4. Hinge holds through 22 deg with the butt on the stump; breaks by 36 deg with the butt kicked forward; by 50 deg the butt is on the ground beside the stump and stays; crown contact from ~50 deg; trunk flat by rest. Crown yield 2.63 s after FALL_STARTED, rest 2.78 s after the yield by the quiet test. Reads as a weighted fall. The notch is invisible in 00-02 (sequence item), visible from 03.

## slope-frames (motion strip) — PASS

As level; the butt lands on the slab beside the stump at 50 deg and the trunk slides downhill while rotating onto the slab; rest by the quiet test (2.55 s after the yield) with the crown crushed downhill. No roll, no skate, nothing floats.

---

## Fix list

1. **Engineer — LEVEL (review light) — make the review sun movable before rotating it.** `GameClient/Source/ShatteredFrontier/Private/Tests/SFHarvestArtReviewScenario.cpp` line 330: before `It->SetActorRotation(...)`, call `It->GetLightComponent()->SetMobility(EComponentMobility::Movable);` (do the same in the intensity block at line 171 so one place owns the light's mobility), and log the resulting rotation (`UE_LOG(... "HARVEST_REVIEW_SUN Rot=%s"`) so the next review can verify it from `client.log`. No other change to the camera: the swing is right (the cut shows at the right silhouette as soon as the tree leans in `03`).
   Acceptance (unchanged from r4): in `00-notched` on both maps the notch shows >= 1200 px of cut-wood colour with mean R >= 150 and a V >= 10 px deep in the near silhouette at the notch rows; `client.log` has no "has to be 'Movable'" warning; keyframes 03/05/08 unchanged in content.

2. **Engineer — split surface: restore the chopped third and turn the socket back into shards.** `GameClient/Source/ShatteredFrontier/Private/Gathering/SFHarvestLocalizedGeometry.cpp` lines 318-332:
   - Log `Highest`, `Lowest`, `HeightScale`, `UpperRoom`, `LowerRoom` once per split (Display) and read them from the r5 `server.log`/`client.log` of the next run; if `HeightScale < 1`, apply it to the tear band term only (`Amplitude*Band*(Noise-ContactNoise)`) and leave the `-0.45*Chop` plane and `-0.04*W` tilt unscaled (clamp the plane to `LowerRoom` separately).
   - Replace the independent per-cell `Noise[ID]` with one 3x3 box-smoothed pass over the grid (compute `Noise`, then `Smooth[ID] = mean of the up-to-9 neighbours`, use `Smooth` for both `Delta` and `ContactNoise`). Adjacent cells then differ by at most ~2/3 Amplitude, so the lofted faces lean instead of standing vertical, and the band reads as shards, not a mortise.
   Acceptance at the `09-stump-close` camera on both maps: a visible crease between the chopped plane and the tear band, the near rim >= 50 px below the far-rim ellipse; tallest shard >= 110 px (~15 cm); no cut-wood face in the band whose silhouette is a straight vertical edge >= 60 px. `ShatteredFrontier.Gathering.{TreeSplit,LocalizedGeometry,LocalizedImportedContact}` must pass.

3. **Engineer — put the whole notch on the stump.** Same file: the split surface is pinned to the notch contact (line ~318, `ContactNoise`) at `Cut.Center.Z`, the middle of a cutter that is +-0.6 R tall (line 158-161), so the top half of the wedge is cut from the log's collar and shows as sunlit cut-wood triangles on the fallen log's bark. Raise the split's reference height to the top of the notch mouth on the chopped side: set the split `Cut.Center.Z` (or add `+0.6*Cut.Radius` to `Z` for cells with `Chop > 0`, blended over the band) so the log's butt is a round torn end with no notch face, and the stump keeps the full wedge (its lower face is the chopped plane the concept shows). Check `UpperRoom` still clears `Amplitude` (cut at 55 cm + 15 cm leaves 29 cm in the 1 m lower trunk).
   Acceptance: 0 cut-wood px on the log's bark within 40 cm of the butt in `09-stump-close` and `10-log-close` on both maps (the torn face itself excluded); the stump's chopped plane still present per fix 2.

4. **Engineer — LEVEL (review script), framing only, not a gate — stump in the slope log shot.** `SFHarvestArtReviewScenario.cpp` line 370: widen the field or move the eye back along `Side` so the stump enters the frame on the slope map too, e.g. `View(Butt-R.FallDir*120+R.Side*620+FVector(0,0,170),Butt+R.FallDir*200+FVector(0,0,40),60);`. Keep the torn face >= 120 px wide.
   Acceptance: stump top visible in `10-log-close` on both maps.

5. **Codex — ART, secondary, not a gate — bring back the bark's fissure contrast in the triplanar.** `tools/Update-HarvestOakFellingArt.py` `triplanar()`: the two lerps use raw `|n.x|` and `|n.z|` as alphas, so faces at ~45 deg get a half-and-half blend of two projections and the fissures wash out. Use normalised weights: `w = pow(abs(n), 4); w /= (w.x+w.y+w.z)`, `result = xy*w.z + yz*w.x + xz*w.y`. Keep the 136 cm tile and the UV normal map. Re-run `tools/Update-HarvestOakFellingArt.py`; `ShatteredFrontier.Gathering.TreeSplit` must pass.
   Acceptance: luminance std of the stump body box (x 750-1100, y 650-900 in level `09-stump-close`) >= 18 (r4: 20.3), with no visible seam at the 1 m join (fix-3 r4 acceptance still met).

Fixes 1 and 4 change the review script only; 2 and 3 change the split surface (shared by stump and log, so both stump views and the log butt change together); 5 is material-only. Fix 1 is the gate for the sequence views, fixes 2-3 for the stump views.

Owner flag carried: the tree still falls away from the notch face (`SFHarvestPresentation.cpp` `TreeSplitFor`), unlike real felling; not a review item.
