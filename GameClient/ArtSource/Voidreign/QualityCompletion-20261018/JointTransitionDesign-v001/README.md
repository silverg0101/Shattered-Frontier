# Voidreign ankle transition design

Read `design.json` and the exact face selections in `../JointTopologyInspection-v003/joint-topology-selections.npz`. Latest views: `../JointTransitionAtlas-v003/` (four outer sides). Blue is protected core, orange collar, grey exterior; red/purple/green projected guides show the current rig without changing its joints.

The current hind collars end below the authored joint, and10triangles on3feet bypass their collar. This is a concrete layout defect to address before more skin tuning. It is not proof that a larger generic collar or automatic subdivision is sufficient.

Next: author and inspect the hind.L source-indexed inner/outer transition boundary, beginning at its8bypass triangles, separating flexible skin, rigid shell and feathers. Review inner/back views and declare any exact local retopology/correspondence before making a candidate. Keep cores/soles, numerical limits and exhausted attempt counts unchanged. Reuse the frozen73-frame motion to compare complete regional deformation before new locomotion.

Reusable steps are in `docs/art-templates/mount-primary-motion.md`; coordinates, IDs and measurements belong here and in MountProfile.md. No source skin/joint/mesh was changed.
