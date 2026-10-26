"""CPU depth-tested review renders for proposed added feature-guide paths.

Paths are shown against the frozen rest mesh. This is not a motion or semantic
acceptance check; the source blend is opened read-only and never saved.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
import numpy as np
from mathutils import Vector


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True)
    parser.add_argument('--guide-report', required=True)
    parser.add_argument('--coverage-report', required=True)
    parser.add_argument('--assembly-root', required=True,
                        help='Exact source object defining guide assembly coordinates.')
    parser.add_argument('--guide', action='append', required=True,
                        help='Exact added-guide name; repeat for each guide to render.')
    parser.add_argument('--per-guide', action='store_true',
                        help='Render each requested guide in its own paired view set.')
    parser.add_argument('--output', required=True)
    return parser.parse_args(sys.argv[sys.argv.index('--') + 1:])


def add_curve(name, coords, material, bevel_depth, cyclic=False):
    curve_data = bpy.data.curves.new(name, 'CURVE')
    curve_data.dimensions = '3D'
    curve_data.resolution_u = 1
    curve_data.bevel_depth = bevel_depth
    curve_data.bevel_resolution = 2
    spline = curve_data.splines.new('POLY')
    spline.points.add(len(coords) - 1)
    for point, coord in zip(spline.points, coords):
        point.co = (*coord, 1.0)
    spline.use_cyclic_u = cyclic
    obj = bpy.data.objects.new(name, curve_data)
    bpy.context.scene.collection.objects.link(obj)
    obj.data.materials.append(material)
    return obj


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


def add_root_marker(name, position, radius, material):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=8, radius=radius,
                                         location=position)
    marker = bpy.context.object
    marker.name = name
    marker.data.materials.append(material)
    return marker


def add_area_light(name, center, direction, extent, energy_scale, size_scale):
    light_data = bpy.data.lights.new(name, 'AREA')
    light_data.energy = energy_scale * (extent / 3.0) ** 2
    light_data.shape = 'DISK'
    light_data.size = size_scale * extent
    light = bpy.data.objects.new(name, light_data)
    bpy.context.scene.collection.objects.link(light)
    direction = Vector(direction).normalized()
    light.location = center + direction * extent * 1.8
    light.rotation_euler = (center - light.location).to_track_quat('-Z', 'Y').to_euler()
    return light


def add_label(camera, text, material):
    data = bpy.data.curves.new('Review label data', 'FONT')
    data.body = text
    data.size = min(0.025, camera.data.ortho_scale * 0.02,
                    camera.data.ortho_scale * 0.72 / max(len(text) * 0.75, 1.0))
    data.extrude = 0
    label = bpy.data.objects.new('Review label', data)
    bpy.context.scene.collection.objects.link(label)
    label.parent = camera
    margin = camera.data.ortho_scale * 0.045
    label.location = (-camera.data.ortho_scale * 0.5 + margin,
                      camera.data.ortho_scale * 0.34, -1.0)
    label.rotation_euler = (0.0, 0.0, 0.0)
    label.data.materials.append(material)
    return label


def main():
    args = parse_args()
    source = Path(args.source).resolve()
    guide_path = Path(args.guide_report).resolve()
    coverage_path = Path(args.coverage_report).resolve()
    out = Path(args.output).resolve()
    if out.exists():
        raise FileExistsError(f'Output already exists; choose a new folder: {out}')

    guide_report = json.loads(guide_path.read_text(encoding='utf-8'))
    coverage_report = json.loads(coverage_path.read_text(encoding='utf-8'))
    expected_source = guide_report['source_sha256']
    if sha256(source) != expected_source or coverage_report['source_sha256'] != expected_source:
        raise ValueError('Source hash mismatch between source, guide report, and coverage report')
    if guide_report['mesh_hash'] != coverage_report['mesh_hash']:
        raise ValueError('Mesh hash mismatch between guide and coverage reports')
    additional_path = coverage_path.parent / 'additional-guides.json'
    if sha256(additional_path) != coverage_report['additional_guides_sha256']:
        raise ValueError('additional-guides.json hash does not match coverage report')
    coverage_npz = coverage_path.parent / 'coverage.npz'
    if sha256(coverage_npz) != coverage_report['coverage_file_sha256']:
        raise ValueError('coverage.npz hash does not match coverage report')
    guide_input_hashes = coverage_report.get('input_sha256', {})
    guide_input_matches = [value for key, value in guide_input_hashes.items()
                           if Path(key.replace('\\', '/')).name.lower() == guide_path.name.lower()]
    if guide_input_matches and (len(guide_input_matches) != 1 or guide_input_matches[0] != sha256(guide_path)):
        raise ValueError('Guide report does not match the hash bound by coverage input_sha256')

    additions = json.loads(additional_path.read_text(encoding='utf-8'))
    by_name = {item['name']: item for item in additions}
    if len(args.guide) != len(set(args.guide)):
        raise ValueError('Guide names must be unique')
    missing = sorted(set(args.guide) - set(by_name))
    if missing:
        raise ValueError(f'Guide name(s) not found in additional-guides.json: {missing}')
    selected = [by_name[name] for name in args.guide]

    bpy.ops.wm.open_mainfile(filepath=str(source))
    scene = bpy.context.scene
    if not math.isclose(scene.unit_settings.scale_length, 1.0, rel_tol=0.0, abs_tol=1e-9):
        raise ValueError('Unsupported unit scale: metre adapter requires scene scale_length == 1')
    scene.frame_set(1)
    bpy.context.view_layer.update()
    mesh = bpy.data.objects.get(guide_report['object'])
    if mesh is None or mesh.type != 'MESH':
        raise ValueError('Guide report object is missing or is not a mesh')
    assembly = bpy.data.objects.get(args.assembly_root)
    if assembly is None:
        raise ValueError(f'Declared assembly root is missing: {args.assembly_root}')
    ancestor = mesh
    while ancestor is not None and ancestor != assembly:
        ancestor = ancestor.parent
    if ancestor != assembly:
        raise ValueError('Declared assembly root is not an ancestor of the guide mesh')
    root_matrix = np.array(assembly.matrix_world, dtype=np.float64)
    assembly_scale = assembly.matrix_world.to_scale()
    root_linear = root_matrix[:3, :3]
    if not np.isfinite(root_matrix).all():
        raise ValueError('Assembly-root transform contains non-finite values')
    if not np.allclose(root_linear.T @ root_linear, np.eye(3), rtol=0.0, atol=1e-6):
        raise ValueError('Assembly root must have a rigid unit-scale transform')
    if not math.isclose(float(np.linalg.det(root_linear)), 1.0, rel_tol=0.0, abs_tol=1e-6):
        raise ValueError('Assembly root must be a proper rigid transform without reflection')
    for modifier in mesh.modifiers:
        if modifier.type != 'ARMATURE':
            raise ValueError(f'Unsupported modifier in rest review: {modifier.type}')
        if modifier.object:
            modifier.object.data.pose_position = 'REST'

    bpy.context.view_layer.update()
    mesh.data.calc_loop_triangles()
    local = np.array([vertex.co[:] for vertex in mesh.data.vertices], dtype=np.float32)
    faces = np.array([tuple(triangle.vertices) for triangle in mesh.data.loop_triangles], dtype=np.int32)
    actual_mesh_hash = hashlib.sha256(local.tobytes() + faces.tobytes()).hexdigest()
    if actual_mesh_hash != guide_report['mesh_hash']:
        raise ValueError('Loaded rest mesh hash does not match guide report')
    selection_path = Path(guide_report['selection_path'])
    if not selection_path.is_absolute():
        selection_path = Path.cwd() / selection_path
    selection_path = selection_path.resolve()
    if sha256(selection_path) != guide_report['selection_sha256']:
        raise ValueError('Selection report hash does not match guide report')
    selection_report = json.loads(selection_path.read_text(encoding='utf-8'))
    if (selection_report['source_sha256'] != expected_source
            or selection_report['mesh_hash'] != guide_report['mesh_hash']):
        raise ValueError('Selection report source or mesh hash mismatch')
    selection_npz = selection_path.parent / selection_report['selection_file']
    with np.load(selection_npz, allow_pickle=False) as selection_data:
        canonical = selection_data['canonical_node'].astype(np.int64, copy=False)
        unique_world = selection_data['unique_world_vertices'].astype(np.float64, copy=False)
    if canonical.shape != (len(local),) or canonical.min() < 0 or canonical.max() >= len(unique_world):
        raise ValueError('Selection canonical-node correspondence has an invalid shape or index')
    relative = np.array(assembly.matrix_world.inverted() @ mesh.matrix_world, dtype=np.float64)
    expected_world = local.astype(np.float64) @ relative[:3, :3].T + relative[:3, 3]
    frame_delta = np.linalg.norm(unique_world[canonical] - expected_world, axis=1)
    maximum_selection_frame_delta_m = float(frame_delta.max(initial=0.0))
    if not math.isfinite(maximum_selection_frame_delta_m) or maximum_selection_frame_delta_m > 1e-6:
        raise ValueError(f'Selection-to-assembly frame mismatch: {maximum_selection_frame_delta_m} m')

    # Guide point coordinates are assembly-local metres; map them through the
    # Explicitly selected assembly root, not an intermediate scale/orientation parent.
    world_guides = []
    for item in selected:
        if item.get('status') != 'unreviewed-coverage-proposal':
            raise ValueError(f"Unexpected review status for {item['name']}: {item.get('status')}")
        points = np.asarray(item['points'], dtype=np.float64)
        if points.ndim != 2 or points.shape[1] != 3 or len(points) < 2 or not np.isfinite(points).all():
            raise ValueError(f"Invalid point path for {item['name']}")
        world = [assembly.matrix_world @ Vector(point.tolist()) for point in points]
        world_guides.append((item, world))

    for obj in scene.objects:
        if obj != mesh:
            obj.hide_render = True
    bpy.context.view_layer.update()
    mesh.hide_render = False

    # Replace source materials only in memory with neutral, non-metallic clay.
    clay = bpy.data.materials.new('Diagnostic | neutral clay, not asset material')
    clay.diffuse_color = (0.38, 0.38, 0.38, 1.0)
    clay.use_nodes = True
    principled = clay.node_tree.nodes.get('Principled BSDF')
    principled.inputs['Base Color'].default_value = (0.32, 0.32, 0.32, 1.0)
    principled.inputs['Roughness'].default_value = 0.82
    mesh.data.materials.clear()
    mesh.data.materials.append(clay)
    for polygon in mesh.data.polygons:
        polygon.material_index = 0

    bounds = np.array([tuple(mesh.matrix_world @ Vector(corner)) for corner in mesh.bound_box])
    mesh_min, mesh_max = bounds.min(axis=0), bounds.max(axis=0)
    mesh_center = Vector((mesh_min + mesh_max) * 0.5)
    mesh_extent = float(np.linalg.norm(mesh_max - mesh_min))
    if not math.isfinite(mesh_extent) or mesh_extent <= 0:
        raise ValueError('Invalid mesh bounds')
    line_radius = max(mesh_extent * 0.0008, 0.0005)
    root_radius = line_radius * 2.5
    root_material = emission_material('Diagnostic | guide root', (0.95, 0.08, 0.06))
    label_material = emission_material('Diagnostic | view label', (1.0, 1.0, 1.0))
    path_materials = []
    for guide_index, (item, _) in enumerate(world_guides):
        color = (1.0, 0.48, 0.02) if guide_index % 2 == 0 else (0.02, 0.72, 1.0)
        path_materials.append(emission_material(f"Diagnostic | path {item['name']}", color))

    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'CPU'
    scene.cycles.samples = 4
    scene.cycles.seed = 0
    scene.cycles.use_denoising = False
    scene.render.threads_mode = 'FIXED'
    scene.render.threads = 4
    scene.render.resolution_x = 768
    scene.render.resolution_y = 640
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.film_transparent = False
    if scene.world is None:
        scene.world = bpy.data.worlds.new('Guide path review world')
    scene.world.use_nodes = True
    world_background = scene.world.node_tree.nodes.get('Background')
    world_background.inputs['Color'].default_value = (0.18, 0.18, 0.18, 1.0)
    world_background.inputs['Strength'].default_value = 0.45
    scene.view_settings.view_transform = 'Standard'
    scene.view_settings.look = 'None'
    scene.view_settings.exposure = 0.0
    scene.view_settings.gamma = 1.0

    camera_data = bpy.data.cameras.new('Guide path review camera')
    camera_data.type = 'ORTHO'
    camera = bpy.data.objects.new('Guide path review camera', camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera

    add_area_light('Review key | area', mesh_center, (1.0, -1.5, 2.0), mesh_extent,
                   36.0, 0.8)
    add_area_light('Review fill | area', mesh_center, (-1.0, -1.0, 0.6), mesh_extent,
                   18.0, 1.0)
    add_area_light('Review rim | area', mesh_center, (0.0, 1.5, 1.5), mesh_extent,
                   28.0, 0.7)

    views = [('side-posX', Vector((1.0, 0.0, 0.0))),
             ('side-negX', Vector((-1.0, 0.0, 0.0))),
             ('rear', Vector((0.0, 1.0, 0.0)))]
    out.mkdir(parents=True, exist_ok=False)
    (out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
    renders = []
    labels = []
    for item, _ in world_guides:
        labels.append({'name': item['name'], 'feature': item['feature'],
                       'root_node': item['root_node'], 'status': item['status'],
                       'source_path_nodes': item['source_path_nodes'],
                       'virtual_edges_in_path': item.get('virtual_edges_in_path', [])})

    guide_groups = [[entry] for entry in world_guides] if args.per_guide else [world_guides]
    aspect = scene.render.resolution_x / scene.render.resolution_y
    for group in guide_groups:
        guide_names = [item['name'] for item, _ in group]
        path_objects = []
        all_points = []
        for item, points in group:
            guide_index = args.guide.index(item['name'])
            path_objects.append(add_curve(f"Proposed guide | {item['name']}", points,
                                          path_materials[guide_index], line_radius))
            path_objects.append(add_root_marker(f"Root marker | {item['name']}", points[0],
                                                root_radius, root_material))
            all_points.extend(points)

        for view_name, direction in views:
            camera.rotation_euler = (-direction).to_track_quat('-Z', 'Y').to_euler()
            view_rotation = camera.rotation_euler.to_matrix()
            right = view_rotation @ Vector((1.0, 0.0, 0.0))
            up = view_rotation @ Vector((0.0, 1.0, 0.0))
            projected = np.array([[point.dot(right), point.dot(up)] for point in all_points])
            low, high = projected.min(axis=0), projected.max(axis=0)
            projected_span = high - low
            framing_pad_m = max(float(projected_span.max()) * 0.35,
                                mesh_extent * 0.025)
            needed_width = float(projected_span[0]) + 2.0 * framing_pad_m
            needed_height = float(projected_span[1]) + 2.0 * framing_pad_m
            ortho_scale = max(needed_width, needed_height * aspect, line_radius * 20.0)
            projected_center = (low + high) * 0.5
            target = right * float(projected_center[0]) + up * float(projected_center[1])
            camera_data.ortho_scale = ortho_scale
            camera.location = target + direction * max(mesh_extent * 1.5, 1.0)
            camera.rotation_euler = (target - camera.location).to_track_quat('-Z', 'Y').to_euler()
            framing = {'guide_names': guide_names, 'view': view_name,
                       'framing_pad_m': framing_pad_m,
                       'orthographic_width_m': ortho_scale,
                       'projected_guide_bounds_m': {'min': low.tolist(), 'max': high.tolist()},
                       'target_world_m': list(target)}
            safe_names = '+'.join(''.join(ch if ch.isalnum() or ch in '-_' else '_' for ch in n)
                                  for n in guide_names)
            for isolated in (False, True):
                mesh.hide_render = isolated
                label_text = ('ISOLATED ONLY | ' if isolated else 'REST | ') + safe_names
                label = add_label(camera, label_text, label_material)
                path = out / f"{safe_names}-{'isolated' if isolated else 'rest'}-{view_name}.png"
                scene.render.filepath = str(path)
                bpy.ops.render.render(write_still=True)
                renders.append({**framing, 'path': path.name,
                                'isolated_guide_only': isolated,
                                'depth_tested_against_mesh': not isolated,
                                'label': label_text})
                bpy.data.objects.remove(label, do_unlink=True)
        for obj in path_objects:
            bpy.data.objects.remove(obj, do_unlink=True)

    report = {
        'schema_version': 1,
        'status': 'unreviewed-guide-path-visualization',
        'cpu_only': True,
        'source': str(source),
        'source_sha256': expected_source,
        'tool': str(Path(__file__).resolve()),
        'tool_sha256': sha256(Path(__file__).resolve()),
        'mesh_hash': actual_mesh_hash,
        'guide_report': str(guide_path),
        'guide_report_sha256': sha256(guide_path),
        'coverage_bound_guide_report_sha256': guide_input_matches[0] if guide_input_matches else None,
        'coverage_report': str(coverage_path),
        'coverage_report_sha256': sha256(coverage_path),
        'additional_guides': str(additional_path),
        'additional_guides_sha256': sha256(additional_path),
        'coverage_file_sha256': sha256(coverage_npz),
        'selection_report': str(selection_path),
        'selection_report_sha256': sha256(selection_path),
        'selection_file': str(selection_npz),
        'selection_file_sha256': sha256(selection_npz),
        'selection_to_assembly_max_delta_m': maximum_selection_frame_delta_m,
        'assembly_root': assembly.name,
        'assembly_root_world_matrix': [list(row) for row in assembly.matrix_world],
        'assembly_root_scale': [float(value) for value in assembly_scale],
        'unit_scale': scene.unit_settings.scale_length,
        'guides': labels,
        'render_settings': {'engine': 'CYCLES', 'device': 'CPU', 'samples': 4,
                            'threads': 4, 'resolution': [768, 640]},
        'per_guide': args.per_guide,
        'lighting': {'world_color_linear': [0.18, 0.18, 0.18], 'world_strength': 0.45,
                     'area_lights': ['Review key | area', 'Review fill | area', 'Review rim | area'],
                     'area_light_energy_scales_w_at_3m_extent': [36.0, 18.0, 28.0],
                     'material': 'Neutral diffuse clay override; source materials untouched'},
        'renders': renders,
        'limitations': [
            'Added guides are unreviewed proposals; these images do not approve clump identity, semantic attachment, or root suitability.',
            'Rest-mesh views are depth-tested; isolated-guide views intentionally omit the mesh and do not show occlusion.',
            'Curves are centerlines with a diagnostic display thickness, not authored strand width or skin representation.',
            'No motion, deformation, collision, material, export/reimport, or runtime acceptance is established.',
            'Source blend was opened read-only; materials and diagnostic objects existed only in memory.'
        ]
    }
    (out / 'render-report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
