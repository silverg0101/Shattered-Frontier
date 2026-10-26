"""Bind worst sampled local cloth gaps to canonical nodes and source faces."""
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
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);profile=Path(a.profile).resolve();out=Path(a.output).resolve();cfg=json.loads(profile.read_text());assert cfg['schema_version']==1 and not out.exists()
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
r=json.loads(Path(cfg['producer_report']).read_text());assert r['source_sha256']==cfg['input_sha256'][cfg['source']]
for row in r['cache_files']:assert sha(Path(cfg['source']).parent/row['path'])==row['sha256']
with np.load(cfg['simulated_points']) as ar:nodes,frames,pins,points=ar['canonical_nodes'],ar['source_frames'],ar['root_local_nodes'],ar['world_points']
with np.load(cfg['region_roots']) as ar:distances=ar['root_distance_m']
free=np.ones(len(nodes),bool);free[pins]=False
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()));scene=bpy.context.scene;body=bpy.data.objects['SOURCE LEG | local context'];source_faces=np.empty(len(body.data.polygons),np.int32);body.data.attributes['source_face'].data.foreach_get('value',source_faces)
cases=[];maximum_replay=0.
for index,frame in enumerate(frames):
    scene.frame_set(int(frame)+r['warmup_frames']);dg=bpy.context.evaluated_depsgraph_get();current=np.empty((len(nodes),3))
    for row in r['simulation_components']:
        ob=bpy.data.objects[row['object']];ids=np.empty(len(ob.data.vertices),np.int32);ob.data.attributes['CanonicalGlobalNode'].data.foreach_get('value',ids);current[ids]=np.array([v.co[:] for v in ob.evaluated_get(dg).data.vertices])
    maximum_replay=max(maximum_replay,float(np.linalg.norm(current-points[index],axis=1).max()));assert maximum_replay<=cfg['replay_tolerance_m']
    ev=body.evaluated_get(dg);body_points=np.array([v.co[:] for v in ev.data.vertices]);bvh=BVHTree.FromPolygons(body_points.tolist(),[p.vertices[:] for p in body.data.polygons],all_triangles=True)
    samples=[]
    for local in np.flatnonzero(free):
        hit,normal,face,distance=bvh.find_nearest(tuple(current[local]));samples.append((distance,local,hit,normal,face))
    for distance,local,hit,normal,face in sorted(samples,key=lambda row:row[0])[:3]:
        cases.append(dict(source_frame=int(frame),scene_frame=int(frame)+r['warmup_frames'],canonical_node=int(nodes[local]),unsigned_gap_m=float(distance),oriented_local_normal_gap_m=float(np.dot(current[local]-np.array(hit),np.array(normal))),root_geodesic_m=float(distances[local]),source_body_face=int(source_faces[face]),point_world_m=current[local].tolist(),closest_body_point_m=list(hit)))
cases.sort(key=lambda row:row['unsigned_gap_m']);out.mkdir(parents=True);(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes());(out/'contacts.json').write_text(json.dumps(dict(schema_version=1,status='source-bound-local-contact-diagnostic-only',input_sha256=cfg['input_sha256'],tool_sha256=sha(__file__),profile_sha256=sha(profile),maximum_cached_replay_error_m=maximum_replay,cases=cases[:20],limitations=['Nearest sampled vertex/triangle gaps and oriented face-normal signs, not signed solid/triangle/swept collision acceptance.','Cropped/open source collider may have seams, holes and ambiguous closest-face normals.','No rebake, contact-exclusion expansion or tolerance changes.']),indent=2));print(json.dumps(dict(status='source-bound-local-contact-diagnostic-only',maximum_cached_replay_error_m=maximum_replay,worst=cases[0])),flush=True)
