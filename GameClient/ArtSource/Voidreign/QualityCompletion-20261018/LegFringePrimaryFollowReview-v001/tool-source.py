"""Bake exact source-indexed local review copies, not a new rig or simulation."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np
from mathutils import Vector

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda:stream.read(8*1024*1024),b''):h.update(chunk)
    return h.hexdigest()

p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);profile=Path(a.profile).resolve();out=Path(a.output).resolve();cfg=json.loads(profile.read_text())
assert cfg['schema_version']==1 and cfg['units']=='metres' and not out.exists()
assert 1<=cfg['frame_start']<=cfg['frame_end']<=500 and 1<=cfg['render_stride']<=12
assert 512<=cfg['resolution']<=1024 and 1<=cfg['samples']<=32 and 0<cfg['reopen_tolerance_m']<=1e-5
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
with np.load(cfg['rest_selection']) as ar:faces=ar['triangles'];rest=ar['vertices']
with np.load(cfg['domains']) as ar:owners=ar['owner_per_vertex']
with np.load(cfg['region_roots']) as ar:feature_vertices,feature_faces,roots=ar['source_vertices'],ar['source_faces'],ar['root_vertices']
member=np.zeros(len(rest),bool);member[feature_vertices]=True
context=((owners[faces]==cfg['context_owner']).all(1))&(rest[faces,2].max(1)<cfg['context_maximum_rest_z_m'])
context[feature_faces]=False
context_faces=np.flatnonzero(context);context_vertices,context_inverse=np.unique(faces[context_faces],return_inverse=True)
assert len(context_faces)>0 and np.isin(roots,feature_vertices).all()
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()));scene=bpy.context.scene;obj=bpy.data.objects[cfg['object']]
assert np.array_equal(faces,np.array([p.vertices[:] for p in obj.data.polygons]))
traced=np.union1d(feature_vertices,context_vertices);flookup,clookup=np.searchsorted(traced,feature_vertices),np.searchsorted(traced,context_vertices)
points=[];frames=list(range(cfg['frame_start'],cfg['frame_end']+1))
for frame in frames:
    scene.frame_set(frame);ev=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());points.append(np.array([(obj.matrix_world@ev.data.vertices[int(i)].co)[:] for i in traced],np.float32))
points=np.array(points);assert np.isfinite(points).all()
out.mkdir(parents=True);(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
np.savez_compressed(out/'source-indexed-world-points.npz',frames=np.array(frames),feature_vertices=feature_vertices,feature_faces=feature_faces,feature_world_points=points[:,flookup],context_vertices=context_vertices,context_faces=context_faces,context_world_points=points[:,clookup])
bpy.ops.wm.read_factory_settings(use_empty=True);scene=bpy.context.scene
def material(name,color):
    mat=bpy.data.materials.new(name);mat.use_nodes=True;shader=mat.node_tree.nodes.get('Principled BSDF');shader.inputs['Base Color'].default_value=(*color,1);shader.inputs['Roughness'].default_value=.8;return mat
gray=material('Existing leg context',(.22,.25,.28));cyan=material('Actual fringe - existing primary motion',(.02,.55,.7));green=material('Geometric root candidates',(.15,.7,.1))
def baked_mesh(name,vertices,face_ids,local_faces,positions,mat):
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(positions[0].tolist(),[],local_faces.tolist());mesh.update();ob=bpy.data.objects.new(name,mesh);scene.collection.objects.link(ob);mesh.materials.append(mat)
    for key,domain,values in [('source_vertex','POINT',vertices),('source_face','FACE',face_ids)]:mesh.attributes.new(key,'INT',domain).data.foreach_set('value',values.astype(np.int32))
    ob.shape_key_add(name='Basis')
    for index,frame in enumerate(frames[1:],1):
        shape=ob.shape_key_add(name='Source frame '+str(frame));shape.data.foreach_set('co',positions[index].ravel())
        for at,value in [(frame-1,0.),(frame,1.),(frame+1,0.)]:shape.value=value;shape.keyframe_insert('value',frame=at)
    return ob
local_faces=np.searchsorted(feature_vertices,faces[feature_faces])
feature=baked_mesh('ACTUAL FRINGE | primary follow only',feature_vertices,feature_faces,local_faces,points[:,flookup],cyan)
feature.data.materials.append(green)
for poly,source_face in zip(feature.data.polygons,feature_faces):poly.material_index=int(np.isin(faces[source_face],roots).sum()>=2)
body=baked_mesh('SOURCE LEG | local context',context_vertices,context_faces,context_inverse.reshape((-1,3)),points[:,clookup],gray)
bpy.ops.object.camera_add();camera=bpy.context.object;scene.camera=camera;camera.data.type='ORTHO';camera.data.ortho_scale=cfg['ortho_scale_m']
offset=Vector(cfg['camera_offset_m']);camera.rotation_euler=(-offset).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.light_add(type='AREA');light=bpy.context.object;light.data.energy=25;light.data.size=1
for index,frame in enumerate(frames):
    target=Vector(points[index,flookup].mean(0));camera.location=target+offset;camera.keyframe_insert('location',frame=frame)
    light.location=target+Vector((1,-1,2));light.rotation_euler=(target-light.location).to_track_quat('-Z','Y').to_euler();light.keyframe_insert('location',frame=frame)
scene.world=bpy.data.worlds.new('Review world');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.18,.18,.18,1);scene.world.node_tree.nodes['Background'].inputs[1].default_value=.6
text_data=bpy.data.curves.new('Scope','FONT');text_data.body='EXISTING PRIMARY FOLLOW | NO SECONDARY DYNAMICS';text_data.size=.018
caption=bpy.data.objects.new('SCOPE - not a new simulation',text_data);scene.collection.objects.link(caption);caption.parent=camera;caption.location=(-.52,-.51,-1)
textmat=bpy.data.materials.new('Scope text');textmat.use_nodes=True;nodes=textmat.node_tree.nodes;nodes.clear();em=nodes.new('ShaderNodeEmission');em.inputs['Color'].default_value=(.8,.8,.8,1);outnode=nodes.new('ShaderNodeOutputMaterial');textmat.node_tree.links.new(em.outputs[0],outnode.inputs[0]);text_data.materials.append(textmat)
for action in bpy.data.actions:
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    for key in curve.keyframe_points:key.interpolation='LINEAR'
scene.frame_start,scene.frame_end=frames[0],frames[-1];scene.render.fps=cfg['fps'];scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=cfg['samples'];scene.cycles.use_denoising=False
scene.render.resolution_x=scene.render.resolution_y=cfg['resolution'];scene.render.resolution_percentage=100;scene.render.image_settings.file_format='PNG';scene.frame_set(frames[0])
candidate=out/'Actual-FringePrimaryFollow.blend';bpy.ops.wm.save_as_mainfile(filepath=str(candidate),check_existing=False)
mesh_specs=[(feature.name,flookup),(body.name,clookup)]
bpy.ops.wm.open_mainfile(filepath=str(candidate));scene=bpy.context.scene;maximum=0.
for index,frame in enumerate(frames):
    scene.frame_set(frame)
    for name,lookup in mesh_specs:
        ev=bpy.data.objects[name].evaluated_get(bpy.context.evaluated_depsgraph_get());actual=np.array([v.co[:] for v in ev.data.vertices]);maximum=max(maximum,float(np.linalg.norm(actual-points[index,lookup],axis=1).max()))
assert maximum<=cfg['reopen_tolerance_m'],maximum
images=[]
for frame in frames[::cfg['render_stride']]:
    scene.frame_set(frame);name='frames/'+str(frame).zfill(4)+'.png';scene.render.filepath=str(out/name);bpy.ops.render.render(write_still=True);images.append(dict(frame=frame,image=name))
(out/'capture-report.json').write_text(json.dumps(dict(schema_version=1,status='checked-source-primary-follow-review-only',cpu_only=True,input_sha256=cfg['input_sha256'],profile_sha256=sha(profile),tool_sha256=sha(__file__),review_source=candidate.name,review_source_sha256=sha(candidate),feature_vertices=len(feature_vertices),feature_faces=len(feature_faces),context_faces=len(context_faces),frames=len(frames),maximum_reopened_position_error_m=maximum,images=images,limitations=['Exact-index baked local source copies, not a new skeleton, skin, gait or simulation.','Only source-following motion is shown; secondary dynamics/collision and anatomical root approval remain open.','Local context is cropped and camera tracks the fringe; not full-mount gait/stance acceptance.','Shape-key diagnostic cache is not a game-ready runtime rig.']),indent=2));assert sha(cfg['source'])==cfg['input_sha256'][cfg['source']]
print(json.dumps(dict(status='checked-source-primary-follow-review-only',maximum_reopened_position_error_m=maximum,rendered_frames=len(images))),flush=True)
