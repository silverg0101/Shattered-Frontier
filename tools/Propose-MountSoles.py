"""Source-bound hoof underside patches; geometric proposals, not contact approval."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda:stream.read(8*1024*1024),b''):h.update(chunk)
    return h.hexdigest()

p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args();profile,out=Path(a.profile).resolve(),Path(a.output).resolve();cfg=json.loads(profile.read_text())
assert cfg['schema_version']==1 and cfg['units']=='metres' and not out.exists()
assert -1<=cfg['maximum_normal_z']<0
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
for key in ['rest_selection','semantic_selection','domains']:assert cfg[key] in cfg['input_sha256']
with np.load(cfg['rest_selection']) as arc:points,faces=arc['vertices'].astype(float),arc['triangles']
with np.load(cfg['semantic_selection']) as arc:canonical=arc['canonical_node']
with np.load(cfg['domains']) as arc:owners=arc['owner_per_vertex']
tri=points[faces];cross=np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0]);length=np.linalg.norm(cross,axis=1)
normal=np.divide(cross,length[:,None],out=np.zeros_like(cross),where=length[:,None]>0)
centroid=tri.mean(1);areas=length/2;rows=[];arrays={}
for foot in cfg['feet']:
    lower,upper=np.array(foot['minimum_m']),np.array(foot['maximum_m']);assert np.all(lower<upper)
    mask=np.all((centroid>=lower)&(centroid<=upper),axis=1)
    mask &= ((owners[faces]==foot['owner_id']).sum(1)>=2)&(normal[:,2]<=cfg['maximum_normal_z'])&(areas>0)
    ids=np.flatnonzero(mask);assert len(ids)>0,foot['id']
    parent={}
    def find(i):
        parent.setdefault(i,i)
        while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
        return i
    for face in canonical[faces[ids]]:
        r=find(int(face[0]))
        for node in face[1:]:parent[find(int(node))]=r
    groups={}
    for index in ids:groups.setdefault(find(int(canonical[faces[index,0]])),[]).append(int(index))
    ranked=sorted(groups.values(),key=lambda g:float(areas[g].sum()),reverse=True)
    selected=np.array(ranked[0],np.int32);vertices=np.unique(faces[selected]).astype(np.int32)
    area=float(areas[selected].sum());center=(centroid[selected]*areas[selected,None]).sum(0)/area
    arrays[foot['id']+'.faces']=selected;arrays[foot['id']+'.vertices']=vertices
    rows.append(dict(id=foot['id'],owner_id=foot['owner_id'],candidate_patches=len(ranked),
        ranked_patch_areas_m2=[float(areas[g].sum()) for g in ranked[:5]],selected_faces=len(selected),vertices=len(vertices),
        area_m2=area,area_center_m=center.tolist(),minimum_height_m=float(points[vertices,2].min()-cfg['ground_height_m']),
        lower_fifth_height_m=float(np.quantile(points[vertices,2],.05)-cfg['ground_height_m']),
        area_normal=(normal[selected]*areas[selected,None]).sum(0).tolist(),
        bounds_m=[points[vertices].min(0).tolist(),points[vertices].max(0).tolist()]))
out.mkdir(parents=True);np.savez_compressed(out/'sole-patches.npz',**arrays)
(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
report=dict(schema_version=1,status='sole-patch-proposals-needs-review',cpu_only=True,input_sha256=cfg['input_sha256'],
    tool_sha256=sha(__file__),profile_sha256=sha(profile),ground_height_m=cfg['ground_height_m'],feet=rows,
    limitations=['Largest connected downward-facing patch in an authored foot domain is a proposal, not an approved sole.',
      'Generated hoof surfaces may be irregular, open or misoriented; inspect exact faces and alternate patches.',
      'No geometry change, bone stretch, ground contact or locomotion acceptance.'])
(out/'sole-report.json').write_text(json.dumps(report,indent=2))
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
print(json.dumps([dict(id=r['id'],faces=r['selected_faces'],minimum_height_m=r['minimum_height_m'],area_m2=r['area_m2']) for r in rows]))
