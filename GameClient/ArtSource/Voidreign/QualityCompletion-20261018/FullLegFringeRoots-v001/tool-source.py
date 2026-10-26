"""Derive local geometric support roots for a reviewed bounded feature proposal."""
import argparse
import hashlib
import heapq
import json
from pathlib import Path
import numpy as np

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda:stream.read(8*1024*1024),b''):h.update(block)
    return h.hexdigest()

p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args();profile=Path(a.profile).resolve();out=Path(a.output).resolve();cfg=json.loads(profile.read_text())
assert cfg['schema_version']==1 and cfg['units']=='metres' and not out.exists()
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
with np.load(cfg['rest_selection']) as ar:points,faces=ar['vertices'].astype(float),ar['triangles']
with np.load(cfg['semantic_selection']) as ar:canonical,labels=ar['canonical_node'],ar['labels']
with np.load(cfg['domains']) as ar:owners=ar['owner_per_vertex']
with np.load(cfg['region_selection']) as ar:vertices=ar[cfg['region_key']]
nodes=np.unique(canonical[vertices]);first=np.unique(canonical,return_index=True)[1];node_points=points[first]
assert np.array_equal(np.sort(vertices),np.flatnonzero(np.isin(canonical,nodes))),'Partial canonical nodes'
assert set(np.unique(labels[vertices]).tolist())<=set(cfg['allowed_region_labels'])
mask=np.isin(np.arange(len(first)),nodes);cf=canonical[faces]
edges=np.unique(np.sort(np.concatenate([cf[:,[0,1]],cf[:,[1,2]],cf[:,[2,0]]]),axis=1),axis=0);edges=edges[edges[:,0]!=edges[:,1]]
cut=edges[mask[edges].sum(1)==1];assert len(cut)>0,'No actual geometric support crosses region boundary'
roots=np.unique(cut[mask[cut]]);supports=np.unique(cut[~mask[cut]])
owner_values,owner_counts=np.unique(owners[first[supports]],return_counts=True)
assert set(owner_values.tolist())<=set(cfg['allowed_support_owners']),'Unexpected support ownership'
assert set(np.unique(labels[first[supports]]).tolist())<=set(cfg['allowed_support_labels']),'Unexpected semantic support'
local=np.full(len(first),-1,int);local[nodes]=np.arange(len(nodes));internal=edges[mask[edges].all(1)]
adj=[[] for _ in nodes]
for i,j in internal:
    d=float(np.linalg.norm(node_points[i]-node_points[j]));assert d>0
    adj[local[i]].append((local[j],d));adj[local[j]].append((local[i],d))
distance=np.full(len(nodes),np.inf);distance[local[roots]]=0;queue=[(0.,int(local[i])) for i in roots];heapq.heapify(queue)
while queue:
    d,i=heapq.heappop(queue)
    if d!=distance[i]:continue
    for j,step in adj[i]:
        new=d+step
        if new<distance[j]:distance[j]=new;heapq.heappush(queue,(new,j))
rootless=nodes[~np.isfinite(distance)];assert not len(rootless),'Rootless geometry requires review; no virtual attachment generated'
full_faces=np.flatnonzero(mask[cf].all(1));boundary_faces=np.flatnonzero(mask[cf].any(1)&~mask[cf].all(1))
out.mkdir(parents=True);np.savez_compressed(out/'region-roots.npz',source_vertices=vertices,canonical_nodes=nodes,canonical_first_vertices=first[nodes],root_nodes=roots,root_vertices=np.flatnonzero(np.isin(canonical,roots)),support_vertices=np.flatnonzero(np.isin(canonical,supports)),source_faces=full_faces,boundary_faces=boundary_faces,root_distance_m=distance,geometric_cut_edges=cut)
(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
report=dict(schema_version=1,status='geometric-region-roots-proposal-needs-review',cpu_only=True,input_sha256=cfg['input_sha256'],tool_sha256=sha(__file__),profile_sha256=sha(profile),feature=cfg['feature'],vertices=len(vertices),canonical_nodes=len(nodes),full_faces=len(full_faces),boundary_faces=len(boundary_faces),root_nodes=len(roots),support_nodes=len(supports),rootless_nodes=len(rootless),maximum_geodesic_distance_m=float(distance.max()),support_owner_node_counts={str(int(k)):int(v) for k,v in zip(owner_values,owner_counts)},root_bounds_m=[node_points[roots].min(0).tolist(),node_points[roots].max(0).tolist()],limitations=['Geometric support is exact but spatial cut and anatomical anchor still require review.','No semantic relabel, bone binding, source edit or simulation.','Boundary-crossing faces remain in source; local full-face isolation is not a separated production mesh.','Root distance applies only to this local feature proposal, not a replacement global weight field.'])
(out/'roots-report.json').write_text(json.dumps(report,indent=2));print(json.dumps({k:report[k] for k in ['status','vertices','canonical_nodes','root_nodes','support_nodes','full_faces','boundary_faces','maximum_geodesic_distance_m','root_bounds_m']}),flush=True)
