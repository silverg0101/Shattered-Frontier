# Reproduce and repair a model without repeating discovery

Read [model-pipeline.md](model-pipeline.md) for ownership, category routing and acceptance. This runbook records the practical sequence and confirmed failure recipes from Voidreign. Resume through [model-catalog.md](model-catalog.md), then the named job and exact evidence links. A passed export check does not complete the model's other gates.

Run the [required acceptance gates](model-production-gates.md) during every batch. The author owns the visual/manual checks and linked gate summary; no separate agent review request is needed. That checklist incorporates the Greenlands findings, defines production profile requirements and calibrated reference comparisons, and identifies checks still lacking generic tooling. The Voidreign recipes below apply only after confirming their asset/profile scope.

## Working sequence

1. **Freeze the brief.** Record the selected clean image/hash, category, target dimensions, silhouette landmarks, materials, moving features, attachments and hidden-surface assumptions. Declare acceptance budgets, viewing distance/hardware and applicable gate methods. Keep the concept sheet and clean conditioning view separate. Reuse suitable existing models first. Reconcile original/prepared/effective input and tool/runtime/profile identities before a finite batch; serialize local GPU ownership. A subagent uses the same GPU as its parent.
2. **Generate once and retain expensive intermediates.** Save raw mesh, attributes/material volume, exact settings, weight/source revisions and logs. Derive comparison exports from the same geometry/UVs when evaluating texture resolution. Changing inference seed or geometry during a texture A/B invalidates that comparison. Keep all attempts in separate folders.
3. **Accept shape before skin.** Pin calibrated light/exposure/color controls and inspect front, both sides, rear, top and three-quarter views against the reference. Check category-appropriate support, orientation and distinctive forms; separate limbs, each foot's ground contact and saddle/rider fit only where relevant. A minimum-Z of zero can coexist with floating feet. Record geometry defects at geometry stage; image projection cannot supply missing structure. Do not repaint washed-out color before checking unlit texture and lighting controls.
4. **Inventory attachments before rigging.** Use `tools/Inspect-MeshComponents.py` on the exact source collection, with a recorded rigid assembly root if present. Preserve report/NPZ selections. Review positional connectivity alongside indexed connectivity: duplicated UV/normal vertices can create thousands of false island candidates. Classify component selections against the reference and intended attachment graph. Remove only confirmed debris with `tools/Repair-MeshComponents.py`, then recheck retained geometry/UVs/materials/deformation. Proximity is not attachment; a small component is not necessarily debris.
5. **Finish materials on the accepted shape.** Confirm source attributes, UV density, map hookup and colorspace before increasing pixels. Preserve UVs for controlled2K/4K tests. Render fixed closeups under neutral illumination. For image-assisted detail, provide a mesh-derived view/depth/normal guide and concept, preserve camera matrices/prompt/edit, project only visible correctly facing surfaces and rebake with padding. Repeat for hidden sides; do not stretch one side's image around the model or paste an image onto a UV atlas without registration. Evaluate base color, roughness/metallic, normal and emission separately. Color detail does not establish geometric relief or a valid normal map.
6. **Author semantic selections, then weights.** Use the selection contract below. Preserve the actual accepted mesh, fixed attachment roots and protected body/equipment. Use the feature's appropriate representation: skinned clumps, cards/groom, cloth or articulated ornaments. Do not count added diagnostic clumps as animation of the original hair.
7. **Bake and inspect motion.** Settle the simulation before the visible clip; preserve the settled state. Check startup, stop, turn, rider/environment contact and relevant extremes. Use multiple guides for independently flowing regions. Check actual deformed surfaces and segment interiors, not only particles/contact counts. Fix collider thickness, anchors, solver behavior or topology at the responsible stage. A rigid translated horse is a secondary-motion harness, not a walking/running leg rig.
8. **Reopen and reimport.** Save the editable source, reopen in a fresh process and inspect renders/playback. Import each delivered exchange into an empty scene. Compare axes, scale, UVs, textures, clip bounds, bone transforms and deformed surfaces. Record importer settings and any required adapter. Include endpoint and transition frames; seven samples missed the largest Voidreign FBX error. Preserve original failures alongside corrected evidence.
9. **Publish a scoped result.** Reconcile the complete gate summary with the job/catalog, exact paths, original/effective input and artifact/profile/tool hashes, check results, previews, defects and next action. Required missing/unimplemented checks stay open; native successes cannot override visual failures. Only required passing checks establish the requested quality target. Preserve clear labels for appearance studies, prototypes, import candidates and engine-validated assets. Engine integration belongs to its authorized session, with the surface/depth/scalability comparisons in the required gates.

## Semantic selection contract for moving features

Selection and deformation weight are separate data: an anchored root is still part of the mane/tail. Save an asset-specific selection artifact bound to the **rest mesh hash**, containing explicit vertex/face indices for:

- Positive feature seeds and the reviewed complete feature region.
- Protected head/skin/body, saddle, straps, rigid ornaments and other neighboring geometry.
- Root/attachment band, permitted feather region, and reviewed cut boundaries.
- Ambiguous fused bridges and unresolved surfaces, with the exact repair needed.

Grow seeds on a seam-aware face graph inside reviewed boundaries, stopping at protected regions. Whole-component selection cannot separate fused features. Expanded bounding boxes cannot distinguish hair from adjacent anatomy. If there is no defensible cut, rebuild/separate that local geometry and invalidate downstream selections/UV/weights as appropriate.

Save both-side, front, rear and top atlases showing feature regions, protected regions, roots, boundaries and unresolved surfaces, with closeups at attachment zones. Review these before solving weights. Check complete semantic coverage, zero protected selection, seam-consistent normalized weights, unchanged rest shape, anchored roots and actual surface behavior across the motion cases. Freeze those selections for reproduction; do not rediscover them from loose coordinate thresholds on every run. Different geometry requires a new semantic profile, rather than reusing Voidreign coordinates.

## Confirmed failures and repairs

| Observation | Diagnosis | Reproducible response |
| --- | --- | --- |
|4K looks almost like2K | Atlas dimensions increased while source material attributes contained similar detail | Keep the mesh/UV fixed, compare closeups, improve source detail/UV allocation before another upscale. See the [texture trial](../../GameClient/ArtSource/Voidreign/PixalDetail-20261018/README.md). |
| Sharp head/saddle on one side, soft elsewhere | Projection pilot covered only the visible+X regions | Use matched guides and occlusion-gated bakes for the remaining views; check seams/material channels independently. |
| Tens of thousands of disconnected islands | Indexed vertices split at normal/UV seams | Read-only exact-position connectivity plus retained raw count; do not weld or mass-delete. See [component review](../../GameClient/ArtSource/Voidreign/ComponentReview-20261018/README.md). |
| A small piece floats below the model | Could be debris or intended attached ornament | Persist mesh-scoped selection, inspect intended attachment and rest/motion evidence, then explicitly repair/rebuild/remove only confirmed defects. |
| Some hair stays rigid | Spatial masks omit lower mane/root regions | Complete semantic selections; protect adjacent anatomy, then redo weights/guides. Higher simulation resolution cannot animate unselected vertices. |
| First-frame hair jump | Simulation starts from an unsettled style | Simulate a pre-roll, retain settled initial position/velocity, inspect frame1→2 surface displacement before motion cases. |
| Bone contacts pass but hair penetrates | Center points/proxy radii do not bound actual rendered geometry | Test deformed surfaces/segments; adapt collider envelopes and topology. Contact counts alone never pass clearance. |
| FBX plays differently despite correct keys | Blender reconnects coincident rest joints and suppresses animated local translations | Preserve source connectivity in a hash-bound import contract and restore it explicitly. See the [FBX recipe](../../GameClient/ArtSource/Voidreign/ComponentReview-20261018/FbxImport-v003/README.md). Do not loosen pose tolerances or blindly disconnect unrelated rigs. |

## CPU-only exchange recipe and limits

`tools/BlenderRigImport.py capture/import` preserves source bone connectivity for one uniquely matching armature. It verifies exchange hash and hierarchy and writes a new imported `.blend`; it does not alter the FBX or guarantee another application's behavior. The Voidreign-specific `tools/Validate-VoidreignRigImport.py` checks all330 integer-frame bone transforms and sampled full surfaces, with bidirectional rest-surface coverage and coincident-source-motion checks. The linked recipe contains exact PowerShell commands. Recapture the contract after source/export changes.

The component tools and exchange checks use CPU and can run while another session owns the GPU. Existing Voidreign generation/render scripts may render with EEVEE or start inference: inspect their entry points before running under a CPU-only restriction. Use explicit CPU Cycles for material-lit rendering when required; static triangle atlases are identification aids, not equivalent beauty/occlusion reviews.

## Current Voidreign acceptance boundary

The original two marked fragments and four additional reviewed fragments have been removed; five substantial clump roots were reconstructed with retained-geometry checks. Expanded semantic selections remain provisional. The third actual-mesh rig passes startup, protected rigidity, seams and proposed attachment-gap checks; full-duration motion and surface collisions still fail. Fresh default GLB/FBX pass for the flat hierarchy. Opposite-side head/saddle color refinement preserves geometry/UV/weights/animation and non-color maps, but full PBR/coverage, stance and locomotion remain open. The actual quality job owns exact current evidence. This is a reproducible authoring/review process, not an unattended universal finishing service or completed production mount.


## Actual-mesh motion lessons from the quality revision

- Preserve substantial disconnected clumps when they contribute the intended silhouette. Review their root-facing endpoints and reconstruct narrow continuations explicitly; debris removal is a different decision. Count components after repairs, but classify their purpose rather than treating the count as a defect total.
- Keep source-hashed semantic membership distinct from moving weights. Review gray locks as either anchored roots or omitted features. Protect rump anatomy and saddle core before guide rooting; a guide inside a body capsule can have mathematically incompatible length/contact constraints.
- Initialize bone orientation from authored rest roll and transport it continuously between frames. Tracking against global up independently each frame can flip roll as a segment crosses vertical, even with smooth particle positions.
- A virtual attachment graph is insufficient by itself: nearest-guide weights can pull its endpoints apart. Give a reviewed clump-root band coherent weights, preserve protected anatomy, and check actual evaluated endpoint gaps over every frame. Candidate relations remain unapproved until semantic review.
- Fit contact envelopes to the skinned surface extent, not only guide-center radii. Recheck actual surface vertices against the unchanged clearance limits after every guide/weight/collider change. Report root exemptions and proxy limitations; do not hide failures by expanding exclusions or changing tolerances.
- Test fresh default FBX import first. Flat independent deform joints avoided the earlier coincident-joint reconnection problem; only require the connectivity adapter when the exact current exchange needs it.
- Resolve all output paths before opening a Blender file and assert rendered files exist. Blender's saved project directory can otherwise redirect relative output paths.
- Snapshot exact tool/profile inputs with each attempt, retain failed attempts, and stop a stage at its declared repair budget. A passing import or particle bake cannot promote a revision whose surface gate fails.

The current [quality revision](../../GameClient/ArtSource/Voidreign/QualityCompletion-20261018/README.md) records concrete source hashes, revisions, limitations and reproduction order. Its job record supersedes the historical benchmark for current work.

## Full-duration particle motion and support checks

### Review hidden shells before expanding weights

After seam-aware component inventory, use `Inspect-ComponentVisibility.py` with its pinned visibility profile. The current revision samples triangle interiors by area in14directions, records first-hit component IDs, and includes a target-only BVH control. A missed control stays unresolved. Save visibility separately from semantic purpose and deletion disposition. The [component ledger review](../../GameClient/ArtSource/Voidreign/QualityCompletion-20261018/ComponentLedgerReview-v002/component-review-ledger.json) links each exact ID directly to its inventory, feature counts and candidate binding.

Through-context atlases show buried geometry too. Confirm candidates with depth-tested and isolated/section views, known visible controls and relevant motion extremes. Hidden shells can contaminate hair selections or collider/root constraints, but low visibility alone never justifies deletion: internal anatomy, nested ornaments and clumps exposed by deformation may be intended. The Voidreignv002 check reports180 low-rest-visibility candidates and18control-miss components; these are review classes, not defect totals. Preserve the earlier194/219 six-direction vertex-sample result as historical evidence.

The third simulation's stretch/startup/root/contact checks pass, yet `SurfaceDiagnosis-v001/trajectory-diagnosis.json` identifies a154mm guide-center step at frame210, corresponding to the failed156mm mesh step. A particle pass limited to startup and stretch does not bound playback velocity. A reviewed next profile must check full-duration guide steps/velocity/acceleration and contact transitions before another rig bake. Read-only diagnosis is not another repair attempt. Ground/neck/far-arm surfaces also extend beyond assigned guide segments; nearest spatial guides are insufficient for all strand shapes.

`Inspect-AssetSupport.py` records source-hashed authored support candidates. Distinguish hoof/sole/foot regions from hair, ornaments and debris before choosing a ground plane; never use global minimum height alone. Raised supports may be intentional pose. Review the intended rest/idle pose and preserve proportions when authoring a leg rig/contact correction; invalidate downstream evidence if geometry or animation changes.

## Color refinement invariants

`Prepare-AssetTextureGuides.py` renders fixed REST-pose unlit views on CPU. Pin source hash, camera matrices and authored paint regions, then generate registered edits with the imagegen workflow. `Project-AssetTextureDetails.py` gates projection by region, normal facing and BVH visibility, bakes4K color and restores unpainted atlas pixels. `Validate-AssetMaterialRevision.py` freshly compares geometry, UVs, weights, non-color images,330 bone frames and19 evaluated surfaces, plus unpainted atlas pixels. These sample counts belong to this Voidreign adapter; other assets need appropriate profiles.

Inspect matched and opposing lighting (`Render-AssetDetailViews.py`), oblique views and mask transitions. Sharper painted chains, cracks or eye sockets do not create geometry, normal detail or emission. A pretty closeup cannot accept lighting baked into base color, incomplete PBR channels or hidden-side seams. Preserve generated prompts/pixels/hashes: stochastic image generation is not bitwise reproducible; reuse the frozen edits for deterministic projection.

## Copyable model-batch session prompt

```text
Work in C:\Projects\Shattered-Frontier. Read docs/dependency-map.md first,
then docs/art-templates/model-catalog.md, model-pipeline.md,
generate-model.md, model-quality-runbook.md and model-production-gates.md.
Execute the required gates during this batch, including author visual/manual
inspection; do not wait for a separate agent review request. Follow the selected milestone
handoff and geometry authority. Use direct catalog/job/package links;
do not reconstruct the queue by grepping folders.

Choose a finite ready batch with stable asset IDs. Reuse suitable existing
models first. Claim each job with an owner/lease and preserve another session's
active jobs. Use the verified local Pixal3D backend and the job's pinned
generation profile for approved image-to-3D work; serialize GPU ownership.
Use clean single-asset references with saved provenance. Surface treatments
follow the material workflow; structural solids follow approved GEO-01
manifests/hashes and applicable GEO-02 fit evidence.

For each asset, declare acceptance budgets and pin calibrated render settings.
Reconcile original/prepared/effective input, runtime/lock/profile/tool hashes
before launching. Save immutable attempts and linked gate summaries that
distinguish tool, author-visual, manual and unimplemented checks.
Inspect rest geometry and both raw/seam-aware components; classify intended
attachments versus debris, and repair only reviewed source-hashed selections.
Persist complete semantic feature/protected/root selections where motion is
required. Skip irrelevant stages with a reason. Check actual deformed surfaces
and attachments across the required cases, plus saved-source renders and
fresh exchange imports with explicit geometry/semantic/attribute correspondence.
Use material/depth comparisons and authorized UE/hardware evidence for engine
claims. Particle contacts and successful exports cannot clear
missing or failing quality gates. Verify texture channels, mask transitions,
unpainted areas and material behavior under changed lighting.

Keep finite repair budgets; diagnose owning failures without loosening limits
or silently resetting attempt counts. Preserve failed evidence and mark
needs-review on exhaustion, then continue another ready asset/stage. Update
the exact job/catalog with previews, checks, defects, blockers and next action.
Distinguish prototypes, import candidates and engine-validated assets.

Deliver visual assets only. Do not change Unreal code/maps, gameplay, live
servers, dependencies/environments or commit/push under this prompt.
```

Add the selected asset IDs and milestone handoff to this prompt. It does not authorize a universal geometry/rig inference adapter; asset-specific tools need a reviewed profile and explicit applicability before reuse.

### Posed visibility correspondence

`Export-DeformedComponentInspection.py` evaluates explicit frames on the actual animated source, preserving exact rest vertex/face component IDs in a unit-scale assembly frame. Only armature deformation is supported. Run the visibility sampler on each exported pose. `PosedVisibilityComparison-v001` records31–32 rest-occluded components becoming sampled-visible after pre-roll or at tested motion extremes. This demonstrates why static occlusion is insufficient for deletion. A failed rig can also expose buried shells unintentionally; establish semantic purpose before deciding whether the exposure is desired. The first posed sampler reports retain an old “rest” status label; their explicit Frame-* inventory and comparison identify the actual poses. The current tool reports frame/pose explicitly.

## Exact-degenerate hygiene before semantic rig work

Run `Inspect-MeshTriangleQuality.py` on the source-hashed inventory before rigging. Preserve exact triangle selections and diagnostic threshold/profile units. `duplicate_position_faces` counts all participating faces, not removable redundancies; canonical edge incidences can include collapsed edges. None of these totals proves an automatic repair.

An exact-zero triangle is a stronger case than a small-area threshold. Verify triangle-to-polygon correspondence, then confirm the face stays collapsed in every required pose: different weights can open coincident rest vertices. For a narrow reviewed repair, remove only listed faces and resulting unused elements; refuse unsupported pre-existing loose geometry or shape keys. Preserve new-to-source vertex/face/loop remaps and check retained geometry, normals, UVs, colors, materials, weights, texture pixels and animation in a fresh process. Reimport new exchanges. Never reuse old vertex IDs, triangle counts or source hashes after cleanup.

The Voidreign hygiene review identifies six inherited exact-degenerate faces and10resulting unused vertices. This is geometry attempt2; the exhausted root-construction/rig/solver records remain unchanged. A successful narrow cleanup would not accept duplicate shells, nonmanifold geometry, semantic coverage, collision or production.

## Coincident surfaces and full-duration guide motion

After triangle hygiene, compare coincident-position groups with `Inspect-CoincidentFaceGroups.py`. Check corner correspondence, winding, every UV layer, normals, material, weights and colors; preserve the exact source and group IDs. Opposite winding or different UVs can represent intentionally distinct surfaces. Even exact rest attributes do not prove equivalent shader, posed or visual behavior. Current Voidreign:2,254participating faces in1,124groups; all groups differ in winding/normals,1,122in UVs,66in colors, and none yields an exact-attribute redundancy candidate. Preserve them pending semantic review; this is not deletion approval or proof they are intentional.

Run `Check-FeatureTrajectory.py` on each saved guide bake before rig or surface review. Its typed pinned profile checks the entire saved duration at the saved frame rate, excludes padded nodes, verifies guide/config inputs and fingerprints all read inputs. The80mm warning ceiling uses the existing surface limit numerically; guide displacement is not surface displacement. Report world and body-relative steps, interval-average speed and finite-difference acceleration. Speed/acceleration remain diagnostic until a reviewed policy exists. Do not smooth, clamp or change timing to hide a failed trajectory. A guide pass cannot accept actual surfaces, contacts, attachments or runtime motion.

Voidreign `TrajectoryCheck-v002` finds118node-step warnings across33frames (175–213),116during turn and2during stop, restricted to Tail_G00/G04/G11. Peak154.968mm atframe210/Tail_G00/node11. This reads the existing third bake without spending/resetting another attempt. The historical simulation report did not store a trajectory-file hash; the new check fingerprints the file it reads, without claiming original producer attestation. Existing exact reproduction evidence remains separate.

## Diagnose and revise contact motion without blind retries

Use `Trace-FeatureGuideMotion.py` to replay a frozen solver with observation hooks. It rejects a source adapter mismatch and must reproduce all saved output-frame points, root transforms, rest points and counts exactly. Review substep prediction, structural, individual capsule and ground position changes; these are interacting displacements, not independent forces or energy attribution. Freeze the trace window/guide IDs and exact instrumented code. A point moving quickly after leaving an obstacle does not establish which earlier correction or force drove it.

The proposed `FeatureContactVelocity.py` separates positional depenetration from velocity. Form a candidate using prediction plus internal-constraint corrections, excluding summed contact depenetration; project approaching velocity relative to active colliders onto frictionless, zero-restitution normal constraints. Preserve tangential motion for a single contact and solve simultaneous normal inequalities iteratively. Report residual failure/infeasibility. Never globally zero velocity, differentiate changing closest-point parameters, or add speed clamps to conceal instability. Its capsule adapter measures endpoint-interpolated axis motion only; actual radial material rotation needs a fuller collider adapter.

`Study-FeatureContactVelocity.py` currently passes14analytic velocity fixtures (static plane rest/approach/slide, separating motion, initial overlap without launch, capsule flank/endcap normals, moving plane, corner, contradictory moving walls, fixed-parameter axis motion and undefined-normal rejection). This is not a position, strand, root, skin or collision-system pass. Before integration, add coupled-strand release, positional feasibility and finite-width/swept cases. If the source stage is exhausted, retain all failed attempts and review a distinct design study; a renamed output is not a budget reset. Single-variable damping/spring experiments must keep guides, colliders, timing, solver and acceptance limits fixed and record remaining contact and style failures.

## Guide-to-skin coverage and fixed-root feasibility

Before increasing contact envelopes or solver iterations, measure the actual surface's distance to its candidate guide segments. `Check-FeatureGuideClearance.py` separates free-node residuals from pinned/root envelope conflicts and validates the complete saved array shapes, finite transforms, rest guide identity, translation and yaw. A pinned node cannot resolve a moving collider by moving; report the fixture/root/envelope conflict explicitly. Distinguish envelope overlap from penetration into the collider's solid. Do not automatically exempt roots or change the physical collider just to pass.

Voidreign free guide nodes have only0.014mm maximum saved-frame envelope residual, while actual skin penetrates up to69mm. The hand-probe envelope overlaps Mane_G10/node0 by22.09mm atframe301. These are separate problems. A point-contact pass cannot accept a finite-width clump, skin weights, rooted feasibility or rendered surfaces.

`Propose-FeatureCoverageGuides.py` computes rest nearest-segment distances and proposes additional paths on the existing rooted graph, preserving source hashes and virtual attachment provenance. The root-band exclusion is pinned to the existing surface profile, not enlarged. In CoverageGuideStudy-v002,24additional proposals reduce worst mane distance236→139mm and tail319→172mm. Still roughly49% of evaluated mane and48% of tail nodes exceed the configured50mm diagnostic warning. This is evidence to review clump correspondence and representation, not approval to double the runtime skeleton. Mesh-surface paths can cross fused unrelated locks or unapproved virtual attachments; smaller Euclidean distance is not better deformation by itself.

Use `Render-FeatureCoverage.py` for depth-tested rest comparisons bound to the report and its NPZ hash. Gray is outside selected features; blue is protected/excluded root band; green<=50mm,amber50–100mm,red100–200mm,magenta>200mm. Interpolated vertex colors are not an area-complete distance field. Blue is not semantic approval. The candidate source remains untouched.

Two reviewed single-variable trajectory diagnostics also show why tuning alone is insufficient: drag5→12 reduces peak155→95.6mm but increases turn/stop lag; spring85→30 reduces peak to94.2mm but nearly doubles settled RMS style deviation87→172mm. Both exceed the unchanged80mm guide warning. Preserve these failed comparisons and their exact frozen code/profiles; do not combine them automatically or select a profile on speed alone. Current contact-velocity fixtures include an oblique contact-order case: sequential normal projection is feasible when converged but order dependent and not a minimum-impulse solver. Its15analytic passes do not approve coupled strands or actual-mesh collision.
