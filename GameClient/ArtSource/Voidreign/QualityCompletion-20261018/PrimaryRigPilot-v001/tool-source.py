"""CPU anatomical rig and short joint-deformation pilot on the original mesh.

Authored source-bound proposal, not automatic locomotion or runtime acceptance.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
from mathutils import Matrix, Quaternion, Vector
import numpy as np

def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(8*1024*1024), b''):
            h.update(chunk)
    return h.hexdigest()

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:])
profile,out=Path(a.profile).resolve(),Path(a.output).resolve()
cfg=json.loads(profile.read_text());assert cfg['schema_version']==1 and cfg['units']=='metres' and not out.exists()
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
assert 1<=cfg['weights']['maximum_influences']<=4
assert .001<=cfg['weights']['distance_floor_m']<=.1
assert 1<=cfg['weights']['distance_exponent']<=4
with np.load(cfg['rest_selection']) as r:
    points,local,triangles=r['vertices'].astype(float),r['local_vertices'].astype(float),r['triangles']
with np.load(cfg['semantic_selection']) as s:
    labels,canonical=s['labels'],s['canonical_node']
with np.load(cfg['domains']) as d:owners=d['owner_per_vertex']
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()))
obj=bpy.data.objects[cfg['object']]
raw=np.empty(len(points)*3,np.float32);obj.data.vertices.foreach_get('co',raw)
assert np.array_equal(raw.reshape(-1,3),local.astype(np.float32))
assert np.array_equal(np.array([list(f.vertices) for f in obj.data.polygons]),triangles)
# Fit the independently captured undeformed object-to-world transform, never pose samples.
fit=np.linalg.lstsq(np.column_stack((local,np.ones(len(local)))),points,rcond=None)[0]
matrix=np.eye(4);matrix[:3,:]=fit.T
error=float(np.linalg.norm(np.column_stack((local,np.ones(len(local))))@fit-points,axis=1).max())
assert error<=cfg['limits']['rest_correspondence_m']
obj.parent=None;obj.animation_data_clear();obj.matrix_world=Matrix(matrix.tolist())
disabled=[]
for mod in obj.modifiers:
    disabled.append(dict(name=mod.name,type=mod.type,viewport=mod.show_viewport,render=mod.show_render))
    mod.show_viewport=mod.show_render=False
for other in bpy.context.scene.objects:
    if other!=obj:other.hide_render=True;other.hide_set(True)
arm=bpy.data.armatures.new('Primary anatomical pilot')
rig=bpy.data.objects.new('PRIMARY | anatomical rig pilot',arm)
bpy.context.collection.objects.link(rig)
bpy.context.view_layer.objects.active=rig;rig.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')
for row in cfg['bones']:
    bone=arm.edit_bones.new(row['id']);bone.head=row['head_m'];bone.tail=row['tail_m'];bone.use_deform=True
    assert (bone.tail-bone.head).length>.001
    if row.get('parent'):bone.parent=arm.edit_bones[row['parent']]
bpy.ops.object.mode_set(mode='OBJECT')
rig.show_in_front=True
names=[r['id'] for r in cfg['bones']]
assert all(obj.vertex_groups.get(name) is None for name in names)
groups=[obj.vertex_groups.new(name=name) for name in names]
# Record original groups; new primary weights are additive and old deformation is disabled.
old_count=len(obj.vertex_groups)-len(groups)
segments=[(np.array(r['head_m']),np.array(r['tail_m'])) for r in cfg['bones']]
distance=np.empty((len(points),len(names)),np.float64)
for j,(start,end) in enumerate(segments):
    delta=end-start;t=np.clip((points-start)@delta/(delta@delta),0,1)
    distance[:,j]=np.linalg.norm(points-start-t[:,None]*delta,axis=1)
eligible=np.zeros(distance.shape,bool)
body=[names.index(n) for n in cfg['body_bones']]
eligible[np.ix_(np.flatnonzero(owners==cfg['body_owner_id']),body)]=True
for leg in cfg['legs']:
    idx=np.flatnonzero(owners==leg['owner_id']);cols=[names.index(n) for n in leg['bones']+leg['root_blend_bones']]
    eligible[np.ix_(idx,cols)]=True
    # Blend upper limb into torso only inside its authored root volume.
    low,high=np.array(leg['root_minimum_m']),np.array(leg['root_maximum_m'])
    idx=np.flatnonzero((owners==cfg['body_owner_id'])&np.all((points>=low)&(points<=high),axis=1))
    eligible[np.ix_(idx,[names.index(leg['bones'][0])])]=True
for label,bone_names in cfg['semantic_bones'].items():
    idx=np.flatnonzero(labels==int(label));eligible[idx,:]=False
    eligible[np.ix_(idx,[names.index(n) for n in bone_names])]=True
unweighted=~eligible.any(axis=1)
distance[~eligible]=np.inf
order=np.argsort(distance,axis=1)[:,:cfg['weights']['maximum_influences']]
chosen=np.take_along_axis(distance,order,axis=1)
weight=np.where(np.isfinite(chosen),np.maximum(chosen,cfg['weights']['distance_floor_m'])**(-cfg['weights']['distance_exponent']),0.)
total=weight.sum(axis=1);weight/=np.maximum(total[:,None],1e-30)
for i in range(len(points)):
    for j,w in zip(order[i],weight[i]):
        if w>0:groups[int(j)].add([i],float(w),'REPLACE')
mod=obj.modifiers.new('Primary anatomy | proposal weights','ARMATURE');mod.object=rig
scene=bpy.context.scene;scene.frame_start=1;scene.frame_end=cfg['frame_end'];scene.render.fps=cfg['fps']
for pose in cfg['poses']:
    for name in names:
        pb=rig.pose.bones[name];pb.rotation_mode='QUATERNION';pb.rotation_quaternion=(1,0,0,0)
    for rotation in pose['rotations']:
        pb=rig.pose.bones[rotation['bone']]
        axis=arm.bones[pb.name].matrix_local.to_3x3().inverted()@Vector(rotation['world_axis'])
        pb.rotation_quaternion=Quaternion(axis,rotation['angle_radians'])
    for name in names:rig.pose.bones[name].keyframe_insert('rotation_quaternion',frame=pose['frame'])
out.mkdir(parents=True);(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
np.savez_compressed(out/'primary-weights.npz',bone_indices=order,weights=weight,unweighted_vertex_ids=np.flatnonzero(unweighted))
# Render directly from the saved actual mesh; no old rigid harness, mannequins or cache proxies.
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=cfg['render']['samples'];scene.cycles.use_denoising=False
scene.render.resolution_x=scene.render.resolution_y=cfg['render']['resolution'];scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('Primary pilot world');scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.4,.4,.4,1)
scene.world.node_tree.nodes['Background'].inputs[1].default_value=.8
bpy.ops.object.light_add(type='AREA',location=cfg['render']['light_position_m']);light=bpy.context.object
light.data.energy=cfg['render']['light_watts'];light.data.size=cfg['render']['light_size_m']
target=Vector(cfg['render']['target_m']);light.rotation_euler=(target-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=cfg['render']['camera_position_m']);camera=bpy.context.object;scene.camera=camera
camera.data.type='ORTHO';camera.data.ortho_scale=cfg['render']['ortho_scale_m'];camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
scene.frame_set(1)
candidate=out/'Mount-PrimaryRigPilot.blend';bpy.ops.wm.save_as_mainfile(filepath=str(candidate),relative_remap=True)
bpy.ops.wm.open_mainfile(filepath=str(candidate));obj=bpy.data.objects[cfg['object']];scene=bpy.context.scene
first=np.unique(canonical,return_index=True)[1];reference=first[canonical]
records=[]
for pose in cfg['poses']:
    scene.frame_set(pose['frame']);evaluated=obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    v=np.empty(len(points)*3,np.float32);evaluated.data.vertices.foreach_get('co',v)
    v=np.column_stack((v.reshape(-1,3),np.ones(len(points))))@np.array(obj.matrix_world).T;v=v[:,:3]
    seams=float(np.linalg.norm(v-v[reference],axis=1).max())
    records.append(dict(frame=pose['frame'],name=pose['id'],maximum_displacement_m=float(np.linalg.norm(v-points,axis=1).max()),coincident_seam_error_m=seams))
    scene.render.image_settings.file_format='PNG';scene.render.filepath=str(out/(pose['id']+'.png'));bpy.ops.render.render(write_still=True)
report=dict(schema_version=1,status='primary-rig-joint-pilot-needs-review',cpu_only=True,input_sha256=cfg['input_sha256'],tool_sha256=sha(__file__),profile_sha256=sha(profile),candidate_sha256=sha(candidate),primary_bones=len(names),original_vertex_groups_retained=old_count,unweighted_vertices=int(unweighted.sum()),rest_transform_fit_error_m=error,poses=records,disabled_original_modifiers=disabled,
 limitations=['Authored landmarks and geodesic ownership are provisional; not gait/stance/contact/semantic acceptance.',
 'Unresolved disconnected nonhair remains unweighted, not silently nearest-bound or deleted.',
 'Original secondary modifiers are disabled. Articulated root targets/colliders and loose-item motion remain to be rebuilt.',
 'No IK, locomotion transitions, skeletal export or engine validation in this joint-only pilot.'])
(out/'rig-report.json').write_text(json.dumps(report,indent=2))
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
print(json.dumps({k:report[k] for k in ['status','primary_bones','unweighted_vertices','poses']}),flush=True)
