"""Fresh CPU retained-surface comparison using an explicit repair correspondence."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np

p = argparse.ArgumentParser(description=__doc__)
for key in ['folder', 'profile', 'output']:
    p.add_argument('--'+key, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
folder, profile_path, out = [Path(value).resolve() for value in [a.folder, a.profile, a.output]]
profile = json.loads(profile_path.read_text())
repair = json.loads((folder/'repair-report.json').read_text())
mapping = np.load(folder/'source-correspondence.npz')
vmap, fmap, lmap = [mapping[name] for name in ['new_to_source_vertex', 'new_to_source_triangle', 'new_to_source_loop']]
source = Path(repair['source'])
assert hashlib.sha256(source.read_bytes()).hexdigest() == repair['source_sha256'] == profile['source_sha256']
assert profile['schema_version'] == 1
start, end = profile['frame_start'], profile['frame_end']
samples = profile['deformation_sample_frames']
assert type(start) is int and type(end) is int and 1 <= start < end <= 10000
assert samples == sorted(set(samples)) and all(start <= frame <= end for frame in samples)
for key in ['surface_tolerance_m', 'matrix_tolerance', 'normal_tolerance']:
    assert type(profile[key]) is float and 1e-8 <= profile[key] <= 1e-5
out.mkdir(parents=True, exist_ok=False)
(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
(out/'profile.json').write_bytes(profile_path.read_bytes())

def capture(path):
    bpy.ops.wm.open_mainfile(filepath=str(path))
    obj = bpy.data.objects[profile['object']]
    mesh, scene = obj.data, bpy.context.scene
    assert all(len(face.vertices) == 3 for face in mesh.polygons)
    vertices = np.array([vertex.co[:] for vertex in mesh.vertices], dtype=np.float32)
    faces = np.array([tuple(face.vertices) for face in mesh.polygons], dtype=np.int32)
    groups = {group.index: group.name for group in obj.vertex_groups}
    weights = [sorted((groups[g.group], g.weight) for g in vertex.groups) for vertex in mesh.vertices]
    uv = {layer.name: np.array([loop.uv[:] for loop in layer.data], dtype=np.float32) for layer in mesh.uv_layers}
    colors = {layer.name: (layer.domain, np.array([item.color[:] for item in layer.data], dtype=np.float32)) for layer in mesh.color_attributes}
    normals = np.array([normal.vector[:] for normal in mesh.corner_normals], dtype=np.float32)
    images = {}
    for material in mesh.materials:
        pending = [node for node in material.node_tree.nodes if node.type == 'OUTPUT_MATERIAL' and node.is_active_output]
        seen = set()
        while pending:
            node = pending.pop()
            if node in seen:
                continue
            seen.add(node)
            if node.type == 'TEX_IMAGE' and node.image:
                pixels = np.empty(len(node.image.pixels), dtype=np.float32)
                node.image.pixels.foreach_get(pixels)
                images[(material.name, node.name)] = dict(size=list(node.image.size), sha256=hashlib.sha256(pixels.tobytes()).hexdigest())
            pending += [link.from_node for socket in node.inputs for link in socket.links]
    arm = next(modifier.object for modifier in obj.modifiers if modifier.type == 'ARMATURE')
    assert arm.data.pose_position == 'POSE'
    names = sorted(arm.pose.bones.keys())
    matrices, surfaces = [], {}
    for frame in range(start, end+1):
        scene.frame_set(frame)
        matrices.append(np.array([arm.matrix_world@arm.pose.bones[name].matrix for name in names]))
        if frame in samples:
            evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
            points = np.empty(len(mesh.vertices)*3, dtype=np.float32)
            evaluated.data.vertices.foreach_get('co', points)
            matrix = np.array(obj.matrix_world)
            surfaces[frame] = points.reshape(-1, 3)@matrix[:3, :3].T+matrix[:3, 3]
    return dict(vertices=vertices, faces=faces, weights=weights, uv=uv, colors=colors,
                normals=normals, images=images, bone_names=names, matrices=np.array(matrices), surfaces=surfaces,
                material_assignments=np.array([face.material_index for face in mesh.polygons]))

before = capture(source)
candidate = folder/'Voidreign-CompleteHair.blend'
after = capture(candidate)
checks = []

def check(name, passed, **values):
    checks.append(dict(id=name, status='passed' if passed else 'failed', **values))

check('retained_positions', np.array_equal(after['vertices'], before['vertices'][vmap]))
check('retained_faces', np.array_equal(vmap[after['faces']], before['faces'][fmap]))
check('retained_uvs', before['uv'].keys() == after['uv'].keys() and all(np.array_equal(values[lmap], after['uv'][name]) for name, values in before['uv'].items()))
check('retained_materials', np.array_equal(after['material_assignments'], before['material_assignments'][fmap]))
check('retained_weights', after['weights'] == [before['weights'][index] for index in vmap])
check('retained_colors', before['colors'].keys() == after['colors'].keys() and all(
    domain == after['colors'][name][0] and np.array_equal(values[vmap if domain == 'POINT' else lmap], after['colors'][name][1])
    for name, (domain, values) in before['colors'].items()))
normal_delta = float(np.max(np.abs(after['normals']-before['normals'][lmap])))
check('retained_normals', normal_delta <= profile['normal_tolerance'], maximum_channel_delta=normal_delta)
check('connected_texture_pixels', before['images'] == after['images'])
check('bone_names', before['bone_names'] == after['bone_names'])
matrix_delta = float(np.max(np.abs(after['matrices']-before['matrices'])))
check('all_frame_bone_transforms', matrix_delta <= profile['matrix_tolerance'], maximum_delta=matrix_delta, frames=end-start+1)
surface_delta = max(float(np.linalg.norm(after['surfaces'][frame]-before['surfaces'][frame][vmap], axis=1).max()) for frame in samples)
check('retained_evaluated_surfaces', surface_delta <= profile['surface_tolerance_m'], maximum_delta_m=surface_delta, frames=samples)
old_faces = before['faces'][mapping['removed_source_triangles']]
corners = before['vertices'][old_faces].astype(np.float64)
removed_area = np.linalg.norm(np.cross(corners[:, 1]-corners[:, 0], corners[:, 2]-corners[:, 0]), axis=1)/2
check('removed_faces_exact_zero_area', bool(np.all(removed_area == 0)), count=len(removed_area))
check('correspondence_complete', len(np.unique(vmap)) == len(after['vertices']) and len(np.unique(fmap)) == len(after['faces'])
      and len(before['faces'])-len(after['faces']) == profile['expected_removed_triangles']
      and len(before['vertices'])-len(after['vertices']) == profile['expected_removed_vertices'])
report = dict(status='passed-targeted-retained-geometry-checks' if all(c['status']=='passed' for c in checks) else 'needs-review',
              source_sha256=repair['source_sha256'], candidate_sha256=hashlib.sha256(candidate.read_bytes()).hexdigest(),
              profile_sha256=hashlib.sha256(profile_path.read_bytes()).hexdigest(),
              correspondence_sha256=hashlib.sha256((folder/'source-correspondence.npz').read_bytes()).hexdigest(), cpu_only=True,
              checks=checks, limitations=['Fresh exchanges are a separate required check.',
                                         'Duplicate/internal/nonmanifold geometry and inherited motion failures are not repaired by this check.'])
(out/'validation.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report), flush=True)
if any(c['status'] != 'passed' for c in checks):
    raise SystemExit(2)
