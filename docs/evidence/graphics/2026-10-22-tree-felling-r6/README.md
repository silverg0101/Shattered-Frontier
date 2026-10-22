# Tree felling review r6

Same run order as r3. Targeted tests `ShatteredFrontier.Gathering.{TreeSplit,LocalizedGeometry,LocalizedImportedContact,FellingAuthority}` 4/4 passed.

Implemented from `../2026-10-22-tree-felling-r5/REVIEW.md`:

1. Review sun set Movable before rotating; `client.log` logs `HARVEST_REVIEW_SUN Rot=...` and has no Movable warning.
2. Split surface: noise 3x3 box-smoothed (x1.6 to keep its range); only the tear term scales to the room (`TREE_SPLIT_SURFACE ... TearScale` logged; 1.000 on both maps), the chopped plane and tilt are clamped to the room separately.
3. On the chopped side the split rises 0.6 R (top of the notch mouth), blended over 0.3 R, so the wedge stays on the stump.
4. Log close-up camera as specified.
5. Triplanar bark uses normalised pow-4 weights.
