# Player start outfit: review, round 3 (final before owner)

Concept: `GameClient/ArtSource/Characters/Player/reference-starter-male.webp`
Live: `front-compare.png`, `three_quarter-compare.png`, `side-compare.png` (idle), `walk_back-compare.png` (walking), `front_close-compare.png` (close front)
Out of scope: belt, pouch, wrist wraps, boots, face, hair.

## front — FAIL

- The hip seam is still there. A lighter, slightly flared lower panel starts at hip level, so the tunic still reads as a top over a skirt, or a dress over tights.
- The neckline is still a wide scoop that shows the collarbones. The short V is present, but there is no lacing and no stitched trim.
- The cloth is a smooth, fuzzy felt with no linen weave and no seams. The sleeve length and roll are right.

## three_quarter — FAIL

- The hip seam and lower panel are the dominant read: a clear horizontal break with a colour step across the whole hip.
- The neckline and cloth have the same faults as the front. The cuffs are plain rolls.

## side — PASS

- The tunic drops straight to mid-thigh, and the side slit reads. The sleeve length matches. The seam is hidden at this angle. Nothing is broken, though the weave is still missing.

## walk_back — FAIL

- The lower tunic still wraps each thigh like shorts, pulling into the gap between the legs instead of hanging as one panel. There is a dark, crumpled, noisy patch across the buttocks and the back of the thighs. This is the same skinning fault as round 2.
- The whole back surface is speckled and fuzzy (high-frequency normal or noise under TAA).

## front_close — FAIL

- At close range the neck opening sits well below the base of the neck, with collarbones and upper chest exposed. The concept's close round neck has a heavy whip-stitched trim and a laced V, and none of that is present.
- The chest shows grainy speckle noise, not a woven texture. The cuff has no stitched band.

## Round-2 items

1. Walk crumple and thigh wrap: **still open** (fix 1).
2. Hip seam and skirt panel: **still open** (fix 2).
3. Felt-like cloth and back shimmer: **still open**. The surface is noisy and has no weave (fix 3).
4. Wide neckline, no lacing: **still open** (fix 4).
5. Plain cuffs: **still open** (fix 5).
6. Leggings read as tights: **still open**, acceptable for the start state (fix 6).
7. Close capture: **fixed**.

## Fix list

1. In the walk, the lower tunic wraps each thigh like shorts, with a dark crumple across the buttocks. ART: re-skin the tunic below the waist. Weight the back and front hem panels mainly to the pelvis, blended 50/50 with thigh_l/r only at the side slits, and never let a single thigh dominate the centre front or back. Check every frame of the walk cycle from behind. Engineer
2. A horizontal hip seam with a lighter, flared lower panel makes the tunic read as a dress. ART: delete the separate lower panel and its seam, and rebuild the tunic as one continuous panel from shoulder to mid-thigh with one UV island per side and matching albedo. Keep the side slits. Engineer
3. The cloth is fuzzy felt with speckle noise and no linen weave. ART: replace the tunic albedo and normal with a tiling plain linen weave matched to the concept swatch (about 10-12 repeats per 10 cm, normal strength 0.6, roughness 0.85, mips enabled). Add painted shoulder and side seams. Confirm in-engine that the speckle and shimmer are gone under TAA. Codex
4. The neck opening is too wide and low, and has no trim or lacing. ART: raise and narrow the neck opening to sit at the base of the neck all round. Keep the short V, and add the concept's whip-stitched neck trim and cross-stitch V lacing (geometry or normal plus albedo). Codex
5. The cuffs are plain rolls. ART: add the concept's stitched band on the rolled cuff (normal plus albedo). Codex
6. The leggings read as painted tights. ART: add a wool weave normal, light folds at the knees and ankles, and an offset of 3-5 mm off the skin. Codex
