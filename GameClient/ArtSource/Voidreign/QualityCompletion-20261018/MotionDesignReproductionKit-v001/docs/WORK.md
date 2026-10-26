# Voidreign quality checkpoint — latest continuation 2026-10-18

## Goal and controls

User authorized continued model/pipeline work until interrupted. Active15minute heartbeat; no supported /compact action exposed. Read docs/dependency-map.md first, then this checkpoint and docs/art-templates/model-jobs/voidreign-quality-20261018.json. CPU-only: Greenlands owns7900XTX. No inference/GPU renders, dependencies/environment/Unreal/server changes or commit/push. Preserve unsaved Blender GUI PID35812 (only Blender process at checkpoint). Do not launch duplicate automation or blind fourth rig/solver bake. Existing /root/visual_review completed the latest audit; no pending task. Prior checkpoint is frozen in ForensicReproductionKit-v001/docs/checkpoint-before-forensics.md.

## Current source and ownership

Base B=GameClient/ArtSource/Voidreign/QualityCompletion-20261018.
Current source B/DegenerateCleanup-v001/Voidreign-CompleteHair.blend; source SHA6c6044a533c16fc7fad23d42c0407dde37a0252aca84f9be1bc9b9a2367364f6; rest meshba2d283db0be5e8cff4cc7549f1f51916cc0a4d18213773300976b11512ececa.284762vertices/297119triangles/219components. Geometry2/3; root/topology3,rig3,secondary_motion3; no reset.133clump bindings remain proposals.24guides/229flat bones; rigid diagnostic body/legs, no gait or engine acceptance.

DegenerateCheck-v001 exact retained geometry/UV/colors/weights/normals/images/330bones/19surfaces pass; default GLB/FBX DegenerateImport-v001 pass. HygieneMaterialReview closeups pixel-identical to prior material candidate. HygieneSemanticComparison exact selections through remap. Current source-specific ledger HygieneComponentReview-v001, selection HygieneSemanticSelection-v001, guide proposals HygieneGuideProposal-v001. No new bake from refreshed metadata.

## Still failed / unresolved

HygieneSurfaceCheck-v001 exactly repeats old failures:156mmmaxstep,68.7mmground/51mmneck/63mmfararm/8mmpost penetration;80mm/5mmlimits unchanged. Rigidity/seams/startup/gaps pass; attachmentgap0.044mm. Particle startup/stretch pass is not surface acceptance. Protected/body checks do not accept all anatomy. SurfaceDiagnosis-v001 and TrajectoryContactReview-v001 locate guide/contact/skin issues; skin lever arms0.18–0.31m. NextRevisionPlan-v001.md proposes reviewed strand-local mapping/contact redesign, not fourth bake approval.

HygieneVisibility-v001:180lowvisibility,18unresolved controls,21other visible; not defect/deletion counts. Historical posed checks expose31–32rest-occluded parts in faulty motion; never mass-delete hidden shells. C1/C4visible controls; C5horncore/C12brow/C18flank/C25skullcap/C26legstrip remain sample-occluded, unresolved purpose.

Materials: OppositeDetail-v001 sharpens head/saddle base color; color-only checks exact outside mask, other maps/geometry/animation. Painted highlights, yellow shoulder transition, coarse faceted geometry, missing red eye, incomplete PBR remain. SupportInspection-v001 authored hoof candidates:front86–103mmraised,rear3–8mm; no approved sole boundaries/primary pose. CompleteHairPlayback-v003 remains old-source diagnostic11sec CPU video, not latest hygiene render.

## New completed read-only findings

Inspect-CoincidentFaceGroups.py → B/CoincidentFaceReview-v001:2254participating faces/1124positiongroups. All groups differ winding/normals,1122UVs,66colors. ZERO exact-attribute redundant-face candidates; preserve all pending semantic/posed review. Shader backface culling enabled. Signature does not cover arbitrary attributes/edge semantics/all shader dependencies. No deletion or geometry attempt.

Check-FeatureTrajectory.py with model-profiles/voidreign-guide-trajectory-check-20261018.json → B/TrajectoryCheck-v002. Reads existing Simulation-v003 only;118node steps>80mm across33frames175–213. Tail_G00=79,G04=30,G11=9;116TURN,2STOP. Peak154.968mm/frame210/G00/node11,body-relative154.471mm;speed4.649m/s; output finite-difference acceleration67.910m/s2/frame189/G04/node11. No velocity/acceleration acceptance thresholds. Guide warning is not measured surface displacement or proof of solver instability. Current NPZ fingerprint does not attest original producer hash (historical report lacked it). v001 retained with an overly strong necessity sentence; v002 corrects wording, metrics identical.

ForensicGateReview-v001/review.json: independent existing agent found no blocking narrow math error, supports these limitations. ContactRedesignCases-v001.md specifies finite substep/contact/velocity fixtures before a separately reviewed solver design. Source solver projects positions without explicit contact velocity solve; investigate, do not claim sole cause or blindly zero velocities.

## Reproduction and docs

B/README.md has current sources/commands/known failures. docs/art-templates/model-quality-runbook.md has reusable sequence, actual semantic hair, internal-shell/posed, color, degenerate, duplicate and full-duration lessons plus finite asset-session prompt. docs/dependency-map.md routes tools. Job/catalog/work-plan updated; only Voidreign catalog row touched. Frozen kits:ReproductionKit-v001,ReproductionSupplement-v001,HygieneReproductionKit-v001,ForensicReproductionKit-v001. Latest18files hash-verified and job schema valid. Never mutate kit references. Earlier historical attempts lack some exact snapshots; no universal bitwise reproduction claim.

## Next ready actions

1. Read exact job/active processes. Keep current source and failed gates intact.
2. Resume finite semantic component dispositions using current source-hashed ledger and depth-tested/isolation/posed evidence. No duplicate deletion suggested by latest forensic check.
3. Prepare/review finite contact/velocity/feasibility fixtures from ContactRedesignCases-v001.md and NextRevisionPlan-v001.md; no fourth blind model bake or reset. Distinct design revision must carry failed attempts forward and undergo review.
4. Other ready work: material transition/PBR coverage and primary stance/sole specification with immutable evidence. Do not relabel diagnostic body as gait or model as complete.
5. Save new attempts/checks, exact tool/profile hashes, job/catalog/checkpoint. Preserve concurrent Greenlands files and GUI. Notify only meaningful results/failure/essential decisions.

## Latest continuation — user renewed hair motion/collision work

Current source/hash unchanged (DegenerateCleanup-v001). User explicitly requested continued hair fixes and reusable findings. Exact BlenderGUI PID35812 preserved; CPU-only, no GPU/Unreal/environment/commit changes. Parent topology/rig/secondary_motion attempts remain3; current job stage now secondary_motion. No new delivered rig/model bake or profile accepted.

New tools: Trace-FeatureGuideMotion.py, FeatureContactVelocity.py, Study-FeatureContactVelocity.py, Check-FeatureGuideClearance.py, Propose-FeatureCoverageGuides.py, Render-FeatureCoverage.py. Routes and commands in dependency-map/shared runbook/B/README.md. Existing visual_review independently reviewed bounded study design/tools; latest closure review pending until recorded.

B/TurnTrace-v001 exactly reproduces all original saved points/root transforms/rest/counts from frozen Simulation-v003 solver/config/guides.14,267events,3tailguides,5.5–7.2s. Max sequential deltas prediction19.84mm/substep,structural2.11mm,post7.53mm,ground3.95mm. Not independent causal forces/energy.

B/ContactVelocityStudy-v002:15analytic fixtures pass. Proposed split-contact velocity excludes depenetration from inferred velocity, projects relative normal approach, preserves single-contact tangential motion, reports contradiction. Order dependence exposed by oblique reversed contacts; not minimum impulse. True fixed-length axis rotation tested; radial capsule spin excluded. NOT wired into delivered solver. Coupled release/root/finite-width/swept/self collision still missing.

B/MotionDesignStudy-v001/study-results-v002.json: reviewed finite single-variable diagnostic simulations from frozen solver and24identicalguides. Drag5→12:peak95.61mm,52nodewarnings27frames; Spring85→30:peak94.20mm,65warnings13frames; baseline154.97mm118warnings33frames. Both fail80mm. Higherdrag increases turn/stop lag; lowerspring nearly doubles settled RMS87→172mm. No combined run or selected profile. Old attempts preserved. Source unchanged.

B/GuideClearanceBaseline-v004 and candidate Drag12Clearance-v002/Spring30Clearance-v002: full shapes/rest/finite transforms/translation/yaw validated; metrics identical to earlier diagnostics. Free guide baseline residual0.014mm vs actual skin up to69mm penetration. Fixed Mane_G10/node0 hand envelope conflict22.09mm atframe301; overall status explicitly needs-review. Envelope conflict is not necessarily root inside solid collider. Do not exempt/move root or alter collider merely to pass.

B/CoverageGuideStudy-v002/coverage-report.json + CoverageViews-v002:24additional rooted mesh-graph path proposals (not skinned/simulated) improve max rest nearest-guide distance236→139mm mane,319→172mm tail. Still~49%mane/~48%tail evaluated canonical nodes>50mmwarning. Source selection/proposedvirtualrelations unapproved; graph paths can cross fused locks. No automatic skeleton doubling. Current NPZ/additional-guides hashes bound in report; renderer verifies NPZ and source/mesh. Gray outsidefeatures; blue protected/existingexcludedrootband; green<=50mm/amber50–100/red100–200/magenta>200. Vertex colors interpolate, not area-complete distance fields. Blue is not semantic approval.

B/ContactDesignReview-v001/review.json records independent findings/corrections. MotionDesignReproductionKit-v001 snapshots this continuation; do not mutate its files/references. Earlier versions retained, including clearance status wording/provenance assertions corrected later. Job schema validated, only Voidreign catalog row updated. Next: source-hashed fixed-envelope feasibility and strand-local clump/path review, then coupled contact/release fixtures before a distinct reviewed solver/skin revision. No more failed-profile tuning or blind fourth bake. Full actual-surface/attachment/semantic/PBR/stance/import/runtime limitations remain.
