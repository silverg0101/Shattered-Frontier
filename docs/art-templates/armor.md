# Armor Image Prompt Template

Use this reusable prompt to create a modeling and rendering reference for one armor design; see [README.md](README.md) for how the template fits the art-reference workflow.

This prompt defaults to a concept sheet. For a separate clean asset image, apply [the shared asset-view override](generate-images.md#clean-asset-view-prompt-block). Specify whether the intended asset is a dressed figure or individual armor piece; do not silently turn equipment into a complete character.

```text
Create a clear visual reference sheet for one fictional armor design intended for later manual Blender modeling and rendering.

MAIN CONCEPT: [Describe the armor in one sentence.]

Treat the concept sentence as the source of truth for the armor's identity and defining features.
Infer all unspecified details into a coherent, wearable design.
The working default is grounded high-fantasy frontier craftsmanship: practical materials, believable construction, restrained ornament, and visible evidence of how the equipment is worn and maintained.
This is a proposed working default, not approved art direction.
Any attached existing project art takes precedence for its established visual language.
Use its appearance, materials, and mood only; do not reproduce reference typography, logos, or collage layout.

Show the same single armor set consistently in every view, worn by the same neutral adult humanoid figure.
Unless the concept specifies another body type or scale, use a neutral figure approximately 1.8 meters tall.
Keep the figure in a relaxed neutral A-pose, upright with arms slightly away from the torso and feet comfortably apart.
Do not change the pose between views.

Present a clean reference sheet with:
- A large front view in the neutral A-pose.
- A matching side view and back view.
- A smaller three-quarter perspective view of the same set.
- A helmet detail view only if the concept includes a helmet.

Keep all views consistent in silhouette, layers, color, material, fit, and fastening.
Show how the armor covers the body while leaving plausible articulation at shoulders, elbows, wrists, hips, knees, and ankles as relevant.
Make overlapping plates, padded layers, garments, closures, and attachments readable.
Use plausible straps and fasteners that connect to real attachment points; avoid impossible routes, floating ends, and straps cutting through skin or armor.
Keep the design wearable and fitted, with practical clearance for movement.

Use materials that communicate their likely equivalents, such as metal, leather, cloth, quilted padding, wood, or hide when suitable.
Keep the figure's body neutral and unobtrusive.
Do not add weapons, shields, or unrelated equipment unless the concept explicitly includes them.
Do not add text, labels, logos, or invented insignia.

Use a plain neutral background, even soft lighting, and restrained shadows.
Keep reflections controlled so armor surfaces, edges, layers, and closures remain visible.
Frame every view with comfortable margins.
Show the full figure, including feet, without cropping.

Include a simple scale cue in meters, using the default 1.8-meter figure unless the concept states otherwise.
The scale cue is an approximate visual aid, not a claim of exact measurement.
Do not imply that the image is a mesh, a CAD model, or a dimensionally exact orthographic drawing.

Prioritize a useful, consistent reference for a human artist to interpret and model later.
Avoid action poses, dramatic perspective distortion, obscured body joints, impossible anatomy, and visual effects that hide the armor's construction.
```

The generated image is a visual reference only; verify fit, articulation, dimensions, attachment routes, and modeling details before building the asset in Blender.
