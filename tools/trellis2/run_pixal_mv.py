"""Run official Pixal3D multi-view checkpoints on the verified Radeon torch/native stack.

--views is a folder of RGBA views plus transforms.json, in upstream inference_mv.py's format;
frame 0 must be the canonical front view. Settings and outputs match run_pixal.py.
"""
import argparse,hashlib,importlib,json,os,sys,time,traceback
from pathlib import Path
os.environ.update(ATTN_BACKEND='sdpa',SPARSE_ATTN_BACKEND='flash_attn',
    TORCH_ROCM_AOTRITON_ENABLE_EXPERIMENTAL='1',PYTORCH_NO_CUDA_MEMORY_CACHING='1',HF_HUB_OFFLINE='1')
lab=Path(os.environ.get('SF_ASSET_LAB_ROOT','/home/grego/sf-asset-lab')).resolve()
source=lab/'sources/Pixal3D'
sys.path.insert(0,str(source))
sys.path.insert(0,str(lab/'sources/NAF'))
import torch
from PIL import Image
import sdpa_adapter,naf_rocm
import inference_mv
# inference_mv sets expandable segments on import; the Radeon stack runs without them.
os.environ.pop('PYTORCH_CUDA_ALLOC_CONF',None)
p=argparse.ArgumentParser()
p.add_argument('--output',required=True)
p.add_argument('--views',required=True,help='Folder with transforms.json and RGBA views; frame 0 is the front view')
p.add_argument('--num-views',type=int,help='Use only the first N views')
p.add_argument('--asset-name',required=True,help='Safe output basename for this asset')
p.add_argument('--seed',type=int,default=42)
p.add_argument('--pipeline',choices=['1024_cascade','1536_cascade'],default='1024_cascade')
p.add_argument('--sampler-steps',type=int,default=12,help='Steps for sparse-structure, shape and texture samplers')
p.add_argument('--max-num-tokens',type=int,default=49152,help='Maximum high-resolution shape tokens')
p.add_argument('--texture-size',type=int,default=2048,help='Baked GLB texture dimension in pixels')
p.add_argument('--decimation-target',type=int,default=300000,help='Per-asset GLB triangle target; runtime LOD/Nanite budgets are separate')
p.add_argument('--remesh-band',type=int,default=1)
p.add_argument('--remesh-project',type=int,default=0)
a=p.parse_args()
for key in ['sampler_steps','max_num_tokens','texture_size','decimation_target']:
    if getattr(a,key) <= 0: raise ValueError(f'--{key.replace("_","-")} must be positive')
if a.remesh_band < 0 or a.remesh_project < 0: raise ValueError('--remesh-band and --remesh-project must be nonnegative')
viewsdir=Path(a.views).resolve()
transforms=viewsdir/'transforms.json'
if not transforms.is_file():raise FileNotFoundError(f'Views folder has no transforms.json: {viewsdir}')
frames=json.loads(transforms.read_text())['frames'][:a.num_views]
viewfiles=[viewsdir/f['file_path'] for f in frames]
for path in viewfiles:
    if not path.is_file():raise FileNotFoundError(f'View image does not exist: {path}')
if not a.asset_name or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-' for c in a.asset_name):
    raise ValueError('--asset-name must contain only letters, numbers, underscore or hyphen')
out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
if (out/'run.json').exists():raise RuntimeError('Choose a new output folder')
sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
report=dict(backend='Pixal3D-mv',status='started',seed=a.seed,pipeline=a.pipeline,steps=a.sampler_steps,
    asset_name=a.asset_name,views=str(viewsdir),transforms_sha256=sha(transforms),
    view_images=[{'file':path.name,'sha256':sha(path)} for path in viewfiles],
    torch=torch.__version__,hip=torch.version.hip,gpu=torch.cuda.get_device_name(0),
    asset_lab_root=str(lab),stages={},texture_size=a.texture_size,decimation_target=a.decimation_target,max_num_tokens=a.max_num_tokens,
    remesh_band=a.remesh_band,remesh_project=a.remesh_project,
    attention='dense and sparse PyTorch SDPA; NAF shifted dilated neighborhoods via chunked PyTorch')
assert torch.version.hip and '7900 XTX' in report['gpu']
def stage(name):
    report['stage']=name
    (out/'run.json').write_text(json.dumps(report,indent=2))
    print(json.dumps({'stage':name}),flush=True)
try:
    stage('loading extensions and weights')
    started=time.perf_counter()
    from pixal3d.pipelines import Pixal3DMVImageTo3DPipeline,rembg
    for name in ['full_attn','windowed_attn']:
        importlib.import_module('pixal3d.modules.sparse.attention.'+name).flash_attn=sdpa_adapter
    from src.model.naf import NAF
    from src.layers.attentions import CrossAttention
    CrossAttention.forward=naf_rocm.forward
    naf=NAF().eval()
    naf.load_state_dict(torch.load(lab/'challengers/NAF.pth',weights_only=True,map_location='cpu'))
    naf.requires_grad_(False)
    class AlphaOnly:
        def __init__(self,**kwargs):pass
        def to(self,*args):return self
        def cpu(self):return self
        def __call__(self,image):raise RuntimeError('Prepared alpha views are required')
    rembg.AlphaOnly=AlphaOnly
    model=json.loads((lab/'challengers/TencentARC--Pixal3D-mv.json').read_text())
    modelroot=Path(model['path'])
    config=json.loads((modelroot/'pipeline_mv.json').read_text())
    for name,value in config['args']['models'].items():config['args']['models'][name]=str(modelroot/value)
    config['args']['rembg_model']={'name':'AlphaOnly','args':{}}
    local=out/'local-config';local.mkdir()
    (local/'pipeline_mv.json').write_text(json.dumps(config,indent=2))
    pipeline=Pixal3DMVImageTo3DPipeline.from_pretrained(str(local),'pipeline_mv.json')
    pipeline.low_vram=True
    pipeline._device=torch.device('cuda')
    from pixal3d.trainers.flow_matching.mixins.image_conditioned_proj import DinoV3ProjMultiViewFeatureExtractor
    downloads=json.loads((lab/'model-downloads.json').read_text())
    dino=next(d['path'] for d in downloads if d['repo']=='PIA-SPACE-LAB/dinov3-vitl-pretrain-lvd1689m')
    shared=None
    for name,config in inference_mv.IMAGE_COND_CONFIGS.items():
        cond=DinoV3ProjMultiViewFeatureExtractor(**{**config,'model_name':dino})
        if shared is None:shared=cond.model
        else:cond.model=shared
        if cond.use_naf_upsample:cond.naf_model=naf
        cond.eval()
        setattr(pipeline,'image_cond_model_'+name,cond)
    report['model_revision']=model['revision']
    report['stages']['load_seconds']=round(time.perf_counter()-started,3)
    stage('loading views')
    # No matting model: a view without real alpha fails here instead of being segmented.
    views=inference_mv.load_views(str(viewsdir),a.num_views,rembg=None)
    inference_mv.check_main_view(views)
    report['camera']=dict(fov_degrees=[round(float(x)*57.29578,3) for x in views['camera_angle_x'][0]],
        distance=[round(float(x),4) for x in views['camera_distance'][0]],mesh_scale=views['mesh_scale'])
    stage('generating shape and materials')
    started=time.perf_counter()
    torch.manual_seed(a.seed)
    with torch.inference_mode():
        mesh=pipeline.run_mv(views,seed=a.seed,pipeline_type=a.pipeline,max_num_tokens=a.max_num_tokens,
            sparse_structure_sampler_params={'steps':a.sampler_steps},
            shape_slat_sampler_params={'steps':a.sampler_steps},
            tex_slat_sampler_params={'steps':a.sampler_steps})[0]
    torch.cuda.synchronize()
    report['stages']['inference_seconds']=round(time.perf_counter()-started,3)
    report['raw_mesh']={'vertices':mesh.vertices.shape[0],'faces':mesh.faces.shape[0]}
    report['effective_grid_resolution']=round(1/float(mesh.voxel_size))
    import numpy as np,trimesh,o_voxel
    trimesh.Trimesh(mesh.vertices.cpu().numpy(),mesh.faces.cpu().numpy(),process=False).export(out/f'{a.asset_name}.raw.ply')
    stage('exporting textured GLB')
    started=time.perf_counter()
    mesh.simplify(16777216)
    glb=o_voxel.postprocess.to_glb(vertices=mesh.vertices,faces=mesh.faces,attr_volume=mesh.attrs,
        coords=mesh.coords,attr_layout=mesh.layout,voxel_size=mesh.voxel_size,aabb=[[-.5]*3,[.5]*3],
        decimation_target=a.decimation_target,texture_size=a.texture_size,remesh=True,
        remesh_band=a.remesh_band,remesh_project=a.remesh_project,verbose=True)
    # Same display coordinate conversion as the official Pixal inference entries.
    glb.apply_transform(np.array([[-1,0,0,0],[0,0,-1,0],[0,-1,0,0],[0,0,0,1]],dtype=np.float64))
    glb.export(str(out/f'{a.asset_name}.glb'),extension_webp=False)
    report['stages']['export_seconds']=round(time.perf_counter()-started,3)
    report['files']={name:{'sha256':sha(out/name),'bytes':(out/name).stat().st_size}
        for name in [f'{a.asset_name}.raw.ply',f'{a.asset_name}.glb']}
    report['status']='complete';stage('complete')
except BaseException as error:
    report.update(status='failed',error=str(error));stage('failed');traceback.print_exc();raise
