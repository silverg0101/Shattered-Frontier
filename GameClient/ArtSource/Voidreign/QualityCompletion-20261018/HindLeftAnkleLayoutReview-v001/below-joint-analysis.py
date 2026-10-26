"""Source-indexed inventory of proposed ankle-band vertices hanging below the authored joint."""
import hashlib, json, sys
import numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import connected_components
B = 'GameClient/ArtSource/Voidreign/QualityCompletion-20261018/'
REST, LAY = B + 'DegenerateInspection-v001/00-component-selections.npz', B + 'HindLeftAnkleLayout-v001/joint-band-selections.npz'
JOINT = np.array([0.448, 0.812, 0.2])  # authored hind.L joint head (JointTransitionAtlas-v003 guides)
out = sys.argv[1]
sha = lambda p: hashlib.sha256(open(p, 'rb').read()).hexdigest()
with np.load(REST) as r: pts, tri = r['vertices'], r['triangles']
with np.load(LAY) as l: core, added, old = l['hind.L.core_vertices'], l['hind.L.added_band_vertices'], l['hind.L.old_collar_vertices']
low = added[pts[added, 2] < JOINT[2]]
m = np.zeros(len(pts), bool); m[low] = True
e = np.concatenate([tri[:, [0, 1]], tri[:, [1, 2]], tri[:, [2, 0]]])
e = e[m[e].all(1)]
idx = -np.ones(len(pts), int); idx[low] = np.arange(len(low))
g = coo_matrix((np.ones(len(e)), (idx[e[:, 0]], idx[e[:, 1]])), shape=(len(low),) * 2)
n, lab = connected_components(g, directed=False)
cm = np.zeros(len(pts), bool); cm[core] = True
touch = cm[tri].any(1)
comps = []
for c in range(n):
    v = low[lab == c]; p = pts[v]
    vm = np.zeros(len(pts), bool); vm[v] = True
    comps.append(dict(id=c, vertices=len(v), z_min_m=float(p[:, 2].min()), z_max_m=float(p[:, 2].max()),
        centroid_minus_joint_m=(p.mean(0) - JOINT).round(4).tolist(),
        shares_face_with_core=bool((vm[tri].any(1) & touch).any())))
comps.sort(key=lambda c: -c['vertices'])
np.savez_compressed(out + '/below-joint-band.npz', vertices=low, component=lab)
json.dump(dict(schema_version=1, input_sha256={REST: sha(REST), LAY: sha(LAY)}, joint_m=JOINT.tolist(),
    rule='added band vertices with rest Z below authored hind.L joint; components over edges among them only',
    below_joint_vertices=len(low), components=len(comps), largest=comps[:12],
    note='Forward is -Y. Inventory only; ownership (skin/feather/shell) is not inferred.'),
    open(out + '/below-joint-band.json', 'w'), indent=2)
print(len(low), n, json.dumps(comps[:8]))
