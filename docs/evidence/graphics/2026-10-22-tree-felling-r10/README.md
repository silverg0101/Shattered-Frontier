# Tree felling review r10

Same run order as r3. From `../2026-10-22-tree-felling-r9/REVIEW.md`:

1. The runtime option was applied. `ASFHarvestFallingTree::PrepareVisualSource` sets the falling upper structure's slot 1 (the 1 m join cap) to its bark material. The asset is not re-exported. On the standing tree the cap stays hidden inside the join.
2. Each rejected split profile is now logged at Display as `TREE_SPLIT_PROFILE_REJECTED Seed Attempt <reason>`.
3. Not changed: level-only log framing, as noted in r9.

The first slope capture of this round hitched under machine memory pressure. The client skipped from the 2 deg frame straight to rest, so that capture was discarded and the slope run repeated.
