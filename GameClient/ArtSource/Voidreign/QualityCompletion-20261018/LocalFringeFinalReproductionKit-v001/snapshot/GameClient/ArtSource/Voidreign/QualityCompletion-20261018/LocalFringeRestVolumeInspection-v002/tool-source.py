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
cases.sort(key=lambda row:row['unsigned_gap_m'])
detail={}
if cfg.get('inspect_worst_topology_and_warmup',False):
    from collections import defaultdict
    worst=cases[0];scene.frame_set(worst['scene_frame']);dg=bpy.context.evaluated_depsgraph_get()
    bp=np.array([v.co[:] for v in body.evaluated_get(dg).data.vertices]);tris=np.array([p.vertices[:] for p in body.data.polygons]);fi=int(np.flatnonzero(source_faces==worst['source_body_face'])[0])
    incidence=defaultdict(list)
    for ti,tri in enumerate(tris):
        for u,v in zip(tri,np.roll(tri,-1)):incidence[tuple(sorted((int(u),int(v))))].append((ti,int(u),int(v)))
    boundary=np.array([edge for edge,rows in incidence.items() if len(rows)==1],int).reshape(-1,2)
    def boundary_distance(point):
        if not len(boundary):return None
        start=bp[boundary[:,0]];delta=bp[boundary[:,1]]-start;length=np.einsum('ij,ij->i',delta,delta)
        t=np.clip(np.einsum('ij,ij->i',point-start,delta)/np.maximum(length,np.finfo(float).tiny),0,1)
        return float(np.linalg.norm(point-start-t[:,None]*delta,axis=1).min())
    local=int(np.flatnonzero(nodes==worst['canonical_node'])[0]);sim_incident=[]
    for row in r['simulation_components']:
        ob=bpy.data.objects[row['object']];ids=np.array([v.value for v in ob.data.attributes['CanonicalGlobalNode'].data]);matches=np.flatnonzero(ids==local)
        if not len(matches):continue
        cloth=next(m for m in ob.modifiers if m.type=='CLOTH');group=ob.vertex_groups[cloth.collision_settings.vertex_group_object_collisions];excluded={v.index for v in ob.data.vertices if any(g.group==group.index and g.weight>0 for g in v.groups)}
        for poly in ob.data.polygons:
            if int(matches[0]) in poly.vertices:sim_incident.append(dict(component=ob.name,face=poly.index,global_nodes=ids[list(poly.vertices)].tolist(),excluded_from_object_collision=all(v in excluded for v in poly.vertices)))
    held=[];previous=None
    for frame in range(1,r['warmup_frames']+1):
        scene.frame_set(frame);dg=bpy.context.evaluated_depsgraph_get();current=np.empty((len(nodes),3))
        for row in r['simulation_components']:
            ob=bpy.data.objects[row['object']];ids=np.array([v.value for v in ob.data.attributes['CanonicalGlobalNode'].data]);current[ids]=np.array([v.co[:] for v in ob.evaluated_get(dg).data.vertices])
        if previous is not None:
            speeds=np.linalg.norm(current-previous,axis=1)*scene.render.fps
            held.append(dict(scene_frame=frame,maximum_free_speed_m_s=float(speeds[free].max()),rms_free_speed_m_s=float(np.sqrt(np.mean(speeds[free]**2)))))
        previous=current
    body_source=np.array([v.value for v in body.data.attributes['source_vertex'].data])
    detail=dict(collider_vertices=len(bp),collider_triangles=len(tris),indexed_boundary_edges=len(boundary),nonmanifold_edges=sum(len(v)>2 for v in incidence.values()),inconsistently_directed_shared_edges=sum(len(v)==2 and v[0][1:]==v[1][1:] for v in incidence.values()),worst_body_source_vertices=body_source[tris[fi]].tolist(),worst_body_triangle_indexed_boundary_edges=sum(len(incidence[tuple(sorted((int(u),int(v))))])==1 for u,v in zip(tris[fi],np.roll(tris[fi],-1))),worst_contact_distance_to_indexed_boundary_m=boundary_distance(np.array(worst['closest_body_point_m'])),worst_incident_simulation_faces=sim_incident,held_speed_trace=held)
    if cfg.get('semantic_selection'):
        with np.load(cfg['semantic_selection']) as ar:canonical=ar['canonical_node']
        body_canonical=canonical[body_source];canonical_incidence=defaultdict(list)
        for ti,tri in enumerate(tris):
            for u,v in zip(tri,np.roll(tri,-1)):canonical_incidence[tuple(sorted((int(body_canonical[u]),int(body_canonical[v]))))].append((ti,int(u),int(v)))
        geometric_boundary=[rows[0][1:] for rows in canonical_incidence.values() if len(rows)==1]
        boundary=np.array(geometric_boundary,int).reshape(-1,2)
        seam_gaps=[]
        for node in np.unique(body_canonical):
            copies=bp[body_canonical==node]
            if len(copies)>1:seam_gaps.append(float(np.linalg.norm(copies-copies[0],axis=1).max()))
        detail.update(canonical_boundary_edges=len(boundary),canonical_nonmanifold_edges=sum(len(v)>2 for v in canonical_incidence.values()),maximum_canonical_seam_copy_gap_m=max(seam_gaps,default=0.),worst_contact_distance_to_canonical_boundary_m=boundary_distance(np.array(worst['closest_body_point_m'])),worst_triangle_canonical_boundary_edges=sum(len(canonical_incidence[tuple(sorted((int(body_canonical[u]),int(body_canonical[v]))))])==1 for u,v in zip(tris[fi],np.roll(tris[fi],-1))))
if cfg.get('inspect_rest_volume_rays',False):
    from mathutils import Vector
    volume=[]
    for row in r['simulation_components']:
        ob=bpy.data.objects[row['object']];mesh=ob.data;mesh.update();cloth=next(m for m in ob.modifiers if m.type=='CLOTH')
        rest=np.array([v.co[:] for v in mesh.vertices]);faces=[p.vertices[:] for p in mesh.polygons];tree=BVHTree.FromPolygons(rest.tolist(),faces,all_triangles=True)
        ids=np.array([v.value for v in mesh.attributes['CanonicalGlobalNode'].data]);root=np.isin(ids,pins);samples=[];reverse_hits=0
        for i,v in enumerate(mesh.vertices):
            direction=-v.normal;origin=v.co+direction*np.finfo(np.float32).eps
            hit,normal,face,distance=tree.ray_cast(origin,direction,cloth.settings.internal_spring_max_length)
            eligible=hit is not None and i not in faces[face] and normal.dot(direction)>=0
            opposite_hit=tree.ray_cast(v.co-direction*np.finfo(np.float32).eps,-direction,cloth.settings.internal_spring_max_length)[0]
            reverse_hits+=int(opposite_hit is not None and not root[i])
            samples.append(dict(canonical_node=int(nodes[ids[i]]),root=bool(root[i]),normal_length=float(v.normal.length),hit=hit is not None,geometric_opposite_normal_eligible=eligible,distance_m=float(distance) if hit is not None else None))
        edge_faces={}
        for fi,tri in enumerate(faces):
            for u,v in zip(tri,tri[1:]+tri[:1]):edge_faces.setdefault(tuple(sorted((u,v))),[]).append((u,v))
        import bmesh
        bm=bmesh.new();bm.from_mesh(mesh);boundary=[e for e in bm.edges if e.is_boundary];bmesh.ops.holes_fill(bm,edges=boundary,sides=0);bmesh.ops.triangulate(bm,faces=list(bm.faces));closed=all(e.is_manifold for e in bm.edges);signed_volume=float(bm.calc_volume(signed=True)) if closed else None;bm.free()
        volume.append(dict(component=ob.name,vertices=len(rest),internal_springs_enabled=cloth.settings.use_internal_springs,native_maximum_creation_length_m=cloth.settings.internal_spring_max_length,native_maximum_diversion_rad=cloth.settings.internal_spring_max_diversion,inconsistent_shared_edges=sum(len(v)==2 and v[0]==v[1] for v in edge_faces.values()),free_vertices=int((~root).sum()),eligible_free_normal_rays=sum(s['geometric_opposite_normal_eligible'] and not s['root'] for s in samples),reverse_free_ray_hits=reverse_hits,temporary_capped_manifold=closed,temporary_capped_signed_volume_m3=signed_volume,samples=samples))
    detail['rest_volume_normal_ray_probe']=volume
    detail['volume_probe_scope']='Zero-diversion geometric probe only. Native5.1.1 uses seeded diverted rays and target/pair filtering; these are NOT actual native spring counts or a volume guarantee.'
    detail['native_source_reference']='https://github.com/blender/blender/blob/v5.1.1/source/blender/blenkernel/intern/cloth.cc#L1278-L1463'
out.mkdir(parents=True);(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes());(out/'contacts.json').write_text(json.dumps(dict(schema_version=1,status='source-bound-local-contact-diagnostic-only',input_sha256=cfg['input_sha256'],tool_sha256=sha(__file__),profile_sha256=sha(profile),maximum_cached_replay_error_m=maximum_replay,cases=cases[:20],detail=detail,limitations=['Nearest sampled vertex/triangle gaps and oriented face-normal signs, not signed solid/triangle/swept collision acceptance.','Cropped/open source collider may have seams, holes and ambiguous closest-face normals.','No rebake, contact-exclusion expansion or tolerance changes.']),indent=2));print(json.dumps(dict(status='source-bound-local-contact-diagnostic-only',maximum_cached_replay_error_m=maximum_replay,worst=cases[0],detail_summary={k:v for k,v in detail.items() if k not in ['held_speed_trace','worst_incident_simulation_faces','rest_volume_normal_ray_probe']})),flush=True)
