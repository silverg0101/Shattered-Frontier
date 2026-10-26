"""Texture a saved shape using the community Hunyuan ROCm paint wrapper."""
import argparse,hashlib,json,os,sys,time,traceback
from pathlib import Path
os.environ.update(PYTORCH_ALLOC_CONF='',PYTORCH_HIP_ALLOC_CONF='',
    PYTORCH_NO_CUDA_MEMORY_CACHING='1',TORCH_ROCM_AOTRITON_ENABLE_EXPERIMENTAL='1',HF_HUB_OFFLINE='1')
os.environ.pop('PYTORCH_CUDA_ALLOC_CONF',None)
lab=Path('/home/grego/sf-asset-lab')
repo=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(lab/'sources/hunyuan-rocm'))
p=argparse.ArgumentParser()
p.add_argument('--mesh',required=True)
p.add_argument('--output',required=True)
p.add_argument('--resolution',type=int,choices=[256,512],default=256)
p.add_argument('--no-remesh',action='store_true',help='Preserve the input triangles instead of the painting remesh')
p.add_argument('--shape-backend',default='Hunyuan3D-2.1')
p.add_argument('--reuse-uv',action='store_true',help='Reuse an existing complete UV atlas; requires --no-remesh')
a=p.parse_args()
if a.reuse_uv and not a.no_remesh:p.error('--reuse-uv requires --no-remesh')
out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
if (out/'run.json').exists():raise RuntimeError('Choose a new output folder')
image=repo/'GameClient/ArtSource/Voidreign/Trellis2-20261018/Voidreign-clean.png'
report=dict(backend='Hunyuan3D-2.1 paint',status='started',image=str(image),
    image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),input_mesh=str(Path(a.mesh).resolve()),
    input_mesh_sha256=hashlib.sha256(Path(a.mesh).read_bytes()).hexdigest(),
    seed=0,shape_seed=42,steps=15,guidance_scale=3.0,scheduler='UniPCMultistepScheduler',
    seed_note='Upstream multiview helper explicitly seeds its texture diffusion with zero.',
    views=6,multiview_resolution=a.resolution,render_size=1024,texture_size=2048,
    rasterizer='community pure PyTorch ROCm',use_remesh=not a.no_remesh,shape_backend=a.shape_backend,reuse_uv=a.reuse_uv,
    geometry_note=('Painting retains the input triangles; UV unwrapping may split vertices.' if a.no_remesh else
        'Painting remeshes a separate copy to approximately 40k triangles; original geometry comparison is preserved.'))
def stage(name):
    report['stage']=name
    (out/'run.json').write_text(json.dumps(report,indent=2))
    print(json.dumps({'stage':name}),flush=True)
try:
    stage('loading painting backend')
    import setuptools  # Activate its vendored distutils for BasicSR on Python3.12.
    import torch,backend,paint
    from PIL import Image
    assert torch.version.hip and '7900 XTX' in torch.cuda.get_device_name(0)
    report.update(torch=torch.__version__,hip=torch.version.hip,gpu=torch.cuda.get_device_name(0))
    torch.manual_seed(0)
    b=backend.detect(force_kind='rocm',force_raster='torch')
    paint.apply_patches(b)
    dino=json.loads((lab/'challengers/facebook--dinov2-giant.json').read_text())
    if dino['status']!='complete':raise RuntimeError('DINOv2 giant download is incomplete')
    report['dino_revision']=dino['revision']
    import textureGenPipeline as tgp
    if a.reuse_uv:
        import numpy as np,trimesh
        def existing_uv(mesh):
            if isinstance(mesh,trimesh.Scene):mesh=mesh.dump(concatenate=True)
            uv=getattr(mesh.visual,'uv',None)
            if uv is None or np.asarray(uv).shape!=(len(mesh.vertices),2) or not np.isfinite(uv).all():
                raise RuntimeError('Input has no complete finite UV atlas')
            report['input_triangles']=len(mesh.faces)
            print(json.dumps({'stage':'reusing input UV atlas','triangles':len(mesh.faces)}),flush=True)
            return mesh
        tgp.mesh_uv_wrap=existing_uv
    config_init=tgp.Hunyuan3DPaintConfig.__init__
    def local_dino_config(self,*args,**kwargs):
        config_init(self,*args,**kwargs)
        self.dino_ckpt_path=dino['path']
    tgp.Hunyuan3DPaintConfig.__init__=local_dino_config
    settings=dict(max_num_view=6,resolution=a.resolution,render_size=1024,texture_size=2048)
    pipe=paint.load_pipeline(b,preset='low',overrides=settings)
    from diffusers.models.attention_processor import AttnProcessor
    pipe.models['multiview_model'].pipeline.vae.set_attn_processor(AttnProcessor())
    report['vae_attention']='Diffusers classic attention; avoids failing ROCm fused VAE head-dimension path'
    stage('painting mesh')
    started=time.perf_counter()
    result=paint.texture_mesh(a.mesh,Image.open(image),b,preset='low',
        overrides=settings,
        use_remesh=not a.no_remesh,output_dir=str(out))
    report['elapsed_seconds']=round(time.perf_counter()-started,3)
    report['outputs']=result
    if not result.get('glb'):raise RuntimeError('Paint did not produce a GLB')
    import shutil
    shutil.copy2(result['glb'],out/'Voidreign.glb')
    report['status']='complete';stage('complete')
except BaseException as error:
    report.update(status='failed',error=str(error));stage('failed');traceback.print_exc();raise
