"""CPU procedural four-beat walk with world-planted foot targets and root motion.

Diagnostic gait only; primary shape and secondary/runtime gates remain separate.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
import bpy
from mathutils import Vector,Quaternion
import numpy as np

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda:stream.read(8*1024*1024),b''):h.update(chunk)
    return h.hexdigest()

p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);profile,out=Path(a.profile).resolve(),Path(a.output).resolve();cfg=json.loads(profile.read_text())
assert cfg['schema_version']==1 and cfg['units']=='metres-seconds-radians' and not out.exists()
assert 1<=cfg['cycles']<=4 and 12<=cfg['frames_per_cycle']<=120 and 1<=cfg['fps']<=120
assert .05<=cfg['stride_m']<=2 and .5<=cfg['stance_fraction']<=.9 and .01<=cfg['lift_m']<=.3
turn=cfg.get('turn_per_cycle_radians',0.);assert abs(turn)<=.35
kind='turn' if turn else 'walk'
def root_pose(t):
    angle=turn*t
    position=Vector((0,-cfg['stride_m']*t,0)) if not turn else Vector((cfg['stride_m']/turn*(1-math.cos(angle)),-cfg['stride_m']/turn*math.sin(angle),0))
    return position,Quaternion((0,0,1),angle)
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
if cfg.get('source_contact_report'):
    contact=json.loads(Path(cfg['source_contact_report']).read_text())
    assert all(contact['primary_contact_checks'].values())
    assert contact['input_sha256'][cfg['source']]==cfg['input_sha256'][cfg['source']]
if cfg.get('source_solver_report'):
    solve=json.loads(Path(cfg['source_solver_report']).read_text())
    assert solve['solver_converged'] and solve['candidate_sha256']==cfg['input_sha256'][cfg['source']]
with np.load(cfg['semantic_selection']) as ar:canonical,labels=ar['canonical_node'],ar['labels']
patches=np.load(cfg['sole_patches']);bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()))
scene=bpy.context.scene;scene.frame_set(cfg['source_standing_frame']);rig=bpy.data.objects[cfg['rig_object']];obj=bpy.data.objects[cfg['object']]
rig.animation_data_clear()
for pb in rig.pose.bones:pb.location=(0,0,0);pb.rotation_mode='QUATERNION';pb.rotation_quaternion=(1,0,0,0)
assembly=bpy.data.objects.new('LOCOMOTION | authored root motion',None);scene.collection.objects.link(assembly)
for child in [obj,rig,scene.camera]+[o for o in scene.objects if o.type=='LIGHT' and not o.hide_render]:
    world=child.matrix_world.copy();child.parent=assembly;child.matrix_world=world
controls=[]
for foot in cfg['feet']:
    target=bpy.data.objects['FOOT TARGET | '+foot['id']];pole=bpy.data.objects['FOOT POLE | '+foot['id']]
    target.animation_data_clear();pole.animation_data_clear()
    base=target.location.copy();base.y=foot['nominal_ankle_y_m']
    controls.append((foot,target,pole,base,target.rotation_quaternion.copy(),pole.location.copy()))
    for name in foot['chain']:
        for c in rig.pose.bones[name].constraints:c.influence=1
root=rig.pose.bones[cfg['body_root_bone']];root_axis=rig.data.bones[root.name].matrix_local.to_3x3().inverted()@Vector((0,0,1))
end=cfg['cycles']*cfg['frames_per_cycle']+1;scene.frame_start=1;scene.frame_end=end;scene.render.fps=cfg['fps'];plan=[]
for frame in range(1,end+1):
    cycle_time=(frame-1)/cfg['frames_per_cycle'];forward=-cfg['stride_m']*cycle_time
    assembly.location=(0,forward,cfg['assembly_base_z_m']);assembly.keyframe_insert('location',frame=frame)
    position,heading=root_pose(cycle_time)
    assembly.location=position+Vector((0,0,cfg['assembly_base_z_m']));assembly.keyframe_insert('location',frame=frame)
    assembly.rotation_mode='QUATERNION';assembly.rotation_quaternion=heading;assembly.keyframe_insert('rotation_quaternion',frame=frame)
    bob=cfg['body_bob_m']*math.sin(4*math.pi*cycle_time);root.location=root_axis*bob;root.keyframe_insert('location',frame=frame)
    foot_rows=[]
    for foot,target,pole,base,rotation,pole_base in controls:
        phase_time=cycle_time+foot['phase_offset'];cycle=math.floor(phase_time+1e-10);phase=phase_time-cycle
        stance=phase<cfg['stance_fraction'];u=0 if stance else (phase-cfg['stance_fraction'])/(1-cfg['stance_fraction'])
        advance=0 if stance else u*u*u*(10-15*u+6*u*u)
        lift=0 if stance else cfg['lift_m']*math.sin(math.pi*u)**2
        y=base.y-cfg['stride_m']*(cycle+advance)+cfg['stride_m']*foot['phase_offset']-cfg['stride_m']*cfg['stance_fraction']/2
        target.location=(base.x,y,base.z+lift);target.rotation_mode='QUATERNION'
        target.rotation_quaternion=Quaternion((1,0,0),cfg['swing_pitch_radians']*math.sin(math.pi*u)**2)@rotation
        if turn:
            # Mid-stance anchors stay fixed in world space; swing interpolates
            # between consecutive world anchors along the circular root path.
            t0=cycle-foot['phase_offset']+cfg['stance_fraction']/2
            p0,q0=root_pose(t0);p1,q1=root_pose(t0+1)
            offset=Vector((base.x,base.y,0))
            a0=p0+q0@offset;a1=p1+q1@offset
            target.location=a0.lerp(a1,advance)+Vector((0,0,base.z+lift))
            target.rotation_quaternion=q0.slerp(q1,advance)@Quaternion((1,0,0),cfg['swing_pitch_radians']*math.sin(math.pi*u)**2)@rotation
        target.keyframe_insert('location',frame=frame);target.keyframe_insert('rotation_quaternion',frame=frame)
        pole.location=pole_base+Vector((0,forward,bob));pole.keyframe_insert('location',frame=frame)
        if turn:
            pole.location=position+heading@pole_base+Vector((0,0,bob));pole.keyframe_insert('location',frame=frame)
        foot_rows.append(dict(id=foot['id'],phase=phase,cycle=cycle,stance=stance,target_m=list(target.location)))
    plan.append(dict(frame=frame,time_s=(frame-1)/cfg['fps'],root_m=list(assembly.location),feet=foot_rows))
# Per-frame target curves must interpolate linearly; do not leave Bezier overshoot.
for animated in [assembly,rig]+[o for row in controls for o in row[1:3]]:
    action=animated.animation_data.action if animated.animation_data else None
    if action:
        for layer in action.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    for curve in bag.fcurves:
                        for key in curve.keyframe_points:key.interpolation='LINEAR'
bpy.ops.mesh.primitive_plane_add(size=cfg['ground_size_m'],location=(0,0,cfg['ground_height_m']));floor=bpy.context.object;floor.name='DIAGNOSTIC | ground, not mount mesh'
mat=bpy.data.materials.new('Walk ground gray');mat.diffuse_color=(.3,.3,.3,1);floor.data.materials.append(mat)
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=cfg['render_samples'];scene.cycles.use_denoising=False
out.mkdir(parents=True);(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes());(out/'foot-plan.json').write_text(json.dumps(plan,indent=2))
scene.frame_set(1);candidate=out/('Mount-TurnPilot.blend' if turn else 'Mount-WalkPilot.blend');bpy.ops.wm.save_as_mainfile(filepath=str(candidate),relative_remap=True);bpy.ops.wm.open_mainfile(filepath=str(candidate))
scene=bpy.context.scene;obj=bpy.data.objects[cfg['object']];rig=bpy.data.objects[cfg['rig_object']]
first=np.unique(canonical,return_index=True)[1];representative=first[canonical];poses=[];anchors={};world_reference=None;loop_error=0.
max_seam=max_slip=max_target=max_bone_change=0.;min_swing=0.;max_contact=0.;minimum_contact_count=4
for record in plan:
    frame=record['frame'];scene.frame_set(frame);evaluated=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());v=np.empty(len(canonical)*3,np.float32);evaluated.data.vertices.foreach_get('co',v)
    world=np.column_stack((v.reshape(-1,3),np.ones(len(canonical))))@np.array(obj.matrix_world).T;world=world[:,:3]
    if frame==1:world_reference=world.copy()
    if frame==cfg['frames_per_cycle']+1:
        loop_error=float(np.linalg.norm(world-world_reference-np.array([0,-cfg['stride_m'],0]),axis=1).max())
        if turn:
            p1,q1=root_pose(1);rotation=np.array(q1.to_matrix())
            origin=np.array([0,0,cfg['assembly_base_z_m']])
            local=(world-np.array(p1)-origin)@rotation
            loop_error=float(np.linalg.norm(local-(world_reference-origin),axis=1).max())
    max_seam=max(max_seam,float(np.linalg.norm(world-world[representative],axis=1).max()))
    max_bone_change=max(max_bone_change,max(abs(b.length/rig.data.bones[b.name].length-1) for b in rig.pose.bones))
    contacts=0;foot_metrics=[]
    for foot in record['feet']:
        ids=patches[foot['id']+'.vertices'];height=float(world[ids,2].min()-cfg['ground_height_m'])
        target=bpy.data.objects['FOOT TARGET | '+foot['id']];chain=next(r['chain'] for r in cfg['feet'] if r['id']==foot['id'])
        tip=rig.matrix_world@rig.pose.bones[chain[-2]].tail;target_error=(tip-target.location).length;max_target=max(max_target,target_error)
        if foot['stance']:
            contacts+=1;max_contact=max(max_contact,abs(height));key=(foot['id'],foot['cycle'])
            if key not in anchors:anchors[key]=world[ids].copy()
            max_slip=max(max_slip,float(np.linalg.norm(world[ids]-anchors[key],axis=1).max()))
        else:min_swing=min(min_swing,height)
        foot_metrics.append(dict(id=foot['id'],stance=foot['stance'],minimum_sole_height_m=height,target_error_m=target_error))
    minimum_contact_count=min(minimum_contact_count,contacts)
    poses.append(dict(frame=frame,feet=foot_metrics,full_mount_minimum_height_m=float(world[:,2].min()-cfg['ground_height_m'])))
    if frame in cfg['render_frames']:
        scene.render.image_settings.file_format='PNG';scene.render.filepath=str(out/('frame-'+str(frame)+'.png'));bpy.ops.render.render(write_still=True)
limits=cfg['limits'];checks=dict(planted_sole_contacts=max_contact<=limits['contact_m'],planted_slip=max_slip<=limits['slip_m'],swing_clearance=min_swing>=-limits['penetration_m'],foot_targets=max_target<=limits['target_error_m'],bone_lengths=max_bone_change<=limits['bone_length_relative'],seams=max_seam<=limits['seam_m'],root_relative_cycle=loop_error<=limits['loop_m'],no_walk_suspension=minimum_contact_count>=2)
report=dict(schema_version=1,status='passed-scoped-walk-pilot-needs-review' if all(checks.values()) else 'needs-review-walk-pilot',cpu_only=True,input_sha256=cfg['input_sha256'],tool_sha256=sha(__file__),profile_sha256=sha(profile),candidate_sha256=sha(candidate),frames=end,checks=checks,maximum_contact_error_m=max_contact,maximum_planted_slip_m=max_slip,minimum_swing_height_m=min_swing,maximum_target_error_m=max_target,maximum_bone_length_relative_change=max_bone_change,maximum_seam_error_m=max_seam,root_relative_cycle_error_m=loop_error,minimum_planned_contacts=minimum_contact_count,root_travel_m=cfg['stride_m']*cfg['cycles'],poses=poses,
    limitations=['Procedural slow walk, not captured equine motion or anatomical/production acceptance.','Secondary hair/tack motion, unresolved ownership, whole-mount ground and self/triangle/swept collision remain separate gates.','Global assembly root carries unweighted islands without semantically binding them. No engine export/transition acceptance.'])
(out/'walk-report.json').write_text(json.dumps(report,indent=2))
if turn:
    report['status']='passed-scoped-turn-pilot-needs-review' if all(checks.values()) else 'needs-review-turn-pilot'
    report['turn_per_cycle_radians']=turn
    report['total_turn_radians']=turn*cfg['cycles']
    report['limitations'].append('Walking circular turn only; no pivot, run turn, transitions or runtime acceptance.')
    (out/'walk-report.json').unlink()
    (out/'turn-report.json').write_text(json.dumps(report,indent=2))
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
print(json.dumps({k:report[k] for k in ['status','checks','maximum_planted_slip_m','maximum_target_error_m','root_relative_cycle_error_m']}),flush=True)
