"""CPU guide/weight-field proposals with explicit disconnected-clump relations.

The optional proposal flag permits a labelled rig experiment, not acceptance of
those attachments. Logical relation edges are preserved separately from geometry.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import dijkstra, connected_components

p = argparse.ArgumentParser(description=__doc__)
for key in ['selection', 'attachments', 'output']:
    p.add_argument('--' + key, required=True)
p.add_argument('--allow-proposed-bindings', action='store_true')
p.add_argument('--motion-profile', help='Additional authored rigid/anchor regions; semantic labels remain unchanged')
a = p.parse_args()
path = Path(a.selection)
selection = json.loads(path.read_text())
attachment_path = Path(a.attachments)
attachments = json.loads(attachment_path.read_text())
assert attachments['selection_sha256'] == hashlib.sha256(path.read_bytes()).hexdigest()
assert attachments['mesh_hash'] == selection['mesh_hash']
assert not attachments['unresolved'], 'Unresolved gaps require repair/review before guide building'
if any(row['status'] != 'approved' for row in attachments['proposals']):
    assert a.allow_proposed_bindings, 'Use explicit experimental flag or reviewed relations'
data = np.load(path.parent / selection['selection_file'])
profile = json.loads(Path(selection['profile_path']).read_text())
assert hashlib.sha256(Path(selection['profile_path']).read_bytes()).hexdigest() == selection['profile_sha256']
canonical = data['canonical_node']
_, first = np.unique(canonical, return_index=True)
labels, roots = data['labels'][first], data['root_nodes'][first]
points = data['unique_world_vertices']
motion_protected = data['explicit_protected'][first].copy()
if a.motion_profile:
    motion_profile_path = Path(a.motion_profile)
    motion_profile = json.loads(motion_profile_path.read_text())
    for region in motion_profile['skin']['protected_weight_bounds_m']:
        bounds = np.array(region['bounds'], dtype=float)
        assert bounds.shape == (3, 2) and np.all(bounds[:, 0] < bounds[:, 1])
        motion_protected |= np.all((points >= bounds[:, 0]) & (points <= bounds[:, 1]), axis=1)
    roots |= motion_protected
edges = data['edges']
bindings = np.array([[row['parent_node'], row['child_node']] for row in attachments['proposals']], dtype=np.int32).reshape(-1, 2)
all_edges = np.concatenate([edges, bindings])
length = np.linalg.norm(points[all_edges[:, 0]]-points[all_edges[:, 1]], axis=1)
graph = coo_matrix((np.tile(length, 2), (np.r_[all_edges[:, 0], all_edges[:, 1]],
                   np.r_[all_edges[:, 1], all_edges[:, 0]])), shape=(len(points), len(points))).tocsr()
out = Path(a.output)
out.mkdir(parents=True, exist_ok=False)
(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
if a.motion_profile:
    (out/'motion-profile.json').write_bytes(Path(a.motion_profile).read_bytes())
guides, records = [], []
distance_all = np.zeros(len(points), dtype=np.float32)
for label, feature in enumerate(profile['features'], 1):
    selected = labels == label
    nodes = np.flatnonzero(selected)
    remap = np.full(len(points), -1, dtype=np.int32)
    remap[nodes] = np.arange(len(nodes))
    local_roots = remap[np.flatnonzero(selected & roots)]
    subgraph = graph[nodes][:, nodes]
    assert len(local_roots)
    distance, previous, source = dijkstra(subgraph, directed=False, indices=local_roots,
                                         min_only=True, return_predecessors=True)
    assert np.isfinite(distance).all(), 'No rooted path for some selected geometry'
    distance_all[nodes] = distance
    candidates = np.flatnonzero(distance >= feature['minimum_guide_length_m'])
    separation = np.full(len(nodes), np.inf)
    count = 0
    for index in range(feature['guides']):
        score = np.minimum(separation[candidates], distance[candidates])
        tip = int(candidates[np.argmax(score)])
        if score.max() < .035:
            break
        path_nodes, cursor = [], tip
        while cursor >= 0:
            path_nodes.append(int(nodes[cursor]))
            cursor = int(previous[cursor])
        path_nodes.reverse()
        line = points[path_nodes]
        arc = np.r_[0, np.cumsum(np.linalg.norm(np.diff(line, axis=0), axis=1))]
        if arc[-1] < feature['minimum_guide_length_m']:
            break
        values = np.linspace(0, arc[-1], feature['guide_nodes'])
        sampled = np.column_stack([np.interp(values, arc, line[:, axis]) for axis in range(3)])
        assert np.linalg.norm(np.diff(sampled, axis=0), axis=1).min() > 1e-5
        guides.append(dict(name=f"{feature['name']}_G{index:02}", feature=feature['name'],
                           points=sampled.tolist(), source_path_nodes=path_nodes,
                           root_node=path_nodes[0], length_m=float(arc[-1])))
        separation = np.minimum(separation, np.linalg.norm(points[nodes]-points[nodes[tip]], axis=1))
        count += 1
    weights = np.clip(distance_all[nodes]/feature['root_feather_distance_m'], 0, 1)
    weights = weights*weights*(3-2*weights)
    records.append(dict(feature=feature['name'], guides=count, root_nodes=len(local_roots),
                        finite_root_paths=True, geometric_and_virtual_edges_distinguished=True,
                        rooted_components=int(connected_components(subgraph, directed=False)[0])))
np.savez_compressed(out/'weight-field.npz', labels=data['labels'],
                    explicit_protected=data['explicit_protected'], motion_protected=motion_protected[canonical],
                    root_distance_m=distance_all[canonical], canonical_node=canonical)
(out/'guides.json').write_text(json.dumps(guides, indent=2))
report = dict(schema_version=1, status='diagnostic-rig-proposal', source_sha256=selection['source_sha256'],
    mesh_hash=selection['mesh_hash'], object=selection['object'], selection_path=str(path),
    selection_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
    attachment_path=str(attachment_path), attachment_sha256=hashlib.sha256(attachment_path.read_bytes()).hexdigest(),
    profile_path=selection['profile_path'], profile_sha256=selection['profile_sha256'],
    guide_file='guides.json', weight_field_file='weight-field.npz', features=records,
    proposed_bindings_used=bool(a.allow_proposed_bindings),
    motion_profile_path=str(Path(a.motion_profile)) if a.motion_profile else None,
    motion_profile_sha256=hashlib.sha256(Path(a.motion_profile).read_bytes()).hexdigest() if a.motion_profile else None,
    additional_motion_protected_vertices=int(motion_protected[canonical].sum()),
    required_reviews=['Semantic boundaries and protected collar patch', 'Proposed separate-clump attachments',
                      'Surface-path guides vs actual strand deformation', 'Motion/surface collision and exports'])
(out/'guide-report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(dict(guides=len(guides), features=records, proposed_bindings=a.allow_proposed_bindings)))
