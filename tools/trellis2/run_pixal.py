"""Run official Pixal3D checkpoints on the verified Radeon torch/native stack."""
import argparse,hashlib,importlib,json,math,os,sys,time,traceback
from pathlib import Path
os.environ.update(ATTN_BACKEND='sdpa',SPARSE_ATTN_BACKEND='flash_attn',
    TORCH_ROCM_AOTRITON_ENABLE_EXPERIMENTAL='1',PYTORCH_NO_CUDA_MEMORY_CACHING='1',HF_HUB_OFFLINE='1')
os.environ.pop('PYTORCH_CUDA_ALLOC_CONF',None)
lab=Path(os.environ.get('SF_ASSET_LAB_ROOT','/home/grego/sf-asset-lab')).resolve()
repo=Path(__file__).resolve().parents[2]
source=lab/'sources/Pixal3D'
sys.path.insert(0,str(source))
sys.path.insert(0,str(lab/'sources/NAF'))
sys.path.insert(0,str(lab/'sources/MoGe'))
import torch
from PIL import Image
import sdpa_adapter,naf_rocm
p=argparse.ArgumentParser()
p.add_argument('--output',required=True)
p.add_argument('--image',required=True,help='Single-object conditioning image; transparent PNG alpha is preferred')
p.add_argument('--reference-image',help='Original catalog reference, when --image is a prepared derivative')
p.add_argument('--asset-name',required=True,help='Safe output basename for this asset')
p.add_argument('--seed',type=int,default=42)
p.add_argument('--pipeline',choices=['1024_cascade','1536_cascade'],default='1024_cascade')
p.add_argument('--sampler-steps',type=int,default=12,help='Steps for sparse-structure, shape and texture samplers')
p.add_argument('--max-num-tokens',type=int,default=49152,help='Maximum high-resolution shape tokens')
p.add_argument('--texture-size',type=int,default=2048,help='Baked GLB texture dimension in pixels')
p.add_argument('--decimation-target',type=int,default=300000,help='Per-asset GLB triangle target; runtime LOD/Nanite budgets are separate')
p.add_argument('--remesh-band',type=int,default=1)
p.add_argument('--remesh-project',type=int,default=0)
p.add_argument('--save-attributes',action='store_true',help='Save generated geometry and material volume for later texture baking')
a=p.parse_args()
for key in ['sampler_steps','max_num_tokens','texture_size','decimation_target']:
    if getattr(a,key) <= 0: raise ValueError(f'--{key.replace("_","-")} must be positive')
if a.remesh_band < 0 or a.remesh_project < 0: raise ValueError('--remesh-band and --remesh-project must be nonnegative')
imagepath=Path(a.image).resolve()
if not imagepath.is_file():raise FileNotFoundError(f'Input image does not exist: {imagepath}')
referencepath=Path(a.reference_image).resolve() if a.reference_image else imagepath
if not referencepath.is_file():raise FileNotFoundError(f'Reference image does not exist: {referencepath}')
if not a.asset_name or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-' for c in a.asset_name):
    raise ValueError('--asset-name must contain only letters, numbers, underscore or hyphen')
out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
if (out/'run.json').exists():raise RuntimeError('Choose a new output folder')
report=dict(backend='Pixal3D',status='started',seed=a.seed,pipeline=a.pipeline,steps=a.sampler_steps,
    asset_name=a.asset_name,image=str(imagepath),image_sha256=hashlib.sha256(imagepath.read_bytes()).hexdigest(),
    reference_image=str(referencepath),reference_image_sha256=hashlib.sha256(referencepath.read_bytes()).hexdigest(),
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
    from pixal3d.pipelines import Pixal3DImageTo3DPipeline,rembg
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
        def __call__(self,image):raise RuntimeError('Prepared alpha image is required')
    rembg.AlphaOnly=AlphaOnly
    model=json.loads((lab/'challengers/TencentARC--Pixal3D.json').read_text())
    modelroot=Path(model['path'])
    config=json.loads((modelroot/'pipeline.json').read_text())
    for name,value in config['args']['models'].items():config['args']['models'][name]=str(modelroot/value)
    config['args']['rembg_model']={'name':'AlphaOnly','args':{}}
    local=out/'local-config';local.mkdir()
    (local/'pipeline.json').write_text(json.dumps(config,indent=2))
    pipeline=Pixal3DImageTo3DPipeline.from_pretrained(str(local))
    pipeline.low_vram=True
    pipeline._device=torch.device('cuda')
    from pixal3d.trainers.flow_matching.mixins.image_conditioned_proj import DinoV3ProjFeatureExtractor
    downloads=json.loads((lab/'model-downloads.json').read_text())
    dino=next(d['path'] for d in downloads if d['repo']=='PIA-SPACE-LAB/dinov3-vitl-pretrain-lvd1689m')
    shared=None
    for name,image_size,grid_size,target in [('ss',512,16,None),('shape_512',512,32,512),('shape_1024',1024,64,512),('tex_1024',1024,64,1024)]:
        cond=DinoV3ProjFeatureExtractor(model_name=dino,image_size=image_size,grid_resolution=grid_size,
            use_naf_upsample=target is not None,naf_target_size=target or 128)
        if shared is None:shared=cond.model
        else:cond.model=shared
        if target:cond.naf_model=naf
        cond.eval()
        setattr(pipeline,'image_cond_model_'+name,cond)
    report['model_revision']=model['revision']
    report['stages']['load_seconds']=round(time.perf_counter()-started,3)
    source_image=Image.open(imagepath)
    image=source_image.convert('RGBA' if source_image.mode=='RGBA' else 'RGB')
    image=pipeline.preprocess_image(image)
    image.save(out/f'{a.asset_name}.preprocessed.png')
    stage('estimating camera with MoGe2')
    started=time.perf_counter()
    from moge.model.v2 import MoGeModel
    moge_root=json.loads((lab/'challengers/Ruicheng--moge-2-vitl.json').read_text())['path']
    moge=MoGeModel.from_pretrained(str(Path(moge_root)/'model.pt')).cuda().eval()
    import numpy as np
    image_tensor=torch.from_numpy(np.array(image).astype(np.float32)/255).permute(2,0,1).cuda()
    with torch.inference_mode():camera=moge.infer(image_tensor)
    fx=float(camera['intrinsics'].squeeze()[0,0])
    angle=2*math.atan(1/(2*fx))
    distance=.5/math.tan(angle/2)
    params=dict(camera_angle_x=angle,distance=distance,mesh_scale=1.0)
    report['camera_params']=params
    report['stages']['camera_seconds']=round(time.perf_counter()-started,3)
    moge.cpu();del moge,camera,image_tensor
    stage('generating shape and materials')
    started=time.perf_counter()
    with torch.inference_mode():
        mesh=pipeline.run(image,camera_params=params,seed=a.seed,pipeline_type=a.pipeline,
            preprocess_image=False,max_num_tokens=a.max_num_tokens,
            sparse_structure_sampler_params={'steps':a.sampler_steps},
            shape_slat_sampler_params={'steps':a.sampler_steps},
            tex_slat_sampler_params={'steps':a.sampler_steps})[0]
    torch.cuda.synchronize()
    report['stages']['inference_seconds']=round(time.perf_counter()-started,3)
    report['raw_mesh']={'vertices':mesh.vertices.shape[0],'faces':mesh.faces.shape[0]}
    report['effective_grid_resolution']=round(1/float(mesh.voxel_size))
    import trimesh,o_voxel
    if a.save_attributes:
        stage('saving reusable material volume')
        torch.save(dict(vertices=mesh.vertices.cpu(),faces=mesh.faces.cpu(),attrs=mesh.attrs.cpu(),
            coords=mesh.coords.cpu(),layout=mesh.layout,voxel_size=mesh.voxel_size),out/'material-volume.pt')
    trimesh.Trimesh(mesh.vertices.cpu().numpy(),mesh.faces.cpu().numpy(),process=False).export(out/f'{a.asset_name}.raw.ply')
    stage('exporting textured GLB')
    started=time.perf_counter()
    mesh.simplify(16777216)
    glb=o_voxel.postprocess.to_glb(vertices=mesh.vertices,faces=mesh.faces,attr_volume=mesh.attrs,
        coords=mesh.coords,attr_layout=mesh.layout,voxel_size=mesh.voxel_size,aabb=[[-.5]*3,[.5]*3],
        decimation_target=a.decimation_target,texture_size=a.texture_size,remesh=True,
        remesh_band=a.remesh_band,remesh_project=a.remesh_project,verbose=True)
    # Same display coordinate conversion as the official Pixal inference entry.
    glb.apply_transform(np.array([[-1,0,0,0],[0,0,-1,0],[0,-1,0,0],[0,0,0,1]],dtype=np.float64))
    glb.export(str(out/f'{a.asset_name}.glb'),extension_webp=False)
    report['stages']['export_seconds']=round(time.perf_counter()-started,3)
    report['files']={name:{'sha256':hashlib.sha256((out/name).read_bytes()).hexdigest(),'bytes':(out/name).stat().st_size}
        for name in [f'{a.asset_name}.preprocessed.png',f'{a.asset_name}.raw.ply',f'{a.asset_name}.glb']}
    report['status']='complete';stage('complete')
except BaseException as error:
    report.update(status='failed',error=str(error));stage('failed');traceback.print_exc();raise
