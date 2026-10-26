"""Assemble evidence for the completed Voidreign comparison."""
import hashlib,json
from pathlib import Path
root=Path('/mnt/c/Projects/Shattered-Frontier/GameClient/ArtSource/Voidreign/Trellis2-20261018')
lock=json.loads((root/'software-lock.json').read_text())
report=dict(asset='Voidreign',date='2026-10-18',method='Local TRELLIS.2-4B inference using ATLAS ROCm port with recorded PyTorch 2.14/ROCm 10.1 compatibility patches',status='visually reviewed generated concept prototype',selected_run='1024',blender_version='5.1.1',coordinates='metres; Z up; muzzle -Y; ground Z=0',inferred_full_height_m=2.6,inputs=lock['input_sha256'],runs={},outputs={},checks=['Radeon FP16 arithmetic and 10 GB memory write/read probe','Fused SDPA adapter compared to explicit FP32 attention','512 and 1024 inference and textured GLB export','Fresh-process saved-source rendering','GLB and FBX empty-scene geometry/scale/UV/material/image checks','Independent visual review against original and cleaned references'],limitations=['Dense joined triangulated mesh; no retopology, rig, animation, collision, LOD or Unreal acceptance','Short horn, broad looped mane/tail, simplified chains and charms, leg fringe and hoof artifacts remain','Cleaned conditioning image is a synthetic derivative and changes some reference details','Resolution and texture size both differ between runs','GPU peak memory was not measured'])
for run in ['512','1024']:
    path=root/run
    report['runs'][run]={'generation':json.loads((path/'run.json').read_text(encoding='utf-8-sig')),'review':json.loads((path/'asset-review.json').read_text()),'validation':json.loads((path/'validation-report.json').read_text())}
    for name in ['Voidreign.blend','Voidreign.glb','Voidreign.fbx','Voidreign.generated.glb','Voidreign.raw.ply']:
        file=path/name
        report['outputs'][f'{run}/{name}']={'bytes':file.stat().st_size,'sha256':hashlib.sha256(file.read_bytes()).hexdigest()}
(root/'asset-manifest.json').write_text(json.dumps(report,indent=2))
print('Completed manifest with output hashes and recorded evidence')
