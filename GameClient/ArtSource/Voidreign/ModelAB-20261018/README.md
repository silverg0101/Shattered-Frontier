# Voidreign: Radeon model comparison

Generated 2026-10-18 on AMD Radeon RX 7900 XTX through WSL Ubuntu. All three requested backends ran successfully. Shared runtime: PyTorch **2.14.0+rocm10.1.0**, ROCm Core SDK **10.1.0** (PyTorch reports HIP7.16.26385). Backend Python packages are isolated; sources, patches and versions are recorded in `backend-lock.json` and package inventories. See [reproduction instructions](../../../../tools/trellis2/CHALLENGERS.md).

## Open and compare

Open [Comparison.blend](Comparison.blend). Collections A–F contain aligned variants; only A starts visible. In the Outliner, hide the current variant and unhide another using its eye icon. The scene starts in uniform solid shading for geometry comparison. Switch to Material Preview to inspect available textures. Hunyuan shape and TripoSG have neutral materials. Painted Hunyuan is a separate, lower triangle count copy.

Each folder contains editable `Voidreign.blend`, GLB/FBX exports, four review views, matching `previews/Clay-*.png`, `asset-review.json` and `validation-report.json`. Selected Pixal output is **Pixal3D-Aligned**; `Pixal3D` preserves the earlier oblique alignment. Its raw generation files remain there; both reviews use the same generated mesh. No additional generation was performed for alignment.

## Fixed input and settings

`Voidreign-reference.png` is the unchanged 1536×1024 RGBA clean conditioning view. SHA-256: `d67522bf160e2dbfe763a5945e651b6b5c508f32676257dac80c98b26a42aca0`. It is derived from the concept, rather than a multiview sheet. Native backend preprocessing differs; reports preserve those choices. No rider or effects geometry is included.

| Variant | Settings | Observed inference time | Delivered geometry/materials |
| --- | --- | --- | --- |
| TRELLIS2-12steps | Prior 1024,12-step baseline | See original run report | ~300k triangles, textures |
| TRELLIS2-24steps | Prior 1024,24-step baseline | See original run report | ~300k triangles, textures |
| Hunyuan3D-2.1 | Seed42,50 steps,guidance5,octree512 | 529s | ~300k triangles, geometry only |
| TripoSG | Seed42,50 steps,guidance7, hierarchical extraction8→9 | 124s | ~300k triangles, geometry only |
| Pixal3D-Aligned | Seed42,12-step cascade1024,49,152 token cap, low VRAM,2K texture | 209s +15s camera +23s export | ~297k triangles, textures |
| Hunyuan3D-2.1-Painted | Shape above, separate ~40k remesh; texture seed0,15 steps,6×256 views | About95s painting after loading | 40k triangles, actual embedded maps1024² |

These are single-run observations with different algorithms, preprocessing and local ports; they are not an equal-compute benchmark. Model loading is excluded above (Hunyuan24s, Tripo6s, Pixal40s). Painting requested a2K atlas, but delivered embedded images are1024²; actual sizes are in validation. The runner measures89.7s inside its paint call; wrapper timing95.2s starts earlier. GPU peak memory was not measured. Seeds and grid numbers are not equivalent across backends.

All reviews use inferred full height2.6m, groundZ0, matching lights/cameras and common clay framing. This is an art scale assumption, not a calibrated anatomical measurement. Import yaw: Hunyuan/Tripo+90°, Pixal−112° (visually corrected with torso PCA guidance). Pixal alignment is approximate; inspect remaining obliqueness when comparing proportions.

## Visual findings

**Pixal3D is the preferred starting geometry in this trial; TripoSG is the strongest alternative.** Pixal preserves more separated mane/tail strands and repeated ornament/rib structure, but its strands resemble thick ropes, skulls/chain details are coarse, and red/yellow colors exceed the reference. Tripo preserves strands and a sharp horn with a plausible hoof silhouette, but its neck/body are stockier and skull/rib/tack detail is soft or merged.

The corrected Pixal view also reveals floating front hooves while its hind feet approach the floor. GroundZ0 normalization only aligns the lowest vertex; it does not establish a valid four-legged stance. Leg/stance correction is required before using this result.

TRELLIS24 improves saddle layers and ornaments over12 and has a closer dark palette, but broad ribbon hair, thin leg blades and split hooves remain. Hunyuan's delivered geometry is weakest here: broad planar mane/tail sheets and rectangular tack strips lose much of the skull/rib/chain structure. Its painted copy is strongly mottled with incorrect light/red patches and remains visually unusable. Investigate Hunyuan settings and compatibility before drawing a general model ranking.

None is production ready. All need anatomy/contact cleanup, detail reconstruction, topology and material work; no rig, animation, collision or Unreal integration is supplied. Confirm the initial preference with more assets and seeds before choosing a default backend.

## Checks and provenance

### Pixal geometry with Hunyuan painting

Follow-up hybrid trial: [Pixal-vs-HunyuanPaint.blend](Pixal-vs-HunyuanPaint.blend) compares original Pixal textures (A) with Hunyuan painting (B). `Pixal3D-HunyuanPaint/` contains the reviewed hybrid source, exports, original generated GLB, run report, validation and previews. Use material shading to compare textures. This trial retains Pixal's triangles **and existing UV atlas** (`--shape-backend Pixal3D --no-remesh --reuse-uv`) and the same frozen concept image; no shape generation was repeated.

Attempt01 was stopped during the redundant slow upstream UV unwrap and recorded as superseded. Attempt02 completed in59.1s after loading, using the same six256px views/15 steps/texture seed0 as the earlier Hunyuan paint trial. Requested2K atlas; actual embedded maps1024². Fresh Blender reopen/render and GLB/FBX reimports passed. `hybrid-comparison-validation.json` confirms296,859 reviewed triangles in each variant and maximum bidirectional nearest-vertex deviation below0.000001m; packed images resolve.

**Visual result rejected:** white/red/gray mottling obscures the intended dark skin and tack. Reusing Pixal geometry and UVs does not cure the earlier painting defect. Retain original Pixal textures as the better current option; diagnose generated views, bake/UV mapping and material channels before accepting Hunyuan painting. Passing export checks establishes usable files, not usable artwork. Hybrid attempt reports/logs and final runner hashes are in `attempt-evidence/` and `hybrid-manifest.json`.

The fitted-skin workflow is: unwrap mesh → render normals/positions from multiple angles → condition texture generation on these guides plus concept → bake generated views into the UV atlas → fill unseen regions and blend seams → inspect the model. The Hunyuan pipeline already implements these stages; a general image model also needs shape guides and a bake step. Hidden surfaces remain inferred rather than recovered from the concept.

Each candidate was reopened and rendered in a fresh Blender5.1.1 process; GLB and FBX were independently reimported. Geometry-only checks explicitly exclude UV/texture acceptance. Passing file checks does not accept visual quality. Matching clay views and material views were inspected, including an independent visual review. `comparison-validation.json` records fresh-process verification of all six collections, ground contact,2.6m normalization, one visible variant and packed images.

`download-verification.json` verifies68 files totaling52,821,811,885 bytes against pinned Hub sizes/LFS hashes or recorded auxiliary hashes. `backend-lock.json`, source patches, licenses and environment package inventories document local adaptations. `comparison-manifest.json` links attempts, output hashes and final runner hashes. `attempt-evidence/` preserves successful run reports and failed attempts/logs, including Tripo01 and Hunyuan paint01–03. Model caches/environments remain outside Git at `/home/grego/sf-asset-lab`; the source input and finished outputs are retained here. Temporary generation evidence is under `Artifacts/Art/Voidreign/model-ab-20261018`.
