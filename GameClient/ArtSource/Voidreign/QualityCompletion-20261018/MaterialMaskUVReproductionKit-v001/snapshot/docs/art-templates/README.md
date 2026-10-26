# Reusable concept-art prompts

For finishing generated mounts, use the shared [mount secondary-motion procedure](mount-secondary-motion.md). Keep per-mount settings and results separate; [Voidreign's profile index](../../GameClient/ArtSource/Voidreign/QualityCompletion-20261018/MountProfile.md) links its authored settings, current source and experimental evidence.

Use these templates to generate consistent visual references for later Blender modeling and rendering. Fill in **MAIN CONCEPT**; the rest of each prompt supplies working defaults. These are art-production aids, not accepted gameplay designs, technical budgets or exact engineering drawings.

**Creating images in another session:** start with [the image-only production guide](generate-images.md), which includes a copyable handoff, review criteria and versioned package format. [Asset planning](asset-planning.md) defines basic → intermediate → detailed progression. Resume work through [the asset catalog](asset-catalog.md), then its named batch and direct package links; [image-jobs.md](image-jobs.md) supplies the batch format. This work can proceed while a separate session tests 3D models; it does not generate meshes or require a chosen 3D backend.

| Template | Use for | Example main concept |
| --- | --- | --- |
| [Mounts](mounts.md) | Rideable creatures and their tack | A broad-backed marsh stag with mossy antlers, carrying frontier scouts through wetlands. |
| [Creatures](creatures.md) | Wildlife, enemies and companions | A burrowing forest lizard with bark-like scales and a luminous throat. |
| [Weapons](weapons.md) | Handheld weapons and their visible construction | A ranger's hooked spear combining dark ash wood with weathered bronze fittings. |
| [Armor](armor.md) | Wearable armor, outfits and protective layers | A marsh scout's layered leather armor with reed-woven camouflage panels. |
| [Buildings](buildings.md) | Structures and architectural modules | A compact timber-and-stone frontier herbalist's workshop with a drying porch. |
| [Props](props.md) | Furniture, tools, containers and set dressing | A traveling cartographer's folding field desk with protected map storage. |
| [Terrain](terrain.md) | Local landscape studies and terrain features | A mossy limestone stream crossing with exposed roots and a worn footpath. |

## Generate a concept sheet

1. Open the appropriate file and copy its entire fenced prompt.
2. Replace its one main-concept placeholder. No other fields are required; the concept overrides defaults where necessary.
3. Optionally attach [the existing project concept image](../Game-Design/concept-art/Concept.png), or a more relevant image from [the concept-art folder](../Game-Design/concept-art/). Instruct the generator to use style/material cues, not its text, branding or collage layout. Existing project art is sufficient as an optional shared reference; no extra generated style image is required to use these templates.
4. Generate one design sheet. Check that every view depicts the same design. If one view disagrees, refine that view using the selected sheet as the reference rather than introducing a new design.
5. Keep the selected sheet attached when deriving the required clean asset view. Later detail, equipment-variant or beauty images are generated only when explicitly named in the active job's exact output list; no extra beauty shot is required by this template.

The grounded high-fantasy frontier style is a proposed working default adapted from the preceding prompt discussion. It does not replace an approved art bible. With no attached image, the written prompt remains usable. Unspecified proportions/materials are generator proposals to review, not new project canon.

## Prepare images for later 3D generation

The category prompts above default to **design sheets** for reviewing a concept. Save a separate **clean single-asset view** using the [shared prompt block and image checks](generate-images.md#clean-asset-view-prompt-block). One image should contain one complete asset, with neutral lighting and a transparent or uniform background. Applicable rider/scale figures, labels, inset details, alternate views and effects belong on the concept sheet or explicitly listed references, never the clean view; their mention does not request extra image files.

Keep both the concept sheet and the clean view, derived from the same selected design: exactly two selected images per ordinary job. Additional separate views require named files and an essential unresolved question in the finite brief; optional template suggestions do not increase the output count. Request one named image per generation call and stop when the declared list passes review. Additional views are not automatically suitable for multi-view reconstruction. Terrain studies and buildings may need separately indexed component concepts. Preserve native image resolution: source-image pixels and a model's geometry-grid resolution are different settings.

Store concept packages under `GameClient/ArtSource/<AssetId>/References/vNNN/` with prompts, source references, image hashes, dimensions, alpha state and separate concept/clean-view review statuses. These images can be ready ahead of model selection. Keep existing benchmark images fixed; select any replacement explicitly as a new experiment.

## From reference to Blender

To create the model in a fresh agent session, use [Generate a Blender model from an image](generate-model.md). It contains a copyable request, default outputs, modeling steps and visual/export acceptance checks.

For a finite queue of assets, use [the staged model pipeline](model-pipeline.md) and [model work catalog](model-catalog.md). They define category-specific stages, actual-mesh rigging, pinned profiles, bounded correction attempts and direct evidence links. The pipeline is an automation contract; current Voidreign tools are not a universal batch generator.

Use [the quality runbook](model-quality-runbook.md) for the practical generation-to-delivery sequence and confirmed repair recipes: seam-aware component review, texture detail versus atlas size, semantic hair selections, motion checks and animated exchange imports.

For reviewed material classes and texture-paint inputs, use [source-bound material masks](material-mask-authoring.md). It separates exact face responses from UV-texel authoring, measures resolution/overlap coverage, and preserves unknown regions.

These prompts produce images, not Blender files, meshes, animation rigs, UVs, PBR maps or production heightfields. Confirm scale, proportions, anatomy and construction while modeling; generated orthographic views can disagree. Dimensions and human silhouettes here are modeling guides, not gameplay balance, collision or voxel configuration. Runtime terrain still follows its owning World/Construction contracts.

For each selected asset, preserve the main concept, complete prompt, actual reference images, generated sheet, generation tool/date and human review status with the art source. Store approved source assets/provenance under the project's existing `GameClient/ArtSource/` workflow after assigning an asset-specific folder; do not replace existing assets or infer import/production approval. No mandatory generation vendor or paid tool is introduced by this library.

For downstream model finishing, the [quality runbook](model-quality-runbook.md) includes the reproducible check/repair sequence and a copyable model-batch prompt. Fixed-view texture repaint guides are derived from an existing mesh/camera and kept separate from concept/conditioning images; preserve their framing and do not treat painted detail as new geometry or PBR maps.


For mounts with moving secondary features, follow [the shared procedure](mount-secondary-motion.md), then the asset’s authored profile and job record. Voidreign’s current offline example is indexed in [ContactSurfaceDelivery-v001](../../GameClient/ArtSource/Voidreign/QualityCompletion-20261018/ContactSurfaceDelivery-v001/README.md); its full-duration surface passes are scoped separately from unresolved settling, semantic, material and runtime gates.

Mount animation: follow [primary rig and locomotion](mount-primary-motion.md) before final [secondary motion](mount-secondary-motion.md). Primary joint pilots and rigid-harness response tests must stay distinct from real gait clips and engine acceptance.
