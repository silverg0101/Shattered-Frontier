"""Fresh-process exact retained-surface and root-connectivity checks; CPU only."""
import argparse
import hashlib
import json
import sys
from pathlib import Path
import bpy
import numpy as np

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--folder', required=True)
p.add_argument('--inspection', required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
folder, inspection_path = Path(a.folder), Path(a.inspection)
assert not (folder/'root-validation.json').exists()
repair = json.loads((folder/'root-repair-report.json').read_text())
inspection = json.loads(inspection_path.read_text())
entry = inspection['objects'][0]
source = Path(repair['source'])
assert hashlib.sha256(source.read_bytes()).hexdigest() == repair['source_sha256']
selection = np.load(inspection_path.parent/entry['selection_file'])
assert inspection['source_sha256'] == hashlib.sha256((folder/'Voidreign-HairPrototype.blend').read_bytes()).hexdigest()


def coords(data):
    result = np.empty(len(data.vertices)*3, dtype=np.float32)
    data.vertices.foreach_get('co', result)
    return result.reshape(-1, 3)


bpy.ops.wm.open_mainfile(filepath=str(source.resolve()))
obj = bpy.data.objects[entry['object']]
before = coords(obj.data)
removed = set(repair['removed_cap_face_indices'])
retained = [f for f in obj.data.polygons if f.index not in removed]
faces = [tuple(f.vertices) for f in retained]
uv = [np.array([obj.data.uv_layers.active.data[i].uv[:] for i in f.loop_indices]) for f in retained]
material = [f.material_index for f in retained]
weights = [{g.group: g.weight for g in v.groups} for v in obj.data.vertices]
frames = [1, 60, 120, 180, 240, 300, 330]
expected = {}
for frame in frames:
    bpy.context.scene.frame_set(frame)
    expected[frame] = coords(obj.evaluated_get(bpy.context.evaluated_depsgraph_get()).data)
bpy.ops.wm.open_mainfile(filepath=str((folder/'Voidreign-HairPrototype.blend').resolve()))
obj = bpy.data.objects[entry['object']]
assert np.array_equal(coords(obj.data)[:len(before)], before)
assert [{g.group: g.weight for g in v.groups} for v in obj.data.vertices[:len(before)]] == weights
for index, face in enumerate(obj.data.polygons[:len(faces)]):
    assert tuple(face.vertices) == faces[index]
    actual = np.array([obj.data.uv_layers.active.data[i].uv[:] for i in face.loop_indices])
    assert np.array_equal(actual, uv[index])
    assert face.material_index == material[index]
error = 0.
for frame in frames:
    bpy.context.scene.frame_set(frame)
    actual = coords(obj.evaluated_get(bpy.context.evaluated_depsgraph_get()).data)[:len(before)]
    error = max(error, float(np.linalg.norm(actual-expected[frame], axis=1).max()))
assert error < 1e-5
new_faces = list(obj.data.polygons[len(faces):])
positions = coords(obj.data)
areas, uv_areas = [], []
for face in new_faces:
    assert len(face.vertices) == 3
    v = positions[list(face.vertices)]
    areas.append(float(np.linalg.norm(np.cross(v[1]-v[0], v[2]-v[0]))/2))
    tex = np.array([obj.data.uv_layers.active.data[i].uv[:] for i in face.loop_indices])
    ab, ac = tex[1]-tex[0], tex[2]-tex[0]
    uv_areas.append(abs(float(ab[0]*ac[1]-ab[1]*ac[0]))/2)
assert min(areas) > 0 and min(uv_areas) > 0
joins = []
for row in repair['repairs']:
    parent = selection['component_per_vertex'][row['source_parent_vertices']]
    child = selection['component_per_vertex'][row['source_child_vertices']]
    assert len(set(parent)) == 1 and np.array_equal(parent, child), row['component_id']
    joins.append(dict(source_component_id=row['component_id'], repaired_component=int(parent[0]), joined=True))
report = dict(status='passed-rest-repair-checks-only', cpu_only=True,
    source_sha256=repair['source_sha256'], candidate_sha256=inspection['source_sha256'],
    tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    original_vertices_and_weights_unchanged=True, retained_faces_uv_materials_unchanged=True,
    changed_source_cap_faces=sorted(removed), new_vertices=len(positions)-len(before), new_triangles=len(new_faces),
    minimum_new_triangle_area_local_units= min(areas), minimum_new_uv_triangle_area=min(uv_areas),
    new_uv_overlap='Intentional reuse of child-cap hair patch for root-continuation triangles',
    sample_frames=frames, max_retained_deformed_vertex_error_m=error,
    component_count=entry['component_count'], repaired_clumps=joins,
    remaining_gates=['Root shading/silhouette and intersections', 'Complete rebuilt hair rig',
                     'Attachment relation review', 'Deformed-surface collision', 'Fresh final exports'])
(folder/'root-validation.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report), flush=True)
