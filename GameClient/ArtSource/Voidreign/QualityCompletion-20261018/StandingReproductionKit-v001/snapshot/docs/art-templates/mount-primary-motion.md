# Mount primary rig and locomotion procedure

Establish anatomical motion before final [secondary motion](mount-secondary-motion.md). Keep source coordinates, joint positions, seed volumes, bone names, pose angles and thresholds in a separate per-mount profile. The Voidreign example is indexed by [its primary plan](../../GameClient/ArtSource/Voidreign/QualityCompletion-20261018/PrimaryLocomotionPlan-v001/README.md).

## 1. Freeze and inspect

Bind the editable source, exact indexed rest mesh, transforms, units, UV/material state and semantic selections to hashes. Require independent rest-retention evidence before using a captured rest array. Declare axes explicitly. Render orthographic front, rear, both sides and closeups; do not mirror an asymmetric stance or infer foot soles from the global minimum vertex.

`Render-MountAnatomyAtlas.py --profile <profile> --output <new folder>` produces a CPU diagnostic mesh, views, authored landmark/chain markers and frozen vertex-region proposals. Its gray diagnostic does not replace the textured asset or preserve a usable rig. Box regions can include nearby straps and ornaments. Review those mistakes before assigning skin weights.

## 2. Propose ownership from attachment topology

`Propose-MountPrimaryDomains.py --profile <profile> --output <new folder>` propagates authored anatomical seeds over the canonical positional mesh graph. This adapter expects a metre-scale, Z-up, source-indexed semantic archive compatible with its recorded fields. Lower-limb tube seeds and upper-body seeds belong in the per-mount configuration. Geodesic ownership can distinguish a nearby hanging strap attached to the torso from leg feathering connected to the limb. False fused bridges can still mislead it. Disconnected non-hair islands remain explicitly unresolved; do not silently nearest-bind or delete them.

Render the frozen graph-derived selections with the atlas tool. Separate anatomical core, limb feathering, fixed tack and hanging objects; geometric reachability is a proposal, not semantic approval. Review each leg independently and inspect joint neighborhoods. Upper-limb blending needs its own authored domain; lower-leg seeds alone do not define an anatomical skin.

## 3. Validate a small primary joint pilot

`Build-MountPrimaryRigPilot.py --profile <profile> --output <new folder>` builds the configured skeleton and a short, authored flexion sequence on the original indexed mesh. It disables original deformation modifiers in that new candidate to prevent duplicate secondary or rigid-harness motion. The independent captured rest establishes the object-to-world transform. Existing vertex groups remain stored; new primary groups and binds intentionally differ from the baseline. Keep the original source unchanged.

The current prototype uses bounded nearest-segment weights within proposed ownership domains and authored root-blend volumes. It leaves unresolved islands unweighted and records them. This is a diagnostic skin proposal, not an anatomical solver. Check fresh-reopened rest correspondence, pose deformation, joint volume, coincident seams, material/UV retention and unintended tack motion before extending the clip. A geometry-retention pass does not accept changed skin weights or skeleton binds. Record the new rig's own influence budget and expected bind changes.

`Open-MountPrimaryReview.py --report <new report> [--play]` opens an explicitly visible unsaved review when run inside the saved pilot. It grants no acceptance and does not save changes.

## 4. Standing contacts and real gait clips

`Propose-MountSoles.py` selects downward-facing connected face patches inside authored foot bounds and limb ownership. Review exact faces from below; the largest patch is a proposal and feather tips are not soles. `Build-MountStandingPilot.py` adds identity-rig/Z-up foot targets, rigid hoof cores with blend collars, fixed-length IK and an authored body translation. It records finite rest-frame pole-axis calibration. Its neutral pose, numerical foot targets, contact patches, lengths and seams are separate gates. Do not transfer its current identity-rig convention to another coordinate system without an adapter.

`Export-CheckedMotionAlembic.py` can capture and freshly reimport the full short clip. `Check-MountStandingCache.py` then checks every frame's seams/sole rigidity and the declared planted hold's contact/slip, while reporting whole-mount ground penetration separately. A hoof-contact pass must not hide tail or ornament penetration. `Render-MountGroundReview.py` adds an unsaved diagnostic plane/light to saved-source pose previews. Keep its diagnostics out of asset geometry.

Always inspect full-source strain before using the skin for gait clips. A hard body/limb ownership boundary can stretch torso surfaces even when foot targets, fixed bone lengths and seams pass. Ownership labels are not hard skin-weight cuts. `Repair-MountRootWeights.py` is a bounded proposal adapter: harmonically blend a declared geodesic boundary band, keep external weights and geometry fixed, record convergence, prune to the declared influence budget, and recheck shape/contact/seams afterwards. Numerical nonconvergence remains explicit; a finite approximate result is not a converged solve or automatic anatomical approval.

Author hoof/sole regions and intended ground contacts; review underside geometry separately from feathers and dangling tips. Add foot targets, stable bend directions and joint controls. Solve standing body height and limb bend together while preserving proportions. Check reachability and deformation before treating a lowered foot as a correction.

Then author or retarget real idle, walk, run and turning clips, recording provenance and conventions. Validate planted-foot sliding, ground clearance, phase/contact sequence, joint limits/deformation, loop seams, root motion, seat/rider placement and transitions. Declare the mount/product's numerical limits and units before automatic pass/fail. A short joint-flex test or rigid-body response sequence is not locomotion. Engine gait blending and runtime delivery require separate evidence.

## 5. Add secondary features after primary motion

Inventory all loose items with stable IDs and attachment ownership. Choose static retention, bone chains, constrained rigid articulation, flexible surface simulation or another authored representation per item. Rebuild hair/tack root adapters and body/rider colliders from the actual primary rig. Repeat whole-mount checks over gait extremes and transitions; earlier rigid-harness evidence does not transfer automatically. Keep failures, finite attempt budgets and source/profile/tool snapshots indexed in the asset job.
