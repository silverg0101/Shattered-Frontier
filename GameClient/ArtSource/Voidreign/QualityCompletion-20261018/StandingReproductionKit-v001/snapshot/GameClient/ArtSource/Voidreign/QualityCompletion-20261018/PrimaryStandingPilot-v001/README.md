# Standing-contact control pilot

Mount-StandingPilot.blend retains the original mesh and adds four fixed-length foot IK targets and orientation controls. The authored body translation is110mm downward; each target uses the freshly reviewed sole patch, not a feather/global minimum. This is a neutral-to-standing test and hold, not locomotion.

## Scoped passes and failures

Standing-frame sole minima are within0.022mm of ground; foot-target residuals within0.022mm. Bone-length relative change below0.0000006; neutralrest and coincident seams pass. Full60frame exchange reimports all284762vertices within0.000258mm. Check-MountStandingCache verifies all60frame seams and rigid sole edges; frames30-60 have0measured hold slip. Sole edge changes under0.003mm.

Whole-mount ground FAILS: Tail252vertices below5mm, maximum89.634mm; nonhair106vertices, maximum48.704mm in standing pose. Articulated secondary/feather/hanging-item treatment remains necessary. Full-source strain also exposes103.090mm edge stretch at a hard torso/leg weight boundary, despite foot and rest retention passes. PrimaryStandingSkinRepair-v001 reduces that specific edge error to1.011mm, but another binary root-volume eligibility boundary still gives71.090mm worst error. See PrimarySkinBoundaryReview-v001 before any new rig work or gait acceptance.

PrimarySoleProposals-v001 pins155/155/159/179downward sole faces across four feet. Parent inspected underside: these cover hoof bottoms, not hanging feather tips. Source sole heights103/86/49/10mm explain why earlier whole-support minima were misleading. The masks are authored review proposals, not universal hoof definitions.

PrimaryStandingGroundReview-v001 supplies saved-source neutral/standing images with an unsaved floor/light. Source geometry and source textures are not changed by those render helpers. No new GUI was opened for this static stance; existing unsaved windows remain preserved.

Exact profiles and tool snapshots are adjacent; dated shared profiles are in docs/art-templates/model-profiles. Two initial read-only check profiles had source hash-key slash mismatch; v002normalizes that exact key without changing limits. Validation-notes.json preserves the failure. Root-weight repair solver nonconvergence and3of3primary budget are explicit. Shared procedure: docs/art-templates/mount-primary-motion.md.
