# Greenlands iron-bearing harvest rock

Editable art prototype for the first mining interaction. The intact boulder is a closed, low-density mesh suitable for evaluating hit-location cuts. The four authored extraction states are retained as comparisons/source, not a commitment to predetermined player damage. Runtime authority, collision and dynamic cuts are owned by the gameplay integration.

Original geometry is authored in Blender 5.2.2 LTS by `tools/Create-HarvestRock.py`. The colour source is GPT Image Gen output from the parent session's `../HarvestReferences/Ironstone-BaseColor-v1.png` and adjacent prompt. It is generated artwork, not photographed geology, scanned PBR or a guaranteed seamless texture. The first procedural orange crack-grid material failed visual review and was replaced with restrained ironstone imagery projected across the geology. Procedural roughness and fine tangent normals remain approximations.

## Contents

- `HarvestRock.blend`: editable source, source materials, shared baked material, review lighting/camera and hidden asset meshes. `Review_` objects are display instances; export only `SM_HarvestRock_` objects.
- `Exports/`: nine standalone FBX files and one combined GLB; no scene lights/ground/camera included.
- `Textures/`: shared 2048px UV atlas BaseColor (sRGB), Roughness and OpenGL tangent Normal (non-color). Unreal normal import requires the appropriate green-channel convention. `T_HarvestRock_InteriorBaseColor.png` is the generated source for a separately projected runtime fracture material; new cut faces must not sample arbitrary coordinates in the packed atlas.
- `HarvestRock_Stages.png` and `HarvestRock_Close.png`: saved-source render checks.
- `manifest.json` and `exchange-validation.json`: provenance, measured topology and clean-scene exchange checks.

Coordinates are metres, Z up. All four main stages share ground-level origin (0,0,0); chips are centered. Intact bounds are approximately 1.604 × 1.578 × 1.356 m, 196 vertices / 388 triangles. The outer shell and authored cuts are closed; exact checks are in the JSON. FBX material slot 0 is `M_HarvestRock_Exterior` (atlas), slot 1 `M_HarvestRock_Fracture` (standalone interior colour with Blender object-box projection). The latter needs an explicit equivalent material in Unreal: exchange files do not transfer Blender projection algorithms. Runtime cuts must use that projected interior material.

## Reproduction and focused review

From repository root, using a **new** output folder:

```powershell
& 'C:/Program Files/Blender Foundation/Blender 5.2/blender.exe' --background --threads 4 --factory-startup --python tools/Create-HarvestRock.py -- --output '<new folder>' --base-color GameClient/ArtSource/HarvestReferences/Ironstone-BaseColor-v1.png
```

The generator refuses overwrite. `Review-Exports.py` loads the saved scene, exports the meshes, renders lineup/close views and reimports each exchange file into an empty scene. Run it with Blender `--background <saved blend> --python <Review-Exports.py>`. `Revise-Material.py` records the bounded colour revision used on this packet; it is not a general asset tool.

Visual review removed the regular orange crack grid, reduced overbright preview lighting and smoothed exterior normals while preserving crisp fracture planes. This packet has no collision hulls, LODs, animation, sound, debris simulation or Unreal visual/performance acceptance. Flat extraction planes are comparison geometry; the user's desired experience uses contact-specific damage. Close-play texture density, mineral scale and runtime interior continuity still require in-engine review. Do not call this production-approved art.
