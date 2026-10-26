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
    parser.add_argument('--guide', action='append', required=True,
                        help='Exact added-guide name; repeat for each guide to render.')
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


def add_label(camera, text, material):
    data = bpy.data.curves.new('Review label data', 'FONT')
    data.body = text
    data.size = 0.035
    data.extrude = 0
    label = bpy.data.objects.new('Review label', data)
    bpy.context.scene.collection.objects.link(label)
    label.parent = camera
    label.location = (-0.47, -0.37, -1.0)
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
    mesh = bpy.data.objects.get(guide_report['object'])
    if mesh is None or mesh.type != 'MESH':
        raise ValueError('Guide report object is missing or is not a mesh')
    assembly = mesh
    while assembly.parent is not None:
        assembly = assembly.parent
    assembly_scale = assembly.matrix_world.to_scale()
    if (not all(math.isfinite(float(value)) and abs(float(value)) > 1e-9 for value in assembly_scale)
            or not all(math.isfinite(float(value)) for row in assembly.matrix_world for value in row)):
        raise ValueError(f'Invalid assembly-root transform for metre paths: {tuple(assembly_scale)}')
    for modifier in mesh.modifiers:
        if modifier.type != 'ARMATURE':
            raise ValueError(f'Unsupported modifier in rest review: {modifier.type}')
        if modifier.object:
            modifier.object.data.pose_position = 'REST'

    scene.frame_set(1)
    bpy.context.view_layer.update()
    mesh.data.calc_loop_triangles()
    local = np.array([vertex.co[:] for vertex in mesh.data.vertices], dtype=np.float32)
    faces = np.array([tuple(triangle.vertices) for triangle in mesh.data.loop_triangles], dtype=np.int32)
    actual_mesh_hash = hashlib.sha256(local.tobytes() + faces.tobytes()).hexdigest()
    if actual_mesh_hash != guide_report['mesh_hash']:
        raise ValueError('Loaded rest mesh hash does not match guide report')
    if sha256(guide_path) != guide_report.get('guide_report_sha256', sha256(guide_path)):
        raise ValueError('Guide report self-hash mismatch')

    # Guide point coordinates are assembly-local metres; map them through the
    # profile-declared assembly root, not an intermediate scale/orientation parent.
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
    line_radius = max(mesh_extent * 0.0012, 0.0005)
    root_radius = line_radius * 3.0
    path_material = emission_material('Diagnostic | proposed path', (1.0, 0.62, 0.04))
    root_material = emission_material('Diagnostic | guide root', (0.95, 0.08, 0.06))
    label_material = emission_material('Diagnostic | view label', (1.0, 1.0, 1.0))

    path_objects = []
    for item, points in world_guides:
        path_objects.append(add_curve(f"Proposed guide | {item['name']}", points,
                                      path_material, line_radius))
        path_objects.append(add_root_marker(f"Root marker | {item['name']}", points[0],
                                            root_radius, root_material))

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
    scene.world.color = (0.055, 0.055, 0.055)
    scene.view_settings.view_transform = 'Standard'
    scene.view_settings.look = 'None'
    scene.view_settings.exposure = 0.0
    scene.view_settings.gamma = 1.0

    camera_data = bpy.data.cameras.new('Guide path review camera')
    camera_data.type = 'ORTHO'
    camera = bpy.data.objects.new('Guide path review camera', camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera

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

    for view_name, direction in views:
        camera.rotation_euler = (-direction).to_track_quat('-Z', 'Y').to_euler()
        for isolated in (False, True):
            mesh.hide_render = isolated
            for obj in path_objects:
                obj.hide_render = False
            if isolated:
                coords = [p[:] for _, points in world_guides for p in points]
                low = np.min(coords, axis=0); high = np.max(coords, axis=0)
                target = Vector((low + high) * 0.5)
                fit_extent = float(np.linalg.norm(high - low))
                label_text = 'ISOLATED GUIDE PATHS | ' + ', '.join(args.guide)
            else:
                target = mesh_center
                fit_extent = mesh_extent
                label_text = 'REST MESH + PROPOSED PATHS | ' + ', '.join(args.guide)
            camera.location = target + direction * max(mesh_extent * 2.5, 1.0)
            camera.rotation_euler = (target - camera.location).to_track_quat('-Z', 'Y').to_euler()
            camera_data.ortho_scale = max(fit_extent * 1.22, line_radius * 20.0)
            label = add_label(camera, label_text, label_material)
            path = out / f"{'isolated-' if isolated else 'rest-'}{view_name}.png"
            scene.render.filepath = str(path)
            bpy.ops.render.render(write_still=True)
            renders.append({'path': path.name, 'view': view_name,
                            'isolated_guide_only': isolated,
                            'depth_tested_against_mesh': not isolated})
            bpy.data.objects.remove(label, do_unlink=True)

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
        'coverage_report': str(coverage_path),
        'coverage_report_sha256': sha256(coverage_path),
        'additional_guides': str(additional_path),
        'additional_guides_sha256': sha256(additional_path),
        'coverage_file_sha256': sha256(coverage_npz),
        'assembly_root': assembly.name,
        'assembly_root_world_matrix': [list(row) for row in assembly.matrix_world],
        'assembly_root_scale': [float(value) for value in assembly_scale],
        'unit_scale': scene.unit_settings.scale_length,
        'guides': labels,
        'render_settings': {'engine': 'CYCLES', 'device': 'CPU', 'samples': 4,
                            'threads': 4, 'resolution': [768, 640]},
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
