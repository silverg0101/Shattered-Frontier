# Terrain image prompt template

[Library workflow](README.md)

This prompt creates a terrain concept study. Use [the image-only workflow](generate-images.md) to package it; a whole landscape study is not a ready-made reconstruction input or terrain system. Create separate clean rock, cliff, vegetation or other component concepts when those are the intended assets.

```text
Create a polished visual reference sheet for one local terrain patch intended for later Blender modeling and rendering.

MAIN CONCEPT: [Describe the terrain patch in one sentence.]

Develop the terrain from the main concept. The working visual default is grounded high fantasy on a frontier, but it is a proposed style direction, not approved canon. The main concept overrides every asset default. Infer unspecified details cohesively, with readable landforms, plausible transitions, and a clear sense of scale. Let the concept determine vegetation, surface materials, weathering, and natural or built landmarks.

Unless the main concept calls for another extent, depict a local patch about 32 meters by 32 meters. This is an art guide for one patch, not a runtime world chunk, topology specification, or gameplay boundary. Do not add a full map, distant region, or separate terrain assets. Include only details that help explain this patch.

Build a legible ground surface with natural macro slopes and recognizable forms. Keep paths, ridges, depressions, banks, and landmarks connected to the surrounding land. Choose a few stable landmarks that remain identifiable across views. Add vegetation, rocks, water, cliffs, or structures only when they fit the main concept; keep their placement and scale consistent. Avoid clutter that obscures the overall terrain shape.

## Views and scale

Show the same patch from a clear elevated perspective and a directly top-down view. Add a simple indicative side profile when it helps communicate slope and elevation. Align views so their features correspond. Include a small adult human figure, about 1.8 meters tall, where it does not hide important forms. Keep the landform readable from edge to edge.

Show transitions at patch edges clearly, but do not imply seamless tiling. Do not include a grid, coordinates, contour values, a production heightmap, texture channels, or a required voxel resolution. Avoid presenting the image as a playable map or technical terrain specification.

## Presentation

Use a plain, light neutral background outside the patch, even studio lighting, and soft shadows that do not hide relief. Keep the entire patch visible with ample space around it. Use a clean reference-sheet layout without a broad scenic backdrop, dramatic atmospheric effects, labels, captions, measurements, logos, or watermark. Favor clear form, material separation, and consistent landmarks over cinematic mood.

Reference images are optional; no attachment is required. If project reference images are supplied, follow them first for visual language and materials. Use references as appearance guidance only. Do not reproduce their typography, logos, or collage layout.

Create a visual reference for modeling, not a runtime chunk definition or production terrain data.
```

Generated views are visual guides, not dimensionally reliable meshes. Check proportions and forms in Blender before modeling decisions.
