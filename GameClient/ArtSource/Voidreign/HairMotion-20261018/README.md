# Voidreign actual-mesh hair-motion prototype

Open [Voidreign-HairPrototype.blend](Voidreign-HairPrototype.blend) and press Space to play330 frames at30fps (11 seconds). [Hair-Motion-Diagnostic.mp4](Hair-Motion-Diagnostic.mp4) highlights the **actual Pixal mesh**: cyan marks weighted mane regions, amber marks tail regions, gray marks static context. Diagnostic colors are for inspection; the editable source retains the4K head/saddle texture trial.

## What this demonstrates

Selected regions of the original Pixal mane and tail are skinned in place. The original mesh coordinates/topology and textures are retained; no added procedural clumps count as the delivered result. The rig has20 bones:8 mane,11 tail and a static body bone. Spatial masks, feathered skinning and attachment roots are specific to this asset. Other models require their own semantic segmentation and rig profiles.

The test sequence is settle(0–1s), walk response(1–3s), run response(3–5s), turn(5–7s), stop/settle(7–9s) and rider collision-probe sweep(9–11s). **The horse uses a rigid translation/bob/turn harness; its legs do not walk or run.** A seated proxy and a swept hand-clearance probe provide collision context. The static orange post tests environment contact. These are diagnostic proxies, not a finished rider/reins rig or accepted riding anatomy.

The solver integrates particle chains with styling springs, drag, gravity, distance/bending constraints, capsule contacts and a ground plane, then bakes bone transforms. This is an **offline bake**, not live Unreal physics or a real-time performance measurement. Scene custom properties expose the recorded author settings; edit `config.json` and rebuild a new folder for changes to take effect.

## Evidence and limits

`simulation-report.json` records330 frames, exact solver roots and maximum segment-length error about4.31%. Constraint activity counts: horse1,646, rider1,089, environment39, ground1,446. These are repeated solver contact corrections, not counts of unique interactions, and do not establish that the whole rendered hair surface clears those objects.

`validation-report.json` records fresh-source reopen/render, unchanged original rest mesh, normalized weights, sampled static-body stability, simulation-to-bone head agreement and independent animated GLB/FBX reimports with resolved4K maps. The GLB uses one merged scene animation so body motion and hair play together. FBX animation simplification is disabled to preserve sampled motion. Blender's importer helper meshes are excluded from the delivered skinned-mesh count.

**Source and GLB checks pass; FBX remains needs-review.** Maximum imported GLB bone-head deviation is about0.0000014m; FBX is0.00586m, above the0.005m prototype check even after disabling animation simplification. FBX is retained as a diagnostic candidate, not accepted for equivalent playback. Use `.blend` or GLB for this preview. The job record keeps this defect open. Source checks sampled868 fully static vertices and found deviation below0.000001m; simulated-to-baked bone heads agree within0.000002m. These are sampled engineering checks, not full surface-collision acceptance.

Remaining limits: only selected hair regions move; original strands are fused and geometric mask boundaries may pull adjacent surfaces. Hair thickness/strand interiors, self-collision, attachment seams and whole-surface rider/environment clearance need further work. Bone lengths remain fixed while the solver permits small chain errors, so joint translations compensate; this is not a production skeleton. Existing stance, anatomy and texture defects remain. No Unreal import, runtime collision, LOD/performance validation or production acceptance is claimed.

## Reproduction and provenance

Source: [Pixal4K head/saddle trial](../PixalDetail-20261018/README.md), using its `Pixal-4096-HeadSaddle/Voidreign.blend`. Generator: `tools/Create-VoidreignHairPrototype.py`; typed settings/defaults: `tools/voidreign-hair-prototype.json`; check script: `tools/Validate-VoidreignHairPrototype.py`; diagnostic frames: `tools/Render-VoidreignHairMotion.py`. Pipeline contract: [model-pipeline.md](../../../../docs/art-templates/model-pipeline.md), tracked [job record](../../../../docs/art-templates/model-jobs/voidreign-motion-20261018.json).

Run Blender5.1.1 with `--background --factory-startup --threads 8 --python-exit-code 1 --python <generator> -- --source <source.blend> --config <settings.json> --actual-mesh --output <new-empty-folder>`. Validate using `--python <validator> -- --folder <new-folder> --reference <original-source.blend>`. Render diagnostic frames with `--python <renderer> -- --folder <new-folder>`; encode at10fps using FFmpeg (every third frame of30fps motion, presented at30fps with repeated display frames). The preview therefore samples motion at10fps, not30 independent rendered frames per second.

The original added-clump experiments are preserved only as scratch history under `Artifacts/Art/Voidreign/hair-prototype-20261018/trial01` and `trial02`. `actual01` supplies this actual-mesh bake. `simulation-trajectory.json`, source/export reports, config and manifest preserve the accepted diagnostic attempt. User requested using the actual mesh after seeing the earlier test-clump approach. Licenses and model/runtime provenance remain in [the model A/B package](../ModelAB-20261018/README.md).
