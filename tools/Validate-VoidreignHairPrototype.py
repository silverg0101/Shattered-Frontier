"""Fresh-source and animated GLB/FBX reimport checks for the scoped hair prototype."""
import argparse,json,sys,hashlib
from pathlib import Path
import bpy,numpy as np
from mathutils import Vector
p=argparse.ArgumentParser();p.add_argument('--folder',required=True);p.add_argument('--reference',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);folder=Path(a.folder).resolve();reference_path=Path(a.reference).resolve()
def matrix_mesh_digest(obj):
    vertices=np.array([v.co[:] for v in obj.data.vertices],dtype=np.float32)
    faces=np.array([tuple(p.vertices) for p in obj.data.polygons],dtype=np.int32)
    return hashlib.sha256(vertices.tobytes()+faces.tobytes()).hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(reference_path))
reference=next(o for o in bpy.data.collections['Voidreign | Generated asset'].all_objects if o.type=='MESH');digest=matrix_mesh_digest(reference)
bpy.ops.wm.open_mainfile(filepath=str(folder/'Voidreign-HairPrototype.blend'));scene=bpy.context.scene
obj=next(o for o in bpy.data.collections['Horse | original static context'].all_objects if o.type=='MESH')
assert matrix_mesh_digest(obj)==digest,'Original mesh topology/coordinates changed'
arm=bpy.data.objects['RIG | mane + tail only'];root=bpy.data.objects['MOTION HARNESS | rigid horse, not gait animation']
static_group=obj.vertex_groups['StaticBody'].index
static=[v.index for v in obj.data.vertices if any(g.group==static_group and g.weight>.99999 for g in v.groups)][::250]
sample_frames=[1,60,120,180,240,300,330];expected={};static_error=0;pose_error=0
trajectories={(r['frame'],r['section']):r['points'] for r in json.loads((folder/'simulation-trajectory.json').read_text())}
weights=[]
for vertex in obj.data.vertices:weights.append(sum(group.weight for group in vertex.groups if obj.vertex_groups[group.group].name=='StaticBody' or obj.vertex_groups[group.group].name.startswith(('Mane_','Tail_'))))
assert max(abs(w-1) for w in weights)<1e-5,'Weights not normalized'
for frame in sample_frames:
    scene.frame_set(frame);bpy.context.view_layer.update();graph=bpy.context.evaluated_depsgraph_get();evaluated=obj.evaluated_get(graph)
    expected[frame]={bone.name:list(arm.matrix_world@bone.head) for bone in arm.pose.bones if bone.name!='StaticBody'}
    for index in static:static_error=max(static_error,(evaluated.matrix_world@evaluated.data.vertices[index].co-obj.matrix_world@obj.data.vertices[index].co).length)
    for section in ['Mane','Tail']:
        points=trajectories[(frame,section)]
        for index,point in enumerate(points[:-1]):pose_error=max(pose_error,(Vector(expected[frame][f'{section}_{index:02}'])-Vector(point)).length)
assert static_error<1e-5 and pose_error<1e-5,(static_error,pose_error)
assert all(im.packed_file for im in bpy.data.images if im.source=='FILE' and im.type=='IMAGE')
scene.frame_set(180);scene.render.filepath=str(folder/'previews'/'FreshProcess.png');bpy.ops.render.render(write_still=True)
report=dict(status='passed for scoped prototype',source=dict(mesh_digest=digest,original_mesh_unchanged=True,static_vertices_sampled=len(static),max_static_local_error_m=static_error,max_simulation_to_bone_head_error_m=pose_error,weight_sum_max_error=max(abs(w-1) for w in weights),packed_images=True),exports={})
for extension in ['glb','fbx']:
    bpy.ops.wm.read_factory_settings(use_empty=True);scene=bpy.context.scene;scene.render.fps=30
    if extension=='glb':bpy.ops.import_scene.gltf(filepath=str(folder/('Voidreign-HairPrototype.'+extension)))
    else:bpy.ops.import_scene.fbx(filepath=str(folder/('Voidreign-HairPrototype.'+extension)),ignore_leaf_bones=False)
    rigs=[o for o in scene.objects if o.type=='ARMATURE'];assert rigs,extension
    arm=next(o for o in rigs if 'Mane_00' in o.pose.bones)
    meshes=[o for o in scene.objects if o.type=='MESH' and any(mod.type=='ARMATURE' for mod in o.modifiers)];assert len(meshes)==1,extension
    assert any(mod.type=='ARMATURE' for mod in meshes[0].modifiers)
    assert bpy.data.actions,extension
    starts=[action.frame_range[0] for action in bpy.data.actions];ends=[action.frame_range[1] for action in bpy.data.actions]
    start,end=min(starts),max(ends);print('Imported animation bounds',extension,start,end,flush=True);assert end-start>300,(extension,start,end)
    error=0
    for frame in sample_frames:
        target=start+(frame-1)*(end-start)/329;scene.frame_set(int(target),subframe=target-int(target));bpy.context.view_layer.update()
        for name,point in expected[frame].items():error=max(error,(arm.matrix_world@arm.pose.bones[name].head-Vector(point)).length)
    sizes=[list(n.image.size) for mat in meshes[0].data.materials for n in mat.node_tree.nodes if n.type=='TEX_IMAGE' and n.image]
    assert [4096,4096] in sizes
    report['exports'][extension]=dict(meshes=len(meshes),hair_bones=sum(b.name.startswith(('Mane_','Tail_')) for b in arm.data.bones),animation_frame_range=[start,end],max_source_to_imported_bone_head_error_m=error,image_sizes=sizes,pose_check='passed' if error<.005 else 'needs-review',pose_tolerance_m=.005)
    (folder/'validation-report.json').write_text(json.dumps(report,indent=2))
    if error>=.005:report['status']='needs-review: export pose deviation'
report['not_accepted']=['Rendered-surface collision/clearance','Full mane/tail coverage','Horse gait rig','Unreal runtime behavior/performance']
(folder/'validation-report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report),flush=True)
if report['status'].startswith('needs-review'):raise SystemExit(2)
