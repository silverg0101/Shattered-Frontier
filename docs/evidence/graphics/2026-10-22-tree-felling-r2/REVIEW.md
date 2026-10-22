# Tree felling r2 — independent review (2026-10-22)

Reviewer: Fable, no part in the build. Concepts: `GameClient/ArtSource/HarvestOak/MightyOak-Felling-v1/Concept/{sequence,stump,log}`. Captures: `Artifacts/HarvestArt/felling-review-02/{level,slope}` (1920x1080) with `client.log` / `server.log`. Pairs and strips in this folder. Every image was read at full resolution; crops and pixel counts were taken from the raw frames.

Scope applied as briefed: oak species, bark tone and the bare slab are not judged. The slab's tiled grass patches are ignored.

Result: **0 PASS / 8 FAIL**. The fall itself is now a fall (speed on slope, crown yield, 78 deg keyframe captured, SkippedLeans=0, foliage no longer black, cut-wood colour on target, 24-sided stump without facet lines, close-ups lit). What still breaks the design: the trunk never reaches the ground (it rests 9-11 deg nose-up, propped by the inner crown), the butt stays inside the stump on level ground (the stump's collision is WorldDynamic, which the butt collar ignores), the slope tree keeps sliding for 8 s after impact, the notch is a 22x17 px bite, and the torn surfaces are a crystalline spike field without the concept's chopped face / splinter band / level break.

Round-1 items resolved (not re-raised): 7 (cut-face colour), 8 (lighting, 09 close-ups 9.8 % / 3.4 % under luminance 30), 9 (foliage subsurface: shadow leaves sample (71,82,14) level, (62,74,7) slope), 4 (1 px/cm camera), 6 in part (no vertical facet edges on the bark), 1 in part (crown yields; `HARVEST_TREE_CROWN_YIELDED Radius=120` on both maps).

Measured facts used below (from `client.log`, `server.log` and pixel sampling of the raw frames):

| | level | slope |
|---|---|---|
| Rest lean from vertical (`HARVEST_FELLING_REST`) | 80.6 deg = trunk axis 9.4 deg nose-up (axis (0.985,-0.04,0.163)) | 94.3 deg = axis (0.98,-0.18,-0.075), i.e. 10.7 deg above the 15 deg slab |
| Lean 2 -> 50 deg | 1.45 s (53.34 -> 54.79) | 1.30 s |
| Lean 50 -> 66 deg | 0.32 s | 0.25 s |
| Lean 66 -> 78 deg | **1.84 s** (55.11 -> 56.96); 50 -> 78 = 2.17 s vs the 0.6 s acceptance | 0.26 s; 50 -> 78 = 0.5 s |
| 78 deg -> `FALL_RESTING` | ~1.1 s | **7.9 s** (32.74 -> 40.62 client; server 32.36 contact-yield -> 39.11 resting) |
| Body origin travel between 78 deg and rest | 3 cm | **X -15 -> -23, Y -356 -> -693: 338 cm along the contour, yaw 72 deg** |
| Butt position at rest (origin + 55 cm along the axis) | cut-face centre X=+42, Z=+27; tear spikes reach back to X=+12, inside the stump (radius 28, Z 0-55) | 23 cm uphill of the stump axis, 3.4 m along the contour |
| Crown top at rest (highest leaf, 08-rest) | y=206 at 1500-1650 cm: ~8 m above ground (standing tree >10 m) | crown spans y 385-1000 |
| Notch silhouette bite, 00-notched left edge | **22 px deep x 17 px tall** (rows 928-945; trunk 57 px wide) | patch 20x35 px, no measurable bite |
| Tear field height, 09-stump-close | 310 px (y 250-560) at ~7.7 px/cm = 40 cm | 230 px |
| Cut-wood lit mean RGB | (196,152,95) vs concept cap (193,145,99) | (209,167,108) |
| Frame under luminance 30: 09 / 10 | 9.8 % / 19.0 % | 3.4 % / 20.4 % |
| Lower-trunk/upper-structure join on the log (10-log-close) | texture break at x~1500-1530 | **bark texture break at x~1430-1445 with a 15 px (~2.5 cm) silhouette step on the top edge** |

---

## level-sequence — FAIL

1. **Trunk does not land; it is a ramp from the stump to the crown.** Concept frame 4: trunk flat on the grass for its full length, crown crushed. Live `08-rest`: axis 9.4 deg nose-up; in the frame the trunk's underside at x=1300 (3 m from the stump) is ~80 px above the ground line (y~930 vs y~1010) with open ground between the trunk and its cast shadow; the crown still stands ~8 m tall (highest leaf y=206, horizon y=545). The crown yielded at radius 120 cm, but the primary limb hulls inside 120 cm still prop the tree.
   Fix type: CODE. See fix 2.
2. **Butt never leaves the stump.** Concept: butt ~1 m past the stump, torn end on the ground. Live `08-rest` and `L08` crop: the torn stump top and the trunk's torn end are one fused mass at (915-1000, 895-960); no gap at all. Cause: `StumpCollision` uses profile `BlockAllDynamic` (object type WorldDynamic) while `CollarSupport` blocks only `ECC_WorldStatic` (`SFHarvestFallingTree.cpp` line 151), so the 55-100 cm collar passes through the stump box and the body settles with its torn end 15 cm inside the stump's downhill flank.
   Fix type: CODE. See fix 1.
3. **Notch unreadable.** Concept frame 1: bright wedge, ~45 % of the trunk width, bite in the silhouette. Live `00-notched` at 1 px/cm: a 22x17 px triangular bite on the left silhouette plus a 20x35 px dark-orange patch (mean (149,111,72), in the crown's shadow). The `TreeNotch` cutter is a prism 2R wide but only 0.7R (17.5 cm) tall, tapering to a line at 18 cm depth; three stacked chips at one point give one 18x17 cm bite.
   Fix type: CODE. See fix 4.
4. **Fall stalls at the end.** 22-66 deg runs at a constant ~45 deg/s (a tree should still be accelerating past 60 deg/s here), then 66 -> 78 deg takes 1.84 s and the tree stops at 80.6 deg. Same root cause as 1.
   Fix type: CODE. See fix 2.

## slope-sequence — FAIL

Fall direction (downhill, away from the player) and hinge are correct, and the fall accelerates properly (50 -> 78 in 0.5 s). Then:

1. **Eight seconds of sliding after impact.** `07-lean-78` -> `08-rest`: the body origin moves 338 cm along the slope contour (Y -356 -> -693) with a 72 deg yaw and the butt comes to rest 23 cm uphill of the stump axis, 3.4 m to the side; the strip shows this as a jump between the two frames, and a player watches the felled tree skate sideways for 8 s before it freezes. The yielded hulls leave the body on a few frictionless convex hulls with zero damping.
   Fix type: CODE. See fix 2.
2. **Trunk 10.7 deg above the slab**, propped by the inner crown as on level; the lower trunk in `08-rest` runs from (815,890) to (1120,960) in front of a slab that descends 15 deg, so the far trunk leaves the ground.
   Fix type: CODE. See fix 2.
3. Notch: same 20x35 px patch as level, no measurable bite.
   Fix type: CODE. See fix 4.

## level-stump — FAIL

1. **The whole top is a noise field; the concept's structure is missing.** Concept: a clean sloped chopped face over ~40 % of the top, a 15-25 cm band of fine splinters behind it, the rest a level break. Live `09-stump-close`: 100 % of the top is +-30 cm random spikes 40 cm tall (y 250-560); no chopped face, no level area. `SFHarvestLocalizedGeometry.cpp` line 311 applies `Amplitude*(Noise-ContactNoise)` to every grid vertex; the 0.025/0.04 slopes are swamped.
   Fix type: CODE. See fix 3.
2. **Spikes are 7-15 cm flat polygons with plank grain running across them.** Grid 17 over ~110 cm gives 7 cm cells; each facet is one flat triangle and the end-grain texture is projected in the cap's UV so grain streaks cross the spikes at random angles (`L09` crop). The concept splinters are 1-3 cm fibres along the grain.
   Fix type: ART (material) + CODE (grid). See fix 6.
3. **Butt interpenetrates the stump.** The log's torn end at (380-700, 400-850) runs into the stump's left flank at x~650-700; geometry puts the butt's spikes 15 cm inside the stump.
   Fix type: CODE. Fix 1.
4. **Root flare reads as a five-sided faceted cone.** `L09-base` crop: hard creases on the flare at x~920, 1120, 1300 and a polygonal bottom silhouette. `Build-SmoothLowerTrunk.py` line 67 `cos(5*theta+0.7)**3` makes five sharp lobes over 10 rings.
   Fix type: ART. See fix 7.

Resolved here: lit cut-wood (196,152,95) within 10 of the concept cap; 9.8 % of the frame under luminance 30; no vertical bark facet lines.

## slope-stump — FAIL

Same spike field (y 320-560, 230 px) with the same facet/grain look, items 1, 2 and 4 of level-stump. Base contact on the uphill side reads buried (the mesh is now built 20 cm below ground), the downhill side is behind the stump in this camera and remains unverified; not scored. 3.4 % under luminance 30, cut-wood (209,167,108): resolved.
Fix type: CODE/ART. Fixes 3, 6, 7.

## level-log — FAIL

1. **Butt fused into the stump.** `10-log-close`: the torn end at (1540-1680, 500-830) meets the stump's left silhouette at x~1680 with no ground between them; concept shows the butt a stride from the stump with grass under its end.
   Fix type: CODE. Fix 1.
2. **Visible join ring on the log.** `L10-seam` crop: the bark texture changes scale and direction at x~1500-1530, 45 cm from the butt, where the rebuilt 0-100 cm lower trunk meets `SM_MightyOak_UpperStructure`.
   Fix type: ART. See fix 5.
3. Torn butt facets as in level-stump item 2.
   Fix type: ART/CODE. Fix 6.
4. Log underside is on the ground only near the butt (from `08-rest` the axis rises 9.4 deg).
   Fix type: CODE. Fix 2.

19 % of the frame is under luminance 30 (the log's own contact shadow and the crown); readable, not scored.

## slope-log — FAIL

The closest view: the log lies on the slab, the butt is clear of the stump (because the tree slid 3.4 m, not by design), the torn end reads as broken wood.

1. **Join ring with a lip.** `S10-butt` crop: bark texture break at x~1430-1445 and the top silhouette steps up 15 px (~2.5 cm radius) at the seam, 45 cm from the butt. The rebuilt top ring (24 radii ray-cast from the old 8-sided chunk, `Build-SmoothLowerTrunk.py` lines 55-64) does not coincide with the upper structure's base ring.
   Fix type: ART. See fix 5.
2. Torn butt facets as in level-stump item 2.
   Fix type: ART/CODE. Fix 6.

## level-frames (motion strip) — FAIL

Direction: correct. Hinge: correct, butt on the stump through 22 deg, released by 36 deg. Speed: ~45 deg/s constant from 22 to 66 deg, then 1.84 s for the last 12 deg; stops at 80.6 deg. Ground contact: crown hulls from ~40 deg; the trunk never touches. Butt: stays on, then in, the stump. Rest: a ramp from the stump into an 8 m standing crown. Fixes 1 and 2.

## slope-frames (motion strip) — FAIL

Direction and hinge correct; 2 -> 78 deg in 1.8 s with proper acceleration (50 -> 78 in 0.5 s): this part passes. Rest: 7.9 s of sliding and yawing after the 78 deg frame, 3.4 m of travel, trunk 10.7 deg above the slab. Fix 2.

---

## Fix list (shared fixes first)

1. **Engineer — CODE — the butt must stop at the stump.** `GameClient/Source/ShatteredFrontier/Private/Gathering/SFHarvestPresentation.cpp` line 341: `StumpCollision->SetCollisionProfileName(TEXT("BlockAllDynamic"));` -> `StumpCollision->SetCollisionProfileName(TEXT("BlockAll"));` (object type WorldStatic, which `CollarSupport` blocks, `SFHarvestFallingTree.cpp` line 151). Line 345: size the box to the cut plane, not the tear tips, so the butt slides off the real stump height: `const double TopZ=FMath::Min(Bounds.Max.Z,double(Cuts.Last().Point.Z)); StumpCollision->SetBoxExtent(FVector((Bounds.Max.X-Bounds.Min.X)*0.5,(Bounds.Max.Y-Bounds.Min.Y)*0.5,(TopZ-Bounds.Min.Z)*0.5));` and use `FVector(Center.X,Center.Y,(Bounds.Min.Z+TopZ)*0.5)` for the location on line 346. `SFHarvestFallingTree.cpp` line 183: the stump is now static, so keep the crown yield tied to the ground: `if (HasAuthority()&&Other&&Other->GetCollisionObjectType()==ECC_WorldStatic&&Other->GetOwner()!=Source.Node) { bSawStaticContact=true; }`. Acceptance: in `08-rest` on level, >= 25 px of ground visible between the stump's downhill silhouette and the butt's torn end at the 1500 cm camera; rest body origin X >= +25 on level; the butt's end face on the ground, not on the stump.

2. **Engineer — CODE — the trunk lands and the tree settles.** `GameClient/Config/DefaultGame.ini` line 142: `CrownYieldRadiusCm=120.0` -> `CrownYieldRadiusCm=60.0` (only the trunk and limb bases stay rigid; the limbs crush). `GameClient/Source/ShatteredFrontier/Public/Gathering/SFHarvestFallingTree.h`, `USFHarvestTreeFallSettings`, after `CrownYieldRadiusCm`:
   ```cpp
   UPROPERTY(Config, EditAnywhere, Category="Rest", meta=(DisplayName="Settle angular damping", ClampMin="0", ClampMax="10", ToolTip="Angular damping applied to the trunk body once the crown has yielded, so the felled tree stops instead of rolling. Per second. Release authors; next world."))
   float SettleAngularDamping=3;
   UPROPERTY(Config, EditAnywhere, Category="Rest", meta=(DisplayName="Settle linear damping", ClampMin="0", ClampMax="5", ToolTip="Linear damping applied with the angular damping; dirt does not let a 1.5 t trunk skate. Per second. Release authors; next world."))
   float SettleLinearDamping=1;
   UPROPERTY(Config, EditAnywhere, Category="Rest", meta=(DisplayName="Max settle seconds", ClampMin="1", ClampMax="6", Units="s", ToolTip="Hard cap after the first ground contact; the pose freezes even if the quiet test has not passed. Release authors; next world."))
   float MaxSettleSeconds=2.5;
   ```
   `Validate()`: `&&In(SettleAngularDamping,0,10)&&In(SettleLinearDamping,0,5)&&In(MaxSettleSeconds,1,6)`. `DefaultGame.ini` after line 149: `SettleAngularDamping=3`, `SettleLinearDamping=1`, `MaxSettleSeconds=2.5`. `SFHarvestFallingTree.cpp` line 207, inside the yield block after `YieldCrown(...)`: `Body->SetAngularDamping(Settings->SettleAngularDamping); Body->SetLinearDamping(Settings->SettleLinearDamping); FirstContactAt=Now;` (add `double FirstContactAt=0;` beside `QuietSince`). Line 213: `if (Now-QuietSince<Settings->RestQuietSeconds&&(FirstContactAt==0||Now-FirstContactAt<Settings->MaxSettleSeconds)) { return; }` and move the `if (!bQuiet) { QuietSince=0; return; }` on line 211 to `if (!bQuiet) { QuietSince=0; if (FirstContactAt==0||Now-FirstContactAt<Settings->MaxSettleSeconds) { return; } }`. Acceptance: `HARVEST_FELLING_REST` lean >= 87 deg on level and >= 85 deg slab-relative on slope; 50 -> 78 deg <= 0.6 s on level; `FALL_RESTING` <= 3 s after the 78 deg shot; body origin within 50 cm horizontally of its 78 deg position; in `08-rest` the trunk's underside touches the ground along its visible length.

3. **Engineer — CODE — tear with the concept's structure (chopped face, splinter band, level break).** `GameClient/Source/ShatteredFrontier/Private/Gathering/SFHarvestLocalizedGeometry.cpp` line 311, replace the single line with:
   ```cpp
   const double W=FromNotch.Dot(WorkingFace);                                   // + toward the chopped face, - toward the far side
   const double Band=FMath::Max(0.2,1.0-FMath::Clamp((FMath::Abs(W+0.2*Cut.Radius)-0.3*Cut.Radius)/(0.3*Cut.Radius),0.0,1.0)); // full tear in a ~0.6 R band behind the notch, 20 % elsewhere
   Delta[ID]=Amplitude*Band*(Noise[ID]-ContactNoise)+0.025*FromNotch.Dot(U)-0.04*W-0.45*FMath::Max(0.0,W-0.3*Cut.Radius); // chopped face drops ~24 deg toward the player past the band
   ```
   Keep the `HeightScale` clamp. Re-run `SFHarvestTreeSplitTests`. Acceptance: in `09-stump-close` the working-face third of the top is a single sloped face (no spike taller than 5 cm there), the middle band carries the 30 cm tear, the far third is within +-8 cm of the cut plane; the butt face is still the complement.

4. **Engineer — CODE — felling notch that reads as a wedge.** `SFHarvestLocalizedGeometry.cpp` lines 156-159, the `TreeNotch` prism: give it a level floor and a 45 deg roof instead of a 0.35R half-height taper:
   ```cpp
   const FVector3d Local[] = {
       {-Cut.Radius, -Cut.Radius * 0.15, Cut.Depth}, {Cut.Radius, -Cut.Radius * 0.15, Cut.Depth},
       {0, -Cut.Radius * 0.15, -Cut.Depth}, {-Cut.Radius, Cut.Depth * 1.1, Cut.Depth},
       {Cut.Radius, Cut.Depth * 1.1, Cut.Depth}, {0, -Cut.Radius * 0.15, -Cut.Depth}};
   ```
   (V is vertical: floor at -0.15R held level to the apex, roof opening = 1.1 x depth at the bark; check the apex pair stays a valid prism edge). `GameClient/Config/DefaultGame.ini` line 89 `WoodDepthCm=18.0` -> `WoodDepthCm=26.0`, raising that setting's `ClampMax` to 28 in its header (43 % of the 60 cm trunk; the hinge keeps 30 cm). Acceptance: `00-notched` left-silhouette bite >= 24 px deep x >= 28 px tall at 1 px/cm with the roof face lit.

5. **Codex — ART — close the lower-trunk/upper-structure seam.** `GameClient/ArtSource/HarvestOak/MightyOak-Felling-v1/tools/Build-SmoothLowerTrunk.py`: read `SM_MightyOak_UpperStructure`'s lowest ring (vertices with z within 2 mm of `top`), and use those positions as `rings[-1]` verbatim (resample to its count if it is not 24, and build the lower rings with that count); for the top two rings take the UV V from the upper mesh's ring loops and the same U-per-metre `density` so the bark continues; do not apply `smooth[]` to the last two rings. Re-import with `tools/Update-HarvestOakFellingArt.py`; the runtime split must still pass (24x10 was the limit found in r2, so keep the ring count at or below that). Acceptance: at the `10-log-close` camera no silhouette step > 2 px and no texture break at the join; split tests pass.

6. **Codex — ART — tear material reads as fibre, not polygons.** (a) `M_MightyOak_CutWood` (slot 1 on the lower trunk/collar): replace the UV-mapped end-grain with object-space projection: rings (`T_MightyOak_CutWood_BaseColor`) sampled with object XY where `abs(ObjectNormal.Z) > 0.6`, and a new fibre texture sampled with object XZ/YZ (triplanar, Z along the trunk) on steeper faces, blended by `saturate((abs(ObjectNormal.Z)-0.4)/0.4)`; roughness 0.75. (b) New fibre texture `GameClient/ArtSource/HarvestOak/MightyOak-v1/materials/T_Oak_Splinter_BaseColor.png` + normal, 1024x1024 tileable, prompt: "freshly torn oak wood fibres, long parallel pale cream splinters running vertically, fine tan grain streaks, shallow relief, flat even daylight, seamless tile, photoreal", target mean RGB (205,165,120) +-10. (c) Engineer, optional, only if the boolean stays stable: `SFHarvestLocalizedGeometry.cpp` line 290 `Grid=17` -> `Grid=25` (4.6 cm cells). Acceptance: in `09-stump-close` spike flanks show streaks along the spike direction, no plank grain crossing a facet.

7. **Codex — ART — organic root flare.** `Build-SmoothLowerTrunk.py` line 25: `FLARE_GAIN=0.5` -> `0.35`; line 67: `** 3` -> `** 1.5`; line 24 `RINGS=10` keep, but distribute six of the ten rings in the bottom 0.3 m (`heights` as `[-BURY_M, -0.1, 0, 0.05, 0.1, 0.17, 0.25, 0.35, 0.5, 0.7, 0.85, top]`); export with smooth shading. Acceptance: no hard crease on the flare at the `09-stump-close` camera; bottom silhouette rounded.

Fixes 1 and 2 are the gate: until the trunk lies on the ground with its butt beside the stump, neither sequence nor the log views can match the concept. Fixes 1-4 use existing code and config; 5-7 are art rebuilds of assets that already exist.
