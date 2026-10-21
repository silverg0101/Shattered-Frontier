# ROCK-GreenlandsBoulderMossy-01

Three irregular, scanned granite boulders, 1.90–2.55 m across. Each mesh has a shallow buried base below Z=0, with the origin at ground contact at the footprint centre. No ground or human marker is exported. Vertex colour `Col.R` masks moss by world-up-facing vertex normal, broken up by positional noise; the `MI_BoulderMossy_*` material specs blend Poly Haven moss over the granite. Keep that vertex colour on import.

`BoulderMossy-Variants.png` includes a render-only 1.8 m marker. `BoulderMossy-Detail-3m.png` views variant 00 from approximately 3 m at 1.6 m eye height. The weathered fractures, uneven profiles and restrained top moss were checked against `docs/Game-Design/concept-art/UI/v5/gameplay-hud.png`. The sources and their CC0 licences are recorded in `asset-manifest.json` and `../Sources/provenance.json`.

Regenerate from this folder with Blender 5.1.1:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe' -b --factory-startup -P build_boulder_mossy.py
```

For a later Unreal Editor session, import with `py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/NatureFamilies/import_nature.py BoulderMossy`. The three FBX meshes and `MI_BoulderMossy_*` material instances belong under `/Game/Frontier/Environment/Greenlands/Nature/`; enable Nanite, preserve vertex colours and place at scale 1 with Z=0 on terrain. Add project collision appropriate to placement. Unreal import and placement have not run.
