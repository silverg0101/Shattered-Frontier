"""Freeze small unreviewed surface domains around failure-targeted graph paths.

Geodesic tubes are selection proposals, not semantic segmentation or proof of a
complete strand. Only geometric edges are used; no source or skin is changed.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import dijkstra, connected_components


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile', required=True)
p.add_argument('--output', required=True)
a = p.parse_args()
profile_path, out = Path(a.profile), Path(a.output)
profile = json.loads(profile_path.read_text())
assert profile['schema_version'] == 1
strategy = profile.get('selection_strategy', 'geodesic-tube')
assert strategy in ['geodesic-tube', 'plane-component']
radius = profile.get('geodesic_half_width_m')
if strategy == 'geodesic-tube':
    assert type(radius) is float and .005 <= radius <= .08
assert 1 <= len(profile['guides']) <= 4
for path, expected in profile['input_sha256'].items():
    assert sha(path) == expected, path
gp, pp, ip = map(Path, [profile['guide_report'], profile['failure_paths_report'], profile['inspection_report']])
guide, paths_report, inspection = [json.loads(f.read_text()) for f in [gp, pp, ip]]
entry = next(v for v in inspection['objects'] if v['object'] == guide['object'])
assert guide['source_sha256'] == inspection['source_sha256'] == paths_report['source_sha256']
assert guide['mesh_hash'] == entry['mesh_hash'] == paths_report['mesh_hash']
archive = ip.parent / entry['selection_file']
sel_path = Path(guide['selection_path'])
assert sha(sel_path) == guide['selection_sha256']
selection = json.loads(sel_path.read_text())
sel_archive = sel_path.parent / selection['selection_file']
field_file = gp.parent / guide['weight_field_file']
paths_file = pp.parent / 'additional-guides.json'
assert sha(paths_file) == paths_report['additional_guides_sha256']
paths = {v['name']: v for v in json.loads(paths_file.read_text())}
consumed = [profile_path, gp, pp, ip, archive, sel_path, sel_archive, field_file, paths_file]
sections = None
if strategy == 'plane-component':
    section_file = Path(profile['sections_report'])
    assert sha(section_file) == profile['sections_report_sha256']
    sections = json.loads(section_file.read_text())
    assert sections['source_sha256'] == guide['source_sha256'] and sections['mesh_hash'] == guide['mesh_hash']
    consumed.append(section_file)
input_hashes = {str(f): sha(f) for f in consumed}
with np.load(archive, allow_pickle=False) as d:
    local, faces = d['local_vertices'], d['triangles']
assert hashlib.sha256(local.astype(np.float32).tobytes() + faces.astype(np.int32).tobytes()).hexdigest() == guide['mesh_hash']
with np.load(sel_archive, allow_pickle=False) as d:
    canonical, points, edges = d['canonical_node'], d['unique_world_vertices'], d['edges']
with np.load(field_file, allow_pickle=False) as d:
    assert np.array_equal(canonical, d['canonical_node'])
    labels, protected = d['labels'], d['motion_protected']
_, first = np.unique(canonical, return_index=True)
labels, protected = labels[first], protected[first]
assert np.array_equal(canonical[first], np.arange(len(points)))
assert len(local) == len(canonical)
length = np.linalg.norm(points[edges[:, 0]] - points[edges[:, 1]], axis=1)
graph = coo_matrix((np.tile(length, 2), (np.r_[edges[:, 0], edges[:, 1]],
    np.r_[edges[:, 1], edges[:, 0]])), shape=(len(points), len(points))).tocsr()
arrays, domains = {}, []
for name in profile['guides']:
    row = paths[name]
    assert not row['virtual_edges_in_path'], 'Review attachment paths separately before domain tracing'
    target = int(row['target_node'])
    label = int(labels[target])
    assert label > 0 and not protected[target]
    nodes = np.flatnonzero((labels == label) & ~protected)
    path_nodes = np.array(row['source_path_nodes'], np.int32)
    assert np.all(labels[path_nodes] == label), 'Path crosses another semantic feature'
    free_path_nodes = path_nodes[~protected[path_nodes]]
    assert len(free_path_nodes) > 1
    seeds = np.searchsorted(nodes, free_path_nodes)
    subgraph = graph[nodes][:, nodes]
    included = np.zeros(len(points), bool)
    plane = None
    if strategy == 'geodesic-tube':
        distance = dijkstra(subgraph, directed=False, indices=seeds, min_only=True, limit=radius)
        included[nodes[np.isfinite(distance)]] = True
    else:
        fraction = profile['cut_fractions'][name]
        assert type(fraction) is float and 0 < fraction < 1
        path_sections = next(v for v in sections['paths'] if v['name'] == name)
        plane = next(s for s in path_sections['sections'] if s['path_fraction'] == fraction)
        origin, normal = np.array(plane['origin_m']), np.array(plane['normal'])
        assert np.isfinite(origin).all() and np.isfinite(normal).all() and abs(np.linalg.norm(normal)-1) < 1e-8
        target_side = float((points[target] - origin) @ normal)
        assert abs(target_side) > 1e-5
        signed = (points[nodes] - origin) @ normal * np.sign(target_side)
        kept_nodes = nodes[signed >= 0]
        assert target in kept_nodes
        cut_graph = graph[kept_nodes][:, kept_nodes]
        _, component_ids = connected_components(cut_graph, directed=False)
        target_component = component_ids[np.searchsorted(kept_nodes, target)]
        included[kept_nodes[component_ids == target_component]] = True
    face_ids = np.flatnonzero(included[canonical[faces]].all(axis=1)).astype(np.int32)
    vertex_ids = np.unique(faces[face_ids]).astype(np.int32)
    domain_nodes = np.unique(canonical[vertex_ids])
    assert len(face_ids) and target in domain_nodes
    assert not protected[domain_nodes].any()
    assert np.all(labels[domain_nodes] == label)
    arrays[name + '_face_ids'], arrays[name + '_vertex_ids'] = face_ids, vertex_ids
    arrays[name + '_canonical_nodes'] = domain_nodes
    local_domain = np.searchsorted(nodes, domain_nodes)
    parts = int(connected_components(subgraph[local_domain][:, local_domain], directed=False, return_labels=False))
    domains.append(dict(name=name, feature=row['feature'], status=f'unreviewed-{strategy}-domain',
        target_node=target, canonical_nodes=len(domain_nodes), vertices=len(vertex_ids), faces=len(face_ids),
        geometric_components=parts, geodesic_half_width_m=radius, selection_strategy=strategy,
        cut_plane=(dict(path_fraction=plane['path_fraction'], origin_m=plane['origin_m'], normal=plane['normal']) if plane else None),
        root_node=row['root_node'], protected_path_nodes_omitted=int(protected[path_nodes].sum()),
        source_path_length_m=row['length_m'],
        bounds_m=[points[domain_nodes].min(0).tolist(), points[domain_nodes].max(0).tolist()]))
assert not out.exists()
out.mkdir(parents=True)
np.savez_compressed(out / 'domains.npz', **arrays)
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
(out / 'profile.json').write_bytes(profile_path.read_bytes())
for f, expected in input_hashes.items():
    assert sha(f) == expected, f
report = dict(schema_version=1, status='unreviewed-clump-domain-proposals-only', cpu_only=True,
    source_sha256=guide['source_sha256'], mesh_hash=guide['mesh_hash'], object=guide['object'],
    input_sha256=input_hashes, tool_sha256=sha(__file__),
    domain_archive='domains.npz', domain_archive_sha256=sha(out / 'domains.npz'), domains=domains,
    limitations=['Geodesic tube or distal half-space component; neither establishes a complete semantic lock, interior centerline or approved root.',
        'Fused branches may enter the tube; inspect isolated surfaces and cross sections before any weight change.',
        'Faces included only when all corners are in the proposed node domain; open selection boundaries are not mesh defects.',
        'No geometry, selection authority, weight, rig, motion, collider, acceptance or runtime budget changed.'])
(out / 'domain-report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(dict(status=report['status'], domains=domains)), flush=True)
