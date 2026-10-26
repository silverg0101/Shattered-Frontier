# Reusable mount secondary-motion procedure

Use this procedure for a mount's mane, tail, tassels, reins, or other authored features that move independently from the primary rig. It adds a review path to the general [model pipeline](model-pipeline.md) and [model quality runbook](model-quality-runbook.md); it does not define universal geometry, motion, or runtime limits. [Voidreign's dated quality revision](../../GameClient/ArtSource/Voidreign/QualityCompletion-20261018/README.md) is a mount-specific worked example. Its coordinates, settings, measurements, warning values, and attempt history are not defaults.

## Dependency: establish primary locomotion first

Use the [primary rig and locomotion procedure](mount-primary-motion.md) for the anatomical domain, joint-pilot, standing-contact and gait stages.

Final secondary-motion tuning depends on the body's real animation. Establish the primary skeleton, anatomical skinning, stance and foot contacts, then authored or retargeted idle/walk/run/turn clips and transitions. Review joint deformation, planted-foot sliding, ground clearance, root motion and rider placement before final mane/tail/tack simulation. A rigid-body harness is useful for debugging inputs and contacts; its response phases are not locomotion clips.

Inventory every moving attachment independently of the hair selection: leg feathering, straps, reins, chains, ornaments and loose cloth. Assign each an owning bone/surface, anchored boundary, motion representation and collision partners. A rigid skull ornament, articulated chain and flexible strap need different controls. Protect fixed tack and avoid applying the same cloth settings to every item. Use deforming body/rider colliders driven by the primary rig, then validate the whole mount across gait extremes and transitions. Bake/recheck secondary motion after primary animation changes; do not carry rigid-harness acceptance forward.

## 1. Freeze the per-mount input contract

Create a versioned, mount-specific job record before deriving guides or tuning motion. Record:

- Original editable source and hash; exact mesh/object identity, topology, transforms, units, rest pose, materials, UVs, shape keys, and animation inputs.
- Intended feature regions and neighboring protected regions; candidate roots and attachment boundaries; whether each feature is fused, separate, card/groom based, skinned geometry, cloth, or articulated decoration.
- Reference views and their source, camera/scale relationship, and which views support semantic interpretation.
- Rig and collider identities, units, authored dimensions, coordinate convention, animation clips/frame rate, and required contact cases.
- Tool versions, profiles/configuration hashes, output paths, provenance, and stage attempt budget.

Bind every selection, guide, diagnostic, and repair proposal to the source and mesh hashes it actually reads. A changed mesh, topology, object identity, or source file requires fresh correspondence and review; do not carry numeric vertex, face, loop, component, or guide IDs forward by assumption.

Design adjustable balance and operating values as declared typed configuration: include shipped defaults, units, safe bounds, permissions, and effective-time rules. Put plain-language controls through the shared configuration workflow. Keep formulas, algorithms, security invariants, and data topology in versioned code/content for compatible releases; do not use editable console expressions. Choose bounds per asset and product policy; this procedure supplies no universal threshold.

## 2. Review semantics, clumps, and roots

### Tool compatibility before reuse

The workflow is reusable; each tool still needs a compatible input adapter. The saved-guide fixture currently fixes its frame rate, clip length and motion phases in versioned code. It uses prescribed rigid mount motion, not locomotion. Source-hashed reports, triangular mesh correspondence, guide array layout, rig hierarchy and assembly coordinates must match the selected tool contract. Check those contracts before execution. For another clip, skeleton, representation or coordinate system, author and validate an adapter instead of weakening assertions or renaming incompatible data. Mount-specific settings remain in that mount's profile index; the shared procedure supplies no physical defaults.

Before weighting, save a source-hashed semantic artifact with explicit positive feature regions, protected body/equipment regions, root/attachment bands, permitted feathering, cut boundaries, and ambiguous/fused areas. Review connected components and clump identities against the source and references. Record unresolved identity or attachment instead of treating geometric proximity as proof.

Represent guide paths and skin/deformation regions together. Each guide or path should identify the clump/region it represents, its root and attachment, the source correspondence, and whether it is authored geometry or a diagnostic proposal. Check that skin weights deform the intended surface and leave protected regions stable. Added guides or diagnostic clumps do not count as coverage or acceptance of the supplied surface unless their correspondence is reviewed.

Use current source-bound inspection and coverage tools where applicable. For example, from the repository root, with fresh output folders:

```powershell
py -3 tools/Propose-FeatureCoverageGuides.py --guide-report <guide-report.json> --profile <mount-coverage-profile.json> --output <new-coverage-folder>
& <blender-executable> --background --factory-startup --threads 4 --python-exit-code 1 --python tools/Render-FeatureCoverage.py -- --source <mount-source.blend> --guide-report <guide-report.json> --coverage <coverage-report.json> --output <new-coverage-view-folder>
```

These tools propose paths on a supplied rooted graph and render depth-tested rest-distance views after checking source/report/array provenance. They do not decide clump semantics, create accepted skinning, or prove finite-width motion coverage. Treat colors and point distances as diagnostics.

## 3. Check root and contact feasibility

Check the full required animation interval, including starts, stops, turns, gait extremes, and mount/environment or rider contact. Review free guide nodes and pinned roots separately. A free-node clearance result cannot make a fixed root feasible; record a root/envelope conflict and identify whether the issue belongs to the authored root, feature representation, collider, or mount pose. Distinguish guide-envelope overlap from penetration of the actual surface into a collider.

Use the saved-bake checker when its input contract fits:

```powershell
py -3 tools/Check-FeatureGuideClearance.py --simulation <saved-simulation-folder> --surface-profile <mount-surface-profile.json> --output <new-clearance-folder>
py -3 tools/Check-FeatureTrajectory.py --simulation <saved-simulation-folder> --profile <mount-trajectory-profile.json> --output <new-trajectory-folder>
```

The clearance checker validates saved array/rest/transform identity and separates free-node residuals from fixed-node conflicts. The trajectory checker inspects a saved guide bake and fingerprints its inputs. Neither modifies or rebakes the source, nor accepts actual surfaces, contacts, appearance, or runtime motion. Keep guide displacement, actual-surface clearance, style deviation, and contact evidence as separate results.

## 4. Freeze diagnostic studies before changing motion

For a suspicious saved bake, preserve it and make a source/code/profile-hashed diagnostic copy. Replay with observation-only instrumentation and require the replay to reproduce saved output-frame points, transforms, rest data, and counts before interpreting substeps. Keep prediction, structural corrections, contact-position changes, and ground changes separate in the report; these are observed displacements, not force or energy attribution.

Use the trace tool for a compatible frozen solver:

```powershell
py -3 tools/Trace-FeatureGuideMotion.py --simulation <frozen-simulation-folder> --profile <mount-trace-profile.json> --output <new-trace-folder>
```

If evaluating a motion change, freeze the rig, guide set, colliders, timing, solver, and acceptance policy. Change one declared variable per diagnostic study and retain every failed case and its style/contact effects. Do not combine favorable ablations automatically, hide failures with smoothing/clamping, or reset an exhausted attempt count by renaming an output folder.

The repository includes analytic study code for a proposed split-contact velocity approach:

```powershell
py -3 tools/Study-FeatureContactVelocity.py --profile <contact-velocity-study-profile.json> --output <new-study-folder>
```

The helper and fixtures are diagnostic/proposed code, not a delivered solver integration. Analytic fixtures alone do not solve coupled strands, collider discovery, positional feasibility, changing contact normals, finite-width skins, swept collision, or fixed-root constraints. Label proposed design, standalone study, and integrated motion distinctly.

### Coupled solver regression before mount integration

Run coupled strand fixtures with static ground, obstacle contact and release, prescribed translating/rotating contacts, and deliberately infeasible pinned roots. Preserve the baseline and candidate under identical geometry, forces, motion and limits. Inspect substep prediction, internal and contact corrections, encountered versus final contacts, relative velocities, convergence, stretch, penetration, style and recovery. A deliberately infeasible case passes its diagnostic contract only by identifying the conflict; it remains unsuitable for delivery.

```powershell
py -3 tools/Study-CoupledFeatureContacts.py --frozen-solver <compatible-frozen-solver.py> --profile <mount-design-profile.json> --motion-config <mount-motion-config.json> --output <new-coupled-study-folder>
```

The experimental contact adapter supports explicitly pinned reconstruction and velocity-projection algorithms. In a coupled position solve, structural corrections can react to earlier contact projections. Subtracting every accumulated contact correction from the solved displacement can therefore destroy a necessary cancellation. Preserve such failures; compare net solved displacement before applying collider-relative normal velocity constraints. A minimum-change feasible velocity is still not a complete coupled strand impulse solve, nor proof against artificial launch, initial overlap or missed swept contacts.

Declare a damping model explicitly if revising response. A spring-frequency damping ratio is a nominal continuous linear-oscillator relation, not a critical-damping guarantee for discrete constrained strands. Report the authored coefficient and effective coefficient, units and bounds. Keep acceptance limits fixed and compare recovery at the end of the stop interval as well as transient maxima. Do not accept a profile solely because it produces less movement.

Only after the relevant fixture contracts pass, run a bounded full-guide experiment with the actual mount data. The command below produces diagnostics, not a replacement mesh or delivery approval:

```powershell
py -3 tools/Simulate-SplitContactGuides.py --frozen-solver <compatible-frozen-solver.py> --profile <mount-design-profile.json> --guides <source-bound-guides.json> --config <mount-motion-config.json> --output <new-trajectory-folder>
```

Retain the generated core, replay adapter, contact kernel, original frozen core, effective profile, inputs and output hashes. Generated core files need their adapter hooks for simulation; use the saved replay adapter to reproduce them. Verify producer trajectory hashes when available and state explicitly when a historical output lacks that attestation.

## 5. Check actual surfaces, then export and reimport

Evaluate deformed source surfaces over the required interval, with skin/clump interiors and actual collider shapes represented. Check contact, seams, attachment gaps, startup, retained pose/style, and protected-body deformation. Particle or centerline passes are not surface acceptance. Rendered evidence complements measurements; it does not substitute for them.

After a reviewed source change, reopen the saved editable file in a fresh process. Export each target exchange and import it into an empty scene with recorded importer settings. Compare scale/axes, mesh and material data, bone/clip mapping, animation bounds, deformed surfaces, contacts, and endpoints/transitions against the source. Bind reports to the exact source, exporter, exchange, importer settings, and reimported artifact. The existing `Check-FeatureMotionSurfaces.py` and `Render-FeatureMotion.py` can check compatible saved motion data; they do not establish engine acceptance. Engine/runtime validation requires its own scoped engine evidence and authorization.

## 6. Bound repairs and report status

Use the general pipeline's finite default of one initial attempt plus at most two repairs per failed stage, or a stricter authored policy. Each repair record names the failed check, changed variable, source/profile/code hashes, before/after evidence, and remaining failures. Preserve old inputs and evidence. If authority, correspondence, representation, or attempt budget is insufficient, leave the stage `needs-review` or `blocked` with the smallest concrete next action. Never convert missing or unimplemented checks into passes.

At handoff, link the source-bound semantic/guide/skin artifacts, diagnostic studies, full-duration surface checks, exports/reimports, visual evidence, and a concise gate summary. State which steps are implemented tools, which are authored per-mount data, and which remain proposed or manual. A successful diagnostic run does not prove unattended automation or production readiness.

### Diagnose failed vertices before changing semantics

Bind each failure vertex/frame to the exact saved source, surface report and selection archive. Render a small patch reached through geometric edges only, in rest and the failing pose, both with surrounding mesh and in isolation. Validate the evaluated vertex against the measured failure position and the semantic coordinate frame. Check camera bounds and positive depth; inspect the image because framing alone does not prove the patch is visible through surrounding surfaces. A nearest-guide distance or an isolated patch cannot establish clump identity. Preserve unresolved buried/occluded regions for wider context review; do not remove them from collision checks merely to pass. `Render-AssetVertexReview.py` implements these diagnostic views for compatible source-bound profiles.

When a reviewed design explicitly allows a single mesh evaluation despite a separately documented failed feasibility gate, use a hash-pinned experimental contract and retain the failed gate in both source and experiment reports. `Build-FeatureRigExperiment.py` is an explicit adapter for that narrow diagnostic case. It leaves the ordinary builder and its acceptance guard intact. The experimental file must undergo the full surface checks; successful baking never promotes its delivery status.

For a diagnostic rebind, `Check-AssetRigRetention.py` compares identical indexed rest geometry and appearance fields in a fresh process, and explicitly reports any named-weight/bind differences. Its material signature covers common properties and connected image pixels, not arbitrary nested shaders/drivers. Evaluate full surfaces separately.

When guide points pass while the skin fails, bind the actual failed vertex/frame to its rest position and influencing guides. `Propose-FailureFeaturePaths.py` traces a finite source-graph path to existing authored roots as a review aid. These paths can follow folded surface triangles, cross proposed attachment links, or terminate in the middle of a lock. Inspect complete clump ownership, roots and centerlines before skinning them; adding a path is not itself a collision repair. Preserve regressions in other contacts when comparing motion improvements.

#### Actual-surface fallback for clumps that miss their guides

If guide clearance passes but an actual clump is too broad, off-guide, or still collides, use a separately frozen CPU diagnostic experiment that evaluates the supplied surface. `Build-ActualSurfaceClothPilot.py`, `Build-FeatureSurfaceCorrective.py`, and `Capture-FeatureAttachmentMotion.py` are specialized diagnostic paths; they do not define universal cloth behavior or deliver a runtime solver. Bind each experiment to the exact source and record its representation and evaluation contract.

Before interpreting a proxy result, require a source-bound seam-canonical index map, a proxy expressed in metres, and a verified evaluated source-to-proxy position match. Record the guide/root attachment map and surface-binding map, including whether each constraint is pinned, sewn, or otherwise attached. Keep the original collider set and collision exclusions unchanged, prove that evaluated colliders are visible to the solve, and check a settled initial contact plus step-by-step contact, attachment, and style behavior over the full required interval. A cleared distance metric alone is insufficient: instantaneous vertex SDF projection can move a surface abruptly, teleport a clump, or tear its attachment. A full-duration pass on unchanged declared contact, gap, seam, startup, and per-step checks is scoped evidence for that tested surface and setup; it does not establish runtime or production acceptance.

Analyze source seams in canonical seam groups. For a cut root, measure the actual cut band/ring; do not substitute the boundary of a broad domain, which can include cap faces or internal seams. If fitting a rigid local root, measure its motion against the deformed source skin throughout the interval rather than assuming it follows correctly. Blender cloth caches and Geometry Nodes animation are not ordinary skeletal exchange data: explicitly bake and import the chosen representation, then keep engine/runtime acceptance as an open gate until verified there.

#### Cache startup and measured playback

Treat cache generation and fresh-reopen playback as separate checks. Renaming cached cloth frames to another frame range failed output comparison after reopening; a cached or baked status alone does not prove usable dynamics. Start from a positively held startup pose and measure both free-surface RMS and peak convergence: an actual full-surface startup can meet its RMS criterion while its peak still fails. Then restore the original animation clock before freezing the measured canonical local points to MDD with `Build-FeatureMotionCache.py`. Reopen the result and compare every output frame. All-frame point equality verifies playback only; it does not certify the underlying cloth dynamics. A negative-clock cache attempt did not show effective dynamics, so do not claim that negative-frame caches work.

`Capture-FeatureOutputPoints.py` can turn a checked final procedural feature into efficient exact-index measured playback. Validate source and proxy transforms, source geometry, and that playback bypasses the old deformer as intended. This is a representation/playback conversion and adds no physics acceptance.

### Fused locks and local ownership

A disconnected-component detector cannot separate strands fused into a common mesh. Freeze proposed face and vertex domains, excluding protected anatomy, and inspect the actual selected surfaces in context and isolation. `Propose-FeatureClumpDomains.py` supplies geometric tubes and plane-cut connected components as diagnostic proposals. A narrow tube may cover only a strip of a lock; a plane cut may still include neighboring branches fused below it. Record these failures instead of declaring the selection complete.

Use finite cross sections to review boundary, width, branching and roots. `Inspect-FeatureCrossSections.py` records nearby open/closed plane contours and candidate area centroids. A closed contour can include multiple fused locks or nested surfaces, and its centroid need not be inside a concave section. These measurements inform authored domains and guides; they do not establish semantic identity or license shell deletion.

When only a distal branch can be isolated safely, label it a local branch. Its attachment may already move with the parent skin. A body-fixed root is then an incompatible replacement: define and verify the moving attachment frame, preserve the surrounding weights, and check the handoff before any local dynamics bake. Keep force-response experiments separate from representation/attachment changes.


### Portable cache packaging and exchange

Copy and hash-bind every active inherited mesh cache into the destination bundle. Resolve its path against the original source before copying. Save destination-relative cache paths without allowing Save As to remap them against the old source folder; keep unrelated image/material dependencies intact. Reopen and compare every output frame, then rerun checks on the evaluated original mesh. Cache-point equality alone can miss downstream modifier mistakes. Preserve failed path-remapping attempts.

`Export-CheckedMotionAlembic.py` supplies an optional single-mesh offline exchange check: export evaluated motion, reopen a diagnostic Alembic import, and compare indexed world positions at every frame. It does not preserve a skeletal rig or establish UV, material, engine or runtime acceptance. Declare the intended delivery contract before choosing an exchange. `Freeze-AssetEvidence.py` freezes an explicit file list and verifies copied snapshots and referenced large inputs; a reference-based kit requires those original files to remain available. Keep its manifest and replay commands indexed from the asset job.


### Attachment gaps do not establish a complete attachment

Audit geometric islands separately from the enabled spring graph. Record each island’s pin membership, incoming/outgoing sewing links, ownership proposal, and attachment-frame support. A path to a pin proves graph reachability; a small endpoint gap does not establish orientation, torque transfer, root-strip support or intended flexible behavior. In particular, a free fragment with one point link may rotate while its distance check passes. Report these cases for authored attachment review rather than counting them as unanchored debris or silently pinning/deleting them.

`Diagnose-FeatureClothWarmup.py` now exports both connectivity graphs, per-island velocity summaries and a source-indexed held-point snapshot after reproducing the producer metrics. Use it to distinguish whole-island motion from local bending before response tuning. Its point-to-point sewing graph is a diagnostic model of enabled edges, not proof that the solver enforces the intended joint.

Check posed exposure before disposing of a fragment that was hidden at rest. `Export-CachedComponentInspection.py` reads the checked full indexed world-position capture from an offline exchange run and preserves original component IDs at a finite declared set of frames. `Inspect-ComponentVisibility.py` accepts an explicit component subset while retaining the full mesh as the occluder. Reports must identify that subset and the sampled poses. Any missed target-only control ray stays unresolved; absence of rest visibility is never deletion approval.

A rigid attached micro-detail and a flexible strand need different authored behavior. `Build-FeatureAttachmentFrame.py` is an experimental option for an explicitly proposed rigid fragment: preserve its rest offsets in a well-conditioned parent-triangle frame, leave every other cached point bit-identical, and check the held samples and all output frames. Verify metre-scale proxy coordinates, matching source-index arrays, the parent support region, frame conditioning, rest-gap retention, source retention and contacts. It does not approve that classification, provide new flexible dynamics, or excuse the remaining settling failures. Do not apply it to every failing vertex or reset exhausted repair counts.

For source-patch rendering after index-preserving Geometry Nodes, `Render-AssetVertexReview.py` requires an explicit modifier-name adapter and hash-bound rest-retention evidence. It disables those nodes only for the rest diagnostic, restores their original enabled states for the posed diagnostic, and checks evaluated vertex count and triangle order in every rendered state. Framing and topology checks still do not prove visibility or semantic ownership.


A single point or hinge may be intentional for a dangling ornament. Declare the intended degrees of freedom before treating rotation as a defect. Do not turn every link warning into a rigid attachment or freeze a flexible strand to make a settling metric pass. Verify root velocities and support geometry separately from the free surface. `Analyze-FeatureIslandMotion.py` decomposes two frozen held samples into best-fit whole-island motion and remaining shape change; this is descriptive evidence, not a corrective transform. Its optional complete held-window analysis uses existing RMS/peak limits and reports every passing frame in the declared interval, including none. Never cherry-pick an early quiet frame or extend a duration ceiling to hide a persistent failure.

For a complete native held recording, `Diagnose-FeatureClothWarmup.py` can hash-bind every held cache file and export the full measured window, requiring agreement with its independently recovered last two frames. `Plot-FeatureHeldWindow.ps1` plots the exported numerical curves using the existing Windows Framework Charting renderer; it performs no simulation. Preserve full data and state any cropped display interval. Neither this plot nor a two-frame fit identifies a physical damping coefficient. Root constraint and material-response studies need separate finite contracts and retained failures.


### Check deformed shape before accepting a contact repair

Preserved source UVs/weights/rest vertices do not prove the evaluated surface retains usable shape. Measure source-edge lengths and triangle areas over the saved clip as well as contacts, gaps and per-frame motion. `Analyze-AssetSurfaceStrain.py` reports relative and absolute indexed-edge changes, baseline edge lengths, per-frame quantiles and area changes from the checked full world-point capture. Tiny-edge relative maxima need their baseline and absolute values; this diagnostic supplies no invented acceptance limit. Declare strain/style allowances for the asset before accepting it. Bending, intentional elastic deformation and material stretching are different cases.

Use `Compare-AssetEdgeMotion.py` to trace the same source edge/frame through guide skin, raw cloth, contact correction and frozen playback. Check its original root/free classification. A large distortion present in guide skin and unchanged by cloth is an upstream input/weight/rest-shape problem; do not hide it with a contact pass or shader repair.

Give each physical simulation explicit primary-deformation and rest-shape inputs. Secondary guide animation must not silently drive both cloth pin targets and animated rest lengths. Moving root targets should follow the owning anatomy/bone/root frame; flexible rest geometry should keep its authored shape unless an explicit model requires animated rest. The existing cloth producer retains its historical `original-skin-dynamic-rest` behavior for old-profile replay. New profiles must declare a compatible input contract rather than inheriting that legacy default.

`Build-ActualSurfaceClothPilot.py` has a bounded `rigid-assembly-rest` adapter for the verified rigid-body harness only: require source-hashed full protected-rigidity evidence and an explicit harness assertion; retain the original source-to-proxy skin preflight; then disable the secondary proxy Armature, verify undeformed metre-scale rest correspondence separately, and use fixed cloth rest geometry. Original pins, collision masks, response parameters and limits remain intact. A mount with articulated primary anatomy needs its own primary-skeleton/root-target adapter; do not use this option to freeze it. Warmup diagnostics must respect the recorded input mode instead of re-enabling a disabled secondary Armature.

Blender’s [cloth Shape documentation](https://docs.blender.org/manual/en/5.3/physics/cloth/settings/shape.html) explains that Dynamic Mesh derives the evolving rest shape from preceding deformation modifiers. Use the runtime/version recorded by the asset; this documentation reference is not an instruction to upgrade Blender.

### Keep routine delivery bounded

Before a full bake, use one short output pilot with the declared settling interval to check primary motion, root targets, rest shape, source correspondence, surface shape and contacts together. If a guide or solver check passes while the original rendered mesh fails, inspect the modifier/input stage that first introduces the defect before changing response parameters. Stop at the job's repair budget; record a concrete blocker and next action rather than restarting the same search under a new output name. Keep architecture experiments explicitly separate from ordinary asset production. After a passing pilot, run one unchanged full clip and a fixed delivery sequence: saved-source checks, exchange, visual review and evidence packaging. A failed gate remains a failed gate; exploratory elapsed time is not routine asset throughput.

## Separate appearance class from motion ownership

A hair-like surface does not automatically belong to the mane or tail. Record material class, physical feature/attachment, primary support and secondary representation independently. Leg fringes, reins or cloth may share a dielectric material while requiring different bone roots, controls and collision partners. Never reuse an appearance mask as motion ownership without support/context review.

`Inventory-SemanticMaterialBoundaries.py` groups mixed triangles by exact canonical-edge connectivity, writes stable source-bound patch IDs and face/vertex selections, and preserves every patch. Pair whole context with isolated priority patches. A mixed triangle can cross a scalp root, strand tip, fused skin or an incorrect selection; none is automatically fillable. Track reviewed and unreviewed IDs directly, not by rescanning folders.

`Inventory-FeatureSupportAdjacency.py` reports each semantic component's direct nonfeature support nodes and authored primary-owner priors. Freeze expected support owners per feature in the profile. Adjacency is evidence, not final binding: fused false bridges and disconnected intended attachments remain possible. Unexpected leg/body ownership requires review, not a nearest-bone fallback.

After an exact reviewed correction, `Revise-SemanticFeatureLabels.py` preserves untouched and coincident vertices and writes a label-only revision. It deliberately omits stale roots, distances, guides and weights. It is not a drop-in replacement for an old simulation selection. Rebuild derived fields, attachment review and per-feature adapters for the new contract; preserve old outputs with their old hashes. A partial region correction does not approve the entire physical feature or other coverage.

Guide and diagnostic-rig producers use `FeatureSelectionContract.py` to reject unknown nonzero labels and missing derived data before authoring. Their ordered feature profile must cover all nonzero labels. Freeze this helper alongside producer snapshots; new outputs copy it for replay. These guards cover those two producers, not every historical adapter. Every other consumer must explicitly declare support for a revised archive before it can be used. Check appearance and motion regressions separately; an owner correction does not fix an unrelated speed/collision failure.

## Prepare local roots and verify the existing parent skin

A spatial region is a proposal, not an anchor. Inspect context and isolation; reject an empty support band. `Prepare-FeatureRegionRoots.py` derives roots from exact canonical edges crossing a declared region boundary, checks allowed support-owner/semantic priors and rejects rootless components. It exports local full/boundary faces, roots/supports and geometric root distances without virtual attachments. The cut boundary still needs anatomical review.

Before reweighting, use `Trace-FeatureRegionParents.py` to inspect actual saved-source root/support bone weights and all declared source-frame trajectories. Ownership labels alone do not describe the saved skin. `Preflight-FeatureParentSkin.py` optionally evaluates one fixed-root harmonic extension of the declared primary chain and every incident edge against an exact Blender baseline. Preserve roots, outer weights, seams and influence limits. If the proposal is numerically equivalent to existing motion, keep the existing weights; do not author a no-op skin just to count progress.

`Capture-FeaturePrimaryFollowReview.py` bakes exact-index local source copies for inspection, saves/reopens and checks every declared integer frame, then renders a sampled CPU preview. It copies source vertex/face IDs and labels its scope as existing primary follow without secondary dynamics. It is neither a production rig nor a simulated response. `Make-AssetReviewGif.py` records source-image hashes and timing and checks total duration against source frame span with GIF timing quantization; preserve/invalidate failed previews independently of valid source motion.

Only after parent/root review, build a separate secondary adapter with fixed rest geometry, deforming roots and the declared collision partners. Local cropped leg context is not full body/tail/environment coverage. Record source integer/subframe sampling and do not claim that a display preview validates physics, interpolation or runtime behavior.

## Choose secondary representation from actual topology

Inspect connected components, edge incidence, boundary loops, isolated/nonmanifold elements and which boundaries are anchored before treating a mesh as cloth. A capped strand/shell whose only open boundary is its attachment differs from a sheet with a free perimeter. Surface cloth does not explicitly retain cross-section. Consider reviewed internal-volume springs, rod/chain/cage or other shape-preserving representations as appropriate; no method is universal across mounts. Keep independent components separate so internal rays cannot create false cross-component springs.

`Build-LocalFeatureClothProof.py` is a finite CPU local diagnostic driven by source-indexed baked parent points, fixed rest and positive held warmup. It records pin error, free motion, edge changes, sampled proximity, exact source sampling and all-frame reopened native-cache replay. Optional component splitting/internal springs is an explicit representation revision, not an untracked tuning sweep. Declared local experiments do not reset exhausted full-model stages or approve runtime/export. Integer-frame linear driver interpolation and cropped/open leg colliders must remain visible limitations.

Diagnose a failure before rebaking. `Diagnose-LocalClothAttachment.py` compares baseline free distances and root/free edge length bounds: distance-to-surface is1-Lipschitz, so a root distance plus rest-edge length gives a necessary upper bound for free clearance without stretching. A ruled-out hypothesis does not authorize more exclusions. `Inspect-LocalClothContacts.py` binds worst sampled gaps to canonical nodes, source faces, cache/frame hashes and oriented local normals. These samples do not prove signed solid or swept/triangle collision acceptance.

Check the native API semantics rather than assuming a vertex-group name describes behavior: Blender5.1.1's object-collision group excludes triangles whose vertices are all in that group. Record settings and which geometry this actually affects. Keep failed results and scope clarifications intact. Warmup held speed, pin correctness, surface shape and contact coverage are separate checks; an exact cache replay cannot approve bad physics.

For contact diagnosis, report indexed topology separately from canonical geometry: UV seam duplicates can produce thousands of indexed boundary edges without holes. Confirm coincident copies before canonical adjacency analysis, retain nonmanifold and cropped-boundary counts, and measure distance from the actual failing point to the geometric boundary. Check every incident simulation triangle against native exclusion semantics. A nearby indexed edge alone does not establish collider leakage. `Inspect-LocalClothContacts.py` optionally records canonical boundary distance, incident mask eligibility and the complete cached held-start speed trace without rebaking. Increasing final held RMS speed is evidence against assuming a longer identical warmup will settle; establish the cause and appropriate volume/contact representation first.
