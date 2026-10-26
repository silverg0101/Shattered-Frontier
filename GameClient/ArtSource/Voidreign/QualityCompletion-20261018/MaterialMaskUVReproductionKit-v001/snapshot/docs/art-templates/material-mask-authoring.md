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

## Class-ID data contract

| PNG value | Meaning |
| --- | --- |
| 0 | Unreviewed/unknown mesh surface |
| 1 | Reviewed strand nonmetal response |
| 2 | Reviewed bone nonmetal response |
| 254 | Multiple UV faces cover this sample; unresolved |
| 255 | Unused atlas |

These values are data IDs, **not metallic, albedo or alpha values**. Use raw/non-color interpretation and nearest sampling for inspection. Preserve `mask-report.json`, its palette, source-face ownership and overlap arrays. New classes require a versioned contract rather than reinterpreting existing codes.

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
