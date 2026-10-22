# Tree felling review r9

Same run order as r3. Targeted tests `ShatteredFrontier.Gathering.{TreeSplit,LocalizedGeometry,LocalizedImportedContact,FellingAuthority}` 4/4 passed. TreeSplit now also covers the pristine-collar split: it succeeds, replays identically, is closed, and holds at least the notched collar's wood.

From `../2026-10-22-tree-felling-r8/REVIEW.md`:

1. Done as specified. `SplitTree` takes an optional pristine trunk. All three runtime call sites pass `Runtime->Base`, so the collar is cut from the uncut trunk and the stump from the notched one. Conservation now checks that stump + collar lies between the notched volume and the pristine volume, and the cap vertices must still match. The front-band drop is widened: full to 1.2 R across, ramp to 1.4 R; full in depth to -(D + 0.1 R), ramp to -(D + 0.35 R).
2. Not changed: level-only framing for the log close-up. The driver has no per-map camera, and the r8 Side-distance change already applies to both maps.

Owner flag: the log is now the un-notched collar, with the notch volume restored on the log. The fall direction flag is carried over.
