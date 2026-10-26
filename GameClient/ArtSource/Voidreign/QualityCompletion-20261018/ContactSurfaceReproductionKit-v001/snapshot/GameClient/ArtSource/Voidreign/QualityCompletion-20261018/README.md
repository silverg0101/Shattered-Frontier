# Voidreign actual-mesh quality revision

CPU-only authoring continuation, 2026-10-18. Current state and exact next action live in [the quality job](../../../../docs/art-templates/model-jobs/voidreign-quality-20261018.json). This revision is a **diagnostic prototype under review**, not a production mount or accepted collision/runtime asset. The separate scenery session retains local GPU ownership. The user's existing unsaved Blender window is preserved.

## What changed

- Removed four reviewed floating arcs/slivers from the earlier cleanup: source mesh`0eb7e75e610e`, componentsC0080/C0109/C0091/C0156. Removed210vertices/182triangles. All retained positions, faces, UVs, materials and seven sampled deformations are unchanged.
- Preserved five larger, silhouette-bearing clumps and authored tapered scalp/rump continuations toward their root-facing endpoints. `RootRepair-v003` adds297vertices/624triangles and replaces exactly10cap faces; all existing vertices and other faces/UVs/materials remain unchanged. Fresh checks confirm all five clumps join the main positional component; count224→219. Their root shading/intersections still require closeup review. New root triangles intentionally reuse the child-cap UV patch; this is not new detailed texture painting.
- Froze expanded mane/tail semantic selections and protected anatomy/tack with source/mesh hashes. Selection is separate from motion weight: roots can remain anchored. A later rig guard protects neck interior and the breast-collar boundary.
- Built a24-guide/229-bone rig on the actual repaired mesh, with at most four influences. About45,524mane and66,912tail vertices carry motion;137,820explicit protected vertices have zero hair weight. A flat deform hierarchy preserves independent transforms for exchange, while logical guide chains remain recorded in metadata and JSON.
- Added settled simulation pre-roll. The second particle bake passes its fixed stretch/startup/contact checks:3.47%maximum segment error,0.206mmmaximum initial particle step, and rider-hand/post/ground contacts. These do not establish rendered-surface clearance.
- Saved a second rig using temporal parallel transport initialized from rest roll. The first rig's global-up tracking caused a104°roll jump near a vertical tail segment; its actual surface checks correctly failed despite particle checks passing.

## Direct evidence

| Folder | Purpose |
| --- | --- |
| `Cleanup-v002/` | Four-fragment removal, source/exports and exact retained-mesh checks. Default old-rig FBX import still fails; `ImportCheck-v001/` checks the source connectivity adapter. |
| `Inspection-v001/` |224component inventory after removal, before root continuations. IDs are scoped to mesh`39b034dd77a9`. |
| `RootRepair-v001/`, `RootRepair-v002/` | Failed before saving source: NumPy/Vector scalar mismatch, then duplicate cap selection guard. Reasons/log pointers preserved. |
| `RootRepair-v003/` | Working root-repair source, exact changes and fresh `root-validation.json`; existing deformed vertices match at seven frames. |
| `RootRepairInspection-v001/`, `RootRepairViews-v001/` |219component inventory and CPU Cycles rest views of repaired geometry. |
| `SemanticSelection-v001/` | Expanded explicit selections on repaired mesh`f580ad6893e5`; source-hashed masks, roots and candidate surface paths. |
| `AttachmentProposals-v001/` |133same-feature virtual binding proposals. No unresolved>25mm forest gap remains after the reviewed geometry repairs, but none of these proximity proposals grants semantic acceptance. |
| `GuideProposal-v002/` |24guide proposals, root-distance weight field and protected-motion regions. |
| `Simulation-v001/`, `Simulation-v002/` | First failed stretch bake and corrected particle bake, preserved trajectories/config hashes. |
| `CompleteHair-v001/`, `CompleteHair-v002/` | Actual-mesh source/GLB/FBX candidates, rig reports. v002 corrects arbitrary roll resets; use job/checks to select the current candidate. |
| `CompleteHairImport-v001/` | v001 GLB and default FBX pass full330-frame bone and18-frame surface comparison with connected4K maps. Its summary string unnecessarily says “required contract”; detailed default-import result passes with no restored joints. This report label is a checker defect, not a reason to apply the adapter to this flat rig. |
| `CompleteHairPreview-v001/` | Six CPU diagnostic source frames from v001, with explicit rigid-harness/under-review labels. These are not v002 acceptance previews. |
| `SurfaceCheck-v001/`, `SurfaceCheck-v002/` | Actual evaluated mesh checks, not just particles: protected rigidity, seams, startup/all-frame steps, proposed binding gaps and capsule/ground penetration. Read measured failures before any promotion. |

Earlier anatomical stance, opposite-side head/saddle texture coverage and PBR-detail limits remain open. The legs remain static in a rigid-motion harness. Capsule tests are approximations, exclude a declared root band, and do not cover triangle-accurate/swept/self collision. Engine behavior is untested.219components does not mean219defects; their semantic attachment dispositions remain part of review.

## Reproduction order

Run from `C:/Projects/Shattered-Frontier` with **new output folders** and absolute Blender output paths. No dependency installation is needed for these CPU tools. Windows Python3.12 supplies NumPy/SciPy/Pillow; Blender5.1.1 supplies its own NumPy.

1. `Inspect-MeshComponents.py` → `Plot-ComponentAtlas.py`: freeze source/component selections and inspect IDs in several views. `Render-AssetSemanticViews.py --component-inspection ... --component N --view ...` provides ordinary depth-tested material/ID closeups; triangle atlases draw through context and cannot prove occlusion.
2. `Repair-MeshComponents.py` → `Validate-VoidreignComponentRepair.py`: remove only explicitly reviewed whole components into a new revision. Preserve the failed default-FBX result and its distinct import correction.
3. `Repair-HairClumpRoots.py --source <cleanup.blend> --inspection <cleanup-inventory.json> --profile docs/art-templates/model-profiles/voidreign-clump-root-repair-v002-20261018.json --output <new-root-folder>`; then inventory and `Validate-HairRootRepair.py --folder <root-folder> --inspection <new-inventory.json>`. This is an authored repair adapter, not automatic inference of missing roots for arbitrary models.
4. `Render-AssetSemanticViews.py` for the repaired source; `Build-MeshFeatureSelection.py` with its exact view metadata, inventory and `voidreign-hair-selections-v003-20261018.json`. Recompute after topology changes; never reuse stale vertex/component IDs.
5. `Propose-FeatureAttachments.py` with `voidreign-feature-binding-20261018.json`; inspect each intended relation and its path to a real root. `Build-FeatureGuides.py --allow-proposed-bindings` explicitly creates a diagnostic experiment, not final attachment approval. Preserve the proposal status through all later reports.
6. `Simulate-FeatureGuides.py` with `voidreign-complete-hair-motion-v002-20261018.json`. It is pure CPU NumPy. Inspect particle limits and required contact cases before rig baking. If a guide runs through protected anatomy, correct the root/selection/collider fit; increasing solver iterations cannot solve an impossible constraint.
7. `Build-FeatureMotionRig.py` with the matching guide report, simulation folder and pinned motion profile. It rejects mismatched hashes/failed particle checks and preserves actual rest geometry. Its copied tool/profile source records this build implementation.
8. `Check-FeatureMotionSurfaces.py` with `voidreign-hair-surface-check-20261018.json`. Inspect worst frame/vertex/binding IDs and `surface-review.npz`. Fix actual surface failures at their owning selection, guide, weight, orientation or collider stage; do not loosen the check limits to clear a report.
9. Capture `BlenderRigImport.py` contract for the exact new source/FBX; run `Validate-VoidreignRigImport.py --name Voidreign-CompleteHair` with the matching triangle/frame profile. Check default FBX first; the new flat hierarchy may need no connectivity restoration. A contract never establishes other applications' behavior.
10. `Render-FeatureMotion.py` reopens the saved source with CPU Cycles and renders specified frames or a fixed stride, with optional `--diagnostic`. Inspect ordinary material and weight views. For a stride3/30fps source, encode the sequential PNGs at10fps for an11second diagnostic; render stride1 for a30fps final review. Save exact FFmpeg command and source-frame mapping.

Blender render paths must be absolute before loading the source. An early preview wrote six images under`C:/GameClient/...`; they were moved individually into the intended project folder, and the renderer now resolves/asserts the destination. A process exit code alone never proves requested artifacts reached the requested path.

The [quality runbook](../../../../docs/art-templates/model-quality-runbook.md) owns general lessons; [model-pipeline.md](../../../../docs/art-templates/model-pipeline.md) owns workflow gates. These per-asset cuts, roots, collider shapes and test phases require new authored profiles on other geometry.

## Third bounded correction and reproduction

`CompleteHair-v003` uses `GuideProposal-v003`, `Simulation-v003` and `voidreign-complete-hair-motion-v003-20261018.json`. It protects rump/saddle-core anatomy, couples proposed clump-root weights and adds declared surface envelopes. Particle stretch is2.80%, warmup settled, initial particle step0.053mm. Actual proposed attachment-gap increase is0.044mm (previously136mm); protected rigidity/seams/startup pass. **Actual maximum surface step156mm and capsule/ground penetration68.7mm still fail**. Neck/far-arm/post also retain measured penetrations. The three-attempt rig/simulation budget is exhausted; review strand-local skin mapping and surface-contact representation before another design revision. Do not loosen the check limits.

`CompleteHairImport-v003` passes default GLB and FBX with no restored joints. The corrected validator summary reports `passed-default-imports`; the earlier report remains unchanged.

Reproduce this locked diagnostic from repository root into a new directory:

```powershell
py -3 tools/Run-VoidreignQuality.py --output Artifacts/Art/Voidreign/quality-completion-20261018/Reproduction-NEW --render
```

The serialized CPU adapter verifies exact source/selection/attachment/profile/tool hashes and Blender5.1.1. It starts from reviewed rest/selection artifacts; it does not infer new semantics or start a GPU generator. Read `run-report.json`: process completion is not acceptance. Failed surfaces remain recorded while diagnostic exchanges/previews continue. `ReproductionEvidence-v001/` preserves the exercised command chain and native reports: all six surface measurements match exactly, including the failures. Full local run outputs remain under ignored `Artifacts/`. This replays a fixed attempt, not a fourth repair. Change pinned inputs only in a new lock revision.

`CompleteHairPlayback-v003/Voidreign-CompleteHair-Diagnostic.mp4` is110 source renders at10fps,11seconds, explicitly labelled as actual mesh/rigid harness/under review. The render report maps every output to its source frame. Encode without changing playback timing:

```powershell
ffmpeg -hide_banner -loglevel error -n -framerate 10 -i GameClient/ArtSource/Voidreign/QualityCompletion-20261018/CompleteHairPlayback-v003/%04d.png -c:v libx264 -preset medium -crf 19 -pix_fmt yuv420p -movflags +faststart GameClient/ArtSource/Voidreign/QualityCompletion-20261018/CompleteHairPlayback-v003/Voidreign-CompleteHair-Diagnostic.mp4
```

## Opposite-side texture refinement

`MaterialGuides-v001` freezes REST-pose unlit cameras/source hash. `MaterialEdits-v001` contains built-in imagegen edits, exact prompts/hashes and the edit manifest. `OppositeDetail-v001` projects only facing, BVH-visible vertices inside authored head/saddle regions, baking into existing4K UVs. Pixels outside the projection mask are restored from the original atlas. `OppositeDetailCheck-v001` freshly confirms exact geometry/UV/weights/non-color maps, zero330-frame bone/19-frame surface differences and zero change across15.4million tested unpainted pixels. `OppositeDetailImport-v001` passes animated default GLB/FBX. Inherited hair failures remain unchanged.

`MaterialBefore-v001`, `MaterialAfter-v001` and `MaterialOpposingLight-v001` show actual lit source closeups under matched/changed lights. `ReviewEvidence-v001` preserves component dispositions and independent edit review. Generated fine chains/rivets/highlights are color detail only and include painted lighting. The empty eye socket, incomplete PBR channels and coarse/faceted geometry remain. This is a diagnostic color refinement.

Texture reproduction, using new output directories:

```powershell
$blender = 'C:/Program Files/Blender Foundation/Blender 5.1/blender.exe'
$asset = 'C:/Projects/Shattered-Frontier/GameClient/ArtSource/Voidreign/QualityCompletion-20261018'
& $blender --background --factory-startup --threads 4 --python-exit-code 1 --python tools/Prepare-AssetTextureGuides.py -- --source "$asset/CompleteHair-v003/Voidreign-CompleteHair.blend" --profile docs/art-templates/model-profiles/voidreign-opposite-texture-guides-20261018.json --output "$asset/MaterialGuides-NEW"
# Review/pin new image edits, or replay the exact saved edits below.
& $blender --background --factory-startup --threads 4 --python-exit-code 1 --python tools/Project-AssetTextureDetails.py -- --source "$asset/CompleteHair-v003/Voidreign-CompleteHair.blend" --guides "$asset/MaterialGuides-v001/projection-guides.json" --edits "$asset/MaterialEdits-v001/edits.json" --output "$asset/OppositeDetail-NEW"
& $blender --background --factory-startup --threads 4 --python-exit-code 1 --python tools/Validate-AssetMaterialRevision.py -- --source "$asset/CompleteHair-v003/Voidreign-CompleteHair.blend" --candidate "$asset/OppositeDetail-NEW/Voidreign-CompleteHair.blend" --object geometry_0 --mask "$asset/OppositeDetail-NEW/textures/ProjectionMask-4K.png" --output "$asset/OppositeDetailCheck-NEW"
```

Image generation is stochastic: replaying a prompt does not guarantee identical pixels. Reusing saved edits/hashes makes the projection repeatable. Recapture/validate exchanges for each new saved source.

## Support and next design work

`SupportInspection-v001` measures authored candidate hoof bounds on the main component: front minima103mm/86mm above the plane; rear minima8mm/3mm. These candidate regions need sole-boundary/pose review; this is not automatic hoof detection. Lower tail strands influence global minimum height, so a bounding-box translation cannot ground all four feet. Establish intended rest/idle pose and leg rig before correcting individual contacts, preserving proportions and invalidating downstream evidence explicitly. This harness never animates a gait.

## Internal-shell and component ledger investigation

`ComponentLedgerReview-v002/component-review-ledger.json` is the finite219-ID review queue, with exact semantic counts, candidate bindings, bounds and visibility evidence. The earlier atlas-based observations were provisional; for example C0018 lies at the flank rather than shoulder, and C0026 is not proven omitted visible hair.

`ComponentVisibility-v001` used six directions and indexed vertices (194 low-visibility results). Independent review prompted `ComponentVisibility-v002`: area-stratified triangle interiors,14directions, target-only BVH controls and first-hit component IDs. It reports180 low-visibility candidates,18control-miss cases unresolved,21other visible candidates. C0001/C0004 serve as visible controls. C0005/C0012/C0018/C0020/C0025/C0026 have1792/1792target-only control hits but no full-mesh first-hit exposure in rest. `EyeCandidateReview-v001`/`EyeCandidatePositive-v001` confirm no C0012 highlight in their tested views; it must not be repurposed as an eye merely from an x-ray atlas. `ComponentVisibilityRender-v001` highlights the original194-candidate set with normal depth occlusion. No geometry was deleted.

`PosedComponentInspection-v001` and `PosedVisibility-Frame0001/0210/0300-v001` evaluate the actual third rig. `PosedVisibilityComparison-v001` finds31–32 previously occluded parts becoming visible in these poses. C0020 exposes a small area; C0005/C0012/C0018/C0025/C0026 remain sample-occluded. Visibility is distinct from purpose, physical containment and deletion disposition. A faulty rig can expose buried shells unintentionally. Review isolated/section views and relevant deformations before geometry or weight changes. These checks justify an internal/overlap-shell gate before further rigging, not a mass cleanup.

The first posed sampler reports retain a historical “rest” label; their Frame-* inventories and comparison identify actual poses. Current sampler output explicitly records frame/pose. ReproductionKit-v001 is a frozen earlier snapshot; later detector/ledger work is preserved in its supplemental kit rather than mutating that manifest.

Paired isolated/depth-tested controls are saved in `HornCoreIsolation-v001` and `Isolation-C0012-v001`/`Isolation-C0026-v001`. `ReproductionSupplement-v001/manifest.json` freezes the later detector/ledger/pose evidence separately; both kit hash manifests verify without mismatches. See [the next revision proposal](NextRevisionPlan-v001.md) for the concrete geometry, strand-local skin, full-duration contact, material and pose gates before another bake. It does not reset the exhausted attempts or approve production.

## Exact-degenerate cleanup — current saved candidate

Current source is `DegenerateCleanup-v001/Voidreign-CompleteHair.blend` (mesh `ba2d283db0be`,284,762vertices/297,119triangles). Geometry attempt2 removes six independently reviewed exact-zero-area triangles and ten resulting unused vertices. It confirms those faces stay collapsed for all330frames, maps triangles to polygons explicitly, and refuses unsupported pre-existing loose geometry or shape keys. No hidden shell, duplicate face or nondegenerate geometry was removed. Root/rig/solver attempt counts remain3.

`DegenerateCheck-v001` freshly confirms exact retained positions/faces/UV/colors/weights/normals/connected texture pixels, zero330-frame bone changes and zero19-frame retained surface changes. `DegenerateImport-v001` passes default GLB/FBX. Matched `HygieneMaterialReview-v001` head/saddle renders are pixel-identical to the preceding material candidate (`HygieneRenderComparison-v001`). Existing unsaved GUI source is preserved; it is not automatically replaced by this new file.

`TriangleQuality-v001`/`TriangleBaseline-v001` establish that the six degenerates were inherited. `TriangleQualityAfter-v001` finds zero remaining degenerate/collapsed triangles, one high-aspect candidate,2,254participating duplicate-position faces, one boundary-position edge and3,552nonmanifold edge incidences. These are diagnostic classifications, not removable counts or an engine budget. Changed edge totals partly reflect removed degenerate incidences.

Topology IDs were refreshed rather than reused. `DegenerateInspection-v001`, `HygieneSemanticSelection-v001`, `HygieneAttachmentProposals-v001`, `HygieneGuideProposal-v001` and `HygieneComponentReview-v001` bind to the new mesh. Explicit labels/protection/roots equal the old selections through the deletion correspondence; guide rest coordinates also match exactly. No new simulation or rig bake was performed. The219components map one-to-one to their historical IDs, while classifications remain under review. Fresh `HygieneVisibility-v001` still reports180low-visibility candidates/18unresolved controls. Fresh `HygieneSurfaceCheck-v001` reproduces all six earlier surface measurements exactly, including the failures. This validates the narrow cleanup without hiding pending work.

Reproduce only the reviewed hygiene and fresh retained-source comparison into new folders:

```powershell
$blender = 'C:/Program Files/Blender Foundation/Blender 5.1/blender.exe'
$asset = 'C:/Projects/Shattered-Frontier/GameClient/ArtSource/Voidreign/QualityCompletion-20261018'
& $blender --background --factory-startup --threads 4 --python-exit-code 1 --python tools/Repair-DegenerateAssetFaces.py -- --source "$asset/OppositeDetail-v001/Voidreign-CompleteHair.blend" --profile docs/art-templates/model-profiles/voidreign-degenerate-hygiene-20261018.json --output "$asset/DegenerateCleanup-NEW"
& $blender --background --factory-startup --threads 4 --python-exit-code 1 --python tools/Validate-AssetGeometryCorrespondence.py -- --folder "$asset/DegenerateCleanup-NEW" --profile docs/art-templates/model-profiles/voidreign-degenerate-hygiene-20261018.json --output "$asset/DegenerateCheck-NEW"
```

Then capture the exact new FBX import contract and use `voidreign-degenerate-import-20261018.json` for exchange checks. Rebuild source-bound inventory/semantic/attachment/guide metadata; do not point old vertex/node IDs at the new mesh. Keep unresolved semantic and motion statuses throughout.

## Further read-only findings — no geometry or bake change

`CoincidentFaceReview-v001` finds2,254position-coincident participating faces in1,124groups. Every group differs in winding and corner normals;1,122differ in UVs,66in colors. No exact-attribute redundant-face candidate remains. Preserve all pending semantic/posed review. Material backface culling is enabled; removing reversed faces could change visibility. This does not establish intentional geometry or approve the unresolved shell inventory.

`TrajectoryCheck-v002` reads Simulation-v003 without resimulation:118guide-node warnings across33frames,175–213, limited to Tail_G00/G04/G11.116occur during TURN RESPONSE,2during STOP / SETTLE. Peak world step154.968mm atframe210/Tail_G00/node11; peak output-frame acceleration67.910m/s2 atframe189/Tail_G04/node11. The80mm warning uses the fixed surface ceiling numerically; guide and surface displacement remain distinct. Acceleration/speed are diagnostics, not new acceptance thresholds. v001 is retained with an overly strong limitation sentence; v002 corrects it, numeric results unchanged. Neither report changes saved sources or stage attempts.

Read-only reproduction (use a fresh output directory):

```powershell
py -3 tools/Check-FeatureTrajectory.py --simulation GameClient/ArtSource/Voidreign/QualityCompletion-20261018/Simulation-v003 --profile docs/art-templates/model-profiles/voidreign-guide-trajectory-check-20261018.json --output <new-trajectory-check-folder>
& 'C:/Program Files/Blender Foundation/Blender 5.1/blender.exe' --background --factory-startup --threads 4 --python-exit-code 1 --python tools/Inspect-CoincidentFaceGroups.py -- --source GameClient/ArtSource/Voidreign/QualityCompletion-20261018/DegenerateCleanup-v001/Voidreign-CompleteHair.blend --inspection GameClient/ArtSource/Voidreign/QualityCompletion-20261018/DegenerateInspection-v001/component-report.json --object geometry_0 --output <new-face-review-folder>
```

The new trajectory report fingerprints the NPZ it actually reads; the original simulation report did not bind that file by hash. No retrospective original-producer attestation is claimed. Fresh actual-surface failures remain authoritative.

## Hair/contact continuation — current diagnostic evidence

The saved candidate above is unchanged. `TurnTrace-v001` exactly replays all saved points/transforms/rest/counts with observation hooks, recording14,267substep events around three tail guides at5.5–7.2seconds. Prediction displacement reaches19.84mm/substep, structural2.11mm, post7.53mm and ground3.95mm. These maxima are not simultaneous or causal energy attribution.

`ContactVelocityStudy-v002` passes15analytic checks for the proposed split-contact velocity module. It preserves single-contact tangential motion and reports contradictory constraints; oblique contact order dependence is explicitly demonstrated. The axis-rotation fixture now preserves length. This module is not installed in the model's solver and does not solve contact discovery, release, fixed-root feasibility, coupled strands or finite-width skins.

`MotionDesignStudy-v001/README.md` and `study-results-v002.json` preserve the two reviewed single-variable full-duration trajectory experiments. Drag12 peaks95.6mm;Spring30 peaks94.2mm,versus baseline155mm. Both still fail80mm; both increase turn/stop style lag, and Spring30 nearly doubles settled deviation. No combined case or delivered rig accepted. Old topology/rig/secondary-motion attempts remain3.

`GuideClearanceBaseline-v004` validates array/rest/translation/yaw consistency and finds only0.014mm free-guide residual, but22.09mm fixed-root hand-envelope conflict atframe301/Mane_G10/node0. Fixed feasibility is explicitly needs-review. This is distinct from actual-skin penetration up to69mm. Earlier baseline clearancev001/v002 had overly broad status wording;v003 flags fixed feasibility;v004 adds the full shape/rest/yaw assertions, with identical measurements. Candidate clearance-v002 reports apply those assertions to both tuning studies.

`CoverageGuideStudy-v002` and `CoverageViews-v002` bind exact source/mesh/report/output-array hashes.24extra unreviewed path proposals improve worst rest distance:236→139mm mane,319→172mm tail. Still49%mane/48%tail evaluated nodes exceed50mm. Source, weights and animation are unchanged. Review clump/path identities and missing representation before any new skin/solver bake; the runtime bone budget remains unapproved. The color views are depth-tested rest vertex distances, not full-surface or motion acceptance. Blue marks excluded/protected bands, not approved roots.

Current reproducible commands are routed in docs/dependency-map.md and the shared model-quality-runbook.md. No GPU, Unreal, environment or unsaved GUI changes. The next work is root-envelope feasibility and strand-local correspondence review, then coupled contact/velocity fixtures and a separately reviewed design revision—not another blind parameter retry.

### Actual-mesh contact-design experiment

ContactMeshExperiment-v001 is the single preregistered diagnostic evaluation of the final guide design, not an accepted replacement. ContactMeshRetention-v001 verifies exact indexed rest positions/faces/edges/loops/UVs/colors/normals, listed material signature and connected image pixels. Regenerated named weights are exactly identical (0 changed vertices) and bind data identical. The common material signature is not an arbitrary nested-shader equivalence proof.

ContactMeshSurfaceCheck-v001: all330frames actual body-relative surface step69.572449mm PASS versus old~156mm; startup0.206558mm PASS, proposed gap increase0.033781mm PASS, protectedrigidity/seams PASS. Sampled capsule clearance remainsFAIL: neck42.976mm,fararm66.310mm,post67.197mm,ground59.964mm, all versus unchanged5mm ceiling. Post penetration regressed from8.247mm despite the guide-step improvement. ContactMeshImport-v001 passes defaultGLB/FBX roundtrip, no connectivity restore required; this is exchange evidence only. Current accepted-source pointer remains DegenerateCleanup-v001.

FailureFeaturePaths-v001 traces exactly four current failed vertices through existing semantic/root graphs, without changing geometry/selections/guides/weights. FailurePathReview-v002 renders these proposals against rest mesh. Tail targets sit309.293/246.847mm from oldguides, exposing a concrete guide-to-surface coverage issue; their graphpaths contain0virtualedges. Neck path is obscured/semanticallyunresolved; fararm path contains1virtualedge. Paths are not approved complete clumps or centerlines. Initial path renderer v001 stopped before creating a folder due unsupported new diagnostic status; failed log retained. v002 adds that explicit unreviewed status to renderer; no acceptance check relaxed.

### Actual-surface and attachment continuation

TailLocalDomains-v001 isolates a232vertex post-side branch and658vertex forked lower branch after source-bound plane cuts; TailLocalDomainReview-v001 shows exact selected surfaces in rest/pose/context/isolation. These are local branch domains, not whole-lock or rig approval. Earlier35mm tubes were partial surface strips. The broad TailBranchDomains-v001 lower cut included8371canonical nodes and is unsuitable as a single lock.

TailCrossSections-v001/TailLocalSections-v001 record nested plane contours and candidate centers/widths; no shell deletion. Capture-FeatureAttachmentMotion.py initially included source seam/cap boundaries (TailAttachmentMotion-v001, unsuitable root fit). Canonical seam-aware cut-band extraction in TailRootMotion-v001 still finds83.865/14.321mm nonrigid root residuals, above the authored5mm diagnostic criterion. A body-fixed or arbitrary rigid local root is incompatible with these existing skin boundaries.

Build-ActualSurfaceClothPilot.py creates a canonical copy of actual selected hair solely for CPU simulation, retains original indexed mesh/UV/weights, and samples simulated coordinates back by exact source indices. ActualSurfaceClothPilot-v001 had inherited4.352624scale and hidden collider evaluation; v002 corrects to metre scale and evaluated wire colliders, source/proxy evaluated positions match0.000417mm.30frame attachment/seam/protected/startup/step checks pass; initial ground overlap32.888mm atframe1 and untouched Mane collisions remain. ActualTailCloth-v001 is the finite330frame Tail-only comparison, same cloth controls and original collision exclusions. Disk cache is required and this is not a skeletal exchange.

SurfaceContactCorrective-v001 uses3passes of actual free-surface analytic capsule/ground projection. It clears sampled vertex collision to~0.000610mm, BUT FAILS full-duration steps241.644mm and attachmentgap121.515mm. It is not a fix or accepted source. Per-feature SurfaceContactCorrectiveDiagnostics-v001: Tail gap0.000615mm butstep241.644mm/frame205;Mane step133.917mm andgap121.515mm/Mane:B066/frame161. Both B066 endpoints are tested free Mane vertices, separated11.282mm atrest; independent nearest-side projection separates the linked root. This is evidence for coupled attachment constraints and collision history, not for exempting or deleting the clump.


Latest scoped motion prototype: CompleteMotionCache-v001 (blend + two adjacent MDD files). Full330 contact/attachment/seam/startup/step checks passed before conversion in ManeSurfaceContactsAllFrames-v001; see final cache checks and MountProfile.md for remaining peak-settling/semantic/style/runtime gates. Open-FeatureMotionReview.py opens an unsaved colored actual-mesh playback. Reusable procedure: docs/art-templates/mount-secondary-motion.md.
