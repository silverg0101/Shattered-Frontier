# Material channel diagnosis

Six CPU emission previews were inspected: head and saddle for base color, roughness and metallic. See review.json and MaterialChannelAudit-v003 for measurements and limits.

Replay each render using its frozen tool-source.py, guides.json and render-report.json: source and frame as recorded, --samples8, --channel as recorded, --material Material_0, and a new output folder. No scene changes were saved.

## Required next material work

1. Review exact UV/source masks for skull/bone, leather, mane/hair, metal hardware and eye; do not derive semantic identity solely from dark/light albedo. Include overlap/unknown areas and both sides.
2. Bone/leather/hair are dielectric surfaces; metal response belongs to intended metal hardware. Do not replace this with an all-object constant that also changes hardware.
3. Author class-appropriate roughness; independently source grain/stitching/etched relief where geometry cannot carry it. Painted light is not a height map.
4. Separate eye/red supernatural emission from painted red color and retain it as a declared effect.
5. Save a new source/material version and provenance; render unlit channels, neutral/opposed lighting, closeups and full coverage. Keep all geometry/UV/bind retention and exchange checks.
