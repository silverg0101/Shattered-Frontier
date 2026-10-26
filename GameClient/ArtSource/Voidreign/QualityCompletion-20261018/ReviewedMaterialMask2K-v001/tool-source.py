"""Rasterize reviewed face classes with explicit unknown and UV-overlap pixels.

Pixel-center, half-open triangle coverage; no antialiasing, dilation, guessing
or material edits. Output class IDs must never be used directly as PBR values.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from PIL import Image


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args();profile,out=Path(a.profile).resolve(),Path(a.output).resolve()
cfg=json.loads(profile.read_text(encoding='utf-8'))
assert cfg['schema_version']==1 and not out.exists()
size=cfg['resolution'];assert size in [1024,2048,4096]
for path,h in cfg['input_sha256'].items():assert sha(path)==h,path
uv_report=json.loads(Path(cfg['uv_report']).read_text(encoding='utf-8'))
approval=json.loads(Path(cfg['approval_report']).read_text(encoding='utf-8'))
assert uv_report['status']=='passed-source-bound-UV-export-only'
assert uv_report['topology_sha256']==cfg['input_sha256'][cfg['uv_topology']]
assert uv_report['source_sha256']==approval['input_sha256'][uv_report['source']]
with np.load(cfg['uv_topology']) as ar:uv=ar['loop_uv'].astype(np.float64)
assert uv.shape==(uv_report['triangles'],3,2)
assert (uv>=0).all() and (uv<=1).all(),'Single bounded atlas required'
labels=np.zeros(len(uv),np.uint8)
with np.load(cfg['approved_faces']) as ar:
    approved=ar['approved_faces']
    for key,code in [('strand_dielectric_faces',1),('bone_dielectric_faces',2)]:
        ids=ar[key];assert (labels[ids]==0).all();labels[ids]=code
assert int((labels!=0).sum())==approval['approved_faces']==len(approved)
owner=np.full((size,size),-1,np.int32)
conflict=np.zeros((size,size),bool)
sample_counts=np.zeros(len(uv),np.int32)
degenerate=[]
points=uv*np.array([size,-size])+np.array([0,size])
for fi,tri in enumerate(points):
    cross=float(np.cross(tri[1]-tri[0],tri[2]-tri[0]))
    if cross==0:degenerate.append(fi);continue
    if cross<0:tri=tri[[0,2,1]]
    lo=np.maximum(np.ceil(tri.min(0)-.5).astype(int),0)
    hi=np.minimum(np.floor(tri.max(0)-.5).astype(int),size-1)
    if (hi<lo).any():continue
    x=np.arange(lo[0],hi[0]+1,dtype=np.float64)[None,:]+.5
    for first_y in range(lo[1],hi[1]+1,64):
        stop_y=min(first_y+64,hi[1]+1)
        y=np.arange(first_y,stop_y,dtype=np.float64)[:,None]+.5
        inside=np.ones((stop_y-first_y,len(x[0])),bool)
        for j in range(3):
            start,end=tri[j],tri[(j+1)%3];delta=end-start
            edge=delta[0]*(y-start[1])-delta[1]*(x-start[0])
            inclusive=delta[1]<0 or (delta[1]==0 and delta[0]>0)
            inside &= (edge>0) | ((edge==0)&inclusive)
        hits=int(inside.sum());sample_counts[fi]+=hits
        if not hits:continue
        current=owner[first_y:stop_y,lo[0]:hi[0]+1]
        conflict[first_y:stop_y,lo[0]:hi[0]+1] |= inside & (current>=0)
        current[inside & (current<0)]=fi
    if fi and fi%50000==0:print('Rasterized',fi,'of',len(uv),flush=True)
mask=np.full((size,size),255,np.uint8);used=owner>=0
mask[used]=labels[owner[used]];mask[conflict]=254
visible_counts=np.bincount(owner[used & ~conflict],minlength=len(uv))
out.mkdir(parents=True)
(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
Image.fromarray(mask).save(out/'MaterialClassIDs.png')
palette=np.zeros((256,3),np.uint8);palette[0]=[90,90,90];palette[1]=[40,190,235];palette[2]=[235,220,155];palette[254]=[255,20,150];palette[255]=[15,15,20]
Image.fromarray(palette[mask]).save(out/'ClassCoveragePreview.png')
np.savez_compressed(out/'coverage.npz',source_face_owner=owner,overlap_pixels=conflict,face_sample_counts=sample_counts,nonconflicting_face_pixel_counts=visible_counts,approved_faces=approved,approved_faces_without_safe_pixel=approved[visible_counts[approved]==0],uv_degenerate_faces=np.array(degenerate,np.int32))
report=dict(schema_version=1,status='UV-class-mask-authoring-data-needs-review',cpu_only=True,
    input_sha256=cfg['input_sha256'],profile_sha256=sha(profile),tool_sha256=sha(out/'tool-source.py'),resolution=size,
    triangles=len(uv),approved_faces=len(approved),uv_degenerate_faces=len(degenerate),
    approved_faces_without_safe_pixel=int((visible_counts[approved]==0).sum()),
    approved_faces_without_center_sample=int((sample_counts[approved]==0).sum()),
    overlap_pixels=int(conflict.sum()),class_pixel_counts={str(i):int((mask==i).sum()) for i in [0,1,2,254,255]},
    class_legend={'0':'unknown mesh surface','1':'reviewed strand nonmetal','2':'reviewed bone nonmetal','254':'overlapping UV faces - unresolved','255':'unused atlas'},
    outputs={name:sha(out/name) for name in ['MaterialClassIDs.png','ClassCoveragePreview.png','coverage.npz']},
    limitations=['Pixel-center source UV coverage only; no filtered/mipped texel footprint or engine acceptance.',
        'Unknown and overlapping pixels are not approvals. No automatic class filling, dilation, PBR map or shader edit.',
        'Use nearest sampling for class IDs; this data PNG is not an albedo/metallic texture.',
        'Per-pixel lack of detected overlap does not prove continuous UV topology is non-overlapping.'])
(out/'mask-report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps({k:report[k] for k in ['status','approved_faces_without_safe_pixel','overlap_pixels','class_pixel_counts']}),flush=True)
