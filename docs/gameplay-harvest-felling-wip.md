# Tree felling

Status: accepted 2026-10-22. A fresh Fable review passed 8/8 views in round 10: `docs/evidence/graphics/2026-10-22-tree-felling-r10/REVIEW.md`, with rounds 1-9 beside it. `bEnablePreparedOakFelling=True` ships in `DefaultGame.ini`. The build recipe is [blueprints/vegetation.md](blueprints/vegetation.md) §6.

Goal: the final approved oak extraction leaves a torn stump and hands the upper collar, wood and foliage to a server-owned fall. Material grants, replay protection, owner-only progression and resource-generation cleanup are preserved. There is no fall damage and no new loot.

Owner decision 2026-10-19 ([GAT-01](decisions/GAT-01-growing-resource-nodes.md)): there is no persistent stump. The torn stump is transient fall presentation, removed with the fallen tree when the node renews.

## Mechanism

- **Fall (`SFHarvestFallingTree`).** The retained hinge sits at the collar's fall-side edge. The lean is driven until the trunk is past balance (8 deg), then the hinge releases at 25 deg. The body is then locked to the fall plane: no roll and no skating. On first static contact (ground or stump), crown hulls further than `CrownYieldRadiusCm` (60 cm) from the trunk axis stop colliding. That radius replicates, so clients release the same hulls. Linear damping applies at once and angular damping 0.6 s later. The pose freezes after the quiet test or `MaxSettleSeconds` (4 s). A welded sphere on the collar keeps the butt out of the ground, and the frozen pose is lifted onto the surface.
- **Split (`SFHarvestLocalizedGeometry::SplitTree`).** The notch cutter is a 0.6 R prism. Over the front band the split surface drops below the notch, so the stump keeps a chopped plane with a splinter band behind it. The collar is cut from the pristine trunk, so the log has a clean torn butt. If no profile gives matching caps, the same six profiles run on the notched trunk. Cutter booleans retry with small deterministic nudges.
- **Stump (`SFHarvestPresentation`).** The stump collision box is world-static, has a slick physical material, and its top sits at the cut. The falling butt slides off it.

## Rejected (do not retry)

These were tried and failed:

- A level-floor wedge cutter and an open-face (vertical V) cutter: repeated strikes left near-coincident faces, and the weld or the split failed.
- A two-surface split: the collar boolean left open edges.
- Per-column copies of the upper structure's UVs: they striped the bark. Bark is object-space triplanar instead.
- Crown yield at 60 cm without the plane lock: the trunk rolled onto its back.

## Owner flags

- The tree falls away from the notch face (`TreeSplitFor`: `FallDirection = -Cut.Normal`). Real felling and the concept drop it toward the notch.
- The log carries no notch faces, because the notch volume is restored on the log.
- The shared `MightyOakV1` art changed: smooth lower trunk, object-space cut wood and bark, foliage subsurface. Standing harvest oaks in every map now use it.
