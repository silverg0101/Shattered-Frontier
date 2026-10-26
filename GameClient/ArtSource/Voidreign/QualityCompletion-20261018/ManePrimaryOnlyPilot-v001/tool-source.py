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
import shutil
import struct
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
assert set(profile['cloth_settings']) == {'quality', 'mass', 'air_damping', 'tension_stiffness', 'compression_stiffness', 'shear_stiffness', 'bending_stiffness', 'tension_damping', 'compression_damping', 'shear_damping', 'bending_damping', 'pin_stiffness'}
assert set(profile['collision_settings']) == {'distance_min', 'friction', 'collision_quality'}
assert profile['frame_start'] == 1 and 2 <= profile['frame_end'] <= 330
assert 1 <= len(profile['feature_labels']) <= 2 and set(profile['feature_labels']) <= {1, 2}
attachment_mode = profile.get('attachment_mode', 'pinned-source')
assert attachment_mode in ['pinned-source', 'sewing']
warmup = profile.get('warmup_frames', 0)
assert type(warmup) is int and 0 <= warmup <= 240
warmup_mode = profile.get('warmup_mode', 'negative-frames')
assert warmup_mode in ['negative-frames', 'positive-retime']
for f, expected in profile['input_sha256'].items():
    assert sha(f) == expected, f
out.mkdir(parents=True)
(out / 'profile.json').write_bytes(profile_path.read_bytes())
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
source = Path(profile['source'])
cache_inputs = []
for cache in profile.get('cache_directories', []):
    folder = Path(cache['path']).resolve()
    assert folder.parent == source.parent.resolve() and folder.is_dir()
    target = out / 'blendcache_Voidreign-ActualSurfacePilot'
    target.mkdir(exist_ok=True)
    for name, expected in cache['files'].items():
        assert Path(name).name == name and sha(folder / name) == expected
        assert not (target / name).exists()
        shutil.copy2(folder / name, target / name)
        assert sha(target / name) == expected
        cache_inputs.append((target / name, expected))
raw = json.loads(Path(profile['motion_profile']).read_text())
deformation_input_mode = profile.get('deformation_input_mode', 'original-skin-dynamic-rest')
assert deformation_input_mode in ['original-skin-dynamic-rest', 'rigid-assembly-rest']
if deformation_input_mode == 'rigid-assembly-rest':
    assert profile['rigid_assembly_harness_only'] is True
    assert profile['rigid_assembly_evidence'] in profile['input_sha256']
    evidence = json.loads(Path(profile['rigid_assembly_evidence']).read_text())
    assert evidence['source_sha256'] == sha(source) and evidence['frame_count'] >= profile['frame_end']
    assert next(row for row in evidence['checks'] if row['id'] == 'skin.protected_rigidity')['status'] == 'passed'
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
assert [m.type for m in obj.modifiers] == profile.get('existing_modifier_types', ['ARMATURE'])
assert all(not m.use_deform_preserve_volume for m in obj.modifiers if m.type == 'ARMATURE')
arm = next(m.object for m in obj.modifiers if m.type == 'ARMATURE')
root = bpy.data.objects[profile['assembly_root']]
_, first = np.unique(canonical, return_index=True)
chosen = np.isin(labels, profile['feature_labels'])
assert np.array_equal(chosen, chosen[first][canonical]), 'Source seam labels disagree'
chosen_nodes = np.flatnonzero(chosen[first])
original_chosen_nodes = chosen_nodes.copy()
attachment_rows = [row for row in attachments['proposals'] if chosen[first[row['child_node']]]]
if attachment_mode == 'sewing':
    assert profile['allow_proposed_attachments'] is True
    assert type(profile['sewing_force_max']) is float and 0 < profile['sewing_force_max'] <= 1000
    assert all(row['gap_m'] <= .03 for row in attachment_rows), 'Long proposed bindings need separate review'
    chosen_nodes = np.unique(np.r_[chosen_nodes, [row['parent_node'] for row in attachment_rows]])
lookup = np.full(len(first), -1, np.int32)
lookup[chosen_nodes] = np.arange(len(chosen_nodes))
representatives = first[chosen_nodes]
face_mask = chosen[triangles].all(axis=1)
proxy_faces = lookup[canonical[triangles[face_mask]]]
assert proxy_faces.min() >= 0
proxy_edges = []
if attachment_mode == 'sewing':
    geometric_edges = {tuple(sorted(e)) for f in proxy_faces for e in [(int(f[0]), int(f[1])), (int(f[1]), int(f[2])), (int(f[2]), int(f[0]))]}
    for row in attachment_rows:
        edge = tuple(sorted((int(lookup[row['parent_node']]), int(lookup[row['child_node']]))))
        assert edge[0] >= 0 and edge[0] != edge[1] and edge not in geometric_edges
        proxy_edges.append(edge)
    proxy_edges = sorted(set(proxy_edges))
proxy_mesh = bpy.data.meshes.new('Actual source hair | canonical simulation surface')
relative = np.array(root.matrix_world.inverted() @ obj.matrix_world)
proxy_rest = local[representatives].astype(np.float64) @ relative[:3, :3].T + relative[:3, 3]
proxy_mesh.from_pydata(proxy_rest.tolist(), proxy_edges, proxy_faces.tolist())
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
root_goal_rest_error = None
if deformation_input_mode == 'rigid-assembly-rest':
    # This adapter's primary body motion is the verified rigid assembly parent.
    # Secondary guide skin must not redefine cloth goals or its rest lengths.
    modifier.show_viewport = modifier.show_render = False
    bpy.context.view_layer.update()
    actual_proxy = proxy.evaluated_get(bpy.context.evaluated_depsgraph_get())
    actual_matrix = np.array(actual_proxy.matrix_world)
    expected_rest = local[representatives] @ expected_matrix[:3, :3].T + expected_matrix[:3, 3]
    actual_rest = np.array([vertex.co[:] for vertex in actual_proxy.data.vertices]) @ actual_matrix[:3, :3].T + actual_matrix[:3, 3]
    root_goal_rest_error = float(np.linalg.norm(actual_rest - expected_rest, axis=1).max())
    assert root_goal_rest_error < 1e-5, 'Rigid primary root-goal/rest coordinate mismatch'
pin = proxy.vertex_groups.new(name='Surface pins | original roots and proposed bindings')
exclude = proxy.vertex_groups.new(name='Contact exclusion | original tested mask complement')
pins = ~free[representatives] | ~chosen[representatives]
if attachment_mode == 'pinned-source':
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
cloth.settings.use_dynamic_mesh = deformation_input_mode == 'original-skin-dynamic-rest'
cloth.settings.use_sewing_springs = attachment_mode == 'sewing'
if attachment_mode == 'sewing':
    cloth.settings.sewing_force_max = profile['sewing_force_max']
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
positive_warmup = warmup > 0 and warmup_mode == 'positive-retime'
pc.frame_start = scene.frame_start if positive_warmup else scene.frame_start - warmup
pc.frame_end = scene.frame_end + warmup if positive_warmup else scene.frame_end
assert pc.frame_start == (profile['frame_start'] if positive_warmup else profile['frame_start'] - warmup)
pc.use_disk_cache = True
pc.name = profile.get('cache_name', 'ActualSurfacePilot')
assert pc.name.isalnum()
assert not any(f.name.startswith(pc.name + '_') for f, _ in cache_inputs), 'New simulation cache collides with inherited cache'


def shift_actions(amount):
    for action in bpy.data.actions:
        for layer in action.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    for curve in bag.fcurves:
                        for key in curve.keyframe_points:
                            key.co.x += amount
                            key.handle_left.x += amount
                            key.handle_right.x += amount


if positive_warmup:
    shift_actions(warmup)
    scene.frame_end = pc.frame_end
scene.frame_start = pc.frame_start
scene.frame_set(pc.frame_start)
bpy.ops.wm.save_as_mainfile(filepath=str(out / 'Voidreign-ActualSurfacePilot.blend'))
bpy.ops.object.select_all(action='DESELECT')
proxy.select_set(True)
bpy.context.view_layer.objects.active = proxy
started = time.monotonic()
with bpy.context.temp_override(object=proxy, active_object=proxy, point_cache=pc):
    bpy.ops.ptcache.bake(bake=True)
retime_reference = None
warmup_velocity = None
if positive_warmup:
    held = []
    for frame in [max(1, warmup-1), warmup]:
        scene.frame_set(frame)
        target_proxy = proxy.evaluated_get(bpy.context.evaluated_depsgraph_get())
        values = np.empty(len(representatives) * 3, np.float32)
        target_proxy.data.vertices.foreach_get('co', values)
        held.append(values.reshape(-1, 3))
    speeds = np.linalg.norm((held[1] - held[0])[~pins], axis=1) * scene.render.fps
    warmup_velocity = dict(rms_speed_m_s=float(np.sqrt(np.mean(speeds*speeds))), peak_speed_m_s=float(speeds.max()),
        rms_limit_m_s=raw['settings']['settle_rms_speed_m_s'], peak_limit_m_s=raw['settings']['settle_peak_speed_m_s'])
    warmup_velocity['passed'] = (warmup_velocity['rms_speed_m_s'] <= warmup_velocity['rms_limit_m_s']
        and warmup_velocity['peak_speed_m_s'] <= warmup_velocity['peak_limit_m_s'])
    expected = []
    for original_frame in range(profile['frame_start'], profile['frame_end'] + 1):
        scene.frame_set(original_frame + warmup)
        target_proxy = proxy.evaluated_get(bpy.context.evaluated_depsgraph_get())
        values = np.empty(len(representatives) * 3, np.float32)
        target_proxy.data.vertices.foreach_get('co', values)
        expected.append(values.reshape(-1, 3))
    retime_reference = np.array(expected)
    np.savez_compressed(out / 'positive-warmup-reference.npz', points=retime_reference)
    # Native cloth cache files are not retimed by renaming: their playback can
    # silently differ after reopening. Preserve them and freeze measured local
    # points into the independently verified MDD playback representation.
    mdd = out / 'Settled-feature-motion.mdd'
    with mdd.open('wb') as f:
        f.write(struct.pack('>2i', retime_reference.shape[0], retime_reference.shape[1]))
        f.write((np.arange(len(retime_reference)) / scene.render.fps).astype('>f4').tobytes())
        for values in retime_reference:
            f.write(values.astype('>f4').tobytes())
    for mod in proxy.modifiers:
        mod.show_viewport = mod.show_render = False
    playback = proxy.modifiers.new('Measured settled surface | verified MDD', 'MESH_CACHE')
    playback.cache_format, playback.deform_mode = 'MDD', 'OVERWRITE'
    playback.time_mode, playback.play_mode = 'FRAME', 'SCENE'
    playback.frame_start, playback.frame_scale = profile['frame_start'], 1.
    playback.forward_axis, playback.up_axis = 'POS_Y', 'POS_Z'
    playback.interpolation, playback.filepath = 'LINEAR', str(mdd)
    shift_actions(-warmup)
scene.frame_start = profile['frame_start']
scene.frame_end = profile['frame_end']
scene.frame_set(scene.frame_end)
evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
assert len(evaluated.data.vertices) == len(local)
result = np.array([v.co[:] for v in evaluated.data.vertices], np.float32)
assert np.isfinite(result).all()
for f, expected in profile['input_sha256'].items():
    assert sha(f) == expected, f
for f, expected in cache_inputs:
    assert sha(f) == expected, 'Inherited simulation cache changed'
scene['Surface pilot status'] = 'Diagnostic CPU surface cloth; no full-duration/contact/import/runtime acceptance'
scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=str(out / 'Voidreign-ActualSurfacePilot.blend'))
cache_metadata = dict(start=pc.frame_start, end=pc.frame_end, baked=pc.is_baked)
cache_retime_max_error = None
if retime_reference is not None:
    proxy_name = proxy.name
    bpy.ops.wm.open_mainfile(filepath=str(out / 'Voidreign-ActualSurfacePilot.blend'))
    scene, proxy = bpy.context.scene, bpy.data.objects[proxy_name]
    cache_retime_max_error = 0.
    for index, original_frame in enumerate(range(profile['frame_start'], profile['frame_end'] + 1)):
        scene.frame_set(original_frame)
        actual = proxy.evaluated_get(bpy.context.evaluated_depsgraph_get())
        values = np.empty(len(representatives) * 3, np.float32)
        actual.data.vertices.foreach_get('co', values)
        error = float(np.linalg.norm(values.reshape(-1, 3) - retime_reference[index], axis=1).max())
        cache_retime_max_error = max(cache_retime_max_error, error)
    assert cache_retime_max_error <= 1e-5, f'Fresh cache retime mismatch: {cache_retime_max_error}'
report = dict(schema_version=1, status='baked-actual-surface-pilot-needs-checks', cpu_only=True,
    deformation_input_mode=deformation_input_mode, root_goal_rest_preflight_error_m=root_goal_rest_error,
    animated_rest_shape=deformation_input_mode == 'original-skin-dynamic-rest',
    source_sha256=sha(source), mesh_hash=guide['mesh_hash'], profile_sha256=sha(profile_path), tool_sha256=sha(__file__),
    input_sha256=profile['input_sha256'], proxy_vertices=len(representatives), proxy_faces=len(proxy_faces),
    original_vertices=len(local), original_triangles=len(triangles), feature_labels=profile['feature_labels'],
    source_to_unit_proxy_max_error_m=preflight_error, proxy_unit_world_scale=list(proxy.matrix_world.to_scale()),
    pinned_proxy_vertices=int(pins.sum()), collision_excluded_vertices=len(exclude_ids),
    attachment_mode=attachment_mode, sewing_edges=len(proxy_edges), warmup_frames=warmup,
    warmup_mode=warmup_mode, fresh_cache_retime_max_error_m=cache_retime_max_error,
    warmup_velocity=warmup_velocity,
    cache_frame_start=cache_metadata['start'], cache_frame_end=cache_metadata['end'],
    inherited_cache_files=len(cache_inputs),
    frame_start=scene.frame_start, frame_end=scene.frame_end, baked=cache_metadata['baked'],
    elapsed_seconds=time.monotonic() - started, blender=bpy.app.version_string,
    source_output_sha256=sha(out / 'Voidreign-ActualSurfacePilot.blend'),
    limitations=['Offline Blender cloth cache and geometry-node source sampling, not an engine-compatible exchange.',
        'Canonical simulation proxy only; original source topology, UVs and named bone weights retained.',
        'Original free-surface collision exclusion retained; proposed bindings use the reported pin or sewing mode and remain semantically unapproved.',
        'No strand self/cross-hair collision; proxy polygon contacts differ slightly from analytic capsule checks.',
        'Source-style deformation, pin/contact conflicts and full surface checks remain required.'])
(out / 'pilot-report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report), flush=True)
