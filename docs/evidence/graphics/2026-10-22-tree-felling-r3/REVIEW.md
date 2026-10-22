# Tree felling r3 — independent review (2026-10-22)

Reviewer: Fable, no part in the build. Concepts: `GameClient/ArtSource/HarvestOak/MightyOak-Felling-v1/Concept/{sequence,stump,log}`. Captures: `Artifacts/HarvestArt/felling-review-03/{level,slope}` (1920x1080) with `client.log` / `server.log`. Pairs and strips in this folder. Every image was read at full resolution; crops and pixel measurements were taken from the raw frames.

Scope applied as briefed: oak species, bark tone and the bare slab are not judged. Repeating ground patches are ignored. Physics varies per run; what is captured is judged.

Result: **0 PASS / 8 FAIL**. Resolved since r2 and not re-raised: the slope slide (body origin moves 24 cm between the 78 deg shot and rest, was 338 cm), the root flare (no creases on either stump), lit close-ups (8.4 % / 13.5 % of `09` under luminance 30), cut-wood colour (level lit mean (192,157,114)), grain streaks running along the tear faces instead of across them, the join-ring silhouette step (top edge continuous within 2 px at the `10` camera), and the butt now on the ground beside the stump on slope. What still breaks the design, in order of visibility: the trunk still never lands (rest 10.8 deg nose-up on level, 9.8 deg above the slab on slope, same as r2), on level the butt rests on top of the stump so the log is a ramp from stump height, the notch bite is unchanged at 21 x 15 px, the tear is a low-poly crystal with no chopped face or level break, and a vertical bark-texture seam cuts the log 45 cm from the butt.

Measured facts (from `client.log`, `server.log` and pixel sampling of the raw frames):

| | level | slope |
|---|---|---|
| Rest lean from vertical (`HARVEST_FELLING_REST` / `08-rest`) | 79.2 deg = trunk axis **10.8 deg nose-up** (r2: 80.6) | 95.2 deg = **9.8 deg above the 15 deg slab** (r2: 94.3) |
| Lean 2 -> 50 deg | 1.78 s (47.947 -> 49.725) | 1.66 s |
| Lean 50 -> 66 deg | 0.32 s | 0.26 s |
| Lean 66 -> 78 deg | **0.79 s** (50 -> 78 = 1.10 s vs the 0.6 s acceptance) | 0.27 s (50 -> 78 = 0.53 s) |
| Crown yield (server) | 2.42 s after FALL_STARTED, at ~48 deg lean | 2.67 s after, at ~50 deg |
| `FALL_RESTING` after yield | 2.52 s = `MaxSettleSeconds`; the quiet test never passed | 2.50 s = settle cap |
| 78 deg shot -> `HARVEST_FELLING_REST` (client) | 2.8 s | 3.7 s |
| Body origin travel 78 deg -> rest | 8 cm | 24 cm |
| Butt at rest | torn end sits on the stump's fall-side top corner, bottom of the collar at cut height (~55 cm); zero ground under it | torn end on the slab, touching the stump's downhill face |
| Trunk underside above ground at 3 m from the stump (`08-rest`, 1 px/cm) | ~100-140 px of open ground and shadow between trunk and ground at x~1300 | trunk leaves the slab within ~1.5 m of the butt; ~100 px of air at x~1300 |
| Notch bite, `00-notched` left silhouette (rows 900-962, 2 px steps) | trunk edge x 917-922 above and 926-927 below; **x 942-944 on rows 928-942 = 21 px deep x 15 px tall**; orange patch 22 x 37 px (x 925-947, y 908-945), mean (123,92,57) in the crown's shadow | edge 918-920 above, 924-926 below: **~6 px** indent only; patch 22 x 37 px, mean (121,93,62) |
| Tear field height, `09-stump-close` | mound y ~290-600 = 310 px (~40 cm) of 10-25 cm flat facets | three 25-30 cm triangular spikes on the right, faceted ±10 cm elsewhere |
| Frame under luminance 30: 09 / 10 | 8.4 % / 22.9 % | 13.5 % / 20.9 % |
| Join on the log, `10-log-close` | bark texture break at x~1442 (full log height), top silhouette continuous | bark texture break at x~1442, silhouette continuous |

---

## level-sequence — FAIL

1. **Trunk does not land; the butt sits on the stump and the trunk is a ramp into an intact crown.** Concept frame 4: trunk flat on the grass, butt ~1 m past the stump, crown crushed. Live `08-rest`: rest lean 79.2 deg (10.8 deg nose-up; r2 acceptance >= 87). The `L08` butt crop shows the collar's torn end resting on the stump's fall-side top corner at cut height, the trunk rising from there; at x~1300 there are ~100-140 px of lit ground and cast shadow under the trunk. Highest leaf y~190 (~7.5 m above ground). Causes, from the logs: the crown yielded at 90 cm (builder's deviation: 60 rolled the body on its back because nothing stops roll about the trunk axis), so primary-limb hulls inside 90 cm still prop the tree; `FALL_RESTING` fired exactly `MaxSettleSeconds` (2.52 s) after the yield, i.e. the pose was frozen while still moving; the collar sphere's centre is at the stump box's fall-side edge and default friction holds it there.
   Fix type: CODE. See fixes 1 and 2.
2. **The fall brakes mid-air after the crown touches.** 50 -> 66 deg takes 0.32 s, 66 -> 78 deg takes 0.79 s (15 deg/s): the angular damping of 3 is applied the moment the first leaf hull touches (~48 deg) and slows the trunk through its last 30 deg before it has reached anything. The strip reads as the tree sinking onto its crown, then freezing.
   Fix type: CODE. See fix 1.
3. **Notch unreadable, unchanged from r2.** Bite 21 x 15 px on a 60 px trunk (r2: 22 x 17); patch 22 x 37 px, mean (123,92,57). The r3 prism is 0.6 R tall each way, but its wide face is placed `Cut.Depth` (18 cm) *outside* the bark (`SFHarvestLocalizedGeometry.cpp` line 159-161, local Z = `Cut.Depth`), so by the time the taper reaches the bark the opening is half height (15 cm) and the apex is 18 cm in: exactly the 15 px bite measured. Half the enlargement is cut out of air.
   Fix type: CODE. See fix 3.

## slope-sequence — FAIL

Fall direction, hinge, and acceleration (50 -> 78 in 0.53 s) are right; the slide is gone (24 cm of travel after the 78 deg shot). Then:

1. **Trunk 9.8 deg above the slab, propped on the inner crown** (rest 95.2 deg; acceptance >= 100 slab-relative = 85 + 15). In `08-rest` the lower trunk leaves the slab within 1.5 m of the butt; the crown stands 4-5 m tall on the downhill side. Pose frozen by the 2.5 s settle cap, not by the quiet test.
   Fix type: CODE. Fix 1.
2. **Notch: ~6 px indent, no readable bite** (rows 930-962 edge 924-926 vs 918-920 above). Same cause as level.
   Fix type: CODE. Fix 3.

## level-stump — FAIL

1. **Butt on top of the stump, torn ends fused.** `09-stump-close`: the collar's tear at (530-700, 380-800) overlaps the stump's top-left tear at (650-780, 380-520); the log's underside at x=100 is at y~830 with its contact shadow on the ground at y~850-900, i.e. the whole visible log is a stump-height ramp. Concept: butt on the ground, a stride from the stump.
   Fix type: CODE. Fix 2.
2. **Top is a low-poly crystal, not the concept's chopped face / splinter band / level break.** The r2 band formula is in (`SFHarvestLocalizedGeometry.cpp` 327-328) but `Band` floors at 0.2, so ±6 cm of random facets cover the parts that should be a plane and a level break; the 30 cm amplitude on 7 cm cells (Grid 17 over ~115 cm) makes 10-25 cm flat triangles, not fibres. In the `L09` crop: the near third (player/chopped side, toward the camera) is faceted ±5-8 cm with no single sloped plane; the mound at the right rear is 40 cm tall and 50 cm wide; no part of the top is within ±8 cm of the cut plane.
   Fix type: CODE (formula only; grid stays 17 for boolean robustness). See fix 4.
3. Grain now runs along the tear faces (resolved); the facet flanks are still flat 10-25 cm polygons with a visible facet edge every face. The object-space material cannot fix flat geometry; fix 4 reduces the facet size by lowering amplitude and narrowing the band.

Resolved here: root flare smooth, bottom silhouette rounded; lit cap (192,157,114); 8.4 % under luminance 30.

## slope-stump — FAIL

The closest stump view: butt on the ground beside the stump, flare smooth, 13.5 % dark. Fails on the top: three 25-30 cm triangular spikes (x 830-1180, y 290-560) and a ±10 cm faceted field elsewhere, no chopped plane, no level third (item 2 of level-stump). The butt's torn end at (420-640, 540-900) is a faceted block of the same kind.
Fix type: CODE. Fix 4.

## level-log — FAIL

1. **Log floats at stump height.** `10-log-close`: the collar end at (1540-1700, 570-700) is level with the stump top at (1720-1920, 560-620); nothing but air and shadow between the log and the ground along the right 400 px. Concept: log flat on the grass, butt on the ground.
   Fix type: CODE. Fixes 1 and 2.
2. **Vertical bark seam at x~1442** running the full log height (`L10` 4x crop): the rebuilt 0-100 cm lower trunk's bark (smooth, cream, cylindrical UV) meets the upper structure's bark (rougher, darker crevices) with a hard texture break; silhouette is continuous now (r2 step resolved). The builder notes the UV V continuation was not done.
   Fix type: ART. See fix 5.
3. The torn end face is edge-on to this camera (only the collar's top spikes are visible at 1560-1700); the end face that matches the stump is only readable in `09`. Secondary.
   Fix type: LEVEL (review camera). See fix 6.

## slope-log — FAIL

1. **Trunk leaves the slab 1.5 m from the butt.** `10-log-close`: contact shadow under the trunk only at x~900-1500; at x~300 the underside is at y~830 with the slab shadow at y~950+, the far trunk rising into the crown. The concept log lies on the grass for its full length.
   Fix type: CODE. Fix 1.
2. Vertical bark seam at x~1442, as level-log item 2.
   Fix type: ART. Fix 5.
3. Torn end face edge-on at the right frame edge (x 1600-1650), only a thin orange sliver.
   Fix type: LEVEL. Fix 6.

## level-frames (motion strip) — FAIL

Direction: correct. Hinge: correct, butt on the stump through 22 deg, lifting off by 38 deg. Speed: accelerating to 50 deg/s at 50-66 deg, then braked to 15 deg/s by the damping applied at first leaf contact (66 -> 78 in 0.79 s). Ground contact: leaf hulls from ~48 deg; the trunk never touches the ground. Butt: lifts, then lands on the stump's top corner. Rest: frozen by the 2.5 s cap at 79.2 deg with a 7.5 m crown standing. Fixes 1, 2.

## slope-frames (motion strip) — FAIL

Direction and hinge correct; 2 -> 78 deg in 2.2 s with proper acceleration (50 -> 78 in 0.53 s); no slide after impact: this part passes. Rest: 9.8 deg above the slab, propped on the inner crown, frozen by the settle cap 2.5 s after the yield while still moving. Fix 1.

---

## Fix list (shared fixes first)

1. **Engineer — CODE — planar fall so the crown can yield to 60 cm without the body rolling, damping only once the trunk is down, settle long enough to finish.** The builder's 60 cm trial rolled because nothing constrains roll about the trunk axis after the hinge breaks. Lock the body to the fall plane the moment the hinge is released; then the 60 cm yield from r2 is safe and the trunk reaches the ground.
   `GameClient/Source/ShatteredFrontier/Private/Gathering/SFHarvestFallingTree.cpp` line 209-210, replace the hinge-break line with:
   ```cpp
   if (LeanDegrees>=Settings->HingeBreakDegrees)
   {
       Hinge->BreakConstraint(); Hinge->DestroyComponent(); Hinge=nullptr;
       // Planar fall: pitch about the lean axis only. No roll about the trunk (a trunk-only body would roll onto its
       // back once the limbs yield) and no yaw or sideways skating along a slope.
       FBodyInstance* BI=Body->GetBodyInstance(); BI->CustomDOFPlaneNormal=LeanAxis; BI->SetDOFLock(EDOFMode::CustomPlane);
   }
   ```
   (`EDOFMode::CustomPlane` locks translation along the normal and rotation about the two in-plane axes; the body may translate in the fall plane and rotate about `LeanAxis`.)
   Line 79 in `YieldCrown`: keep `SetLinearDamping`, delete `Body->SetAngularDamping(...)` from it. In `Tick` after the yield block (line 214) add:
   ```cpp
   if (bCrownYielded&&!bSettleDamped&&Now-FirstContactAt>=0.6)
   { bSettleDamped=true; Body->SetAngularDamping(Settings->SettleAngularDamping); }
   ```
   with `bool bSettleDamped=false;` beside `bCrownYielded` in `SFHarvestFallingTree.h` line 117 (the fall keeps accelerating through its last 30 deg; the brake arrives when the trunk is on the ground).
   `GameClient/Config/DefaultGame.ini` line 142 `CrownYieldRadiusCm=90.0` -> `60.0`; line 145 `MaxSettleSeconds=2.5` -> `4.0` (both runs hit the cap while still moving; the quiet test must be what freezes the pose).
   Acceptance: `HARVEST_FELLING_REST` lean >= 87 deg on level and >= 100 deg on the 15 deg slab; 50 -> 78 deg <= 0.6 s on level; `FALL_RESTING` before `MaxSettleSeconds` on at least one map (quiet test passed); rest roll (second rotator component, pitch/yaw/roll as logged) within ±15 deg of the 78 deg shot; trunk underside touches the ground along its visible length in `08-rest` on both maps.

2. **Engineer — CODE — the butt slides off the stump instead of resting on its corner.** `GameClient/Source/ShatteredFrontier/Private/Gathering/SFHarvestPresentation.cpp` after line 343 (`SetCollisionProfileName(TEXT("BlockAll"))`): give the stump box a slick surface so the collar sphere, which already sits with its centre at the box's fall-side edge, slides off under the trunk's weight once fix 1 lets the trunk pitch down:
   ```cpp
   auto* Slick=NewObject<UPhysicalMaterial>(StumpCollision,TEXT("StumpSlick")); Slick->Friction=0.05f; Slick->FrictionCombineMode=EFrictionCombineMode::Min; Slick->Restitution=0.f;
   StumpCollision->SetPhysMaterialOverride(Slick);
   ```
   (`#include "PhysicalMaterials/PhysicalMaterial.h"`.) If after fix 1 the butt still rests on the stump on level, add the felling kick at the hinge break in `SFHarvestFallingTree.cpp` (inside the fix 1 block, after the DOF lock): `Body->AddImpulseAtLocation(FVector::VectorPlaneProject(Body->GetUpVector(),InitialUp).GetSafeNormal()*Settings->MassKg*120.f,CollarSupport?CollarSupport->GetComponentLocation():Body->GetComponentLocation());` (1.2 m/s forward at the butt, the hinge wood snapping).
   Acceptance: in `08-rest` on level >= 25 px of ground visible between the stump's downhill silhouette and the butt's torn end at the 1500 cm camera; the collar sphere's bottom within 3 cm of ground height at rest (`BodyLocation` Z plus the collar offset); in `09-stump-close` the butt's tear does not overlap the stump's tear.

3. **Engineer — CODE — felling notch that bites the silhouette.** Keep the robust prism; put its wide face at the bark instead of 18 cm outside it, so the full 1.2 R opening is cut from wood. `SFHarvestLocalizedGeometry.cpp` lines 158-161:
   ```cpp
   const FVector3d Local[] = {
       {-Cut.Radius, -Cut.Radius * 0.6, Cut.Depth * 0.15}, {Cut.Radius, -Cut.Radius * 0.6, Cut.Depth * 0.15},
       {0, -Cut.Radius * 0.6, -Cut.Depth}, {-Cut.Radius, Cut.Radius * 0.6, Cut.Depth * 0.15},
       {Cut.Radius, Cut.Radius * 0.6, Cut.Depth * 0.15}, {0, Cut.Radius * 0.6, -Cut.Depth}};
   ```
   (wide face 2.7 cm outside the bark so it never lies in the bark surface; opening at the bark ~26 cm tall x 50 wide, apex 18 cm in; `WoodDepthCm` stays 18, which the boolean already accepts). Re-run `ShatteredFrontier.Gathering.{TreeSplit,FellingAuthority}`; if the second strike fails the weld, raise the retry nudges on line 204 from 3 to 5.
   Acceptance: `00-notched` left-silhouette bite >= 18 px deep x >= 24 px tall at 1 px/cm on both maps, with at least one lit (mean R >= 160) face of the wedge visible.

4. **Engineer — CODE — tear with a real plane, a narrower fibre band and a level break (no grid change).** `SFHarvestLocalizedGeometry.cpp` line 319: `30.0` -> `22.0` (amplitude cap; 7 cm cells cannot carry 30 cm spikes without 20 cm facets). Lines 327-328, replace with:
   ```cpp
   // Full tear only in a ~0.4 R band behind the notch; +-0.7 cm elsewhere so the far side reads level and the chopped face is a plane.
   const double Band=FMath::Max(0.03,1.0-FMath::Clamp((FMath::Abs(W+0.2*Cut.Radius)-0.2*Cut.Radius)/(0.25*Cut.Radius),0.0,1.0));
   const double Chop=FMath::Max(0.0,W-0.3*Cut.Radius);
   Delta[ID]=(Chop>0?0.0:Amplitude*Band*(Noise[ID]-ContactNoise))+0.025*FromNotch.Dot(U)-0.04*W-0.45*Chop;
   ```
   Keep the `HeightScale` clamp; re-run `ShatteredFrontier.Gathering.TreeSplit`.
   Acceptance: in `09-stump-close` the chopped third (toward the player/camera) is one sloped plane with no facet taller than 3 cm; the far third within ±4 cm of the cut plane; spikes confined to a band <= 0.5 R wide, tallest <= 22 cm; butt face still the complement.

5. **Codex — ART — continue the upper structure's bark across the join.** `GameClient/ArtSource/HarvestOak/MightyOak-Felling-v1/tools/Build-SmoothLowerTrunk.py` line 99: instead of `(j / SEGMENTS * circ * density, loop.vert.co.z * density)`, read the upper mesh's lowest ring loop UVs once (vertices within 2 mm of `top`, sorted by angle to match `rings[-1]`'s order) into `top_uv[j]`; set the top ring's loops to `top_uv[j]` verbatim and every lower ring's loop to `(top_uv[j].x, top_uv[j].y - (top - loop.vert.co.z) * density_v)` where `density_v` is the upper mesh's own V-per-metre measured from its two lowest rings (so U columns and V scale match at the seam; U wraps where the upper mesh wraps). Confirm `SM_MightyOak_LowerTrunk` slot 0 uses the same material instance (same normal/roughness maps) as `SM_MightyOak_UpperStructure` slot 0 in `tools/Update-HarvestOakFellingArt.py`; the r3 seam is lighter and smoother on the lower side, which a UV fix alone will not hide if the materials differ. Re-import; split tests must pass.
   Acceptance: at the `10-log-close` camera no vertical bark discontinuity at the join (a 40 px wide column straddling x~1442 has the same mean luminance within 8 and the same crevice frequency as the columns either side).

6. **Engineer — LEVEL (review script) — show the torn butt face in the log shot.** `SFHarvestArtReviewScenario.cpp` line 368: `View(Butt-R.FallDir*300+R.Side*380+FVector(0,0,150),Butt+Rest.GetUnitAxis(EAxis::Z)*180,50);` -> `View(Butt-R.FallDir*420+R.Side*260+FVector(0,0,140),Butt+Rest.GetUnitAxis(EAxis::Z)*220,50);` (further behind the butt, less to the side, so the end face is ~35 deg off edge-on and the stump stays in frame on the right).
   Acceptance: in `10-log-close` the butt's torn face spans >= 120 px wide and the stump is in frame.

Fixes 1 and 2 are the gate: until the trunk lies on the ground with its butt beside the stump, neither sequence nor the log views can match the concept. Fixes 1-4 and 6 change existing code and config only; 5 is a rebuild of an existing asset.
