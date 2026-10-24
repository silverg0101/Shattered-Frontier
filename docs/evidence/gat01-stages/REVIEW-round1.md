# GAT-01 gathering node growth stages, round 1

Evidence: `docs/evidence/gat01-stages/stages.png` (iron node; young, growing and mature; same camera).
Intent: show the growth stage by scale using existing meshes; the depleted state is separate and not judged here.

Overall: PASS (minor).

## Young / growing / mature: PASS
The three stages are easy to tell apart at this distance: about 0.5x, 0.8x and 1.0x screen height. All three sit on the ground with a contact shadow, with no visible floating or sinking. Young doesn't clip into the terrain. Because it is the same mesh, a smaller node clearly reads as the same object.

Issues:
- At any stage the rock doesn't read as iron. It is a plain dark boulder with no ore veins, rust or metallic flecks. At young size it reads as scenery, not a harvest node.
- The growing and mature stages differ less than young and growing do (silhouette step about 1.25x against 1.6x).
- The node sits in tree shadow, so at young size it is low contrast against dark foliage.
- Only flat ground is shown. Grounding on slopes, where a small node can float on the downhill side, isn't shown.

## Fix list (most visible first)
1. **Iron identity:** the resource isn't legible at any stage. Apply the existing iron-ore material or decal (rust/ore veins), or a material tint, to the node mesh. Check it at young scale first. ART, Engineer.
2. **Stage spacing:** growing and mature are too close. Widen the gap to about 0.45 / 0.7 / 1.0, as declared config values, not hard-coded. CODE, Codex.
3. **Grounding on slopes:** capture young and mature on a slope. If the downhill side floats, sink the node by a small fraction of its bounds scaled by stage, or align it to the surface normal. CODE, Codex.
4. **Capture:** add one shot in open light and one at the normal third-person gameplay distance, so young-stage readability is checked without heavy shade. CODE, Codex.
