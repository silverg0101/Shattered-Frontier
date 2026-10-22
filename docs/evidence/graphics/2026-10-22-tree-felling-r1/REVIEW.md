# Tree felling r1 — independent review (2026-10-22)

Reviewer: Fable, no part in the build. Concepts: `GameClient/ArtSource/HarvestOak/MightyOak-Felling-v1/Concept/{sequence,stump,log}`. Captures: `Artifacts/HarvestArt/felling-review-01/{level,slope}` (1920x1080) with `client.log` lean/rest lines. Pairs and strips in this folder.

Scope applied: the oak species, bark tone and the bare test slab are not judged. The ground's tiled 8 x 8 grass-patch repeat is noted only because it does not block judging.

Result: **0 PASS / 8 FAIL**.

Measured facts used below (from `client.log` and pixel sampling):

| | level | slope |
|---|---|---|
| Rest lean from vertical | 68.9 deg (21 deg above ground) | 67.2 deg (23 deg above the slope plane, ~8 deg above horizontal downhill) |
| Lean 2 -> 51 deg | 1.47 s (frames 975 -> 36) | similar |
| Lean 51 -> 64 deg | 2.18 s, 345 frames (36 -> 381) | 55.6 -> 64.9 similar creep |
| 78 deg keyframe | never reached (`SkippedLeans=1`) | never reached |
| Body pivot Z at rest vs ground | -11.2 cm | -22.9 cm (-76.5 vs BaseZ -53.6) |
| Notch cutter (DefaultGame.ini) | WoodRadiusCm 14, WoodDepthCm 9 on a ~60 cm trunk; at the 00 camera (2200 cm, FOV 65 = 0.68 px/cm) that is ~19 x 6 px | same |
| Cut-face colour, lit | live cap mean RGB (102,53,21); source texture mean (156,99,47) | concept cap mean (206,167,125) |
| 10-log-close pixels under luminance 30 | 49.9 % | 81.0 % |

---

## level-sequence — FAIL

1. **Rest pose is a lean-to, not a fallen tree.** Concept frame 4: trunk flat on the ground, butt kicked ~1 m off the stump, branches crushed under it. Live `08-rest`: trunk axis 21 deg above ground, tip held ~3 m up by the crown; the trunk is visible rising from (1280,790) to (1100,650) and leaves the frame top-left. The tree stops at 69 deg because the 52 UCX limb hulls (up to 6.3 m high, 2.7 m from the axis) are rigid and prop it; nothing yields. `CVarFellingSettleRadius` is 0, so `YieldCrown` never ran (no `HARVEST_TREE_CROWN_YIELDED` in either log).
   Fix type: CODE. See fix 1.
2. **Fall stalls instead of accelerating into impact.** 2 -> 51 deg in 1.47 s, then 51 -> 64 deg takes 2.18 s and 64 -> 69 a further ~2.6 s of creep before the quiet timer fires. A real fall accelerates through 50-90 deg and ends with one impact; the strip reads as the tree slowly sinking onto its branches. Same root cause as 1.
   Fix type: CODE. See fix 1.
3. **Notch is invisible in `00-notched`.** Concept frame 1 shows a bright wedge ~45 % of the trunk width and ~1/3 of the stump height. Live frame 1 at 1920 px is an untouched trunk; the three 14 x 9 cm chips total ~19 x 6 px at this camera distance. A player cannot tell the tree has been cut.
   Fix type: CODE (cutter size) + LEVEL (review camera). See fixes 3 and 4.
4. **Butt sinks.** Pivot is 11 cm under the ground at rest; in `08-rest` the lower collar at (1200-1300, 740-800) merges into the dirt with no visible end face.
   Fix type: CODE. See fix 2.
5. Minor: the crown's lower foliage is sheared flat by the ground plane along y~900 (x 680-1250). Fix 1 lowers the trunk so this is mostly covered by leaves lying on the ground; not scored separately.

## slope-sequence — FAIL

Same four failures as level: rest at 67 deg from vertical propped on the crown (the trunk never meets the slope; in `08-rest` the trunk is fully hidden in foliage, the butt and stump are not identifiable at all), 44 -> 65 deg creep, notch unreadable in `00-notched`, pivot 23 cm into the slab. Fall direction (downhill, +X, away from the player) is correct and the hinge holds the butt at the stump until 25 deg; those parts are not at fault.
Fix type: CODE. Fixes 1, 2, 3, 4.

## level-stump — FAIL

1. **No torn hinge, no wedge.** Concept: a sloped notch face across half the top, a 15-25 cm tall field of splintered fibres behind it, back cut level. Live `09-stump-close`: a flat octagonal cap at (740-1290, 320-630) with a 2-3 cm step at the back edge. The split surface displacement is `Amplitude = clamp(min(R*0.25, D*0.5), 1, 6)` = 3.5 cm, then scaled down by the 0-10 cm of room left above the cut, so the tear is at most a few cm.
   Fix type: CODE. See fix 5.
2. **Stump is a 100 cm, 8-sided prism.** Visible hard vertical facet edges at x~900 and x~1230; no root flare; the cut sits at the very top of `SM_MightyOak_LowerTrunk` (0-100 cm). Concept stump is ~55 cm tall with a flared, lichened base.
   Fix type: ART (new lower-trunk mesh) + CODE (cut height in the review). See fixes 6 and 3.
3. **Cut-face material is saturated orange.** Lit cap mean (102,53,21) vs concept (206,167,125); the source `T_Oak_Endgrain_BaseColor` is already (156,99,47) and renders darker. Reads as a placeholder orange.
   Fix type: ART. See fix 7.
4. **Shot is in the crown's shadow.** Cap in shadow samples (41,23,12); the whole left half is black. The sun comes from the fall side, so the fallen crown shades the stump. Blocks judging the stump surface detail.
   Fix type: LEVEL. See fix 8.

## slope-stump — FAIL

Same flat orange octagon (750-1280, 300-600), same shadow (background sampled (44,27,13)), plus the stump base is cropped below y=1080 so the downhill side of the base (where a vertical stump on a 15 deg slab could float) cannot be checked. Bottom-left (230-600, 870-1080) shows the collar's end face as a dark maroon (25,14,8) wedge: the notch interior on the butt renders near-black.
Fix type: CODE/LEVEL/ART. Fixes 5, 6, 7, 8.

## level-log — FAIL

1. **No torn butt.** Concept: splintered end face on the log, matching the stump tear, lying flat on the grass 1 m from the stump. Live `10-log-close`: the trunk rises left at ~25 deg, the butt end at (1180-1300, 760-800) is a smooth bevel buried in the ground; no end grain visible, the orange cap is on the stump only.
   Fix type: CODE. Fixes 1, 2, 5.
2. **Foliage renders black.** Leaves at the top (650-1100, 0-280) sample (10,12,5); 49.9 % of the frame is under luminance 30. `M_MightyOak_Foliage` is Two Sided Foliage with no SubsurfaceColor input, so the shadow side is black.
   Fix type: ART. See fix 9.
3. Shadow placement as in level-stump.
   Fix type: LEVEL. Fix 8.

## slope-log — FAIL

Unjudgeable: 81 % of the frame is under luminance 30. The camera (`Look - Side*420 - RestZ*120 + 160 up`) sits inside the fallen crown; black leaf silhouettes (5,6,5) cover the trunk, the stump at (1440-1700, 230-620) is the only readable object and the log butt is hidden. Blocks judging the log entirely.
Fix type: LEVEL + ART. Fixes 8, 9; then re-judge.

## level-frames (motion strip) — FAIL

Direction: correct (away from the mannequin, toward the notch side). Hinge: correct, the butt stays on the stump through 22 deg and releases at 25 deg. Speed: wrong after 50 deg (creep, see table). Ground contact: happens on branch hulls at ~50 deg and never on the trunk. Rest: 69 deg, tip in the air. A believable fall needs the trunk to reach the ground in one continuous motion. Fix 1.

## slope-frames (motion strip) — FAIL

Direction: correct (downhill). Hinge: correct. Speed: same creep from 44 deg. Contact/rest: same, trunk never reaches the slab; the slope makes it worse because the uphill crown hulls touch first and the trunk settles 8 deg above horizontal while the downhill crown is already underground. Fix 1.

---

## Fix list (shared fixes first)

1. **Engineer — CODE — crown yields on first ground contact so the trunk lands.** `GameClient/Source/ShatteredFrontier/Public/Gathering/SFHarvestFallingTree.h`, `USFHarvestTreeFallSettings`: add after `RestQuietSeconds`
   ```cpp
   UPROPERTY(Config, EditAnywhere, Category="Rest", meta=(DisplayName="Crown yield radius", ClampMin="0", ClampMax="400", Units="cm", ToolTip="After the first static-world contact, limb collision further than this from the trunk axis is released so branches crush and the trunk reaches the ground. 0 keeps every hull rigid. Release authors; next world."))
   float CrownYieldRadiusCm=120;
   ```
   and in `Validate()` add `&&In(CrownYieldRadiusCm,0,400)`. `GameClient/Config/DefaultGame.ini`: `CrownYieldRadiusCm=120.0` beside the other `USFHarvestTreeFallSettings` keys.
   `SFHarvestFallingTree.cpp`: delete lines 13-14 (`CVarFellingSupportRadius`, `CVarFellingSettleRadius`), lines 63-64 (`// PROTOTYPE ...` and `YieldCrown(CVarFellingSupportRadius...)`), and lines 210-211 (the `if (!bCrownYielded&&CVarFellingSettleRadius...` block). In `Tick`, directly after the `if (Hinge) { ... return; }` block, insert
   ```cpp
   if (bSawStaticContact&&!bCrownYielded&&Settings->CrownYieldRadiusCm>0)
   { bCrownYielded=true; YieldCrown(Settings->CrownYieldRadiusCm); UE_LOG(LogTemp,Display,TEXT("HARVEST_TREE_CROWN_YIELDED Radius=%.0f"),Settings->CrownYieldRadiusCm); }
   ```
   Acceptance: `HARVEST_FELLING_REST` lean >= 85 deg on level and >= 80 deg (slab-relative) on slope, the 78 deg keyframe captured, `SkippedLeans=0`, and 51 -> 78 deg taking under 0.6 s.

2. **Engineer — CODE — stop the butt sinking.** `SFHarvestFallingTree.cpp`, `Tick`, immediately before `RestPose.Transform=GetActorTransform(); RestPose.bResting=true;`:
   ```cpp
   if (CollarSupport)
   {
       const FVector C=CollarSupport->GetComponentLocation(); const float R=CollarSupport->GetScaledSphereRadius(); FHitResult Hit;
       if (GetWorld()->LineTraceSingleByChannel(Hit,C+FVector(0,0,2*R),C-FVector(0,0,2*R),ECC_WorldStatic)&&Hit.ImpactPoint.Z>C.Z-R)
       { AddActorWorldOffset(FVector(0,0,Hit.ImpactPoint.Z-(C.Z-R)),false,nullptr,ETeleportType::TeleportPhysics); }
   }
   ```
   Also in `InitializeFall` before `Body->SetSimulatePhysics(true);` add `Body->SetUseCCD(true);` so the 1500 kg body does not tunnel the welded sphere on impact. Acceptance: `BodyLocation Z` at rest within +-3 cm of `BaseZ` (level) and of the slab surface under the pivot (slope).

3. **Engineer — CODE — felling notch large enough to read, cut at concept height.** `GameClient/Config/DefaultGame.ini` lines 88-89: `WoodRadiusCm=25.0`, `WoodDepthCm=18.0` (already the declared bounds). `SFHarvestArtReviewScenario.cpp` line ~336: aim the felling interaction at `Wood->GetActorLocation()-FVector(0,0,15)` instead of `Wood->GetActorLocation()` so the final cut lands ~55 cm above the base (the node origin is base+70), giving the stump the concept's proportion and leaving ~45 cm of room for the tear in fix 5. Acceptance: in the new `00-notched` the notch spans >= 40 px wide and >= 20 px deep.

4. **Engineer — LEVEL (review script) — wide camera close enough to read the cut.** `SFHarvestArtReviewScenario.cpp` line 331: replace `View(Target-R.Side*2200-FVector(0,0,150),Target,65);` with `View(R.Base+FVector(0,0,450)-R.Side*1500,R.Base+FVector(0,0,450),65);` (1.0 px/cm; a 10 m tree still fits the 39 deg vertical FOV). Keep this camera for the lean shots.

5. **Engineer — CODE — real tear on stump and butt.** `GameClient/Source/ShatteredFrontier/Private/Gathering/SFHarvestLocalizedGeometry.cpp` line 304: replace `const double Amplitude=FMath::Clamp(FMath::Min(Cut.Radius*0.25,Cut.Depth*0.5),1.0,6.0);` with `const double Amplitude=FMath::Clamp(FMath::Max(Cut.Radius*0.8,Cut.Depth),8.0,30.0);` and raise the fence resolution on line 290 from `Grid=9` to `Grid=17` so the 30 cm spikes are 4-6 cm wide like the concept splinters rather than one broad bump. Keep the `HeightScale` room clamp (fix 3 supplies the room). Re-run `SFHarvestTreeSplitTests` (closed halves must still pass). Acceptance: in `09-stump-close` the tear field is >= 120 px tall at the fix-8 camera and the stump and butt faces are visibly complementary.

6. **Codex — ART — replace `SM_MightyOak_LowerTrunk` (88 tri, 8 sides).** Blender, from `GameClient/ArtSource/HarvestOak/MightyOak-v1/source`: rebuild the 0-100 cm trunk as a 24-segment cylinder, 16 rings, radius matching the UpperStructure base ring at z=100 exactly (same vertex count on the top ring so the seam stays closed), add 4-5 root buttresses flaring to 1.5x radius over the bottom 25 cm, bake the same bark UV layout (slot 0 bark, slot 1 cut wood). Export FBX cm scale, re-import with `tools/Import-HarvestMightyOak.py` (keep `join_cap_material_index: 1`). Target <= 1500 triangles. Acceptance: no visible facet edges at the fix-8 stump camera; the split tests still pass on the new mesh.

7. **Codex — ART — cut-wood texture to concept sapwood.** Regenerate `GameClient/ArtSource/HarvestOak/MightyOak-v1/materials/T_Oak_Endgrain_BaseColor.png` at 1024x1024, sRGB, tileable: prompt "fresh-cut oak end grain, pale cream sapwood, fine concentric growth rings in light tan, no bark, flat even daylight, top-down, seamless tile, photoreal", reference crop `Concept/stump/stump-close-concept.png` (900,300)-(1150,420). Target mean RGB (205,165,120) +-10 (concept cap measures (206,167,125)). Roughness texture mean 0.75. Re-import via `Import-HarvestMightyOak.py`. Acceptance: lit cap in `09-stump-close` samples within dE2000 <= 8 of the concept cap.

8. **Engineer — LEVEL — review lighting and close-up cameras.** (a) `tools/Create-TreeFellingDevMap.py`, after `lighting.apply()`: find the map's `DirectionalLight` and set rotation `pitch=-38, yaw=20` (sun from -X, behind the stump, so the crown's shadow falls away from the stump and the butt; the oak falls toward +X on both maps). (b) `SFHarvestArtReviewScenario.cpp` line 363: `View(R.Base-R.FallDir*260+R.Side*120+FVector(0,0,140),R.Base+FVector(0,0,45),45);` (behind the stump, base in frame). Lines 366-368: `const FVector Butt=Rest.TransformPosition(FVector(0,0,40)); View(Butt-R.FallDir*300+R.Side*380+FVector(0,0,150),Butt+Rest.GetUnitAxis(EAxis::Z)*180,50);` (outside the crown, looking down the log from its butt). Acceptance: < 10 % of each close-up under luminance 30, stump base and butt end face both in frame.

9. **Codex — ART — foliage subsurface so shadowed leaves are not black.** `tools/Import-HarvestMightyOak.py` after line 161 (`connect_output(... MP_OPACITY_MASK)`): add a Constant3Vector `(0.32, 0.46, 0.10)` connected to `MP_SUBSURFACE_COLOR`, and set `foliage.set_editor_property("two_sided", True)` explicitly; re-import. Acceptance: shadow-side leaves in `10-log-close` sample >= (25,35,10).

Fix 1 is the gate: nothing else can be judged as the concept while the tree rests on its crown. Fixes 1-5 and 8 use existing assets and code; 6, 7 and 9 need new art/texture work.
