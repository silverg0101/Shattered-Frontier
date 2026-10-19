"""Author a checked, fixed-foot idle diagnostic from a frozen standing IK source."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
import bpy
import numpy as np
from mathutils import Vector, Quaternion

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda:stream.read(8*1024*1024),b''):h.update(block)
    return h.hexdigest()

p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);profile=Path(a.profile).resolve();out=Path(a.output).resolve();cfg=json.loads(profile.read_text())
assert cfg['schema_version']==1 and cfg['units']=='metres-seconds-radians' and not out.exists()
assert 24<=cfg['frames_per_cycle']<=240 and 1<=cfg['cycles']<=4 and 1<=cfg['fps']<=120
assert 0<=cfg['body_bob_m']<=.02
for row in cfg['rotations']:assert abs(row['amplitude_radians'])<=.1 and abs(Vector(row['axis']).length-1)<1e-6
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
contact=json.loads(Path(cfg['source_contact_report']).read_text());assert all(contact['primary_contact_checks'].values())
assert contact['input_sha256'][cfg['source']]==cfg['input_sha256'][cfg['source']]
with np.load(cfg['semantic_selection']) as ar:canonical=ar['canonical_node']
patches=np.load(cfg['sole_patches']);bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()))
scene=bpy.context.scene;scene.frame_set(cfg['standing_frame']);rig=bpy.data.objects[cfg['rig_object']];obj=bpy.data.objects[cfg['object']]
# Freeze standing controller values before removing inherited transition curves.
controls=[bpy.data.objects[n] for n in cfg['control_objects']]
for control in controls:
    loc=control.location.copy();rot=control.rotation_quaternion.copy();control.animation_data_clear();control.location=loc;control.rotation_quaternion=rot
bases={pb.name:(pb.location.copy(),pb.rotation_quaternion.copy()) for pb in rig.pose.bones};rig.animation_data_clear()
root=rig.pose.bones[cfg['body_root_bone']];axis=rig.data.bones[root.name].matrix_local.to_3x3().inverted()@Vector((0,0,1))
end=cfg['frames_per_cycle']*cfg['cycles']+1;scene.frame_start=1;scene.frame_end=end;scene.render.fps=cfg['fps']
for frame in range(1,end+1):
    phase=2*math.pi*(frame-1)/cfg['frames_per_cycle']
    root.location=bases[root.name][0]+axis*(cfg['body_bob_m']*math.sin(phase));root.keyframe_insert('location',frame=frame)
    for row in cfg['rotations']:
        pb=rig.pose.bones[row['bone']];pb.rotation_mode='QUATERNION';pb.rotation_quaternion=bases[pb.name][1]@Quaternion(Vector(row['axis']),row['amplitude_radians']*math.sin(phase*row['frequency']+row['phase_radians']))
        pb.keyframe_insert('rotation_quaternion',frame=frame)
for layer in rig.animation_data.action.layers:
    for strip in layer.strips:
        for bag in strip.channelbags:
            for curve in bag.fcurves:
                for key in curve.keyframe_points:key.interpolation='LINEAR'
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=cfg['render_samples'];scene.cycles.use_denoising=False
out.mkdir(parents=True);(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
scene.frame_set(1);candidate=out/'Mount-IdlePilot.blend';bpy.ops.wm.save_as_mainfile(filepath=str(candidate),relative_remap=True);bpy.ops.wm.open_mainfile(filepath=str(candidate))
scene=bpy.context.scene;obj=bpy.data.objects[cfg['object']];rig=bpy.data.objects[cfg['rig_object']]
first=np.unique(canonical,return_index=True)[1];representative=first[canonical];reference=None;anchors={};max_contact=max_slip=max_seam=max_bone=loop=0.;poses=[]
for frame in range(1,end+1):
    scene.frame_set(frame);ev=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());v=np.empty(len(canonical)*3,np.float32);ev.data.vertices.foreach_get('co',v)
    world=(np.column_stack((v.reshape(-1,3),np.ones(len(canonical))))@np.array(obj.matrix_world).T)[:,:3]
    if reference is None:reference=world.copy()
    if (frame-1)%cfg['frames_per_cycle']==0:loop=max(loop,float(np.linalg.norm(world-reference,axis=1).max()))
    max_seam=max(max_seam,float(np.linalg.norm(world-world[representative],axis=1).max()))
    max_bone=max(max_bone,max(abs(pb.length/rig.data.bones[pb.name].length-1) for pb in rig.pose.bones))
    for foot in cfg['feet']:
        ids=patches[foot+'.vertices'];max_contact=max(max_contact,abs(float(world[ids,2].min()-cfg['ground_height_m'])))
        if foot not in anchors:anchors[foot]=world[ids].copy()
        max_slip=max(max_slip,float(np.linalg.norm(world[ids]-anchors[foot],axis=1).max()))
    poses.append(dict(frame=frame,full_mount_minimum_height_m=float(world[:,2].min()-cfg['ground_height_m'])))
    if frame in cfg['render_frames']:
        scene.render.image_settings.file_format='PNG';scene.render.filepath=str(out/f'frame-{frame}.png');bpy.ops.render.render(write_still=True)
lim=cfg['limits'];checks=dict(contact=max_contact<=lim['contact_m'],slip=max_slip<=lim['slip_m'],seams=max_seam<=lim['seam_m'],bone_lengths=max_bone<=lim['bone_length_relative'],cycle=loop<=lim['loop_m'])
report=dict(schema_version=1,status='passed-scoped-idle-needs-review' if all(checks.values()) else 'needs-review-idle',frames=end,cpu_only=True,input_sha256=cfg['input_sha256'],candidate_sha256=sha(candidate),profile_sha256=sha(profile),tool_sha256=sha(__file__),checks=checks,maximum_contact_error_m=max_contact,maximum_slip_m=max_slip,maximum_seam_error_m=max_seam,maximum_bone_length_relative_change=max_bone,cycle_error_m=loop,poses=poses,limitations=['Authored idle diagnostic, no capture or anatomical acceptance.','Full-source strain, materials, semantic attachments and secondary collisions remain separate gates.'])
(out/'idle-report.json').write_text(json.dumps(report,indent=2))
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
print(json.dumps({'status':report['status'],'checks':checks}),flush=True)
