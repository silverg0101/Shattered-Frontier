"""CPU cloth contact pilot on a canonical copy of the actual hair surface.

Exact source-index sampling returns simulated points to the original mesh.
Proxy seam welding is simulation only; source topology/UVs/weights stay intact.
Offline Blender cache, not an engine-ready exchange or production acceptance.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
import time
import bpy
import numpy as np
from mathutils import Matrix, Vector


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def capsule_mesh(name, start, end, radius):
    start, end = Vector(start), Vector(end)
    axis = end - start
    rotation = Vector((0, 0, 1)).rotation_difference(axis.normalized())
    rings = []
    for phi in np.linspace(-math.pi / 2, 0, 9):
        rings.append((radius * math.sin(phi), radius * max(1e-6, math.cos(phi))))
    for phi in np.linspace(0, math.pi / 2, 9):
        rings.append((axis.length + radius * math.sin(phi), radius * max(1e-6, math.cos(phi))))
    vertices = [tuple(start + rotation @ Vector((r * math.cos(a), r * math.sin(a), z)))
                for z, r in rings for a in np.arange(32) * math.tau / 32]
    faces = []
    for ring in range(len(rings) - 1):
        for side in range(32):
            j = (side + 1) % 32
            faces.append((ring * 32 + side, ring * 32 + j, (ring + 1) * 32 + j, (ring + 1) * 32 + side))
    faces += [tuple(reversed(range(32))), tuple((len(rings) - 1) * 32 + j for j in range(32))]
    data = bpy.data.meshes.new(name)
    data.from_pydata(vertices, [], faces)
    data.update()
    obj = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(obj)
    return obj


p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile', required=True)
p.add_argument('--output', required=True)
a = p.parse_args(sys.argv[sys.argv.index('--') + 1:])
profile_path, out = Path(a.profile).resolve(), Path(a.output).resolve()
profile = json.loads(profile_path.read_text())
assert profile['schema_version'] == 1 and not out.exists()
assert profile['scope'] == 'finite-actual-surface-cloth-diagnostic'
assert profile['frame_start'] == 1 and 2 <= profile['frame_end'] <= 330
assert 1 <= len(profile['feature_labels']) <= 2 and set(profile['feature_labels']) <= {1, 2}
for f, expected in profile['input_sha256'].items():
    assert sha(f) == expected, f
out.mkdir(parents=True)
(out / 'profile.json').write_bytes(profile_path.read_bytes())
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
source = Path(profile['source'])
raw = json.loads(Path(profile['motion_profile']).read_text())
guide = json.loads(Path(profile['guide_report']).read_text())
attachments = json.loads(Path(guide['attachment_path']).read_text())
selection = np.load(profile['selection_archive'], allow_pickle=False)
canonical, labels = selection['canonical_node'], selection['labels']
review = np.load(profile['surface_review'], allow_pickle=False)
free = review['free_surface_mask']
assert free.shape == labels.shape == canonical.shape
bpy.ops.wm.open_mainfile(filepath=str(source.resolve()))
scene, obj = bpy.context.scene, bpy.data.objects[guide['object']]
scene.frame_set(1)
scene.frame_start, scene.frame_end = profile['frame_start'], profile['frame_end']
scene.render.threads_mode, scene.render.threads = 'FIXED', 4
scene.gravity = (0, 0, -raw['settings']['gravity_m_s2'])
obj.data.calc_loop_triangles()
local = np.array([v.co[:] for v in obj.data.vertices], np.float32)
triangles = np.array([v.vertices[:] for v in obj.data.loop_triangles], np.int32)
assert hashlib.sha256(local.tobytes() + triangles.tobytes()).hexdigest() == guide['mesh_hash']
assert all(m.type == 'ARMATURE' and not m.use_deform_preserve_volume for m in obj.modifiers)
arm = next(m.object for m in obj.modifiers)
root = bpy.data.objects[profile['assembly_root']]
_, first = np.unique(canonical, return_index=True)
chosen = np.isin(labels, profile['feature_labels'])
assert np.array_equal(chosen, chosen[first][canonical]), 'Source seam labels disagree'
chosen_nodes = np.flatnonzero(chosen[first])
lookup = np.full(len(first), -1, np.int32)
lookup[chosen_nodes] = np.arange(len(chosen_nodes))
representatives = first[chosen_nodes]
face_mask = chosen[triangles].all(axis=1)
proxy_faces = lookup[canonical[triangles[face_mask]]]
assert proxy_faces.min() >= 0
proxy_mesh = bpy.data.meshes.new('Actual source hair | canonical simulation surface')
relative = np.array(root.matrix_world.inverted() @ obj.matrix_world)
proxy_rest = local[representatives].astype(np.float64) @ relative[:3, :3].T + relative[:3, 3]
proxy_mesh.from_pydata(proxy_rest.tolist(), [], proxy_faces.tolist())
proxy_mesh.update()
proxy = bpy.data.objects.new('SIM | actual source hair surface', proxy_mesh)
scene.collection.objects.link(proxy)
proxy.parent = root
proxy.matrix_parent_inverse = Matrix.Identity(4)
proxy.matrix_basis = Matrix.Identity(4)
assert np.allclose(proxy.matrix_world.to_scale(), [1, 1, 1], atol=1e-6)
group_names = {g.index: g.name for g in obj.vertex_groups}
groups = {name: proxy.vertex_groups.new(name=name) for name in group_names.values()}
for index, old_id in enumerate(representatives):
    for g in obj.data.vertices[int(old_id)].groups:
        groups[group_names[g.group]].add([index], g.weight, 'REPLACE')
modifier = proxy.modifiers.new('Original skin targets', 'ARMATURE')
modifier.object = arm
bpy.context.view_layer.update()
dg = bpy.context.evaluated_depsgraph_get()
expected_obj, actual_proxy = obj.evaluated_get(dg), proxy.evaluated_get(dg)
expected_matrix, actual_matrix = np.array(expected_obj.matrix_world), np.array(actual_proxy.matrix_world)
expected_points = np.array([expected_obj.data.vertices[int(i)].co[:] for i in representatives]) @ expected_matrix[:3, :3].T + expected_matrix[:3, 3]
actual_points = np.array([v.co[:] for v in actual_proxy.data.vertices]) @ actual_matrix[:3, :3].T + actual_matrix[:3, 3]
preflight_error = float(np.linalg.norm(expected_points - actual_points, axis=1).max())
assert preflight_error < 1e-5, f'Source-to-unit-proxy skin frame mismatch: {preflight_error}'
pin = proxy.vertex_groups.new(name='Surface pins | original roots and proposed bindings')
exclude = proxy.vertex_groups.new(name='Contact exclusion | original tested mask complement')
pins = ~free[representatives]
for row in attachments['proposals']:
    for node in [row['parent_node'], row['child_node']]:
        if lookup[node] >= 0:
            pins[lookup[node]] = True
pin_ids = np.flatnonzero(pins).tolist()
exclude_ids = np.flatnonzero(~free[representatives]).tolist()
if pin_ids:
    pin.add(pin_ids, 1., 'REPLACE')
if exclude_ids:
    exclude.add(exclude_ids, 1., 'REPLACE')
cloth = proxy.modifiers.new('Actual-surface contact pilot | CPU', 'CLOTH')
cloth.settings.vertex_group_mass = pin.name
cloth.settings.use_dynamic_mesh = True
for k, v in profile['cloth_settings'].items():
    prop = cloth.settings.bl_rna.properties[k]
    assert prop.type in ['FLOAT', 'INT', 'BOOLEAN']
    if prop.type in ['FLOAT', 'INT']:
        assert prop.hard_min <= v <= prop.hard_max
    setattr(cloth.settings, k, v)
cloth.collision_settings.vertex_group_object_collisions = exclude.name
cloth.collision_settings.use_collision = True
cloth.collision_settings.use_self_collision = False
for k, v in profile['collision_settings'].items():
    prop = cloth.collision_settings.bl_rna.properties[k]
    if prop.type in ['FLOAT', 'INT']:
        assert prop.hard_min <= v <= prop.hard_max
    setattr(cloth.collision_settings, k, v)
# Create closed CPU collision proxies matching the frozen analytic layout.
collider_objects = []
for spec in raw['colliders']:
    collider = capsule_mesh('CONTACT | ' + spec['label'], spec['start_m'], spec['end_m'], spec['radius_m'])
    if spec['space'] != 'world':
        collider.parent = root
    if spec['space'] == 'hand':
        for frame in range(1, scene.frame_end + 1):
            t = (frame - 1) / scene.render.fps
            collider.location.x = -raw['settings']['hand_sweep_distance_m'] * math.sin(math.pi * min(1., max(0., (t - 9) / 2)))
            collider.keyframe_insert('location', frame=frame)
    collider_objects.append(collider)
bpy.ops.mesh.primitive_plane_add(size=100, location=(0, 0, raw['settings']['ground_height_m']))
ground = bpy.context.object
ground.name = 'CONTACT | analytic ground'
collider_objects.append(ground)
collision_collection = bpy.data.collections.new('Actual-surface pilot colliders')
scene.collection.children.link(collision_collection)
for collider in collider_objects:
    for collection in list(collider.users_collection):
        collection.objects.unlink(collider)
    collision_collection.objects.link(collider)
    collider.modifiers.new('Surface contact only', 'COLLISION')
    collider.collision.thickness_outer = profile['collider_thickness_outer_m']
    collider.collision.thickness_inner = 0.001
    collider.collision.cloth_friction = 0.0
    collider.hide_render = True
    collider.display_type = 'WIRE'
    collider.hide_set(False)
    assert collider.visible_get(), 'Collider must stay evaluated in the simulation view layer'
cloth.collision_settings.collection = collision_collection
proxy.hide_render = True
# Keep proxy evaluated for sampling while hiding its duplicate display.
proxy.display_type = 'WIRE'
index_attr = obj.data.attributes.new('SurfaceSimIndex', 'INT', 'POINT')
index_attr.data.foreach_set('value', lookup[canonical])
selected_attr = obj.data.attributes.new('SurfaceSimSelected', 'BOOLEAN', 'POINT')
selected_attr.data.foreach_set('value', chosen)
tree = bpy.data.node_groups.new('Actual surface sampling | source indices', 'GeometryNodeTree')
tree.interface.new_socket(name='Geometry', in_out='INPUT', socket_type='NodeSocketGeometry')
tree.interface.new_socket(name='Geometry', in_out='OUTPUT', socket_type='NodeSocketGeometry')
nodes, links = tree.nodes, tree.links
inp, output = nodes.new('NodeGroupInput'), nodes.new('NodeGroupOutput')
info = nodes.new('GeometryNodeObjectInfo')
info.transform_space = 'RELATIVE'
info.inputs['Object'].default_value = proxy
sample = nodes.new('GeometryNodeSampleIndex')
sample.data_type, sample.domain, sample.clamp = 'FLOAT_VECTOR', 'POINT', True
position = nodes.new('GeometryNodeInputPosition')
index = nodes.new('GeometryNodeInputNamedAttribute')
index.data_type = 'INT'
index.inputs['Name'].default_value = index_attr.name
selected = nodes.new('GeometryNodeInputNamedAttribute')
selected.data_type = 'BOOLEAN'
selected.inputs['Name'].default_value = selected_attr.name
set_position = nodes.new('GeometryNodeSetPosition')
links.new(info.outputs['Geometry'], sample.inputs['Geometry'])
links.new(position.outputs['Position'], sample.inputs['Value'])
links.new(index.outputs['Attribute'], sample.inputs['Index'])
links.new(inp.outputs['Geometry'], set_position.inputs['Geometry'])
links.new(selected.outputs['Attribute'], set_position.inputs['Selection'])
links.new(sample.outputs['Value'], set_position.inputs['Position'])
links.new(set_position.outputs['Geometry'], output.inputs['Geometry'])
sample_mod = obj.modifiers.new('Actual-surface simulated coordinates | diagnostic', 'NODES')
sample_mod.node_group = tree
pc = cloth.point_cache
pc.frame_start, pc.frame_end = scene.frame_start, scene.frame_end
pc.use_disk_cache = True
pc.name = 'ActualSurfacePilot'
scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=str(out / 'Voidreign-ActualSurfacePilot.blend'))
bpy.ops.object.select_all(action='DESELECT')
proxy.select_set(True)
bpy.context.view_layer.objects.active = proxy
started = time.monotonic()
with bpy.context.temp_override(object=proxy, active_object=proxy, point_cache=pc):
    bpy.ops.ptcache.bake(bake=True)
scene.frame_set(scene.frame_end)
evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
assert len(evaluated.data.vertices) == len(local)
result = np.array([v.co[:] for v in evaluated.data.vertices], np.float32)
assert np.isfinite(result).all()
for f, expected in profile['input_sha256'].items():
    assert sha(f) == expected, f
scene['Surface pilot status'] = 'Diagnostic CPU surface cloth; no full-duration/contact/import/runtime acceptance'
scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=str(out / 'Voidreign-ActualSurfacePilot.blend'))
report = dict(schema_version=1, status='baked-actual-surface-pilot-needs-checks', cpu_only=True,
    source_sha256=sha(source), mesh_hash=guide['mesh_hash'], profile_sha256=sha(profile_path), tool_sha256=sha(__file__),
    input_sha256=profile['input_sha256'], proxy_vertices=len(representatives), proxy_faces=len(proxy_faces),
    original_vertices=len(local), original_triangles=len(triangles), feature_labels=profile['feature_labels'],
    source_to_unit_proxy_max_error_m=preflight_error, proxy_unit_world_scale=list(proxy.matrix_world.to_scale()),
    pinned_proxy_vertices=int(pins.sum()), collision_excluded_vertices=len(exclude_ids),
    frame_start=scene.frame_start, frame_end=scene.frame_end, baked=pc.is_baked,
    elapsed_seconds=time.monotonic() - started, blender=bpy.app.version_string,
    source_output_sha256=sha(out / 'Voidreign-ActualSurfacePilot.blend'),
    limitations=['Offline Blender cloth cache and geometry-node source sampling, not an engine-compatible exchange.',
        'Canonical simulation proxy only; original source topology, UVs and named bone weights retained.',
        'Original free-surface collision exclusion retained; proposed clump-binding endpoints pinned, not semantically approved.',
        'No strand self/cross-hair collision; proxy polygon contacts differ slightly from analytic capsule checks.',
        'Source-style deformation, pin/contact conflicts and full surface checks remain required.'])
(out / 'pilot-report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report), flush=True)
