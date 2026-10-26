# Voidreign — local TRELLIS.2 Radeon comparison

Generated locally on an AMD Radeon RX 7900 XTX with **PyTorch 2.14.0+rocm10.1.0 / ROCm Core SDK 10.1.0**, using the ATLAS ROCm port and recorded compatibility patches. The selected result is [1024/Voidreign.blend](1024/Voidreign.blend). It opens with the clean reference beside the generated mesh in the viewport. Geometry, material, UVs, packed textures, cameras and neutral studio lighting are editable.

## Results

| Run, seed 42 | Load | Inference | GLB bake/export | Raw triangles | Delivered triangles | Texture size |
| --- | --- | --- | --- | --- | --- | --- |
| 512 | 28.4 s | 124.9 s | 16.4 s | 1,378,204 | 298,467 | 1024² |
| 1024 cascade | 42.4 s | 188.3 s | 25.9 s | 4,863,018 | 295,394 | 2048² |

Times are recorded stage durations, not a steady-state performance benchmark. Peak GPU memory was not measured: the caching allocator was disabled, and native HIP allocations are outside PyTorch accounting. A separate 10 GB allocation/write/readback and FP16 arithmetic probe passed on this GPU.

Both runs preserve the recognizable equine silhouette, four legs, skull face, horn, saddle, ragged barding and swept mane/tail. The 1024 result has substantially clearer skull contours, ribs, tack trim and surface textures. Both geometry resolution and texture resolution changed, so this comparison does not isolate the contribution of either.

Remaining artist work: lengthen/refine the horn; replace broad looped mane/tail ribbons with controlled hair geometry; reconstruct chains and skull charms; remove detached ornament fragments and projecting leg flaps; correct the partly cloven-looking hooves and inferred far-side anatomy. The red eye/glow remains weak. This is a generated concept prototype with dense triangulated topology, one joined textured mesh and one UV layer, without retopology, rigging, animation, LODs or gameplay acceptance.

## Inputs and review

`Voidreign-original.png` preserves the supplied concept sheet. `Voidreign-clean.png` is a transparent single-view derivative made with the built-in imagegen tool; exact prompt: `reference.prompt.txt`. It is a synthetic isolation/reinterpretation, not a pixel-exact crop: it alters some flank rib presentation and skull-charm count. Fidelity was compared against both the original and conditioning view. Smoke, rider and text panels were excluded from the conditioning image.

The 512 and 1024 folders each contain the raw PLY, original generated GLB, normalized Blender source, GLB/FBX, explicit texture files, review renders and validation report. The 1024 folder also includes a clay view. Each `.blend` was reopened in a fresh Blender 5.1.1 process and rendered; each delivered GLB and FBX was reimported into an empty scene. Geometry counts, scale, material assignments, UVs and image dependencies passed. GLB is the primary PBR handoff; the checks do not establish identical shading in every FBX consumer. A separate independent visual review informed the findings above.

Normalized coordinates are metres, Z-up, muzzle toward -Y, ground Z=0. Full height is an **inferred art scale of 2.6 m**, not a measured anatomical dimension. The selected asset bounds are approximately **0.931 × 4.122 × 2.600 m** (X/Y/Z). It is a joined generated mesh; tack, hair and anatomy have not been separated for deformation or editing. No rider fit, collision or Unreal import was validated.

## Reproduce and provenance

[Local runtime and commands](../../../../tools/trellis2/README.md) describe the isolated WSL environment, exact active stack and scripts. `software-lock.json` records source revisions and compatibility patches; `model-downloads.json` records exact checkpoint revisions. DINOv3 was acquired from the public PIA mirror with a SHA-256 equal to official weight metadata. `python-packages.txt`, `runtime-report.json`, per-run `run.json`, and `upstream-licenses/` preserve the environment and evidence. Model caches and build dependencies remain local under `/home/grego/sf-asset-lab` and the user's Hugging Face cache; no model weights are committed here.

Example source reconstruction, from the repository root, into a new folder:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe' --background --factory-startup --python-exit-code 1 --python tools/Review-GeneratedAsset.py -- --input 'GameClient/ArtSource/Voidreign/Trellis2-20261018/1024/Voidreign.generated.glb' --output 'Artifacts/Art/Voidreign/new-review' --reference 'GameClient/ArtSource/Voidreign/Trellis2-20261018/Voidreign-clean.png'
```

Runtime workers exit after generation and release model residency. No application UI or automatic service has been installed in this step. No Unreal or gameplay content was changed.

Follow-up: [controlled sampling, geometry-resolution and export-budget tests](experiments/README.md) preserve the same reference/seed/texture size and include the challenger-model download inventory. The original 512/1024 comparison above remains its own recorded experiment.

