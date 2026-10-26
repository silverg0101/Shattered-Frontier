# Silverbrook water / swim / bridges / trees / hay — independent review, round 1

Reviewer: fresh Opus art director (did not build). Evidence: `Artifacts/Evidence/silverbrook-water/compare/*` (before left, after right). No concept for water/swim; judged against shipped-quality realism.

Result: 3 PASS, 12 FAIL.

## bridge-end-side-a — FAIL
Yellow-green placeholder patch is gone; ramp now reads as stone. But the shoreline foam is a constant-width, fully opaque white band along the whole bank, like a painted surf line on a beach. A slow town river has no breaking surf; this is the most visible new defect.

## bridge-end-side-b — FAIL
Placeholder patch is fixed: the ramp is paved. It is paler and flatter than the deck and the road, with a hard straight edge where it meets the cobbles. The same opaque white foam band runs along both banks.

## bridge-end-main-a — FAIL
A floating white rectangular foam block sits in the river beside the bridge, upstream of the pier (top-left of the after frame, around 650,60 to 730,120 at 1920x1080). It reads as a broken box. The shore foam is again a uniform white strip. The water is a single flat teal with no depth gradient from bank to channel.

## bridge-top-side-a — FAIL (evidence)
The after frame matches the before frame: cobbles and grass, with no bridge, water or bank join in shot. The capture cannot show that the fix worked. Re-aim the camera.

## bridge-top-main-a — FAIL (evidence)
A ground close-up with no bridge or water in frame, identical to the before frame. It proves nothing. Re-aim the camera.

## river-level-main — FAIL
The water now has a darker colour and visible ripples. Under the arches the waterline steps in blocky straight segments with hard foam lines (stepped edges at the pier bases), which reads as strip seams or intersection artefacts, not a natural contact line. The colour is still a saturated pool cyan.

## river-level-side — FAIL
A thick opaque white foam band runs the full length of both banks and around the pier at constant width. It reads as a swimming-pool edge or snow, not a river margin. The water colour does not darken toward the channel centre.

## under-water — PASS
Reads as underwater: blue-green tint, the surface visible from below with ripples, and the arch and bank silhouettes behind. Minor: there is no distance fog falloff, so the far arch stays crisp.

## under-surface-look — FAIL
Underwater tint and the underside of the surface read correctly. A hard, perfectly straight horizontal seam with a dark blue streak crosses the whole frame where the surface meets the far murk, and above and below it are two flat colour fields. It looks like the edge of a post-process volume or plane rather than a fading horizon. The seabed also does not fade with distance.

## swim-side — FAIL
The pose is not swimming. The torso is upright at about 45 deg with the head and shoulders far out of the water and one arm raised straight up: this reads as waving or drowning, not a front crawl. The submerged body shows as a pale, ghostly white slab through the water, and a dark rectangular shadow or decal sits on the surface to the left of the body. The character is an untextured pale body (placeholder look).

## swim-front — FAIL
Same as swim-side: upright torso, arm vertical, body not level at the surface, a white translucent slab under the water and a dark slab on the surface. Also conspicuous: the low-poly lime-green blob oaks (SM_Oak_01 dressing) on the far bank clash with the realistic pines.

## tree-node — PASS
The node now shows a pine trunk matching the surrounding pines; the blobby harvest oak is gone. Note (out of scope): the bright low-poly SM_Oak_01 blobs in the midground are conspicuous and break the realistic forest.

## hay-bales — FAIL
Better than the cubes with stiff straw cards, but not real small square bales when set beside the reference. The colour is saturated egg-yolk yellow; the reference is a muted tan-gold. The shapes are soft and pillow-rounded with smooth sides, the side faces show a vertical stripe-tile pattern, and almost no straw ends stick out at the edges or the cut faces. The twine bands are faint. The bales read as foam or sponge blocks.

## main-street — FAIL
Unchanged except the hay: the new bales at the right read as bright yellow blocks against the muted street. Out of scope: the right-hand house fronts are still near-black in shade (lighting was not changed).

## house-frontage — PASS
No regression; nothing broken in frame. Out of scope: the shaded timber and window bay read dark.

## Fix list
1. Shoreline foam is an opaque constant-width white band on every bank and pier. ART (water material): drive foam from depth or distance-to-shore with a thin, broken, low-opacity mask (about 10-30 cm, noise-eroded, opacity under 0.5). Show it mostly at piers and on the downstream side, not as a continuous strip.
2. The swim pose is upright and waving, not a crawl. CODE+ART: play a horizontal front-crawl cycle with the capsule pitched so the spine lies at the surface (head at the waterline, shoulders just breaking it, legs just below). Alternate the arm strokes and remove the raised-arm idle.
3. The underwater body renders as a pale white slab, and a dark slab or shadow sits on the surface beside the swimmer. ART/CODE: make the character's material render correctly behind translucent water (fix the translucency sort or the depth-fade on the body), and remove or fix the stray surface decal or shadow plane.
4. A floating white foam box sits beside the main bridge (bridge-end-main-a). LEVEL: delete or sink the foam actor or mesh that sits there, or mask foam off on submerged boxes.
5. Hay bales are oversaturated, pillowy and lack straw. ART: desaturate the albedo toward tan-gold (roughly sRGB 190,160,95), sharpen the bale edges to slightly compressed squares, and add straw-end cards along the edges and cut faces at the reference density. Make the twine bands visible and darker, and remove the vertical stripe tiling.
6. Stepped, blocky waterline and foam at the arch bases (river-level-main). LEVEL/ART: close the gaps between water strips under the bridge so one continuous surface meets the piers, and let depth-based foam follow the real contact instead of the strip edges.
7. Hard horizontal seam on the underwater horizon. CODE/ART: replace the hard cut with exponential underwater fog so the seabed and the surface fade to murk with distance, and remove the visible edge of the post-process volume.
8. Flat water colour. ART: add a depth-colour gradient (lighter green-brown over the shallow banks, deeper blue-green in the channel) and lower the overall saturation away from pool cyan.
9. The two bridge-top captures do not show the bridge or water. LEVEL (evidence): re-aim bridge-top-side-a and bridge-top-main-a at the deck-to-bank joint and recapture.
10. The bridge-end ramp is paler and flatter than the deck, with a hard edge against the road. ART: match the ramp's material tint and scale to the deck stones and blend the road edge with a decal or vertex blend.
11. Conspicuous low-poly SM_Oak_01 blob trees (out of scope; owner note). LEVEL: replace them with the realistic oak or pine foliage used near the nodes.
12. Near-black house fronts in shade (out of scope; owner note). Lighting pass: add skylight or GI fill.
