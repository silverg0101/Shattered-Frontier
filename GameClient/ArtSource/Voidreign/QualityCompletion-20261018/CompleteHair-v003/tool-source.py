"""Skin the actual supplied mesh to a CPU-baked multi-guide diagnostic rig.

Flat deform joints preserve independent TRS transforms in exchange formats;
logical guide chains are recorded separately. No replacement hair is generated.
"""
import argparse
import hashlib
import json
import math
import sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Vector, Matrix

p = argparse.ArgumentParser(description=__doc__)
for key in ['source', 'guide-report', 'simulation', 'config', 'output']:
    p.add_argument('--'+key, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
source, guide_report_path, simulation_path, config_path, out = map(lambda value: Path(value).resolve(),
    [a.source, a.guide_report, a.simulation, a.config, a.output])
guide_report = json.loads(guide_report_path.read_text())
guides_path = guide_report_path.parent/guide_report['guide_file']
guides = json.loads(guides_path.read_text())
simulation_report = json.loads((simulation_path/'simulation-report.json').read_text())
raw = json.loads(config_path.read_text())
source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
config_hash = hashlib.sha256(config_path.read_bytes()).hexdigest()
assert source_hash == guide_report['source_sha256']
assert simulation_report['config_sha256'] == config_hash == guide_report['motion_profile_sha256']
assert simulation_report['guides_sha256'] == hashlib.sha256(guides_path.read_bytes()).hexdigest()
assert simulation_report['status'] == 'passed-particle-checks-only', 'Repair failed particle checks before rig baking'
trajectory = np.load(simulation_path/'trajectory.npz')
field = np.load(guide_report_path.parent/guide_report['weight_field_file'])
feature_profile = json.loads(Path(guide_report['profile_path']).read_text())
assert hashlib.sha256(Path(guide_report['profile_path']).read_bytes()).hexdigest() == guide_report['profile_sha256']
assert raw['skin']['maximum_bone_influences'] == 4
softness = raw['skin']['guide_blend_softness_m']
assert .001 <= softness <= .1
out.mkdir(parents=True, exist_ok=False)
(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
(out/'motion-profile.json').write_bytes(config_path.read_bytes())
bpy.ops.wm.open_mainfile(filepath=str(source.resolve()))
scene = bpy.context.scene
scene.frame_set(1)
obj = bpy.data.objects[guide_report['object']]
obj.data.calc_loop_triangles()
local = np.array([v.co[:] for v in obj.data.vertices], dtype=np.float32)
faces = np.array([tuple(t.vertices) for t in obj.data.loop_triangles], dtype=np.int32)
assert hashlib.sha256(local.tobytes()+faces.tobytes()).hexdigest() == guide_report['mesh_hash']
points = np.array([obj.matrix_world@v.co for v in obj.data.vertices])
root = bpy.data.objects['MOTION HARNESS | rigid horse, not gait animation']
root.animation_data_clear()
old_armatures = []
for modifier in list(obj.modifiers):
    if modifier.type == 'ARMATURE':
        old_armatures.append(modifier.object)
        obj.modifiers.remove(modifier)
for old in set(old_armatures):
    bpy.data.objects.remove(old, do_unlink=True)
obj.vertex_groups.clear()
arm_data = bpy.data.armatures.new('Actual-mesh feature guides | flat deform hierarchy')
arm = bpy.data.objects.new('RIG | complete feature guides', arm_data)
scene.collection.objects.link(arm)
arm.parent = root
arm.show_in_front = True
bpy.ops.object.select_all(action='DESELECT')
arm.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode='EDIT')
static = arm_data.edit_bones.new('StaticBody')
static.head, static.tail = (0, 0, 0), (0, 0, .3)
bone_names = []
for guide in guides:
    names = []
    for index, (start, end) in enumerate(zip(guide['points'], guide['points'][1:])):
        bone = arm_data.edit_bones.new(f"{guide['name']}_{index:02}")
        bone.head, bone.tail = start, end
        bone.parent = static
        bone.use_connect = False
        names.append(bone.name)
    bone_names.append(names)
bpy.ops.object.mode_set(mode='OBJECT')
for bone in arm.pose.bones:
    bone.rotation_mode = 'QUATERNION'
groups = {name: obj.vertex_groups.new(name=name) for name in ['StaticBody']+
          [name for chain in bone_names for name in chain]}
labels = field['labels']
protected = field['motion_protected']
assert len(labels) == len(obj.data.vertices)
mask = np.zeros(len(labels), dtype=np.float64)
feature_reports = []
for label, feature in enumerate(feature_profile['features'], 1):
    indices = np.flatnonzero((labels == label) & ~protected)
    amount = np.clip(field['root_distance_m'][indices]/feature['root_feather_distance_m'], 0, 1)
    amount = amount*amount*(3-2*amount)
    mask[indices] = amount
    guide_indices = [i for i, g in enumerate(guides) if g['feature'] == feature['name']]
    assert len(guide_indices) >= 2
    for offset in range(0, len(indices), 2048):
        ids = indices[offset:offset+2048]
        selected_points = points[ids]
        distances, segments, parameters = [], [], []
        for gi in guide_indices:
            rest = np.array(guides[gi]['points'])
            starts, vectors = rest[:-1], np.diff(rest, axis=0)
            u = np.clip(np.einsum('nsi,si->ns', selected_points[:, None]-starts, vectors)/
                        np.sum(vectors*vectors, axis=1), 0, 1)
            projected = starts+u[:, :, None]*vectors
            ds = np.sum((selected_points[:, None]-projected)**2, axis=2)
            nearest = np.argmin(ds, axis=1)
            distances.append(np.sqrt(ds[np.arange(len(ids)), nearest]))
            segments.append(nearest)
            parameters.append(u[np.arange(len(ids)), nearest])
        distances = np.array(distances).T
        segments, parameters = np.array(segments).T, np.array(parameters).T
        closest = np.argsort(distances, axis=1)[:, :2]
        blend = 1/(np.take_along_axis(distances, closest, axis=1)+softness)**2
        blend /= blend.sum(axis=1, keepdims=True)
        for row, vertex in enumerate(ids):
            if mask[vertex] <= 1e-7:
                continue
            weights = {}
            for slot, chosen in enumerate(closest[row]):
                gi = guide_indices[chosen]
                segment = int(segments[row, chosen])
                u = float(parameters[row, chosen])
                for bi, fraction in [(segment, 1-u), (min(segment+1, len(bone_names[gi])-1), u)]:
                    name = bone_names[gi][bi]
                    weights[name] = weights.get(name, 0)+float(blend[row, slot])*fraction
            # Include StaticBody within the four-influence limit at feather bands.
            keep = 4 if mask[vertex] >= 1-1e-7 else 3
            active = sorted(weights.items(), key=lambda x: -x[1])[:keep]
            total = sum(value for _, value in active)
            for name, value in active:
                if value > 1e-8:
                    groups[name].add([int(vertex)], float(mask[vertex]*value/total), 'REPLACE')
        print('Skinned', feature['name'], min(offset+2048, len(indices)), '/', len(indices), flush=True)
    feature_reports.append(dict(feature=feature['name'], semantic_vertices=int((labels == label).sum()),
        moving_vertices=int(((labels == label) & (mask > .001)).sum()),
        protected_or_anchored_vertices=int(((labels == label) & (mask <= .001)).sum())))
for index, value in enumerate(mask):
    if value < 1-1e-7:
        groups['StaticBody'].add([index], float(1-value), 'REPLACE')
# A virtual clump attachment needs coherent root weights. Nearest-guide
# weights alone let two near endpoints choose unrelated chains and pull apart.
# This only couples proposed endpoints; it does not approve their semantics.
attachment_weight_report = []
if raw['skin'].get('attachment_root_blend_m'):
    blend_radius = raw['skin']['attachment_root_blend_m']
    assert type(blend_radius) is float and .005 <= blend_radius <= .1
    attachment_path = Path(guide_report['attachment_path'])
    assert hashlib.sha256(attachment_path.read_bytes()).hexdigest() == guide_report['attachment_sha256']
    attachments = json.loads(attachment_path.read_text())
    selection_path = Path(guide_report['selection_path'])
    selection = json.loads(selection_path.read_text())
    selected = np.load(selection_path.parent/selection['selection_file'])
    canonical = field['canonical_node']
    _, representative = np.unique(canonical, return_index=True)
    node_labels = labels[representative]
    adjacency = [[] for _ in representative]
    for left, right in selected['edges']:
        if node_labels[left] and node_labels[left] == node_labels[right]:
            adjacency[left].append(int(right))
            adjacency[right].append(int(left))
    component = np.full(len(representative), -1, dtype=np.int32)
    components = []
    for seed in np.flatnonzero(node_labels):
        if component[seed] >= 0:
            continue
        stack, members = [int(seed)], []
        component[seed] = len(components)
        while stack:
            node = stack.pop()
            members.append(node)
            for neighbor in adjacency[node]:
                if component[neighbor] < 0:
                    component[neighbor] = len(components)
                    stack.append(neighbor)
        components.append(np.array(members))
    vertices_by_node = [[] for _ in representative]
    for vertex, node in enumerate(canonical):
        vertices_by_node[node].append(vertex)
    group_names = {group.index: group.name for group in obj.vertex_groups}
    for relation in attachments['proposals']:
        parent, child = relation['parent_node'], relation['child_node']
        pi, ci = int(representative[parent]), int(representative[child])
        if protected[ci] and mask[pi] > 1e-7:
            attachment_weight_report.append(dict(id=relation['id'], status='protected-child-conflict'))
            continue
        inherited = {group_names[g.group]: g.weight for g in obj.data.vertices[pi].groups}
        nodes = components[component[child]]
        distances = np.linalg.norm(points[representative[nodes]]-points[ci], axis=1)
        changed = 0
        for node, distance in zip(nodes[distances < blend_radius], distances[distances < blend_radius]):
            vertex = int(representative[node])
            if protected[vertex]:
                continue
            factor = float(1-distance/blend_radius)
            factor = factor*factor*(3-2*factor)
            weights = {group_names[g.group]: (1-factor)*g.weight for g in obj.data.vertices[vertex].groups}
            for name, value in inherited.items():
                weights[name] = weights.get(name, 0)+factor*value
            weights = dict(sorted(weights.items(), key=lambda item: -item[1])[:4])
            total = sum(weights.values())
            ids = vertices_by_node[node]
            for existing in list(obj.data.vertices[vertex].groups):
                obj.vertex_groups[existing.group].remove(ids)
            for name, value in weights.items():
                if value > 1e-8:
                    groups[name].add(ids, float(value/total), 'REPLACE')
            changed += len(ids)
        attachment_weight_report.append(dict(id=relation['id'], status='diagnostic-coherent-root', vertices=changed))
    mask = np.array([sum(g.weight for g in vertex.groups if group_names[g.group] != 'StaticBody')
                     for vertex in obj.data.vertices])
    for label, record in enumerate(feature_reports, 1):
        record['moving_vertices'] = int(((labels == label) & (mask > .001)).sum())
        record['protected_or_anchored_vertices'] = int(((labels == label) & (mask <= .001)).sum())
for label, feature in enumerate(feature_profile['features'], 1):
    for prefix, values in [('Semantic | ', (labels == label).astype(float)),
                            ('Motion | ', mask*(labels == label))]:
        name = prefix+feature['name']
        old = obj.data.color_attributes.get(name)
        if old:
            obj.data.color_attributes.remove(old)
        attribute = obj.data.color_attributes.new(name=name, type='FLOAT_COLOR', domain='POINT')
        rgba = np.column_stack([values, values, values, np.ones(len(values))]).astype(np.float32)
        attribute.data.foreach_set('color', rgba.ravel())
modifier = obj.modifiers.new('Actual mesh | multi-guide secondary motion', 'ARMATURE')
modifier.object = arm
weight_error = max(abs(sum(g.weight for g in v.groups)-1) for v in obj.data.vertices)
influences = max(len(v.groups) for v in obj.data.vertices)
assert weight_error < 1e-5 and influences <= 4
assert not np.any(mask[protected]) and not np.any(mask[labels == 0])


def capsule(name, start, end, radius, material, parent=None):
    old = bpy.data.objects.get(name)
    if old:
        bpy.data.objects.remove(old, do_unlink=True)
    start, end = Vector(start), Vector(end)
    length = (end-start).length
    verts, faces = [], []
    rings = [(-length/2+radius*math.sin(theta), radius*math.cos(theta))
             for theta in [-math.pi/2, -math.pi/3, -math.pi/6, 0]]
    rings += [(length/2+radius*math.sin(theta), radius*math.cos(theta))
              for theta in [0, math.pi/6, math.pi/3, math.pi/2]]
    for z, r in rings:
        for k in range(12):
            verts.append((r*math.cos(k*math.tau/12), r*math.sin(k*math.tau/12), z))
    for ring in range(len(rings)-1):
        for k in range(12):
            faces.append((ring*12+k, ring*12+(k+1)%12, (ring+1)*12+(k+1)%12, (ring+1)*12+k))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    proxy = bpy.data.objects.new(name, mesh)
    scene.collection.objects.link(proxy)
    proxy.location = (start+end)/2
    proxy.rotation_euler = (end-start).to_track_quat('Z', 'Y').to_euler()
    proxy.parent = parent
    proxy.data.materials.append(material)
    for face in mesh.polygons:
        face.use_smooth = True
    return proxy


hand_spec = next(c for c in raw['colliders'] if c['space'] == 'hand')
post_spec = next(c for c in raw['colliders'] if c['space'] == 'world')
hand = capsule('Rider hand | contact demonstration', hand_spec['start_m'], hand_spec['end_m'],
               hand_spec['radius_m'], bpy.data.materials['Rider collision proxy'], root)
capsule('STATIC environment test post', post_spec['start_m'], post_spec['end_m'],
        post_spec['radius_m'], bpy.data.materials['Environment contact post'])
hand_base = hand.location.copy()
scene.render.fps = raw['settings']['fps']
scene.frame_start, scene.frame_end = 1, len(trajectory['points'])
max_head_error = 0.
max_rotation_step = 0.
last_directions = [[(Vector(g['points'][i+1])-Vector(g['points'][i])).normalized()
                    for i in range(len(g['points'])-1)] for g in guides]
last_rotations = [[arm_data.bones[name].matrix_local.to_quaternion() for name in names]
                  for names in bone_names]
for frame, (all_points, root_transform) in enumerate(zip(trajectory['points'], trajectory['root_transforms']), 1):
    scene.frame_set(frame)
    pos, angle = root_transform[:3], root_transform[3]
    c, s = math.cos(angle), math.sin(angle)
    rot = Matrix([[c, -s, 0], [s, c, 0], [0, 0, 1]])
    root.location = pos
    root.rotation_mode = 'QUATERNION'
    root.rotation_quaternion = rot.to_quaternion()
    root.keyframe_insert('location', frame=frame)
    root.keyframe_insert('rotation_quaternion', frame=frame)
    t = (frame-1)/scene.render.fps
    off = -raw['settings']['hand_sweep_distance_m']*math.sin(math.pi*min(1, max(0, (t-9)/2)))
    hand.location = hand_base+Vector((off, 0, 0))
    hand.keyframe_insert('location', frame=frame)
    bpy.context.view_layer.update()
    for gi, guide in enumerate(guides):
        local_points = (all_points[gi, :len(guide['points'])]-pos) @ np.array(rot)
        for index, name in enumerate(bone_names[gi]):
            direction = Vector(local_points[index+1]-local_points[index])
            bone = arm.pose.bones[name]
            unit_direction = direction.normalized()
            rotation = last_directions[gi][index].rotation_difference(unit_direction) @ last_rotations[gi][index]
            rotation.normalize()
            if frame > 1:
                change = rotation.rotation_difference(last_rotations[gi][index]).angle
                max_rotation_step = max(max_rotation_step, min(change, math.tau-change))
            last_directions[gi][index] = unit_direction
            last_rotations[gi][index] = rotation.copy()
            matrix = (Matrix.Translation(Vector(local_points[index]))
                      @ rotation.to_matrix().to_4x4()
                      @ Matrix.Diagonal((1, direction.length/arm_data.bones[name].length, 1, 1)))
            bone.matrix_basis = arm_data.bones[name].matrix_local.inverted() @ matrix
            for path in ['location', 'rotation_quaternion', 'scale']:
                bone.keyframe_insert(path, frame=frame)
    bpy.context.view_layer.update()
    for gi, names in enumerate(bone_names):
        for index, name in enumerate(names):
            actual = arm.matrix_world@arm.pose.bones[name].head
            max_head_error = max(max_head_error, (actual-Vector(all_points[gi, index])).length)
    if frame % 60 == 0:
        print('Baked actual-mesh guide frame', frame, flush=True)
assert max_head_error < 1e-5, max_head_error
camera = bpy.data.objects['Beauty']
camera.animation_data_clear()
camera.data.type, camera.data.ortho_scale = 'ORTHO', 6.
for frame, value in enumerate(trajectory['root_transforms'], 1):
    target = Vector(value[:3])+Vector((0, .1, 1.4))
    camera.location = target+Vector((7, -4, 2.7))
    camera.rotation_euler = (target-camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera.keyframe_insert('location', frame=frame)
    camera.keyframe_insert('rotation_euler', frame=frame)
for animated in [root, arm, hand, camera]:
    action = animated.animation_data.action
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    for key in curve.keyframe_points:
                        key.interpolation = 'LINEAR'
for marker in list(scene.timeline_markers):
    scene.timeline_markers.remove(marker)
for frame, title in [(1, 'SETTLED'), (31, 'WALK RESPONSE'), (91, 'RUN RESPONSE'),
                      (151, 'TURN RESPONSE'), (211, 'STOP / SETTLE'), (271, 'RIDER CONTACT')]:
    scene.timeline_markers.new(title, frame=frame)
scene.camera = camera
scene['Scope'] = 'Actual repaired mesh;24 guide proposal bake. Rigid body harness, not gait/runtime animation.'
scene['Attachment status'] = 'Separate-clump bindings are diagnostic proposals requiring surface-motion review.'
arm['Logical guide chains'] = json.dumps(bone_names)
arm['Hierarchy rationale'] = 'Flat independent deform transforms;logical chains live in preserved guide profile.'
scene.frame_set(125)
bpy.ops.file.pack_all()
bpy.ops.wm.save_as_mainfile(filepath=str(out/'Voidreign-CompleteHair.blend'))
bpy.ops.object.select_all(action='DESELECT')
for item in [obj, arm, root]:
    item.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.export_scene.gltf(filepath=str(out/'Voidreign-CompleteHair.glb'), export_format='GLB',
    use_selection=True, export_animations=True, export_animation_mode='SCENE', export_anim_scene_split_object=False)
bpy.ops.export_scene.fbx(filepath=str(out/'Voidreign-CompleteHair.fbx'), use_selection=True,
    object_types={'MESH', 'ARMATURE', 'EMPTY'}, path_mode='COPY', embed_textures=True,
    bake_anim=True, bake_anim_simplify_factor=0., bake_anim_use_nla_strips=False,
    bake_anim_use_all_actions=False, add_leaf_bones=False)
report = dict(status='diagnostic-rig-candidate; visual/surface/import checks required', cpu_only=True,
    source_sha256=source_hash, source_mesh_hash=guide_report['mesh_hash'], source=str(source.resolve()),
    guide_report_sha256=hashlib.sha256(guide_report_path.read_bytes()).hexdigest(),
    simulation_report_sha256=hashlib.sha256((simulation_path/'simulation-report.json').read_bytes()).hexdigest(),
    config_sha256=config_hash, tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    guides=len(guides), bones=len(arm_data.bones), features=feature_reports,
    vertices=len(obj.data.vertices), triangles=len(faces), rest_geometry_unchanged=True,
    max_weight_sum_error=weight_error, maximum_bone_influences=influences,
    protected_vertices=int(protected.sum()), protected_motion_weight_max=float(mask[protected].max()),
    max_source_bone_head_to_simulation_error_m=max_head_error,
    maximum_frame_rotation_step_rad=max_rotation_step,
    orientation_method='Temporal parallel transport initialized from authored rest roll; no world-up roll resets',
    attachment_root_weights=attachment_weight_report,
    hierarchy='Flat deform joints parented to StaticBody; explicit logical chains in armature metadata and guides.json',
    limitations=['Existing isolated-clump binding proposals are not final attachment acceptance.',
                 'Rigid horse harness; legs static. Offline motion, no engine simulation.',
                 'Particle contacts do not establish deformed-surface clearance.'])
(out/'rig-report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report), flush=True)
