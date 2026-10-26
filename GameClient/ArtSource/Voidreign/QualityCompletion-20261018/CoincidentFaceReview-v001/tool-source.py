"""CPU forensic grouping of coincident triangles; never removes geometry.

Position equality alone is insufficient: compare winding, material, every UV
layer, corner normals, skin weights, point/corner colors and face flags exactly.
Even an exact rest match remains a review candidate until posed/visual checks.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np

p = argparse.ArgumentParser(description=__doc__)
for key in ['source', 'inspection', 'object', 'output']:
    p.add_argument('--'+key, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
source, inventory_path, out = [Path(value).resolve() for value in [a.source, a.inspection, a.output]]
inventory = json.loads(inventory_path.read_text())
assert hashlib.sha256(source.read_bytes()).hexdigest() == inventory['source_sha256']
entry = next(row for row in inventory['objects'] if row['object'] == a.object)
data = np.load(inventory_path.parent/entry['selection_file'])
out.mkdir(parents=True, exist_ok=False)
(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
bpy.ops.wm.open_mainfile(filepath=str(source))
bpy.context.scene.frame_set(1)
obj = bpy.data.objects[a.object]
mesh = obj.data
mesh.calc_loop_triangles()
points = np.array([vertex.co[:] for vertex in mesh.vertices], dtype=np.float32)
faces = np.array([tuple(triangle.vertices) for triangle in mesh.loop_triangles], dtype=np.int32)
assert hashlib.sha256(points.tobytes()+faces.tobytes()).hexdigest() == entry['mesh_hash']
assert np.array_equal(faces, np.array([tuple(face.vertices) for face in mesh.polygons], dtype=np.int32)), 'Triangle/polygon adapter required'
_, canonical = np.unique(points, axis=0, return_inverse=True)
canonical_faces = canonical[faces]
_, inverse, counts = np.unique(np.sort(canonical_faces, axis=1), axis=0, return_inverse=True, return_counts=True)
duplicate_groups = np.flatnonzero(counts > 1)
group_names = {group.index: group.name for group in obj.vertex_groups}
uv = {layer.name: layer for layer in mesh.uv_layers}
colors = {layer.name: layer for layer in mesh.color_attributes}
assert all(layer.domain in ['POINT', 'CORNER'] for layer in colors.values()), 'Unsupported color domain needs review'
materials = []
for material in mesh.materials:
    active = set()
    if material and material.use_nodes:
        pending = [node for node in material.node_tree.nodes if node.type == 'OUTPUT_MATERIAL' and node.is_active_output]
        while pending:
            node = pending.pop()
            if node in active:
                continue
            active.add(node)
            pending += [link.from_node for socket in node.inputs for link in socket.links]
    principled = [node for node in active if node.type == 'BSDF_PRINCIPLED']
    opacity = 'needs-review'
    if len(principled) == 1 and not principled[0].inputs['Alpha'].is_linked and principled[0].inputs['Alpha'].default_value == 1:
        opacity = 'principled-alpha-one; full shader review still required'
    materials.append(dict(name=material.name if material else None, active_node_types=sorted({node.type for node in active}),
                          opacity_summary=opacity, backface_culling=material.use_backface_culling if material else None))

def signature(index):
    face = mesh.polygons[index]
    vertices = faces[index]
    order = np.argsort(canonical_faces[index])
    loops = np.array(list(face.loop_indices))[order]
    ordered_vertices = vertices[order]
    winding = canonical_faces[index].tolist()
    winding = min(tuple(winding[shift:]+winding[:shift]) for shift in range(3))
    fields = dict(winding=winding, material=int(face.material_index), smooth=bool(face.use_smooth),
                  uv={name: np.array([layer.data[int(loop)].uv[:] for loop in loops], dtype=np.float32).tobytes().hex() for name, layer in uv.items()},
                  normals=np.array([mesh.corner_normals[int(loop)].vector[:] for loop in loops], dtype=np.float32).tobytes().hex(),
                  weights=[sorted((group_names[g.group], g.weight) for g in mesh.vertices[int(vertex)].groups) for vertex in ordered_vertices],
                  colors={name: np.array([layer.data[int(vertex if layer.domain == 'POINT' else loop)].color[:]
                      for vertex, loop in zip(ordered_vertices, loops)], dtype=np.float32).tobytes().hex() for name, layer in colors.items()})
    if hasattr(face, 'use_freestyle_mark'):
        fields['freestyle_mark'] = bool(face.use_freestyle_mark)
    return fields

rows, flat, offsets = [], [], [0]
equivalent_candidates = []
reasons = {key: 0 for key in ['winding', 'material', 'smooth', 'uv', 'normals', 'weights', 'colors', 'freestyle_mark']}
for group_number, group in enumerate(duplicate_groups):
    indices = np.flatnonzero(inverse == group)
    signatures = [signature(int(index)) for index in indices]
    partitions = {}
    for index, fields in zip(indices, signatures):
        key = json.dumps(fields, sort_keys=True)
        partitions.setdefault(key, []).append(int(index))
    clusters = [members for members in partitions.values() if len(members) > 1]
    exact = [index for cluster in clusters for index in cluster[1:]]
    equivalent_candidates.extend(exact)
    differences = [key for key in reasons if len({json.dumps(fields.get(key), sort_keys=True) for fields in signatures}) > 1]
    for key in differences:
        reasons[key] += 1
    rows.append(dict(id=entry['mesh_hash'][:12]+f':D{group_number:04}', faces=indices.tolist(),
                     source_component_ids=sorted({entry['components'][int(data['component_per_vertex'][faces[index, 0]])]['id'] for index in indices}),
                     differing_attributes=differences, exact_equivalence_clusters=clusters,
                     candidate_redundant_faces=exact, status='needs-semantic-posed-and-visual-review'))
    flat.extend(indices.tolist())
    offsets.append(len(flat))
np.savez_compressed(out/'face-groups.npz', group_offsets=np.array(offsets, dtype=np.int32),
                    group_faces=np.array(flat, dtype=np.int32), exact_attribute_candidate_faces=np.array(equivalent_candidates, dtype=np.int32))
report = dict(schema_version=1, status='forensic-review-only; no-removal-approval', cpu_only=True,
              source=str(source), source_sha256=inventory['source_sha256'], mesh_hash=entry['mesh_hash'], object=a.object,
              inspection_sha256=hashlib.sha256(inventory_path.read_bytes()).hexdigest(),
              tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              participating_faces=len(flat), coincident_position_groups=len(rows),
              exact_attribute_candidate_redundant_faces=len(equivalent_candidates),
              groups_differing_by_attribute=reasons, materials=materials, groups=rows,
              limitations=['Exact float32 rest positions/UV/normals/colors and exact stored weights, no tolerance merging.',
                           'Position groups count participants; only equivalence partitions suggest possible redundancy.',
                           'Rest coincidence cannot establish deformation equivalence or intended double surfaces.',
                           'Active shader attributes/alpha/backface semantics and all required poses need review before removal.',
                           'No geometry, material, animation or environment edits.'])
(out/'face-group-report.json').write_text(json.dumps(report, indent=2))
print(json.dumps({key: report[key] for key in ['participating_faces', 'coincident_position_groups', 'exact_attribute_candidate_redundant_faces', 'groups_differing_by_attribute']}))
