"""Blender: bounded multi-view rest/pose and motion evidence on the actual skinned meshes."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
import numpy as np
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view

sys.path.insert(0, str(Path(__file__).resolve().parent))
from render_views import load


def evaluated_points(meshes):
    dg=bpy.context.evaluated_depsgraph_get();out=[]
    for obj in meshes:
        ev=obj.evaluated_get(dg);me=ev.to_mesh()
        co=np.array([ev.matrix_world@v.co for v in me.vertices],float)
        ev.to_mesh_clear();out.append(co)
    return np.concatenate(out)


def main():
    ap=argparse.ArgumentParser();ap.add_argument('model');ap.add_argument('folder');ap.add_argument('--mode',choices=['rig','motion'],default='rig')
    ap.add_argument('--in-place-out');a=ap.parse_args(sys.argv[sys.argv.index('--')+1:])
    folder=Path(a.folder);folder.mkdir(parents=True,exist_ok=True)
    arm=load(str(Path(a.model).resolve()),fps=24)
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH' and any(m.type=='ARMATURE' and m.object==arm for m in o.modifiers)]
    if not meshes:raise ValueError('no skinned creature meshes')
    # Display widgets such as glTF's Icosphere never contribute to evidence or bounds.
    for o in bpy.context.scene.objects:
        if o.type=='MESH':o.hide_render=o not in meshes
    scene=bpy.context.scene
    if a.mode=='motion':
        actions=[o.animation_data.action for o in [arm,*meshes] if o.animation_data and o.animation_data.action]
        if not actions:raise ValueError('motion review requires a keyed animation')
        scene.frame_start=math.floor(min(act.frame_range[0] for act in actions))
        scene.frame_end=math.ceil(max(act.frame_range[1] for act in actions))
        if scene.frame_end<=scene.frame_start:raise ValueError('motion review requires more than one frame')
    low=[b for b in arm.pose.bones if b.name.lower() in ('hips','pelvis','mixamorig:hips')]
    root=low[0] if low else next(b for b in arm.pose.bones if b.bone.use_deform)
    scene.frame_set(scene.frame_start);bpy.context.view_layer.update()
    rest=evaluated_points(meshes);lo,hi=rest.min(0),rest.max(0);h=float(hi[2]-lo[2])
    if not np.isfinite(rest).all() or h<=1e-6:raise ValueError('invalid creature bounds')
    bone_list=list(arm.pose.bones)
    skin_weight={b.name:0.0 for b in bone_list}
    for obj in meshes:
        names={g.index:g.name for g in obj.vertex_groups}
        for v in obj.data.vertices:
            for g in v.groups:
                if names[g.group] in skin_weight:skin_weight[names[g.group]]+=g.weight
    meta={'mode':a.mode,'model_sha256':hashlib.sha256(Path(a.model).read_bytes()).hexdigest(),
          'height':h,'bounds':{'min':lo.tolist(),'max':hi.tolist()},'root':root.name,
          'bones':[{'id':i,'name':b.name,'parent':b.parent.name if b.parent else None,
                    'deform':skin_weight[b.name]>1e-6,'skin_weight_sum':skin_weight[b.name],'head':list(arm.matrix_world@b.head),
                    'tail':list(arm.matrix_world@b.tail)} for i,b in enumerate(bone_list)],'tiles':[],'metrics':{}}
    frames=list(range(scene.frame_start,scene.frame_end+1)) if a.mode=='motion' else [scene.frame_start]
    if len(frames)>1200:raise ValueError('review clips are limited to 1200 frames')
    roots=[];joints=[];floors=[];extents=[]
    first_rotations=None
    angular_travel=np.zeros(len(bone_list))
    for f in frames:
        scene.frame_set(f);bpy.context.view_layer.update()
        rotations=[(arm.matrix_world@b.matrix).to_quaternion() for b in bone_list]
        if first_rotations is None:first_rotations=rotations
        angular_travel=np.maximum(angular_travel,[math.degrees(2*math.acos(min(1.,abs(a.dot(b))))) for a,b in zip(first_rotations,rotations)])
        verts=evaluated_points(meshes)
        if not np.isfinite(verts).all():raise ValueError('nonfinite deformed vertices')
        roots.append(np.array(arm.matrix_world@root.head));joints.append(np.array([arm.matrix_world@b.head for b in bone_list]))
        floors.append(float(verts[:,2].min()));extents.append(float(np.linalg.norm(np.ptp(verts,axis=0))))
    roots=np.array(roots);joints=np.array(joints)-roots[:,None,:]
    # Bone-head displacement alone misses rotations about a joint (e.g. an idle head turn).
    # This is evidence for the reviewer, not another acceptance threshold.
    meta['joint_angular_travel_deg_from_first_pose']={b.name:round(float(angle),3) for b,angle in zip(bone_list,angular_travel)}
    meta['metrics']={'frames':len(frames),'root_travel_pct':float(np.linalg.norm(roots[:,:2]-roots[0,:2],axis=1).max()/h*100),
        'root_bob_pct':float(np.ptp(roots[:,2])/h*100),
        'floor_penetration_pct':float(max(0,lo[2]-min(floors))/h*100),
        'joint_step_pct':float(np.linalg.norm(np.diff(joints,axis=0),axis=2).max(initial=0)/h*100),
        'extent_ratio':max(extents)/max(extents[0],1e-8)}
    if a.in_place_out:
        if a.mode!='motion':raise ValueError('in-place repair requires motion')
        top=root
        while top.parent:top=top.parent
        if top==root:raise ValueError('in-place repair needs a separate ground root control')
        # Move the control with the pelvis first (preserving the original deformation),
        # then remove its planar displacement. The root-to-hips connector stays local.
        scene.frame_set(frames[0]);p0=root.head.copy()
        saved=[]
        for f in frames:
            scene.frame_set(f);saved.append((root.matrix.copy(),top.matrix.copy(),root.head.copy()-p0))
        for f,(hip,ground,delta) in zip(frames,saved):
            scene.frame_set(f)
            moved=ground.copy();moved.translation+=delta;top.matrix=moved
            bpy.context.view_layer.update();root.matrix=hip;bpy.context.view_layer.update()
            anchored=moved.copy();anchored.translation.x-=delta.x;anchored.translation.y-=delta.y;top.matrix=anchored
            for b in (top,root):
                b.rotation_mode='QUATERNION';b.keyframe_insert('location',frame=f);b.keyframe_insert('rotation_quaternion',frame=f)
        bpy.ops.object.select_all(action='DESELECT')
        for o in [arm,*meshes]:o.select_set(True)
        bpy.context.view_layer.objects.active=arm
        bpy.ops.export_scene.gltf(filepath=str(Path(a.in_place_out).resolve()),export_format='GLB',use_selection=True,
            export_animations=True,export_animation_mode='ACTIVE_ACTIONS',export_force_sampling=True)
        return
    # Fresh CPU rendering avoids accidentally selecting the worker's reserved GPU 0.
    scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=4;scene.cycles.use_denoising=False
    scene.render.resolution_x=384;scene.render.resolution_y=384;scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG';scene.render.film_transparent=False
    scene.world=bpy.data.worlds.new('QA world');scene.world.use_nodes=True
    scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.65,.65,.65,1)
    scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.8
    center=Vector((lo+hi)/2);radius=float(np.linalg.norm(hi-lo))
    light_data=bpy.data.lights.new('QA light','AREA');light_data.energy=400;light_data.size=radius*2
    light=bpy.data.objects.new('QA light',light_data);scene.collection.objects.link(light);light.location=center+Vector((radius,-radius,radius*2))
    light.rotation_euler=(center-light.location).to_track_quat('-Z','Y').to_euler()
    cam_data=bpy.data.cameras.new('QA camera');cam_data.type='ORTHO';cam_data.ortho_scale=max(float(np.ptp(rest,axis=0).max()),h)*1.25
    cam=bpy.data.objects.new('QA camera',cam_data);scene.collection.objects.link(cam);scene.camera=cam
    views=[('from -Y (screen X/Z)',(0,-1,0)),('from -X (screen Y/Z)',(-1,0,0)),('from +Z (screen X/Y)',(0,0,1))]
    cases=[(scene.frame_start,'rest',None,None)]
    if a.mode=='motion':
        cases=[(int(f),f'frame {int(f)}',None,None) for f in np.linspace(frames[0],frames[-1],8)]
        views=views[:2]
    else:
        # Isolate both hips and both knee/hock hinges. A hip-only test can miss
        # buckling and skin bleed at the lower joint entirely.
        chosen=set()
        for prefix,axis in [('Thigh.R','Z'),('Shin.R','Z'),('Thigh.L','Z'),('Shin.L','Z'),
                            ('WingUpper.R','X'),('WingUpper.L','X'),('Tail1','X'),('Neck1','X')]:
            b=next((b for b in bone_list if b.name.lower()==prefix.lower()),None)
            if b:
                cases.append((scene.frame_start,f'pose {b.name} local {axis} 35deg',b.name,axis))
                chosen.add(b.name)
        if len(cases)==1:
            candidates=[b for b in bone_list if b.parent and b.parent.name==root.name and len(b.children_recursive)>1]
            for b in candidates[:6]:cases.append((scene.frame_start,'pose '+b.name,b.name,'Z'))
    for f,label,stress,axis in cases:
        scene.frame_set(f)
        saved_pose={b.name:b.matrix_basis.copy() for b in bone_list}
        if stress:
            b=arm.pose.bones[stress];b.rotation_mode='XYZ'
            if axis=='Z':b.rotation_euler.z+=math.radians(35)
            else:b.rotation_euler.x+=math.radians(35)
        bpy.context.view_layer.update()
        for view,direction in views:
            cam.location=center+Vector(direction)*radius*3
            cam.rotation_euler=(center-cam.location).to_track_quat('-Z','Y').to_euler()
            bpy.context.view_layer.update()
            if not direction[2]:
                p=world_to_camera_view(scene,cam,center);up=world_to_camera_view(scene,cam,center+Vector((0,0,h/4)))
                if abs(up.x-p.x)>1e-4 or up.y<=p.y:raise ValueError('diagnostic camera failed its vertical-axis check')
            index=len(meta['tiles']);name=f'view-{index:02d}.png';scene.render.filepath=str((folder/name).resolve())
            bpy.ops.render.render(write_still=True)
            project=lambda p:world_to_camera_view(scene,cam,arm.matrix_world@p)
            bones=[]
            for i,b in enumerate(bone_list):
                p,q=project(b.head),project(b.tail)
                bones.append({'id':i,'deform':skin_weight[b.name]>1e-6,'head':[p.x,1-p.y],'tail':[q.x,1-q.y]})
            meta['tiles'].append({'image':name,'label':label+' | '+view,'bones':bones})
        for b in bone_list:b.matrix_basis=saved_pose[b.name]
    (folder/'evidence.json').write_text(json.dumps(meta,indent=2))


if __name__=='__main__':main()
