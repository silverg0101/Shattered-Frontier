# VEG-GreenlandsShrubBroadleaf-01

Three dense, multi-stem broadleaf shrub variants for look-profile role `shrub_broadleaf`. The FBX meshes are in metres, Z up, front -Y, with their origin at ground-level footprint centre. Each has a low UCX woody-core hull and no ground plane. The variant render includes a ground and human scale reference only; neither is exported.

Regenerate from this folder with Blender 5.1.1 after acquiring the shared `../Sources` through `../acquire_sources.py`:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe' -b --factory-startup -P build_shrub.py
```

For the separate Unreal Editor session, use the same Greenlands nature import route as SaplingBroadleaf:

```powershell
py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/NatureFamilies/import_nature.py ShrubBroadleaf
```

That route reads `asset-manifest.json` and imports the meshes and `MI_ShrubBroadleaf_*` materials under `/Game/Frontier/Environment/Greenlands/Nature/`. Verify the three imported mesh sizes, Nanite, masked two-sided foliage, and desired UCX collision handling in the editor; `ue-import-report.json` is pending until that session runs. The look-profile mesh swap and placement remain separate.
