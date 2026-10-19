"""Remove explicitly reviewed whole mesh components into a new revision; CPU only.

Requires the inspection's mesh hash/selection artifact and explicit component IDs.
Never interprets size or distance as permission to delete another component.
"""
import argparse,hashlib,json,sys
from pathlib import Path
import bpy,bmesh,numpy as np
p=argparse.ArgumentParser();p.add_argument('--source',required=True);p.add_argument('--report',required=True);p.add_argument('--object-index',type=int,default=0)
p.add_argument('--component',type=int,action='append',required=True);p.add_argument('--output',required=True);p.add_argument('--name',required=True);p.add_argument('--reason',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);source=Path(a.source).resolve();report_path=Path(a.report).resolve();out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=False)
report=json.loads(report_path.read_text());entry=report['objects'][a.object_index]
assert hashlib.sha256(source.read_bytes()).hexdigest()==report['source_sha256'],'Source changed since inspection'
selections=np.load(report_path.parent/entry['selection_file']);labels=selections['component_per_vertex'];local=selections['local_vertices']
bpy.ops.wm.open_mainfile(filepath=str(source));obj=bpy.data.objects[entry['object']];data=obj.data
original=np.array([v.co[:] for v in data.vertices],dtype=np.float32)
assert np.array_equal(original,local),'Vertex selection no longer matches source'
data.calc_loop_triangles();triangles=np.array([tuple(t.vertices) for t in data.loop_triangles],dtype=np.int32)
assert hashlib.sha256(original.tobytes()+triangles.tobytes()).hexdigest()==entry['mesh_hash']
remove=np.isin(labels,a.component);assert remove.any() and not remove.all()
face_mask=remove[triangles].any(1)
assert np.all(remove[triangles[face_mask]]),'Selection crosses a component boundary'
kept_points=original[~remove].copy();before=(len(data.vertices),len(data.polygons));removed_faces=int(face_mask.sum())
mesh=bmesh.new();mesh.from_mesh(data);mesh.verts.ensure_lookup_table()
bmesh.ops.delete(mesh,geom=[mesh.verts[i] for i in np.flatnonzero(remove)],context='VERTS');mesh.to_mesh(data);mesh.free();data.update()
after_points=np.array([v.co[:] for v in data.vertices],dtype=np.float32)
assert np.array_equal(after_points,kept_points),'Unselected vertex positions changed'
groups={g.index:g.name for g in obj.vertex_groups};weight_error=0
skin_bones={bone.name for modifier in obj.modifiers if modifier.type=='ARMATURE' and modifier.object for bone in modifier.object.data.bones}
for vertex in data.vertices:
    if skin_bones:
        weight=sum(g.weight for g in vertex.groups if groups[g.group] in skin_bones)
        weight_error=max(weight_error,abs(weight-1))
assert weight_error<1e-5
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(out/(a.name+'.blend')))
bpy.ops.object.select_all(action='DESELECT');parents=[]
for inspected in report['objects']:
    inspected_obj=bpy.data.objects[inspected['object']];inspected_obj.select_set(True);parent=inspected_obj.parent
    while parent:parents.append(parent);parent=parent.parent
    for modifier in inspected_obj.modifiers:
        if modifier.type=='ARMATURE' and modifier.object:
            parents.append(modifier.object);parent=modifier.object.parent
            while parent:parents.append(parent);parent=parent.parent
for parent in set(parents):parent.select_set(True)
bpy.context.view_layer.objects.active=obj
bpy.ops.export_scene.gltf(filepath=str(out/(a.name+'.glb')),export_format='GLB',use_selection=True,export_animations=True,export_animation_mode='SCENE',export_anim_scene_split_object=False)
bpy.ops.export_scene.fbx(filepath=str(out/(a.name+'.fbx')),use_selection=True,object_types={'MESH','ARMATURE','EMPTY'},path_mode='COPY',embed_textures=True,bake_anim=True,bake_anim_simplify_factor=0.,bake_anim_use_nla_strips=False,bake_anim_use_all_actions=False,add_leaf_bones=False)
audit=dict(status='targeted geometry repair; reimport checks pending',source=str(source),source_sha256=report['source_sha256'],source_mesh_hash=entry['mesh_hash'],
    component_ids=[entry['components'][i]['id'] for i in a.component],reason=a.reason,removed_vertices=int(remove.sum()),removed_triangles=removed_faces,
    remaining_vertices=len(data.vertices),remaining_triangles=sum(len(p.vertices)-2 for p in data.polygons),unselected_vertex_positions_unchanged=True,weight_sum_max_error=weight_error,
    note='Whole reviewed islands only; no welding, UV unwrap, texture edits or other component removal. Previous motion/export defects remain open.')
(out/'repair-report.json').write_text(json.dumps(audit,indent=2));print(json.dumps(audit),flush=True)
