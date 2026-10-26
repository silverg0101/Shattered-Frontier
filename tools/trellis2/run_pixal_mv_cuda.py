"""Upstream Pixal3D multi-view inference on the RTX 3060 (CUDA) with no kernel substitutes.

Environment: setup-pixal-cuda.sh (env-pixal-cuda). Runs upstream inference_mv.py's own
init_pipeline/load_views/run_mv/to_glb sequence; differences from run_inference() are only:
- ATTN_BACKEND=sdpa (upstream's documented no-flash_attn path),
- the gated RMBG-2.0 matting model is replaced by a stub (views must carry alpha),
- steps/tokens/texture/decimation are arguments and textures are exported as PNG.
"""
import argparse,hashlib,json,os,sys,time,traceback
from pathlib import Path
os.environ.setdefault('ATTN_BACKEND','sdpa')
lab=Path(os.environ.get('SF_ASSET_LAB_ROOT','/home/grego/sf-asset-lab')).resolve()
source=lab/'sources/Pixal3D'
sys.path.insert(0,str(source));os.chdir(source)
p=argparse.ArgumentParser()
p.add_argument('--output',required=True);p.add_argument('--views',required=True);p.add_argument('--asset-name',required=True)
p.add_argument('--num-views',type=int);p.add_argument('--seed',type=int,default=42)
p.add_argument('--pipeline',choices=['1024_cascade','1536_cascade'],default='1024_cascade')
p.add_argument('--sampler-steps',type=int,default=12);p.add_argument('--max-num-tokens',type=int,default=49152)
p.add_argument('--texture-size',type=int,default=4096);p.add_argument('--decimation-target',type=int,default=1000000)
p.add_argument('--save-attributes',action='store_true',help='Also save mesh + material volume (local diagnostics; never commit)')
a=p.parse_args()
if not a.asset_name or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-' for c in a.asset_name):
    raise ValueError('--asset-name must contain only letters, numbers, underscore or hyphen')
out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
if (out/'run.json').exists():raise RuntimeError('Choose a new output folder')
import torch,numpy as np,trimesh,o_voxel
import inference_mv
from pixal3d.pipelines import rembg
class NoMatting:
    def __init__(self,**kwargs):pass
    def to(self,*args):return self
    def cpu(self):return self
    def __call__(self,image):raise RuntimeError('Prepared alpha views are required')
rembg.BiRefNet=NoMatting
viewsdir=Path(a.views).resolve();sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
model=json.loads((lab/'challengers/TencentARC--Pixal3D-mv.json').read_text())
report=dict(backend='Pixal3D-mv upstream CUDA',status='started',seed=a.seed,pipeline=a.pipeline,steps=a.sampler_steps,
    asset_name=a.asset_name,views=str(viewsdir),transforms_sha256=sha(viewsdir/'transforms.json'),
    torch=torch.__version__,cuda=torch.version.cuda,gpu=torch.cuda.get_device_name(0),model_revision=model['revision'],
    texture_size=a.texture_size,decimation_target=a.decimation_target,max_num_tokens=a.max_num_tokens,
    attention='upstream: sdpa dense/sparse, natten 0.21 NAF',stages={})
assert torch.version.cuda and '3060' in report['gpu']
def stage(name):
    report['stage']=name;(out/'run.json').write_text(json.dumps(report,indent=2));print(json.dumps({'stage':name}),flush=True)
try:
    stage('loading');t=time.perf_counter()
    # inference_mv.init_pipeline(low_vram=True), except the four extractors share one DINOv3
    # backbone (same weights): four CPU copies plus the flow models overflow WSL's 31 GB.
    pipeline=inference_mv.Pixal3DMVImageTo3DPipeline.from_pretrained(model['path'],'pipeline_mv.json')
    shared=None
    for name,config in inference_mv.IMAGE_COND_CONFIGS.items():
        cond=inference_mv.build_image_cond_model(config)
        if shared is None:shared=cond.model
        else:cond.model=shared
        if cond.use_naf_upsample:cond._load_naf()
        setattr(pipeline,'image_cond_model_'+name,cond)
    pipeline._device=torch.device('cuda');pipeline.low_vram=True
    report['stages']['load_seconds']=round(time.perf_counter()-t,3)
    views=inference_mv.load_views(str(viewsdir),a.num_views,rembg=None);inference_mv.check_main_view(views)
    stage('generating');t=time.perf_counter();torch.manual_seed(a.seed)
    tex=dict(steps=a.sampler_steps,guidance_strength=1.0,guidance_rescale=0.0,rescale_t=3.0)
    meshes,(_,_,res)=pipeline.run_mv(views,seed=a.seed,return_latent=True,pipeline_type=a.pipeline,max_num_tokens=a.max_num_tokens,
        sparse_structure_sampler_params=dict(steps=a.sampler_steps,guidance_strength=7.5,guidance_rescale=0.7,rescale_t=5.0),
        shape_slat_sampler_params=dict(steps=a.sampler_steps,guidance_strength=7.5,guidance_rescale=0.5,rescale_t=3.0),
        tex_slat_sampler_params=tex)
    mesh=meshes[0];report['stages']['inference_seconds']=round(time.perf_counter()-t,3)
    report['effective_grid_resolution']=res;report['raw_mesh']=dict(vertices=mesh.vertices.shape[0],faces=mesh.faces.shape[0])
    report['peak_vram_gb']=round(torch.cuda.max_memory_allocated()/2**30,2)
    if a.save_attributes:
        torch.save(dict(vertices=mesh.vertices.cpu(),faces=mesh.faces.cpu(),attrs=mesh.attrs.cpu(),coords=mesh.coords.cpu(),
            layout=pipeline.pbr_attr_layout,grid_size=res),out/f'{a.asset_name}.volume.pt')
    stage('exporting');t=time.perf_counter()
    glb=o_voxel.postprocess.to_glb(vertices=mesh.vertices,faces=mesh.faces,attr_volume=mesh.attrs,coords=mesh.coords,
        attr_layout=pipeline.pbr_attr_layout,grid_size=res,aabb=[[-0.5]*3,[0.5]*3],decimation_target=a.decimation_target,
        texture_size=a.texture_size,remesh=True,remesh_band=1,remesh_project=0,use_tqdm=True)
    glb.apply_transform(np.array([[-1,0,0,0],[0,0,-1,0],[0,-1,0,0],[0,0,0,1]],dtype=np.float64))
    glb.export(str(out/f'{a.asset_name}.glb'),extension_webp=False)
    report['stages']['export_seconds']=round(time.perf_counter()-t,3)
    report['files']={f'{a.asset_name}.glb':dict(sha256=sha(out/f'{a.asset_name}.glb'),bytes=(out/f'{a.asset_name}.glb').stat().st_size)}
    report['status']='complete';stage('complete')
except BaseException as error:
    report.update(status='failed',error=str(error));stage('failed');traceback.print_exc();raise
