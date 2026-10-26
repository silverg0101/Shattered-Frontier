"""Source-indexed joint-band layout from an authored outer plane; no skin edit."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args();profile,out=Path(a.profile),Path(a.output)
cfg=json.loads(profile.read_text(encoding='utf-8'))
assert cfg['schema_version']==1 and cfg['units']=='metres' and not out.exists()
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
for key in ['rest_selection','semantic_selection','domains','contract','joint_inspection']:
    assert cfg[key] in cfg['input_sha256']
contract=json.loads(Path(cfg['contract']).read_text(encoding='utf-8'))
assert sha(contract['selection'])==contract['selection_sha256']
assert cfg['input_sha256'][cfg['rest_selection']]==contract['rest_selection_sha256']
inspection=json.loads(Path(cfg['joint_inspection']).read_text(encoding='utf-8'))
assert inspection['status']=='source-bound-joint-topology-design-evidence-only'
assert inspection['input_sha256'][cfg['rest_selection']]==cfg['input_sha256'][cfg['rest_selection']]
assert inspection['input_sha256'][cfg['semantic_selection']]==cfg['input_sha256'][cfg['semantic_selection']]
with np.load(cfg['rest_selection']) as ar:points,faces=ar['vertices'].astype(float),ar['triangles']
with np.load(cfg['semantic_selection']) as ar:canonical,labels=ar['canonical_node'],ar['labels']
with np.load(cfg['domains']) as ar:owners=ar['owner_per_vertex']
_,first=np.unique(canonical,return_index=True)
assert np.array_equal(owners,owners[first][canonical]) and np.array_equal(labels,labels[first][canonical])
nodes=points[first];cf=canonical[faces]
edges=np.unique(np.sort(np.concatenate([cf[:,[0,1]],cf[:,[1,2]],cf[:,[2,0]]]),axis=1),axis=0)
adj=[[] for _ in nodes]
for u,v in edges:adj[u].append(int(v));adj[v].append(int(u))
assert 1<=len(cfg['feet'])<=16
arrays={};rows=[]
with np.load(contract['selection']) as parts:
    for item in cfg['feet']:
        foot=next(f for f in contract['feet'] if f['id']==item['id']);prefix=item['id']
        core=parts[foot['core_key']];old=parts[foot['collar_key']]
        plane=np.array(item['outer_plane_origin_m'],float);normal=np.array(item['outer_plane_normal'],float)
        assert plane.shape==normal.shape==(3,) and np.isfinite(plane).all() and np.isfinite(normal).all()
        assert abs(np.linalg.norm(normal)-1)<1e-8
        eligible=(owners[first]==item['owner_id'])&np.isin(labels[first],item['allowed_semantic_labels'])&((nodes-plane)@normal<=0)
        roots=np.unique(canonical[core]);assert eligible[roots].all(),'Authored boundary excludes protected core'
        grown=np.zeros(len(nodes),bool);grown[roots]=True;stack=roots.tolist()
        while stack:
            u=stack.pop()
            for v in adj[u]:
                if eligible[v] and not grown[v]:grown[v]=True;stack.append(v)
        state=np.zeros(len(nodes),np.int8);state[grown]=2;state[roots]=1
        band=np.flatnonzero(state[canonical]==2);assert len(band)>0
        el=state[edges];outer_edges=edges[((el[:,0]==0)&(el[:,1]==2))|((el[:,0]==2)&(el[:,1]==0))]
        outer=np.unique(outer_edges[state[outer_edges]==2])
        bypass=np.flatnonzero((state[cf]==1).any(1)&(state[cf]==0).any(1)&~(state[cf]==2).any(1))
        crossing=np.flatnonzero((state[cf]!=0).any(1)&(state[cf]==0).any(1))
        external=edges[(el!=0).any(1)&(el==0).any(1)]
        outside=external[state[external]==0]
        semantic_cut=outside[labels[first][outside]!=0]
        foot_joint=next(f for f in inspection['feet'] if f['id']==prefix)
        for key,value in {'core_vertices':core,'band_vertices':band,'old_collar_vertices':old,'added_band_vertices':np.setdiff1d(band,old),'removed_collar_vertices':np.setdiff1d(old,band),'local_faces':np.flatnonzero((state[cf]!=0).any(1)),'bypass_faces':bypass,'outer_crossing_faces':crossing,'outer_boundary_vertices':np.flatnonzero(np.isin(canonical,outer)),'semantic_cut_canonical_nodes':np.unique(semantic_cut)}.items():arrays[prefix+'.'+key]=value
        rows.append(dict(id=prefix,owner_prior=item['owner_id'],protected_core_vertices=len(core),old_collar_vertices=len(old),proposed_band_vertices=len(band),added_band_vertices=int(len(np.setdiff1d(band,old))),removed_old_collar_vertices=int(len(np.setdiff1d(old,band))),direct_core_exterior_bypass_faces=len(bypass),outer_boundary_canonical_nodes=len(outer),outer_crossing_faces=len(crossing),semantic_cut_canonical_nodes=int(len(np.unique(semantic_cut))),band_bounds_m=[points[band].min(0).tolist(),points[band].max(0).tolist()],joint_head_signed_distance_to_outer_plane_m=float((np.array(foot_joint['rest_joint_head_m'])-plane)@normal),outer_plane_origin_m=plane.tolist(),outer_plane_normal=normal.tolist()))
out.mkdir(parents=True);(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
np.savez_compressed(out/'joint-band-selections.npz',**arrays)
report=dict(schema_version=1,status='authored-plane-joint-band-layout-proposal-needs-visual-review',cpu_only=True,input_sha256=cfg['input_sha256'],profile_sha256=sha(profile),tool_sha256=sha(out/'tool-source.py'),selection_sha256=sha(out/'joint-band-selections.npz'),feet=rows,limitations=['Authored plane, owner and semantic labels are proposal constraints, not inferred physical anatomy.','Protected source core indices retained exactly; no weight, UV, mesh, pose, rig or budget change.','No blend weights are generated. Zero direct bypass faces does not prove a good physical transition or deformation.','Outer crossings and semantic cuts need source-indexed review; do not weld/delete shells or include feathers automatically.'])
(out/'joint-band-report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(rows),flush=True)
