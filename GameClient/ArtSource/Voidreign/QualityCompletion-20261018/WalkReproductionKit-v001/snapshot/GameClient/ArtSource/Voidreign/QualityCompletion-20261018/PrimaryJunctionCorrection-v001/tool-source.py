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
policy_owners=node_owners.copy()
if cfg.get('include_semantic_boundaries'):
    hair=labels[first]!=0;valid|=hair
    policy_owners[hair]=int(node_owners.max())+labels[first][hair]
fixed=np.zeros(len(nodes),bool)
for selection in cfg.get('fixed_vertex_selections',[]):
    assert selection['path'] in cfg['input_sha256']
    with np.load(selection['path']) as archive:
        for key in selection['keys']:fixed[canonical[archive[key]]]=True
cf=canonical[faces];edges=np.unique(np.sort(np.concatenate([cf[:,[0,1]],cf[:,[1,2]],cf[:,[2,0]]]),axis=1),axis=0)
length=np.linalg.norm(nodes[edges[:,0]]-nodes[edges[:,1]],axis=1);mask=valid[edges[:,0]]&valid[edges[:,1]]&(length>1e-10)
edges,length=edges[mask],length[mask];adj=[[] for _ in nodes]
for (i,j),d in zip(edges,length):adj[int(i)].append((int(j),float(d)));adj[int(j)].append((int(i),float(d)))
boundary=edges[policy_owners[edges[:,0]]!=policy_owners[edges[:,1]]];assert len(boundary)>0
dist=np.full(len(nodes),np.inf);queue=[]
for i in np.unique(boundary):dist[i]=0.;heapq.heappush(queue,(0.,int(i)))
while queue:
    d,i=heapq.heappop(queue)
    if d!=dist[i]:continue
    for j,step in adj[i]:
        nd=d+step
        if nd<=cfg['boundary_band_m'] and nd<dist[j]:dist[j]=nd;heapq.heappush(queue,(nd,j))
band=np.flatnonzero((dist<=cfg['boundary_band_m'])&~fixed);assert len(band)>0
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()));obj=bpy.data.objects[cfg['object']]
names=cfg['primary_bones'];lookup={obj.vertex_groups[n].index:k for k,n in enumerate(names)}
old=np.zeros((len(points),len(names)),np.float64)
for v in obj.data.vertices:
    for group in v.groups:
        if group.group in lookup:old[v.index,lookup[group.group]]=group.weight
weights=old[first].copy();assert np.allclose(old,weights[canonical],atol=1e-7), 'Canonical seam weights differ before repair'
solver=cfg.get('solver','jacobi');assert solver in ['jacobi','pcg']
body_prior_nodes=np.array([],np.int32)
if cfg.get('body_only_priors'):
    assert cfg['rig_profile'] in cfg['input_sha256']
    rig_cfg=json.loads(Path(cfg['rig_profile']).read_text())
    body_prior_nodes=np.flatnonzero((node_owners==cfg['body_owner_id'])&valid)
    body_names=rig_cfg['body_bones'];cols=[names.index(n) for n in body_names]
    bone_rows={r['id']:r for r in rig_cfg['bones']};ds=[]
    for name in body_names:
        start,end=np.array(bone_rows[name]['head_m']),np.array(bone_rows[name]['tail_m']);delta=end-start
        t=np.clip((nodes[body_prior_nodes]-start)@delta/(delta@delta),0,1)
        ds.append(np.linalg.norm(nodes[body_prior_nodes]-start-t[:,None]*delta,axis=1))
    ds=np.array(ds).T;near=np.argsort(ds,axis=1)[:,:cfg['maximum_influences']]
    values=np.maximum(np.take_along_axis(ds,near,axis=1),rig_cfg['weights']['distance_floor_m'])**(-rig_cfg['weights']['distance_exponent'])
    values/=values.sum(1)[:,None];weights[body_prior_nodes]=0
    weights[body_prior_nodes[:,None],np.array(cols)[near]]=values
local_index=np.full(len(nodes),-1,np.int32);local_index[band]=np.arange(len(band))
dest=[];neighbor=[];conductance=[]
for i in band:
    for j,d in adj[i]:dest.append(int(local_index[i]));neighbor.append(j);conductance.append(1/d)
dest=np.array(dest);neighbor=np.array(neighbor);conductance=np.array(conductance)
denom=np.bincount(dest,weights=conductance,minlength=len(band));assert np.all(denom>0)
history=[]
if solver=='jacobi':
    for iteration in range(cfg['maximum_iterations']):
        averaged=np.zeros((len(band),len(names)))
        for col in range(len(names)):
            averaged[:,col]=np.bincount(dest,weights=conductance*weights[neighbor,col],minlength=len(band))/denom
        change=float(abs(averaged-weights[band]).max());weights[band]=averaged;history.append(change)
        if change<=cfg['weight_convergence_tolerance']:break
else:
    inside=local_index[neighbor]>=0
    ii=local_index[neighbor[inside]];dd=dest[inside];cc=conductance[inside]
    def apply(value):
        answer=denom[:,None]*value
        for col in range(len(names)):
            answer[:,col]-=np.bincount(dd,weights=cc*value[ii,col],minlength=len(band))
        return answer
    rhs=np.zeros((len(band),len(names)))
    for col in range(len(names)):
        rhs[:,col]=np.bincount(dest[~inside],weights=conductance[~inside]*weights[neighbor[~inside],col],minlength=len(band))
    solution=weights[band].copy();residual=rhs-apply(solution)
    direction=residual/denom[:,None];rho=(residual*direction).sum(0)
    for iteration in range(cfg['maximum_iterations']):
        applied=apply(direction);product=(direction*applied).sum(0)
        alpha=np.divide(rho,product,out=np.zeros_like(rho),where=abs(product)>1e-30)
        solution+=direction*alpha;residual-=applied*alpha
        metric=float(abs(residual/denom[:,None]).max());history.append(metric)
        assert np.isfinite(solution).all()
        if metric<=cfg['weight_convergence_tolerance']:break
        preconditioned=residual/denom[:,None];next_rho=(residual*preconditioned).sum(0)
        beta=np.divide(next_rho,rho,out=np.zeros_like(rho),where=abs(rho)>1e-30)
        direction=preconditioned+direction*beta;rho=next_rho
    weights[band]=np.maximum(solution,0)
    weights[band]/=weights[band].sum(1)[:,None]
indices=np.argsort(-weights[band],axis=1)[:,:cfg['maximum_influences']];pruned=np.zeros_like(weights[band])
np.put_along_axis(pruned,indices,np.take_along_axis(weights[band],indices,axis=1),axis=1);pruned/=pruned.sum(1)[:,None];weights[band]=pruned
assert np.allclose(weights[fixed],old[first[fixed]],atol=1e-12), 'Fixed sole weights changed'
affected=np.flatnonzero((np.isfinite(dist[canonical])&~fixed[canonical])|(abs(weights[canonical]-old).max(1)>1e-7));groups=[obj.vertex_groups[n] for n in names]
for i in affected:
    for col,group in enumerate(groups):
        value=float(weights[canonical[i],col])
        if value>0:group.add([int(i)],value,'REPLACE')
        elif old[i,col]>0:group.remove([int(i)])
out.mkdir(parents=True);(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
np.savez_compressed(out/'weight-repair.npz',source_vertex_ids=affected,canonical_band=band,geodesic_distance_m=dist[band],before=old[affected],after=weights[canonical[affected]])
candidate=out/'Mount-StandingSkinRepair.blend';bpy.ops.wm.save_as_mainfile(filepath=str(candidate),relative_remap=True)
report=dict(schema_version=1,status='root-weight-repair-needs-checks',cpu_only=True,input_sha256=cfg['input_sha256'],tool_sha256=sha(__file__),profile_sha256=sha(profile),candidate_sha256=sha(candidate),boundary_edges=len(boundary),canonical_band_nodes=len(band),body_only_prior_nodes=len(body_prior_nodes),affected_source_vertices=len(affected),iterations=len(history),solver=solver,convergence_measure='normalized unpruned Laplacian residual' if solver=='pcg' else 'maximum Jacobi weight update',final_solver_metric=history[-1],solver_converged=history[-1]<=cfg['weight_convergence_tolerance'],iteration_history=history,
    limitations=['Harmonic blending is a skin proposal, not semantic/anatomical approval.','Ownership band and optional explicit body-only priors change; geometry, UVs and controls retained.','Solver convergence is measured before influence pruning. Recheck source strain, contacts, seams and weight budgets.'])
report.update(included_semantic_boundaries=bool(cfg.get('include_semantic_boundaries')),fixed_canonical_nodes=int(fixed.sum()))
(out/'weight-repair-report.json').write_text(json.dumps(report,indent=2))
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
print(json.dumps({k:report[k] for k in ['status','affected_source_vertices','solver_converged','final_solver_metric']}),flush=True)
