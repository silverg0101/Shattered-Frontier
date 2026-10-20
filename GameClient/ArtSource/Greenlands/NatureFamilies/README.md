# Greenlands nature families (Silverbrook)

Four asset families, four variants each, all from CC0 Poly Haven data (scans, models, textures); no Fab, Megascans or stylized assets.

| Family | Role | Folder |
|---|---|---|
| VEG-GreenlandsPineTall-01 | `tree_pine_tall` | `PineTall/` |
| VEG-GreenlandsBroadleafYoung-01 | `tree_broadleaf_young` | `BroadleafYoung/` |
| ROCK-GreenlandsBoulderLargeMoss-01 | `boulder_large_moss` | `BoulderLargeMoss/` |
| ROCK-GreenlandsBedrockSlab-01 | `bedrock_slab` | `BedrockSlab/` |

- `acquire_sources.py`: downloads models/scans to `Sources/` with md5 verification and `Sources/provenance.json` (large `.bin`/`.gltf` git-ignored; rerun to restore).
- `nature_kit.py`, `rock_family.py`: shared Blender helpers (materials, vertex-colour moss, export, lineup renders, manifest).
- Each family folder: build script, FBX per variant, `.blend`, renders, `asset-manifest.json`, README. Material specs in the manifest are the single description used by the Blender look and by Unreal.
- `import_nature.py`: full-editor import into `/Game/Frontier/Environment/Greenlands/Nature/` (`py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/NatureFamilies/import_nature.py [Family ...]`). Masters `M_GN_Rock`, `M_GN_Wood`, `M_GN_Leaf`; `MI_` instances per spec; all four Nanite; rocks convex decomposition, trees trunk-only UCX prisms from the FBX (no canopy collision, no LOD chain). Not yet run: no editor answered on the remote-execution port.
- Mesh names are `SM_<Family>_00..03` (e.g. `SM_PineTall_00`).
