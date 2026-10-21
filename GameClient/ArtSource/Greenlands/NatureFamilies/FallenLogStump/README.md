# VEG-GreenlandsFallenLogStump-01

Three forest deadwood meshes: two slightly buried fallen logs, 5.10 m and 6.35 m long, with ragged broken ends and snapped branch bases; and a 1.27 m rooted stump with eight exposed, tapering roots. Origins are at ground contact; the buried portions extend below Z=0. No ground or human marker is exported.

`Col.R` stores a world-up moss mask with noise breakup. `MI_FallenLogStump_Bark` combines CC0 Poly Haven pine bark and mossy-rock textures, while the lighter `MI_FallenLogStump_BrokenWood` marks broken faces. Preserve vertex colours and both material slots on import. The CC0 licence rows and file hashes are in `asset-manifest.json` and `../Sources/provenance.json`.

`FallenLogStump-Variants.png` has a render-only 1.8 m marker; `FallenLogStump-Detail-3m.png` and `FallenLogStump-Stump-3m.png` are 3 m views at 1.6 m eye height. The bark, moss cap, bent silhouettes, broken ends and root flare were checked against the forest floor in `docs/Game-Design/concept-art/UI/v5/gameplay-hud.png`.

Regenerate from this folder with Blender 5.1.1:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe' -b --factory-startup -P build_fallen_log_stump.py
```

For a later Unreal Editor session, import with `py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/NatureFamilies/import_nature.py FallenLogStump`. The FBX meshes and material instances belong under `/Game/Frontier/Environment/Greenlands/Nature/`; enable Nanite and preserve vertex colours. Place at scale 1 with Z=0 on terrain, then set collision for the game route. Unreal import and placement have not run.
