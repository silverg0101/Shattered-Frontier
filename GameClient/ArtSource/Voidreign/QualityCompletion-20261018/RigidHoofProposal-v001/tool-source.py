"""Propose sole-seeded connected hoof cores and geodesic collars; no skin changes."""
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
assert cfg['schema_version']==1 and cfg['units']=='metres' and not out.exists() and 1<=len(cfg['feet'])<=16
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
with np.load(cfg['rest_selection']) as ar:points,faces=ar['vertices'].astype(float),ar['triangles']
with np.load(cfg['semantic_selection']) as ar:canonical,labels=ar['canonical_node'],ar['labels']
with np.load(cfg['domains']) as ar:owners=ar['owner_per_vertex']
first=np.unique(canonical,return_index=True)[1];nodes=points[first]
assert np.allclose(points,nodes[canonical],atol=1e-8)
assert np.array_equal(owners,owners[first][canonical]) and np.array_equal(labels,labels[first][canonical])
cf=canonical[faces];edges=np.unique(np.sort(np.concatenate([cf[:,[0,1]],cf[:,[1,2]],cf[:,[2,0]]]),axis=1),axis=0)
length=np.linalg.norm(nodes[edges[:,0]]-nodes[edges[:,1]],axis=1)
patches=np.load(cfg['sole_patches']);sole_report=json.loads(Path(cfg['sole_report']).read_text());sole_rows={r['id']:r for r in sole_report['feet']}
arrays={};rows=[]
for foot in cfg['feet']:
    assert .001<=foot['blend_band_m']<=.2 and 0<=foot['hoof_margin_m']<=.1
    allowed=(owners[first]==foot['owner_id'])&np.isin(labels[first],cfg['allowed_semantic_labels'])
    usable=allowed[edges].all(1)&(length>1e-10)
    adj={int(i):[] for i in np.flatnonzero(allowed)}
    for (i,j),d in zip(edges[usable],length[usable]):adj[int(i)].append((int(j),float(d)));adj[int(j)].append((int(i),float(d)))
    below=allowed&(nodes[:,2]<=foot['rigid_hoof_maximum_z_m']);seed=np.unique(canonical[patches[foot['id']+'.vertices']]);assert below[seed].all()
    core=np.zeros(len(nodes),bool);core[seed]=True;stack=seed.tolist()
    while stack:
        i=stack.pop()
        for j,d in adj[i]:
            if below[j] and not core[j]:core[j]=True;stack.append(j)
    distance=np.full(len(nodes),np.inf);distance[core]=0;queue=[(0.,int(i)) for i in np.flatnonzero(core)];heapq.heapify(queue)
    while queue:
        d,i=heapq.heappop(queue)
        if d!=distance[i]:continue
        for j,step in adj[i]:
            nd=d+step
            if nd<=foot['blend_band_m'] and nd<distance[j]:distance[j]=nd;heapq.heappush(queue,(nd,j))
    core_vertices=np.flatnonzero(core[canonical]);collar_vertices=np.flatnonzero(np.isfinite(distance[canonical])&~core[canonical])
    chosen_faces=np.flatnonzero(core[canonical[faces]].all(1))
    low,high=np.array(sole_rows[foot['id']]['bounds_m']);xy=((points[:,:2]>=low[:2]-foot['hoof_margin_m'])&(points[:,:2]<=high[:2]+foot['hoof_margin_m'])).all(1)
    legacy=(owners==foot['owner_id'])&xy&(points[:,2]<=foot['rigid_hoof_maximum_z_m']);legacy[patches[foot['id']+'.vertices']]=True
    lost=np.flatnonzero(legacy&~core[canonical]);gained=np.flatnonzero(core[canonical]&~legacy)
    for key,value in {'core_vertices':core_vertices,'core_faces':chosen_faces,'collar_vertices':collar_vertices,'collar_blend':1-distance[canonical[collar_vertices]]/foot['blend_band_m'],'legacy_rigid_not_in_core':lost,'core_outside_legacy_rigid':gained}.items():arrays[foot['id']+'.'+key]=value
    rows.append(dict(id=foot['id'],seed_vertices=len(patches[foot['id']+'.vertices']),core_vertices=len(core_vertices),core_faces=len(chosen_faces),collar_vertices=len(collar_vertices),legacy_rigid_vertices=int(legacy.sum()),legacy_rigid_not_in_core=len(lost),core_outside_legacy_rigid=len(gained),core_bounds_m=[points[core_vertices].min(0).tolist(),points[core_vertices].max(0).tolist()]))
out.mkdir(parents=True);np.savez_compressed(out/'hoof-selections.npz',**arrays)
(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
(out/'proposal-report.json').write_text(json.dumps(dict(schema_version=1,status='connected-hoof-selection-proposal-needs-review',cpu_only=True,input_sha256=cfg['input_sha256'],profile_sha256=sha(profile),tool_sha256=sha(__file__),feet=rows,limitations=['Owner labels, height planes and source topology are authored priors, not semantic proof.','Inspect missing legacy rigid vertices and newly included geometry before accepting this selection.','No weights, geometry, animation or acceptance limits changed; not a new primary rig candidate.']),indent=2))
print(json.dumps({'status':'connected-hoof-selection-proposal-needs-review','feet':rows}),flush=True)
