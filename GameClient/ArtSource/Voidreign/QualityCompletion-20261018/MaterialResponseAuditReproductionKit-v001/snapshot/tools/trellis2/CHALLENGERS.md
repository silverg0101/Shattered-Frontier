# Radeon challenger backends

These backends use the verified WSL Radeon runtime from [the TRELLIS recipe](README.md). `env-hunyuan`, `env-tripo` and `env-pixal` live under `/home/grego/sf-asset-lab`. Their `.pth` files reuse the existing PyTorch **2.14.0+rocm10.1.0**, torchvision and native ROCm binaries read-only. Backend-specific packages install into the child environments. Transformers/diffusers versions differ to match each backend's APIs; PyTorch/ROCm are not downgraded. The child environments depend on the base environment remaining available.

## Source and setup

External sources are cloned under the lab's `sources/`: official `Tencent-Hunyuan/Hunyuan3D-2.1`, `VAST-AI-Research/TripoSG`, `TencentARC/Pixal3D`, `valeoai/NAF`, `microsoft/MoGe`, and the community `VladimirTalyzin/hunyuan3d-2.1-mac-rocm` paint wrapper. The run's `backend-lock.json` records exact revisions, remotes and patch hashes. Downloaded checkpoint revisions and hashes are in the previous comparison's `experiments/challenger-downloads.json`.

`setup-challengers.sh hunyuan|tripo|pixal` creates a child environment and installs its dependencies with explicit constraints preventing torch/torchvision replacement. Hunyuan's mesh library also needs Ubuntu `libopengl0`. `patch_challengers.py` applies the narrow source compatibility changes. Source and weight caches remain outside Git. Capture the resulting state with `capture_challengers.py --output <new-lock-folder>`; package files and actual imported runtime versions are recorded separately.

Hunyuan painting additionally uses `setup_hunyuan_paint.py` to link verified local weights and the official source into the community wrapper's expected layout. Fetch its additional DINOv2-Giant encoder with `fetch_challengers.py --group hunyuan-dino` in `fetch-env`; the runner selects that pinned local snapshot explicitly. Build its CPU mesh-inpainting module with the Hunyuan Python and `sources/hunyuan-rocm/scripts/build_extensions.py --raster torch`. The runner imports setuptools to activate vendored distutils for BasicSR on Python3.12. No GPU rasterizer compilation or Blender installation inside WSL is required. PyTorch supplies the rasterization operations; the wrapper's memory preflight remains enabled.

The painting runner uses Diffusers' classic `AttnProcessor` for the image VAE: the fused VAE attention path failed on this GPU with `hipErrorProfilerNotInitialized`. Other attention retains the community wrapper's bounded SDPA adaptation. The successful run is `hunyuan-paint-04`; earlier failed attempts are preserved. This workaround does not alter the base runtime or original shape output.

## Generation

Use the appropriate child Python and a new output directory:

```text
env-hunyuan/bin/python tools/trellis2/run_challenger.py --backend hunyuan --output <new-folder>
env-tripo/bin/python tools/trellis2/run_challenger.py --backend tripo --output <new-folder>
env-pixal/bin/python tools/trellis2/run_pixal.py --output <new-folder>
env-hunyuan/bin/python tools/trellis2/run_hunyuan_paint.py --mesh <saved-hunyuan.glb> --output <new-folder>
env-hunyuan/bin/python tools/trellis2/run_hunyuan_paint.py --mesh <pixal-generated.glb> --shape-backend Pixal3D --no-remesh --reuse-uv --output <new-folder>
```

These are schematic paths: child interpreters are under the WSL lab; repository scripts are under `/mnt/c/Projects/Shattered-Frontier/`. Runners intentionally use the frozen `Trellis2-20261018/Voidreign-clean.png` reference and are **Voidreign-specific**, not a general image-generation application. Reports record the input hash and GPU identity and reject a non-7900-XTX/non-HIP runtime. Run one GPU job at a time. Keep a Windows `wsl.exe` client attached to the worker when launching hidden background jobs; a detached `nohup` process alone is not durable on this workstation.

The painting stage accepts geometry from Pixal3D as well as Hunyuan. `--shape-backend` records provenance; `--no-remesh` retains input triangles rather than reducing to the painting pipeline's approximately40k triangle default. `--reuse-uv` additionally retains the existing atlas, requires no remeshing, and rejects absent/incomplete/nonfinite UVs. Otherwise upstream xatlas re-unwraps the input, which can be slow at the full Pixal triangle count. Use the native generated GLB coordinate convention and apply the same Blender review alignment afterward. Preserve original Pixal textures in a separate review folder and inspect the repainted result before accepting it.

| Backend | Initial settings / adaptation |
| --- | --- |
| Hunyuan shape | Seed42, 50 steps, guidance5, 512 octree grid, upstream marching cubes; GPU diffusion and field decoding |
| TripoSG | Seed42, 50 steps, guidance7, native RGBA/white-background preparation; upstream hierarchical extraction depths8→9 and marching cubes. CUDA-only `diso` import is lazy; its flash decoder is not selected |
| Pixal3D | Seed42, 12-step samplers, 1024 cascade, 49,152 token cap, low VRAM mode, 2K textures and ~300k exported triangles. Local MoGe2 estimates the camera; dense/sparse SDPA and the existing ROCm voxel kernels are reused |
| Hunyuan paint | Separate process/copy after shape generation, six views at256 pixels, 15 UniPC steps, guidance3, 2K atlas, community PyTorch rasterizer. Upstream explicitly uses texture seed0 (shape seed42). Its ~40k painting remesh is separate from the original 300k geometry comparison |

The seed number is shared but noise and sampling algorithms differ. Grid numbers are backend-specific, not comparable measures of quality. Native preprocessing differs and is recorded; the source image itself stays fixed. Timings include local adaptations and are observations, not a steady-state cross-model performance benchmark. GPU peak memory is not measured with the disabled caching allocator.

## Pixal neighborhood attention

`naf_rocm.py` implements NAF's shifted dilated neighborhoods with chunked ordinary PyTorch operations. NAF upsamples keys/values with nearest-exact integer factors; gathering the corresponding original low-resolution neighborhoods avoids materializing expanded values. Learned NAF parameters and image conditioning remain enabled. This is a local compatibility implementation, not upstream NATTEN or an official AMD support claim. A small CPU comparison against direct per-pixel neighborhood attention measured maximum absolute error `3.5762786865234375e-7`; it does not establish bitwise equivalence to every native NATTEN kernel. The exact NAF weights load unchanged.

## Blender comparison

### Pixal texture-resolution and local-detail trials

`run_pixal.py --save-attributes` retains generated raw geometry, sparse material attributes, coordinates, layout and voxel size in a local `material-volume.pt` cache before export. `rebake_pixal.py --volume <local-cache> --mesh <original-generated.glb> --output <new-folder>` samples that volume onto the existing mesh/UV atlas at2048 and4096, using the same native raster/BVH/trilinear method as o_voxel; it does not resize a2K image. The cache is produced locally and loaded with weights-only deserialization plus the explicit slice type required by the attribute layout. Use one Radeon job at a time.

The [Voidreign texture trial](../../GameClient/ArtSource/Voidreign/PixalDetail-20261018/README.md) records the controlled2K/4K comparison and a visible-side head/saddle albedo pilot. `Prepare-VoidreignTextureGuides.py`, `Project-VoidreignTextureDetails.py` and `Render-VoidreignTextureCloseups.py` are asset-specific Blender scripts for fixed guides, visibility-gated UV projection/baking and matched closeups. Image edits and prompts are retained separately; this is not a general automated full-skin pipeline.

`Review-GeneratedAsset.py --geometry-only` gives untextured Hunyuan/Tripo geometry a neutral material without inventing UVs or textures. Use the same flag with `Validate-GeneratedAsset.py`; its report explicitly excludes texture/UV acceptance. Textured Pixal and painted Hunyuan use the normal checks. `--yaw`, `--pitch`, `--roll` normalize source coordinate conventions before the common 2.6m inferred full-height art scale. Record all rotations and visually confirm orientation.

`Render-AssetClayComparison.py --folder <review-folder>` (repeat for every candidate) renders four matching clay cameras, lights, material, exposure and a common orthographic scale. `Compare-GeneratedAssets.py --baseline-label <name>` creates the aligned Outliner comparison. Always launch batch Blender with `--factory-startup --python-exit-code 1` to exclude unrelated installed add-ons and propagate script errors. Reopen each saved source and reimport its exports before delivery. Geometry comparison and texture comparison are separate; TripoSG has no native texture generator. Preserve failed attempts and compatibility logs alongside the successful run evidence.

## CPU material-volume diagnosis

With the existing Pixal Python, run `audit_pixal_volume.py --volume <locally-generated material-volume.pt> --source-root <Pixal3D source> --output <new folder>`. It uses CPU tensors only, hides devices in that child process and asserts no GPU context; it does not invoke inference, baking or runtime setup. Save raw channel/layout statistics and compare the exact historical rebaker/cache hashes before blaming ORM packing or changing channel order. Nearly constant source PBR data requires material-class authoring or a reviewed new painting result; a4K export alone cannot add that information. Voidreign PixalVolumeChannelAudit-v001 records the actual finding and its limits.
