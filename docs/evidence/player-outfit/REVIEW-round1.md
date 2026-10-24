# Player start outfit: review, round 1

Concept: `GameClient/ArtSource/Characters/Player/reference-starter-male.webp`
Live: `docs/evidence/player-outfit/outfit-compare.png` (walking from behind, mid-stride, idle)
Out of scope: belt, pouch, wrist wraps and boots.

## Outfit — FAIL

A player sees a beige top and brown legs, but the cut is wrong in ways they would notice. As built, it reads as a short smock dress over tights, not the concept's linen tunic and trousers.

- **Sleeves:** short, flared cap sleeves stop at mid upper arm. The concept has 3/4 sleeves rolled to just below the elbow, with a stitched cuff. This is the biggest silhouette miss.
- **Hem:** the lower tunic flares out like a skirt and bunches into a pleated band around the hips in all three captures. The concept hangs straight to mid-thigh, with side slits. The live hem is also shorter: it ends at the buttocks.
- **Hip notch:** there is a dark crumpled flap on the right hip in the walk and idle captures. It reads as a mesh fold or tear, not a clean side slit.
- **Neckline (back):** a wide scoop exposes the upper back and the base of the neck. The concept's back neck is a high crew with stitched trim. The front V-neck with cross-stitching can't be judged because no front or 3/4 capture was supplied.
- **Cloth look and colour:** a flat pale cream with a smooth, felt-like surface. It has no linen weave, no seams and no neckline stitching. The concept is a warmer oatmeal/greige with visible coarse weave. Up close, it reads as close to a default material.
- **Leggings:** painted-on tights in a lighter olive-brown. The concept has straight-cut dark brown-grey wool trousers with visible volume and a weave texture. Leggings are acceptable for the start state, but the colour and the zero-volume look need to move toward the concept.
- **Fit and scale:** body scale is fine. I saw no skin poke-through and no floating. The shoulders sit boxy and square, away from the body.
- **Evidence:** the mid-stride capture is soft and noisy (the hem shimmers under TAA). There is no front view.

## Fix list

1. Sleeves are short and flared instead of rolled 3/4. ART: lengthen the sleeves to just below the elbow, taper them to the forearm, and add a rolled cuff band with stitching (concept detail crop). Codex
2. The hem flares and pleats like a skirt. ART: rebuild the lower tunic as a straight drop to mid-thigh with side slits, and remove the hip pleat and the flare. Check it in the walk cycle so the hem doesn't kick out. Engineer
3. The cloth reads as flat cream, near default. ART: author a linen material (coarse weave normal plus albedo with the concept swatch's warm oatmeal tint, roughness about 0.85), with seam and neckline stitch detail in the normal/albedo. Codex
4. The back neckline scoops too wide. ART: raise it to a high crew that sits at the base of the neck, add stitched trim, and cut the front as a short V with cross-stitching per the concept. Codex
5. The dark crumpled flap on the right hip. ART: clean up the geometry, or bind the side-slit weights so it opens cleanly, with no dark fold. Engineer
6. Leggings are too light and too thin. ART: darken them to the concept's brown-grey wool, add a weave normal, and loosen the fit slightly so they read as cloth, not tights. Codex
7. Boxy shoulders. ART: drop the shoulder seam to the deltoid, and pull the shoulder volume in to follow the body. Engineer
8. Evidence is incomplete. LEVEL: add a front and a 3/4 idle capture at 1920x1080 with a sharp frame (pause the animation or turn TAA jitter off for the shot) so the neckline and front can be judged. Engineer
