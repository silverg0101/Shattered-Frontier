# GEAR-02 hood: independent review, round 4 of 5

Reviewer: a fresh Opus agent that did not build the hood, working from docs/visual-review.md. It judged the five compare images, the plain captures, the raw, cleaned and section clay renders and the stage strips. The review was run on 2026-10-25.

**Verdict: FAIL.** The hair poke-through is fixed, and the weave no longer reads as stripes. The cape is still not the concept's bell. It is a short, stiff yoke that ends at chest height and has a ragged hem.

## Findings, most severe first

1. **The cape is too short and stands off the body like a board.** In every view, the hem stops at armpit or shoulder-blade height (about 134–137 cm), so the chest and midriff show. In the side view, the front panel juts forward with a large gap to the chest. The concept's bell falls to the upper arm and chest, and it follows the shoulders. Fix (ART, Engineer): lengthen the cape about 12–15 cm, aiming for a lowest hem of about 120–124 cm. Then cut the forward and outward push on the front panel, so that it hangs 3–5 cm off the chest instead of tenting.
2. **The hem is still ragged.** The back and walk_back views show a notched, lumpy, scalloped edge with a V-notch at centre back. The front view has a dark notch or hole at the lower right of the hem. Fix (ART, Engineer): rebuild the hem as one smooth boundary loop with even, gentle waves. Close the notches, and keep a rolled hem band all the way round, as in the concept.
3. **The sides split open and curl when walking.** In walk_back, the back panel lifts away from the sides, the arms show through the opening, and the edge flips. Fix (ART/CODE, Engineer): make the cape one continuous skirt with no slit at the sides. Weight the side and back hem verts to the clavicle and spine_05 bones, and not to the upper arms, so arm swing can't tear them open.
4. **Clean-up turned the bell into a wide, flat yoke.** The stage strip (back) shows the silhouette getting much wider and squarer after stage 2. The puffed lobes are gone, but the narrow raw bell has gone with them. Fix (ART, Engineer): cap the outward push at the shoulder at about 40 mm, or blend it towards the stage-2 silhouette, so that the shoulder line slopes like the raw v4 model.
5. **The front still hangs like a boxy poncho.** In the three-quarter view, deep vertical box folds stand out over the upper arms. Fix (ART, Engineer): after fixes 1 and 4, smooth out the two biggest front folds. Aim for the concept's soft, evenly spaced flutes.
6. **The neck band can't be seen in game.** There is no seam or band where the hood meets the cape in the front or three-quarter views, and the face opening reads as a plain dark hole. Fix (ART, Codex): add a raised neck-band ridge in the geometry and a seam line in the base colour and normal map at the hood-to-cape join. Lift the colour of the inner face-opening rim.
7. **The cleaned clay still has defects.** The bottom and underside views show small dark specks and dents. The section view shows a bumpy inner cape surface. Fix (ART, Engineer): remove the specks (stray or flipped faces), and run one more local smoothing pass on the inner cape faces only.
8. **The texture reads as felt or suede.** The stripes are gone, but the concept's fine twill or slub weave can't be seen at play distance. Fix (ART, Codex): raise the contrast of a fine weave of about 4–6 px a little in the normal and roughness maps. Don't go back to the 24 px stripes.

## Round-3 items

| Item | Status |
|---|---|
| 1. Hair pokes through the hood crown | Resolved |
| 2. Weave reads as stripes | Resolved (but now too flat; see 8) |
| 3. Clean-up puffed the shoulders | Partly resolved (lobes gone, now a wide flat yoke) |
| 4. Back hem short and ragged; sides open when walking | Not resolved |
| 5. Front hangs like a boxy poncho | Not resolved |
| 6. Neck band not visible in game | Not resolved |
| 7. Cleaned clay surface lumpy | Partly resolved |
