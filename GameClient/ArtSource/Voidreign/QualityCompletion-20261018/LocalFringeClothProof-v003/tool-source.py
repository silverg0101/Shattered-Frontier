"""Finite CPU fixed-rest cloth proof driven by an exact local source capture.

Independent local experiment only. Does not edit the original full mount or
reset its exhausted rig/gait/secondary acceptance budgets.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import time
import bpy
import numpy as np
from mathutils.bvhtree import BVHTree

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda:stream.read(8*1024*1024),b''):h.update(block)
    return h.hexdigest()

p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);profile=Path(a.profile).resolve();out=Path(a.output).resolve();cfg=json.loads(profile.read_text())
assert cfg['schema_version']==1 and not out.exists() and cfg['scope']=='local-primary-driven-fixed-rest-cloth-proof'
assert 1<=cfg['attempt_index']<=cfg['maximum_local_attempts']<=3
assert 0<=cfg['warmup_frames']<=120 and 1<=cfg['render_stride']<=12
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
review=json.loads(Path(cfg['root_review']).read_text());assert review['status']=='author-reviewed-for-local-physics-only'
capture=json.loads(Path(cfg['capture_report']).read_text());assert capture['status']=='checked-source-primary-follow-review-only'
assert capture['review_source_sha256']==cfg['input_sha256'][cfg['source']]
with np.load(cfg['capture_points']) as ar:vertices,source_faces,parent_points=ar['feature_vertices'],ar['feature_faces'],ar['feature_world_points'];source_frames=ar['frames']
with np.load(cfg['semantic_selection']) as ar:canonical=ar['canonical_node']
with np.load(cfg['region_roots']) as ar:roots=ar['root_nodes']
nodes,first=np.unique(canonical[vertices],return_index=True);mapping=np.searchsorted(nodes,canonical[vertices]);pins=np.searchsorted(nodes,roots);assert (nodes[pins]==roots).all()
assert len(nodes)<=2000 and parent_points.shape[0]<=500
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()));scene=bpy.context.scene
display=bpy.data.objects['ACTUAL FRINGE | primary follow only'];body=bpy.data.objects['SOURCE LEG | local context']
assert len(display.data.vertices)==len(vertices) and not any(m.type=='CLOTH' for ob in scene.objects for m in ob.modifiers)
out.mkdir(parents=True);(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
warmup=cfg['warmup_frames']
for action in bpy.data.actions:
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    for key in curve.keyframe_points:key.co.x+=warmup;key.handle_left.x+=warmup;key.handle_right.x+=warmup
faces=mapping[np.array([p.vertices[:] for p in display.data.polygons])]
internal_settings=cfg.get('internal_springs',{})
split=cfg.get('split_components',False)
assert not internal_settings or split,'Internal springs require isolated components'
components=[np.arange(len(nodes))]
if split:
    parent=np.arange(len(nodes))
    def root(i):
        while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
        return i
    for tri in faces:
        for i,j in [(tri[0],tri[1]),(tri[1],tri[2])]:parent[root(i)]=root(j)
    groups={}
    for i in range(len(nodes)):groups.setdefault(root(i),[]).append(i)
    components=[np.array(v,int) for v in groups.values()]
corrections=cfg.get('component_winding_corrections',[]);matched=[]
assert len({row['canonical_nodes_sha256'] for row in corrections})==len(corrections)
proxies=[];cloths=[];proxy_names=[];joined_order=np.concatenate(components);global_to_joined=np.empty(len(nodes),int);global_to_joined[joined_order]=np.arange(len(nodes))
for component_index,component in enumerate(components):
    selected=np.isin(faces,component).all(1);local_faces=np.searchsorted(component,faces[selected]);local_pins=np.flatnonzero(np.isin(component,pins));assert len(local_pins)>0
    component_hash=hashlib.sha256(nodes[component].astype('<i8').tobytes()).hexdigest()
    for correction in corrections:
        if correction['canonical_nodes_sha256']==component_hash:
            assert correction['operation']=='reverse-simulation-face-winding-only'
            assert cfg['input_sha256'][correction['evidence_path']]==correction['evidence_sha256']
            local_faces=local_faces[:,::-1];matched.append(dict(component=component_index,canonical_nodes_sha256=component_hash,evidence_path=correction['evidence_path']))
    mesh=bpy.data.meshes.new('Actual canonical fringe component '+str(component_index));mesh.from_pydata(parent_points[0,first[component]].tolist(),[],local_faces.tolist());mesh.update()
    mesh.attributes.new('CanonicalGlobalNode','INT','POINT').data.foreach_set('value',component.astype(np.int32))
    proxy=bpy.data.objects.new('SIM | local actual fringe '+str(component_index),mesh);scene.collection.objects.link(proxy);proxy_names.append(proxy.name);proxies.append(proxy)
    proxy.shape_key_add(name='Basis')
    for index,frame in enumerate(source_frames[1:],1):
        shape=proxy.shape_key_add(name='Primary frame '+str(frame));shape.data.foreach_set('co',parent_points[index,first[component]].ravel())
        for at,value in [(int(frame)+warmup-1,0.),(int(frame)+warmup,1.),(int(frame)+warmup+1,0.)]:shape.value=value;shape.keyframe_insert('value',frame=at)
    pin=proxy.vertex_groups.new(name='Exact geometric root candidates');pin.add(local_pins.tolist(),1.,'REPLACE')
    cloth=proxy.modifiers.new('Fixed-rest primary-driven local proof','CLOTH');cloths.append(cloth);cloth.settings.vertex_group_mass=pin.name;cloth.settings.use_dynamic_mesh=False
    for k,v in cfg['cloth_settings'].items():
        prop=cloth.settings.bl_rna.properties[k];assert prop.type in ['INT','FLOAT'] and prop.hard_min<=v<=prop.hard_max;setattr(cloth.settings,k,v)
    cloth.settings.use_internal_springs=bool(internal_settings)
    for k,v in internal_settings.items():
        prop=cloth.settings.bl_rna.properties[k]
        assert prop.type=='BOOLEAN' and type(v) is bool or prop.type in ['INT','FLOAT'] and prop.hard_min<=v<=prop.hard_max
        setattr(cloth.settings,k,v)
    cloth.collision_settings.use_collision=True;cloth.collision_settings.use_self_collision=False;cloth.collision_settings.vertex_group_object_collisions=pin.name
    for k,v in cfg['collision_settings'].items():
        prop=cloth.collision_settings.bl_rna.properties[k];assert prop.type in ['INT','FLOAT'] and prop.hard_min<=v<=prop.hard_max;setattr(cloth.collision_settings,k,v)
    proxy.hide_render=True;proxy.display_type='WIRE';proxy.hide_set(False)
assert len(matched)==len(corrections),'Unmatched winding correction'
for action in bpy.data.actions:
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    for key in curve.keyframe_points:key.interpolation='LINEAR'
collection=bpy.data.collections.new('LOCAL CONTACTS | incomplete whole-mount coverage');scene.collection.children.link(collection)
for c in list(body.users_collection):c.objects.unlink(body)
collection.objects.link(body);body.modifiers.new('Deforming source leg collision','COLLISION');body.collision.thickness_outer=cfg['collider_thickness_m'];body.collision.thickness_inner=.001
bpy.ops.mesh.primitive_plane_add(size=10,location=(0,0,cfg['ground_height_m']));ground=bpy.context.object;ground.name='LOCAL CONTACT | ground';ground.hide_render=True
for c in list(ground.users_collection):c.objects.unlink(ground)
collection.objects.link(ground);ground.modifiers.new('Ground collision','COLLISION');ground.collision.thickness_outer=cfg['collider_thickness_m'];ground.collision.thickness_inner=.001
for cloth in cloths:cloth.collision_settings.collection=collection
attribute=display.data.attributes.new('LocalClothNode','INT','POINT');attribute.data.foreach_set('value',mapping.astype(np.int32))
tree=bpy.data.node_groups.new('Exact source-to-canonical cloth sampling','GeometryNodeTree');tree.interface.new_socket(name='Geometry',in_out='INPUT',socket_type='NodeSocketGeometry');tree.interface.new_socket(name='Geometry',in_out='OUTPUT',socket_type='NodeSocketGeometry')
ns,links=tree.nodes,tree.links;inp=ns.new('NodeGroupInput');output=ns.new('NodeGroupOutput');join=ns.new('GeometryNodeJoinGeometry')
for proxy in proxies:
    info=ns.new('GeometryNodeObjectInfo');info.transform_space='RELATIVE';info.inputs['Object'].default_value=proxy;links.new(info.outputs['Geometry'],join.inputs['Geometry'])
sort=ns.new('GeometryNodeSortElements');sort.domain='POINT';global_index=ns.new('GeometryNodeInputNamedAttribute');global_index.data_type='INT';global_index.inputs['Name'].default_value='CanonicalGlobalNode';links.new(join.outputs['Geometry'],sort.inputs['Geometry']);links.new(global_index.outputs['Attribute'],sort.inputs['Sort Weight'])
sample=ns.new('GeometryNodeSampleIndex');sample.data_type='FLOAT_VECTOR';sample.domain='POINT';sample.clamp=False;position=ns.new('GeometryNodeInputPosition');index=ns.new('GeometryNodeInputNamedAttribute');index.data_type='INT';index.inputs['Name'].default_value=attribute.name;setpos=ns.new('GeometryNodeSetPosition')
links.new(sort.outputs['Geometry'],sample.inputs['Geometry']);links.new(position.outputs['Position'],sample.inputs['Value']);links.new(index.outputs['Attribute'],sample.inputs['Index']);links.new(inp.outputs['Geometry'],setpos.inputs['Geometry']);links.new(sample.outputs['Value'],setpos.inputs['Position']);links.new(setpos.outputs['Geometry'],output.inputs['Geometry'])
display.modifiers.new('Actual geometry | local cloth sampling','NODES').node_group=tree
bpy.data.curves['Scope'].body='LOCAL SECONDARY PROOF | COVERAGE INCOMPLETE'
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=cfg['render_samples'];scene.render.fps=cfg['fps'];scene.gravity=(0,0,-cfg['gravity_m_s2']);scene.frame_start=1;scene.frame_end=int(source_frames[-1])+warmup
for i,cloth in enumerate(cloths):
    pc=cloth.point_cache;pc.frame_start=1;pc.frame_end=scene.frame_end;pc.use_disk_cache=True;pc.name='LocalFringeFixedRest'+str(i)
candidate=out/'Actual-FringeSecondary.blend';scene.frame_set(1);bpy.ops.wm.save_as_mainfile(filepath=str(candidate),check_existing=False)
started=time.monotonic()
for proxy,cloth in zip(proxies,cloths):
    scene.frame_set(1);bpy.ops.object.select_all(action='DESELECT');proxy.select_set(True);bpy.context.view_layer.objects.active=proxy;pc=cloth.point_cache
    with bpy.context.temp_override(object=proxy,active_object=proxy,point_cache=pc):bpy.ops.ptcache.bake(bake=True)
    assert pc.is_baked
bake_seconds=time.monotonic()-started
def evaluated_points(ob):return np.array([v.co[:] for v in ob.evaluated_get(bpy.context.evaluated_depsgraph_get()).data.vertices],float)
def evaluated_simulation():
    result=np.empty((len(nodes),3))
    for name,component in zip(proxy_names,components):result[component]=evaluated_points(bpy.data.objects[name])
    return result
edge=np.unique(np.sort(np.concatenate([faces[:,[0,1]],faces[:,[1,2]],faces[:,[2,0]]]),axis=1),axis=0);rest=parent_points[0,first].astype(float);rest_length=np.linalg.norm(rest[edge[:,0]]-rest[edge[:,1]],axis=1)
free=np.ones(len(nodes),bool);free[pins]=False
curves=[];points=[];held=[];maximum_sampling=0.;maximum_pin=0.
for at in [max(1,warmup-1),max(1,warmup)]:scene.frame_set(at);held.append(evaluated_simulation())
for index,frame in enumerate(source_frames):
    scene.frame_set(int(frame)+warmup);sim=evaluated_simulation();actual=evaluated_points(display);maximum_sampling=max(maximum_sampling,float(np.linalg.norm(actual-sim[mapping],axis=1).max()));gap=float(np.linalg.norm(sim[pins]-parent_points[index,first[pins]],axis=1).max());maximum_pin=max(maximum_pin,gap)
    err=abs(np.linalg.norm(sim[edge[:,0]]-sim[edge[:,1]],axis=1)-rest_length)
    body_points=evaluated_points(body);body_faces=[p.vertices[:] for p in body.data.polygons];bvh=BVHTree.FromPolygons(body_points.tolist(),body_faces,all_triangles=True)
    distances=[bvh.find_nearest(tuple(q))[3] for q in sim[free]]
    curves.append(dict(source_frame=int(frame),scene_frame=int(frame)+warmup,maximum_pin_error_m=gap,maximum_edge_change_m=float(err.max()),minimum_free_unsigned_leg_distance_m=float(min(distances)),minimum_free_ground_clearance_m=float(sim[free,2].min()-cfg['ground_height_m']),maximum_free_parent_offset_m=float(np.linalg.norm(sim[free]-parent_points[index,first[free]],axis=1).max())))
    points.append(sim)
points=np.array(points);assert np.isfinite(points).all();bpy.ops.wm.save_as_mainfile(filepath=str(candidate),check_existing=False)
np.savez_compressed(out/'simulated-points.npz',source_frames=source_frames,canonical_nodes=nodes,source_vertices=vertices,source_mapping=mapping,root_local_nodes=pins,world_points=points)
bpy.ops.wm.open_mainfile(filepath=str(candidate));scene=bpy.context.scene;maximum_replay=0.
for index,frame in enumerate(source_frames):
    scene.frame_set(int(frame)+warmup);sim=evaluated_simulation();maximum_replay=max(maximum_replay,float(np.linalg.norm(sim-points[index],axis=1).max()))
assert maximum_replay<=cfg['replay_tolerance_m'] and maximum_sampling<=cfg['replay_tolerance_m']
images=[]
for frame in source_frames[::cfg['render_stride']]:
    scene.frame_set(int(frame)+warmup);name='frames/'+str(frame).zfill(4)+'.png';scene.render.filepath=str(out/name);bpy.ops.render.render(write_still=True);images.append(dict(frame=int(frame),scene_frame=int(frame)+warmup,image=name))
cache_files=[dict(path=str(p.relative_to(out)),sha256=sha(p),bytes=p.stat().st_size) for p in sorted(out.glob('blendcache_*/*')) if p.is_file()]
report=dict(schema_version=1,status='local-fixed-rest-cloth-proof-needs-review',cpu_only=True,input_sha256=cfg['input_sha256'],tool_sha256=sha(__file__),profile_sha256=sha(profile),source=candidate.name,source_sha256=sha(candidate),bake_seconds=bake_seconds,warmup_frames=warmup,source_frames=len(source_frames),canonical_nodes=len(nodes),root_nodes=len(pins),fixed_rest=True,self_collision=False,held_final_free_speed_m_s=float(np.linalg.norm(held[1]-held[0],axis=1)[free].max()*cfg['fps']),maximum_pin_error_m=maximum_pin,maximum_source_sampling_error_m=maximum_sampling,maximum_reopened_replay_error_m=maximum_replay,maximum_free_step_m=float(np.linalg.norm(np.diff(points,axis=0),axis=2)[:,free].max()),curves=curves,images=images,cache_files=cache_files,limitations=['Finite local experiment, not a new full-mount rig/gait or reset of exhausted stage budgets.','Local cropped/open leg mesh and ground only; missing nearby tail/otherleg, self/cross-feature and whole-mount collision coverage.','Unsigned vertex-to-leg distances are sampled proximity, not signed solid/triangle/swept penetration acceptance.','Input drivers are linearly interpolated integer-frame source captures, not freshly evaluated skeletal subframe trajectories.','Pin-region anatomy, settled startup, surface strain and visible response still require review; the native group excludes triangles whose vertices are all roots, not arbitrary individual vertices.'])
report.update(simulation_components=[dict(object=name,canonical_nodes=len(component)) for name,component in zip(proxy_names,components)],internal_springs=bool(internal_settings),cross_component_internal_springs=False,winding_corrections=matched)
(out/'cloth-report.json').write_text(json.dumps(report,indent=2));(out/'capture-report.json').write_text(json.dumps(dict(status=report['status'],source=report['source'],source_sha256=report['source_sha256'],images=images,scope='LOCAL SECONDARY PROOF ONLY'),indent=2))
print(json.dumps({k:report[k] for k in ['status','canonical_nodes','root_nodes','held_final_free_speed_m_s','maximum_pin_error_m','maximum_reopened_replay_error_m','maximum_free_step_m']}),flush=True)
