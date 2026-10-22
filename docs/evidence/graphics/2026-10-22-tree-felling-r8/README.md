# Tree felling review r8

Same run order as r3. Targeted tests `ShatteredFrontier.Gathering.{TreeSplit,LocalizedGeometry,LocalizedImportedContact,FellingAuthority}` 4/4 passed.

From `../2026-10-22-tree-felling-r7/REVIEW.md`:

1. Not done (deviation): the two-surface split was implemented and failed. Its collar boolean left open edges on the slot roof in `ShatteredFrontier.Gathering.TreeSplit` (all six profiles), so every felling would be rejected. A lateral-only notch footprint was also tried and left a V-shaped pit in the stump top. The split stays a single surface that drops over the front band holding the notch.
2. Done: the splinter band is centred behind the notch apex (W = -(1.3 Depth + 0.2 R)), and the tear is masked out of the dropped front band.
3. Done: `HitCm` removed from the review driver. The log keeps `RadialToViewDeg`.
4. Done: the log close-up eye moved closer (Side 620 -> 500 cm). This applies to both maps.
