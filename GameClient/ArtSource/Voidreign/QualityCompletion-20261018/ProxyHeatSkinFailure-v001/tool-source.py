"""Primary skin from bone-heat weights solved on a voxel proxy, compared under an existing clip.

Run: blender -b --factory-startup --python tools/Build-MountProxyHeatSkin.py -- --profile <json> --output <new folder>
Only vertices whose existing weights use primary bones exclusively are re-weighted; feature/hair groups,
geometry, UVs, rig and animation are untouched. Rigid cores are forced to their foot bone.
"""
import argparse, hashlib, json, sys, time
from pathlib import Path
import bpy, bmesh
import numpy as np
from mathutils.bvhtree import BVHTree


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
assert cfg['schema_version'] == 1 and cfg['units'] == 'metres' and not out.exists()
assert 0.004 <= cfg['proxy_voxel_m'] <= 0.03 and 1 <= cfg['maximum_influences'] <= 4
for f, h in cfg['input_sha256'].items():
    assert sha(f) == h, f
with np.load(cfg['rest_selection']) as r:
    rest, tri = r['vertices'].astype(np.float64), r['triangles']
with np.load(cfg['semantic_selection']) as s:
    labels, canonical = s['labels'], s['canonical_node']

bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()))
scene = bpy.context.scene
obj, rig = bpy.data.objects[cfg['object']], bpy.data.objects[cfg['rig_object']]
names = [b.name for b in rig.data.bones if b.use_deform]
gidx = {obj.vertex_groups[n].index: k for k, n in enumerate(names) if n in obj.vertex_groups}
nv = len(obj.data.vertices); assert nv == len(rest)
_co = np.empty(nv * 3); obj.data.vertices.foreach_get('co', _co)
rest_offset_m = float(np.abs(_co.reshape(-1, 3) - rest).max())  # older rest file may predate source rest repairs
rest = _co.reshape(-1, 3).astype(np.float64)
assert np.array_equal(np.array([list(q.vertices) for q in obj.data.polygons]), tri), 'exact indexed triangles required'
old = np.zeros((nv, len(names))); foreign = np.zeros(nv, bool)
for v in obj.data.vertices:
    for g in v.groups:
        if g.group in gidx: old[v.index, gidx[g.group]] = g.weight
        elif g.weight > 0: foreign[v.index] = True
target = (~foreign) & (old.sum(1) > 0) & np.isin(labels, cfg['reweight_semantic_labels'])

# Proxy: body-only rest surface, voxel remeshed into a closed manifold for bone heat.
keep = np.isin(labels, cfg['proxy_semantic_labels'])
ft = tri[keep[tri].all(1)]
used, inv = np.unique(ft, return_inverse=True)
pm = bpy.data.meshes.new('PROXY | heat'); pm.from_pydata(rest[used].tolist(), [], inv.reshape(-1, 3).tolist()); pm.update()
proxy = bpy.data.objects.new('PROXY | heat skin', pm); scene.collection.objects.link(proxy)
proxy.matrix_world = obj.matrix_world.copy()
pm.remesh_voxel_size = cfg['proxy_voxel_m']; pm.remesh_voxel_adaptivity = 0.0
bpy.ops.object.select_all(action='DESELECT'); bpy.context.view_layer.objects.active = proxy; proxy.select_set(True)
bpy.ops.object.voxel_remesh()
proxy_faces = len(proxy.data.polygons)
rig.data.pose_position = 'REST'; scene.frame_set(cfg['frames'][0])
rig.select_set(True); bpy.context.view_layer.objects.active = rig
t0 = time.time(); bpy.ops.object.parent_set(type='ARMATURE_AUTO'); heat_s = time.time() - t0
rig.data.pose_position = 'POSE'
pidx = {proxy.vertex_groups[n].index: k for k, n in enumerate(names) if n in proxy.vertex_groups}
pw = np.zeros((len(proxy.data.vertices), len(names)))
for v in proxy.data.vertices:
    for g in v.groups:
        if g.group in pidx: pw[v.index, pidx[g.group]] = g.weight
unweighted_proxy = int((pw.sum(1) <= 0).sum())

# Transfer: nearest proxy surface point, barycentric weight interpolation, rest space.
bm = bmesh.new(); bm.from_mesh(proxy.data); bmesh.ops.triangulate(bm, faces=bm.faces[:]); bm.faces.ensure_lookup_table()
ptri = np.array([[v.index for v in f.verts] for f in bm.faces]); pco = np.array([v.co[:] for v in bm.verts])
tree = BVHTree.FromBMesh(bm)
new = old.copy(); far = 0.0
for i in np.flatnonzero(target):
    loc, _, fi, d = tree.find_nearest(rest[i]); far = max(far, d)
    A, B, C = pco[ptri[fi]]; v0, v1, v2 = B - A, C - A, np.array(loc) - A
    d00, d01, d11, d20, d21 = v0 @ v0, v0 @ v1, v1 @ v1, v2 @ v0, v2 @ v1
    den = d00 * d11 - d01 * d01
    b1 = (d11 * d20 - d01 * d21) / den if den > 1e-20 else 0.; b2 = (d00 * d21 - d01 * d20) / den if den > 1e-20 else 0.
    bary = np.clip([1 - b1 - b2, b1, b2], 0, 1); bary /= bary.sum()
    new[i] = bary @ pw[ptri[fi]]
for core in cfg['rigid_cores']:
    with np.load(core['path']) as c:
        ids = c[core['key']]
    new[ids] = 0; new[ids, names.index(core['bone'])] = 1
# Seam copies share one canonical weight (mean), then top-k and normalise.
cn = canonical[target]; sums = np.zeros((canonical.max() + 1, len(names))); cnt = np.bincount(cn, minlength=len(sums))
np.add.at(sums, cn, new[target]); new[target] = sums[cn] / cnt[cn][:, None]
k = cfg['maximum_influences']; idx = np.argsort(-new, 1)[:, k:]; np.put_along_axis(new, idx, 0, 1)
bad = target & (new.sum(1) <= 0); new[bad] = old[bad]
new[target] /= new[target].sum(1)[:, None]


def edges_of(mask):
    e = np.concatenate([tri[:, [0, 1]], tri[:, [1, 2]], tri[:, [2, 0]]]); e = np.unique(np.sort(e, 1), axis=0)
    return e[mask[e].all(1)]


groups = {'all': np.ones(nv, bool), 'body': labels == 0, 'Mane': labels == 1, 'Tail': labels == 2}
E = {g: edges_of(m) for g, m in groups.items()}
L0 = {g: np.linalg.norm(rest[e[:, 0]] - rest[e[:, 1]], axis=1) for g, e in E.items()}
cores = {c['id']: np.load(c['path'])[c['key']] for c in cfg['rigid_cores']}
core_edges = [(e, np.linalg.norm(rest[e[:, 0]] - rest[e[:, 1]], axis=1)) for e in (edges_of(np.isin(np.arange(nv), ids)) for ids in cores.values())]
first = np.unique(canonical, return_index=True)[1]


def write(w):
    for j, n in enumerate(names):
        vg = obj.vertex_groups[n]
        nz, z = np.flatnonzero(target & (w[:, j] > 0)), np.flatnonzero(target & (w[:, j] <= 0))
        for i in nz: vg.add([int(i)], float(w[i, j]), 'REPLACE')
        if len(z): vg.remove(z.tolist())


def measure(tag):
    res = {g: dict(max_rel=0., max_abs_m=0., worst_p99_rel=0., worst_frame=None) for g in E}
    res['core_max_edge_change_m'] = 0.; res['seam_gap_m'] = 0.; per = []
    for fr in cfg['frames']:
        scene.frame_set(fr); dg = bpy.context.evaluated_depsgraph_get()
        me = obj.evaluated_get(dg).to_mesh(); co = np.empty(len(me.vertices) * 3); me.vertices.foreach_get('co', co)
        obj.evaluated_get(dg).to_mesh_clear(); co = co.reshape(-1, 3); assert len(co) == nv
        row = {'frame': fr}
        for g, e in E.items():
            l = np.linalg.norm(co[e[:, 0]] - co[e[:, 1]], axis=1); d = np.abs(l - L0[g]); rel = d / np.maximum(L0[g], 1e-9)
            r = res[g]; p99 = float(np.percentile(rel, 99)); row[g] = p99
            if p99 > r['worst_p99_rel']: r['worst_p99_rel'], r['worst_frame'] = p99, fr
            r['max_rel'] = max(r['max_rel'], float(rel.max())); r['max_abs_m'] = max(r['max_abs_m'], float(d.max()))
        for e, l0 in core_edges:
            l = np.linalg.norm(co[e[:, 0]] - co[e[:, 1]], axis=1)
            res['core_max_edge_change_m'] = max(res['core_max_edge_change_m'], float(np.abs(l - l0).max()))
        res['seam_gap_m'] = max(res['seam_gap_m'], float(np.linalg.norm(co - co[first][canonical], axis=1).max()))
        per.append(row)
    print(tag, json.dumps({g: res[g] for g in res}), flush=True)
    return res, per


baseline, base_curve = measure('BASELINE')
write(new); candidate, cand_curve = measure('CANDIDATE')
bpy.data.objects.remove(proxy)
out.mkdir(parents=True)
(out / 'profile.json').write_bytes(profile.read_bytes()); (out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
np.savez_compressed(out / 'weights.npz', bones=np.array(names), target=np.flatnonzero(target), before=old[target], after=new[target])
blend = out / 'Mount-ProxyHeatSkin.blend'
if cfg.get('save_candidate', True): bpy.ops.wm.save_as_mainfile(filepath=str(blend), relative_remap=True)
report = dict(schema_version=1, status='skin-candidate-needs-review', cpu_only=True, input_sha256=cfg['input_sha256'],
              tool_sha256=sha(__file__), profile_sha256=sha(profile), candidate_sha256=sha(blend) if blend.exists() else None,
              reweighted_vertices=int(target.sum()), rest_selection_max_offset_m=rest_offset_m, fallback_vertices=int(bad.sum()), proxy_faces=proxy_faces,
              unweighted_proxy_vertices=unweighted_proxy, max_transfer_distance_m=far, heat_seconds=heat_s,
              baseline=baseline, candidate=candidate,
              limitations=['Same frozen clip and bone motion; skin only. Hair/feature groups and contact solvers untouched.',
                           'Edge strain is a distortion measure, not visual or anatomical approval.'])
(out / 'skin-report.json').write_text(json.dumps(report, indent=2))
(out / 'strain-curves.json').write_text(json.dumps(dict(baseline=base_curve, candidate=cand_curve)))
print('DONE', json.dumps({k: report[k] for k in ['reweighted_vertices', 'fallback_vertices', 'proxy_faces', 'unweighted_proxy_vertices', 'max_transfer_distance_m']}), flush=True)
