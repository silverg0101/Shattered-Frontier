"""Measure retained projection edit weights at source-bound UV pixel centers."""
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
a=p.parse_args();profile,out=Path(a.profile),Path(a.output)
cfg=json.loads(profile.read_text(encoding='utf-8'))
assert cfg['schema_version']==1 and not out.exists()
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
r=json.loads(Path(cfg['uv_report']).read_text(encoding='utf-8'))
assert r['topology_sha256']==cfg['input_sha256'][cfg['uv_topology']]
assert r['status']=='passed-source-bound-UV-export-only'
with np.load(cfg['uv_topology']) as ar:
    faces=ar['triangles'];uv=ar['loop_uv'].astype(np.float64)
    attrs={x['name']:ar[x['array_key']] for x in r['point_color_attributes']}
    for x in r['point_color_attributes']:
        v=attrs[x['name']]
        assert hashlib.sha256(v.tobytes()).hexdigest()==x['sha256']
        assert np.array_equal(v[:,0],v[:,1]) and np.array_equal(v[:,0],v[:,2])
        assert ((v[:,0]>=0)&(v[:,0]<=1)).all()
with np.load(cfg['baseline_uv_topology']) as ar:
    assert np.array_equal(faces,ar['triangles']) and np.array_equal(uv,ar['loop_uv'])
assert 1<=len(cfg['regions'])<=8 and 1<=len(cfg['mask_reports'])<=4
weights=np.stack([np.maximum.reduce([attrs[name][:,0] for name in region['attributes']]) for region in cfg['regions']],axis=1)
out.mkdir(parents=True)
(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
rows=[]
for path in cfg['mask_reports']:
    path=Path(path);mask=json.loads(path.read_text(encoding='utf-8'));size=mask['resolution']
    assert size in [1024,2048,4096]
    assert mask['input_sha256'][cfg['baseline_uv_topology']]==cfg['input_sha256'][cfg['baseline_uv_topology']]
    assert sha(path.parent/'coverage.npz')==mask['outputs']['coverage.npz']
    with np.load(path.parent/'coverage.npz') as ar:
        owner=ar['source_face_owner'];conflict=ar['overlap_pixels']
    assert owner.shape==conflict.shape==(size,size)
    ids=np.flatnonzero((owner>=0)&~conflict)
    totals=np.zeros((len(cfg['regions']),4),np.float64)
    preview=np.zeros((size*size,3),np.uint8);preview[:]=[12,12,16]
    preview[owner.ravel()>=0]=[45,45,45];preview[conflict.ravel()]=[255,0,180]
    for start in range(0,len(ids),131072):
        pixels=ids[start:start+131072];fi=owner.ravel()[pixels]
        tri=uv[fi];q=np.column_stack([(pixels%size+.5)/size,1-(pixels//size+.5)/size])
        d1,d2=tri[:,1]-tri[:,0],tri[:,2]-tri[:,0];d=q-tri[:,0]
        det=d1[:,0]*d2[:,1]-d1[:,1]*d2[:,0];assert (det!=0).all()
        b=(d[:,0]*d2[:,1]-d[:,1]*d2[:,0])/det
        c=(d1[:,0]*d[:,1]-d1[:,1]*d[:,0])/det
        bary=np.column_stack([1-b-c,b,c]);assert bary.min()>-1e-7 and bary.max()<1+1e-7
        v=np.einsum('ij,ijk->ik',bary,weights[faces[fi]])
        # Max of vertex weights is a diagnostic union, not exact composited bake alpha.
        for index in range(v.shape[1]):
            x=v[:,index];totals[index]+=[(x>0).sum(),x.sum(),(x>=.5).sum(),(x>=.99).sum()]
        rgb=np.zeros((len(pixels),3));rgb[:]=45
        for index,region in enumerate(cfg['regions']):
            rgb=rgb*(1-v[:,index,None])+np.array(region['preview_rgb'])*v[:,index,None]
        preview[pixels]=np.clip(rgb,0,255).astype(np.uint8)
    name=f'ProjectionCoverage-{size}.png';Image.fromarray(preview.reshape(size,size,3)).save(out/name)
    rows.append(dict(resolution=size,safe_mesh_pixel_centers=len(ids),excluded_overlap_pixels=int(conflict.sum()),preview=name,preview_sha256=sha(out/name),regions=[dict(name=region['name'],support_pixel_centers=int(t[0]),weight_equivalent_pixels=float(t[1]),weight_equivalent_square_side_pixels=float(np.sqrt(t[1])),pixels_at_least_half_weight=int(t[2]),pixels_at_least_099_weight=int(t[3])) for region,t in zip(cfg['regions'],totals)]))
report=dict(schema_version=1,status='projection-UV-allocation-measurement-only',cpu_only=True,input_sha256=cfg['input_sha256'],tool_sha256=sha(out/'tool-source.py'),profile_sha256=sha(profile),exact_baseline_topology_bridge=True,measurements=rows,limitations=['Existing edit projections are not semantic head/saddle material masks or full anatomical coverage.','Region diagnostic combines vertex weights by maximum before interpolation; not the actual ordered projection composite or final bake alpha.','Pixel centers exclude overlaps, filtering, mipmapping and bake dilation.','Square-side statistic is only sqrt(weight sum), not contiguous resolution, texel density per metre or an acceptance threshold.','No geometry/UV/material edits, repair-budget reset or model/engine acceptance.'])
(out/'measurement.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(rows),flush=True)
