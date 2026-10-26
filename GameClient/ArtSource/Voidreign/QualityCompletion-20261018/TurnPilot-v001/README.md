# Voidreign walking turn v001

This is a circular walking turn, not a pivot or run turn. The actual mesh follows a 1.8 m radius, turning 0.25 rad per cycle and 0.5 rad across two cycles. All 73 frames are checked after reopening. Foot anchors and orientations remain fixed in world space during stance; swing interpolates between consecutive anchors. Cycle closure is compared in the root's rotating coordinate frame.

All eight scoped checks pass: planted slip 0.03308 mm, contact error 0.05308 mm, target error 0.07906 mm and root-relative cycle error 0.000671 mm. Full-source exchange and strain remain separate from these foot checks. The underlying skin is the rejected-as-an-improvement continuous reduction candidate; this is gait development evidence only. No anatomical, secondary or runtime approval.

Rebuild with tools/Build-MountWalkPilot.py and docs/art-templates/model-profiles/voidreign-turn-pilot-20261019.json in a new output folder using CPU background Blender. Shared procedure: docs/art-templates/mount-primary-motion.md. Run, transitions and articulated hair/tack collision work remain open.
