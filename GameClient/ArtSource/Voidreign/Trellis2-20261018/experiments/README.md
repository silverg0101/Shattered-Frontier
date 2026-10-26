# Voidreign generation experiments — 2026-10-18

Controlled screening on RX 7900 XTX using the existing PyTorch 2.14.0 / ROCm 10.1.0 runtime. [Baseline](../1024/Voidreign.blend), prepared reference, model weights, seed 42 and 2048-pixel textures are shared. Each case changes one control:

| Case | Geometry request | Sampling steps per stage | Export target | Question |
| --- | --- | --- | --- | --- |
| Baseline | 1024 cascade | 12 | 300k triangles | Existing comparison |
| retained-detail | 1024 cascade | 12 | 1M triangles | Does export simplification discard useful detail? |
| more-steps | 1024 cascade | 24 | 300k triangles | Does extra sampling improve fidelity? |
| higher-resolution | 1536 cascade | 12 | 300k triangles | Does a finer geometry grid help at the same export budget? |

Requested triangle budgets are approximate. Requested 1536 resolution may fall back under the common 49,152-token limit; each run records its effective grid. Extra sampling is a denoising budget, not language-model thinking. These single-seed tests do not establish a general model ranking.

The retained-detail raw PLY SHA-256 is `76c6f33b6d4940aadb670e97b7b9685b583b56903d021edeb801729a20c16ccb`, identical to the baseline before export processing. Thus that comparison isolates the export budget.

## Results

| Case | Actual grid | Raw triangles | Delivered triangles | Inference | GLB export |
| --- | --- | --- | --- | --- | --- |
| Baseline | 1024 | 4,863,018 | 295,394 | 188.3 s | 25.9 s |
| retained-detail | 1024 | 4,863,018 | 939,464 | 186.0 s | 56.3 s |
| more-steps | 1024 | 4,706,928 | 291,270 | 372.5 s | 19.4 s |
| higher-resolution | **1536, no fallback** | 10,018,178 | 288,632 | 345.8 s | 32.7 s |

Stage durations exclude weight loading (43–46 s for new runs). They are observations from this workstation, not a general throughput benchmark. GPU peak memory was not measured.

- **Retained detail:** no convincing visual improvement over baseline at the reviewed full-body scale. The same underlying defects remain. Keep the 300k budget for routine screening; retain more geometry only when close inspection or downstream work justifies it.
- **More steps:** improves the repeated hanging ornament arrangement, horn ring, layered tack pattern, red eye and red accents. The pendants still look like rounded caps, chains remain coarse, and some saddle/ornament forms look molded. The independent reviewer preferred this variant for reference identity. Use **1024 / 24 steps / 2K textures / 300k target** as the next Voidreign trial, pending more seeds and model comparisons.
- **Higher resolution:** improves fine mane contours, flank rib repetition and surface variation. Some ornaments merge or disappear, and red mane/tail areas become broad patches. It is a useful alternative, with mixed reference fidelity rather than a universal improvement.

All variants preserve four separated legs but retain a short horn, broad ribbon/loop mane and tail, projecting leg fringe, split-looking hooves, coarse chains and fused ornaments. Clay views confirm major geometry defects; their bright shading limits judgments about subtle surface detail. These remain generated concept prototypes requiring artist work, with no retopology, rig, LOD or Unreal acceptance.

All three candidates passed fresh-process saved-source rendering and GLB/FBX geometry-count, scale, UV and image-dependency checks. Final checks used Blender 5.1.1 with `--factory-startup --python-exit-code 1`. The aligned comparison scene independently reopened with four variants at 2.6 m full height and only baseline visible. The three CPU queue tests passed. An independent visual review informed the findings above. [Provenance and output hashes](comparison-manifest.json), [comparison scene checks](comparison-validation.json).

## Review and reproduction

The [experiment specification](../../../../../tools/trellis2/voidreign-experiments.json) defines the cases. Run `tools/trellis2/run_experiments.py --output <new-folder>` with the WSL inference Python. [Runtime instructions](../../../../../tools/trellis2/README.md) document the controls. Generation runs sequentially. `review-status.json` records completed Blender preparation; `experiments.json` records generation outcomes.

Each successful reviewed folder contains editable Blender source, normalized GLB/FBX, packed/external textures, raw PLY, run timings, matched studio views, clay view and fresh-process export validation. Use the side views for silhouette/anatomy, beauty/clay for geometry and the front/rear views for inferred surfaces. Compare horn, skull, eyes, four legs, hooves, ribs, saddle, chains and hair separately. Higher polygon counts cannot prove improved reference fidelity.

[Comparison.blend](Comparison.blend) aligns the baseline and candidates at the same origin. Switch the Outliner eye icons so exactly one variant is visible at a time: A baseline, B retained detail, C more steps, D higher resolution. Candidate collections start hidden. This is a viewport comparison scene; candidates are excluded from rendering until their render visibility is changed. Individual source files provide the validated render setups.

## Other model families

[Verified download record](challenger-downloads.json): all three model families and auxiliary weights completed, 64 files totaling 48,275,802,436 bytes. Hub LFS weights match the SHA-256 values published at their pinned revisions; non-LFS Hub files passed size checks. Auxiliary release files have locally recorded SHA-256 hashes.

| Model | Downloaded scope | Radeon inference |
| --- | --- | --- |
| Hunyuan3D-2.1 | Shape, VAE and PBR paint | Not yet brought up |
| TripoSG | Shape model and DINOv2 | Not yet brought up |
| Pixal3D | Single-view flow/decoder weights, MoGe-2 and NAF | Not yet brought up; NAF/NATTEN compatibility remains open |

Weights live outside Git in WSL. Download completion does not establish backend compatibility with Radeon. Compare geometry in clay first, since TripoSG is geometry-only; compare textures separately. Use the same reference and studio, record each backend's suitable sampler settings and runtime, and repeat promising settings with more seeds before selecting a pipeline default.
