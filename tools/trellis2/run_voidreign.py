import argparse,hashlib,importlib,json,os,sys,time,traceback
from pathlib import Path

os.environ['ATTN_BACKEND']='sdpa'
os.environ['SPARSE_ATTN_BACKEND']='flash_attn'
os.environ['TORCH_ROCM_AOTRITON_ENABLE_EXPERIMENTAL']='1'
os.environ['PYTORCH_NO_CUDA_MEMORY_CACHING']='1'
os.environ.pop('PYTORCH_CUDA_ALLOC_CONF',None)
lab=Path('/home/grego/sf-asset-lab')
source=lab/'sources/trellis2-amd/forks/TRELLIS2-rocm'
sys.path.insert(0,str(source))
import torch
from PIL import Image
import sdpa_adapter

p=argparse.ArgumentParser()
p.add_argument('--output',required=True)
p.add_argument('--pipeline',choices=['512','1024_cascade','1536_cascade'],default='512')
p.add_argument('--seed',type=int,default=42)
p.add_argument('--texture-size',type=int,choices=[1024,2048,4096],default=1024)
p.add_argument('--steps',type=int,choices=range(12,49),default=12,metavar='12..48')
p.add_argument('--decimation-target',type=int,choices=[300000,1000000],default=300000)
a=p.parse_args()
out=Path(a.output); out.mkdir(parents=True,exist_ok=True)
if (out/'run.json').exists(): raise RuntimeError('Choose a new output folder')
imagepath=Path('/mnt/c/Projects/Shattered-Frontier/GameClient/ArtSource/Voidreign/Trellis2-20261018/Voidreign-clean.png')
report=dict(status='started',pipeline=a.pipeline,seed=a.seed,torch=torch.__version__,hip=torch.version.hip,gpu=torch.cuda.get_device_name(0),image=str(imagepath),image_sha256=hashlib.sha256(imagepath.read_bytes()).hexdigest(),attention='PyTorch SDPA; sparse varlen adapter',stages={})
report['texture_size']=a.texture_size
report['decimation_target']=a.decimation_target
report['sampler_steps']={'sparse_structure':a.steps,'shape':a.steps,'texture':a.steps}
report['max_num_tokens']=49152
assert torch.version.hip and '7900 XTX' in report['gpu']
def stage(name):
    report['stage']=name
    (out/'run.json').write_text(json.dumps(report,indent=2))
    print(json.dumps({'stage':name}),flush=True)

try:
    stage('loading native extensions')
    from trellis2.pipelines import Trellis2ImageTo3DPipeline,rembg
    for module in ['full_attn','windowed_attn']:
        importlib.import_module('trellis2.modules.sparse.attention.'+module).flash_attn=sdpa_adapter
    import o_voxel,trimesh
    class AlphaOnly:
        def __init__(self,**kwargs): pass
        def __call__(self,image): raise RuntimeError('A transparent prepared image is required')
        def to(self,*args): return self
        def cpu(self): return self
    rembg.AlphaOnly=AlphaOnly
    downloads=json.loads((lab/'model-downloads.json').read_text())
    models={x['repo']:Path(x['path']) for x in downloads}
    modelroot=models['microsoft/TRELLIS.2-4B']
    config=json.loads((modelroot/'pipeline.json').read_text())
    for name,value in config['args']['models'].items():
        if value.startswith('microsoft/TRELLIS-image-large/'):
            value=str(models['microsoft/TRELLIS-image-large']/value.split('/',2)[2])
        else: value=str(modelroot/value)
        config['args']['models'][name]=value
    config['args']['image_cond_model']['args']['model_name']=str(models['PIA-SPACE-LAB/dinov3-vitl-pretrain-lvd1689m'])
    config['args']['rembg_model']={'name':'AlphaOnly','args':{}}
    local=out/'local-config'; local.mkdir()
    (local/'pipeline.json').write_text(json.dumps(config,indent=2))
    if a.pipeline=='512':
        Trellis2ImageTo3DPipeline.model_names_to_load=[n for n in Trellis2ImageTo3DPipeline.model_names_to_load if '1024' not in n]
    stage('loading model weights')
    start=time.perf_counter()
    pipeline=Trellis2ImageTo3DPipeline.from_pretrained(str(local))
    pipeline.low_vram=True
    pipeline.cuda()
    report['stages']['load_seconds']=round(time.perf_counter()-start,3)
    image=Image.open(imagepath)
    assert image.mode=='RGBA' and image.getchannel('A').getextrema()[0]==0
    stage('generating shape and materials')
    start=time.perf_counter()
    mesh=pipeline.run(image,seed=a.seed,pipeline_type=a.pipeline,
        sparse_structure_sampler_params={'steps':a.steps},
        shape_slat_sampler_params={'steps':a.steps},
        tex_slat_sampler_params={'steps':a.steps},max_num_tokens=49152)[0]
    torch.cuda.synchronize()
    report['stages']['inference_seconds']=round(time.perf_counter()-start,3)
    report['raw_mesh']={'vertices':mesh.vertices.shape[0],'faces':mesh.faces.shape[0]}
    report['voxel_size']=float(mesh.voxel_size)
    report['effective_grid_resolution']=round(1/float(mesh.voxel_size))
    trimesh.Trimesh(vertices=mesh.vertices.cpu().numpy(),faces=mesh.faces.cpu().numpy(),process=False).export(out/'Voidreign.raw.ply')
    stage('exporting textured GLB')
    start=time.perf_counter()
    mesh.simplify(16777216)
    glb=o_voxel.postprocess.to_glb(vertices=mesh.vertices,faces=mesh.faces,attr_volume=mesh.attrs,coords=mesh.coords,attr_layout=mesh.layout,voxel_size=mesh.voxel_size,aabb=[[-.5,-.5,-.5],[.5,.5,.5]],decimation_target=a.decimation_target,texture_size=a.texture_size,remesh=True,remesh_band=1,remesh_project=0,verbose=True)
    glb.export(str(out/'Voidreign.glb'),extension_webp=False)
    report['stages']['export_seconds']=round(time.perf_counter()-start,3)
    report['status']='complete'
    report['peak_torch_memory_bytes']=torch.cuda.max_memory_allocated() or None
    report['memory_note']='Peak GPU memory is not measured with the disabled caching allocator; native HIP allocations are also outside PyTorch accounting.'
    stage('complete')
except BaseException as e:
    report['status']='failed';report['error']=str(e)
    stage('failed');traceback.print_exc();raise
