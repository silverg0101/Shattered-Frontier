# Voidreign: 2K, 4K and local texture refinement

Open [FinalTextureComparison.blend](FinalTextureComparison.blend). Toggle Outliner collections A/B/C, keeping one visible:

- **A:** fresh Pixal2K bake, controlled baseline.
- **B:** fresh Pixal4K bake, same mesh, UVs and material volume.
- **C:**4K with projected head/saddle albedo refinements; initially visible.

[TextureResolutionComparison.blend](TextureResolutionComparison.blend) isolates A/B. Each variant folder contains `Voidreign.blend`, GLB/FBX exports, textures, source/export validation and matching previews. `previews/Detail-Head.png` and `Detail-Saddle.png` use identical1024px cameras and exposure across all variants.

## Findings

The4K export gives only a small visual improvement over2K: the material volume already contains soft detail.4K is a fresh bake, not an enlargement of the2K PNG. The projected edits give a much clearer improvement: ivory bone surface detail on the head, darker leather, stitching and worn trim on the saddle, and clearer skull-charms.

The refinement is a **visible-side albedo pilot**, not a completed full skin. Opposite/hidden surfaces retain Pixal appearance. Roughness/metallic maps are preserved; no new normal, displacement or emission maps were generated. Image-generated detail can contain painted lighting and may stretch or drift around depth changes. The feathered projection has local transitions and irregularities on straps, buckles and grazing surfaces; inspect oblique views before extending it. Thick hair geometry, fused details and floating front hooves remain unchanged. No production acceptance or Unreal integration.

## Controlled bake

The original Pixal export did not retain its material volume. `run_pixal.py --save-attributes` repeated the frozen input/seed42/1024cascade/12steps/49,152 token settings on7900XTX, PyTorch2.14+ROCm10.1. The regenerated raw PLY SHA-256 exactly matches the first run: `d98f70f64976e06f3a1eb3fff6b00bd6d7202246e4b43a8a51de2f4bf5932f06`. The recreated material volume was then sampled onto the **original** exported mesh and UV atlas by `rebake_pixal.py`. Both new resolutions use this one volume; the earlier2K export remains preserved in ModelAB-20261018.

`rebake_pixal.py` uses nvdiffrast UV rasterization, CuMesh nearest high-resolution surface projection and sparse trilinear material sampling, following the installed `o_voxel.postprocess.to_glb` method. It reverses the native Pixal export coordinate transform for sampling and retains the original mesh/UVs. Observed bake times:2K12.9s,4K34.8s. These timings exclude218.6s inference/model loading and are not general benchmarks.4K maps have four times the texels of2K; they do not create missing source detail.

Local reusable cache: `Artifacts/Art/Voidreign/pixal-detail-20261018/generated/material-volume.pt` (not a downloaded executable checkpoint). Material-volume SHA-256: `b8931bd8e6a5ea6bd81b3b82ab9dcb66846dd3c9527e65a2be12663f1135236f`. The cache remains ignored scratch data and can be recreated; finished textures and run reports are retained here.

## Projection method and provenance

`Prepare-VoidreignTextureGuides.py` renders registered orthographic base-color guides from the original reviewed mesh. Built-in **image_gen** edited each guide separately, using the [frozen concept](../ModelAB-20261018/Voidreign-reference.png) as style reference. Full prompts, fixed camera matrices, source guides and selected1254² image edits are saved under [ProjectionInputs](ProjectionInputs/). Prompt set: [prompts.json](ProjectionInputs/prompts.json). AI images were copied from the tool's generated-images location into this asset package.

`Project-VoidreignTextureDetails.py` applies the fixed camera projections only to spatially selected, camera-facing surfaces that pass a BVH visibility check. It feathers masks, blends with the original color, and bakes the result to the existing4K UV atlas using an emission-only Cycles bake. Weighted mesh vertices: head5,862, saddle8,958. Image refinement used the built-in image service; Pixal generation/material baking used the local Radeon. No Hunyuan painting is used in this trial.

All three sources were reopened and rendered in fresh Blender5.1.1 processes; GLB and FBX exports were independently reimported. Actual linked maps are2048² for A and4096² for B/C. All reviews retain296,859 triangles and the inferred2.6m full height. `comparison-validation.json` reopens the combined scene, verifies all three collections, packed images and nearest-vertex deviation below0.000001m. Passing these checks does not validate appearance for production.

`generation-run.json`, `bake-run.json` and `manifest.json` preserve settings, hashes, source paths and tools. Original model/source locks and licenses remain in [ModelAB-20261018](../ModelAB-20261018/README.md). All original results are preserved.

## Reproduce

Use the external WSL environment `/home/grego/sf-asset-lab/env-pixal/bin/python` and scripts under `/mnt/c/Projects/Shattered-Frontier/tools/trellis2/`:

```text
run_pixal.py --output <new-generation-folder> --save-attributes
rebake_pixal.py --volume <generation-folder>/material-volume.pt --mesh <original-pixal-generated.glb> --output <new-bake-folder>
```

Review each bake with `Review-GeneratedAsset.py --yaw -112`; use `Validate-GeneratedAsset.py` for reopen/reimport. Blender scripts run with `--background --factory-startup --threads 8 --python-exit-code 1 --python <script> -- <arguments>`. `Prepare-VoidreignTextureGuides.py --source <review.blend> --output <new-guides-folder>` prepares image edit inputs; `Project-VoidreignTextureDetails.py --source <4K-review.blend> --guides <projection-guides.json> --head <head-edit.png> --saddle <saddle-edit.png> --output <new-folder>` bakes selected edits. `Render-VoidreignTextureCloseups.py --folder <review-folder>` (repeat) `--guides <projection-guides.json>` makes matched closeups. Preserve prompts and inspect edit registration before projection. Outputs require new empty folders.
