"""Rider clips on an Unreal skeleton (from Dump-UnrealSkeleton.py JSON), seated on a mount's saddle.

Run: blender -b --factory-startup --python tools/Build-RiderClips.py -- --profile <json> --output <new folder>
Rebuilds the Unreal reference skeleton in Blender (Y mirrored, cm->m) so exported FBX node transforms
reproduce Unreal's reference frames. Finds the saddle seat on the mount mesh, poses the rider from eased
key poses (pelvis/spine/neck/head rotations, two-bone analytic IK for arms and legs with rest bend
planes and pole points), bakes FK actions, renders review frames with the mount and exports one FBX
(armature only, every clip as a take). Writes the seat socket in the mount's bone space for Unreal.
"""
import argparse, hashlib, json, math, sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for chunk in iter(lambda: f.read(8 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


p = argparse.ArgumentParser()
p.add_argument('--profile', required=True); p.add_argument('--output', required=True)
a = p.parse_args(sys.argv[sys.argv.index('--') + 1:])
profile, out = Path(a.profile).resolve(), Path(a.output).resolve()
cfg = json.loads(profile.read_text())
assert cfg['schema_version'] == 1 and not out.exists()
for f, h in cfg['input_sha256'].items():
    assert sha(f) == h, f
skel = json.loads(Path(cfg['skeleton']).read_text())
MIR = Matrix.Diagonal((1, -1, 1, 1))


def ue_to_blender(tr):
    q = tr['q']; t = tr['t']
    m = Quaternion((q[0], -q[1], q[2], -q[3])).to_matrix().to_4x4()
    m.translation = Vector((t[0], -t[1], t[2])) / 100.0
    return m


# Mount: rest pose, seat point on the saddle ridge.
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['mount_blend']).resolve()))
scene = bpy.context.scene; horse = bpy.data.objects[cfg['mount_object']]; harm = bpy.data.objects[cfg['mount_armature']]
harm.name = 'MOUNT'; harm.animation_data.action = None  # free the name 'Armature' for the rider export
for pb in harm.pose.bones: pb.matrix_basis = Matrix.Identity(4)
bpy.context.view_layer.update()
dg = bpy.context.evaluated_depsgraph_get(); me = horse.evaluated_get(dg).to_mesh()
co = np.empty(len(me.vertices) * 3); me.vertices.foreach_get('co', co); horse.evaluated_get(dg).to_mesh_clear()
Mh = np.array(horse.matrix_world); W = np.c_[co.reshape(-1, 3), np.ones(len(co) // 3)] @ Mh.T
sr = cfg['seat_search']; ridge = W[(np.abs(W[:, 0]) < sr['half_width_m']) & (W[:, 1] > sr['y_range_m'][0]) & (W[:, 1] < sr['y_range_m'][1])]
bins = np.linspace(*sr['y_range_m'], sr['bins'] + 1); tops = []
for lo, hi in zip(bins, bins[1:]):
    s = ridge[(ridge[:, 1] >= lo) & (ridge[:, 1] < hi)]
    if len(s): tops.append((0.5 * (lo + hi), float(s[:, 2].max())))
tops = np.array(tops); k = int(np.argmin(tops[:, 1])); seat = Vector((0.0, tops[k, 0], tops[k, 1]))
sock_bone = harm.pose.bones[cfg['socket_bone']]; Bw = harm.matrix_world @ sock_bone.matrix
v_b = Bw.inverted() @ seat; R_rel = (Bw.to_quaternion().inverted()).to_matrix()
socket = dict(bone=cfg['socket_bone'], name=cfg['socket_name'],
              relative_location_cm=[100 * v_b.x, -100 * v_b.y, 100 * v_b.z],
              relative_rotation_quat_wxyz=list((MIR.to_3x3() @ R_rel @ MIR.to_3x3()).to_quaternion()),
              seat_world_m=list(seat), ridge_profile=tops.round(4).tolist())

def ground(v):  # "ground+0.135" -> height above world ground expressed relative to the seat
    if isinstance(v, str) and v.startswith('ground'): return float(v[6:] or 0) - seat.z
    if isinstance(v, list): return [ground(x) for x in v]
    if isinstance(v, dict): return {k: ground(x) for k, x in v.items()}
    return v
cfg['clips'] = ground(cfg['clips'])

# Rider armature: exact reference frames.
rig_data = bpy.data.armatures.new('Armature'); rig = bpy.data.objects.new('Armature', rig_data); scene.collection.objects.link(rig)
rig.matrix_world = Matrix.Translation(seat)
bpy.context.view_layer.objects.active = rig; bpy.ops.object.mode_set(mode='EDIT')
world_rest = {b['name']: ue_to_blender(b['world']) for b in skel['bones']}
for b in skel['bones']:
    e = rig_data.edit_bones.new(b['name']); e.head = (0, 0, 0); e.tail = (0, cfg['bone_display_length_m'], 0)
    e.matrix = world_rest[b['name']]
for b in skel['bones']:
    if b['parent']: rig_data.edit_bones[b['name']].parent = rig_data.edit_bones[b['parent']]
bpy.ops.object.mode_set(mode='OBJECT')
rest = {b.name: b.matrix_local.copy() for b in rig_data.bones}
order = [b['name'] for b in skel['bones']]; parent = {b['name']: b['parent'] for b in skel['bones']}
children = {}
for n in order:
    if parent[n]: children.setdefault(parent[n], []).append(n)


def ease(u): return 0.5 - 0.5 * math.cos(math.pi * min(max(u, 0.), 1.))


def kv(keys, field, frame, default):
    pts = [(k['frame'], k[field]) for k in keys if field in k]
    if not pts: return default
    if frame <= pts[0][0]: return pts[0][1]
    for (f0, v0), (f1, v1) in zip(pts, pts[1:]):
        if f0 <= frame <= f1:
            u = ease((frame - f0) / max(f1 - f0, 1))
            if isinstance(v0, dict):
                return {n: [x + (y - x) * u for x, y in zip(v0.get(n, [0, 0, 0]), v1.get(n, [0, 0, 0]))] for n in set(v0) | set(v1)}  # missing = zero offset
            return [x + (y - x) * u for x, y in zip(v0, v1)]
    return pts[-1][1]


def arm_rot(deg):
    pu, ro, ya = (math.radians(v) for v in deg)
    return Quaternion((0, 0, 1), ya) @ Quaternion((0, 1, 0), ro) @ Quaternion((1, 0, 0), -pu)


def two_bone(A, B, C, Ap, Cp, pole):
    """Rotations for upper/lower bones mapping rest chain A-B-C onto A'-B'-C' with knee/elbow toward pole."""
    L1, L2 = (B - A).length, (C - B).length; d = Cp - Ap; dl = min(d.length, (L1 + L2) * 0.9999); dn = d.normalized()
    x = (L1 * L1 - L2 * L2 + dl * dl) / (2 * dl); h = math.sqrt(max(L1 * L1 - x * x, 0))
    pv = (pole - Ap); pv = (pv - dn * pv.dot(dn)).normalized(); Bp = Ap + dn * x + pv * h; Cp2 = Ap + dn * dl
    n0 = (B - A).cross(C - B).normalized(); n1 = (Bp - Ap).cross(Cp2 - Bp).normalized()
    def frame(u, n): u = u.normalized(); return Matrix((u, n, u.cross(n))).transposed()
    R1 = frame(Bp - Ap, n1) @ frame(B - A, n0).transposed(); R2 = frame(Cp2 - Bp, n1) @ frame(C - B, n0).transposed()
    return R1, R2, (Cp2 - Cp).length


LIMBS = {'leg_l': ('thigh_l', 'calf_l', 'foot_l'), 'leg_r': ('thigh_r', 'calf_r', 'foot_r'),
         'arm_l': ('upperarm_l', 'lowerarm_l', 'hand_l'), 'arm_r': ('upperarm_r', 'lowerarm_r', 'hand_r')}


def pose(clip, fr):
    """Armature-space matrices (rider rig space, origin at seat) for every bone at this frame."""
    keys = clip['keys']; rot = kv(keys, 'rot', fr, {}); pel = Vector(kv(keys, 'pelvis_m', fr, [0, 0, 0]))
    targets = kv(keys, 'targets', fr, {}); poles = kv(keys, 'poles', fr, {})
    for name, chans in clip.get('osc', {}).items():
        t = (fr - 1) / clip['frames']; cur = rot.get(name, [0, 0, 0])
        rot[name] = [cur[i] + sum(o['amp'] * math.sin(2 * math.pi * (o.get('harmonic', 1) * t + o.get('phase', 0))) for o in chans[i]) for i in range(3)]
    M = {}; err = 0.
    for n in order:
        local = (rest[parent[n]].inverted() @ rest[n]) if parent[n] else rest[n].copy()
        base = (M[parent[n]] @ local) if parent[n] else local
        if n == 'pelvis':
            base = Matrix.Translation(pel + rest[n].translation) @ arm_rot(rot.get('pelvis', [0, 0, 0])).to_matrix().to_4x4() @ rest[n].to_3x3().to_4x4()
        elif n in rot:
            R = arm_rot(rot[n]).to_matrix(); base = Matrix.Translation(base.translation) @ (R @ base.to_3x3()).to_4x4()
        M[n] = base
        for limb, (u, l, e) in LIMBS.items():
            if n == u and limb in targets:
                A = rest[u].translation; B = rest[l].translation; C = rest[e].translation
                Ap = M[u].translation; parent_rot = M[parent[u]].to_3x3() @ rest[parent[u]].to_3x3().inverted()
                Arest = (parent_rot @ (A - rest[parent[u]].translation)) + M[parent[u]].translation
                Ap = Arest; Bc = parent_rot @ (B - A) + Ap; Cc = parent_rot @ (C - A) + Ap
                R1, R2, e_ = two_bone(Ap, Bc, Cc, Ap, Vector(targets[limb]), Vector(poles.get(limb, [0, -1, 0])))
                err = max(err, e_)
                M[u] = Matrix.Translation(Ap) @ (R1 @ parent_rot @ rest[u].to_3x3()).to_4x4()
                M['__' + limb] = (R1, R2, parent_rot)
            if n == l and limb in targets:
                R1, R2, pr = M['__' + limb]; head = M[u] @ (rest[u].inverted() @ rest[l]).translation
                M[l] = Matrix.Translation(head) @ (R2 @ pr @ rest[l].to_3x3()).to_4x4()
            if n == e and limb in targets:
                head = M[l] @ (rest[l].inverted() @ rest[e]).translation
                er = kv(keys, 'end_rot', fr, {}).get(limb)
                R = (arm_rot(er).to_matrix() if er else Matrix.Identity(3)) @ rest[e].to_3x3()  # end effector keeps world-rest orientation (+ offset)
                M[e] = Matrix.Translation(head) @ R.to_4x4()
    return {k: v for k, v in M.items() if not k.startswith('__')}, err


out.mkdir(parents=True)
(out / 'profile.json').write_bytes(profile.read_bytes()); (out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
rig.animation_data_create(); catalog = []
for clip in cfg['clips']:
    n = clip['frames']; action = bpy.data.actions.new(clip['id']); action.use_fake_user = True
    slot = action.slots.new(id_type='OBJECT', name=rig.name); bag = action.layers.new('Baked').strips.new(type='KEYFRAME').channelbag(slot, ensure=True)
    vals = {b: ([], []) for b in order}; max_err = 0.; min_foot = 1e9
    for fr in range(1, n + 1):
        M, err = pose(clip, fr); max_err = max(max_err, err)
        min_foot = min(min_foot, min((rig.matrix_world @ M[f].translation).z for f in ('foot_l', 'foot_r', 'ball_l', 'ball_r')))
        for b in order:
            local = (rest[parent[b]].inverted() @ rest[b]) if parent[b] else rest[b]
            basis = local.inverted() @ (M[parent[b]].inverted() if parent[b] else Matrix.Identity(4)) @ M[b]
            loc, q, _ = basis.decompose(); vals[b][0].append(loc); vals[b][1].append(q)
    for b in order:
        for path, size, idx in [('location', 3, 0), ('rotation_quaternion', 4, 1)]:
            for i in range(size):
                fc = bag.fcurves.new(f'pose.bones["{b}"].{path}', index=i, group_name=b); fc.keyframe_points.add(n)
                fc.keyframe_points.foreach_set('co', [v for fr in range(n) for v in (fr + 1, vals[b][idx][fr][i])])
    catalog.append(dict(id=clip['id'], purpose=clip.get('purpose', ''), frames=n, loop=clip['loop'], events=clip.get('events', {}),
                        ik_target_error_m=round(max_err, 6), min_foot_height_m=round(min_foot, 4),
                        checks=dict(ik_reach=max_err <= cfg['limits']['ik_target_error_m'])))
    print('RIDER', clip['id'], json.dumps(catalog[-1]['checks']), round(max_err, 5), flush=True)

# Review: proxy capsules on rider bones, mount in rest pose, side camera.
for b in order:
    if b.startswith(('ik_', 'index', 'middle', 'pinky', 'ring', 'thumb')) or not children.get(b): continue
    c = children[b][0]; L = (rest[c].translation - rest[b].translation).length
    if L < 0.04: continue
    bpy.ops.mesh.primitive_cylinder_add(radius=cfg['review']['proxy_radius_m'], depth=L, vertices=8)
    cyl = bpy.context.object; cyl.parent = rig; cyl.parent_type = 'BONE'; cyl.parent_bone = b
    d = rig_data.bones[b].matrix_local.inverted().to_3x3() @ (rest[c].translation - rest[b].translation)
    cyl.matrix_parent_inverse = Matrix.Identity(4)
    cyl.location = d / 2 - Vector((0, rig_data.bones[b].length, 0)); cyl.rotation_euler = Vector((0, 0, 1)).rotation_difference(d).to_euler()
cam = bpy.data.objects.new('REVIEW', bpy.data.cameras.new('REVIEW')); scene.collection.objects.link(cam)
cam.data.type = 'ORTHO'; cam.data.ortho_scale = cfg['review']['ortho_scale_m']; cam.location = (6, seat.y, 1.4); cam.rotation_euler = (math.radians(90), 0, math.radians(90))
scene.camera = cam; scene.render.engine = 'CYCLES'; scene.cycles.device = 'CPU'; scene.cycles.samples = cfg['review']['samples']
scene.render.resolution_x, scene.render.resolution_y = cfg['review']['resolution']
for entry in catalog:
    act = bpy.data.actions[entry['id']]; rig.animation_data.action = act; rig.animation_data.action_slot = act.slots[0]; imgs = []
    for fr in sorted({1, max(1, entry['frames'] // 2), entry['frames']}):
        scene.frame_set(fr); scene.render.filepath = str(out / f"{entry['id']}-{fr:03d}.png"); bpy.ops.render.render(write_still=True); imgs.append(Path(scene.render.filepath).name)
    entry['review_images'] = imgs
rig.animation_data.action = None
fbx = out / 'Rider-Mannequin.fbx'
bpy.ops.object.select_all(action='DESELECT'); rig.select_set(True); bpy.context.view_layer.objects.active = rig
rig.matrix_world = Matrix.Identity(4)  # takes are seat-socket relative
bpy.ops.export_scene.fbx(filepath=str(fbx), use_selection=True, object_types={'ARMATURE'}, add_leaf_bones=False, bake_anim=True,
                         bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True,
                         bake_anim_simplify_factor=0.0, use_armature_deform_only=False)
report = dict(schema_version=1, status='rider-clips-need-review', cpu_only=True, input_sha256=cfg['input_sha256'], tool_sha256=sha(__file__),
              profile_sha256=sha(profile), fbx_sha256=sha(fbx), skeleton=skel['skeleton'], socket=socket, clips=catalog,
              passed=all(all(c['checks'].values()) for c in catalog),
              limitations=['Key-pose blockouts on proxy capsules; needs animator review with the real rider mesh.',
                           'Clips are seat-socket relative; mount/dismount assume the rider detaches at clip end/start.'])
(out / 'rider-report.json').write_text(json.dumps(report, indent=1))
print('RIDER_DONE', json.dumps(dict(passed=report['passed'], socket=socket['relative_location_cm'], seat=socket['seat_world_m'])), flush=True)
