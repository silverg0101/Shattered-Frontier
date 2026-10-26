# Voidreign walk diagnostic v001

Actual mesh,23 primary bones,73 frames/two cycles at30fps;36frames/cycle,stride0.45m,travel0.9m,stance0.65,lift0.10m,bodybob0.006m. These are mount settings, not shared defaults. Exact producer/profile copies are in this folder; canonical producer tools/Build-MountWalkPilot.py and dated voidreign-walk-pilot profile.

All8scoped checks pass: planted slip0.02582mm,contact error0.05165mm,target error0.07365mm,seams0,root-relative cycle closure0.000355mm. Full73-frame animated reimport passes; restgeometry/UV/material retention passes. Weights/binds intentionally changed vs initialprimarypilot. Actual stepping reviewed in frames1/10/19; Blender walk autoplay GUI63916.

Source PrimaryJunctionCorrection-v001 has two reviewed code corrections after exhausted automaticprimarybudget3/3. Totalprimarycandidates5; neverresetbudget. Standingworstedge103.09mm to22.65mm remains unrated. No productionpromotion. Wholemountground,loose semantics,hair/tack,materials,idle/run/turn/transitions andruntime remain open. Oldrigid-harness hair evidence does not transfer to articulatedwalk.

Rebuild using CPU background Blender with canonical producer/profile into a newfolder, then full animated exchange/strain and Check-AssetRigRetention.py. Shared rationale: docs/art-templates/mount-primary-motion.md. Exact commands and source inputs are in the reproduction manifest when frozen.
