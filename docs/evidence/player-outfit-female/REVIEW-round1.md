# Female player start outfit: review, round 1

Concepts: `docs/evidence/player-outfit/concept-female/female_starter_turnaround.png`, `female_starter_closeup.png`
Live pairs: `Hilda_front-compare.png`, `Hilda_three_quarter-compare.png`, `Hilda_side-compare.png` (idle), `Hilda_walk_back-compare.png` (walking)
Previous open list: none (male round 3 used for context only).
Out of scope: face, hair, lighting, background, idle pose.

## Hilda_front — FAIL

- The neckline is a deep plunging V down to the sternum. The concept has a high round neck at the base of the throat with a short slit V. A grey patch shows inside the V (an undergarment or body layer showing through). That is breakage.
- The tunic flares into an A-line skirt from the hip, and the hem is uneven (lower on her left). It reads as a dress. The concept is a straight, loose tunic with side slits and a level hem.
- There is a lighter shading band across the hips: the same hip seam/panel break as the male version.
- The cloth is a smooth, fuzzy felt with no linen weave. The colour is saturated yellow-tan; the concept is a grey-beige oatmeal linen.
- The bodice clings tight over the bust with strong bust shading. The concept hangs loosely from the shoulders.
- The rolled cuffs are puffy, balloon-like bulbs, not flat folded bands.

## Hilda_three_quarter — FAIL

- Same deep V with grey showing through, same flared and uneven skirt hem, and same felt cloth and yellow colour.
- The concept's side slit, running from hem to hip, is not visible.

## Hilda_side — FAIL

- The side slit renders as a thin black vertical slot near the hem. It doesn't read as a cloth opening with the legging visible behind it.
- The skirt bells out behind the seat. The concept drops straight to mid-thigh.
- The sleeve length is right.

## Hilda_walk_back — FAIL

- The lower tunic wraps the buttocks and pulls into the gap between the thighs, like shorts, instead of hanging as one back panel. This is the same skinning fault as the male version's walk.
- The whole back surface is speckled and fuzzy (shared noise/TAA shimmer issue).
- On the lifted leg, the legging edge at the back of the knee and shin is fuzzy and crumpled.

## Shared with the male outfit (not re-litigated)

The hip seam/panel, felt cloth with no weave, speckle under TAA, walk-cycle thigh wrap, plain cuffs, and leggings that read as tights are all present here too.

## Fix list

1. The deep V neckline with grey showing through. ART: rebuild the neck opening as a high round neck at the base of the throat, with a short slit V (about 6-8 cm) and a stitched trim. Hide or delete whatever grey body or undergarment layer shows through in the chest region. Check the front and three-quarter views. Engineer
2. The flared A-line skirt and uneven hem make it read as a dress. ART: reshape the lower tunic to hang straight from the hips to mid-thigh with a level hem. Remove the flare and the hip panel/seam (one continuous panel, matching albedo). Engineer
3. The walk-cycle wrap between the thighs and over the buttocks. ART: weight the front and back hem panels mainly to the pelvis, and blend with thigh_l/r only near the side slits. Check every walk frame from behind. Engineer
4. The side slits are missing or render as a black slot. ART: cut real side slits from the hem to the upper thigh, with backfaces and a hem edge, so the legging shows through. Remove the black slot strip. Engineer
5. The felt cloth and yellow colour. ART: replace the tunic albedo with grey-beige oatmeal linen matched to the concept close-up. Add a tiling linen weave normal (strength about 0.6, roughness about 0.85, mips on). Confirm in-engine that the back speckle and shimmer are gone. Codex
6. The tight bust fit. ART: loosen the bodice so it drapes from the shoulders and bust apex without hugging the underbust, and soften the bust shading. Engineer
7. The balloon cuffs. ART: flatten the rolled cuffs into a folded band about 4 cm deep that sits close to the forearm, with a stitched edge. Codex
8. The leggings read as smooth tights. ART: add a brown wool knit normal and albedo variation, light folds at the knees and ankles, and a 3-5 mm offset off the skin. Fix the fuzzy, crumpled edge behind the knee in the walk. Codex
