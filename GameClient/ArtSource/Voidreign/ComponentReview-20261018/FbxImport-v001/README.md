# Animated FBX import correction

CPU-only, Blender5.1.1. Original source, FBX, GLB, geometry, textures and animation keys are unchanged. This is an explicit **Blender import adapter**, not a repaired FBX that every application will interpret identically.

## Cause and result

Blender's FBX importer connects children whose rest heads coincide with their parent's tail, even with `force_connect_children=False`. Connected Blender bones ignore their animated local translations. This rig deliberately has disconnected joints to represent simulated segment-length changes. Seventeen imported hair joints were incorrectly connected.

`rig-import-contract.json` records the source hierarchy/connectivity, FPS and source/exchange hashes. `tools/BlenderRigImport.py` checks the exact FBX hash and hierarchy, then restores the authored connectivity. No rest positions, keys, weights or tolerances are changed.

| Fresh import | Maximum bone-head error, all330 integer frames | Maximum deformed-surface error, sampled frames | Result |
| --- | --- | --- | --- |
| GLB | 0.00195mm | 0.0720mm | Pass |
| FBX default | 11.24mm | 11.08mm | Fail |
| FBX with source contract | 0.00370mm | 0.00213mm | Pass |

The earlier5.86mm FBX result covered seven frames; this check covers all330 frames for bone transforms and18 frames for full deformed surfaces. The limits remain5mm for heads and0.1mm for sampled surfaces. `validation.json` records exact measurements, frames, hashes and changed connections. Nearest rest-position correspondence accounts for duplicated/reordered UV vertices; that correspondence is valid for this spatially weighted mesh, not arbitrary split vertices with different skinning.

Open `Voidreign-FBX-Reimport.blend` to inspect the corrected empty-scene import. This has no rider, test post, lights or preview camera because those are intentionally excluded from asset exports. The source `.blend` remains the current visible model. Use GLB for Blender exchange when a sidecar adapter is undesirable. Other applications require their own fresh import/deformation check; Unreal has not been tested.

## Reproduce

From the repository root in PowerShell, choose a new evidence folder:

```powershell
$blender = 'C:/Program Files/Blender Foundation/Blender 5.1/blender.exe'
$sourceFolder = 'GameClient/ArtSource/Voidreign/ComponentReview-20261018/Cleanup-v001'
$evidence = 'Artifacts/Art/Voidreign/fbx-import-recheck-001'
& $blender --background --factory-startup --threads 4 --python-exit-code 1 --python tools/BlenderRigImport.py -- capture --source "$sourceFolder/Voidreign-HairPrototype.blend" --exchange "$sourceFolder/Voidreign-HairPrototype.fbx" --armature 'RIG | mane + tail only' --output "$evidence/rig-import-contract.json"
& $blender --background --factory-startup --threads 4 --python-exit-code 1 --python tools/Validate-VoidreignRigImport.py -- --folder $sourceFolder --output $evidence
```

For another model, `BlenderRigImport.py capture/import` is generic for a single matching armature. The validator above is specific to this Voidreign mesh/330-frame rig. A direct import can use:

```powershell
& $blender --background --factory-startup --threads 4 --python-exit-code 1 --python tools/BlenderRigImport.py -- import --exchange "$sourceFolder/Voidreign-HairPrototype.fbx" --contract "$evidence/rig-import-contract.json" --output "$evidence/Imported.blend"
```

Do not change every imported rig to disconnected joints without a source contract. Recapture the contract whenever the source or export changes. Existing outputs are never overwritten. Full hair coverage, attachment classification, startup settling, surface collision, stance, textures and gait remain separate open model gates.

