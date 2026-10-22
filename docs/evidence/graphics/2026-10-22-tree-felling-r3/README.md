# Tree felling review r3

Run order: `tools/Create-TreeFellingDevMap.py` (open editor), `tools/Update-HarvestOakFellingArt.py` (open editor; art fixes), build, then with the editor closed `tools/Test-HarvestFellingReview.ps1 -Ground Level|Slope -OutputDirectory <Artifacts/HarvestArt/felling-review-03/...>`, then `py -3 make_pairs.py <that root>`. Targeted tests: `ShatteredFrontier.Gathering.{TreeSplit,LocalizedGeometry,LocalizedImportedContact,FellingAuthority}` 4/4 passed.

Implemented from `../2026-10-22-tree-felling-r2/REVIEW.md`, with deviations:

1. Stump box is `BlockAll` (world-static) with its top at the final cut. Deviation: the box XY is the collar's section, not the stump bounds (the root flare made a 85 x 107 cm box the butt could not slide off). Stump contact also counts as a landing (excluding it left a tree propped on the stump with no rest ever confirmed).
2. Settle damping 3 / 1 and a 2.5 s settle cap are typed settings. Deviation: `CrownYieldRadiusCm=90`, not 60: at 60 the trunk-only body rolled onto its back (rest roll -120 deg, crown below ground). 90 and 120 trials: `Artifacts/HarvestArt/felling-proto-yield{90,120}`.
3. Tear band mask and chopped-face slope as specified.
4. Deviation: the level-floor wedge cutter made repeated strikes fail the boolean weld (near-coplanar faces; authority test and live review both rejected the third strike). The notch keeps the robust prism, enlarged from 0.35 R to 0.6 R tall each way; `WoodDepthCm` stays 18 (26 also failed the third strike). The cutter boolean retries with three deterministic sub-centimetre nudges.
5. The join ring is sampled from the upper structure's own base section and the last two rings are not smoothed. Not done: continuing the upper mesh's bark UV V across the join.
6. Cut wood is object-space: end-grain rings on near-horizontal faces, a new torn-fibre texture (`Concept/splinter`) on steep faces. Not done: a fibre normal map; 6c (Grid 25) skipped as optional.
7. Flare gain 0.35, buttress power 1.5, six rings in the bottom 0.3 m.

Also changed: the hinge is now at the fall-side edge of the collar (the butt lifts off the stump as in real felling) and the initial lean torque continues until 8 deg, past the trunk's balance over that edge.
