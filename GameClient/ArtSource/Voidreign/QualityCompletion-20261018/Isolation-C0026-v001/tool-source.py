"""CPU Cycles multiview inspection of an explicit mesh, with optional mask views.

Read-only source; creates a new output directory. This is inspection, not an
automatic semantic segmentation or an acceptance decision.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Vector

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', required=True)
parser.add_argument('--object', required=True)
parser.add_argument('--output', required=True)
parser.add_argument('--resolution', type=int, default=960)
parser.add_argument('--samples', type=int, default=12)
parser.add_argument('--attribute', action='append', default=[])
parser.add_argument('--selection-report', help='Source-hashed selection report for semantic/root review views')
parser.add_argument('--component-inspection', help='Source-hashed component inventory for depth-tested ID closeups')
parser.add_argument('--component', type=int)
parser.add_argument('--component-set', help='Explicit source/mesh-hashed component ID set for depth-tested review')
parser.add_argument('--isolate-selected', action='store_true', help='Additional same-camera ID views without surrounding geometry')
parser.add_argument('--view', action='append', choices=['side-posX', 'side-negX', 'front', 'rear', 'top'])
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
assert 256 <= args.resolution <= 2048 and 4 <= args.samples <= 128
source = Path(args.source).resolve()
out = Path(args.output).resolve()
out.mkdir(parents=True, exist_ok=False)
(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
bpy.ops.wm.open_mainfile(filepath=str(source))
scene = bpy.context.scene
scene.frame_set(1)
obj = bpy.data.objects[args.object]
assert obj.type == 'MESH'
for modifier in obj.modifiers:
    if modifier.type == 'ARMATURE':
        modifier.object.data.pose_position = 'REST'
for other in scene.objects:
    if other.type in {'MESH', 'CURVE', 'FONT', 'SURFACE', 'META', 'LIGHT'}:
        other.hide_render = other != obj
bpy.context.view_layer.update()
points = [obj.matrix_world @ Vector(p) for p in obj.bound_box]
lo = Vector(tuple(min(p[i] for p in points) for i in range(3)))
hi = Vector(tuple(max(p[i] for p in points) for i in range(3)))
center = (lo + hi) / 2
extent = hi - lo
component_mask = None
if args.component_inspection:
    inspection_path = Path(args.component_inspection).resolve()
    inspection = json.loads(inspection_path.read_text())
    assert inspection['source_sha256'] == hashlib.sha256(source.read_bytes()).hexdigest()
    entry = next(e for e in inspection['objects'] if e['object'] == obj.name)
    inspected = np.load(inspection_path.parent / entry['selection_file'])
    if args.component_set:
        component_set_path = Path(args.component_set).resolve()
        selected = json.loads(component_set_path.read_text())
        assert selected['source_sha256'] == inspection['source_sha256']
        assert selected['mesh_hash'] == entry['mesh_hash']
        indices = [row['component_index'] for row in entry['components'] if row['id'] in selected['component_ids']]
        assert len(indices) == len(selected['component_ids'])
        component_mask = np.isin(inspected['component_per_vertex'], indices)
        (out/'component-set.json').write_bytes(component_set_path.read_bytes())
    else:
        assert args.component is not None
        component_mask = inspected['component_per_vertex'] == args.component
    selected_points = inspected['vertices'][component_mask]
    assert len(selected_points)
    center = Vector((selected_points.min(0)+selected_points.max(0))/2)
    span = max(float(np.ptp(selected_points, axis=0).max())+.35, .5)
    extent = Vector((span, span, span))
distance = max(extent) * 3
scene.render.engine = 'CYCLES'
scene.cycles.device = 'CPU'
scene.cycles.samples = args.samples
scene.cycles.use_denoising = True
if hasattr(scene.cycles, 'denoising_use_gpu'):
    scene.cycles.denoising_use_gpu = False
scene.render.resolution_x = args.resolution
scene.render.resolution_y = args.resolution
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.render.film_transparent = False
scene.world = bpy.data.worlds.new('Semantic inspection world')
scene.world.use_nodes = True
scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.32, .34, .38, 1)
scene.world.node_tree.nodes['Background'].inputs[1].default_value = .65
scene.view_settings.view_transform = 'AgX'
scene.view_settings.exposure = .8
for name, offset, power, size in [('Key', (3, -4, 5), 1300, 5),
                                  ('Fill', (-4, 1, 3), 1100, 4),
                                  ('Top', (0, 2, 6), 900, 3)]:
    data = bpy.data.lights.new('Inspection ' + name, 'AREA')
    data.energy, data.shape, data.size = power, 'DISK', size
    light = bpy.data.objects.new(data.name, data)
    scene.collection.objects.link(light)
    light.location = center + Vector(offset)
    light.rotation_euler = (center-light.location).to_track_quat('-Z', 'Y').to_euler()
camera_data = bpy.data.cameras.new('Semantic inspection')
camera = bpy.data.objects.new(camera_data.name, camera_data)
scene.collection.objects.link(camera)
camera_data.type = 'ORTHO'
camera_data.ortho_scale = max(extent) * 1.12
scene.camera = camera
views = {'side-posX': (1, 0, 0), 'side-negX': (-1, 0, 0),
         'front': (0, -1, 0), 'rear': (0, 1, 0), 'top': (0, 0, 1)}
if args.view:
    views = {name: views[name] for name in args.view}
original_materials = list(obj.data.materials)
variants = [('materials', None)]
isolated = None
if component_mask is not None:
    colors = np.tile([.12, .13, .15, 1.], (len(component_mask), 1)).astype(np.float32)
    colors[component_mask] = [.95, .035, .015, 1]
    attribute = obj.data.color_attributes.new(name='Component ID', type='FLOAT_COLOR', domain='POINT')
    attribute.data.foreach_set('color', colors.ravel())
    material = bpy.data.materials.new('Depth-tested component ID')
    material.use_nodes = True
    nodes, links = material.node_tree.nodes, material.node_tree.links
    nodes.clear()
    color = nodes.new('ShaderNodeVertexColor')
    color.layer_name = attribute.name
    emission = nodes.new('ShaderNodeEmission')
    output = nodes.new('ShaderNodeOutputMaterial')
    links.new(color.outputs['Color'], emission.inputs[0])
    links.new(emission.outputs[0], output.inputs['Surface'])
    variants.append(('depth-tested-component', material))
    if args.isolate_selected:
        selected_indices = np.flatnonzero(component_mask)
        remap = np.full(len(component_mask), -1, dtype=np.int32)
        remap[selected_indices] = np.arange(len(selected_indices))
        isolated_mesh = bpy.data.meshes.new('Inspection-only isolated component')
        isolated_mesh.from_pydata([obj.data.vertices[int(i)].co[:] for i in selected_indices], [],
            [[int(remap[i]) for i in face.vertices] for face in obj.data.polygons if all(component_mask[i] for i in face.vertices)])
        isolated = bpy.data.objects.new('Inspection-only isolated component', isolated_mesh)
        scene.collection.objects.link(isolated)
        isolated.matrix_world = obj.matrix_world.copy()
        isolated.hide_render = True
        flat = bpy.data.materials.new('Isolated component ID')
        flat.use_nodes = True
        nodes, links = flat.node_tree.nodes, flat.node_tree.links
        nodes.clear()
        emission = nodes.new('ShaderNodeEmission')
        emission.inputs[0].default_value = (.95, .035, .015, 1)
        output = nodes.new('ShaderNodeOutputMaterial')
        links.new(emission.outputs[0], output.inputs['Surface'])
        isolated_mesh.materials.append(flat)
        variants.append(('isolated-component', None))
elif args.isolate_selected:
    raise ValueError('Isolation requires an explicit source-hashed component inventory/selection')
if args.selection_report:
    selection_path = Path(args.selection_report).resolve()
    selection = json.loads(selection_path.read_text())
    assert selection['source_sha256'] == hashlib.sha256(source.read_bytes()).hexdigest()
    assert selection['object'] == obj.name
    obj.data.calc_loop_triangles()
    local = np.array([v.co[:] for v in obj.data.vertices], dtype=np.float32)
    tris = np.array([tuple(t.vertices) for t in obj.data.loop_triangles], dtype=np.int32)
    assert hashlib.sha256(local.tobytes()+tris.tobytes()).hexdigest() == selection['mesh_hash']
    selected = np.load(selection_path.parent / selection['selection_file'])
    labels = selected['labels']
    assert len(labels) == len(obj.data.vertices)
    for label, roots_only in [('semantic-proposal', False), ('roots-and-unresolved', True)]:
        colors = np.tile([.12, .13, .15, 1.], (len(labels), 1)).astype(np.float32)
        colors[selected['explicit_protected']] = [.12, .20, .36, 1]
        colors[labels == 1] = [.02, .65, .62, 1]
        colors[labels == 2] = [.90, .25, .025, 1]
        if roots_only:
            colors[selected['root_nodes']] = [.9, .9, .9, 1]
            colors[(labels > 0) & ~np.isfinite(selected['root_distance_m'])] = [.9, .015, .06, 1]
        attribute = obj.data.color_attributes.new(name=label, type='FLOAT_COLOR', domain='POINT')
        attribute.data.foreach_set('color', colors.ravel())
        material = bpy.data.materials.new(label)
        material.use_nodes = True
        nodes, links = material.node_tree.nodes, material.node_tree.links
        nodes.clear()
        color = nodes.new('ShaderNodeVertexColor')
        color.layer_name = label
        emission = nodes.new('ShaderNodeEmission')
        output = nodes.new('ShaderNodeOutputMaterial')
        links.new(color.outputs['Color'], emission.inputs[0])
        links.new(emission.outputs[0], output.inputs['Surface'])
        variants.append((label, material))
if args.attribute:
    mask = bpy.data.materials.new('Semantic inspection masks')
    mask.use_nodes = True
    nodes, links = mask.node_tree.nodes, mask.node_tree.links
    nodes.clear()
    result = nodes.new('ShaderNodeEmission')
    result.inputs[0].default_value = (.12, .13, .15, 1)
    result.inputs[1].default_value = 1
    base = None
    for index, name in enumerate(args.attribute):
        assert obj.data.color_attributes.get(name), name
        attr = nodes.new('ShaderNodeVertexColor')
        attr.layer_name = name
        mix = nodes.new('ShaderNodeMixRGB')
        mix.inputs[1].default_value = (.12, .13, .15, 1)
        mix.inputs[2].default_value = [(0.02, .65, .62, 1), (.9, .25, .025, 1)][index % 2]
        links.new(attr.outputs['Color'], mix.inputs[0])
        if base:
            links.new(base, mix.inputs[1])
        base = mix.outputs[0]
    links.new(base, result.inputs[0])
    output = nodes.new('ShaderNodeOutputMaterial')
    links.new(result.outputs[0], output.inputs['Surface'])
    variants.append(('current-weights', mask))
records = []
for variant, material in variants:
    obj.hide_render = variant == 'isolated-component'
    if isolated:
        isolated.hide_render = variant != 'isolated-component'
    for index in range(len(obj.data.materials)):
        obj.data.materials[index] = material or original_materials[index]
    for name, axis in views.items():
        camera.location = center + Vector(axis) * distance
        camera.rotation_euler = (center-camera.location).to_track_quat('-Z', 'Y').to_euler()
        scene.render.filepath = str(out / (variant + '-' + name + '.png'))
        bpy.ops.render.render(write_still=True)
        assert Path(scene.render.filepath).is_file(), 'Render missed requested output path'
        records.append(dict(variant=variant, view=name, image=Path(scene.render.filepath).name,
                            camera_world=[list(row) for row in camera.matrix_world],
                            ortho_scale=camera_data.ortho_scale))
        print('Inspection view complete:', variant, name, flush=True)
report = dict(source=str(source), source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
              tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              object=obj.name, blender=bpy.app.version_string, cpu_only=True,
              engine='Cycles', device='CPU', samples=args.samples,
              rest_pose=True, attributes=args.attribute, views=records,
              component_set_sha256=hashlib.sha256(Path(args.component_set).read_bytes()).hexdigest() if args.component_set else None,
              isolated_selected=args.isolate_selected,
              scope='Inspection renders only; no source edits or semantic acceptance')
(out / 'views.json').write_text(json.dumps(report, indent=2))
