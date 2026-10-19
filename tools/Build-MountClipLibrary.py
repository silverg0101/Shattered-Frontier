"""Procedural mount clip library on an IK primary rig: gaits plus eased key-pose clips, baked to FK actions.

Run: blender -b --factory-startup --python tools/Build-MountClipLibrary.py -- --profile <json> --output <new folder>
Each clip is generated through the existing foot IK, checked (contacts, slip, IK reach, bone length,
loop closure, ground, surface strain), rendered to a small review sheet and baked to its own action.
Root motion is recorded per frame in clips.json; it is not baked into bones.
Bone rotation channels are armature-space [pitch_up, roll, yaw] degrees applied at the bone head
(+pitch_up raises the front, +yaw turns toward +X, +roll lowers +X side).
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
assert cfg['schema_version'] == 1 and cfg['units'] == 'metres-seconds-degrees' and not out.exists()
for f, h in cfg['input_sha256'].items():
    assert sha(f) == h, f
with np.load(cfg['semantic_selection']) as s:
    labels, canonical = s['labels'], s['canonical_node']
patches = np.load(cfg['sole_patches'])
lim = cfg['limits']

bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()))
scene = bpy.context.scene; vl = bpy.context.view_layer
obj, rig = bpy.data.objects[cfg['object']], bpy.data.objects[cfg['rig_object']]
assembly = bpy.data.objects[cfg['assembly_object']]
feet = cfg['feet']; T = {f['id']: bpy.data.objects['FOOT TARGET | ' + f['id']] for f in feet}
POLE = {f['id']: bpy.data.objects['FOOT POLE | ' + f['id']] for f in feet}
nv = len(obj.data.vertices)
rest = np.empty(nv * 3); obj.data.vertices.foreach_get('co', rest); rest = rest.reshape(-1, 3)

# Neutral bases: each target's lowest (planted) location in the source clip, Y replaced by the nominal ankle.
lowest = {k: None for k in T}
for fr in range(scene.frame_start, scene.frame_end + 1):
    scene.frame_set(fr)
    for k, o in T.items():
        if lowest[k] is None or o.location.z < lowest[k][0].z:
            lowest[k] = (o.location.copy(), o.rotation_quaternion.copy())
scene.frame_set(scene.frame_start)
pole_base = {k: o.location.copy() for k, o in POLE.items()}
base = {}
for f in feet:
    loc, rot = lowest[f['id']]; base[f['id']] = (Vector((loc.x, f['nominal_ankle_y_m'], loc.z)), rot)

for o in [rig, assembly] + list(T.values()) + list(POLE.values()):
    o.animation_data_clear()
for pb in rig.pose.bones:
    pb.location = (0, 0, 0); pb.rotation_mode = 'QUATERNION'; pb.rotation_quaternion = (1, 0, 0, 0)
assembly.rotation_mode = 'QUATERNION'
base_z = cfg['assembly_base_z_m']
assembly.location = (0, 0, base_z); assembly.rotation_quaternion = (1, 0, 0, 0)
for k in T:
    T[k].location, T[k].rotation_quaternion = base[k][0], base[k][1]; POLE[k].location = pole_base[k]
vl.update()
SPACE = {'fore': 'PRI.body.spine', 'hind': 'PRI.body.pelvis'}
rest_space = {n: (rig.matrix_world @ rig.pose.bones[n].matrix).copy() for n in SPACE.values()}
bone_rest_q = {b.name: b.matrix_local.to_quaternion() for b in rig.data.bones}
first = np.unique(canonical, return_index=True)[1]
tri = np.array([list(q.vertices) for q in obj.data.polygons])
E = np.unique(np.sort(np.concatenate([tri[:, [0, 1]], tri[:, [1, 2]], tri[:, [2, 0]]]), 1), axis=0)
E = E[(labels[E] == 0).all(1)]; L0 = np.linalg.norm(rest[E[:, 0]] - rest[E[:, 1]], axis=1)
bone_len = {b.name: b.length for b in rig.data.bones}


def arm_rot(deg):
    """[pitch_up, roll, yaw] degrees -> armature-space quaternion."""
    pu, ro, ya = (math.radians(v) for v in deg)
    return Quaternion((0, 0, 1), ya) @ Quaternion((0, 1, 0), ro) @ Quaternion((1, 0, 0), -pu)


def ease(u):
    return 0.5 - 0.5 * math.cos(math.pi * min(max(u, 0.), 1.))


def keyed_value(keys, field, frame, default):
    """Cosine-eased value between the keys that define `field` (missing -> default)."""
    pts = [(k['frame'], k[field]) for k in keys if field in k]
    if not pts: return default
    if frame <= pts[0][0]: return pts[0][1]
    for (f0, v0), (f1, v1) in zip(pts, pts[1:]):
        if f0 <= frame <= f1:
            u = ease((frame - f0) / (f1 - f0))
            if isinstance(v0, dict):
                return {n: [a + (b - a) * u for a, b in zip(v0.get(n, [0, 0, 0]), v1.get(n, [0, 0, 0]))] for n in set(v0) | set(v1)}
            if isinstance(v0, list): return [a + (b - a) * u for a, b in zip(v0, v1)]
            return v0 + (v1 - v0) * u
    return pts[-1][1]


def osc(spec, t):
    """Sum of sinusoids: [{amp, harmonic, phase}] over cycle time t (0..1)."""
    return sum(s['amp'] * math.sin(2 * math.pi * (s.get('harmonic', 1) * t + s.get('phase', 0))) for s in spec or [])


def ramp(g, x):
    """Speed ramp over the clip's cycles: returns (rate, progress) at cycle time x."""
    C = g.get('cycles', 1); kind = g.get('ramp', 'none'); u = min(max(x / C, 0.), 1.)
    if kind == 'none': return 1., x
    if kind == 'start':
        r = 3 * u * u - 2 * u ** 3; prog = C * (u ** 3 - u ** 4 / 2) + max(x - C, 0.)
    elif kind == 'stop':
        r = 1 - (3 * u * u - 2 * u ** 3); prog = (min(x, C) if x > 0 else x) - C * (u ** 3 - u ** 4 / 2)
    else:  # bump: rest -> move -> rest, total progress C
        r = 2 * math.sin(math.pi * u) ** 2; prog = C * (u - math.sin(2 * math.pi * u) / (2 * math.pi))
    return r, prog


def path(g, prog):
    """Root position/heading after `prog` cycles of travel (straight or circular arc)."""
    turn = math.radians(g.get('turn_deg_per_cycle', 0.)); s = g['stride_m'] * prog; a = turn * prog
    pos = Vector((0, -s, 0)) if abs(turn) < 1e-9 else Vector((g['stride_m'] / turn * (1 - math.cos(a)), -g['stride_m'] / turn * math.sin(a), 0))
    return pos, Quaternion((0, 0, 1), a)


def pose_frame(clip, frame, n):
    """Set root/bone channels for this frame; return per-foot target modes."""
    t = (frame - 1) / n
    bones, root, yaw, modes = {}, [0., 0., 0.], 0., {}
    if clip['type'] == 'gait':
        g = clip; rate, prog = ramp(g, t * g.get('cycles', 1)); amp = min(rate, 1.); pos, head = path(g, prog)
        root = [pos.x, pos.y, g.get('root_z_m', 0.) * (amp if g.get('ramp_crouch', False) else 1.) + amp * osc(g.get('bob'), t)]
        yaw = math.degrees(head.to_euler().z)
        for name, chans in g.get('bone_osc', {}).items():
            bones[name] = [g.get('bone_offset', {}).get(name, [0, 0, 0])[i] + amp * osc(chans[i], t) for i in range(3)]
        for name, off in g.get('bone_offset', {}).items(): bones.setdefault(name, off)
    else:
        keys = clip['keys']
        root = keyed_value(keys, 'root_m', frame, [0., 0., 0.]); yaw = keyed_value(keys, 'yaw_deg', frame, 0.)
        bones = dict(keyed_value(keys, 'bones', frame, {}))
        for name, chans in clip.get('bone_osc', {}).items():
            cur = bones.get(name, [0, 0, 0]); bones[name] = [cur[i] + osc(chans[i], t) for i in range(3)]
        root = [root[i] + osc(clip.get('root_osc', [[], [], []])[i], t) for i in range(3)]
    assembly.location = Vector(root) + Vector((0, 0, base_z)); assembly.rotation_quaternion = Quaternion((0, 0, 1), math.radians(yaw))
    for pb in rig.pose.bones:
        q = Quaternion((1, 0, 0, 0))
        if pb.name in bones:
            bq = bone_rest_q[pb.name]; q = bq.inverted() @ arm_rot(bones[pb.name]) @ bq
        pb.rotation_quaternion = q
    # Pass 1: world-anchored targets (gait feet, planted, skid) do not depend on the body.
    world_feet = {}
    for f in feet:
        k = f['id']; b, rq = base[k]
        if clip['type'] == 'gait':
            g = clip; tc = t * g.get('cycles', 1); ph = tc + g['phase'][k]; cyc = math.floor(ph + 1e-9); u = ph - cyc
            stance = u < g['stance_fraction']; s = 0. if stance else (u - g['stance_fraction']) / (1 - g['stance_fraction'])
            adv = 0. if stance else s * s * s * (10 - 15 * s + 6 * s * s)
            # Mid-stance anchors are fixed on the root path; swing interpolates between consecutive anchors.
            t0 = cyc - g['phase'][k] + g['stance_fraction'] / 2; off = Vector((b.x, b.y, 0))
            p0, q0 = path(g, ramp(g, t0)[1]); p1, q1 = path(g, ramp(g, t0 + 1)[1])
            lift = 0. if stance else min(ramp(g, tc)[0], 1.) * g['lift_m'] * math.sin(math.pi * s) ** 2
            pitch = Quaternion((1, 0, 0), min(ramp(g, tc)[0], 1.) * math.radians(g.get('swing_pitch_deg', -15)) * math.sin(math.pi * s) ** 2)
            world_feet[k] = ((p0 + q0 @ off).lerp(p1 + q1 @ off, adv) + Vector((0, 0, b.z + lift)), q0.slerp(q1, adv) @ pitch @ rq)
            modes[k] = ('planted', cyc) if stance else ('free', None)
        else:
            m = keyed_value(clip['keys'], 'feet', frame, {}).get(k) if any('feet' in kk for kk in clip['keys']) else None
            spec = clip.get('foot_modes', {}).get(k, 'planted')
            if spec == 'planted' and not m:
                world_feet[k] = (b.copy(), rq); modes[k] = ('planted', 0)
            elif spec == 'skid':
                world_feet[k] = (b + Vector((root[0], root[1], 0)), rq); modes[k] = ('skid', None)
            else:
                modes[k] = ('free', Vector(m) if m else Vector((0, 0, 0)))
    # Reach clamp: lower the body just enough that every world-anchored foot is inside its leg chain.
    clamp = 0.
    for _ in range(6):
        vl.update(); worst = 0.
        for f in feet:
            if f['id'] not in world_feet: continue
            head = rig.matrix_world @ rig.pose.bones[f['chain'][0]].head
            reach = cfg['reach_fraction'] * sum(bone_len[n] for n in f['chain'][:3])
            worst = max(worst, (world_feet[f['id']][0] - head).length - reach)
        if worst <= 1e-5: break
        assembly.location.z -= worst; clamp += worst
    vl.update()
    space = {n: (rig.matrix_world @ rig.pose.bones[n].matrix) @ rest_space[n].inverted() for n in SPACE.values()}
    for f in feet:
        k = f['id']; b, rq = base[k]; sp = space[SPACE[k.split('.')[0]]]
        POLE[k].location = sp @ pole_base[k]
        if k in world_feet:
            T[k].location, T[k].rotation_quaternion = world_feet[k]
        else:
            loc = sp @ (b + modes[k][1]); floored = loc.z <= b.z; loc.z = max(loc.z, b.z)  # free feet never below their ground base
            T[k].location = loc; T[k].rotation_quaternion = rq if floored else sp.to_quaternion() @ rq; modes[k] = ('free', None)  # floored hooves stay level
    root = [root[0], root[1], root[2] - clamp]
    vl.update()
    return modes, root, yaw


def world_points():
    dg = bpy.context.evaluated_depsgraph_get(); me = obj.evaluated_get(dg).to_mesh()
    co = np.empty(len(me.vertices) * 3); me.vertices.foreach_get('co', co); obj.evaluated_get(dg).to_mesh_clear()
    co = co.reshape(-1, 3); M = np.array(obj.matrix_world)
    return co, co @ M[:3, :3].T + M[:3, 3]


out.mkdir(parents=True)
(out / 'profile.json').write_bytes(profile.read_bytes()); (out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
cam = bpy.data.objects.new('REVIEW | side', bpy.data.cameras.new('REVIEW | side')); scene.collection.objects.link(cam)
cam.data.type = 'ORTHO'; cam.data.ortho_scale = cfg['review']['ortho_scale_m']; cam.parent = assembly
cam.location = cfg['review']['camera_local_m']; cam.rotation_euler = (math.radians(90), 0, math.radians(90))
bpy.ops.mesh.primitive_plane_add(size=40, location=(0, 0, 0)); bpy.context.object.name = 'REVIEW | ground'
scene.camera = cam; scene.render.engine = 'CYCLES'; scene.cycles.device = 'CPU'; scene.cycles.samples = cfg['review']['samples']
scene.cycles.use_denoising = False; scene.render.resolution_x, scene.render.resolution_y = cfg['review']['resolution']
scene.render.fps = cfg['fps']; catalog = []; bake_store = {}
for clip in cfg['clips']:
    n = clip['frames']; loop = clip['loop']; frames = range(1, n + 2 if loop else n + 1)
    anchors, rec = {}, dict(max_contact_m=0., max_slip_m=0., min_sole_height_m=1e9, max_target_error_m=0., max_bone_length_rel=0.,
                            min_full_height_m=1e9, body_edge_p99_rel=0., body_edge_max_abs_m=0., seam_gap_m=0., loop_error_m=0.)
    root_curve, ref = [], None
    for fr in frames:
        modes, root, yaw = pose_frame(clip, fr, n)
        local, world = world_points()
        if fr == 1: ref = local.copy()
        if loop and fr == n + 1:  # object-local points exclude root motion: closure is direct
            rec['loop_error_m'] = float(np.linalg.norm(local - ref, axis=1).max()); break
        root_curve.append(dict(frame=fr, root_m=[float(v) for v in root], yaw_deg=float(yaw)))
        rec['seam_gap_m'] = max(rec['seam_gap_m'], float(np.linalg.norm(local - local[first][canonical], axis=1).max()))
        rec['min_full_height_m'] = min(rec['min_full_height_m'], float(world[:, 2].min()))
        rec['max_bone_length_rel'] = max(rec['max_bone_length_rel'], max(abs(pb.length / bone_len[pb.name] - 1) for pb in rig.pose.bones))
        for f in feet:
            k = f['id']; ids = patches[k + '.vertices']; h = float(world[ids, 2].min())
            tip = rig.matrix_world @ rig.pose.bones[f['chain'][-2]].tail
            rec['max_target_error_m'] = max(rec['max_target_error_m'], (tip - T[k].location).length)
            rec['min_sole_height_m'] = min(rec['min_sole_height_m'], h)
            mode, cyc = modes[k]
            if mode == 'planted':
                rec['max_contact_m'] = max(rec['max_contact_m'], abs(h))
                key = (k, cyc); anchors.setdefault(key, world[ids].copy())
                rec['max_slip_m'] = max(rec['max_slip_m'], float(np.linalg.norm(world[ids] - anchors[key], axis=1).max()))
        if (fr - 1) % cfg['strain_every_frames'] == 0:
            d = np.abs(np.linalg.norm(local[E[:, 0]] - local[E[:, 1]], axis=1) - L0)
            rec['body_edge_p99_rel'] = max(rec['body_edge_p99_rel'], float(np.percentile(d / np.maximum(L0, 1e-9), 99)))
            rec['body_edge_max_abs_m'] = max(rec['body_edge_max_abs_m'], float(d.max()))
    checks = dict(planted_contact=rec['max_contact_m'] <= lim['contact_m'], planted_slip=rec['max_slip_m'] <= lim['slip_m'],
                  ground=rec['min_sole_height_m'] >= -lim['penetration_m'], ik_reach=rec['max_target_error_m'] <= lim['target_error_m'],
                  bone_lengths=rec['max_bone_length_rel'] <= lim['bone_length_relative'], seams=rec['seam_gap_m'] <= lim['seam_m'],
                  loop=(not loop) or rec['loop_error_m'] <= lim['loop_m'])
    # Review sheet frames, then bake to an FK action named by clip id.
    sheet = []
    for fr in sorted({1, max(1, n // 3), max(1, 2 * n // 3), n}):
        pose_frame(clip, fr, n); scene.render.filepath = str(out / f"{clip['id']}-{fr:03d}.png"); bpy.ops.render.render(write_still=True); sheet.append(scene.render.filepath)
    baked = {}  # frame -> bone -> (armature-space evaluated matrix, basis loc, basis quat)
    for fr in range(1, n + 1):
        pose_frame(clip, fr, n); ev = rig.evaluated_get(bpy.context.evaluated_depsgraph_get())
        mats = {pb.name: ev.pose.bones[pb.name].matrix.copy() for pb in rig.pose.bones}; row = {}
        for pb in rig.pose.bones:
            par = mats[pb.parent.name] if pb.parent else Matrix.Identity(4)
            restm = (pb.parent.bone.matrix_local.inverted() @ pb.bone.matrix_local) if pb.parent else pb.bone.matrix_local
            loc, rot, _ = (restm.inverted() @ par.inverted() @ mats[pb.name]).decompose()
            row[pb.name] = (mats[pb.name], loc, rot)
        baked[fr] = row
    action = bpy.data.actions.new(clip['id']); action.use_fake_user = True
    slot = action.slots.new(id_type='OBJECT', name=rig.name); strip = action.layers.new('Baked').strips.new(type='KEYFRAME')
    bag = strip.channelbag(slot, ensure=True)
    for pb in rig.pose.bones:
        for dpath, size, pick in [('location', 3, 1), ('rotation_quaternion', 4, 2)]:
            for i in range(size):
                fc = bag.fcurves.new(f'pose.bones["{pb.name}"].{dpath}', index=i, group_name=pb.name)
                fc.keyframe_points.add(n)
                fc.keyframe_points.foreach_set('co', [v for fr in range(1, n + 1) for v in (fr, baked[fr][pb.name][pick][i])])
                for kp in fc.keyframe_points: kp.interpolation = 'LINEAR'
                fc.update()
    bake_store[clip['id']] = (action, slot, {fr: {b: m for b, (m, _, _) in baked[fr].items()} for fr in sorted({1, n // 2 or 1, n})})
    rec = {k: (round(v, 7) if isinstance(v, float) else v) for k, v in rec.items()}
    entry = dict(id=clip['id'], purpose=clip.get('purpose', ''), frames=n, fps=cfg['fps'], loop=loop, type=clip['type'],
                 root_motion=clip.get('root_motion', 'in-place'), speed_mps=(clip['stride_m'] * clip.get('cycles', 1) * cfg['fps'] / n) if clip['type'] == 'gait' and clip.get('ramp', 'none') == 'none' else None, turn_deg_per_s=(clip.get('turn_deg_per_cycle', 0) * clip.get('cycles', 1) * cfg['fps'] / n) if clip['type'] == 'gait' else None,
                 events=clip.get('events', {}), checks=checks, passed=all(checks.values()), metrics=rec, review_images=[Path(s).name for s in sheet], root_curve=root_curve)
    catalog.append(entry)
    print('CLIP', clip['id'], json.dumps(dict(passed=entry['passed'], checks={k: v for k, v in checks.items() if not v}, metrics=rec)), flush=True)

for o in [rig, assembly] + list(T.values()) + list(POLE.values()):
    o.animation_data_clear()
for pb in rig.pose.bones:
    for c in pb.constraints: c.mute = True
rig.animation_data_create()
for entry in catalog:
    action, slot, ref_mats = bake_store[entry['id']]; rig.animation_data.action = action; rig.animation_data.action_slot = slot; err = 0.
    for fr, ref in ref_mats.items():
        scene.frame_set(fr); ev = rig.evaluated_get(bpy.context.evaluated_depsgraph_get())
        err = max(err, max((ev.pose.bones[b].matrix.translation - m.translation).length + (ev.pose.bones[b].matrix.col[1].xyz - m.col[1].xyz).length for b, m in ref.items()))
    entry['checks']['fk_bake_replay'] = err <= lim['bake_m']; entry['metrics']['fk_bake_replay_error'] = err; entry['passed'] = all(entry['checks'].values())
    print('BAKE', entry['id'], err, flush=True)
rig.animation_data.action = None
pose_frame(dict(type='keyed', keys=[{'frame': 1}]), 1, 1)
scene.frame_start, scene.frame_end = 1, max(c['frames'] for c in cfg['clips'])
blend = out / 'Voidreign-ClipLibrary.blend'; bpy.ops.wm.save_as_mainfile(filepath=str(blend), relative_remap=True)
(out / 'clips.json').write_text(json.dumps(dict(schema_version=1, status='procedural-clip-library-needs-review', cpu_only=True,
    input_sha256=cfg['input_sha256'], tool_sha256=sha(__file__), profile_sha256=sha(profile), library_sha256=sha(blend),
    rig_object=cfg['rig_object'], clips=catalog,
    limitations=['Procedural key-pose/gait animation, not captured or hand-animated motion; needs animator review for production feel.',
                 'Actions are FK pose keys for the primary rig; root motion is recorded separately per clip and is not in bones.',
                 'Hair, tack and feathering follow primary weights rigidly; secondary motion is a separate stage.',
                 'Surface strain and contact are measured diagnostics, not visual approval.']), indent=1))
print('DONE', sum(c['passed'] for c in catalog), '/', len(catalog), flush=True)
