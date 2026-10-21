"""Auto-rig a generated biped GLB onto the UE4 mannequin skeleton and carry mannequin clips over.

Run: blender -b --factory-startup --python tools/Build-NpcAutoRig.py -- --input <glb> --skeleton <SK_Mannequin-skeleton.json>
         --clips <clip.fbx> [...] --output <new folder> [--height 1.8] [--triangles 50000] [--yaw-deg 180]
Input: one front-facing, roughly symmetric A-pose biped with arms clear of the body (Pixal3D output faces +Y,
hence the default 180 degree yaw to the mannequin's -Y). Steps: normalize height and floor, decimate, find
landmarks from horizontal slices (armpits, hand tips, crotch gap, limb centroids), place the mannequin's joints
there while keeping its rest orientations, skin with bone heat on a voxel proxy transferred back to the mesh,
then copy each clip's world-space bone rotations (pelvis translation scaled by hip height). Clips must be
exported from the same skeleton (Export-UnrealAnimFbx.py). Writes an FBX in centimetres, a .blend, review renders
and rig-report.json; the result needs a visual deformation review.
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
p.add_argument('--input', required=True); p.add_argument('--skeleton', required=True)
p.add_argument('--clips', nargs='*', default=[]); p.add_argument('--output', required=True)
p.add_argument('--height', type=float, default=1.8, help='Target standing height in metres')
p.add_argument('--triangles', type=int, default=50000, help='Decimation target for the skinned mesh')
p.add_argument('--yaw-deg', type=float, default=180.0, help='Rotation that makes the input face -Y (Blender front)')
p.add_argument('--name', default='Npc', help='Asset basename')
a = p.parse_args(sys.argv[sys.argv.index('--') + 1:])
out = Path(a.output).resolve(); assert not out.exists(), out; out.mkdir(parents=True)
skel = json.loads(Path(a.skeleton).read_text())
H = a.height

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene


def select(objs, active=None):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = active or objs[0]


def mid(x):  # extent centre; vertex medians drift toward densely detailed straps and buckles
    return float(0.5 * sum(np.percentile(x, [5, 95])))


def verts(o):
    co = np.empty(len(o.data.vertices) * 3); o.data.vertices.foreach_get('co', co)
    co = co.reshape(-1, 3); M = np.array(o.matrix_world)
    return co @ M[:3, :3].T + M[:3, 3]


# Mesh: one object, facing -Y, feet on z=0, centred between the feet, scaled to height, decimated.
bpy.ops.import_scene.gltf(filepath=str(Path(a.input).resolve()))
meshes = [o for o in scene.objects if o.type == 'MESH']
select(meshes); bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM')
if len(meshes) > 1: bpy.ops.object.join()
body = bpy.context.view_layer.objects.active; body.name = a.name
for o in [o for o in scene.objects if o is not body]: bpy.data.objects.remove(o)
select([body]); bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
V = verts(body); s = H / np.ptp(V[:, 2])
body.matrix_world = Matrix.Scale(s, 4) @ Matrix.Rotation(math.radians(a.yaw_deg), 4, 'Z')
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
V = verts(body); feet = V[V[:, 2] < V[:, 2].min() + 0.05 * H]; torso = V[np.abs(V[:, 2] - V[:, 2].min() - 0.6 * H) < 0.05 * H]
body.location = (-0.5 * sum(np.percentile(feet[:, 0], [2, 98])), -mid(torso[:, 1]), -V[:, 2].min())
bpy.ops.object.transform_apply(location=True)
source_tris = len(body.data.polygons)
V = verts(body)  # landmarks use the dense source surface; decimated spacing breaks up slices
if source_tris > a.triangles:
    m = body.modifiers.new('decimate', 'DECIMATE'); m.ratio = a.triangles / source_tris
    bpy.ops.object.modifier_apply(modifier=m.name)
bpy.ops.object.shade_smooth()

# Landmarks from horizontal slices. Clusters are runs of sorted x separated by a gap.
GAP, BAND = 0.025, 0.008 * H


def band(z, pts=None):
    pts = V if pts is None else pts
    return pts[np.abs(pts[:, 2] - z) < BAND]


def clusters(pts):
    if not len(pts): return []
    order = np.argsort(pts[:, 0]); xs = pts[order, 0]
    cuts = np.where(np.diff(xs) > GAP)[0] + 1
    return [pts[idx] for idx in np.split(order, cuts)]


def torso_cluster(cl):
    return next((c for c in cl if c[:, 0].min() <= 0 <= c[:, 0].max()), None)


# Hand tips are the widest points; climb from each hand until its arm merges into the torso (ears and
# mane tufts above the shoulders would otherwise read as separate "arms").
tips = {side: Vector(V[np.argmax(V[:, 0] * sign)]) for side, sign in (('l', 1), ('r', -1))}
armpit = {}
for side, sign in (('l', 1), ('r', -1)):
    last = None
    for z in np.arange(tips[side].z + 0.04 * H, 0.95 * H, 0.004 * H):
        cl = clusters(band(z)); t = torso_cluster(cl)
        if t is None: continue
        if any((c[:, 0] * sign).min() > (t[:, 0] * sign).max() for c in cl):
            last = (float(z), float(min(-t[:, 0].min(), t[:, 0].max())), mid(t[:, 1]))
        elif last: break
    armpit[side] = last
assert all(armpit.values()), f'Arms never separate from the torso: {armpit}'
z_arm, half_w, y_chest = max(armpit.values())  # a pauldron can merge one side lower; trust the higher armpit

center_gap = None
for z in np.arange(0.06 * H, 0.65 * H, 0.004 * H):
    if np.any(np.abs(band(z)[:, 0]) < 0.006 * H): center_gap = float(z); break
assert center_gap, 'No crotch gap found between the legs'
z_hip = min(center_gap + 0.055 * H, 0.68 * z_arm)


def centre(z, sign):  # limb cross-section centre on one side at height z
    c = [c for c in clusters(band(z)) if np.median(c[:, 0]) * sign > 0]
    c = max(c, key=len)
    return Vector((0.5 * (c[:, 0].min() + c[:, 0].max()), mid(c[:, 1]), z))


def recentre(v, r):
    near = V[np.linalg.norm(V - np.array(v), axis=1) < r]
    return Vector(near.mean(0)) if len(near) > 20 else v


W = {b['name']: b['world']['t'] for b in skel['bones']}
mz = lambda n: W[n][2] / 100.0
J = {}
side_j = {}
for side, sign in (('l', 1), ('r', -1)):
    S = Vector((sign * half_w, y_chest, z_arm + 0.045 * H))
    T = tips[side]
    E = recentre(S.lerp(T, 0.379), 0.05 * H); Wr = recentre(S.lerp(T, 0.714), 0.04 * H)
    thigh = centre(center_gap - 0.03 * H, sign); thigh.z = z_hip
    foot_z = 0.142 * z_hip; foot = centre(foot_z, sign)
    knee = centre(foot_z + 0.484 * (z_hip - foot_z), sign)
    low = V[(V[:, 2] < 0.05 * H) & (V[:, 0] * sign > 0)]; toe_y = float(low[:, 1].min())
    ball = Vector((foot.x, foot.y + 0.6 * (toe_y - foot.y), 0.0295 * z_hip))
    side_j[side] = dict(upperarm=S, lowerarm=E, hand=Wr, tip=T, thigh=thigh, calf=knee, foot=foot, ball=ball,
                        toe=Vector((foot.x, toe_y, ball.z)))
# Symmetrize: mirror-average the two sides so clips play evenly.
for k in side_j['l']:
    L_, R_ = side_j['l'][k], side_j['r'][k]
    m = Vector((0.5 * (L_.x - R_.x), 0.5 * (L_.y + R_.y), 0.5 * (L_.z + R_.z)))
    J[f'{k}_l'] = m; J[f'{k}_r'] = Vector((-m.x, m.y, m.z))
spine_y = lambda z: mid(band(z)[np.abs(band(z)[:, 0]) < 0.6 * half_w][:, 1])
z_pel = z_hip * mz('pelvis') / mz('thigh_l'); z_sh = J['upperarm_l'].z
HEAD_TOP_M = 1.82  # mannequin crown, above the head bone
for n in ['pelvis', 'spine_01', 'spine_02', 'spine_03']:
    z = z_pel + (mz(n) - mz('pelvis')) / (mz('upperarm_l') - mz('pelvis')) * (z_sh - z_pel)
    J[n] = Vector((0, spine_y(z), z))
for n in ['neck_01', 'head']:
    z = z_sh + (mz(n) - mz('upperarm_l')) / (HEAD_TOP_M - mz('upperarm_l')) * (H - z_sh)
    J[n] = Vector((0, spine_y(z), z))
for side, sign in (('l', 1), ('r', -1)):
    J[f'clavicle_{side}'] = Vector((sign * W['clavicle_l'][0] / W['upperarm_l'][0] * J['upperarm_l'].x, J['upperarm_l'].y,
                                    z_pel + (mz('clavicle_l') - mz('pelvis')) / (mz('upperarm_l') - mz('pelvis')) * (z_sh - z_pel)))
J['root'] = Vector((0, 0, 0))


def ue_to_blender(tr):
    q = tr['q']; t = tr['t']
    m = Quaternion((q[0], -q[1], q[2], -q[3])).to_matrix().to_4x4()
    m.translation = Vector((t[0], -t[1], t[2])) / 100.0
    return m


manny = {b['name']: ue_to_blender(b['world']) for b in skel['bones']}
order = [b['name'] for b in skel['bones']]; parent = {b['name']: b['parent'] for b in skel['bones']}
k_len = H / HEAD_TOP_M
for n in order:  # fingers, twist and IK bones keep their mannequin offset from the fitted parent
    if n not in J:
        J[n] = J[parent[n]] + k_len * (manny[n].translation - manny[parent[n]].translation)

# Final rig: mannequin rest orientations at fitted joints.
rig_data = bpy.data.armatures.new('Armature'); rig = bpy.data.objects.new('Armature', rig_data); scene.collection.objects.link(rig)
select([rig]); bpy.ops.object.mode_set(mode='EDIT')
for n in order:
    e = rig_data.edit_bones.new(n); e.head = (0, 0, 0); e.tail = (0, 0.05, 0)
    e.matrix = Matrix.Translation(J[n]) @ manny[n].to_3x3().normalized().to_4x4()
for n in order:
    if parent[n]: rig_data.edit_bones[n].parent = rig_data.edit_bones[parent[n]]
bpy.ops.object.mode_set(mode='OBJECT')
rest = {b.name: b.matrix_local.copy() for b in rig_data.bones}

# Weights: bone heat on a closed voxel proxy using a joint-to-joint deform skeleton, transferred by name.
CHAIN = {'pelvis': 'spine_01', 'spine_01': 'spine_02', 'spine_02': 'spine_03', 'spine_03': 'neck_01', 'neck_01': 'head',
         'head': None}
for sd in 'lr':
    CHAIN.update({f'clavicle_{sd}': f'upperarm_{sd}', f'upperarm_{sd}': f'lowerarm_{sd}', f'lowerarm_{sd}': f'hand_{sd}',
                  f'hand_{sd}': f'tip_{sd}', f'thigh_{sd}': f'calf_{sd}', f'calf_{sd}': f'foot_{sd}',
                  f'foot_{sd}': f'ball_{sd}', f'ball_{sd}': f'toe_{sd}'})
J['head_top'] = Vector((0, J['head'].y, H))
wdata = bpy.data.armatures.new('Weights'); wrig = bpy.data.objects.new('Weights', wdata); scene.collection.objects.link(wrig)
select([wrig]); bpy.ops.object.mode_set(mode='EDIT')
for n, c in CHAIN.items():
    e = wdata.edit_bones.new(n); e.head = J[n]; e.tail = J[c] if c else J['head_top']
for n in CHAIN:
    pn = parent[n]
    while pn and pn not in CHAIN: pn = parent[pn]
    if pn: wdata.edit_bones[n].parent = wdata.edit_bones[pn]
bpy.ops.object.mode_set(mode='OBJECT')
proxy = body.copy(); proxy.data = body.data.copy(); proxy.name = 'PROXY'; scene.collection.objects.link(proxy)
for slot in proxy.material_slots: slot.material = None
rm = proxy.modifiers.new('voxel', 'REMESH'); rm.mode = 'VOXEL'; rm.voxel_size = 0.012 * H / 1.8
select([proxy]); bpy.ops.object.modifier_apply(modifier=rm.name)
# Generated surfaces are thin shells, so the remesh also yields inner cavity shells that leave the bones
# "outside" and fail the heat solve. Drop shells lying inside the outer one (largest bounding box) and
# specks; keep separate outer parts such as a limb the remesh cut off.
import bmesh
from mathutils.bvhtree import BVHTree
bm = bmesh.new(); bm.from_mesh(proxy.data); bm.verts.ensure_lookup_table()
seen, parts = set(), []
for v in bm.verts:
    if v.index in seen: continue
    stack, comp = [v], []; seen.add(v.index)
    while stack:
        x = stack.pop(); comp.append(x)
        for e in x.link_edges:
            o = e.other_vert(x)
            if o.index not in seen: seen.add(o.index); stack.append(o)
    c = np.array([q.co for q in comp]); parts.append((float(np.prod(np.ptp(c, 0))), comp))
parts.sort(key=lambda t: -t[0])
outer = set(parts[0][1]); faces = [f for f in bm.faces if f.verts[0] in outer]
shell = BVHTree.FromPolygons([v.co.copy() for v in bm.verts], [[v.index for v in f.verts] for f in faces])


def inside_outer(pt):
    hits, o, d = 0, pt.copy(), Vector((1, 0.0013, 0.0007)).normalized()
    while (h := shell.ray_cast(o, d))[0] is not None:
        hits += 1; o = h[0] + d * 1e-5
    return hits % 2 == 1


drop = [comp for _, comp in parts[1:] if len(comp) < 50 or inside_outer(comp[0].co)]
proxy_shells = (len(parts), len(drop))
bmesh.ops.delete(bm, geom=[v for comp in drop for v in comp], context='VERTS'); bm.to_mesh(proxy.data); bm.free()
select([proxy, wrig], wrig); bpy.ops.object.parent_set(type='ARMATURE_AUTO')
proxy_weighted = sum(1 for v in proxy.data.vertices if any(g.weight > 0 for g in v.groups)) / len(proxy.data.vertices)
if proxy_weighted < 0.5: bpy.ops.wm.save_as_mainfile(filepath=str(out / 'heat-debug.blend'), copy=True)
dt = body.modifiers.new('weights', 'DATA_TRANSFER'); dt.object = proxy; dt.use_vert_data = True
dt.data_types_verts = {'VGROUP_WEIGHTS'}; dt.vert_mapping = 'POLYINTERP_NEAREST'
dt.layers_vgroup_select_src = 'ALL'; dt.layers_vgroup_select_dst = 'NAME'
select([body]); bpy.ops.object.datalayout_transfer(modifier=dt.name); bpy.ops.object.modifier_apply(modifier=dt.name)
# Any vertex the heat solve missed goes fully to the nearest deform segment.
segs = [(n, np.array(J[n]), np.array(J[c] if c else J['head_top'])) for n, c in CHAIN.items()]
total = np.zeros(len(body.data.vertices))
for v in body.data.vertices:
    total[v.index] = sum(g.weight for g in v.groups)
missing = np.where(total < 1e-4)[0]
if len(missing):
    P = verts(body)[missing]; dists = []
    for n, A, B in segs:
        d = B - A; t = np.clip(((P - A) @ d) / max(d @ d, 1e-9), 0, 1)
        dists.append(np.linalg.norm(P - (A + t[:, None] * d), axis=1))
    nearest = np.argmin(np.array(dists), axis=0)
    for idx, si in zip(missing, nearest):
        body.vertex_groups.get(segs[si][0]).add([int(idx)], 1.0, 'REPLACE')
select([body]); bpy.ops.object.vertex_group_limit_total(limit=4); bpy.ops.object.vertex_group_normalize_all(lock_active=False)
counts = {g.name: 0 for g in body.vertex_groups}
for v in body.data.vertices:
    for g in v.groups:
        if g.weight > 0.05: counts[body.vertex_groups[g.group].name] += 1
empty_groups = sorted(n for n in CHAIN if counts.get(n, 0) == 0)
print('WEIGHTS', json.dumps(dict(empty=empty_groups, missing_fixed=int(len(missing)), vertices=len(body.data.vertices),
                                 proxy_weighted=round(proxy_weighted, 4), proxy_shells=proxy_shells[0], proxy_shells_dropped=proxy_shells[1])), flush=True)
bpy.data.objects.remove(proxy); bpy.data.objects.remove(wrig)

# Clips: per-frame world rotation deltas from the source skeleton, applied over the fitted rest.
k_hip = z_hip / mz('thigh_l')
rig.animation_data_create(); catalog = []
for clip in a.clips:
    before = set(scene.objects)
    bpy.ops.import_scene.fbx(filepath=str(Path(clip).resolve()))
    new = [o for o in scene.objects if o not in before]; src = next(o for o in new if o.type == 'ARMATURE')
    src_action = src.animation_data.action; f0, f1 = (int(round(x)) for x in src_action.frame_range)
    srest = {b.name: src.matrix_world @ b.matrix_local for b in src.data.bones}
    name = Path(clip).stem; action = bpy.data.actions.new(name); action.use_fake_user = True
    slot = action.slots.new(id_type='OBJECT', name=rig.name)
    bag = action.layers.new('Baked').strips.new(type='KEYFRAME').channelbag(slot, ensure=True)
    vals = {n: ([], []) for n in order}
    for fr in range(f0, f1 + 1):
        scene.frame_set(fr); M = {}
        for n in order:
            if n not in srest:  # FBX import turns the root node into the armature object itself
                M[n] = (M[parent[n]] @ rest[parent[n]].inverted() @ rest[n]) if parent[n] else rest[n].copy(); continue
            pose = src.matrix_world @ src.pose.bones[n].matrix
            dq = pose.to_3x3().normalized().to_quaternion() @ srest[n].to_3x3().normalized().to_quaternion().inverted()
            R = dq.to_matrix() @ rest[n].to_3x3()
            if n in ('root', 'pelvis'):
                pos = rest[n].translation + k_hip * (pose.translation - srest[n].translation)
            else:
                pos = M[parent[n]] @ (rest[parent[n]].inverted() @ rest[n].translation)
            M[n] = Matrix.Translation(pos) @ R.to_4x4()
        for n in order:
            local = (rest[parent[n]].inverted() @ rest[n]) if parent[n] else rest[n]
            basis = local.inverted() @ (M[parent[n]].inverted() if parent[n] else Matrix.Identity(4)) @ M[n]
            loc, q, _ = basis.decompose(); vals[n][0].append(loc); vals[n][1].append(q)
    n_fr = f1 - f0 + 1
    for n in order:
        for path, size, idx in [('location', 3, 0), ('rotation_quaternion', 4, 1)]:
            for i in range(size):
                fc = bag.fcurves.new(f'pose.bones["{n}"].{path}', index=i, group_name=n); fc.keyframe_points.add(n_fr)
                fc.keyframe_points.foreach_set('co', [v for fr in range(n_fr) for v in (fr + 1, vals[n][idx][fr][i])])
    catalog.append(dict(id=name, source=str(Path(clip).resolve()), source_sha256=sha(clip), frames=n_fr))
    for o in new: bpy.data.objects.remove(o)
    if src_action.users == 0: bpy.data.actions.remove(src_action)

# Review renders: rest with joint markers, then front and side at two frames of each clip.
mod = body.modifiers.new('Armature', 'ARMATURE'); mod.object = rig
body.parent = rig
scene.render.engine = 'BLENDER_WORKBENCH'; scene.display.shading.color_type = 'TEXTURE'; scene.display.shading.light = 'STUDIO'
scene.render.resolution_x, scene.render.resolution_y = 900, 1000
cam = bpy.data.objects.new('REVIEW', bpy.data.cameras.new('REVIEW')); scene.collection.objects.link(cam); scene.camera = cam
cam.data.type = 'ORTHO'; cam.data.ortho_scale = H * 1.15
VIEWS = {'front': ((0, -6, H / 2), (math.radians(90), 0, 0)), 'side': ((6, 0, H / 2), (math.radians(90), 0, math.radians(90)))}
images = []


def shoot(tag):
    for view, (loc, rot) in VIEWS.items():
        cam.location, cam.rotation_euler = loc, rot
        scene.render.filepath = str(out / 'review' / f'{tag}-{view}.png'); bpy.ops.render.render(write_still=True)
        images.append(f'review/{tag}-{view}.png')


markers = []
for n in CHAIN:
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.015 * H, location=J[n]); markers.append(bpy.context.object)
body.hide_render = False; shoot('rest-joints')
for m_ in markers: bpy.data.objects.remove(m_)
for entry in catalog:
    act = bpy.data.actions[entry['id']]; rig.animation_data.action = act; rig.animation_data.action_slot = act.slots[0]
    for fr in sorted({1, max(1, entry['frames'] // 4), max(1, entry['frames'] // 2)}):
        scene.frame_set(fr); shoot(f"{entry['id']}-{fr:03d}")
rig.animation_data.action = None; scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=str(out / f'{a.name}.blend'))

# FBX in centimetres (no root scale in Unreal), mesh + armature + every clip as a take.
body.parent = None; body.modifiers.remove(mod)
sys.path.insert(0, str(Path(__file__).resolve().parent)); from BlenderUnrealUnits import to_centimetres
to_centimetres([rig, body], [bpy.data.actions[c['id']] for c in catalog])
mod = body.modifiers.new('Armature', 'ARMATURE'); mod.object = rig
select([body, rig], rig); bpy.ops.object.parent_set(type='OBJECT', keep_transform=True)
fbx = out / f'{a.name}.fbx'
bpy.ops.export_scene.fbx(filepath=str(fbx), use_selection=True, object_types={'ARMATURE', 'MESH'}, add_leaf_bones=False,
                         mesh_smooth_type='FACE', path_mode='COPY', embed_textures=True, bake_anim=bool(catalog),
                         bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True,
                         bake_anim_simplify_factor=0.0, use_armature_deform_only=False)
report = dict(schema_version=1, status='rig-needs-review', input=str(Path(a.input).resolve()), input_sha256=sha(a.input),
              skeleton=skel['skeleton'], tool_sha256=sha(__file__), height_m=H, source_triangles=source_tris,
              triangles=len(body.data.polygons), unweighted_vertices_fixed=int(len(missing)), empty_deform_groups=empty_groups,
              vertices_per_group=counts, fbx_sha256=sha(fbx),
              landmarks_m=dict(armpit=round(z_arm, 4), torso_half_width=round(half_w, 4), center_gap=round(center_gap, 4), hip=round(z_hip, 4)),
              joints_m={n: [round(x, 4) for x in J[n]] for n in CHAIN}, clips=catalog, review_images=images,
              limitations=['Joint landmarks are slice heuristics; review the rest-joints render before use.',
                           'Clips copy mannequin rotations; contacts (feet, hands) are not re-solved for new proportions.',
                           'Finger bones are placed but unweighted: hands deform as one piece.'])
(out / 'rig-report.json').write_text(json.dumps(report, indent=1))
print('RIG_DONE', json.dumps(dict(triangles=report['triangles'], landmarks=report['landmarks_m'], unweighted=report['unweighted_vertices_fixed'])), flush=True)
