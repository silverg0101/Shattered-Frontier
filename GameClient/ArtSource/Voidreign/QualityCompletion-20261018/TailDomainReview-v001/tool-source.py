"""CPU views of selected actual-mesh vertices and their source-connected patches.

The tool reads a frozen source and profile, highlights source topology only,
and never saves a Blender file or changes mesh geometry/weights.
"""
import argparse
import hashlib
import heapq
import json
import math
from pathlib import Path
import sys

import bpy
import numpy as np
from mathutils import Vector


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def resolve_profile_path(value):
    path = Path(value)
    return (path if path.is_absolute() else Path.cwd() / path).resolve()


def checked_id_array(array, name, upper_bound):
    values = np.asarray(array)
    if values.ndim != 1 or values.size == 0 or not np.issubdtype(values.dtype, np.integer):
        raise ValueError(f'{name} must be a nonempty one-dimensional integer array')
    values = values.astype(np.int64, copy=False)
    if np.unique(values).size != values.size:
        raise ValueError(f'{name} must contain unique IDs')
    if values.min() < 0 or values.max() >= upper_bound:
        raise ValueError(f'{name} contains IDs outside [0, {upper_bound})')
    return values


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True)
    parser.add_argument('--profile', required=True)
    parser.add_argument('--case', action='append', required=True,
                        help='Case id from profile; repeat for each case to render.')
    parser.add_argument('--output', required=True)
    return parser.parse_args(sys.argv[sys.argv.index('--') + 1:])


def emission_material(name, color):
    material = bpy.data.materials.new(name)
    material.diffuse_color = (*color, 1.0)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    nodes.clear()
    output = nodes.new('ShaderNodeOutputMaterial')
    emission = nodes.new('ShaderNodeEmission')
    emission.inputs['Color'].default_value = (*color, 1.0)
    material.node_tree.links.new(emission.outputs[0], output.inputs['Surface'])
    return material


def add_area_light(name, center, direction, extent, energy_scale, size):
    data = bpy.data.lights.new(name, 'AREA')
    data.shape = 'DISK'
    data.energy = energy_scale * (extent / 3.0) ** 2
    data.size = size
    obj = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(obj)
    direction = Vector(direction).normalized()
    obj.location = center + direction * extent * 2.0
    obj.rotation_euler = (center - obj.location).to_track_quat('-Z', 'Y').to_euler()
    return obj


def geodesic_nodes(start, edges, points, radius):
    adjacency = [[] for _ in range(len(points))]
    for left, right in edges:
        left, right = int(left), int(right)
        if left == right:
            continue
        weight = float(np.linalg.norm(points[left] - points[right]))
        if not math.isfinite(weight):
            raise ValueError('Non-finite geometric edge length in selection data')
        adjacency[left].append((right, weight))
        adjacency[right].append((left, weight))
    distances = {int(start): 0.0}
    queue = [(0.0, int(start))]
    while queue:
        distance, node = heapq.heappop(queue)
        if distance != distances.get(node):
            continue
        if distance > radius:
            continue
        for neighbor, cost in adjacency[node]:
            candidate = distance + cost
            if candidate <= radius and candidate < distances.get(neighbor, float('inf')):
                distances[neighbor] = candidate
                heapq.heappush(queue, (candidate, neighbor))
    return distances


def make_patch_mesh(name, vertices, polygons, include_vertex):
    faces = [tuple(poly.vertices) for poly in polygons
             if any(include_vertex[int(index)] for index in poly.vertices)]
    if not faces:
        raise ValueError(f'No source faces selected for {name}')
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update(calc_edges=True)
    return mesh, len(faces)


def add_label(camera, text, material):
    data = bpy.data.curves.new('Vertex review label data', 'FONT')
    data.body = text
    scale = camera.data.ortho_scale
    data.size = min(0.025, scale * 0.02,
                    scale * 0.72 / max(len(text) * 0.75, 1.0))
    obj = bpy.data.objects.new('Vertex review label', data)
    bpy.context.scene.collection.objects.link(obj)
    obj.parent = camera
    margin = scale * 0.045
    obj.location = (-scale * 0.5 + margin, scale * 0.34, -1.0)
    obj.data.materials.append(material)
    return obj


def main():
    args = parse_args()
    source = Path(args.source).resolve()
    profile_path = Path(args.profile).resolve()
    out = Path(args.output).resolve()
    if out.exists():
        raise FileExistsError(f'Output already exists; choose a new folder: {out}')
    profile = json.loads(profile_path.read_text(encoding='utf-8'))
    if sha256(source) != profile['source_sha256']:
        raise ValueError('Source hash does not match profile')
    cases_by_id = {case['id']: case for case in profile['cases']}
    if len(args.case) != len(set(args.case)):
        raise ValueError('Case ids must be unique')
    unknown = sorted(set(args.case) - set(cases_by_id))
    if unknown:
        raise ValueError(f'Unknown case id(s): {unknown}')
    cases = [cases_by_id[case_id] for case_id in args.case]

    surface_path = resolve_profile_path(profile['surface_report'])
    if sha256(surface_path) != profile['surface_report_sha256']:
        raise ValueError('Surface report hash does not match profile')
    surface_report = json.loads(surface_path.read_text(encoding='utf-8'))
    if surface_report['source_sha256'] != profile['source_sha256']:
        raise ValueError('Surface report source hash mismatch')

    selection_path = resolve_profile_path(profile['selection_report'])
    if sha256(selection_path) != profile['selection_report_sha256']:
        raise ValueError('Selection report hash does not match profile')
    selection_report = json.loads(selection_path.read_text(encoding='utf-8'))
    if (selection_report['mesh_hash'] != profile['mesh_hash']
            or selection_report['object'] != profile['object']):
        raise ValueError('Selection report mesh or object does not match profile')
    retention_path = None
    retention_report = None
    if 'retention_report' in profile or 'retention_report_sha256' in profile:
        if not {'retention_report', 'retention_report_sha256'}.issubset(profile):
            raise ValueError('Retention report path and SHA-256 must be provided together')
        retention_path = resolve_profile_path(profile['retention_report'])
        if sha256(retention_path) != profile['retention_report_sha256']:
            raise ValueError('Retention report hash does not match profile')
        retention_report = json.loads(retention_path.read_text(encoding='utf-8'))
        checks = retention_report.get('checks')
        if (retention_report.get('source_sha256') != selection_report['source_sha256']
                or retention_report.get('candidate_sha256') != profile['source_sha256']
                or not isinstance(checks, dict) or not checks
                or not all(value is True for value in checks.values())):
            raise ValueError('Retention report does not establish all-true source-to-candidate retention')
    elif selection_report['source_sha256'] != profile['source_sha256']:
        raise ValueError('Selection report source differs from candidate; a bound retention report is required')

    domain_archive_path = None
    if 'domain_archive' in profile or 'domain_archive_sha256' in profile:
        if not {'domain_archive', 'domain_archive_sha256'}.issubset(profile):
            raise ValueError('Domain archive path and SHA-256 must be provided together')
        domain_archive_path = resolve_profile_path(profile['domain_archive'])
        if sha256(domain_archive_path) != profile['domain_archive_sha256']:
            raise ValueError('Domain archive hash does not match profile')
    if any(case.get('domain_prefix') is not None for case in cases) and domain_archive_path is None:
        raise ValueError('A case specifies domain_prefix but no bound domain_archive is configured')
    selection_npz = selection_path.parent / selection_report['selection_file']
    if not selection_npz.is_file():
        raise FileNotFoundError(selection_npz)

    bpy.ops.wm.open_mainfile(filepath=str(source))
    scene = bpy.context.scene
    if not math.isclose(scene.unit_settings.scale_length, 1.0, rel_tol=0.0, abs_tol=1e-9):
        raise ValueError('Unsupported unit scale: metre adapter requires scene scale_length == 1')
    scene.frame_set(1)
    bpy.context.view_layer.update()
    mesh_obj = bpy.data.objects.get(profile['object'])
    assembly = bpy.data.objects.get(profile['assembly_root'])
    if mesh_obj is None or mesh_obj.type != 'MESH' or assembly is None:
        raise ValueError('Profile mesh object or assembly root is missing')
    ancestor = mesh_obj
    while ancestor is not None and ancestor != assembly:
        ancestor = ancestor.parent
    if ancestor != assembly:
        raise ValueError('Assembly root is not an ancestor of the profile mesh object')
    root_matrix = np.array(assembly.matrix_world, dtype=np.float64)
    root_linear = root_matrix[:3, :3]
    if not np.isfinite(root_matrix).all():
        raise ValueError('Assembly-root transform contains non-finite values')
    if (not np.allclose(root_linear.T @ root_linear, np.eye(3), rtol=0.0, atol=1e-6)
            or not math.isclose(float(np.linalg.det(root_linear)), 1.0, rel_tol=0.0, abs_tol=1e-6)):
        raise ValueError('Assembly root must have a finite proper rigid unit-scale transform')

    mesh_obj.data.calc_loop_triangles()
    raw_rest = np.array([vertex.co[:] for vertex in mesh_obj.data.vertices], dtype=np.float32)
    triangles = np.array([tuple(triangle.vertices) for triangle in mesh_obj.data.loop_triangles], dtype=np.int32)
    actual_mesh_hash = hashlib.sha256(raw_rest.tobytes() + triangles.tobytes()).hexdigest()
    if actual_mesh_hash != profile['mesh_hash']:
        raise ValueError('Loaded source mesh hash does not match profile')
    with np.load(selection_npz, allow_pickle=False) as data:
        canonical = data['canonical_node'].astype(np.int64, copy=False)
        unique_world = data['unique_world_vertices'].astype(np.float64, copy=False)
        geometric_edges = data['edges'].astype(np.int64, copy=False)
    domain_data = {}
    if domain_archive_path is not None:
        with np.load(domain_archive_path, allow_pickle=False) as data:
            for case in cases:
                prefix = case.get('domain_prefix')
                if prefix is None:
                    continue
                if not isinstance(prefix, str) or not prefix:
                    raise ValueError(f"Invalid domain_prefix for {case['id']}")
                vertex_key = prefix + '_vertex_ids'
                face_key = prefix + '_face_ids'
                if vertex_key not in data.files or face_key not in data.files:
                    raise ValueError(f'Domain archive is missing {vertex_key!r} or {face_key!r}')
                vertex_ids = checked_id_array(data[vertex_key], vertex_key, len(raw_rest))
                face_ids = checked_id_array(data[face_key], face_key, len(triangles))
                selected_triangles = triangles[face_ids]
                face_vertex_ids = np.unique(selected_triangles.reshape(-1))
                if not np.array_equal(np.sort(vertex_ids), face_vertex_ids):
                    raise ValueError(f'{prefix} vertex IDs do not equal the union of its selected source triangles')
                if int(case['vertex']) not in set(vertex_ids.tolist()):
                    raise ValueError(f"Failed vertex {case['vertex']} is outside the explicit domain {prefix}")
                domain_data[case['id']] = {
                    'prefix': prefix,
                    'vertex_ids': vertex_ids,
                    'face_ids': face_ids
                }
    if canonical.shape != (len(raw_rest),) or canonical.min() < 0 or canonical.max() >= len(unique_world):
        raise ValueError('Selection canonical-node correspondence has invalid shape or indices')
    if geometric_edges.ndim != 2 or geometric_edges.shape[1] != 2:
        raise ValueError('Selection geometric edges have an invalid shape')
    relative = np.array(assembly.matrix_world.inverted() @ mesh_obj.matrix_world, dtype=np.float64)
    rest_assembly = raw_rest.astype(np.float64) @ relative[:3, :3].T + relative[:3, 3]
    frame_delta = np.linalg.norm(unique_world[canonical] - rest_assembly, axis=1)
    max_selection_frame_delta_m = float(frame_delta.max(initial=0.0))
    if not math.isfinite(max_selection_frame_delta_m) or max_selection_frame_delta_m > 1e-6:
        raise ValueError(f'Selection-to-assembly frame mismatch: {max_selection_frame_delta_m} m')

    for modifier in mesh_obj.modifiers:
        if modifier.type != 'ARMATURE':
            raise ValueError(f'Unsupported modifier for index-preserving review: {modifier.type}')
    armatures = [modifier.object for modifier in mesh_obj.modifiers
                 if modifier.type == 'ARMATURE' and modifier.object is not None]
    if not armatures:
        raise ValueError('No armature modifier is available for posed review')

    patch_radius_m = float(profile['patch_radius_m'])
    context_pad_m = float(profile['context_pad_m'])
    if not math.isfinite(patch_radius_m) or patch_radius_m <= 0:
        raise ValueError('patch_radius_m must be finite and positive')
    if not math.isfinite(context_pad_m) or context_pad_m <= 0:
        raise ValueError('context_pad_m must be finite and positive')

    # Diagnostic display materials replace source slots only in memory.
    clay = bpy.data.materials.new('Vertex review | neutral clay')
    clay.diffuse_color = (0.32, 0.32, 0.32, 1.0)
    clay.use_nodes = True
    bsdf = clay.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (0.32, 0.32, 0.32, 1.0)
    bsdf.inputs['Roughness'].default_value = 0.78
    highlight = bpy.data.materials.new('Vertex review | selected source patch')
    highlight.diffuse_color = (0.98, 0.28, 0.025, 1.0)
    highlight.use_nodes = True
    patch_bsdf = highlight.node_tree.nodes.get('Principled BSDF')
    patch_bsdf.inputs['Base Color'].default_value = (0.98, 0.24, 0.015, 1.0)
    patch_bsdf.inputs['Roughness'].default_value = 0.65
    marker_mat = emission_material('Vertex review | selected vertex', (0.05, 0.85, 1.0))
    label_mat = emission_material('Vertex review | label', (1.0, 1.0, 1.0))
    mesh_obj.data.materials.clear()
    mesh_obj.data.materials.append(clay)
    mesh_obj.data.materials.append(highlight)

    for other in scene.objects:
        if other != mesh_obj:
            other.hide_render = True
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'CPU'
    scene.cycles.samples = 4
    scene.cycles.seed = 0
    scene.cycles.use_denoising = True
    scene.render.threads_mode = 'FIXED'
    scene.render.threads = 4
    scene.render.resolution_x = 768
    scene.render.resolution_y = 640
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.film_transparent = False
    if scene.world is None:
        scene.world = bpy.data.worlds.new('Vertex review world')
    scene.world.use_nodes = True
    background = scene.world.node_tree.nodes.get('Background')
    background.inputs['Color'].default_value = (0.18, 0.18, 0.18, 1.0)
    background.inputs['Strength'].default_value = 0.45
    scene.view_settings.view_transform = 'Standard'
    scene.view_settings.look = 'None'
    scene.view_settings.exposure = 0.0
    scene.view_settings.gamma = 1.0

    camera_data = bpy.data.cameras.new('Vertex review camera')
    camera_data.type = 'ORTHO'
    camera = bpy.data.objects.new('Vertex review camera', camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera
    views = [('side-posX', Vector((1.0, 0.0, 0.0))),
             ('side-negX', Vector((-1.0, 0.0, 0.0)))]
    aspect = scene.render.resolution_x / scene.render.resolution_y
    out.mkdir(parents=True, exist_ok=False)
    (out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
    renders = []
    case_results = []

    for case in cases:
        vertex_id = int(case['vertex'])
        if not 0 <= vertex_id < len(raw_rest):
            raise ValueError(f"Vertex id outside source mesh for {case['id']}: {vertex_id}")
        explicit_domain = domain_data.get(case['id'])
        if explicit_domain is None:
            distances = geodesic_nodes(canonical[vertex_id], geometric_edges,
                                       unique_world, patch_radius_m)
            canonical_patch = np.zeros(len(unique_world), dtype=bool)
            canonical_patch[list(distances)] = True
            vertex_patch = canonical_patch[canonical]
            selected_nodes = len(distances)
            selected_vertices = int(np.count_nonzero(vertex_patch))
            selected_face_ids = None
            selected_triangle_faces = None
            selection_mode = 'canonical-geodesic-radius'
        else:
            vertex_patch = np.zeros(len(raw_rest), dtype=bool)
            vertex_patch[explicit_domain['vertex_ids']] = True
            selected_nodes = int(np.unique(canonical[explicit_domain['vertex_ids']]).size)
            selected_vertices = int(explicit_domain['vertex_ids'].size)
            selected_face_ids = explicit_domain['face_ids']
            selected_triangle_faces = [tuple(int(index) for index in face)
                                       for face in triangles[selected_face_ids]]
            selection_mode = 'profile-bound-explicit-triangle-domain'
        if selected_vertices == 0:
            raise ValueError(f"Empty source-connected patch for {case['id']}")
        for polygon in mesh_obj.data.polygons:
            polygon.material_index = (1 if explicit_domain is None
                                      and any(vertex_patch[int(index)] for index in polygon.vertices)
                                      else 0)

        case_state_results = []
        for state in ('rest', 'posed'):
            for armature in armatures:
                armature.data.pose_position = 'REST' if state == 'rest' else 'POSE'
            frame = 1 if state == 'rest' else int(case['frame'])
            scene.frame_set(frame)
            bpy.context.view_layer.update()
            depsgraph = bpy.context.evaluated_depsgraph_get()
            evaluated = mesh_obj.evaluated_get(depsgraph)
            evaluated_mesh = evaluated.to_mesh()
            if len(evaluated_mesh.vertices) != len(raw_rest):
                evaluated.to_mesh_clear()
                raise ValueError('Evaluated mesh changed vertex count; source vertex IDs are not preserved')
            vertex_local = evaluated_mesh.vertices[vertex_id].co.copy()
            chosen_world = evaluated.matrix_world @ vertex_local
            actual_world = np.array(chosen_world[:], dtype=np.float64)
            expected_world = np.asarray(case['expected_world_position_m'], dtype=np.float64)
            position_error = float(np.linalg.norm(actual_world - expected_world)) if state == 'posed' else None
            if state == 'posed' and (not math.isfinite(position_error) or position_error > 1e-6):
                evaluated.to_mesh_clear()
                raise ValueError(f"{case['id']} posed vertex mismatch at frame {frame}: {position_error} m")

            evaluated_vertices = np.array([tuple(vertex.co) for vertex in evaluated_mesh.vertices], dtype=np.float64)
            evaluated_world = evaluated_vertices @ np.array(evaluated.matrix_world, dtype=np.float64)[:3, :3].T
            evaluated_world += np.array(evaluated.matrix_world, dtype=np.float64)[:3, 3]
            patch_points = evaluated_world[vertex_patch]
            if len(patch_points) == 0:
                evaluated.to_mesh_clear()
                raise ValueError(f"No evaluated patch points for {case['id']} / {state}")
            if selected_triangle_faces is not None:
                patch_faces = selected_triangle_faces
            else:
                patch_faces = [tuple(poly.vertices) for poly in evaluated_mesh.polygons
                               if any(vertex_patch[int(index)] for index in poly.vertices)]
            if not patch_faces:
                evaluated.to_mesh_clear()
                raise ValueError(f"No evaluated patch faces for {case['id']} / {state}")
            patch_mesh = bpy.data.meshes.new(f"Patch display | {case['id']} | {state}")
            patch_mesh.from_pydata(evaluated_vertices.tolist(), [], patch_faces)
            patch_mesh.update(calc_edges=True)
            patch_obj = bpy.data.objects.new(f"Isolated patch | {case['id']} | {state}", patch_mesh)
            scene.collection.objects.link(patch_obj)
            patch_obj.matrix_world = evaluated.matrix_world.copy()
            patch_obj.data.materials.append(highlight)
            evaluated.to_mesh_clear()

            marker_radius = patch_radius_m * 0.06
            bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=8,
                                                 radius=marker_radius, location=chosen_world)
            marker = bpy.context.object
            marker.name = f"Selected source vertex | {case['id']} | {state}"
            marker.data.materials.append(marker_mat)

            # Full-mesh and isolated-patch views use the same camera transform.
            bounds = np.asarray(patch_points, dtype=np.float64)
            patch_center = Vector((bounds.min(axis=0) + bounds.max(axis=0)) * 0.5)
            display_extent = float(np.linalg.norm(bounds.max(axis=0) - bounds.min(axis=0)))
            if not math.isfinite(display_extent):
                raise ValueError('Non-finite selected patch bounds')
            case_state_result = {
                'state': state, 'frame': frame, 'vertex_id': vertex_id,
                'actual_world_position_m': actual_world.tolist(),
                'expected_world_position_m': expected_world.tolist() if state == 'posed' else None,
                'posed_world_position_error_m': position_error,
                'selection_mode': selection_mode,
                'selected_canonical_node_count': selected_nodes,
                'selected_source_vertex_count': selected_vertices,
                'highlighted_face_count': len(patch_faces),
                'patch_radius_m': patch_radius_m,
                'context_pad_m': context_pad_m,
                'patch_center_world_m': list(patch_center)
            }
            case_state_results.append(case_state_result)

            for view_name, direction in views:
                camera.rotation_euler = (-direction).to_track_quat('-Z', 'Y').to_euler()
                view_rotation = camera.rotation_euler.to_matrix()
                right = view_rotation @ Vector((1.0, 0.0, 0.0))
                up = view_rotation @ Vector((0.0, 1.0, 0.0))
                projected = np.array([[Vector(point).dot(right), Vector(point).dot(up)]
                                      for point in bounds])
                low, high = projected.min(axis=0), projected.max(axis=0)
                projected_span = high - low
                width = float(projected_span[0]) + 2.0 * context_pad_m
                height = float(projected_span[1]) + 2.0 * context_pad_m
                ortho_width = max(width, height * aspect, marker_radius * 20.0)
                # Projected extents determine scale, but target must retain
                # the patch center's view-axis coordinate as well. Omitting
                # it can place a camera behind patches far from world origin.
                target = patch_center.copy()
                camera_data.ortho_scale = ortho_width
                camera_distance = max(display_extent + context_pad_m * 4.0 + 0.5, 0.75)
                camera.location = target + direction * camera_distance
                camera.rotation_euler = (target - camera.location).to_track_quat('-Z', 'Y').to_euler()
                camera_data.clip_start = 0.01
                camera_data.clip_end = camera_distance + display_extent + context_pad_m * 4.0 + 1.0
                bpy.context.view_layer.update()
                camera_inverse = camera.matrix_world.inverted()
                camera_points = np.array([tuple(camera_inverse @ Vector(point)) for point in bounds])
                depth = -camera_points[:, 2]
                projected_x = 0.5 + camera_points[:, 0] / ortho_width
                projected_y = 0.5 + camera_points[:, 1] / (ortho_width / aspect)
                if (not np.isfinite(camera_points).all() or not np.isfinite(depth).all()
                        or depth.min() <= camera_data.clip_start
                        or depth.max() >= camera_data.clip_end
                        or projected_x.min() < 0.0 or projected_x.max() > 1.0
                        or projected_y.min() < 0.0 or projected_y.max() > 1.0):
                    raise ValueError(f'Patch framing/clipping validation failed: {case["id"]} / {state} / {view_name}')
                framing = {
                    'patch_projected_bounds_normalized': {
                        'min_x': float(projected_x.min()), 'max_x': float(projected_x.max()),
                        'min_y': float(projected_y.min()), 'max_y': float(projected_y.max())},
                    'patch_camera_depth_min_m': float(depth.min()),
                    'patch_camera_depth_max_m': float(depth.max()),
                    'camera_clip_range_m': [float(camera_data.clip_start), float(camera_data.clip_end)],
                    'camera_distance_m': camera_distance
                }
                lights = [
                    add_area_light(f"Review key | {case['id']} | {state} | {view_name}",
                                   target, (1.0, -1.5, 2.0), ortho_width,
                                   36.0, max(ortho_width * 0.8, 0.1)),
                    add_area_light(f"Review fill | {case['id']} | {state} | {view_name}",
                                   target, (-1.0, -1.0, 0.6), ortho_width,
                                   18.0, max(ortho_width, 0.1)),
                    add_area_light(f"Review rim | {case['id']} | {state} | {view_name}",
                                   target, (0.0, 1.5, 1.5), ortho_width,
                                   28.0, max(ortho_width * 0.7, 0.1))
                ]
                label_base = f"{case['id']} | {state.upper()} | frame {frame}"
                for isolated in (False, True):
                    mesh_obj.hide_render = isolated
                    patch_obj.hide_render = not isolated
                    marker.hide_render = False
                    label_text = label_base + (' | PATCH ONLY' if isolated else ' | MESH + PATCH')
                    label = add_label(camera, label_text, label_mat)
                    filename = f"{case['id']}-{state}-{'patch-only' if isolated else 'mesh-patch'}-{view_name}.png"
                    image_path = out / filename
                    scene.render.filepath = str(image_path)
                    bpy.ops.render.render(write_still=True)
                    renders.append({
                        'path': filename, 'case_id': case['id'], 'state': state,
                        'frame': frame, 'vertex_id': vertex_id, 'view': view_name,
                        'isolated_patch_only': isolated,
                        'depth_tested_against_full_mesh': not isolated,
                        'camera_ortho_width_m': ortho_width,
                        'framing_pad_m': context_pad_m,
                        'camera_target_world_m': list(target),
                        **framing,
                        'label': label_text
                    })
                    bpy.data.objects.remove(label, do_unlink=True)
                for light in lights:
                    data = light.data
                    bpy.data.objects.remove(light, do_unlink=True)
                    bpy.data.lights.remove(data)

            scene.collection.objects.unlink(patch_obj)
            bpy.data.objects.remove(patch_obj, do_unlink=True)
            bpy.data.meshes.remove(patch_mesh)
            marker_mesh = marker.data
            bpy.data.objects.remove(marker, do_unlink=True)
            bpy.data.meshes.remove(marker_mesh)
            case_state_result['patch_bounds_world_m'] = {
                'min': bounds.min(axis=0).tolist(), 'max': bounds.max(axis=0).tolist()}
        case_results.append({
            'case_id': case['id'], 'label': case['label'], 'vertex_id': vertex_id,
            'failure_frame': int(case['frame']),
            'expected_failure_world_position_m': case['expected_world_position_m'],
            'observed_penetration_m': case.get('observed_penetration_m'),
            'selection_mode': selection_mode,
            'domain_prefix': explicit_domain['prefix'] if explicit_domain else None,
            'domain_vertex_ids': explicit_domain['vertex_ids'].tolist() if explicit_domain else None,
            'domain_face_ids': selected_face_ids.tolist() if selected_face_ids is not None else None,
            'selected_canonical_node_count': selected_nodes,
            'selected_source_vertex_count': selected_vertices,
            'states': case_state_results
        })

    report = {
        'schema_version': 1,
        'status': 'diagnostic-vertex-patch-views-only',
        'cpu_only': True,
        'source': str(source), 'source_sha256': sha256(source),
        'profile': str(profile_path), 'profile_sha256': sha256(profile_path),
        'surface_report': str(surface_path), 'surface_report_sha256': sha256(surface_path),
        'selection_report': str(selection_path), 'selection_report_sha256': sha256(selection_path),
        'selection_source_sha256': selection_report['source_sha256'],
        'selection_file': str(selection_npz), 'selection_file_sha256': sha256(selection_npz),
        'domain_archive': str(domain_archive_path) if domain_archive_path else None,
        'domain_archive_sha256': sha256(domain_archive_path) if domain_archive_path else None,
        'retention_report': str(retention_path) if retention_path else None,
        'retention_report_sha256': sha256(retention_path) if retention_path else None,
        'retention_status': retention_report.get('status') if retention_report else None,
        'retention_checks': retention_report.get('checks') if retention_report else None,
        'retention_source_sha256': retention_report.get('source_sha256') if retention_report else None,
        'retention_candidate_sha256': retention_report.get('candidate_sha256') if retention_report else None,
        'mesh_hash': actual_mesh_hash, 'assembly_root': assembly.name,
        'selection_to_assembly_max_delta_m': max_selection_frame_delta_m,
        'patch_radius_m': patch_radius_m, 'context_pad_m': context_pad_m,
        'selection_strategy': 'Per-case canonical geodesic radius or explicit profile-bound source triangle domain.',
        'connectivity': 'Radius mode uses selection archive canonical_node + geometric edges only; explicit domain mode uses listed source triangles and vertex IDs.',
        'cases': case_results,
        'render_settings': {'engine': 'CYCLES', 'device': 'CPU', 'samples': 4,
                            'threads': 4, 'resolution': [768, 640]},
        'renders': renders,
        'limitations': [
            'Highlighted patch is a topology-local visualization around the failed vertex, not a semantic classification.',
            'A highlighted hair, anatomy, or buried-shell appearance requires human review; no approval or deletion is implied.',
            'Rest and posed views show the same source faces; isolated views omit surrounding mesh and do not show occlusion.',
            'No source mesh, weights, material, rig, animation, export, or runtime data was changed or saved.'
        ]
    }
    (out / 'render-report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
