"""Retexture a finished mesh with upstream TRELLIS.2-4B's mesh-texturing pipeline (CUDA, env-pixal-cuda).

Geometry stays exactly as given (Pixal3D's is good); the colour comes from TRELLIS.2's
image+shape texture flow conditioned on the front view, the split used by the owner's
visualbruno/ComfyUI-Trellis2 workflow. Upstream Trellis2TexturingPipeline.run(); local
changes: gated DINOv3/RMBG names point at the cached ungated copy / a no-matting stub (the
view must carry alpha), the sampler takes the workflow's CFG, and the output is put back in
the input mesh's frame and scale.
"""
import argparse,hashlib,json,os,sys,time
from pathlib import Path
os.environ.setdefault('ATTN_BACKEND','sdpa');os.environ['PYTORCH_CUDA_ALLOC_CONF']='expandable_segments:True'
lab=Path(os.environ.get('SF_ASSET_LAB_ROOT','/home/grego/sf-asset-lab'))
sys.path.insert(0,str(lab/'cuda/src/TRELLIS.2'))
p=argparse.ArgumentParser()
p.add_argument('--mesh',required=True,help='GLB/PLY/OBJ to texture (Y-up, as run_pixal_mv exports)')
p.add_argument('--image',required=True,help='RGBA front view');p.add_argument('--output',required=True,help='New folder')
p.add_argument('--asset-name',required=True);p.add_argument('--seed',type=int,default=12345)
p.add_argument('--steps',type=int,default=25);p.add_argument('--guidance',type=float,default=7.5,help='Upstream default 1.0; workflow 7.5')
p.add_argument('--guidance-rescale',type=float,default=0.01);p.add_argument('--rescale-t',type=float,default=3.0)
p.add_argument('--resolution',type=int,choices=[512,1024],default=1024);p.add_argument('--texture-size',type=int,default=4096)
p.add_argument('--keep-uvs',action='store_true',help='Bake into the input UVs instead of a fresh CuMesh unwrap')
a=p.parse_args()
out=Path(a.output);out.mkdir(parents=True,exist_ok=False)
import torch,numpy as np,trimesh
from PIL import Image
from huggingface_hub import hf_hub_download,snapshot_download
from trellis2.pipelines import Trellis2TexturingPipeline,rembg
from trellis2.modules import image_feature_extractor as ife
class NoMatting:
    def __init__(self,**kwargs):pass
    def to(self,*args):return self
    def cpu(self):return self
    def __call__(self,image):raise RuntimeError('Prepared alpha view is required')
rembg.BiRefNet=NoMatting
dino=snapshot_download('camenduru/dinov3-vitl16-pretrain-lvd1689m')
_init=ife.DinoV3FeatureExtractor.__init__
ife.DinoV3FeatureExtractor.__init__=lambda self,model_name,**k:_init(self,dino,**k)
revision='af44b45f2e35a493886929c6d786e563ec68364d'
root=Path(hf_hub_download('microsoft/TRELLIS.2-4B','texturing_pipeline.json',revision=revision)).parent
assert torch.version.cuda and '3060' in torch.cuda.get_device_name(0)
t=time.perf_counter()
pipe=Trellis2TexturingPipeline.from_pretrained(str(root),config_file='texturing_pipeline.json');pipe.cuda()
mesh=trimesh.load(a.mesh,force='mesh',process=False)
if not a.keep_uvs:mesh=trimesh.Trimesh(mesh.vertices,mesh.faces,process=False)
v=np.asarray(mesh.vertices);centre=(v.min(0)+v.max(0))/2;scale=0.99999/(v.max(0)-v.min(0)).max()
image=Image.open(a.image);assert image.mode=='RGBA'
params=dict(steps=a.steps,guidance_strength=a.guidance,guidance_rescale=a.guidance_rescale,rescale_t=a.rescale_t)
result=pipe.run(mesh,image,seed=a.seed,tex_slat_sampler_params=params,resolution=a.resolution,texture_size=a.texture_size)
result.vertices=np.asarray(result.vertices)/scale+centre  # back to the input mesh frame
result.export(str(out/f'{a.asset_name}.glb'))
sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
(out/'run.json').write_text(json.dumps(dict(backend='TRELLIS.2-4B texturing (upstream, CUDA)',model_revision=revision,
    mesh=str(Path(a.mesh).resolve()),mesh_sha256=sha(a.mesh),image=str(Path(a.image).resolve()),image_sha256=sha(a.image),
    seed=a.seed,sampler=params,resolution=a.resolution,texture_size=a.texture_size,keep_uvs=a.keep_uvs,
    gpu=torch.cuda.get_device_name(0),torch=torch.__version__,seconds=round(time.perf_counter()-t,1),
    peak_vram_gb=round(torch.cuda.max_memory_allocated()/2**30,2),faces=len(result.faces),
    glb_sha256=sha(out/f'{a.asset_name}.glb')),indent=2))
print('done',out)
