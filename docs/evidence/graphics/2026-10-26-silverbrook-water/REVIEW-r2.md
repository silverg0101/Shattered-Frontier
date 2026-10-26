# Silverbrook water / swim / bridges / trees / hay: independent review, round 2

Reviewer: a fresh Opus art director who did not build this work. Evidence: `Artifacts/Evidence/silverbrook-water/compare/*` (before on the left, after on the right) and the `after-*.png` full-resolution captures. Water has no concept, so it is judged against shipped-quality realism. The hay is judged against `PropHayBales-reference.png`.

Result: 4 PASS, 10 FAIL.

## bridge-end-side-a — PASS
The placeholder patch is gone and the ramp reads as paving. The shore edge is now a thin broken line rather than a band. Minor: the ramp is slightly paler than the deck.

## bridge-end-side-b — FAIL
The ramp is still visibly paler and flatter than the deck. It meets the cobbles with a hard, straight, unblended edge, so it reads as a pasted-on slab. The shallows are a bright mint-green strip.

## bridge-end-main-a — FAIL
The floating white foam box from round 1 is still beside the bridge (after frame, about 1730,90 at 2560 width). The shallows read as a wide, pale mint band along the whole bank, like a tropical beach, not a town river.

## bridge-end-main-b — FAIL
The same white foam box sits in the water right of the bridge (about 2235,315 at 2560 width). Its hard rectangular outline shows clearly. The shore band is the same pale mint strip.

## river-level-main — FAIL
The colour is better: it is now blue rather than pool cyan. The waterline under the arches still steps in straight segments with a pale hard line at the pier bases (left and centre arch). It reads as water-strip edges, not a contact line.

## river-level-side — FAIL
The foam is thinner, but a continuous pale-mint shallow band of constant width still runs the length of both banks, and a pale collar rings the pier. Under the arches the far water sits as a separate flat, lighter strip with a hard horizontal edge (about y 690 to 720 in after-river-level-side). That is a strip seam or a height step.

## under-water — PASS
Reads as underwater. The far arch is still too crisp, with no fog falloff (minor).

## under-surface-look — FAIL
Unchanged from round 1. A hard, straight horizontal seam with a dark blue streak crosses the full frame where the surface meets the murk. The seabed does not fade with distance.

## swim-side — FAIL
The pose is unchanged in effect. The torso is pitched about 45 deg back, with the head, shoulders and chest well out of the water and the arm straight up. The body arches backward like a breaching dolphin, not a level front crawl. The submerged body is still a pale, opaque, flat slab with a hard outline.

## swim-front — FAIL
Same upright arched pose and pale slab. A dark streak or slab still lies on the surface to the left of the swimmer (about 1750 to 1870, 330 at 2560 width).

## tree-node — PASS
The node shows a pine that matches its neighbours. Out of scope: the low-poly SM_Oak_01 blobs are still conspicuous.

## hay-bales — FAIL
Against the reference, the bales are still saturated egg-yolk yellow; the reported desaturation does not read in the capture. The sides are smooth with vertical stripe tiling. The edges are pillow-rounded, and no loose straw ends break the silhouette on the edges or the cut faces. The reference shows ragged edges and a lot of protruding straw. The twine is now visible (fixed).

## main-street — FAIL
The new bales at the right read as bright yellow blocks, the most saturated object in the street. Out of scope: the right-hand house fronts are near-black in shade.

## house-frontage — PASS
No regression. Out of scope: the shaded timber and window bay are dark.

## Fix list
1. The swim pose is still upright and arched, not a crawl. CODE+ART: pitch the mesh or capsule so the spine is level at the waterline (head at the surface, chest below it, legs trailing just under the surface). Play a front-crawl cycle with alternating arms and no back-arch, and verify from the swim-side camera.
2. The submerged swimmer renders as a pale opaque slab, and a dark slab sits on the surface beside the swimmer. CODE/ART: render the body correctly behind the translucent water (fix the translucency sort priority or the depth test, or apply underwater absorption to the body), and find and remove the dark surface plane, shadow or decal.
3. The floating white foam box is still beside the main bridge (main-a, main-b). LEVEL: find the actor at that spot (a foam card, a submerged box or a collision mesh with water foam) and delete it, sink it, or mask foam off it.
4. The hay is still saturated and smooth with no loose straw. ART: shift the albedo to about sRGB 185,155,95 and check it in the sunlit capture. Add straw-end cards or alpha fringe on every edge and on the cut faces at the reference density, break the silhouette, square the edges, and remove the vertical stripe tile.
5. The underwater horizon has a hard seam. CODE/ART: replace the cut with exponential underwater fog and scattering so the surface and the seabed fade into the murk, and remove the visible post-process or plane edge.
6. Water strips step under the bridges (river-level-main, river-level-side). LEVEL: merge the strips, or align their heights and edges under the arches, so one continuous surface meets the piers. Remove the pale contact lines at the strip edges.
7. The shallow band is a wide, uniform pale mint strip on every bank. ART: shrink the shallow-colour width, shift it to a muted green-brown, and vary it with depth so it is not a constant-width ribbon. Lower the pier collar.
8. The ramp is paler than the deck, with a hard edge against the road (side-b). ART: match its tint and tile scale to the deck stones, and blend the road edge with a decal or a vertex blend.
9. Out of scope (owner note): replace the SM_Oak_01 blob trees with realistic foliage.
10. Out of scope (owner note): the near-black house fronts in shade need a skylight or GI fill.
