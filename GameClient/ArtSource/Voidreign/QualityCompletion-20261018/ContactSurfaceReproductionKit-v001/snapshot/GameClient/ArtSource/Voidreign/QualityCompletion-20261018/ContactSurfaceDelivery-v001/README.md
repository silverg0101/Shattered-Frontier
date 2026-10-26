# Checked actual-surface motion prototype

Open `../CompleteMotionCache-v001/Voidreign-ActualSurfacePilot.blend` with both adjacent MDD files. Current GUI63452 plays this actual mesh; original unsaved GUI35812 and prior Tail review56080 are preserved. Cyan/amber colors are unsaved review overlays. File source retains textures. Legs stay rigid: this is a response/collision harness, not a gait.

## Evidence index
- CompleteMotionCacheCheck-v001/surface-report.json: all six original checks PASS, every one of330frames. Max penetration0.000682mm, maxstep76.444894mm, initial12.436172mm, gap increase0.033806mm. Protected rest and seams pass unchanged limits.
- CompleteMotionCacheRetention-v001/validation.json: indexed geometry, UVs, colors, normals, common material signature, images exact;0namedweight changes;binds exact.
- CompleteMotionAlembic-v001/exchange-report.json: all284762indexed world vertices across330frames reproduce within0.000544mm after fresh diagnostic reimport. Offline geometry cache only, not skeletal or engine acceptance. PBR not tested by Alembic.
- CompleteSurfaceContactsPreview-v001: four CPU diagnostic frames. CompleteMotionTexturedPreview-v001: CPU textured frames1,122,299.
- ManeWarmupDiagnostic-v001: producer RMS/peak exactly reproduced from frozen native held cache. Peak sourcevertex133921 (canonical91569, rootdistance269.4mm), nearby133914/133915 form a moving distal patch. Not a directly sewn endpoint; body capsules at least124.6mm away for top12points. No claim that a new hard pin, deletion or mass/damping guess is justified.

## Reproduction and limits
Shared procedure: docs/art-templates/mount-secondary-motion.md. Mount settings and failed experiments: ../MountProfile.md. New immutable reproduction index: ../ContactSurfaceReproductionKit-v001/manifest.json. Follow the commands there using the recorded stage producer snapshots and profiles; original profiles refer to retained immutable paths. A fresh rerun needs new output paths and freshly pinned downstream input hashes; Blender binary outputs need not have identical bytes. Never overwrite old attempts or reset exhausted parent counts.

The kit references large sources/caches externally and is not a self-contained archive. Verify it using tools/Freeze-AssetEvidence.py --verify <manifest>. The final blend plus its two MDDs contains the active feature-motion dependencies; original source materials/images still have their own retention/provenance contract.

Unresolved gates: held startup peak301.066mm/s exceeds80 despite RMS16.580 below20; semantic binding/domain approval; style/strain, painted material/PBR/head/saddle review; stance/sole authority; triangle/swept/self/runtime and final delivery budget. This result is a checked offline prototype. It is not a production, import-candidate or engine-validated asset. Historical GUI launcher report says Mane contacts under repair; that stale descriptive sentence predates the completed source-bound pass and grants no acceptance. Current utility uses a neutral acceptance caveat.
