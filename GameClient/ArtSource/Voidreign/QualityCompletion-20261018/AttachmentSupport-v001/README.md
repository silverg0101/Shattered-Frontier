# Attachment support findings

Start at queue.json. It contains five stable source-component IDs and exact next actions; no folder-wide discovery is needed. The visible Blender63452 source remains the prior passing CompleteMotionCache-v001. The B067 follow-parent candidate is separate and unapproved.

## What changed
- Source C0122/B067 is a16canonical-point fragment linked once to its parent. Original full spring graph has28components, all with a pin path, yet geometric graph has117islands;45unpinned islands have only one sewing link. Connectivity and endpoint gaps did not diagnose orientation.
- At rest, C0122 had0/128samples visible across14directions, with1792/1792target-only controls. On the current measured source it becomes exposed: frame122peakvisibility92.97%, frame29999.22%, both with complete controls. Frame1has one missed control ray and remains unresolved. No deletion approval.
- Two-frame best-fit decomposition attributes99.996% of C0122 squared motion to whole-island motion. C0007/C0048 similarly exceed99.8%; C0002 is different: six stationary root pins, positive three-axis pin covariance, and substantial shape motion. These are descriptive fits, not corrective transforms.
- B067ParentFrameStudy-v001 proposes rigid follow for this micro-fragment using parent triangle11780/11782/11772 and original rest offsets. Only16of26758canonical points change; all other cached points are bit-exact. Child held peak falls301.066→3.180mm/s; global still128.246mm/s, above the unchanged80limit. Minimum frame sine0.404; restgap retained within0.000118mm. This classification is proposed, not approved flexible-hair behavior.
- B067ParentFrameCache-v001 fresh330frame cache match is exact0. B067ParentFrameSurfaceCheck-v001 passes all six original330frame checks. B067ParentFrameRetention-v001 preserves indexedrest geometry/UV/normals/images/common materials and all weights/binds. B067ParentFrameReview-v001 supplies exact-index CPU views. Its own animated Alembic check is separate from the baseline's check.

## Reusable process versus settings
Shared: docs/art-templates/mount-secondary-motion.md. Mount-specific source IDs, triangle, traces, limits and settings: ../MountProfile.md and the pinned profiles saved with each attempt. An intended dangling charm may legitimately rotate; do not replace every point joint with a rigid frame. Long flexible clumps need authored root support rather than whole-clump freezing.

ManeHeldWindow-v001 contains the complete240-frame native held recording, with every input cache file hash-bound and the final pair independently reproduced. ManeHeldWindowAnalysis-v002 uses the original3-8s policy and finds no passing RMS+peak frame. Original free-surface peak spans227.6-308.5mm/s in that interval; every pinned point stays stationary. Plot-v001 preserves full history; Plot-v002 explicitly displays3-8s. No earlier frame or longer ceiling was selected to manufacture a pass.

The first optional Python plotting attempt lacked matplotlib; no packages/environments were changed. Its partial output stays preserved. The successful analysis exports numeric curves; Windows Framework Charting renders the plots with a recorded display window.

## Evidence and replay
- ManeWarmupDiagnostic-v005: graphs, pin support and source-indexed held pair.
- ManeStartupPatchReview-v001; ManeSettlingIslandDomains-v001; ManeSettlingIslandReview-v001: exact source patch/whole-island CPU views. Whole geometric islands are not approved semantic clumps.
- ManeFragmentPoseInspection-v001 and ManeFragmentVisibility-Frame1/122/299-v001: finite posed visibility using full-mesh occluders, explicit component subset.
- ManeIslandMotionAnalysis-v001; ManeHeldWindowAnalysis-v002; ManeHeldWindowPlot-v001/v002: fits, raw speed curves and plots.
- B067ParentFrameStudy/Cache/SurfaceCheck/Retention/Review-v001: bounded one-fragment representation proposal.
- New AttachmentSupportReproductionKit-v001 supplements the immutable ContactSurfaceReproductionKit-v001. Both require external large inputs. Replay the recorded producer snapshot/profile into new folders and repin rebuilt downstream hashes. Never mutate old attempts or reset failed-stage counts.

Remaining gates include orientation/support of other clumps, true bending/material response, source surface strain/style, semantic approval, painted/PBR/head/saddle finishing, stance and engine/runtime/triangle/swept/self-collision acceptance. Passing point/capsule checks alone does not finish these gates.
