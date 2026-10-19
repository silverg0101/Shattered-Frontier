# Mighty Oak Trees, prepared asset v1

Source: **“Mighty oak trees” by Jagobo**, [Sketchfab model](https://sketchfab.com/3d-models/mighty-oak-trees-4f6ab5594a8a415aba3f958682b9ced5), licensed [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Credit: **Mighty oak trees by Jagobo, CC BY 4.0.** Commercial reuse is allowed with attribution. The original download archive and source Blender file/textures are retained under `source/`; their SHA-256 values are recorded in `manifest.json`. The four exact source maps are also available separately in `materials/`, with role, color-space intent, alpha presence, and SHA-256 in the manifest.

This packet adapts Tree 3 from the supplied source. It scales the source to a 10 m full-tree height and keeps the source bark texture, normal, roughness, foliage texture/alpha, UVs, and branch silhouette. The lower trunk is split at 1.0 m; its ground rim and join cap are closed and welded, and the upper structure receives a matching join cap. The gameplay hit point near 0.85 m lies inside the closed lower mesh. Hidden end caps use slot 1, `M_HarvestOak_CutWood`, with the project’s existing endgrain color and roughness maps. The source foliage is a separate mesh with the original alpha material.

The prepared source contains three objects at one ground origin. Their measured FBX bounds are recorded in `validation/import-audit.json` (dimensions are X × Y × Z, meters):

- `SM_MightyOak_LowerTrunk`: 0.732 × 0.973 × 1.000 m, closed, 88 triangles; bark slot 0, cutwood slot 1.
- `SM_MightyOak_UpperStructure`: 13.634 × 12.257 × 8.964 m, 58,701 triangles; source branch structure above the 1.0 m join, with a matching cap in slot 1.
- `SM_MightyOak_Foliage`: 14.557 × 13.500 × 8.783 m, 78,240 triangles; original leaf geometry, texture, and alpha.

Use the three separate `.fbx` files in `exports/` for Unreal import. They were re-imported in Blender 5.2.2 for mesh bounds, material slots, UV layer, and texture checks; the lower FBX has 0 nonmanifold edges and embeds both bark and cutwood texture sets. Import at uniform scale 1.0. The editable packed source is `exports/MightyOak_v1.blend`. `MightyOak_v1.glb` is a review export only; prefer the FBX files for runtime use.

`previews/` contains a neutral full-tree view, lower-trunk close-up, and isolated cut cap. Neither preview changes source geometry or materials.

Rebuild the asset and FBX/GLB exports with Blender 5.2.x:

```powershell
& 'C:/Program Files/Blender Foundation/Blender 5.2/blender.exe' --background --factory-startup --threads 4 --python 'GameClient/ArtSource/HarvestOak/MightyOak-v1/tools/Prepare-HarvestMightyOak.py'
```

Rerender previews with `tools/Render-HarvestMightyOak-Previews.py`. The source tree’s upper branch mesh retains its original terminal topology, and the foliage uses alpha-masked planes. Only the lower trunk and split join were topology-checked and capped for the harvesting use case; configure the foliage material as a masked material in Unreal and verify its leaf mask after import.

The duplicate original ZIP and optional review GLB remain local and are excluded from Git. The original editable `source/extracted/source/oaks.blend`, exact source maps, prepared Blender file and three tested FBXs are retained for reproducibility.
