"""Author explicit tapered root continuations for reviewed existing hair clumps.

Requires source/mesh hashes, exact clump IDs and root-facing endpoint/anchor
coordinates. Retains existing vertices and all faces except two selected caps
per continuation. New geometry/UVs/temporary weights are fully audited.
This is an authored repair candidate; render/topology/motion review is required.
"""
import argparse
import hashlib
import json
import math
import sys
from pathlib import Path
import bpy
import bmesh
import numpy as np
from mathutils import Vector

p = argparse.ArgumentParser(description=__doc__)
for key in ['source', 'inspection', 'profile', 'output']:
    p.add_argument('--' + key, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
source, inspection_path, profile_path, out = map(Path, [a.source, a.inspection, a.profile, a.output])
inspection = json.loads(inspection_path.read_text())
profile = json.loads(profile_path.read_text())
assert profile['schema_version'] == 1
source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
assert source_hash == inspection['source_sha256'] == profile['source_sha256']
entry = next(e for e in inspection['objects'] if e['object'] == profile['object'])
assert entry['mesh_hash'] == profile['mesh_hash']
selected = np.load(inspection_path.parent / entry['selection_file'])
out.mkdir(parents=True, exist_ok=False)
bpy.ops.wm.open_mainfile(filepath=str(source.resolve()))
scene = bpy.context.scene
scene.frame_set(1)
obj = bpy.data.objects[profile['object']]
data = obj.data
assert obj.matrix_world.to_scale().length > 0
local = np.array([v.co[:] for v in data.vertices], dtype=np.float32)
data.calc_loop_triangles()
triangles = np.array([tuple(t.vertices) for t in data.loop_triangles], dtype=np.int32)
assert hashlib.sha256(local.tobytes()+triangles.tobytes()).hexdigest() == entry['mesh_hash']
assert len(data.polygons) == len(triangles), 'Authored adapter expects a triangulated mesh'
world = np.array([obj.matrix_world @ v.co for v in data.vertices])
component = selected['component_per_vertex']
face_component = component[triangles[:, 0]]
vectors = world[triangles]
centers = vectors.mean(1)
normals = np.cross(vectors[:, 1]-vectors[:, 0], vectors[:, 2]-vectors[:, 0])
normals /= np.maximum(np.linalg.norm(normals, axis=1)[:, None], 1e-12)
original_faces = [tuple(f.vertices) for f in data.polygons]
original_uv = [np.array([data.uv_layers.active.data[i].uv[:] for i in f.loop_indices]) for f in data.polygons]
original_material = [f.material_index for f in data.polygons]
original_weights = [{g.group: g.weight for g in v.groups} for v in data.vertices]
before_vertices = len(data.vertices)
bm = bmesh.new()
bm.from_mesh(data)
bm.verts.ensure_lookup_table()
bm.faces.ensure_lookup_table()
original_bm_vertices = list(bm.verts)
original_bm_faces = list(bm.faces)
uv_layer = bm.loops.layers.uv.active
deform = bm.verts.layers.deform.verify()
inverse = obj.matrix_world.inverted()
removed, records = set(), []
for repair in profile['repairs']:
    cid = repair['component_index']
    assert repair['component_id'] == entry['components'][cid]['id']
    endpoint = np.array(repair['root_facing_endpoint_m'], dtype=float)
    anchor = np.array(repair['anatomical_anchor_m'], dtype=float)
    radius = repair['radius_m']
    step = repair['segment_length_m']
    assert .002 <= radius <= .025 and .01 <= step <= .08
    direction = endpoint-anchor
    direction /= np.linalg.norm(direction)
    parent_candidates = np.flatnonzero((face_component == repair['parent_component_index'])
        & (np.linalg.norm(centers-anchor, axis=1) < repair['anchor_search_radius_m'])
        & ((normals @ direction) > .05))
    parent_candidates = parent_candidates[~np.isin(parent_candidates, list(removed))]
    assert len(parent_candidates), 'No outward-facing anatomical root cap'
    parent_face = int(parent_candidates[np.argmin(np.linalg.norm(centers[parent_candidates]-anchor, axis=1))])
    child_candidates = np.flatnonzero((face_component == cid)
        & (np.linalg.norm(centers-endpoint, axis=1) < repair['endpoint_search_radius_m'])
        & ((normals @ (-direction)) > .05))
    assert len(child_candidates), 'No root-facing clump cap near authored endpoint'
    child_face = int(child_candidates[np.argmin(np.linalg.norm(centers[child_candidates]-endpoint, axis=1))])
    assert parent_face not in removed and child_face not in removed, 'Root caps overlap'
    parent_ids = list(original_faces[parent_face])
    child_ids = list(reversed(original_faces[child_face]))
    start, end = centers[parent_face], centers[child_face]
    length = np.linalg.norm(end-start)
    assert .01 <= length <= repair['maximum_continuation_length_m']
    n = max(4, int(math.ceil(length/step)))
    p0 = Vector(start)
    p1 = p0 + Vector(normals[parent_face])*min(.10, length*.3)
    p3 = Vector(end)
    p2 = p3 + Vector(normals[child_face])*min(.08, length*.25)
    centers_curve, tangents = [], []
    for k in range(n+1):
        t = k/n
        centers_curve.append((1-t)**3*p0 + 3*(1-t)**2*t*p1 + 3*(1-t)*t*t*p2 + t**3*p3)
        tangent = 3*(1-t)**2*(p1-p0) + 6*(1-t)*t*(p2-p1) + 3*t*t*(p3-p2)
        tangents.append(tangent.normalized())
    u = (Vector(world[parent_ids[0]])-p0)
    u = (u-tangents[0]*u.dot(tangents[0])).normalized()
    frames = [(u.copy(), tangents[0].cross(u).normalized())]
    for k in range(1, n+1):
        u = tangents[k-1].rotation_difference(tangents[k]) @ u
        u = (u-tangents[k]*u.dot(tangents[k])).normalized()
        frames.append((u.copy(), tangents[k].cross(u).normalized()))
    final_u, final_v = frames[-1]
    choices = []
    for shift in range(3):
        ids = child_ids[shift:]+child_ids[:shift]
        vector = Vector(world[ids[0]])-p3
        angle = math.atan2(vector.dot(final_v), vector.dot(final_u))
        choices.append((abs(angle), angle, ids))
    _, twist, child_ids = min(choices, key=lambda row: row[0])
    start_radius = np.mean(np.linalg.norm(world[parent_ids]-start, axis=1))
    end_radius = np.mean(np.linalg.norm(world[child_ids]-end, axis=1))
    parent_weights = {}
    child_weights = {}
    for ids, target in [(parent_ids, parent_weights), (child_ids, child_weights)]:
        for index in ids:
            for group, weight in original_weights[index].items():
                target[group] = target.get(group, 0)+weight/3
    rings = [[original_bm_vertices[i] for i in parent_ids]]
    for k in range(1, n):
        t = k/n
        u, v = frames[k]
        blend_start = min(1., t/.18)
        blend_end = min(1., (1-t)/.18)
        r = float(((1-blend_start)*start_radius+blend_start*radius)*blend_end + end_radius*(1-blend_end))
        ring = []
        for j in range(3):
            angle = j*math.tau/3 + twist*t
            point = centers_curve[k]+r*(math.cos(angle)*u+math.sin(angle)*v)
            vertex = bm.verts.new(inverse @ point)
            for group in set(parent_weights) | set(child_weights):
                weight = (1-t)*parent_weights.get(group, 0)+t*child_weights.get(group, 0)
                if weight > 0:
                    vertex[deform][group] = weight
            ring.append(vertex)
        rings.append(ring)
    rings.append([original_bm_vertices[i] for i in child_ids])
    uv = original_uv[child_face]
    new_faces = 0
    for lower, upper in zip(rings, rings[1:]):
        for j in range(3):
            jj = (j+1)%3
            for verts, tex in [([lower[j], lower[jj], upper[jj]], uv),
                               ([lower[j], upper[jj], upper[j]], uv)]:
                face = bm.faces.new(verts)
                face.material_index = original_material[child_face]
                face.smooth = True
                for loop, value in zip(face.loops, tex):
                    loop[uv_layer].uv = value
                new_faces += 1
    removed.update([parent_face, child_face])
    records.append(dict(component_id=repair['component_id'], parent_face=parent_face,
        child_face=child_face, source_parent_vertices=parent_ids, source_child_vertices=child_ids,
        actual_anchor_m=start.tolist(), actual_endpoint_m=end.tolist(), new_vertices=(n-1)*3,
        new_triangles=new_faces, continuation_length_m=float(length), radius_m=radius,
        centerline_m=[list(v) for v in centers_curve],
        uv_method='Reuse child-cap hair texture patch; no new texture synthesis'))
for index in sorted(removed, reverse=True):
    bm.faces.remove(original_bm_faces[index])
bm.to_mesh(data)
bm.free()
data.update()
after = np.array([v.co[:] for v in data.vertices], dtype=np.float32)
assert np.array_equal(after[:before_vertices], local), 'Existing vertices moved/reordered'
retained = [index for index in range(len(original_faces)) if index not in removed]
for new_index, old_index in enumerate(retained):
    face = data.polygons[new_index]
    assert tuple(face.vertices) == original_faces[old_index], 'Retained face changed/reordered'
    actual_uv = np.array([data.uv_layers.active.data[i].uv[:] for i in face.loop_indices])
    assert np.array_equal(actual_uv, original_uv[old_index]), 'Retained UV changed'
    assert face.material_index == original_material[old_index]
weight_error = max(abs(sum(g.weight for g in v.groups)-1) for v in data.vertices)
assert weight_error < 1e-5, weight_error
bpy.ops.file.pack_all()
bpy.ops.wm.save_as_mainfile(filepath=str(out/'Voidreign-HairPrototype.blend'))
report = dict(status='authored root-repair candidate; inspection and new rig required',
    source=str(source.resolve()), source_sha256=source_hash, mesh_hash=entry['mesh_hash'],
    profile_sha256=hashlib.sha256(profile_path.read_bytes()).hexdigest(),
    tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), repairs=records,
    original_vertices_unchanged=True, retained_faces_uv_materials_unchanged=True,
    removed_cap_face_indices=sorted(removed), before_vertices=before_vertices,
    after_vertices=len(data.vertices), after_triangles=sum(len(f.vertices)-2 for f in data.polygons),
    weight_sum_max_error=weight_error,
    limitations=['Three-sided tapered root continuations; review their silhouette and shading.',
                 'Temporary interpolated old-rig weights; full semantic rig must be rebuilt.',
                 'Existing cap faces intentionally replaced; topology/UV/rig/export evidence invalidated.'])
(out/'root-repair-report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report), flush=True)
