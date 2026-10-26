"""Inspect retained rigid-part transition topology and rest joint placement.

CPU read-only evidence; neither anatomical approval nor a mesh/skin candidate.
"""
import argparse
from collections import deque
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);profile,out=Path(a.profile),Path(a.output)
cfg=json.loads(profile.read_text(encoding='utf-8'))
assert cfg['schema_version']==1 and cfg['units']=='metres' and not out.exists()
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
for k in ['source','source_retention','rest_selection','semantic_selection','contract']:
    assert cfg[k] in cfg['input_sha256']
r=json.loads(Path(cfg['source_retention']).read_text(encoding='utf-8'))
assert r['status']=='passed-indexed-rest-retention-only' and r['candidate_sha256']==cfg['input_sha256'][cfg['source']]
contract=json.loads(Path(cfg['contract']).read_text(encoding='utf-8'))
assert sha(contract['selection'])==contract['selection_sha256']
assert cfg['input_sha256'][cfg['rest_selection']]==contract['rest_selection_sha256']
with np.load(cfg['rest_selection']) as ar:points,faces=ar['vertices'].astype(float),ar['triangles']
assert hashlib.sha256(faces.astype(r['after']['faces']['dtype']).tobytes()).hexdigest()==r['after']['faces']['sha256']
with np.load(cfg['semantic_selection']) as ar:canonical=ar['canonical_node']
assert canonical.shape==(len(points),) and canonical.min()==0
cf=canonical[faces];edges,incidence=np.unique(np.sort(np.concatenate([cf[:,[0,1]],cf[:,[1,2]],cf[:,[2,0]]]),axis=1),axis=0,return_counts=True)
assert not (edges[:,0]==edges[:,1]).any()
n=int(canonical.max())+1;first=np.full(n,-1,int)
first[canonical]=np.arange(len(points));assert (first>=0).all()
cp=points[first];assert np.linalg.norm(points-cp[canonical],axis=1).max()<1e-6
adj=[[] for _ in range(n)]
for u,v in edges:adj[u].append(int(v));adj[v].append(int(u))
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()))
obj=bpy.data.objects[cfg['object']];rig=bpy.data.objects[cfg['rig_object']]
bpy.context.scene.frame_set(cfg['rest_frame'])
assert np.array_equal(faces,np.array([x.vertices[:] for x in obj.data.polygons],np.int32))
coords=np.array([v.co[:] for v in obj.data.vertices]);world=np.column_stack([coords,np.ones(len(coords))])@np.array(obj.matrix_world).T
translation=np.array(cfg['source_world_to_reference_translation_m'],float)
assert translation.shape==(3,) and np.isfinite(translation).all() and np.abs(translation).max()<=1
world[:,:3]+=translation
error=float(np.linalg.norm(world[:,:3]-points,axis=1).max())
print(json.dumps(dict(rest_world_maximum_error_m=error,raw_maximum_error=float(np.linalg.norm(coords-points,axis=1).max()),world_matrix=np.array(obj.matrix_world).tolist())),flush=True)
assert error<=cfg['rest_tolerance_m']
arrays={};rows=[]
with np.load(contract['selection']) as parts:
    for foot in contract['feet']:
        core,collar=parts[foot['core_key']],parts[foot['collar_key']]
        assert not np.intersect1d(core,collar).size
        labels=np.zeros(len(points),np.int8);labels[core]=1;labels[collar]=2
        lo=np.full(n,127,np.int8);hi=np.zeros(n,np.int8)
        np.minimum.at(lo,canonical,labels);np.maximum.at(hi,canonical,labels)
        cl=hi.copy();cl[lo!=hi]=3
        involved=(cl[cf]!=0).any(1);local=np.flatnonzero(involved)
        bypass=np.flatnonzero((cl[cf]==1).any(1)&(cl[cf]==0).any(1)&~(cl[cf]==2).any(1)&~(cl[cf]==3).any(1))
        el=cl[edges];cc=edges[((el[:,0]==1)&(el[:,1]==2))|((el[:,0]==2)&(el[:,1]==1))]
        ce=edges[((el[:,0]==0)&(el[:,1]==2))|((el[:,0]==2)&(el[:,1]==0))]
        depth=np.full(n,-1,np.int32);roots=np.flatnonzero(cl==1);depth[roots]=0;q=deque(roots.tolist())
        while q:
            u=q.popleft()
            for v in adj[u]:
                if cl[v]==2 and depth[v]<0:depth[v]=depth[u]+1;q.append(v)
        outer=np.unique(ce[cl[ce]==2]);outer_depth=depth[outer]
        bone=rig.data.bones[foot['rigid_bone']]
        head=np.array(rig.matrix_world@bone.head_local)+translation;tail=np.array(rig.matrix_world@bone.tail_local)+translation
        near=np.linalg.norm(points[core]-head,axis=1)
        lengths=np.linalg.norm(cp[edges[:,0]]-cp[edges[:,1]],axis=1)
        local_edges=np.flatnonzero((el!=0).any(1));bad_edges=local_edges[incidence[local_edges]>2]
        prefix=foot['id'];arrays[prefix+'.local_faces']=local;arrays[prefix+'.bypass_faces']=bypass
        arrays[prefix+'.core_collar_canonical_edges']=cc;arrays[prefix+'.collar_exterior_canonical_edges']=ce
        arrays[prefix+'.outer_collar_nodes']=outer;arrays[prefix+'.outer_collar_core_hops']=outer_depth
        arrays[prefix+'.conflicting_canonical_nodes']=np.flatnonzero(cl==3)
        rows.append(dict(id=prefix,core_vertices=len(core),collar_vertices=len(collar),core_bounds_m=[points[core].min(0).tolist(),points[core].max(0).tolist()],collar_bounds_m=[points[collar].min(0).tolist(),points[collar].max(0).tolist()],canonical_membership_conflicts=int((cl==3).sum()),core_to_exterior_faces_without_collar=len(bypass),core_collar_edges=len(cc),collar_exterior_edges=len(ce),outer_collar_reachable_from_core=int((outer_depth>=0).sum()),outer_collar_unreachable_from_core=int((outer_depth<0).sum()),outer_collar_hop_histogram={str(int(d)):int((outer_depth==d).sum()) for d in np.unique(outer_depth)},incident_edge_length_quantiles_m=np.quantile(lengths[local_edges],[0,.5,.9,.99,1]).tolist(),incident_nonmanifold_canonical_edges=len(bad_edges),rigid_bone=bone.name,rigid_bone_parent=bone.parent.name if bone.parent else None,rest_joint_head_m=head.tolist(),rest_bone_tail_m=tail.tolist(),joint_head_nearest_core_vertex_distance_m=float(near.min()),joint_head_height_above_core_top_m=float(head[2]-points[core,2].max())))
out.mkdir(parents=True);(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
np.savez_compressed(out/'joint-topology-selections.npz',**arrays)
report=dict(schema_version=1,status='source-bound-joint-topology-design-evidence-only',cpu_only=True,input_sha256=cfg['input_sha256'],contract_selection_sha256=sha(contract['selection']),profile_sha256=sha(profile),tool_sha256=sha(out/'tool-source.py'),rest_world_maximum_error_m=error,selections_sha256=sha(out/'joint-topology-selections.npz'),feet=rows,limitations=['Rest topology and authored core/collar membership only; no anatomical inference, skin/rig/mesh edit or candidate-budget reset.','Hop counts measure canonical mesh graph resolution, not physical joint loops or proof retopology is sufficient.','Existing joint coordinates are authored rig landmarks, not independently established anatomy.','Multiple overlapping shells may belong to one physical hoof; neither component count nor nonmanifold edges authorize deletion.'])
(out/'joint-topology.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(rows),flush=True)
