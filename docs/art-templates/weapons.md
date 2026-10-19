# Weapon Image Prompt Template

Use this reusable prompt to create a modeling and rendering reference for one weapon; see [README.md](README.md) for how the template fits the art-reference workflow.

This prompt defaults to a concept sheet. For a separate clean single-weapon image, apply [the shared asset-view override](generate-images.md#clean-asset-view-prompt-block); keep detail/stowed views and scale cues in the sheet only.

```text
Create a clear visual reference sheet for one fictional weapon intended for later manual Blender modeling and rendering.

MAIN CONCEPT: [Describe the weapon in one sentence.]

Treat the concept sentence as the source of truth for the weapon's identity and defining features.
Infer all unspecified details into a coherent, buildable design.
The working default is grounded high-fantasy frontier craftsmanship: practical materials, believable construction, restrained ornament, and visible evidence of how the object is made and used.
This is a proposed working default, not approved art direction.
Any attached existing project art takes precedence for its established visual language.
Use its appearance, materials, and mood only; do not reproduce reference typography, logos, or collage layout.

Show the same single asset consistently in every view.
Do not redesign, omit, or add parts between views.
Give the weapon a readable overall silhouette and proportions suited to a human hand.
Unless the concept calls for another form, use a practical one-handed weapon with a comfortable grip and plausible balance.
Let the concept override that default wherever it specifies otherwise.

Present a clean reference sheet with:
- A large three-quarter view as the primary view.
- A clear side view and front or end view of the same weapon.
- A small detail view only where a feature needs clarification.
- A safe sheathed or stowed view when that makes sense for this weapon.

Keep the views visually consistent in shape, materials, color, and construction.
Orient views so the silhouette and major proportions are easy to read.
Include enough visible detail to distinguish blade or striking surface, grip or haft, pommel, guard, fittings, fasteners, and joins where applicable.
Show believable thickness and transitions between joined parts.
Explain moving parts visually only when the concept includes them; keep their arrangement plausible and safe.

Use materials that communicate their likely real-world equivalents, such as forged metal, wood, leather, cord, bone, or cloth when suitable.
Keep surfaces legible and avoid decorative clutter that obscures construction.
Do not add text, labels, logos, or invented insignia.

Use a plain neutral background, even soft lighting, and restrained shadows.
Keep reflections controlled so material boundaries and edges remain visible.
Frame every view with comfortable margins.
Make the weapon large enough to inspect without cropping any part.

Include a simple scale cue in meters and make the overall dimensions believable relative to a human hand and body.
The scale cue is an approximate visual aid, not a claim of exact measurement.
Do not imply that the image is a mesh, a CAD model, or a dimensionally exact orthographic drawing.

Prioritize a useful, consistent reference for a human artist to interpret and model later.
Avoid action poses, hands covering key details, dramatic perspective distortion, exploded assembly diagrams, unsafe assembly instructions, and visual effects that hide the object's form.
```

The generated image is a visual reference only; verify proportions, dimensions, joins, and modeling details before building the asset in Blender.
