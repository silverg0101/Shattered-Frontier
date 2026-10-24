# Female player start outfit: review, round 2

Concepts: `docs/evidence/player-outfit/concept-female/female_starter_turnaround.png`, `female_starter_closeup.png`
Live pairs: `r2-front-compare.png`, `r2-three_quarter-compare.png`, `r2-side-compare.png` (idle), `r2-walk_back-compare.png` (walking)
Previous open list: `REVIEW-round1.md`.
Out of scope: face, hair, lighting, background, idle pose. Trim/stitch texture not applied (no UV island for bands); judged as is.

## r2-front — FAIL

- The tunic still flares into an A-line skirt from just under the bust; the hem is wider than the shoulders and it reads as a dress. Concept: straight, loose tunic hanging from the shoulders, sides near-vertical to mid-thigh.
- The bodice is still fitted: a defined underbust/waist taper and strong bust shading (empire-waist look). Concept hangs loose with no waist.
- Neckline is now a round neck with a short slit (fixed shape), but a small black void shows at the bottom of the slit at the sternum. The neck opening is also a little wide and scooped compared with the concept's close base-of-throat neck.
- Colour now matches the oatmeal grey-beige. Cloth is still smooth felt with no linen weave.
- Sleeve ends are plain open tubes that flare slightly; no folded cuff band.
- Hip seam band is gone.

## r2-three_quarter — FAIL

- Same flared skirt, fitted bodice and black void at the bottom of the slit.
- Side slit not visible from this angle (it is in the concept); hem is level.

## r2-side — PASS (with notes)

- Silhouette is a straight tunic to mid-thigh and the side slit now reads as a cloth opening with the legging behind it. Recognisable as the concept.
- Notes: the slit edge has a thin black line along its back edge (backface/gap). Bust is still more shaped than the concept.

## r2-walk_back — FAIL

- The lower back panel splits up the centre into two flaps that follow each thigh, opening a vertical slit up the back. The concept's back is one uninterrupted panel. No longer wraps like shorts (improved), but the split is new breakage.
- The back panel clings to the seat and waist (fitted, waist taper visible). Concept hangs straight from the shoulder blades.
- Speckle is much reduced; silhouette edges on the right side are still serrated/dithered.
- Leggings are smooth, flat tights; the crumpled knee edge from round 1 is not visible.

## Round 1 items

1. Deep V neckline: improved (short slit now; black void at slit base remains).
2. Flared A-line / uneven hem / hip seam: hem level and seam gone; flare still open.
3. Walk wrap between thighs: improved, now a centre-back split instead.
4. Side slits: fixed in side view (minor black edge line).
5. Felt cloth and yellow colour: colour fixed; weave still open.
6. Tight bust fit: still open.
7. Balloon cuffs: balloon fixed; no folded cuff band (improved).
8. Leggings read as tights: crumpled edge fixed; knit still open.

## Fix list

1. Flared A-line skirt reads as a dress (front, three-quarter). ART: straighten the tunic sides so they drop near-vertically from the armpit to the mid-thigh hem; hem width about equal to hip width plus 4-6 cm ease, not wider than the shoulders. Engineer
2. Fitted bodice and waist taper (front, three-quarter, walk back). ART: remove the underbust and waist suppression front and back; let the cloth bridge from bust apex and shoulder blades straight down. Soften bust shading. Engineer
3. Centre-back split during walk (walk back). ART: weld the back hem into one panel; weight the centre-back hem verts to pelvis (blend to thigh_l/r only within about 8 cm of each side slit). Check every walk frame from behind. Engineer
4. Black void at the bottom of the neck slit (front, three-quarter). ART: close the slit base (cap it with backface or fill with tunic cloth) and hide the body under it; pull the neck opening in to sit at the base of the throat. Engineer
5. No linen weave; felt look (all views). ART: add a tiling linen weave normal (strength about 0.6, roughness about 0.85) and slight albedo slub variation on the existing UVs. Codex
6. Leggings read as smooth tights (all views). ART: add a brown wool knit normal and albedo variation. Codex
7. Sleeve ends have no cuff (front, three-quarter). ART: add a modelled folded band about 4 cm deep at the sleeve end, close to the forearm, no flare. Codex
8. Thin black line on the side slit edge (side). ART: give the slit edges backfaces/thickness so no gap renders. Engineer
