# Tree felling review r5

Same run order as r3 (`../2026-10-22-tree-felling-r3/README.md`). Targeted tests `ShatteredFrontier.Gathering.{TreeSplit,LocalizedGeometry,LocalizedImportedContact,FellingAuthority}` 4/4 passed.

Implemented from `../2026-10-22-tree-felling-r4/REVIEW.md`:

1. Sequence camera swung 35 deg toward the notch side, review sun from behind the camera at 30 deg elevation, Side flip dropped (as specified; a 55 deg trial pushed the crown out of frame and was discarded).
2. Lower trunk top cap is bark (slot 0); the top ring is ray-sampled on the upper structure's own base section.
3. The affine UV fit was measured (`GameClient/ArtSource/HarvestOak/MightyOak-Felling-v1/SmoothLowerTrunk/build.json` `uv_fit`: U rms 0.16, V rms 0.32, so the upper UVs are an atlas), so the specified fallback was taken: `M_MightyOak_Bark` colour and roughness are object-space triplanar (136 cm tile) on both meshes; the normal map keeps mesh UVs.
4. Log close-up camera as specified.
5. Optional splinter tuft not done.

Owner flag carried from r4: the tree falls away from the notch face (`SFHarvestPresentation.cpp` `TreeSplitFor`), unlike real felling.
