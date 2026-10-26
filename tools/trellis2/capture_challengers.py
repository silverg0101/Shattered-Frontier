"""Capture backend package/source locks and compatibility evidence."""
import argparse,hashlib,json,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',required=True);a=p.parse_args()
out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
if (out/'backend-lock.json').exists():raise RuntimeError('Choose a new lock folder')
lab=Path('/home/grego/sf-asset-lab')
lock={'shared_runtime':'../Trellis2-20261018/software-lock.json','environments':{},'sources':{}}
lock['models']=[json.loads(path.read_text()) for path in sorted((lab/'challengers').glob('*.json'))]
for backend in ['hunyuan','tripo','pixal']:
    python=lab/f'env-{backend}/bin/python'
    packages=subprocess.check_output([str(python),'-m','pip','freeze'],text=True)
    (out/f'{backend}-packages.txt').write_text(packages)
    code="import json,torch,transformers,diffusers;print(json.dumps(dict(torch=torch.__version__,torch_file=torch.__file__,hip=torch.version.hip,transformers=transformers.__version__,diffusers=diffusers.__version__)))"
    versions=json.loads(subprocess.check_output([str(python),'-c',code],text=True))
    versions['python']=str(python)
    versions['base_site_packages_reused_read_only']=str(lab/'venv-current/lib/python3.12/site-packages')
    lock['environments'][backend]=versions
for name in ['hunyuan3d-2.1','TripoSG','Pixal3D','NAF','MoGe','hunyuan-rocm']:
    source=lab/'sources'/name
    git=lambda *args:subprocess.check_output(['git','-C',str(source),*args],text=True).strip()
    patch=git('diff','--binary')
    (out/(name+'.patch')).write_text(patch+'\n' if patch else '')
    lock['sources'][name]={'path':str(source),'revision':git('rev-parse','HEAD'),
        'remote':git('remote','get-url','origin'),'patch_sha256':hashlib.sha256((patch+'\n' if patch else '').encode()).hexdigest()}
    licenses=out/'licenses'/name;licenses.mkdir(parents=True,exist_ok=True)
    for filename in ['LICENSE','NOTICE','Notice.txt','LICENSE-WRAPPERS']:
        if (source/filename).is_file():shutil.copy2(source/filename,licenses/filename)
probe=Path('/mnt/c/Projects/Shattered-Frontier/Artifacts/Art/RadeonSetup/pixal-compatibility.json')
if probe.exists():lock['numerical_compatibility']=json.loads(probe.read_text())
lock['system_library']=subprocess.check_output(['dpkg-query','-W','libopengl0'],text=True).strip()
extension=lab/'sources/hunyuan3d-2.1/hy3dpaint/DifferentiableRenderer/mesh_inpaint_processor.cpython-312-x86_64-linux-gnu.so'
if extension.exists():lock['cpu_inpaint_extension']={'path':str(extension),'sha256':hashlib.sha256(extension.read_bytes()).hexdigest()}
(out/'backend-lock.json').write_text(json.dumps(lock,indent=2))
print('Backend environment/source locks and patches saved')
