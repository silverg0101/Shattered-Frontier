"""CPU foot-control and standing-contact pilot; immutable articulated candidate."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
import bpy
from mathutils import Vector
import numpy as np

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda:stream.read(8*1024*1024),b''):h.update(chunk)
    return h.hexdigest()

p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);profile,out=Path(a.profile).resolve(),Path(a.output).resolve();cfg=json.loads(profile.read_text())
assert cfg['schema_version']==1 and cfg['units']=='metres' and not out.exists()
assert 1<=cfg['ik_iterations']<=256 and 0<cfg['pole_distance_m']<=2
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
for key in ['source','rig_profile','sole_patches','sole_report','rest_selection','semantic_selection','domains']:assert cfg[key] in cfg['input_sha256']
rig_cfg=json.loads(Path(cfg['rig_profile']).read_text());soles=json.loads(Path(cfg['sole_report']).read_text())
sole_rows={r['id']:r for r in soles['feet']}
with np.load(cfg['rest_selection']) as r:points=r['vertices'].astype(float)
with np.load(cfg['semantic_selection']) as r:canonical=r['canonical_node']
with np.load(cfg['domains']) as r:owners=r['owner_per_vertex']
patches=np.load(cfg['sole_patches'])
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()));scene=bpy.context.scene
obj=bpy.data.objects[cfg['object']];rig=bpy.data.objects[cfg['rig_object']];rig.animation_data_clear()
for pb in rig.pose.bones:
    pb.location=(0,0,0);pb.rotation_mode='QUATERNION';pb.rotation_quaternion=(1,0,0,0);pb.scale=(1,1,1)
    for c in list(pb.constraints):pb.constraints.remove(c)
primary_names=[r['id'] for r in rig_cfg['bones']]
controls=[];calibration=[];weight_rows=[]
for foot in cfg['feet']:
    name=foot['id'];chain=foot['chain'];last=rig.data.bones[chain[-1]]
    endpoint=Vector(rig.data.bones[chain[-2]].tail_local)
    target=bpy.data.objects.new('FOOT TARGET | '+name,None);scene.collection.objects.link(target)
    target.empty_display_type='CUBE';target.empty_display_size=.07;target.location=endpoint
    target.rotation_mode='QUATERNION';target.rotation_quaternion=last.matrix_local.to_quaternion()
    start=Vector(rig.data.bones[chain[0]].head_local);knee=Vector(rig.data.bones[chain[0]].tail_local)
    axis=(endpoint-start).normalized();bend=knee-start-axis*(knee-start).dot(axis)
    assert bend.length>.001,'Authored pole required for straight limb'
    pole=bpy.data.objects.new('FOOT POLE | '+name,None);scene.collection.objects.link(pole)
    pole.location=knee+bend.normalized()*cfg['pole_distance_m'];pole.empty_display_size=.06
    ik=rig.pose.bones[chain[-2]].constraints.new('IK');ik.name='Primary foot IK | '+name
    ik.target=target;ik.pole_target=pole;ik.chain_count=len(chain)-1;ik.iterations=cfg['ik_iterations'];ik.use_stretch=False
    for bone in chain:rig.pose.bones[bone].ik_stretch=0
    rotation=rig.pose.bones[chain[-1]].constraints.new('COPY_ROTATION');rotation.name='Hoof orientation | '+name
    rotation.target=target;rotation.owner_space='WORLD';rotation.target_space='WORLD'
    # Finite rest-frame pole-axis calibration, not a response/limit search.
    scores=[]
    for angle in cfg['pole_calibration_angles_radians']:
        ik.pole_angle=angle;bpy.context.view_layer.update()
        error=sum((rig.pose.bones[n].head-rig.data.bones[n].head_local).length_squared for n in chain)
        scores.append(dict(angle_radians=angle,squared_joint_error_m2=error))
    best=min(scores,key=lambda r:r['squared_joint_error_m2']);ik.pole_angle=best['angle_radians']
    calibration.append(dict(id=name,candidates=scores,selected_angle_radians=best['angle_radians'],pole_m=list(pole.location)))
    # Rigid hoof core and a declared blend collar; exact sole vertices must remain rigid.
    ids=patches[name+'.vertices'];low=np.array(sole_rows[name]['bounds_m'][0]);high=np.array(sole_rows[name]['bounds_m'][1])
    xy=(points[:,:2]>=low[:2]-foot['hoof_margin_m']).all(1)&(points[:,:2]<=high[:2]+foot['hoof_margin_m']).all(1)
    mask=(owners==foot['owner_id'])&xy&(points[:,2]<=foot['rigid_hoof_maximum_z_m']+foot['blend_band_m'])
    mask[ids]=True
    modified=np.flatnonzero(mask);foot_group=obj.vertex_groups[chain[-1]]
    for index in modified:
        i=int(index);blend=float(np.clip((foot['rigid_hoof_maximum_z_m']+foot['blend_band_m']-points[i,2])/foot['blend_band_m'],0,1))
        xy_mode=cfg.get('hoof_xy_blend','legacy-hard-margin')
        assert xy_mode in ['legacy-hard-margin','continuous-margin']
        if xy_mode=='continuous-margin':
            # Fade inside the existing margin. The old binary rectangle
            # introduced a full rigid-bone jump at its outer boundary.
            outside=float(np.maximum(np.maximum(low[:2]-points[i,:2],points[i,:2]-high[:2]),0).max())
            assert foot['hoof_margin_m']>0
            blend*=float(np.clip(1-outside/foot['hoof_margin_m'],0,1))
        if i in ids:blend=1.
        old={obj.vertex_groups[g.group].name:g.weight for g in obj.data.vertices[i].groups if obj.vertex_groups[g.group].name in primary_names}
        for bone in primary_names:
            group=obj.vertex_groups[bone];value=(1-blend)*old.get(bone,0)+(blend if bone==chain[-1] else 0)
            if value>0:group.add([i],value,'REPLACE')
            elif bone in old:group.remove([i])
    weight_rows.append(dict(id=name,modified_vertices=len(modified),rigid_sole_vertices=len(ids)))
    controls.append((foot,target,pole,ik,rotation,endpoint))
    ik.influence=rotation.influence=0
root=rig.pose.bones[cfg['body_root_bone']]
delta=rig.data.bones[root.name].matrix_local.to_3x3().inverted()@Vector(cfg['body_translation_m'])
scene.frame_start=1;scene.frame_end=cfg['frame_end'];scene.render.fps=cfg['fps']
for frame,standing in [(1,False),(cfg['standing_frame'],True),(cfg['frame_end'],True)]:
    root.location=delta if standing else Vector((0,0,0));root.keyframe_insert('location',frame=frame)
    for foot,target,pole,ik,rotation,endpoint in controls:
        shift=Vector((0,0,-sole_rows[foot['id']]['minimum_height_m'])) if standing else Vector((0,0,0))
        target.location=endpoint+shift;target.keyframe_insert('location',frame=frame)
        ik.influence=rotation.influence=float(standing);ik.keyframe_insert('influence',frame=frame);rotation.keyframe_insert('influence',frame=frame)
out.mkdir(parents=True);(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
scene.frame_set(cfg['standing_frame']);candidate=out/'Mount-StandingPilot.blend';bpy.ops.wm.save_as_mainfile(filepath=str(candidate),relative_remap=True)
bpy.ops.wm.open_mainfile(filepath=str(candidate));scene=bpy.context.scene;obj=bpy.data.objects[cfg['object']];rig=bpy.data.objects[cfg['rig_object']]
first=np.unique(canonical,return_index=True)[1];reference=first[canonical];poses=[]
for frame in [1,cfg['standing_frame'],cfg['frame_end']]:
    scene.frame_set(frame);evaluated=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());values=np.empty(len(points)*3,np.float32);evaluated.data.vertices.foreach_get('co',values)
    world=np.column_stack((values.reshape(-1,3),np.ones(len(points))))@np.array(obj.matrix_world).T;world=world[:,:3]
    rows=[]
    for foot in cfg['feet']:
        ids=patches[foot['id']+'.vertices'];height=world[ids,2]-cfg['ground_height_m'];target=bpy.data.objects['FOOT TARGET | '+foot['id']]
        tip=rig.pose.bones[foot['chain'][-2]].tail
        rows.append(dict(id=foot['id'],minimum_sole_height_m=float(height.min()),lower_fifth_height_m=float(np.quantile(height,.05)),
            near_ground_vertices=int((abs(height)<=cfg['limits']['sole_contact_height_m']).sum()),
            endpoint_error_m=(tip-target.location).length))
    stretch=max(abs(rig.pose.bones[n].length/rig.data.bones[n].length-1) for n in primary_names)
    poses.append(dict(frame=frame,feet=rows,maximum_bone_length_relative_change=stretch,
        coincident_seam_error_m=float(np.linalg.norm(world-world[reference],axis=1).max()),
        overall_minimum_height_m=float(world[:,2].min()),maximum_rest_displacement_m=float(np.linalg.norm(world-points,axis=1).max())))
    scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=cfg['render_samples'];scene.cycles.use_denoising=False
    scene.render.image_settings.file_format='PNG';scene.render.filepath=str(out/('pose-'+str(frame)+'.png'));bpy.ops.render.render(write_still=True)
standing=poses[1];limits=cfg['limits']
checks=dict(neutral_rest=poses[0]['maximum_rest_displacement_m']<=limits['neutral_rest_error_m'],
    sole_contacts=all(abs(r['minimum_sole_height_m'])<=limits['sole_contact_height_m'] and r['near_ground_vertices']>0 for r in standing['feet']),
    foot_targets=all(r['endpoint_error_m']<=limits['foot_target_error_m'] for r in standing['feet']),
    no_bone_stretch=standing['maximum_bone_length_relative_change']<=limits['bone_length_relative_change'],
    seams=standing['coincident_seam_error_m']<=limits['seam_error_m'])
report=dict(schema_version=1,status='passed-scoped-standing-pilot-needs-review' if all(checks.values()) else 'needs-review-standing-pilot',cpu_only=True,
    input_sha256=cfg['input_sha256'],tool_sha256=sha(__file__),profile_sha256=sha(profile),candidate_sha256=sha(candidate),checks=checks,poses=poses,
    pole_calibration=calibration,hoof_weight_changes=weight_rows,
    limitations=['Source-bound sole proposals and standing pose still require visual/anatomical review.',
      'Overall mesh minimum includes hair/tack; no full-mount ground/triangle/self collision acceptance.',
      'Rest-to-stand transition is not a planted gait. No idle/walk/run/turn, rider, skeletal export or engine acceptance.'])
(out/'standing-report.json').write_text(json.dumps(report,indent=2))
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
print(json.dumps(dict(status=report['status'],checks=checks,standing=standing)),flush=True)
