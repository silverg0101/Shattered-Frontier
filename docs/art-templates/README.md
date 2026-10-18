# Reusable concept-art prompts

Use these templates to generate consistent visual references for later Blender modeling and rendering. Fill in **MAIN CONCEPT**; the rest of each prompt supplies working defaults. These are art-production aids, not accepted gameplay designs, technical budgets or exact engineering drawings.

| Template | Use for | Example main concept |
| --- | --- | --- |
| [Mounts](mounts.md) | Rideable creatures and their tack | A broad-backed marsh stag with mossy antlers, carrying frontier scouts through wetlands. |
| [Creatures](creatures.md) | Wildlife, enemies and companions | A burrowing forest lizard with bark-like scales and a luminous throat. |
| [Weapons](weapons.md) | Handheld weapons and their visible construction | A ranger's hooked spear combining dark ash wood with weathered bronze fittings. |
| [Armor](armor.md) | Wearable armor, outfits and protective layers | A marsh scout's layered leather armor with reed-woven camouflage panels. |
| [Buildings](buildings.md) | Structures and architectural modules | A compact timber-and-stone frontier herbalist's workshop with a drying porch. |
| [Props](props.md) | Furniture, tools, containers and set dressing | A traveling cartographer's folding field desk with protected map storage. |
| [Terrain](terrain.md) | Local landscape studies and terrain features | A mossy limestone stream crossing with exposed roots and a worn footpath. |

## Generate a reference

1. Open the appropriate file and copy its entire fenced prompt.
2. Replace its one main-concept placeholder. No other fields are required; the concept overrides defaults where necessary.
3. Optionally attach [the existing project concept image](../Game-Design/concept-art/Concept.png), or a more relevant image from [the concept-art folder](../Game-Design/concept-art/). Instruct the generator to use style/material cues, not its text, branding or collage layout. Existing project art is sufficient as an optional shared reference; no extra generated style image is required to use these templates.
4. Generate one design sheet. Check that every view depicts the same design. If one view disagrees, refine that view using the selected sheet as the reference rather than introducing a new design.
5. Keep an approved sheet attached to later detail, equipment-variant or beauty-render prompts. Generate a separate atmospheric beauty shot after structural decisions are settled.

The grounded high-fantasy frontier style is a proposed working default adapted from the preceding prompt discussion. It does not replace an approved art bible. With no attached image, the written prompt remains usable. Unspecified proportions/materials are generator proposals to review, not new project canon.

## From reference to Blender

To create the model in a fresh agent session, use [Generate a Blender model from an image](generate-model.md). It contains a copyable request, default outputs, modeling steps and visual/export acceptance checks.

These prompts produce images, not Blender files, meshes, animation rigs, UVs, PBR maps or production heightfields. Confirm scale, proportions, anatomy and construction while modeling; generated orthographic views can disagree. Dimensions and human silhouettes here are modeling guides, not gameplay balance, collision or voxel configuration. Runtime terrain still follows its owning World/Construction contracts.

For each selected asset, preserve the main concept, complete prompt, actual reference images, generated sheet, generation tool/date and human review status with the art source. Store approved source assets/provenance under the project's existing `GameClient/ArtSource/` workflow after assigning an asset-specific folder; do not replace existing assets or infer import/production approval. No mandatory generation vendor or paid tool is introduced by this library.
