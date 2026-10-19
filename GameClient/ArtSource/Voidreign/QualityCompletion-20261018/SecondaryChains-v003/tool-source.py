"""Add secondary hair bone chains (tail, mane) to a mount rig and weight hair vertices to them.

Run: blender -b --factory-startup --python tools/Build-MountSecondaryChains.py -- --profile <json> --output <new folder>
Each region (semantic label) is split into k chains by k-means; a chain runs from the point nearest the
body surface through equal-count distance bins and is parented to the body bone its root vertices already
follow. Hair near the body keeps its existing primary weights; with distance from the body surface
(smoothstep over free_distance_m) it hands over to its chain. Weights are diffused across hair edges and
averaged over seam copies.
Rest geometry, UVs and body weights are untouched.
"""
import argparse, hashlib, json, sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Vector
from mathutils.kdtree import KDTree


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
with np.load(cfg['semantic_selection']) as s:
    labels, canonical = s['labels'], s['canonical_node']

bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()))
obj, rig = bpy.data.objects[cfg['object']], bpy.data.objects[cfg['rig_object']]
nv = len(obj.data.vertices)
co = np.empty(nv * 3); obj.data.vertices.foreach_get('co', co); co = co.reshape(-1, 3)
M = np.array(obj.matrix_world); A = np.array(rig.matrix_world)
arm = (np.c_[co, np.ones(nv)] @ M.T @ np.linalg.inv(A).T)[:, :3]  # mesh rest in armature space
tri = np.array([list(q.vertices) for q in obj.data.polygons])
pri = [vg.name for vg in obj.vertex_groups if vg.name.startswith('PRI.')]; pidx = {obj.vertex_groups[n].index: i for i, n in enumerate(pri)}
old = np.zeros((nv, len(pri)))
for v in obj.data.vertices:
    for g in v.groups:
        if g.group in pidx: old[v.index, pidx[g.group]] = g.weight
body = np.flatnonzero(labels == 0); kd = KDTree(len(body))
for i, v in enumerate(body): kd.insert(arm[v], i)
kd.balance()

chains, report = [], []
for region in cfg['regions']:
    ids = np.flatnonzero(labels == region['label']); P = arm[ids]
    dist = np.array([kd.find(P[i])[2] for i in range(len(ids))])
    # k-means seeded along the region's longest axis
    k = region['chains']; axis = np.linalg.svd(P - P.mean(0), full_matrices=False)[2][0]
    proj = (P - P.mean(0)) @ axis; C = np.array([P[np.argsort(proj)[int((j + .5) * len(P) / k)]] for j in range(k)])
    for _ in range(15):
        lab = np.argmin(((P[:, None, :] - C[None]) ** 2).sum(2), 1)
        C = np.array([P[lab == j].mean(0) if (lab == j).any() else C[j] for j in range(k)])
    for j in range(k):
        sel = lab == j; Q = P[sel]; dq = dist[sel]
        root = Q[dq <= max(region['root_contact_m'], np.percentile(dq, 2))].mean(0)
        r = np.linalg.norm(Q - root, axis=1); qs = np.percentile(r, np.linspace(0, 98, region['bones'] + 1))
        joints = [root] + [Q[(r > qs[b]) & (r <= qs[b + 1])].mean(0) for b in range(region['bones'] - 1)] + [Q[r >= np.percentile(r, 95)].mean(0)]
        name = f"SEC.{region['id']}.{j}"
        def seg_d(bn):
            bb = rig.data.bones[bn]; h, tl = np.array(bb.head_local), np.array(bb.tail_local); d = tl - h
            return np.linalg.norm(root - (h + np.clip((root - h) @ d / (d @ d), 0, 1) * d))
        near = ids[sel][dq <= np.percentile(dq, 10)]
        parent = pri[int(np.bincount(old[near].argmax(1), minlength=len(pri)).argmax())]
        chains.append(dict(id=name, parent=parent, joints=np.array(joints), verts=ids[sel], dist=dq, region=region))
        report.append(dict(chain=name, parent=parent, vertices=int(sel.sum()), joints_m=np.round(joints, 4).tolist(),
                           length_m=float(sum(np.linalg.norm(np.diff(joints, axis=0), axis=1)))))

vl = bpy.context.view_layer; vl.objects.active = rig; rig.select_set(True); bpy.ops.object.mode_set(mode='EDIT')
eb = rig.data.edit_bones; bone_names = {}
for ch in chains:
    prev = eb[ch['parent']]; names = []
    for b in range(len(ch['joints']) - 1):
        bone = eb.new(f"{ch['id']}.{b:02d}"); bone.head = Vector(ch['joints'][b]); bone.tail = Vector(ch['joints'][b + 1])
        bone.parent = prev; bone.use_connect = b > 0; bone.use_deform = True; prev = bone; names.append(bone.name)
    bone_names[ch['id']] = names
bpy.ops.object.mode_set(mode='OBJECT')

# Weights along chains: triangular bone basis, root blend into the parent body bone.
cols = [n for ch in chains for n in bone_names[ch['id']]] + pri
col = {n: i for i, n in enumerate(cols)}; W = np.zeros((nv, len(cols))); hair = np.zeros(nv, bool)
for ch in chains:
    J = ch['joints']; X = arm[ch['verts']]; nb = len(J) - 1; best = np.full(len(X), np.inf); u = np.zeros(len(X))
    for s in range(nb):
        d = J[s + 1] - J[s]; t = np.clip(((X - J[s]) @ d) / (d @ d), 0, 1); e = np.linalg.norm(X - (J[s] + t[:, None] * d), axis=1)
        better = e < best; best[better] = e[better]; u[better] = s + t[better]
    for b in range(nb):
        w = np.clip(1 - np.abs(u - (b + .5)), 0, 1)
        if b == 0: w[u < .5] = 1
        if b == nb - 1: w[u > nb - .5] = 1
        W[ch['verts'], col[bone_names[ch['id']][b]]] = w
    s = np.clip(ch['dist'] / ch['region']['free_distance_m'], 0, 1); c = s * s * (3 - 2 * s)  # 0 at skin, 1 when free
    Wc = W[ch['verts']]; Wc /= np.maximum(Wc.sum(1, keepdims=True), 1e-12)
    W[ch['verts']] = c[:, None] * Wc
    W[np.ix_(ch['verts'], [col[n] for n in pri])] += (1 - c)[:, None] * old[ch['verts']]
    hair[ch['verts']] = True
W[hair] /= W[hair].sum(1, keepdims=True)
# Diffuse across hair-only edges so neighbouring chains do not tear apart.
e = np.concatenate([tri[:, [0, 1]], tri[:, [1, 2]], tri[:, [2, 0]]]); e = e[hair[e].all(1)]
src, dst = np.r_[e[:, 0], e[:, 1]], np.r_[e[:, 1], e[:, 0]]; deg = np.bincount(src, minlength=nv).astype(float)
for _ in range(cfg['smooth_iterations']):
    avg = np.stack([np.bincount(src, weights=W[dst, j], minlength=nv) for j in range(len(cols))], 1)
    has = hair & (deg > 0); W[has] = 0.5 * W[has] + 0.5 * avg[has] / deg[has][:, None]
first_sum = np.zeros((canonical.max() + 1, len(cols))); np.add.at(first_sum, canonical[hair], W[hair])
cnt = np.bincount(canonical[hair], minlength=len(first_sum)); W[hair] = first_sum[canonical[hair]] / cnt[canonical[hair]][:, None]
idx = np.argsort(-W, 1)[:, cfg['maximum_influences']:]; np.put_along_axis(W, idx, 0, 1); W[hair] /= W[hair].sum(1, keepdims=True)

hv = np.flatnonzero(hair); existing = [vg for vg in obj.vertex_groups if vg.name.startswith('PRI.')]
for vg in existing: vg.remove(hv.tolist())
for n in cols:
    vg = obj.vertex_groups.get(n) or obj.vertex_groups.new(name=n)
    nz = hv[W[hv, col[n]] > 0]
    for i in nz: vg.add([int(i)], float(W[i, col[n]]), 'REPLACE')

# Rest invariance: evaluated mesh in rest pose must equal source rest.
rig.data.pose_position = 'REST'; dg = bpy.context.evaluated_depsgraph_get(); me = obj.evaluated_get(dg).to_mesh()
ev = np.empty(nv * 3); me.vertices.foreach_get('co', ev); obj.evaluated_get(dg).to_mesh_clear(); rig.data.pose_position = 'POSE'
rest_err = float(np.abs(ev.reshape(-1, 3) - co).max())
out.mkdir(parents=True)
(out / 'profile.json').write_bytes(profile.read_bytes()); (out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
blend = out / 'Mount-SecondaryChains.blend'; bpy.ops.wm.save_as_mainfile(filepath=str(blend), relative_remap=True)
rep = dict(schema_version=1, status='secondary-chain-rig-needs-review', cpu_only=True, input_sha256=cfg['input_sha256'], tool_sha256=sha(__file__),
           profile_sha256=sha(profile), candidate_sha256=sha(blend), chains=report, chain_bones=bone_names, hair_vertices=int(hair.sum()),
           rest_pose_max_error_m=rest_err, bones_total=len(rig.data.bones),
           limitations=['Chains follow clustered strand masses, not individual strands.', 'Leg feathering stays on leg bones.'])
(out / 'chain-report.json').write_text(json.dumps(rep, indent=1))
print('CHAINS', json.dumps(dict(chains=[(r['chain'], r['vertices'], round(r['length_m'], 3)) for r in report], rest_err=rest_err, bones=len(rig.data.bones))), flush=True)
