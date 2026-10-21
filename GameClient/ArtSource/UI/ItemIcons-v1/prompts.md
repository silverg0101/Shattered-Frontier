# Prompts and provenance

New art used the built-in image generation tool, one call per missing item. Shared prompt: “Use case: stylized-concept. Create ONE square fantasy-survival game inventory item icon, isolated on a genuinely transparent background (alpha). Match a unified HUD-v1 kit: painterly-realistic naturalistic hand-rendered object, muted material colors, three-quarter angle, soft warm upper-left light and gentle brass-colored rim light, crisp silhouette readable at 48 px, centered with generous margin. No ground, cast shadow, scenery, text, frame, border, watermark, extra objects, or background color.”

Subjects:

- `iron_nails`: a small tidy fan of eight hand-forged iron nails, squared heads, dark wrought metal.
- `hide_strips`: three narrow cut strips of coarse brown boar leather, loosely coiled, raw edges.
- `rough_stone`: one irregular rough gray quarry stone block, fractured faces, subtle moss flecks.
- `starter_tool_repair_kit`: a compact field tool repair kit: rolled weathered leather pouch tied with cord, small iron nails and wood handle peeking out.
- `foundation_material_bundle`: a compact bound stack of rough gray foundation stones and two short rough timber beams.
- `wall_material_bundle`: a compact bound stack of rough timber wall planks with a few hand-forged iron nails tucked into the cord.
- `door_material_bundle`: a compact bound bundle of short timber boards, iron hinges and hide tie strips, clearly material supplies rather than a complete door.
- `roof_material_bundle`: a compact bound stack of overlapping rough timber roof boards with hide ties and a few iron nails, clearly material supplies rather than a complete roof.

The other seven icon subjects come from `GameClient/ArtSource/UI/Icons-v1/{boar_hide,iron_axe,iron_ingot,iron_ore,iron_pickaxe,oak_log,rough_timber}.png`. Their original prompt is in that folder's `generate.sh`. Those opaque slate-background sources were segmented by low-luminance background and centered on transparent canvases. All 64 px images were downsampled from the corresponding 256 px exports with Pillow Lanczos.
