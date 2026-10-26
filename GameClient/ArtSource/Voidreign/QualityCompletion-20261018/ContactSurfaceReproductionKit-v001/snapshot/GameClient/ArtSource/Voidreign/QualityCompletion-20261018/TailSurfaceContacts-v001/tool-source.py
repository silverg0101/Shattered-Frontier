"""Build a source-bound analytic surface-contact corrective in Blender nodes.

Vertex projection is a diagnostic contact corrector, not a replacement dynamics
solver. Full motion, attachments, style/strain and exchange gates still apply.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import shutil
import sys
import bpy
import numpy as np


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile', required=True)
p.add_argument('--output', required=True)
a = p.parse_args(sys.argv[sys.argv.index('--') + 1:])
profile_path, out = Path(a.profile).resolve(), Path(a.output).resolve()
cfg = json.loads(profile_path.read_text())
assert cfg['schema_version'] == 1 and not out.exists()
assert cfg['scope'] == 'diagnostic-actual-surface-contact-projection'
assert type(cfg['projection_passes']) is int and 1 <= cfg['projection_passes'] <= 8
for f, expected in cfg['input_sha256'].items():
    assert sha(f) == expected, f
source = Path(cfg['source'])
raw = json.loads(Path(cfg['motion_profile']).read_text())
surface = json.loads(Path(cfg['surface_report']).read_text())
surface_policy = json.loads(Path(cfg['surface_profile']).read_text())
assert surface['source_sha256'] == sha(source)
assert surface['profile_sha256'] == sha(cfg['surface_profile'])
with np.load(cfg['surface_review'], allow_pickle=False) as d:
    free = d['free_surface_mask']
assert free.dtype.kind == 'b'
original_mask_count = int(free.sum())
if cfg.get('corrective_feature_labels'):
    assert set(cfg['corrective_feature_labels']) <= {1, 2}
    with np.load(cfg['selection_archive'], allow_pickle=False) as selection:
        labels = selection['labels']
    assert labels.shape == free.shape
    free = free & np.isin(labels, cfg['corrective_feature_labels'])
bpy.ops.wm.open_mainfile(filepath=str(source.resolve()))
scene, obj = bpy.context.scene, bpy.data.objects[cfg['object']]
scene.frame_set(1)
obj.data.calc_loop_triangles()
local = np.array([v.co[:] for v in obj.data.vertices], np.float32)
faces = np.array([t.vertices[:] for t in obj.data.loop_triangles], np.int32)
assert hashlib.sha256(local.tobytes() + faces.tobytes()).hexdigest() == cfg['mesh_hash']
assert len(free) == len(local)
scale = np.array(obj.matrix_world.to_scale())
assert np.isfinite(scale).all() and scale.min() > 0 and np.allclose(scale, scale[0], atol=1e-6)
world_scale = float(scale[0])
root = bpy.data.objects[cfg['assembly_root']]
margin = surface_policy['limits']['collision_margin_m']
assert 0 <= margin <= .02
helper_collection = bpy.data.collections.new('Surface contact helpers | analytic frozen layout')
scene.collection.children.link(helper_collection)


def empty(name, position, space):
    ob = bpy.data.objects.new(name, None)
    helper_collection.objects.link(ob)
    ob.empty_display_size = .01
    ob.hide_render = True
    if space != 'world':
        ob.parent = root
    ob.location = position
    if space == 'hand':
        base = float(position[0])
        for frame in range(1, surface_policy['limits']['frame_count'] + 1):
            t = (frame - 1) / scene.render.fps
            ob.location.x = base - raw['settings']['hand_sweep_distance_m'] * math.sin(math.pi * min(1., max(0., (t - 9) / 2)))
            ob.keyframe_insert('location', frame=frame)
        for layer in ob.animation_data.action.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    for curve in bag.fcurves:
                        for key in curve.keyframe_points:
                            key.interpolation = 'LINEAR'
    return ob


capsules = []
for spec in raw['colliders']:
    assert spec['space'] in ['body', 'hand', 'world'] and spec['radius_m'] > 0
    assert np.linalg.norm(np.array(spec['end_m']) - spec['start_m']) > 1e-6
    capsules.append((empty('Contact | ' + spec['label'] + ' | A', spec['start_m'], spec['space']),
                     empty('Contact | ' + spec['label'] + ' | B', spec['end_m'], spec['space']),
                     (spec['radius_m'] + margin) / world_scale))
ground_objects = [empty('Contact | ground ' + name, point, 'world') for name, point in
    [('O', [0, 0, raw['settings']['ground_height_m']]),
     ('X', [1, 0, raw['settings']['ground_height_m']]),
     ('Y', [0, 1, raw['settings']['ground_height_m']])]]
assert obj.data.attributes.get('SurfaceContactFree') is None
attribute = obj.data.attributes.new('SurfaceContactFree', 'BOOLEAN', 'POINT')
attribute.data.foreach_set('value', free)
tree = bpy.data.node_groups.new('Analytic actual-surface contacts | diagnostic', 'GeometryNodeTree')
tree.interface.new_socket(name='Geometry', in_out='INPUT', socket_type='NodeSocketGeometry')
tree.interface.new_socket(name='Geometry', in_out='OUTPUT', socket_type='NodeSocketGeometry')
nodes, links = tree.nodes, tree.links
input_node, output_node = nodes.new('NodeGroupInput'), nodes.new('NodeGroupOutput')
position = nodes.new('GeometryNodeInputPosition').outputs['Position']
selected = nodes.new('GeometryNodeInputNamedAttribute')
selected.data_type = 'BOOLEAN'
selected.inputs['Name'].default_value = attribute.name


def vector(op, a, b=None):
    n = nodes.new('ShaderNodeVectorMath')
    n.operation = op
    links.new(a, n.inputs[0])
    if b is not None:
        links.new(b, n.inputs[1])
    return n.outputs['Value'] if op in ['DOT_PRODUCT', 'LENGTH', 'DISTANCE'] else n.outputs['Vector']


def math_node(op, a=None, b=None, av=None, bv=None):
    n = nodes.new('ShaderNodeMath')
    n.operation = op
    if a is not None:
        links.new(a, n.inputs[0])
    elif av is not None:
        n.inputs[0].default_value = av
    if b is not None:
        links.new(b, n.inputs[1])
    elif bv is not None:
        n.inputs[1].default_value = bv
    return n.outputs[0]


def scaled(v, factor):
    n = nodes.new('ShaderNodeVectorMath')
    n.operation = 'SCALE'
    links.new(v, n.inputs[0])
    links.new(factor, n.inputs['Scale'])
    return n.outputs['Vector']


def location(ob):
    n = nodes.new('GeometryNodeObjectInfo')
    n.transform_space = 'RELATIVE'
    n.inputs['Object'].default_value = ob
    return n.outputs['Location']


def set_position(geometry, target, label):
    n = nodes.new('GeometryNodeSetPosition')
    n.label = label
    links.new(geometry, n.inputs['Geometry'])
    links.new(target, n.inputs['Position'])
    links.new(selected.outputs['Attribute'], n.inputs['Selection'])
    return n.outputs['Geometry']


capsule_fields = [(location(a), location(b), radius) for a, b, radius in capsules]
ground_o, ground_x, ground_y = [location(o) for o in ground_objects]
ground_n = vector('NORMALIZE', vector('CROSS_PRODUCT', vector('SUBTRACT', ground_x, ground_o), vector('SUBTRACT', ground_y, ground_o)))
geometry = input_node.outputs['Geometry']
for iteration in range(cfg['projection_passes']):
    for index, (start, end, radius) in enumerate(capsule_fields):
        axis = vector('SUBTRACT', end, start)
        delta = vector('SUBTRACT', position, start)
        u = math_node('DIVIDE', vector('DOT_PRODUCT', delta, axis), vector('DOT_PRODUCT', axis, axis))
        u = math_node('MINIMUM', math_node('MAXIMUM', u, bv=0), bv=1)
        closest = vector('ADD', start, scaled(axis, u))
        radial = vector('SUBTRACT', position, closest)
        amount = math_node('MAXIMUM', math_node('SUBTRACT', b=vector('LENGTH', radial), av=radius), bv=0)
        corrected = vector('ADD', position, scaled(vector('NORMALIZE', radial), amount))
        geometry = set_position(geometry, corrected, f'Pass{iteration + 1} | {raw["colliders"][index]["label"]}')
    distance = vector('DOT_PRODUCT', vector('SUBTRACT', position, ground_o), ground_n)
    push = math_node('MAXIMUM', math_node('SUBTRACT', b=distance, av=margin / world_scale), bv=0)
    geometry = set_position(geometry, vector('ADD', position, scaled(ground_n, push)), f'Pass{iteration + 1} | Ground')
links.new(geometry, output_node.inputs['Geometry'])
modifier = obj.modifiers.new('Actual surface contacts | diagnostic projection', 'NODES')
modifier.node_group = tree
scene['Surface contact status'] = 'Diagnostic analytic projection; motion, attachments, style and source/export checks pending'
scene.frame_set(1)
out.mkdir(parents=True)
cache_copies = []
for cache in cfg.get('cache_directories', []):
    folder = Path(cache['path']).resolve()
    assert folder.parent == source.parent.resolve() and folder.is_dir()
    target = out / ('blendcache_Voidreign-SurfaceContacts' + cache.get('suffix', ''))
    assert not target.exists()
    for name, expected in cache['files'].items():
        assert Path(name).name == name and sha(folder / name) == expected
    shutil.copytree(folder, target)
    for name, expected in cache['files'].items():
        assert sha(target / name) == expected
    cache_copies.append(dict(source=str(folder), output=str(target), files=cache['files']))
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
(out / 'profile.json').write_bytes(profile_path.read_bytes())
bpy.ops.wm.save_as_mainfile(filepath=str(out / 'Voidreign-SurfaceContacts.blend'))
for f, expected in cfg['input_sha256'].items():
    assert sha(f) == expected, f
report = dict(schema_version=1, status='surface-contact-corrective-needs-checks', cpu_only=True,
    input_sha256=cfg['input_sha256'], tool_sha256=sha(__file__), profile_sha256=sha(profile_path),
    source_sha256=sha(source), candidate_sha256=sha(out / 'Voidreign-SurfaceContacts.blend'),
    mesh_hash=cfg['mesh_hash'], selected_free_vertices=int(free.sum()), projection_passes=cfg['projection_passes'],
    original_free_mask_vertices=original_mask_count, corrective_feature_labels=cfg.get('corrective_feature_labels'),
    copied_source_caches=cache_copies,
    collision_margin_m=margin, source_uniform_world_scale=world_scale, node_count=len(nodes),
    limitations=['Vertex-only instantaneous projection; no impulses, inertia, swept/self/triangle contacts or physical stability guarantee.',
        'No source topology/UV/texture/weight change; original free/root exemption mask retained exactly.',
        'May introduce stretch, pin/attachment conflicts or side-switching jumps; all full-duration checks and visual review remain required.',
        'Blender procedural diagnostic; animated exchange needs a separate explicit bake/import contract.'])
(out / 'corrective-report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report), flush=True)
