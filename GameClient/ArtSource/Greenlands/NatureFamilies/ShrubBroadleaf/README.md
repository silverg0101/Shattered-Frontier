# VEG-GreenlandsShrubBroadleaf-01

Three irregular, multi-stem broadleaf shrub variants for look-profile role `shrub_broadleaf`. The FBX meshes are in metres, Z up, front -Y, with their origin at ground-level footprint centre. Each has a low UCX woody-core hull and no ground plane. The variant render includes a ground and 1.8 m human scale reference only; neither is exported.

Pass 2 visual review: I inspected both `ShrubBroadleaf-Variants.png` (eye-level three-quarter lineup) and `ShrubBroadleaf-Detail-3m.png` (variant 00 from 3 m at eye level), then widened the lineup, reframed the close view, opened the spaces between leaf lobes, lowered outer foliage, and darkened the bark. The final images show distinct uneven tops, drooping outer clumps, visible openings, a tapered gap below the foliage, and curved bark-textured stems from a narrow base. No bare twig projects above the leaf mass. The 3 m view shows varied leaf size, direction and value; the inner and lower foliage receives darker, less saturated vertex-colour blending. The three mesh sizes are 2.52×2.02×1.70 m, 2.14×1.78×1.42 m and 1.74×1.50×1.10 m, at 32,356–32,684 triangles. The camera and render ground are not exported.

Regenerate from this folder with Blender 5.1.1 after acquiring the shared `../Sources` through `../acquire_sources.py`:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe' -b --factory-startup -P build_shrub.py
```

For the separate Unreal Editor session, use the same Greenlands nature import route as SaplingBroadleaf:

```powershell
py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/NatureFamilies/import_nature.py ShrubBroadleaf
```

That route reads `asset-manifest.json` and imports the meshes and `MI_ShrubBroadleaf_*` materials under `/Game/Frontier/Environment/Greenlands/Nature/`. Unreal import and placement have not run for pass 2.
