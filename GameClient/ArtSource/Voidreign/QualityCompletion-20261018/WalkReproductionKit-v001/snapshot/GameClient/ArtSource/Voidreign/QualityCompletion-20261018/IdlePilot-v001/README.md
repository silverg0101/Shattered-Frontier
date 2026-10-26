# Voidreign idle v001: diagnostic only

The actual PrimaryContinuousInfluences-v001 mesh has 181 frames covering two three-second cycles at 30 fps. Voidreign settings: 3 mm body bob, 0.015 rad neck pitch and 0.012 rad head yaw with a quarter-cycle offset. Standing foot controls remain fixed; there is no root travel.

All five reopened checks pass: contact error 0.03537 mm, slip 0.01490 mm, coincident seams zero and cycle closure zero. Full animated exchange passes. Full-source strain remains unrated, with a maximum edge-length change of 27.29 mm. These checks do not approve anatomical shape, materials, attachments or secondary collisions. Frame 23 was inspected; its render has no diagnostic ground plane.

Producer: tools/Build-MountIdlePilot.py. Settings and source hashes: docs/art-templates/model-profiles/voidreign-idle-pilot-20261019.json. Exact copies are in this folder. Rebuild in a new output using CPU background Blender. Keep these mount values separate from docs/art-templates/mount-primary-motion.md.
