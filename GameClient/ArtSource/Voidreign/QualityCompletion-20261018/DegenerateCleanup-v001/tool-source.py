"""Remove explicitly reviewed exact-zero-area triangles in a new animated copy.

No threshold-based removal, welding, simplification or hidden-shell cleanup.
All retained geometry, UVs, material assignments, point colors and weights must
match their recorded source correspondence before any file is saved.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bmesh
import bpy
import numpy as np

p = argparse.ArgumentParser(description=__doc__)
for key in ['source', 'profile', 'output']:
    p.add_argument('--'+key, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
source, profile_path, out = [Path(value).resolve() for value in [a.source, a.profile, a.output]]
profile = json.loads(profile_path.read_text())
assert profile['schema_version'] == 1 and profile['review_status'] == 'reviewed-exact-degenerates-only'
assert hashlib.sha256(source.read_bytes()).hexdigest() == profile['source_sha256']
out.mkdir(parents=True, exist_ok=False)
(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
(out/'profile.json').write_bytes(profile_path.read_bytes())
bpy.ops.wm.open_mainfile(filepath=str(source))
scene = bpy.context.scene
scene.frame_set(1)
obj = bpy.data.objects[profile['object']]
old = obj.data
assert not old.shape_keys, 'Shape-key correspondence needs a separate reviewed adapter'
assert all(len(face.vertices) == 3 for face in old.polygons)
old.calc_loop_triangles()
vertices = np.array([v.co[:] for v in old.vertices], dtype=np.float32)
faces = np.array([tuple(t.vertices) for t in old.loop_triangles], dtype=np.int32)
assert np.array_equal(faces, np.array([tuple(f.vertices) for f in old.polygons], dtype=np.int32))
assert hashlib.sha256(vertices.tobytes()+faces.tobytes()).hexdigest() == profile['mesh_hash']
ids = np.array(profile['triangle_indices'], dtype=np.int32)
assert len(ids) == len(np.unique(ids)) == profile['expected_removed_triangles']
assert np.all((ids >= 0) & (ids < len(faces)))
points = vertices[faces].astype(np.float64)
areas = np.linalg.norm(np.cross(points[:, 1]-points[:, 0], points[:, 2]-points[:, 0]), axis=1)/2
assert np.all(areas[ids] == 0), 'Selection includes a nondegenerate face'
assert np.array_equal(np.flatnonzero(areas == 0), np.sort(ids)), 'Review all exact degenerates explicitly'
expected_faces = np.setdiff1d(np.arange(len(faces)), ids)
expected_vertices = np.unique(faces[expected_faces])
assert len(vertices)-len(expected_vertices) == profile['expected_removed_vertices']
assert len(np.unique(faces)) == len(vertices), 'Pre-existing unused vertices need separate review'
assert profile['frame_start'] == 1 and profile['frame_end'] == 330
maximum_removed_posed_area = 0.
for frame in range(profile['frame_start'], profile['frame_end']+1):
    scene.frame_set(frame)
    evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    corners = np.array([[evaluated.data.vertices[int(index)].co[:] for index in face] for face in faces[ids]], dtype=np.float64)
    posed_area = np.linalg.norm(np.cross(corners[:, 1]-corners[:, 0], corners[:, 2]-corners[:, 0]), axis=1)/2
    maximum_removed_posed_area = max(maximum_removed_posed_area, float(posed_area.max()))
assert maximum_removed_posed_area == 0., 'A rest-collapsed face opens under animation; separate semantic review required'
scene.frame_set(1)
groups = {group.index: group.name for group in obj.vertex_groups}
old_weights = [sorted((groups[g.group], g.weight) for g in vertex.groups) for vertex in old.vertices]
old_uvs = {layer.name: np.array([loop.uv[:] for loop in layer.data], dtype=np.float32) for layer in old.uv_layers}
old_colors = {layer.name: (layer.domain, np.array([item.color[:] for item in layer.data], dtype=np.float32)) for layer in old.color_attributes}
old_materials = np.array([face.material_index for face in old.polygons], dtype=np.int32)
old_normals = np.array([normal.vector[:] for normal in old.corner_normals], dtype=np.float32)
had_custom_normals = old.has_custom_normals
new = old.copy()
new.name = old.name+' | reviewed degenerate-face cleanup'
obj.data = new
bm = bmesh.new()
bm.from_mesh(new)
bm.verts.ensure_lookup_table()
bm.faces.ensure_lookup_table()
assert not any(not edge.link_faces for edge in bm.edges), 'Pre-existing loose edges need a separate reviewed correspondence'
v_layer = bm.verts.layers.int.new('_quality_source_vertex')
f_layer = bm.faces.layers.int.new('_quality_source_face')
l_layer = bm.loops.layers.int.new('_quality_source_loop')
for vertex in bm.verts:
    vertex[v_layer] = vertex.index
for face in bm.faces:
    face[f_layer] = face.index
    source_face = old.polygons[face.index]
    source_loops = {old.loops[index].vertex_index: index for index in source_face.loop_indices}
    assert set(loop.vert.index for loop in face.loops) == set(source_face.vertices)
    for loop in face.loops:
        loop[l_layer] = source_loops[loop.vert.index]
for face in [bm.faces[int(index)] for index in ids]:
    bm.faces.remove(face)
for edge in list(bm.edges):
    if not edge.link_faces:
        bm.edges.remove(edge)
for vertex in list(bm.verts):
    if not vertex.link_faces and not vertex.link_edges:
        bm.verts.remove(vertex)
bm.to_mesh(new)
bm.free()
new.update()
v_map = np.array([item.value for item in new.attributes['_quality_source_vertex'].data], dtype=np.int32)
f_map = np.array([item.value for item in new.attributes['_quality_source_face'].data], dtype=np.int32)
l_map = np.array([item.value for item in new.attributes['_quality_source_loop'].data], dtype=np.int32)
for name in ['_quality_source_vertex', '_quality_source_face', '_quality_source_loop']:
    new.attributes.remove(new.attributes[name])
assert set(v_map.tolist()) == set(expected_vertices.tolist())
assert set(f_map.tolist()) == set(expected_faces.tolist())
assert np.array_equal(np.array([v.co[:] for v in new.vertices], dtype=np.float32), vertices[v_map])
actual_faces = np.array([tuple(face.vertices) for face in new.polygons], dtype=np.int32)
assert np.array_equal(v_map[actual_faces], faces[f_map])
assert np.array_equal(np.array([f.material_index for f in new.polygons]), old_materials[f_map])
for layer in new.uv_layers:
    assert np.array_equal(np.array([loop.uv[:] for loop in layer.data], dtype=np.float32), old_uvs[layer.name][l_map])
for layer in new.color_attributes:
    domain, values = old_colors[layer.name]
    assert domain == layer.domain and domain in ['POINT', 'CORNER']
    mapping = v_map if domain == 'POINT' else l_map
    assert np.array_equal(np.array([item.color[:] for item in layer.data], dtype=np.float32), values[mapping])
for vertex, original in zip(new.vertices, v_map):
    assert sorted((groups[g.group], g.weight) for g in vertex.groups) == old_weights[original]
if had_custom_normals:
    new.normals_split_custom_set(old_normals[l_map])
new_normals = np.array([normal.vector[:] for normal in new.corner_normals], dtype=np.float32)
normal_delta = float(np.max(np.abs(new_normals-old_normals[l_map])))
assert normal_delta <= 1e-6, 'Retained shading normals changed; review before saving'
np.savez_compressed(out/'source-correspondence.npz', new_to_source_vertex=v_map,
                    new_to_source_triangle=f_map, new_to_source_loop=l_map,
                    removed_source_triangles=ids,
                    removed_source_vertices=np.setdiff1d(np.arange(len(vertices)), v_map))
scene.frame_set(125)
bpy.ops.file.pack_all()
bpy.ops.wm.save_as_mainfile(filepath=str(out/'Voidreign-CompleteHair.blend'))
arms = [modifier.object for modifier in obj.modifiers if modifier.type == 'ARMATURE']
root = bpy.data.objects[profile['assembly_root']]
bpy.ops.object.select_all(action='DESELECT')
for item in [obj, root]+arms:
    item.select_set(True)
bpy.context.view_layer.objects.active = arms[0] if arms else obj
bpy.ops.export_scene.gltf(filepath=str(out/'Voidreign-CompleteHair.glb'), export_format='GLB',
    use_selection=True, export_animations=True, export_animation_mode='SCENE', export_anim_scene_split_object=False)
bpy.ops.export_scene.fbx(filepath=str(out/'Voidreign-CompleteHair.fbx'), use_selection=True,
    object_types={'MESH', 'ARMATURE', 'EMPTY'}, path_mode='COPY', embed_textures=True,
    bake_anim=True, bake_anim_simplify_factor=0., bake_anim_use_nla_strips=False,
    bake_anim_use_all_actions=False, add_leaf_bones=False)
report = dict(status='targeted-geometry-hygiene-candidate; fresh comparison/import review required',
              source=str(source), source_sha256=profile['source_sha256'], source_mesh_hash=profile['mesh_hash'],
              profile_sha256=hashlib.sha256(profile_path.read_bytes()).hexdigest(),
              removed_triangles=len(ids), removed_vertices=len(vertices)-len(new.vertices),
              retained_vertices=len(new.vertices), retained_triangles=len(new.polygons),
              retained_geometry_uv_material_colors_weights_exact=True,
              maximum_retained_corner_normal_delta=normal_delta, cpu_only=True,
              removed_faces_collapsed_frames=profile['frame_end']-profile['frame_start']+1,
              maximum_removed_posed_area_m2=maximum_removed_posed_area,
              scope='Exact-zero-area faces and resulting unused elements only. Duplicate/hidden shells retained. Rig/root repair budgets and inherited failures unchanged.')
(out/'repair-report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report), flush=True)
