# Building image prompt template

[Library workflow](README.md)

This prompt defaults to a concept sheet. For a separate clean building or module image, apply [the shared asset-view override](generate-images.md#clean-asset-view-prompt-block); omit the scale figure, cutaways and surroundings from that image. Record whether this is a whole-building concept or one module.

```text
Create a polished visual reference sheet for one building intended for later Blender modeling and rendering.

MAIN CONCEPT: [Describe the building in one sentence.]

Develop the building from the main concept. The working visual default is grounded high fantasy on a frontier, but it is a proposed style direction, not approved canon. The main concept overrides every asset default. Infer unspecified details cohesively, with believable construction, readable shapes, practical details, and a clear overall silhouette. Let the concept determine the building's purpose, character, materials, condition, and ornament.

Unless the main concept says otherwise, use a compact, single-story frontier structure. The concept may call for a different size, number of levels, or architectural tradition. Show one building only, without a surrounding settlement or unrelated structures. Add props only when they clarify the building's use, and keep them visually secondary.

Make the structure plausible to build. Show a clear path for loads through walls, posts, beams, and roof supports. Include practical entrances and windows where appropriate. Give the roof a believable pitch or drainage solution for its material and climate. Keep stairs, porches, chimneys, braces, and overhangs attached coherently. Avoid unsupported projections and ambiguous intersections.

## Views and scale

Arrange consistent front, side, rear, and three-quarter exterior views. Include a clear roof or top view when it helps explain the footprint. Keep dimensions, openings, roof form, materials, and details consistent across views. Include a small adult human silhouette, about 1.8 meters tall, and a doorway about 2.1 meters high as default art guides. Adjust either if the main concept clearly calls for different proportions. These guides are not game collision metrics.

If a cutaway clarifies the concept, include one simple schematic cutaway as a separate view, showing only useful interior structure. Keep it consistent with the exterior. Do not present it as an engineering plan or fill the sheet with construction annotations.

## Presentation

Use a plain, light neutral background, even studio lighting, and soft contact shadows. Keep the building fully visible with ample space around it. Use a clean reference-sheet layout without a landscape scene, dramatic effects, labels, captions, measurements, logos, or watermark. Make material boundaries and structural connections easy to read.

Reference images are optional; no attachment is required. If project reference images are supplied, follow them first for visual language and materials. Use references as appearance guidance only. Do not reproduce their typography, logos, or collage layout.

Create a visual design reference for modeling, not a construction document. Favor coherent form and clear views over precise technical specification.
```

Generated views are visual guides, not dimensionally reliable meshes. Check proportions and forms in Blender before modeling decisions.
