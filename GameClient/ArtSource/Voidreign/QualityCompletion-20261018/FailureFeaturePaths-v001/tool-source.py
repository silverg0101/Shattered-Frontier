"""Trace finite measured failure vertices to existing authored feature roots.

Read-only proposals: surface graph paths are not approved hair centerlines.
No guide/weight/source/animation or collision-policy changes are made.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import dijkstra


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


p = argparse.ArgumentParser(description=__doc__)
for key in ['profile', 'output']:
    p.add_argument('--' + key, required=True)
a = p.parse_args()
profile_path, out = Path(a.profile), Path(a.output)
profile = json.loads(profile_path.read_text())
assert profile['schema_version'] == 1 and 1 <= len(profile['colliders']) <= 8
for path, expected in profile['input_sha256'].items():
    assert sha(path) == expected, path
gp, sp, rp = map(Path, [profile['guide_report'], profile['surface_report'], profile['retention_report']])
guide, surface, retention = [json.loads(v.read_text()) for v in [gp, sp, rp]]
assert retention['source_sha256'] == guide['source_sha256']
assert retention['candidate_sha256'] == surface['source_sha256']
assert all(retention['checks'].values())
sel_path, att_path = Path(guide['selection_path']), Path(guide['attachment_path'])
assert sha(sel_path) == guide['selection_sha256'] and sha(att_path) == guide['attachment_sha256']
selection, attachments = [json.loads(v.read_text()) for v in [sel_path, att_path]]
feature_path = Path(guide['profile_path'])
assert sha(feature_path) == guide['profile_sha256']
features = json.loads(feature_path.read_text())['features']
selection_file = sel_path.parent / selection['selection_file']
field_file = gp.parent / guide['weight_field_file']
guides_file = gp.parent / guide['guide_file']
old_guides = json.loads(guides_file.read_text())
with np.load(selection_file, allow_pickle=False) as d:
    canonical, points, edges, root_flags = [d[k] for k in ['canonical_node', 'unique_world_vertices', 'edges', 'root_nodes']]
with np.load(field_file, allow_pickle=False) as d:
    assert np.array_equal(canonical, d['canonical_node'])
    labels, protected = d['labels'], d['motion_protected']
_, first = np.unique(canonical, return_index=True)
assert np.array_equal(canonical[first], np.arange(len(points)))
labels, protected, roots = labels[first], protected[first], root_flags[first] | protected[first]
bindings = np.array([[v['parent_node'], v['child_node']] for v in attachments['proposals']], np.int32).reshape(-1, 2)
virtual = {tuple(sorted(e)) for e in bindings.tolist()}
all_edges = np.concatenate([edges, bindings])
length = np.linalg.norm(points[all_edges[:, 0]] - points[all_edges[:, 1]], axis=1)
graph = coo_matrix((np.tile(length, 2), (np.r_[all_edges[:, 0], all_edges[:, 1]],
    np.r_[all_edges[:, 1], all_edges[:, 0]])), shape=(len(points), len(points))).tocsr()
proposals, witnesses = [], []
for collider in profile['colliders']:
    failure = surface['collision_proxies'][collider]
    vertex = failure['vertex']
    assert vertex is not None and 0 <= vertex < len(canonical)
    node = int(canonical[vertex])
    label = int(labels[node])
    assert 1 <= label <= len(features) and not protected[node]
    feature = features[label - 1]
    nodes = np.flatnonzero(labels == label)
    local = int(np.searchsorted(nodes, node))
    root_indices = np.flatnonzero(roots[nodes])
    assert len(root_indices)
    distance, previous, _ = dijkstra(graph[nodes][:, nodes], directed=False,
        indices=root_indices, min_only=True, return_predecessors=True)
    assert np.isfinite(distance[local])
    path, cursor = [], local
    while cursor >= 0:
        path.append(int(nodes[cursor]))
        cursor = int(previous[cursor])
        assert len(path) <= len(nodes)
    path.reverse()
    line = points[path]
    arc = np.r_[0., np.cumsum(np.linalg.norm(np.diff(line, axis=0), axis=1))]
    assert arc[-1] >= feature['minimum_guide_length_m']
    sampled = np.column_stack([np.interp(np.linspace(0, arc[-1], feature['guide_nodes']), arc, line[:, axis]) for axis in range(3)])
    assert np.linalg.norm(np.diff(sampled, axis=0), axis=1).min() > 1e-5
    nearest = float('inf')
    for g in old_guides:
        if g['feature'] != feature['name']:
            continue
        rest = np.array(g['points'])
        starts, vec = rest[:-1], np.diff(rest, axis=0)
        u = np.clip(np.einsum('si,si->s', points[node] - starts, vec) / np.sum(vec * vec, axis=1), 0, 1)
        nearest = min(nearest, float(np.linalg.norm(points[node] - starts - u[:, None] * vec, axis=1).min()))
    proposals.append(dict(name=f"{feature['name']}_FAILURE_{len(proposals):02}", feature=feature['name'],
        points=sampled.tolist(), source_path_nodes=path, root_node=path[0], target_node=node,
        length_m=float(arc[-1]), virtual_edges_in_path=[list(e) for e in zip(path[:-1], path[1:]) if tuple(sorted(e)) in virtual],
        status='unreviewed-failure-path-proposal'))
    witnesses.append(dict(collider=collider, failure_vertex=vertex, failure_frame=failure['frame'],
        maximum_penetration_m=failure['maximum_penetration_m'], canonical_node=node,
        rest_position_m=points[node].tolist(), original_nearest_guide_m=nearest,
        proposed_path=proposals[-1]['name']))
assert not out.exists()
out.mkdir(parents=True)
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
(out / 'profile.json').write_bytes(profile_path.read_bytes())
(out / 'additional-guides.json').write_text(json.dumps(proposals, indent=2))
np.savez_compressed(out / 'coverage.npz', target_nodes=np.array([w['canonical_node'] for w in witnesses], np.int32))
consumed = [gp, sp, rp, sel_path, att_path, feature_path, selection_file, field_file, guides_file, profile_path]
report = dict(schema_version=1, status='failure-targeted-proposals-only', cpu_only=True,
    source_sha256=guide['source_sha256'], failure_source_sha256=surface['source_sha256'],
    mesh_hash=guide['mesh_hash'], additions=len(proposals), witnesses=witnesses,
    coverage_file_sha256=sha(out / 'coverage.npz'), additional_guides_sha256=sha(out / 'additional-guides.json'),
    input_sha256={str(f): sha(f) for f in consumed},
    limitations=['Paths terminate at measured vertices and are not necessarily complete clumps or centerlines.',
        'Paths may follow folded/hidden surfaces or virtual proposal edges; semantic/root review required.',
        'No source, selection, guide, weight, animation, collision limit or runtime-budget change.'])
(out / 'coverage-report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(dict(status=report['status'], witnesses=witnesses)), flush=True)
