# Harvest material sources

Created 2026-10-19 with the built-in GPT image generation tool, following the `imagegen` skill. Exact prompts are retained beside both PNGs. These are generated flat color sources, not scans, measured PBR data or automatically seamless materials.

- `OakBark-BaseColor-v1.png`: realistic mature oak bark color reference, used by the authored oak prototype. Normal/roughness derived by its generator are approximations requiring rendered review.
- `Ironstone-BaseColor-v1.png`: restrained dark layered ironstone color source, projected/baked by the rock generator. Runtime-cut faces use a separate projected material rather than sampling the packed exterior atlas.

The initial procedural rock crack pattern and oak bark were visually rejected before these sources were used. The oak crown still failed the professional visual bar after geometry revision; generated textures do not resolve that structural problem. Source meshes, UVs, interiors, topology, lighting and actual interaction must be reviewed together.
