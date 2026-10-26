"""Generate geometry from the frozen reference with Hunyuan3D-2.1 or TripoSG."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys
import time
import traceback

os.environ['PYTORCH_NO_CUDA_MEMORY_CACHING'] = '1'
os.environ.pop('PYTORCH_CUDA_ALLOC_CONF', None)
os.environ['TORCH_ROCM_AOTRITON_ENABLE_EXPERIMENTAL'] = '1'
os.environ['HF_HUB_OFFLINE'] = '1'
lab = Path('/home/grego/sf-asset-lab')
repo = Path(__file__).resolve().parents[2]
imagepath = repo / 'GameClient/ArtSource/Voidreign/Trellis2-20261018/Voidreign-clean.png'
p = argparse.ArgumentParser()
p.add_argument('--backend', choices=['hunyuan','tripo'], required=True)
p.add_argument('--output', required=True)
p.add_argument('--seed', type=int, default=42)
p.add_argument('--steps', type=int, choices=range(20,101), default=50)
a = p.parse_args()
out = Path(a.output)
out.mkdir(parents=True, exist_ok=True)
if (out / 'run.json').exists(): raise RuntimeError('Choose a new output folder')
import torch
from PIL import Image
import trimesh
report = dict(backend=a.backend, status='started', seed=a.seed, steps=a.steps,
    image=str(imagepath), image_sha256=hashlib.sha256(imagepath.read_bytes()).hexdigest(),
    torch=torch.__version__, hip=torch.version.hip, gpu=torch.cuda.get_device_name(0), stages={},
    texture='geometry-only initial comparison', decimation_target=300000)
assert torch.version.hip and '7900 XTX' in report['gpu']
def stage(name):
    report['stage'] = name
    (out / 'run.json').write_text(json.dumps(report, indent=2))
    print(json.dumps({'stage':name}), flush=True)
try:
    stage('loading weights')
    started = time.perf_counter()
    image = Image.open(imagepath)
    assert image.mode == 'RGBA' and image.getchannel('A').getextrema()[0] == 0
    if a.backend == 'hunyuan':
        sys.path.insert(0,str(lab / 'sources/hunyuan3d-2.1/hy3dshape'))
        from hy3dshape.pipelines import Hunyuan3DDiTFlowMatchingPipeline
        model = json.loads((lab / 'challengers/tencent--Hunyuan3D-2.1.json').read_text())
        pipe = Hunyuan3DDiTFlowMatchingPipeline.from_pretrained(model['path'])
        report.update(model_revision=model['revision'], octree_resolution=512, surface_extractor='upstream marching cubes', guidance_scale=5.0)
    else:
        sys.path.insert(0,str(lab / 'sources/TripoSG'))
        from triposg.pipelines.pipeline_triposg import TripoSGPipeline
        model = json.loads((lab / 'challengers/VAST-AI--TripoSG.json').read_text())
        pipe = TripoSGPipeline.from_pretrained(model['path']).to('cuda',torch.float16)
        report.update(model_revision=model['revision'], dense_octree_depth=8, hierarchical_octree_depth=9,
            surface_extractor='upstream hierarchical marching cubes; flash decoder disabled', guidance_scale=7.0)
        sys.path.insert(0,str(lab / 'sources/TripoSG/scripts'))
        from image_process import prepare_image
        import numpy as np
        image = prepare_image(str(imagepath),bg_color=np.array([1.0,1.0,1.0]),rmbg_net=None)
        report['preprocessing']='upstream prepare_image using supplied alpha; no segmentation model'
    report['stages']['load_seconds'] = round(time.perf_counter()-started,3)
    stage('generating geometry')
    started=time.perf_counter()
    generator=torch.Generator(device='cuda').manual_seed(a.seed)
    if a.backend == 'hunyuan':
        mesh = pipe(image=image,num_inference_steps=a.steps,generator=generator,
            octree_resolution=512,mc_algo='mc',num_chunks=8000)[0]
    else:
        result=pipe(image=image,generator=generator,num_inference_steps=a.steps,
            guidance_scale=7.0,use_flash_decoder=False,dense_octree_depth=8,hierarchical_octree_depth=9)
        mesh=result.meshes[0]
    torch.cuda.synchronize()
    report['stages']['inference_seconds']=round(time.perf_counter()-started,3)
    report['raw_mesh']={'vertices':len(mesh.vertices),'faces':len(mesh.faces)}
    mesh.export(out / 'Voidreign.raw.ply')
    stage('exporting geometry')
    if len(mesh.faces)>300000:
        mesh=mesh.simplify_quadric_decimation(face_count=300000)
    report['exported_mesh']={'vertices':len(mesh.vertices),'faces':len(mesh.faces)}
    mesh.export(out / 'Voidreign.glb')
    report['status']='complete'
    stage('complete')
except BaseException as error:
    report.update(status='failed',error=str(error))
    stage('failed')
    traceback.print_exc()
    raise
