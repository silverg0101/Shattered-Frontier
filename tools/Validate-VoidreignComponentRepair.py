"""CPU-only exact delta/UV/deformation/import checks for the Voidreign rig profile."""
import argparse,hashlib,json,sys
from pathlib import Path
import bpy,numpy as np
from mathutils import Vector
p=argparse.ArgumentParser();p.add_argument('--folder',required=True);p.add_argument('--inspection',required=True);p.add_argument('--name',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);folder=Path(a.folder).resolve();inspection_path=Path(a.inspection).resolve()
repair=json.loads((folder/'repair-report.json').read_text());inspection=json.loads(inspection_path.read_text());entry=inspection['objects'][0]
selection=np.load(inspection_path.parent/entry['selection_file']);removed_indices=[int(cid.split(':C')[-1]) for cid in repair['component_ids']]
keep=~np.isin(selection['component_per_vertex'],removed_indices)
source=Path(repair['source']);assert hashlib.sha256(source.read_bytes()).hexdigest()==repair['source_sha256']
def coords(data):
    output=np.empty(len(data.vertices)*3,dtype=np.float32);data.vertices.foreach_get('co',output);return output.reshape(-1,3)
def uv(data):return np.array([loop.uv[:] for loop in data.uv_layers.active.data],dtype=np.float32)
def images(obj):return [list(node.image.size) for mat in obj.data.materials if mat and mat.use_nodes for node in mat.node_tree.nodes if node.type=='TEX_IMAGE' and node.image]
bpy.ops.wm.open_mainfile(filepath=str(source));obj=bpy.data.objects[entry['object']]
old_to_new=np.full(len(keep),-1,dtype=np.int32);old_to_new[keep]=np.arange(keep.sum())
polygons=[poly for poly in obj.data.polygons if all(keep[i] for i in poly.vertices)]
expected_faces=np.array([old_to_new[list(poly.vertices)] for poly in polygons],dtype=np.int32)
expected_uv=uv(obj.data)[np.concatenate([np.array(poly.loop_indices) for poly in polygons])]
expected_materials=[poly.material_index for poly in polygons]
frames=[1,60,120,180,240,300,330];deformed={};expected_bones={}
for frame in frames:
    bpy.context.scene.frame_set(frame);graph=bpy.context.evaluated_depsgraph_get();deformed[frame]=coords(obj.evaluated_get(graph).data)[keep]
    arm=next(mod.object for mod in obj.modifiers if mod.type=='ARMATURE')
    expected_bones[frame]={bone.name:list(arm.matrix_world@bone.head) for bone in arm.pose.bones}
bpy.ops.wm.open_mainfile(filepath=str(folder/(a.name+'.blend')));obj=bpy.data.objects[entry['object']]
assert np.array_equal(coords(obj.data),selection['local_vertices'][keep])
assert np.array_equal(np.array([tuple(poly.vertices) for poly in obj.data.polygons]),expected_faces)
assert np.array_equal(uv(obj.data),expected_uv),'UV loops changed'
assert [poly.material_index for poly in obj.data.polygons]==expected_materials
max_error=0
for frame in frames:
    bpy.context.scene.frame_set(frame);graph=bpy.context.evaluated_depsgraph_get();delta=coords(obj.evaluated_get(graph).data)-deformed[frame]
    max_error=max(max_error,float(np.linalg.norm(delta,axis=1).max()))
assert max_error<1e-5,max_error
report=dict(status='needs-review',cleanup_status='passed',cpu_only=True,
    checks=dict(exact_requested_components_removed=True,retained_vertex_positions_unchanged=True,retained_faces_unchanged=True,retained_uvs_unchanged=True,retained_material_assignments_unchanged=True,
        sampled_frames=frames,max_retained_deformed_vertex_error_m=max_error),exports={})
for extension in ['glb','fbx']:
    bpy.ops.wm.read_factory_settings(use_empty=True);scene=bpy.context.scene;scene.render.fps=30
    if extension=='glb':bpy.ops.import_scene.gltf(filepath=str(folder/(a.name+'.glb')))
    else:bpy.ops.import_scene.fbx(filepath=str(folder/(a.name+'.fbx')),ignore_leaf_bones=False)
    meshes=[o for o in scene.objects if o.type=='MESH' and any(mod.type=='ARMATURE' for mod in o.modifiers)]
    assert len(meshes)==1;mesh=meshes[0];mesh.data.calc_loop_triangles();assert len(mesh.data.loop_triangles)==repair['remaining_triangles']
    assert mesh.data.uv_layers.active is not None
    image_sizes=images(mesh);assert image_sizes and all(min(size)>0 for size in image_sizes)
    arm=next(mod.object for mod in mesh.modifiers if mod.type=='ARMATURE');assert len(arm.data.bones)==20
    start=min(action.frame_range[0] for action in bpy.data.actions);end=max(action.frame_range[1] for action in bpy.data.actions);error=0
    for frame in frames:
        target=start+(frame-1)*(end-start)/329;scene.frame_set(int(target),subframe=target-int(target))
        for name,point in expected_bones[frame].items():error=max(error,(arm.matrix_world@arm.pose.bones[name].head-Vector(point)).length)
    report['exports'][extension]=dict(triangles=len(mesh.data.loop_triangles),bones=len(arm.data.bones),image_sizes=image_sizes,max_source_bone_head_error_m=error,pose_check='passed' if error<.005 else 'needs-review',pose_tolerance_m=.005)
report['remaining_gates']=['Unclassified other components','Full mane/tail coverage','Thin bridges','Deformed-surface attachment/collision','FBX pose deviation','Unreal runtime validation']
(folder/'cleanup-validation.json').write_text(json.dumps(report,indent=2));print(json.dumps(report),flush=True)
if any(export['pose_check']!='passed' for export in report['exports'].values()):raise SystemExit(2)
