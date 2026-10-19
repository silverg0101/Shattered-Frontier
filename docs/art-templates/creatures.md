# Creature image prompt template

[Library workflow](README.md)

This prompt defaults to a concept sheet. For a separate clean single-creature image, apply [the shared asset-view override](generate-images.md#clean-asset-view-prompt-block); keep scale figures in the sheet only.

```text
Create a polished visual reference sheet for a 3D creature intended for later Blender modeling and rendering.

MAIN CONCEPT: [Describe the creature in one sentence.]

Develop the creature from the main concept. The working visual default is grounded high fantasy on a frontier, but it is a proposed style direction, not approved canon. The main concept overrides every asset default. Infer unspecified details cohesively, with believable anatomy, materials, body mass, and scale. Prioritize a clear, memorable silhouette.

Show the same single creature consistently in every view. Do not add alternate creatures, unrelated props, a rider, or a scene. Unless the main concept says otherwise, depict a wild creature without a saddle, clothing, or humanoid armor. Use markings, horns, plates, feathers, fur, scales, and scars only when they support the concept.

Use a neutral, rig-friendly standing or resting pose with the body relaxed and its weight visibly supported. Avoid dramatic action, extreme foreshortening, overlapping limbs, or poses that hide anatomy. Make the head, torso, tail, and each major limb easy to distinguish. If the creature has wings, show them extended to the side, fully in frame, and clear of the body.

## Views and scale

Arrange front, side, rear, and three-quarter views of the same creature. Keep proportions, markings, appendages, and coloration consistent across views. Include a small adult human silhouette for scale, about 1.8 meters tall, and keep it visually secondary.

## Presentation

Use a plain, light neutral background, even studio lighting, and soft contact shadows. Keep the creature fully visible with ample space around its silhouette. Use a clean reference-sheet layout without decorative environment, dramatic effects, labels, captions, measurements, logos, or watermark. Keep edges and surface details crisp enough to guide a modeler.

Reference images are optional; no attachment is required. If project reference images are supplied, follow them first for visual language and materials. Use references as appearance guidance only. Do not reproduce their typography, logos, or collage layout.

Create a visual design reference, not a finished 3D asset or technical anatomy plate. Favor clear form and consistent design over photorealistic effects.
```

Generated views are visual guides, not dimensionally reliable meshes. Check proportions and forms in Blender before modeling decisions.
