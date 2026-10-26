"""Record the exact local TRELLIS backend and its compatibility patches."""
import hashlib,json,subprocess,sys
from pathlib import Path
from importlib.metadata import version
lab=Path.home()/'sf-asset-lab'
asset=Path('/mnt/c/Projects/Shattered-Frontier/GameClient/ArtSource/Voidreign/Trellis2-20261018')
source=lab/'sources/trellis2-amd'
report={'python':sys.version,'packages':{n:version(n) for n in ['torch','torchvision','rocm','rocm-sdk-devel','transformers','trimesh','flex_gemm','cumesh','o_voxel','nvdiffrast']},'repositories':{},'input_sha256':{}}
for name,path in [('umbrella',source)]+[(p.name,p) for p in (source/'forks').iterdir() if (p/'.git').exists()]:
    revision=subprocess.check_output(['git','-C',str(path),'rev-parse','HEAD'],text=True).strip()
    url=subprocess.check_output(['git','-C',str(path),'remote','get-url','origin'],text=True).strip()
    patch=subprocess.check_output(['git','-C',str(path),'diff','--binary'])
    (asset/f'{name}.patch').write_bytes(patch)
    report['repositories'][name]={'url':url,'revision':revision,'local_patch':f'{name}.patch','patch_sha256':hashlib.sha256(patch).hexdigest()}
    license_dir=asset/'upstream-licenses';license_dir.mkdir(exist_ok=True)
    for license_file in path.glob('LICENSE*'):
        if license_file.is_file():
            target=license_dir/f'{name}-{license_file.name}'
            target.write_bytes(license_file.read_bytes())
for name in ['Voidreign-original.png','Voidreign-clean.png']:
    report['input_sha256'][name]=hashlib.sha256((asset/name).read_bytes()).hexdigest()
(asset/'software-lock.json').write_text(json.dumps(report,indent=2))
(asset/'python-packages.txt').write_text(subprocess.check_output([sys.executable,'-m','pip','freeze'],text=True))
print('Saved package lock, source revisions, patches and source hashes')
