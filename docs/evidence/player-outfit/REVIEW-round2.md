# Player start outfit: review, round 2

Concept: `GameClient/ArtSource/Characters/Player/reference-starter-male.webp`
Live: `front-compare.png`, `three_quarter-compare.png`, `side-compare.png` (idle), `walk_back-compare.png` (walking)
Out of scope: belt, pouch, wrist wraps, boots, face, hair.

## front — FAIL

- The tunic has a hard horizontal seam at hip level. Below it, a lighter, slightly wider panel hangs like a separate skirt, so the outfit reads as a dress over tights. The concept is one continuous straight drop from shoulder to mid-thigh, with no waist seam.
- The neckline is a wide boat scoop that shows the collarbones. The concept has a close round neck with a short V. The V is there, but it has no cross-stitch lacing or stitched trim.
- The cloth is a smooth fine knit or felt, with no coarse linen weave and no seams. The colour is now close to the oatmeal.
- The sleeves are right: elbow length, with a rolled cuff.

## three_quarter — FAIL

- The same hip seam and skirt panel are the dominant read in this view.
- The neckline and cloth have the same problems as the front view. The cuffs have no stitching.
- The shoulders follow the body, with no clipping visible.

## side — PASS

- The tunic drops straight, and a side slit is visible at the hem. The sleeve length and roll match the concept. The hip seam is barely readable from this angle.
- The cloth surface still lacks a weave (see the fix list). Nothing is broken.

## walk_back — FAIL

- Across the buttocks there is a dark, crumpled, noisy band, which is the round-1 hip flap moved to the back. Below it, the hem wraps around the right thigh like shorts and pulls up off the left leg, opening a gap between the legs. This is a skinning/weight fault on the lower tunic.
- The back surface shimmers and looks fuzzy. That is either TAA noise on a high-frequency normal or a broken normal map.
- The back neck is now a high crew, which is right.

## Round-1 items

1. Sleeves short and flared: **fixed** (the cuff stitching is still missing, see fix 5).
2. Hem flare and pleat: **improved**. The flare is gone, but a hip seam still splits off a skirt panel (fix 2).
3. Flat cream, near-default cloth: **improved**. The colour is right, but there is still no linen weave, seams or stitching (fix 3).
4. Neckline: **improved**. The back is fixed. The front V is present, but the neck opening is too wide and has no lacing (fix 4).
5. Dark hip flap: **still open**. It now shows as a crumple across the buttocks, with the hem wrapping the thigh in the walk (fix 1).
6. Leggings too light and thin: **improved**. The colour is fixed. They are still skin-tight with no weave, which is acceptable for the start state (fix 6).
7. Boxy shoulders: **fixed**.
8. Missing front and 3/4 evidence: **fixed**. The figure fills only about 600 px of each frame, which is too small to judge stitching (fix 7).

## Fix list

1. In the walk, the lower tunic crumples dark across the buttocks and wraps the right thigh. ART: re-weight the tunic below the waist. Blend the hem verts 50/50 between pelvis and thigh_l/r, with no single-thigh dominance. Make the back panel follow the pelvis, and check frames across the full walk cycle from behind. Engineer
2. A hip seam splits the tunic into a top plus a skirt, so it reads as a dress. ART: remove the horizontal seam and the separate lower panel. Make the tunic one continuous panel with matching UVs and albedo from shoulder to mid-thigh, keeping the side slits. Engineer
3. The cloth looks like a smooth knit or felt, not coarse linen, and the back shimmers. ART: replace the tunic normal and albedo with a tiling linen weave sampled from the concept swatch (texel density around 10-12 repeats per 10 cm, normal strength about 0.6, roughness 0.85). Paint shoulder and side seams in. Confirm the shimmer is gone under TAA. Codex
4. The neckline is too wide and the V has no lacing. ART: close the neck opening to sit at the base of the neck, front and sides. Keep the short V and add the cross-stitch lacing and stitched neck trim from the concept crop (normal plus albedo). Codex
5. The cuffs are plain rolls. ART: add the stitched band on the rolled cuff, per the concept sleeve crop. Codex
6. The leggings read as tights. ART: add a wool weave normal and slight fold detail at the knees and ankles, and loosen the fit by about 3-5 mm off the skin. Codex
7. The captures are too small to judge stitching and trim. LEVEL: add one close idle frame (upper body filling at least 900 px tall, front view) to the evidence set. Engineer
