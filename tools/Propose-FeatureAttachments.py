"""CPU nearest-surface-node forest for explicitly selected disconnected features.

Proposals connect only parts of the same authored feature and never edit mesh
topology. They remain unapproved until semantic/rest/motion review establishes
the intended relation. Long or unsupported gaps remain unresolved.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import connected_components
from scipy.spatial import cKDTree

p = argparse.ArgumentParser(description=__doc__)
for key in ['selection', 'inspection', 'profile', 'output']:
    p.add_argument('--' + key, required=True)
a = p.parse_args()
selection_path, inspection_path, profile_path = map(Path, [a.selection, a.inspection, a.profile])
selection = json.loads(selection_path.read_text())
inspection = json.loads(inspection_path.read_text())
profile = json.loads(profile_path.read_text())
assert profile['schema_version'] == 1
limit = profile['maximum_proposed_gap_m']
assert type(limit) is float and .001 <= limit <= .05
assert selection['source_sha256'] == inspection['source_sha256']
entry = next(e for e in inspection['objects'] if e['object'] == selection['object'])
assert selection['mesh_hash'] == entry['mesh_hash']
data = np.load(selection_path.parent / selection['selection_file'])
source = np.load(inspection_path.parent / entry['selection_file'])
canonical = data['canonical_node']
_, first = np.unique(canonical, return_index=True)
labels = data['labels'][first]
rooted = np.isfinite(data['root_distance_m'][first])
points, edges = data['unique_world_vertices'], data['edges']
source_components = source['component_per_vertex'][first]
out = Path(a.output)
out.mkdir(parents=True, exist_ok=False)
proposals, unresolved, groups = [], [], []
for feature in selection['features']:
    nodes = np.flatnonzero(labels == feature['label'])
    remap = np.full(len(points), -1, dtype=np.int32)
    remap[nodes] = np.arange(len(nodes))
    es = edges[(labels[edges[:, 0]] == feature['label']) & (labels[edges[:, 1]] == feature['label'])]
    local_edges = remap[es]
    graph = coo_matrix((np.ones(len(es)*2), (np.r_[local_edges[:, 0], local_edges[:, 1]],
                       np.r_[local_edges[:, 1], local_edges[:, 0]])), shape=(len(nodes), len(nodes))).tocsr()
    count, component = connected_components(graph, directed=False)
    members = [nodes[component == cid] for cid in range(count)]
    visited = np.array([rooted[indices].any() for indices in members])
    assert visited.any(), feature['name'] + ' has no authored cut roots'
    distances = np.full((count, count), np.inf)
    pairs = {}
    for left, indices in enumerate(members):
        other = np.flatnonzero(component != left)
        tree = cKDTree(points[indices])
        ds, nearest = tree.query(points[nodes[other]])
        for right in range(left + 1, count):
            subset = np.flatnonzero(component[other] == right)
            if not len(subset):
                continue
            k = subset[np.argmin(ds[subset])]
            distances[left, right] = distances[right, left] = ds[k]
            pairs[(left, right)] = (int(indices[nearest[k]]), int(nodes[other[k]]))
    added = 0
    while not visited.all():
        candidates = distances.copy()
        candidates[~visited, :] = np.inf
        candidates[:, visited] = np.inf
        parent, child = np.unravel_index(np.argmin(candidates), candidates.shape)
        gap = float(candidates[parent, child])
        if gap > limit:
            break
        left, right = sorted([parent, child])
        endpoint_left, endpoint_right = pairs[(left, right)]
        parent_node, child_node = ((endpoint_left, endpoint_right) if parent == left
                                   else (endpoint_right, endpoint_left))
        component_ids = sorted(set(source_components[members[child]].tolist()))
        proposals.append(dict(id=f"{feature['name']}:B{added:03}", feature=feature['name'],
            status='needs-review', parent_selection_component=int(parent),
            child_selection_component=int(child), parent_node=parent_node, child_node=child_node,
            gap_m=gap, child_source_component_ids=[entry['components'][cid]['id'] for cid in component_ids],
            child_nodes=int(len(members[child])), relation='candidate skinned clump attachment; no topology welding'))
        visited[child] = True
        added += 1
    for cid in np.flatnonzero(~visited):
        unresolved.append(dict(feature=feature['name'], selection_component=int(cid),
            source_component_ids=[entry['components'][i]['id'] for i in sorted(set(source_components[members[cid]].tolist()))],
            nodes=int(len(members[cid])), nearest_attached_gap_m=float(distances[cid, visited].min())))
    groups.append(dict(feature=feature['name'], components=count, proposed_bindings=added,
                       unresolved_components=int((~visited).sum())))
report = dict(schema_version=1, status='needs-review', source_sha256=selection['source_sha256'],
    mesh_hash=entry['mesh_hash'], selection_path=str(selection_path),
    selection_sha256=hashlib.sha256(selection_path.read_bytes()).hexdigest(),
    profile_path=str(profile_path), profile_sha256=hashlib.sha256(profile_path.read_bytes()).hexdigest(),
    tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    maximum_proposed_gap_m=limit, groups=groups, proposals=proposals, unresolved=unresolved,
    note='Minimum gap forest among selected surface vertices; not triangle-accurate distance, semantic approval, physical attachment, or an instruction to delete any part.')
(out / 'attachment-proposals.json').write_text(json.dumps(report, indent=2))
print(json.dumps(dict(groups=groups, max_proposed_gap_m=max([r['gap_m'] for r in proposals], default=0),
                      unresolved=unresolved), indent=2))
