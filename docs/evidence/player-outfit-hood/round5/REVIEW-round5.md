# GEAR-02 hood: independent review, round 5 of 5 (final)

Reviewer: a fresh Opus agent that did not build the hood, working from docs/visual-review.md. It judged the five compare images, the plain captures (walk_back at full size), the raw, cleaned and section clay renders, the six stage strips and checks.txt. The review was run on 2026-10-25.

**Verdict: FAIL.** The cape is longer and the shoulders slope, but it is now a straight-sided tube with a flat, sliced hem rather than the concept's flared bell. The walk_back view also shows two new defects: a hole in the back panel, and a brown patch on the back of the left thigh.

## Findings, most severe first

1. **There is a hole in the back panel, and the lower corners are torn (new).** In walk_back, a dark tear sits left of centre on the back panel, and both lower corners break into ragged, see-through fringes. checks.txt agrees: the inner faces have six boundary loops, with sizes 374, 173, 121, 90, 85 and 39. A hood should have only three (face, neck and hem), so 90, 85 and 39 are holes. It also reports 5 boundary edges and 22 non-manifold edges on the low mesh. Fix (ART, Engineer): before decimation, fill every boundary loop apart from the face, neck and hem loops. After decimation, reject the export if there are more than 3 boundary loops or any non-manifold edges.
2. **A brown patch shows on the back of the left thigh when walking (new).** In walk_back there is a woven, orange-brown patch on the back of the upper left thigh. It is most likely stray hood triangles, or hem verts that the thigh_l or pelvis bone is moving. Fix (ART/CODE, Engineer): find the faces that render there (isolate the hood material in game). Delete any detached islands, and give every hood vertex zero weight on thigh, calf and pelvis bones, keeping spine_03–05, clavicles, neck and head only.
3. **"Level hem" turned the bell into a tube.** The stage strips (back and left) show stage 1b stretching the cape straight down. The flare from the raw model is gone, and later stages never bring it back. In game the cape hangs as a narrow, straight-sided poncho with a flat hem, not the concept's wide bell, which flares out about 1.6 times the shoulder width at the hem. Fix (ART, Engineer): lengthen the cape by scaling each column outwards as it goes down (hem radius about 1.4–1.6 times the shoulder radius, and no longer 1.03–1.35). Don't stretch it straight down.
4. **The hem is a hard, sliced edge.** The cleaned clay shows a flat, horizontal band with a crusty, uneven bottom lip, and the raw model's rolled hem is gone. The front and side views in game show the same flat cut. Fix (ART, Engineer): rebuild the rolled hem band (about 1.5 cm) along the final boundary loop. Let it rise and fall by ±2–3 cm in 8–10 soft waves, as the concept's does.
5. **The front still stands off the chest.** The side view shows the front panel tented forward with a wide gap, and checks.txt gives 7.4 cm at z126 against a 3–5 cm target. Fix (ART, Engineer): pull the front columns in to 3–5 cm below z140.
6. **The neck band is gone.** The raw model has the seam ridge at the join, but the cleaned clay and the in-game captures show none. Fix (ART, Engineer): protect the raw neck-band verts from the drape and push stages, or put the ridge back afterwards.
7. **Specks remain.** The cleaned clay still shows small dark specks on the front-left and the back. Fix (ART, Engineer): delete degenerate faces and recompute normals before the bake.

## Round-4 items

| Item | Status |
|---|---|
| 1. Cape too short, front tents | Partly resolved (lowest hem 121.6 cm; the front still has a 7.4 cm gap) |
| 2. Ragged hem | Not resolved (the notches are gone, but the hem is now sliced flat and torn at the corners in walk_back) |
| 3. Sides split and curl when walking | Not resolved (the hole and torn corners in walk_back) |
| 4. Clean-up made a wide flat yoke | Partly resolved (the shoulders slope, but the bell is now a tube) |
| 5. Boxy poncho front | Partly resolved (shallower folds, still a straight poncho) |
| 6. Neck band not visible | Not resolved (the ridge is gone from the cleaned mesh) |
| 7. Clay specks and dents | Partly resolved (inner surface smoother, specks remain) |
| 8. Felt or suede texture | Partly resolved (the weave has more grain but reads as fur or nap at play distance) |

## For the owner

Worth keeping: the hood itself, meaning the crown shape, the face opening, no hair poke-through, the face visible from the front, and clearance passing in all three poses. The colour is right, and the texture is close. What blocks it is the method. Each round, the scripted deform pipeline trades one defect for another: lobes, then a yoke, then a tube. That happens because it is pushing a generated mesh that was never draped on this body. A rigid, single-mesh deform cannot produce a bell that flares, clears the arms and stays closed in a walk cycle. I'd recommend one of two routes. The first is to model the cape in Blender (or Marvelous) as a clean, quad circle-cape draped on the MetaHuman body, and keep the generated hood only for the head part. The second is to keep the shape and add real cloth simulation (Chaos Cloth) on the lower cape. Without one of these, I don't expect a further scripted round to pass.
