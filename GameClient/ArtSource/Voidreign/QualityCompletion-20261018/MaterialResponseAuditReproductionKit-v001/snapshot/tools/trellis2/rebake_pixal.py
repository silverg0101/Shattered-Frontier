"""Bake a locally saved Pixal material volume onto the fixed exported mesh/UVs.

Uses the same nvdiffrast, CuMesh BVH and sparse trilinear sampling as o_voxel.
Does not upscale an existing image or regenerate geometry/UVs.
"""
import argparse,hashlib,json,os,time
from pathlib import Path
os.environ['PYTORCH_NO_CUDA_MEMORY_CACHING']='1'
import torch,numpy as np,trimesh,cv2,cumesh
import nvdiffrast.torch as dr
from PIL import Image
from flex_gemm.ops.grid_sample import grid_sample_3d
p=argparse.ArgumentParser()
p.add_argument('--volume',required=True);p.add_argument('--mesh',required=True);p.add_argument('--output',required=True)
a=p.parse_args();out=Path(a.output);out.mkdir(parents=True,exist_ok=False)
assert torch.version.hip and '7900 XTX' in torch.cuda.get_device_name(0)
# Only load the material-volume cache produced locally by run_pixal.py.
with torch.serialization.safe_globals([slice]):
    data=torch.load(a.volume,map_location='cuda',weights_only=True)
mesh=trimesh.load(a.mesh,force='mesh',process=False)
vertices=torch.tensor(np.asarray(mesh.vertices)*[-1,1,-1],dtype=torch.float32,device='cuda')
faces=torch.tensor(np.asarray(mesh.faces),dtype=torch.int32,device='cuda')
uv=np.asarray(mesh.visual.uv).copy();uv[:,1]=1-uv[:,1]
uv=torch.tensor(uv,dtype=torch.float32,device='cuda')
ctx=dr.RasterizeCudaContext();bvh=cumesh.cuBVH(data['vertices'],data['faces'])
coords=data['coords'];attrs=data['attrs'];layout=data['layout']
voxel=torch.as_tensor(data['voxel_size'],device='cuda',dtype=torch.float32).flatten()
if voxel.numel()==1:voxel=voxel.repeat(3)
grid_size=(1/voxel).round().int();raw_faces=data['faces'].long()
rows=[]
for size in [2048,4096]:
    started=time.perf_counter();folder=out/f'{size}';folder.mkdir()
    print(f'Baking {size} onto fixed geometry and UVs',flush=True)
    positions=torch.cat([uv*2-1,torch.zeros_like(uv[:,:1]),torch.ones_like(uv[:,:1])],-1)[None]
    rast=torch.zeros((1,size,size,4),device='cuda')
    for start in range(0,len(faces),100000):
        block,_=dr.rasterize(ctx,positions,faces[start:start+100000],resolution=[size,size])
        valid=block[...,3:4]>0;block[...,3:4]+=start
        rast=torch.where(valid,block,rast)
    mask=rast[0,...,3]>0
    pos=dr.interpolate(vertices[None],rast,faces)[0][0][mask]
    del rast,positions
    values=torch.empty((len(pos),attrs.shape[1]),device='cuda')
    for start in range(0,len(pos),131072):
        sample=pos[start:start+131072]
        _,fid,bary=bvh.unsigned_distance(sample,return_uvw=True)
        projected=(data['vertices'][raw_faces[fid.long()]]*bary[...,None]).sum(1)
        values[start:start+len(sample)]=grid_sample_3d(attrs,torch.cat([torch.zeros_like(coords[:,:1]),coords],-1),
            shape=torch.Size([1,attrs.shape[1],*grid_size.tolist()]),
            grid=((projected+.5)/voxel).reshape(1,-1,3),mode='trilinear')
    image=torch.zeros((size,size,attrs.shape[1]),device='cuda');image[mask]=values
    invalid=(~mask).cpu().numpy().astype(np.uint8)
    channels={}
    for name in ['base_color','metallic','roughness','alpha']:
        channel=np.clip(image[...,layout[name]].cpu().numpy()*255,0,255).astype(np.uint8)
        channel=cv2.inpaint(channel,invalid,3 if name=='base_color' else 1,cv2.INPAINT_TELEA)
        if channel.ndim==2:channel=channel[...,None]
        channels[name]=channel
    color=Image.fromarray(np.concatenate([channels['base_color'],channels['alpha']],-1))
    mr=Image.fromarray(np.concatenate([np.zeros_like(channels['metallic']),channels['roughness'],channels['metallic']],-1))
    color.save(folder/'BaseColor.png');mr.save(folder/'MetallicRoughness.png')
    result=mesh.copy();result.visual=trimesh.visual.TextureVisuals(uv=np.asarray(mesh.visual.uv).copy(),
        material=trimesh.visual.material.PBRMaterial(baseColorTexture=color,metallicRoughnessTexture=mr,
            baseColorFactor=[255]*4,metallicFactor=1.,roughnessFactor=1.,alphaMode='OPAQUE',doubleSided=False))
    result.export(folder/'Voidreign.glb',extension_webp=False)
    rows.append(dict(texture_size=size,elapsed_seconds=round(time.perf_counter()-started,3),triangles=len(mesh.faces)))
    del image,values,pos
report=dict(status='complete',gpu=torch.cuda.get_device_name(0),torch=torch.__version__,
    volume_sha256=hashlib.sha256(Path(a.volume).read_bytes()).hexdigest(),
    input_mesh_sha256=hashlib.sha256(Path(a.mesh).read_bytes()).hexdigest(),exports=rows,
    method='Fresh material-volume sampling onto fixed original Pixal mesh and UVs; native export coordinate transform reversed')
(out/'run.json').write_text(json.dumps(report,indent=2));print(json.dumps(report),flush=True)
