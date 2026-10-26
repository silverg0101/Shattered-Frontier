"""Bounded harmonic blend across authored primary ownership boundaries.

Only a source-geodesic boundary band changes. Geometry/UVs and rig controls stay.
"""
import argparse
import hashlib
import heapq
import json
from pathlib import Path
import sys
import bpy
import numpy as np

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda:stream.read(8*1024*1024),b''):h.update(chunk)
    return h.hexdigest()

p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);profile,out=Path(a.profile).resolve(),Path(a.output).resolve();cfg=json.loads(profile.read_text())
assert cfg['schema_version']==1 and cfg['units']=='metres' and not out.exists()
assert .01<=cfg['boundary_band_m']<=.5 and 1<=cfg['maximum_iterations']<=1000
assert 0<cfg['weight_convergence_tolerance']<=.001 and 1<=cfg['maximum_influences']<=4
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
with np.load(cfg['semantic_selection']) as a:canonical,labels=a['canonical_node'],a['labels']
with np.load(cfg['rest_selection']) as a:points,faces=a['vertices'].astype(float),a['triangles']
with np.load(cfg['domains']) as a:owners=a['owner_per_vertex']
first=np.unique(canonical,return_index=True)[1];nodes=points[first];node_owners=owners[first];valid=(labels[first]==0)&(node_owners>0)
cf=canonical[faces];edges=np.unique(np.sort(np.concatenate([cf[:,[0,1]],cf[:,[1,2]],cf[:,[2,0]]]),axis=1),axis=0)
length=np.linalg.norm(nodes[edges[:,0]]-nodes[edges[:,1]],axis=1);mask=valid[edges[:,0]]&valid[edges[:,1]]&(length>1e-10)
edges,length=edges[mask],length[mask];adj=[[] for _ in nodes]
for (i,j),d in zip(edges,length):adj[int(i)].append((int(j),float(d)));adj[int(j)].append((int(i),float(d)))
boundary=edges[node_owners[edges[:,0]]!=node_owners[edges[:,1]]];assert len(boundary)>0
dist=np.full(len(nodes),np.inf);queue=[]
for i in np.unique(boundary):dist[i]=0.;heapq.heappush(queue,(0.,int(i)))
while queue:
    d,i=heapq.heappop(queue)
    if d!=dist[i]:continue
    for j,step in adj[i]:
        nd=d+step
        if nd<=cfg['boundary_band_m'] and nd<dist[j]:dist[j]=nd;heapq.heappush(queue,(nd,j))
band=np.flatnonzero(dist<=cfg['boundary_band_m']);assert len(band)>0
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()));obj=bpy.data.objects[cfg['object']]
names=cfg['primary_bones'];lookup={obj.vertex_groups[n].index:k for k,n in enumerate(names)}
old=np.zeros((len(points),len(names)),np.float64)
for v in obj.data.vertices:
    for group in v.groups:
        if group.group in lookup:old[v.index,lookup[group.group]]=group.weight
weights=old[first].copy();assert np.allclose(old,weights[canonical],atol=1e-7), 'Canonical seam weights differ before repair'
local_index=np.full(len(nodes),-1,np.int32);local_index[band]=np.arange(len(band))
dest=[];neighbor=[];conductance=[]
for i in band:
    for j,d in adj[i]:dest.append(int(local_index[i]));neighbor.append(j);conductance.append(1/d)
dest=np.array(dest);neighbor=np.array(neighbor);conductance=np.array(conductance)
denom=np.bincount(dest,weights=conductance,minlength=len(band));assert np.all(denom>0)
history=[]
for iteration in range(cfg['maximum_iterations']):
    averaged=np.zeros((len(band),len(names)))
    for col in range(len(names)):
        averaged[:,col]=np.bincount(dest,weights=conductance*weights[neighbor,col],minlength=len(band))/denom
    change=float(abs(averaged-weights[band]).max());weights[band]=averaged;history.append(change)
    if change<=cfg['weight_convergence_tolerance']:break
indices=np.argsort(-weights[band],axis=1)[:,:cfg['maximum_influences']];pruned=np.zeros_like(weights[band])
np.put_along_axis(pruned,indices,np.take_along_axis(weights[band],indices,axis=1),axis=1);pruned/=pruned.sum(1)[:,None];weights[band]=pruned
affected=np.flatnonzero(np.isfinite(dist[canonical]));groups=[obj.vertex_groups[n] for n in names]
for i in affected:
    for col,group in enumerate(groups):
        value=float(weights[canonical[i],col])
        if value>0:group.add([int(i)],value,'REPLACE')
        elif old[i,col]>0:group.remove([int(i)])
out.mkdir(parents=True);(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
np.savez_compressed(out/'weight-repair.npz',source_vertex_ids=affected,canonical_band=band,geodesic_distance_m=dist[band],before=old[affected],after=weights[canonical[affected]])
candidate=out/'Mount-StandingSkinRepair.blend';bpy.ops.wm.save_as_mainfile(filepath=str(candidate),relative_remap=True)
report=dict(schema_version=1,status='root-weight-repair-needs-checks',cpu_only=True,input_sha256=cfg['input_sha256'],tool_sha256=sha(__file__),profile_sha256=sha(profile),candidate_sha256=sha(candidate),boundary_edges=len(boundary),canonical_band_nodes=len(band),affected_source_vertices=len(affected),iterations=len(history),maximum_weight_update=history[-1],solver_converged=history[-1]<=cfg['weight_convergence_tolerance'],iteration_history=history,
    limitations=['Harmonic blending is a skin proposal, not semantic/anatomical approval.','Only ownership-boundary band changed; sole core, geometry, UVs, controls and external bands retained.','Recheck source strain, contacts, seams and bind/weight budgets after this repair.'])
(out/'weight-repair-report.json').write_text(json.dumps(report,indent=2))
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
print(json.dumps({k:report[k] for k in ['status','affected_source_vertices','solver_converged','maximum_weight_update']}),flush=True)
