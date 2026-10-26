"""Freeze authored view-space feature cuts into source-hashed seam-aware selections.

Uses NumPy/SciPy on CPU. Produces candidate labels, root distances and surface
guide paths; does not claim semantic approval or edit any geometry.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import dijkstra, connected_components

parser = argparse.ArgumentParser(description=__doc__)
for key in ['inspection', 'views', 'profile', 'output']:
    parser.add_argument('--' + key, required=True)
args = parser.parse_args()
inspection_path, view_path, profile_path = map(Path, [args.inspection, args.views, args.profile])
inspection = json.loads(inspection_path.read_text())
views = json.loads(view_path.read_text())
profile = json.loads(profile_path.read_text())
assert profile['schema_version'] == 1
assert inspection['source_sha256'] == views['source_sha256']
entry = next(e for e in inspection['objects'] if e['object'] == views['object'])
data = np.load(inspection_path.parent / entry['selection_file'])
local, points, triangles = data['local_vertices'], data['vertices'], data['triangles']
assert hashlib.sha256(local.tobytes()+triangles.tobytes()).hexdigest() == entry['mesh_hash']
out = Path(args.output)
out.mkdir(parents=True, exist_ok=False)
view = next(v for v in views['views'] if v['variant'] == profile['view']['variant']
            and v['view'] == profile['view']['name'])
matrix = np.linalg.inv(np.array(view['camera_world']))
camera_points = points @ matrix[:3, :3].T + matrix[:3, 3]
resolution = profile['view']['resolution']
assert type(resolution) is int and 256 <= resolution <= 2048
pixels = np.column_stack([camera_points[:, 0], -camera_points[:, 1]])
pixels = pixels * resolution / view['ortho_scale'] + resolution / 2


def inside(polygon):
    poly = np.array(polygon, dtype=float)
    assert poly.shape[1] == 2 and len(poly) >= 3 and np.isfinite(poly).all()
    x, y = pixels.T
    selected = np.zeros(len(points), dtype=bool)
    for start, end in zip(poly, np.roll(poly, -1, axis=0)):
        if abs(end[1]-start[1]) < 1e-12:
            continue
        crosses = ((start[1] > y) != (end[1] > y))
        selected ^= crosses & (x < (end[0]-start[0]) * (y-start[1]) / (end[1]-start[1]) + start[0])
    return selected


protected = np.zeros(len(points), dtype=bool)
for region in profile['protected_regions']:
    region_mask = inside(region['polygon_pixels'])
    if 'coordinate_bounds_m' in region:
        bounds = np.array(region['coordinate_bounds_m'], dtype=float)
        assert bounds.shape == (3, 2) and np.isfinite(bounds).all()
        assert np.all(bounds[:, 0] < bounds[:, 1])
        region_mask &= np.all((points >= bounds[:, 0]) & (points <= bounds[:, 1]), axis=1)
    protected |= region_mask
labels = np.zeros(len(points), dtype=np.int16)
for index, feature in enumerate(profile['features'], 1):
    assert feature['name'] and 1 <= feature['guides'] <= 64 and 3 <= feature['guide_nodes'] <= 32
    assert .005 <= feature['root_feather_distance_m'] <= .5
    mask = inside(feature['polygon_pixels']) & ~protected
    assert not np.any(labels[mask]), 'Feature proposals overlap; author a boundary'
    labels[mask] = index
# Exact-position nodes link UV seams without modifying the original mesh.
unique, representatives, inverse = np.unique(local, axis=0, return_index=True, return_inverse=True)
node_points = points[representatives]
node_labels = labels[representatives]
assert np.array_equal(labels, node_labels[inverse]), 'Coincident vertices received inconsistent semantics'
faces = inverse[triangles]
edges = np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]])
edges = np.unique(np.sort(edges, axis=1), axis=0)
edges = edges[edges[:, 0] != edges[:, 1]]
lengths = np.linalg.norm(node_points[edges[:, 0]] - node_points[edges[:, 1]], axis=1)
graph = coo_matrix((np.tile(lengths, 2), (np.r_[edges[:, 0], edges[:, 1]],
                   np.r_[edges[:, 1], edges[:, 0]])), shape=(len(unique), len(unique))).tocsr()
records, guides = [], []
node_distance = np.zeros(len(unique), dtype=np.float32)
node_roots = np.zeros(len(unique), dtype=bool)
for index, feature in enumerate(profile['features'], 1):
    selected = node_labels == index
    nodes = np.flatnonzero(selected)
    assert len(nodes)
    cut = selected[edges[:, 0]] != selected[edges[:, 1]]
    roots = np.unique(edges[cut].ravel())
    roots = roots[selected[roots]]
    local_index = np.full(len(unique), -1, dtype=np.int32)
    local_index[nodes] = np.arange(len(nodes))
    subgraph = graph[nodes][:, nodes]
    if len(roots):
        distance, predecessors, _ = dijkstra(subgraph, directed=False, indices=local_index[roots],
                                             min_only=True, return_predecessors=True)
    else:
        distance, predecessors = np.full(len(nodes), np.inf), np.full(len(nodes), -9999)
    node_distance[nodes] = distance
    node_roots[roots] = True
    component_count, components = connected_components(subgraph, directed=False)
    rootless = np.flatnonzero(~np.isfinite(distance))
    # Farthest surface paths provide reproducible guide proposals. Rootless
    # fragments are explicitly unresolved, never silently declared attached.
    candidates = np.flatnonzero(np.isfinite(distance) & (distance >= feature['minimum_guide_length_m']))
    chosen = []
    separation = np.full(len(nodes), np.inf)
    for _ in range(feature['guides']):
        if not len(candidates):
            break
        score = np.minimum(separation[candidates], distance[candidates])
        tip = candidates[np.argmax(score)]
        if score.max() < .035:
            break
        path, cursor = [], int(tip)
        while cursor >= 0:
            path.append(int(nodes[cursor]))
            cursor = int(predecessors[cursor])
        path.reverse()
        line = node_points[path]
        arc = np.r_[0, np.cumsum(np.linalg.norm(np.diff(line, axis=0), axis=1))]
        if arc[-1] < feature['minimum_guide_length_m']:
            break
        parameters = np.linspace(0, arc[-1], feature['guide_nodes'])
        resampled = np.column_stack([np.interp(parameters, arc, line[:, axis]) for axis in range(3)])
        guides.append(dict(feature=feature['name'], points=resampled.tolist(),
                           path_node_indices=path, root_node=int(path[0]), tip_node=int(path[-1]),
                           length_m=float(arc[-1]), type='surface-path-proposal'))
        chosen.append(int(nodes[tip]))
        separation = np.minimum(separation, np.linalg.norm(node_points[nodes]-node_points[nodes[tip]], axis=1))
    records.append(dict(name=feature['name'], label=index, vertices=int((labels == index).sum()),
                        unique_position_nodes=len(nodes), cut_root_nodes=len(roots),
                        rootless_nodes=len(rootless), selected_graph_components=component_count,
                        guide_count=len(chosen), root_feather_distance_m=feature['root_feather_distance_m']))
np.savez_compressed(out / 'selections.npz', labels=labels, explicit_protected=protected,
                    root_distance_m=node_distance[inverse], root_nodes=node_roots[inverse],
                    canonical_node=inverse, unique_local_vertices=unique,
                    unique_world_vertices=node_points, edges=edges)
(out / 'guide-proposals.json').write_text(json.dumps(guides, indent=2))
report = dict(schema_version=1, status='needs-review', source=inspection['source'],
              source_sha256=inspection['source_sha256'], mesh_hash=entry['mesh_hash'], object=entry['object'],
              tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              profile_path=str(profile_path), profile_sha256=hashlib.sha256(profile_path.read_bytes()).hexdigest(),
              inspection_path=str(inspection_path), views_path=str(view_path),
              selection_file='selections.npz', guide_file='guide-proposals.json', features=records,
              explicit_protected_vertices=int(protected.sum()),
              selected_protected_vertices=int(np.count_nonzero(protected & (labels > 0))),
              note='Authored polygon proposals frozen as explicit selections. Require multiview semantic review; rootless fragments/guide paths are not accepted attachments or centerlines.')
(out / 'selection-report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
