# Tree felling review r7

Same run order as r3. Targeted tests `ShatteredFrontier.Gathering.{TreeSplit,LocalizedGeometry,LocalizedImportedContact,FellingAuthority}` 4/4 passed.

Implemented from `../2026-10-22-tree-felling-r6/REVIEW.md`:

1. Sequence camera framed from the radial mouth direction (trunk axis to the cut point) at 40 deg; `client.log` logs `HARVEST_REVIEW_NOTCH_VIEW RadialToViewDeg=40.0 HitCm=...` (HitCm is the distance from the first visibility hit to the cut point; the cut point is the deepest contact inside the notch, ~20 cm in from the bark).
2. Deviation: two split surfaces were not built. An open-face (vertical V) cutter was implemented first and rejected: every live felling then failed the split preflight (all six profiles, near-coincident notch faces), so the notch keeps the robust prism. Instead, over the notch's footprint the single split surface drops below the slot's floor (-0.6 R - 6 cm), so the slot's cut-wood walls leave with the log and the stump front is the chopped plane, with the tear band behind it. The split retries now shift the surface +-1.7 cm steps (six profiles).
3. Log close-up aims at the butt/stump midpoint.
