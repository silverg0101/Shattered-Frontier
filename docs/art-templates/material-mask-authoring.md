# Source-bound material masks

Use this procedure after source geometry/UV retention and material-channel inspection. Keep asset coordinates, reviewed IDs, settings and measured results in the asset profile/job. This procedure prepares authoring data; it does not approve a material or modify a model.

## Choose the representation

- Use exact face assignments for a wholly reviewed common response, such as a nonmetal surface. A small face can receive a constant response even when it has no texture pixel-center sample.
- Use a reviewed UV-texel mask when a triangle genuinely crosses material classes or needs spatial variation. Geometry membership or albedo brightness alone does not establish those classes.
- Keep unknown faces, ambiguous transitions and shared-UV conflicts explicit. Do not fill them by majority class or reset an exhausted repair stage.

## Prepare and measure

1. Finish the finite patch review described in [production gates](model-production-gates.md). Save the author, exact face IDs, class/response scope, reasons and hashed context/isolation evidence.
2. Run `tools/Build-MaterialFaceApproval.py --profile <profile> --output <new folder>` with independent retained-source evidence. It checks source SHA and triangle fingerprint, subset integrity and image hashes. Its output is a reviewed selection, not a shader edit.
3. Run CPU Blender `tools/Export-AssetUVTopology.py --profile <profile> --output <new folder>`. Require the exact source/candidate SHA, retained triangle and UV hashes, and one named UV layer. It exports source face IDs through indexed triangles/loop UVs.
4. Run ordinary Python `tools/Rasterize-ReviewedMaterialMasks.py --profile <profile> --output <new folder>`. The profile pins all input hashes and integer `resolution` in `{1024, 2048, 4096}` texels per side. The adapter supports one normalized atlas only; other UV conventions/UDIMs need their own declared adapter.
5. Use `tools/Compare-MaterialMaskResolutions.py --profile <profile> --output <new folder>` for a finite resolution comparison. It verifies raster coverage hashes and reports missing approved faces, overlap participation and zero-area UV triangles against the same topology. Inspect the coverage preview and relevant source images.

Blender commands use `--background --factory-startup --threads <bounded CPU count> --python-exit-code 1 --python <frozen tool> -- --profile <profile> --output <new folder>`. Freeze the executable before launch; do not edit it during execution.

## Measure detail-projection allocation

When detail painting is soft, measure the existing edit footprint before changing the atlas. `Export-AssetUVTopology.py` optionally accepts up to16 named `point_color_attributes`; it verifies each POINT/FLOAT_COLOR array against retained-source shape, dtype and SHA256. Preserve those names in the mount profile.

Run ordinary Python `tools/Measure-AssetProjectionUV.py --profile <profile> --output <new folder>`. It checks an exact triangle/UV bridge to the existing raster, excludes overlapping pixels and interpolates the retained grayscale weight at UV pixel centers. Profiles declare region attribute names and preview colors. Diagnostic region unions take the maximum of vertex weights before interpolation; this is not the ordered bake composite alpha. Keep that distinction when comparing painting results.

Record support pixels, summed weights and descriptive half/full-weight counts at each resolution. The square root of summed weights is only an area-equivalent square side: scattered UV islands do not become a contiguous image, and the statistic is neither anatomical coverage nor texels per metre. Do not treat diagnostic thresholds as quality gates. Compare registered head/detail surface views and source paint detail before deciding whether to repaint or propose a reviewed UV allocation change. Preserve exhausted stage budgets.

## Class-ID data contract

| PNG value | Meaning |
| --- | --- |
| 0 | Unreviewed/unknown mesh surface |
| 1 | Reviewed strand nonmetal response |
| 2 | Reviewed bone nonmetal response |
| 3 | Reviewed organic common nonmetal response; finer class unresolved (contract v2) |
| 254 | Multiple UV faces cover this sample; unresolved |
| 255 | Unused atlas |

Contract v2 adds code3 without changing previous codes. Approval arrays use `organic_dielectric_faces` for a reviewed common metallic-zero response across organic interfaces; that response does not assign tissue, strand motion, roughness or normals. Historic v1 outputs stay immutable. Consumers must read the saved contract/legend and reject unsupported classes.

These values are data IDs, **not metallic, albedo or alpha values**. Use raw/non-color interpretation and nearest sampling for inspection. Preserve `mask-report.json`, its palette, source-face ownership and overlap arrays. New classes require a versioned contract rather than reinterpreting existing codes.

An optional `coverage_cache_report` reuses a prior exact-UV raster after checking its report hash, coverage hash, resolution and topology binding. New approval labels are applied to unchanged owner/conflict/count arrays; save an exact-array comparison and new output revision. Approval changes do not require rerasterizing every triangle.

The rasterizer evaluates pixel centers with half-open edge ownership, converts Blender bottom-left UVs to top-left image rows, and performs no antialiasing, dilation or guessed filling. A clear sampled pixel does not prove its filtered/mipped footprint is clear.

## Correct the owning cause

- An approved face without a pixel-center sample retains its exact face-level response. Review UV allocation if texture variation is required there; do not automatically increase every asset to a larger atlas.
- A nonzero surface triangle with zero UV area cannot carry independent painted variation. Record its source ID, visibility and intended treatment. Do not delete it or declare an exception automatically.
- UV overlap may be intentional reuse. Check the involved surfaces and desired responses before re-unwrapping. Keep unresolved pixels explicit; no sampled-overlap result alone authorizes a UV edit.
- For AI repainting, use registered surface views/projection guides and reviewed masks. A fragmented atlas alone is not a reliable full-skin instruction. Preserve camera/UV correspondence and validate projected coverage and seams before applying output.
- A constant source PBR channel needs semantic authoring or a reviewed painting result. A higher-resolution export does not create the missing material information.

Applying a response/mask is a separate, counted material attempt with saved-source rendering, deliberate-change checks, geometry/UV/image/weight/bind retention and exchange verification. Applying face IDs to another source revision requires an independently checked correspondence bridge.

## Resume without discovery scans

Read the model catalog, then the exact job and latest mask review. Track unresolved stable IDs, source hashes, masks, defects and the next action there. Freeze the tools, profiles, evidence and parent dependencies; preserve prior attempts and do not claim a reference-only kit is self-contained.
