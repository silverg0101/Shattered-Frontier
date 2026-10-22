# Tree felling review r4

Same run order as r3 (`../2026-10-22-tree-felling-r3/README.md`); branch rebased onto origin/main before this capture. Targeted tests `ShatteredFrontier.Gathering.{TreeSplit,LocalizedGeometry,LocalizedImportedContact,FellingAuthority}` 4/4 passed; FellingAuthority now also checks crown yield, replicated rest pose = frozen body pose, rest lean >= 60 deg, butt not below ground, world-static stump box with its top at the final cut.

Implemented from `../2026-10-22-tree-felling-r3/REVIEW.md`:

1. Fall-plane DOF lock at hinge break; angular damping 0.6 s after landing; `CrownYieldRadiusCm=60`, `MaxSettleSeconds=4`.
2. Slick stump physical material, plus the forward 1.2 m/s kick at the butt on hinge break (the butt still rested on the stump with the slick material alone).
3. Notch prism wide face at 0.15 Depth outside the bark; retry nudges 3 -> 5.
4. Tear amplitude cap 22, narrow band, planar chopped face.
5. Deviation: taking the upper structure's base-ring UVs per column stretched the bark into vertical stripes on the 24-column loft (seen in a discarded r4 capture), so the lower trunk keeps cylindrical UVs at the source texel density. The geometry seam stays closed; a bark texture break at the 1 m join remains. Both meshes use `M_MightyOak_Bark` in slot 0.
6. Log close-up camera moved behind the butt as specified.
