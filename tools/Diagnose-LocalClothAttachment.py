"""Check exact local topology and necessary pin/contact distance feasibility."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np
from mathutils.bvhtree import BVHTree

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);profile=Path(a.profile).resolve();out=Path(a.output).resolve();cfg=json.loads(profile.read_text());assert cfg['schema_version']==1 and not out.exists() and .0001<=cfg['required_clearance_m']<=.01
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
producer=json.loads(Path(cfg['producer_report']).read_text());assert producer['source_sha256']==cfg['input_sha256'][cfg['source']]
for row in producer['cache_files']:assert sha(Path(cfg['source']).parent/row['path'])==row['sha256']
with np.load(cfg['capture_points']) as ar:baseline=ar['feature_world_points'][0]
with np.load(cfg['semantic_selection']) as ar:canonical=ar['canonical_node']
with np.load(cfg['region_roots']) as ar:vertices,nodes,distances=ar['source_vertices'],ar['canonical_nodes'],ar['root_distance_m']
_,first=np.unique(canonical[vertices],return_index=True);rest=baseline[first].astype(float)
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()));scene=bpy.context.scene;proxy=bpy.data.objects['SIM | local actual fringe'];body=bpy.data.objects['SOURCE LEG | local context'];assert proxy.modifiers[-1].point_cache.is_baked
faces=np.array([p.vertices[:] for p in proxy.data.polygons]);directed=np.concatenate([faces[:,[0,1]],faces[:,[1,2]],faces[:,[2,0]]]);edges,counts=np.unique(np.sort(directed,axis=1),axis=0,return_counts=True)
pin_group=proxy.vertex_groups['Exact geometric root candidates'];pins=np.array([any(g.group==pin_group.index and g.weight>.999 for g in v.groups) for v in proxy.data.vertices]);free=~pins
def evaluated(ob):return np.array([v.co[:] for v in ob.evaluated_get(bpy.context.evaluated_depsgraph_get()).data.vertices])
scene.frame_set(cfg['held_frame']);current=evaluated(proxy);body_points=evaluated(body);bvh=BVHTree.FromPolygons(body_points.tolist(),[p.vertices[:] for p in body.data.polygons],all_triangles=True)
gap=np.array([bvh.find_nearest(tuple(p))[3] for p in rest]);length=np.linalg.norm(rest[edges[:,0]]-rest[edges[:,1]],axis=1)
rows=[]
for edge,d in zip(edges,length):
    i,j=edge
    if pins[i]==pins[j]:continue
    root,loose=(i,j) if pins[i] else (j,i);upper=float(gap[root]+d)
    if upper<cfg['required_clearance_m']:
        rows.append(dict(root_canonical_node=int(nodes[root]),free_canonical_node=int(nodes[loose]),rest_edge_m=float(d),root_body_distance_m=float(gap[root]),upper_bound_free_body_distance_m=upper,necessary_edge_extension_m=float(cfg['required_clearance_m']-upper),free_root_geodesic_m=float(distances[loose])))
scene.frame_set(max(1,cfg['held_frame']-1));previous=evaluated(proxy);speed=np.linalg.norm(current-previous,axis=1)*cfg['fps'];order=np.argsort(speed[free])[-10:][::-1];free_ids=np.flatnonzero(free)
degree=np.bincount(edges.ravel(),minlength=len(rest));boundary=edges[counts==1]
report=dict(schema_version=1,status='local-attachment-feasibility-diagnostic-only',cpu_only=True,input_sha256=cfg['input_sha256'],profile_sha256=sha(profile),tool_sha256=sha(__file__),topology=dict(nodes=len(rest),faces=len(faces),edges=len(edges),boundary_edges=len(boundary),nonmanifold_edges=int((counts>2).sum()),isolated_nodes=int((degree==0).sum()),euler_characteristic=int(len(rest)-len(edges)+len(faces)),closed_surface=bool((counts==2).all())),baseline_free_vertices_below_required_gap=int(((gap<cfg['required_clearance_m'])&free).sum()),root_contact_incompatible_edges=rows,required_clearance_m=cfg['required_clearance_m'],largest_held_speeds=[dict(canonical_node=int(nodes[free_ids[i]]),speed_m_s=float(speed[free_ids[i]]),root_distance_m=float(distances[free_ids[i]]),baseline_body_gap_m=float(gap[free_ids[i]])) for i in order],limitations=['Distance-to-triangle-set is1-Lipschitz: root distance plus rest-edge length is a necessary upper bound for free clearance without stretching. This proves a local incompatibility, not total solver causation.','Baseline unsigned distances include cropped/open local source geometry, not signed solid penetration.','No simulation rebake, parameter changes, free-vertex exclusion or physical-root approval.'])
out.mkdir(parents=True);(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes());(out/'diagnosis.json').write_text(json.dumps(report,indent=2));print(json.dumps(dict(topology=report['topology'],baseline_contact_free=report['baseline_free_vertices_below_required_gap'],incompatible_root_free_edges=len(rows))),flush=True)
