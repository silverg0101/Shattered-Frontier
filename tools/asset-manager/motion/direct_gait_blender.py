"""Bake direct gait controls onto the existing skin, using joint heads and world matrices."""
import hashlib
import json
import math
import sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Matrix, Vector
sys.path.insert(0,str(Path(__file__).resolve().parent))
from direct_gait_math import solve, foot_path, idle_motion, run_motion
from native_policy import DEFAULTS


def main():
    source, planfile, configfile, clip, output = sys.argv[sys.argv.index('--')+1:]
    receipt=json.loads(Path(planfile).read_text())
    if hashlib.sha256(Path(source).read_bytes()).hexdigest()!=receipt['model_sha256']:
        raise ValueError('gait plan belongs to a different rig')
    plan=receipt['plan'];config={**DEFAULTS,**json.loads(Path(configfile).read_text())}
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(Path(source).resolve()))
    arms=[o for o in bpy.context.scene.objects if o.type=='ARMATURE']
    if len(arms)!=1:raise ValueError('expected exactly one armature')
    arm=arms[0];arm.animation_data_clear()
    for bone in arm.pose.bones:bone.rotation_mode='QUATERNION'
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH' and any(m.type=='ARMATURE' and m.object==arm for m in o.modifiers)]
    rest={b.name:arm.matrix_world @ b.matrix_local for b in arm.data.bones}
    pelvis=plan['pelvis']
    forward=rest[plan['head']].translation-rest[pelvis].translation;forward.z=0
    if forward.length<1e-4:raise ValueError('no measurable forward direction')
    forward.normalize();axis=forward.cross(Vector((0,0,1))).normalized()
    points=[o.matrix_world @ v.co for o in meshes for v in o.data.vertices]
    height=max(p.z for p in points)-min(p.z for p in points)
    if height<=0:raise ValueError('invalid mesh bounds')
    minimum=Vector(tuple(min(p[i] for p in points) for i in range(3)))
    wing_pose={}
    if plan['body']=='bird' and len(plan.get('wings',[]))!=2:
        raise ValueError('bird ground animation requires a generated folded-wing plan')
    for wing in plan.get('wings',[]):
        chain=wing['joints']
        position=rest[chain[0]].translation.copy()
        for i,name in enumerate(chain):
            if i<len(chain)-1:
                reference=rest[chain[i+1]].translation-rest[name].translation
                target=minimum+Vector(wing['targets'][i])*height
                direction=target-position
            else:
                # Imported glTF bone tails are heuristics. Use the last real joint-head segment.
                reference=rest[name].translation-rest[chain[i-1]].translation
                direction=Vector(wing['tip_direction'])
            if reference.length<1e-6 or direction.length<1e-6:
                raise ValueError('folded-wing plan has a degenerate segment')
            matrix=reference.rotation_difference(direction).to_matrix().to_4x4() @ rest[name]
            matrix.translation=position
            wing_pose[name]=matrix
            position=position+direction.normalized()*reference.length
    legs=[]
    for entry in plan['legs']:
        names=entry['joints']
        if any(n not in rest for n in names) or any(arm.data.bones[b].parent.name!=a for a,b in zip(names,names[1:])):
            raise ValueError('gait plan is not a direct joint chain')
        heads=np.array([rest[n].translation[:] for n in names])
        foot=arm.data.bones[names[-1]]
        foot_names={foot.name,*[b.name for b in foot.children_recursive]}
        sole=[]
        for obj in meshes:
            groups={g.index for g in obj.vertex_groups if g.name in foot_names}
            sole.extend((obj.matrix_world @ v.co).z for v in obj.data.vertices if sum(g.weight for g in v.groups if g.group in groups)>.5)
        if not sole:raise ValueError('foot has no dominant skin influence')
        floor=min(sole)
        if abs(floor)>height*.10:raise ValueError('foot rest position is too far from the floor')
        target=heads[-1].copy();target[2]-=floor
        legs.append(dict(names=names,heads=heads,vectors=np.diff(heads,axis=0),target=target,role=entry['role']))
    period=config['direct_idle_period_s'] if clip=='Idle' else config['direct_run_period_s'] if clip=='Run' else config['direct_gait_period_s']
    fps=30;count=max(12,round(period*fps));scene=bpy.context.scene
    scene.render.fps=fps;scene.frame_start=1;scene.frame_end=count+1
    phases=({'L':0,'R':.5} if plan['body']=='bird' else
            {'HL':0,'FL':.25,'HR':.5,'FR':.75} if clip!='Run' else {'HL':0,'FR':0,'HR':.5,'FL':.5})
    half_stride=height*config['direct_run_stride_pct' if clip=='Run' else 'direct_gait_stride_pct']/200
    lift=height*config['direct_run_lift_pct' if clip=='Run' else 'direct_gait_lift_pct']/100
    stance=config['direct_run_stance_fraction'] if clip=='Run' else .62
    frames=[];max_error=0
    # Reach is checked before baking. A bounded stride reduction is reported, never hidden.
    failures=[]
    for scale in (1.,.75,.5):
        frames=[];max_error=0
        try:
            for k in range(count+1):
                t=(k%count)/count
                flight=0.
                body_rotation=Matrix.Identity(3)
                if clip=='Run':
                    controls=run_motion(t,config)
                    flight=height*controls['flight_pct']/100
                    bob=height*controls['bob_pct']/100
                    body_rotation=Matrix.Rotation(math.radians(controls['body_pitch_deg']),3,axis)
                else:
                    bob=height*.003*(math.cos(4*math.pi*t)-1) if clip!='Idle' else height*idle_motion(t,config)['settle_pct']/100
                offset=np.array([0.,0.,bob])
                if clip=='Idle':offset+=np.array(forward)*height*idle_motion(t,config)['shift_pct']/100
                states=[]
                for leg in legs:
                    # Reducing reach must not silently turn a run into a low-clearance shuffle.
                    sweep,up=(0.,0.) if clip=='Idle' else foot_path(t-phases[leg['role']],stance,half_stride*scale,lift*(1 if clip=='Run' else scale))
                    target=leg['target']+np.array(forward)*sweep+np.array([0,0,up+flight])
                    origin=leg['heads'][0]+offset
                    if clip=='Run':
                        pivot=rest[pelvis].translation
                        origin=np.array(pivot+body_rotation @ (Vector(leg['heads'][0])-pivot))+offset
                    try:
                        angles,positions,error=solve(origin,leg['vectors'],axis,target)
                    except ValueError as error:
                        raise ValueError(f"{leg['role']} phase {t:.3f}, stride scale {scale}: {error}") from error
                    max_error=max(max_error,error)
                    states.append((angles,positions))
                frames.append((offset,states))
            break
        except ValueError as error:
            failures.append(str(error))
    else:raise ValueError('direct gait cannot reach its foot targets: '+'; '.join(failures))
    inverse=arm.matrix_world.inverted()
    for k,(offset,states) in enumerate(frames,1):
        scene.frame_set(k)
        for bone in arm.pose.bones:bone.matrix_basis=Matrix.Identity(4)
        root_matrix=rest[pelvis].copy()
        if clip=='Run':
            controls=run_motion((k-1)%count/count,config)
            root_matrix=Matrix.Rotation(math.radians(controls['body_pitch_deg']),4,axis) @ root_matrix
            root_matrix.translation=rest[pelvis].translation
        root_matrix.translation+=Vector(offset)
        arm.pose.bones[pelvis].matrix=inverse @ root_matrix
        bpy.context.view_layer.update()
        if clip in ('Idle','Run'):
            controls=(idle_motion if clip=='Idle' else run_motion)((k-1)%count/count,config)
            head=arm.pose.bones[plan['head']]
            neck=head.parent
            # Never tilt an ancestor that also carries walking legs; those remain controlled by planted-foot IK.
            leg_ancestors=set()
            for leg in legs:
                parent=arm.data.bones[leg['names'][0]].parent
                while parent:
                    leg_ancestors.add(parent.name);parent=parent.parent
            if neck and neck.name not in leg_ancestors:
                world=arm.matrix_world @ neck.matrix
                pivot=world.translation.copy()
                world=Matrix.Rotation(math.radians(controls['neck_pitch_deg']),4,axis) @ world
                world.translation=pivot
                neck.matrix=inverse @ world
                bpy.context.view_layer.update()
            world=arm.matrix_world @ head.matrix
            pivot=world.translation.copy()
            world=Matrix.Rotation(math.radians(controls.get('head_yaw_deg',0)),4,'Z') @ Matrix.Rotation(math.radians(controls['head_pitch_deg']),4,axis) @ world
            world.translation=pivot
            head.matrix=inverse @ world
            bpy.context.view_layer.update()
        wing_parents={wing['joints'][0]:arm.data.bones[wing['joints'][0]].parent.name for wing in plan.get('wings',[])}
        wing_delta={}
        for wing in plan.get('wings',[]):
            parent=wing_parents[wing['joints'][0]]
            delta=(arm.matrix_world @ arm.pose.bones[parent].matrix) @ rest[parent].inverted()
            for name in wing['joints']:wing_delta[name]=delta
        for name,matrix in wing_pose.items():
            if clip in ('Idle','Run'):
                folded=wing_delta[name] @ matrix
            else:
                folded=matrix.copy();folded.translation+=Vector(offset)
            arm.pose.bones[name].matrix=inverse @ folded
            bpy.context.view_layer.update()
        for leg,(angles,positions) in zip(legs,states):
            total=0.
            for i,name in enumerate(leg['names']):
                if i<len(angles):total+=float(angles[i])
                # Hold the foot's WORLD orientation, irrespective of proximal flexion.
                rotation=Matrix.Rotation(total,4,axis) if i<len(angles) else Matrix.Identity(4)
                world=rotation @ rest[name]
                world.translation=Vector(positions[i])
                arm.pose.bones[name].matrix=inverse @ world
                bpy.context.view_layer.update()
        for bone in arm.pose.bones:
            bone.rotation_mode='QUATERNION'
            bone.keyframe_insert('location',frame=k,group=bone.name)
            bone.keyframe_insert('rotation_quaternion',frame=k,group=bone.name)
            bone.keyframe_insert('scale',frame=k,group=bone.name)
    arm.animation_data.action.name=clip
    bpy.ops.object.select_all(action='DESELECT')
    for obj in [arm,*meshes]:obj.select_set(True)
    bpy.context.view_layer.objects.active=arm
    bpy.ops.export_scene.gltf(filepath=str(Path(output).resolve()),export_format='GLB',use_selection=True,
        export_animations=True,export_force_sampling=True,export_frame_range=True)
    Path(output+'.json').write_text(json.dumps({'source_sha256':receipt['model_sha256'],'clip':clip,
        'controller':'direct-gait-v3','body':plan['body'],'frames':count+1,'fps':fps,'in_place':True,
        'stride_scale':scale,'max_foot_target_error_m':max_error,'config':config,'plan':plan},indent=2))


if __name__=='__main__':main()
