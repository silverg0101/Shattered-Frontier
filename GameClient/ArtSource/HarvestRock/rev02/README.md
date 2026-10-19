# Harvest rock revision 02 — untouched resource shell

Original Blender prototype corrected after the first in-engine review. The two artificial geological side-plane cuts in revision 01 made the untouched resource look already mined. They are removed here. Intact uses an asymmetrical organic shell; its only planar closure is concealed at the ground. Runtime hit-location fracture is separate gameplay work. The three static extraction states remain comparison/source meshes and are excluded from both review renders.

`tools/Create-HarvestRock.py` authored all nine meshes together and created a new shared 2048px UV atlas for this exact revision: BaseColor (sRGB), prototype Roughness and OpenGL tangent Normal (non-color). Do not reuse V1 textures with rev02 UVs. Source colour is the preserved GPT-generated ironstone from `../../HarvestReferences/Ironstone-BaseColor-v1.png`, hash in the manifest; its adjacent prompt is the generation record. This is generated imagery, not scanned/photogrammetric PBR or guaranteed seamless material.

`HarvestRock.blend` is editable source; select only `SM_HarvestRock_` for exports. `Review_` objects are preview instances. `Exports/` holds nine individual FBX files and combined GLB. Metres, Z up; main resource origins are at ground, fragments centered. Slot0 is atlas exterior; slot1 is standalone interior colour with Blender box projection. Unreal must implement its own projected interior material; FBX does not transfer Blender projection logic. Source/intact carries no pre-baked extraction face above ground.

Reproduce into a **new** folder from repository root:

```powershell
& 'C:/Program Files/Blender Foundation/Blender 5.2/blender.exe' --background --threads 4 --factory-startup --python tools/Create-HarvestRock.py -- --output '<new folder>' --base-color GameClient/ArtSource/HarvestReferences/Ironstone-BaseColor-v1.png
```

Then run Blender with `--background --threads 4 <saved blend> --python <rev02/Review-Exports.py>` for saved-source rendering and clean-scene reimport checks. Exact topology/bounds and output hashes are in `manifest.json`; `exchange-validation.json` records reimports. Preview lighting is neutral and reveals the dark mineral colour. This does not diagnose the independently observed washed-out Unreal material defect.

No Unreal import, collision/LOD authoring, runtime cuts, audio/VFX or production-quality acceptance is included. In-engine local fracture shape, material continuity, normal convention and close-play texture density still require review. Keep V1 source/imports intact until this revision passes the serial integration review.
