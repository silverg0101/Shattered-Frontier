# Voidreign walk v002: diagnostic only

This is the same 73-frame slow walk as v001, using the reviewed continuous four-influence reduction. It preserves stride, phases, geometry, fixed soles and numerical check limits. The automatic primary budget remains exhausted at 3/3; three separately reviewed code corrections bring the total primary candidates to six.

All eight foot, bone, seam and loop checks pass. Full animated exchange also passes. However, full-source strain worsened: the largest edge-length change is 85.18 mm, compared with 65.38 mm in v001. The 99th percentile changed only slightly, from 19.50% to 19.31%. This candidate is rejected as a quality improvement and must not replace an accepted source. Both walks remain prototypes needing anatomical and semantic skin review.

Frame 10 was visually inspected. The current Blender window (PID60952) shows this diagnostic walk; it is not a final model. Six older review windows were closed normally. The first hair prototype's unsaved state was preserved in HairMotion-20261018/Voidreign-HairPrototype-UnsavedReview-20261019-v001.blend.

Rebuild with tools/Build-MountWalkPilot.py and docs/art-templates/model-profiles/voidreign-walk-continuous-20261019.json in CPU background Blender, writing a new output folder. The producer and profile are copied here. Shared procedure: docs/art-templates/mount-primary-motion.md. Secondary motion, whole-mount clearance, materials, semantic ownership and runtime acceptance remain separate gates.
