# Silverbrook water / swim / bridges / trees / hay: independent review, round 3

Reviewer: a fresh Opus art director who did not build this work. Evidence: `Artifacts/Evidence/silverbrook-water/compare/*` (before on the left, after on the right) and `after-swim-side.png`. Coordinates are at 2560 width unless noted.

Result: 6 PASS, 8 FAIL.

## bridge-end-side-a — PASS
The ramp reads as paving and the shoreline is a thin, broken foam line. Minor: the ramp is still a shade paler than the road.

## bridge-end-side-b — FAIL
The ramp is still paler than the cobbles and ends in a hard, straight cut against the road (about 1290 to 1690, 345 at 2000 width). It reads as a laid-on slab. The shallows are fixed: they are now sandy, not mint.

## bridge-end-main-a — PASS
The white box is gone, and the shallows now read as a sandy margin. Minor: the band on the left bank is still constant in width and meets the deep blue with a crisp edge.

## bridge-end-main-b — PASS
The box is gone and the shoreline is plausible. The bales at the left are still bright yellow blocks; see hay-bales.

## river-level-main — FAIL
Unchanged from round 2. The waterline steps in straight segments at the pier bases, and pale hard lines run along the strip edges under the left and centre arches. The water seen through the centre arch is a separate, lighter flat plane.

## river-level-side — FAIL
A constant-width pale band still runs along both banks and ends at a white edge line, and a pale collar still rings the pier. The far water under the arches is a lighter, flat strip with a hard horizontal edge.

## under-water — FAIL
The far seabed under the arches shows the same hard horizontal seam as under-surface-look: a bright cyan band with a straight cut, and no fog falloff. The near pier faces are flat black boxes. In round 2 this view passed only on the overall read; with the seam this visible, it fails.

## under-surface-look — FAIL
Unchanged. A hard, full-width horizontal seam with a dark blue streak (about y 320 at 2000 width) separates the surface from the seabed. The seabed does not fade with distance.

## swim-side — FAIL
This is the worst defect. The submerged body still renders as a pale, glowing green-white slab with a hard rectangular outline that sits on top of the water. It does not read as clothing seen through water. The head, the shoulders and the upper back are still well above the surface, and the lead arm is held at face height with the palm out, like a paddle or a wave, not a crawl stroke. The 76 deg lean does not read: the spine still rises from the hips to the head.

## swim-front — FAIL
The same pale slab sits under the swimmer. The pose reads as a backstroke or a sunbather: the head is tipped back and an arm loops over the top.

## tree-node — PASS
The pines match their neighbours. Out of scope: the SM_Oak_01 blobs.

## hay-bales — FAIL
The bales are darker on the shaded side only. The sunlit faces are still saturated egg-yolk yellow, the sides are smooth vertical stripes, the edges are pillow-rounded, and no straw breaks the silhouette. They are worse than the noisy "before" bales for straw read: the before bales at least had protruding stalks.

## main-street — PASS
The bales at the right are now a pale straw colour at street distance and no longer dominate the street. Out of scope: the near-black house fronts in shade.

## house-frontage — PASS
No regression. Out of scope: the shaded timber is dark.

## Fix list
1. The submerged swimmer is a pale, opaque slab with a hard outline (swim-side, swim-front). CODE/ART: the body below the surface must be tinted and darkened by the water. Make the water material's refraction and depth absorption apply to the character: render the character before the translucency and keep the water's depth-based absorption, or add an underwater tint in the character material below the water height. Remove whatever flat card or volume produces the rectangular outline.
2. The swim pose still has the head and shoulders out and does not read as a crawl. CODE+ART: lower the capsule or mesh so only the back of the head and the top of the shoulders break the surface, keep the spine level from head to heels, and drive a real alternating front-crawl cycle in which one arm enters ahead of the head while the other pulls under. Check it from the swim-side camera.
3. The underwater horizon has a hard seam (under-surface-look, under-water). CODE: replace the hard cut with exponential underwater fog (UE water post-process volume or height fog inside the water body) so the surface and the seabed fade into murk with distance. Find and remove the plane or post-process edge that makes the dark streak.
4. The hay is saturated, smooth and rounded with no loose straw. ART: set the sunlit albedo to about sRGB 185,155,95 and verify it in the sunlit capture. Square the edges. Add alpha straw-end cards along every edge and on the cut faces at the reference density, and replace the vertical stripe tile with compressed-flake straw.
5. Water strips step under the bridges (river-level-main, river-level-side). LEVEL: use one continuous water body, or snap the strip heights and edges to match under the arches, so a single surface meets the piers. Remove the pale edge lines.
6. The shallow band is a constant-width ribbon with a white edge line, and the pier has a pale collar (river-level-side, main-a). ART: drive the shallow colour and the foam from real depth or the distance field, not a fixed shore offset, so the band width varies. Halve the pier collar.
7. The ramp on side-b is paler than the road and ends in a hard straight edge. ART: match its tint to the road cobbles and blend the road edge with a decal or a vertex-paint blend.
8. Out of scope (owner note): replace the SM_Oak_01 blob trees.
9. Out of scope (owner note): the near-black house fronts in shade need a skylight or GI fill.
