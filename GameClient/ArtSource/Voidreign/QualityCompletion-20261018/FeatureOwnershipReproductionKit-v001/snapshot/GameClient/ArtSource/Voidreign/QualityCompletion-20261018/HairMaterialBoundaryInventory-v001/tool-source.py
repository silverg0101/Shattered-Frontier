"""Inventory mixed semantic triangles with stable source-bound patch IDs.

Connected patches share canonical edges, not mere spatial proximity. No material,
mesh or semantic-label change; classification remains an authored review.
"""
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
a=p.parse_args();profile=Path(a.profile).resolve();out=Path(a.output).resolve();cfg=json.loads(profile.read_text())
assert cfg['schema_version']==1 and cfg['units']=='metres' and not out.exists()
assert 1<=len(cfg['selected_labels'])<=8 and len(set(cfg['selected_labels']))==len(cfg['selected_labels'])
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
with np.load(cfg['rest_selection']) as ar:points,faces=ar['vertices'].astype(float),ar['triangles']
with np.load(cfg['semantic_selection']) as ar:canonical,labels=ar['canonical_node'],ar['labels']
assert len(points)==len(labels)==len(canonical)
selected=np.isin(labels,cfg['selected_labels']);whole=selected[faces].all(1);mixed=selected[faces].any(1)&~whole
ids=np.flatnonzero(mixed);assert len(ids)>0
parent=np.arange(len(ids));edge_owner={}
def root(i):
    while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
    return i
for row,face in enumerate(canonical[faces[ids]]):
    for edge in [(face[0],face[1]),(face[1],face[2]),(face[2],face[0])]:
        key=tuple(sorted(int(i) for i in edge));assert key[0]!=key[1]
        if key in edge_owner:parent[root(row)]=root(edge_owner[key])
        else:edge_owner[key]=row
groups={}
for i,f in enumerate(ids):groups.setdefault(root(i),[]).append(int(f))
arrays={'whole_faces':np.flatnonzero(whole),'whole_vertices':np.unique(faces[whole]),'mixed_faces':ids,'mixed_vertices':np.unique(faces[mixed])}
rows=[];summaries=[]
for label in cfg['selected_labels']:
    group_faces=ids[(labels[faces[ids]]==label).any(1)]
    if not len(group_faces):continue
    vertices=np.unique(faces[group_faces]);key='label-'+str(label)
    arrays[key+'.faces']=group_faces;arrays[key+'.vertices']=vertices
    summaries.append(dict(label=label,name=cfg['label_names'][str(label)],mixed_faces=len(group_faces),bounds_m=[points[vertices].min(0).tolist(),points[vertices].max(0).tolist()],face_key=key+'.faces',vertex_key=key+'.vertices'))
for faces_in_group in groups.values():
    face_ids=np.array(sorted(faces_in_group),dtype=np.int32);vertices=np.unique(faces[face_ids]);patch=hashlib.sha256(face_ids.astype('<i4').tobytes()).hexdigest()[:12]
    stable_id='SMB-'+cfg['input_sha256'][cfg['rest_selection']][:10]+'-'+patch
    tri=points[faces[face_ids]];area=float(np.linalg.norm(np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0]),axis=1).sum()/2)
    arrays[stable_id+'.faces']=face_ids;arrays[stable_id+'.vertices']=vertices
    rows.append(dict(id=stable_id,status='unreviewed-keep-existing-material',faces=len(face_ids),vertices=len(vertices),labels=np.unique(labels[vertices]).tolist(),area_m2=area,bounds_m=[points[vertices].min(0).tolist(),points[vertices].max(0).tolist()],face_key=stable_id+'.faces',vertex_key=stable_id+'.vertices'))
rows.sort(key=lambda row:(-row['area_m2'],row['id']))
out.mkdir(parents=True);np.savez_compressed(out/'boundary-selections.npz',**arrays)
(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
report=dict(schema_version=1,status='semantic-material-boundary-inventory-needs-review',units='metres',cpu_only=True,input_sha256=cfg['input_sha256'],profile_sha256=sha(profile),tool_sha256=sha(__file__),whole_selected_faces=int(whole.sum()),mixed_faces=len(ids),patches=len(rows),feature_summaries=summaries,items=rows,limitations=['Canonical-edge connectivity is topology, not material or physical attachment ownership.','All patches retained; area sorting is review priority, not removal/filtering.','One mixed face may contain several features; summed per-feature counts can overlap.','Stable IDs bind exact source indexing; changed topology requires a new inventory/correspondence.'])
(out/'inventory.json').write_text(json.dumps(report,indent=2));print(json.dumps({k:report[k] for k in ['status','whole_selected_faces','mixed_faces','patches','feature_summaries']}),flush=True)
