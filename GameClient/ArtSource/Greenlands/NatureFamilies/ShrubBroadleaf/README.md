# VEG-GreenlandsShrubBroadleaf-01

Three irregular, multi-stem broadleaf shrub variants for look-profile role `shrub_broadleaf`. The FBX meshes are in metres, Z up, front -Y, with their origin at ground-level footprint centre. Each has a low UCX woody-core hull and no ground plane. The variant render includes a ground and 1.8 m human scale reference only; neither is exported.

Pass 3 visual review: I inspected both `ShrubBroadleaf-Variants.png` (eye-level lineup with 1.8 m marker) and `ShrubBroadleaf-Detail-3m.png` (variant 00 from 3 m at eye level) after rebuilding. Compared with a wild hazel/bramble at a wood edge, each now reads as a lumpy leafy mound rather than a small tree: 24–26 slender canes start across a 0.3–0.5 m base, short suckers and side shoots bear leaves low on the stems, and outer foliage droops to the ground. Darker inner and lower leaves leave only glimpses of stems. The leaf sizes and muted brown spotting remain varied. The three mesh sizes are 2.52×2.02×1.70 m, 2.14×1.78×1.42 m and 1.74×1.50×1.10 m, at 57,659–58,074 triangles. The camera and render ground are not exported.

Regenerate from this folder with Blender 5.1.1 after acquiring the shared `../Sources` through `../acquire_sources.py`:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe' -b --factory-startup -P build_shrub.py
```

For the separate Unreal Editor session, use the same Greenlands nature import route as SaplingBroadleaf:

```powershell
py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/NatureFamilies/import_nature.py ShrubBroadleaf
```

That route reads `asset-manifest.json` and imports the meshes and `MI_ShrubBroadleaf_*` materials under `/Game/Frontier/Environment/Greenlands/Nature/`. Unreal import and placement have not run for pass 3.
