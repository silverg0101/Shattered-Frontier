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

## Boundary solver correction and walk adapter

Treat anatomical ownership and semantic hair labels as priors, never categorical skin cuts. Binary root-box eligibility can create discontinuities even between vertices with the same body owner. Rebuild body-only priors with the frozen distance formula before blending boundaries. Include semantic feather/body junctions where primary skin crosses the label boundary. Preserve exact sole selections as fixed Dirichlet anchors.

`Repair-MountRootWeights.py` supports an explicit `pcg` solver, body-only priors, semantic boundaries and fixed vertex selections in the hash-bound profile. Record normalized Laplacian convergence before normalization/influence pruning; convergence does not approve the resulting anatomy. Keep failed candidates and exhausted automatic budgets visible. A reviewed code correction must have its own review record and count; it does not reset the automatic budget.

`Build-MountWalkPilot.py --profile <profile> --output <new folder>` authors a CPU four-beat diagnostic walk from checked standing IK. Its source solver/contact reports must match the source hash. Keep the mesh and rig under a common assembly root; world foot targets stay outside that root. Global assembly motion carries unresolved islands but does not assign anatomical attachment ownership. Following cameras/lights and diagnostic floor geometry are review aids, not export geometry.

Declare stride, rate, duty fraction, lift, phase order, body bob and contact/loop tolerances per mount. Save, reopen and evaluate every frame: planted sole height and slip, swing clearance, foot target error, bone lengths, coincident seams, contact count and root-relative cycle closure. Exchange/reimport all frames and measure full-source strain separately. Rest geometry/UV/material retention does not imply unchanged skin weights or binds. Keep a scoped walk pass separate from whole-mount penetration, visual gait quality, secondary motion, transitions and skeletal runtime acceptance.

The current walk adapter assumes straight travel, an identity-rig Z-up standing prototype and a non-suspending walk. Run/gallop and turns need their own declared contact and root-motion contracts; do not merely rename or speed up a walk. Rebuild secondary root targets and deforming body colliders from actual primary motion before carrying hair/tack evidence forward.

### Continuous influence reduction and idle

Hard top-k pruning can reintroduce a finite jump after a converged smooth solve: two neighboring vertices may abruptly exchange a head influence for an elbow influence. Inspect actual weights at the worst absolute and relative strain edges with `Inspect-MountStrainEdges.py --profile <profile> --output <new folder>` before changing the rig or stride. Profiles bind source, strain report, domains and semantic selection to hashes.

The optional `continuous-top-k` reduction in `Repair-MountRootWeights.py` subtracts the (k+1)th largest weight from all influences, clamps to zero and renormalizes. It preserves the declared k influence limit and makes rank-entry weights approach zero. Near-equal ties with insufficient remaining mass stop for authored review. It can alter the deformation substantially; full-source strain/contact/shape checks remain required. This is an explicitly reviewed code correction, not an automatic budget reset or acceptance.

`Build-MountIdlePilot.py --profile <profile> --output <new folder>` freezes standing foot controls and authors bounded body/head motion. It saves/reopens and checks every frame for contact/slip/seams/bone lengths and cycle closure. Keep amplitude, axes, timing and bones in the mount profile. Idle does not replace run/turn or transition work, and foot checks do not approve secondary collisions.

### Evidence from the first articulated clips

The continuous influence reduction experiment did **not** improve Voidreign's worst full-source walk distortion (65.38 mm became 85.18 mm), despite passing foot checks. Preserve both candidates and the rejection record. Do not adopt this optional method as a recommended default. Inspect anatomical ownership, hanging tack and fixed-sole transition collars before another correction. Successful solver convergence and finite weights cannot substitute for deformation review.

For a walking turn, the walk adapter accepts a bounded `turn_per_cycle_radians`. It computes world stance anchors from the circular root path, holds their orientations and interpolates only during swing. Compare cycle closure after removing root rotation and translation. Straight-walk results do not transfer automatically to turns. Run and transition contracts remain separate work.

For occluded loose items, `Render-MountAnatomyAtlas.py` accepts per-view `isolate_regions`, paired with context views. It builds temporary diagnostic copies and records exact source face indices in `isolated-source-faces.npz`; the original source file is unchanged. Majority vertex membership can include partial-boundary faces. Isolation reveals shape, not semantic identity or permission to remove an item.

### Disconnected support proposal

`Inspect-DetachedAssetSupports.py --profile <profile> --output <new folder>` measures unsigned source-vertex distances to a declared support surface using a CPU triangle BVH. Freeze the rest mesh, component selections, semantic mask, owner domains and inventory hashes. Exclude each queried item from its own support. Store stable item IDs, closest source face/point and the declared distance review threshold.

These are sampled vertex distances, not exact triangle separation or signed solid clearance. A nearby face is a support proposal, not attachment ownership; an item can intersect support between sampled vertices. Preserve the item until context/reference review determines whether it is intended, detached, debris or unresolved. Keep topology detection, support proximity, semantic disposition and repair acceptance as separate records. Fused tack requires a separate face/attachment inventory.

### Blender review lifecycle

Keep one current visible review unless a comparison needs more. Track exact source path, process/window identity and review purpose when opening it. Close finished review windows normally; never use a blanket kill command for Blender. Preserve unsaved changes in a uniquely named recovery file before closing, without overwriting the immutable source. Keep active background generation/check processes separate from completed GUI reviews. Record the cleanup and kept current source; background jobs should exit normally when their script finishes. Respect the shared CPU/GPU ownership constraint.

### Exact local contribution analysis

`Analyze-MountSkinContributions.py` is a read-only diagnostic for declared edges and bone-weight transfers. It first reconstructs linear skinning and requires agreement with Blender's evaluated positions within the declared tolerance. Unsupported active modifiers, dual-quaternion skinning, envelopes or segmented bones stop the analysis. A locally improved hypothetical edge does not approve a global remap: it cannot establish behavior elsewhere or joint volume. Save the source/profile/tool hashes and keep this evidence separate from generated candidates and repair counts.

Replay historical attempts with their copied producer and profile, not an evolved canonical tool. The Voidreign gait reproduction commands demonstrate this distinction. New dependent outputs need new profiles with their actual hashes; thresholds and historical failures stay fixed.

### Hoof collar construction

Do not make the outer XY hoof margin a binary rigid-weight cutoff. A point just inside that rectangle can receive a fully rigid hoof weight while an adjacent point just outside retains a mixed leg weight, even though both are below the rigid-height threshold. Inspect full-surface strain at that boundary; passing sole contact alone misses it.

`Build-MountStandingPilot.py` supports explicit `hoof_xy_blend: continuous-margin`. It fades rigidity within the existing declared XY margin and multiplies the existing Z collar blend; exact sole vertices still override to rigid weights. Historical profiles retain `legacy-hard-margin` for replay. Keep source bounds, margin, influence budget and contact tolerances fixed while checking the code correction. Propagate the corrected standing source through the same reviewed root-weight construction, then recheck the actual gait, local edge deformation, fixed soles and retained geometry/materials. This experimental correction improves the target edge but was rejected as a full hoof fix: it also changes previously rigid shell weights. Do not adopt it as a default. Establish the anatomical hoof selection and inspect every incident edge before accepting a replacement. Unrelated shoulder/body deformation remains open.

### Regional regression checks

After a local weight correction, compare all source edges incident to every changed-weight vertex, including edges crossing the changed region boundary. `Compare-CheckedAssetEdgeCaches.py` reads hash-bound checked indexed motion caches and optionally a retained-geometry report plus changed-weight selection. It records full-clip regional maxima, per-frame quantiles and per-edge increases against the declared reference. The numerical delta reporting floor suppresses noise only; it never approves a quality regression. A lower worst value or improved target edge can coexist with many newly damaged edges.

### Material dependency audit

`Audit-AssetMaterialChannels.py` reads the frozen saved source, traces Principled channels and their image/socket paths, marks whether each shader reaches the active surface output, and records image size, color space, packed/external presence and content hash. Nested groups remain explicitly incomplete. This CPU audit does not repaint or render, infer a normal map from albedo, or approve material appearance. Use authored masks and reference/lighting checks before changing bone, leather, metal or eye materials; keep per-mount findings and channel requirements outside this shared procedure.
