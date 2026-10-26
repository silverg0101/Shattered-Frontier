"""CPU coverage-driven guide proposals on existing semantic/root graphs.

No mesh, selections, weights, animation or attachment approvals are changed.
Nearest-segment distance is a rest coverage diagnostic, not contact acceptance.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import dijkstra


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def segment_distance(points, line):
    result = np.full(len(points), np.inf)
    starts, vectors = line[:-1], np.diff(line, axis=0)
    for offset in range(0, len(points), 4096):
        selected = points[offset:offset+4096]
        u = np.clip(np.einsum('nsi,si->ns', selected[:, None]-starts, vectors)/np.sum(vectors*vectors, axis=1), 0., 1.)
        result[offset:offset+4096] = np.sqrt(np.min(np.sum((selected[:, None]-starts-u[..., None]*vectors)**2, axis=-1), axis=1))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ['guide-report', 'profile', 'output']:
        parser.add_argument('--'+key, required=True)
    args = parser.parse_args()
    guide_path, profile_path, out = map(Path, [args.guide_report, args.profile, args.output])
    guide = json.loads(guide_path.read_text(encoding='utf-8'))
    profile = json.loads(profile_path.read_text(encoding='utf-8'))
    assert profile['schema_version'] == 1
    cap, warning = profile['maximum_guides_per_feature'], profile['coverage_warning_m']
    assert type(cap) is int and 1 <= cap <= 48
    assert type(warning) is float and .01 <= warning <= .10
    surface_path = Path(profile['surface_profile'])
    assert sha(surface_path) == profile['surface_profile_sha256']
    surface = json.loads(surface_path.read_text(encoding='utf-8'))
    assert profile['existing_root_exemption_extra_m'] == surface['limits']['root_exemption_extra_m']
    selection_path, attachments_path = Path(guide['selection_path']), Path(guide['attachment_path'])
    assert sha(selection_path) == guide['selection_sha256'] and sha(attachments_path) == guide['attachment_sha256']
    selection = json.loads(selection_path.read_text(encoding='utf-8'))
    attachments = json.loads(attachments_path.read_text(encoding='utf-8'))
    feature_path, motion_path = Path(guide['profile_path']), Path(guide['motion_profile_path'])
    assert sha(feature_path) == guide['profile_sha256'] and sha(motion_path) == guide['motion_profile_sha256']
    assert guide['mesh_hash'] == selection['mesh_hash'] == attachments['mesh_hash']
    assert not attachments['unresolved']
    feature_profile = json.loads(feature_path.read_text(encoding='utf-8'))
    guides_path = guide_path.parent/guide['guide_file']
    original = json.loads(guides_path.read_text(encoding='utf-8'))
    with np.load(selection_path.parent/selection['selection_file'], allow_pickle=False) as data:
        canonical, points, edges, root_vertices = data['canonical_node'], data['unique_world_vertices'], data['edges'], data['root_nodes']
    with np.load(guide_path.parent/guide['weight_field_file'], allow_pickle=False) as field:
        labels, protected, root_distance = field['labels'], field['motion_protected'], field['root_distance_m']
        assert np.array_equal(canonical, field['canonical_node'])
    _, representatives = np.unique(canonical, return_index=True)
    labels, protected = labels[representatives], protected[representatives]
    root_distance = root_distance[representatives]
    roots = root_vertices[representatives] | protected
    bindings = np.array([[row['parent_node'], row['child_node']] for row in attachments['proposals']], dtype=np.int32).reshape(-1, 2)
    virtual = {tuple(sorted(pair)) for pair in bindings.tolist()}
    all_edges = np.concatenate([edges, bindings])
    lengths = np.linalg.norm(points[all_edges[:, 0]]-points[all_edges[:, 1]], axis=1)
    graph = coo_matrix((np.tile(lengths, 2), (np.r_[all_edges[:, 0], all_edges[:, 1]], np.r_[all_edges[:, 1], all_edges[:, 0]])), shape=(len(points), len(points))).tocsr()
    proposed, records, arrays = [], [], {}
    for label, feature in enumerate(feature_profile['features'], 1):
        name = feature['name']
        nodes = np.flatnonzero(labels == label)
        remap = np.full(len(points), -1, dtype=np.int32)
        remap[nodes] = np.arange(len(nodes))
        root_local = np.flatnonzero(roots[nodes])
        assert len(root_local)
        distance, previous, _ = dijkstra(graph[nodes][:, nodes], directed=False, indices=root_local, min_only=True, return_predecessors=True)
        assert np.isfinite(distance).all()
        eligible = ~protected[nodes] & (root_distance[nodes] > feature['root_feather_distance_m']+profile['existing_root_exemption_extra_m'])
        assert np.any(eligible)
        existing = [g for g in original if g['feature'] == name]
        assert len(existing) <= cap
        nearest = np.full(len(nodes), np.inf)
        for row in existing:
            nearest = np.minimum(nearest, segment_distance(points[nodes], np.array(row['points'])))
        baseline = nearest.copy()
        additions = []
        tried = np.zeros(len(nodes), dtype=bool)
        while len(existing)+len(additions) < cap:
            candidates = eligible & ~tried & (distance >= feature['minimum_guide_length_m'])
            if not np.any(candidates) or nearest[candidates].max() <= warning:
                break
            tip = int(np.argmax(np.where(candidates, nearest, -1.)))
            tried[tip] = True
            path, cursor = [], tip
            while cursor >= 0:
                path.append(int(nodes[cursor]))
                cursor = int(previous[cursor])
                assert len(path) <= len(nodes), 'Predecessor cycle'
            path.reverse()
            line = points[path]
            arc = np.r_[0., np.cumsum(np.linalg.norm(np.diff(line, axis=0), axis=1))]
            if arc[-1] < feature['minimum_guide_length_m']:
                continue
            values = np.linspace(0., arc[-1], feature['guide_nodes'])
            sampled = np.column_stack([np.interp(values, arc, line[:, axis]) for axis in range(3)])
            if np.linalg.norm(np.diff(sampled, axis=0), axis=1).min() <= 1e-5:
                continue
            new_distance = segment_distance(points[nodes], sampled)
            # Never consume the finite proposal budget on an ineffective sampled path.
            if new_distance[tip] >= nearest[tip]-1e-6:
                continue
            row = dict(name=f'{name}_COVERAGE_{len(additions):02}', feature=name, points=sampled.tolist(),
                source_path_nodes=path, root_node=path[0], length_m=float(arc[-1]),
                virtual_edges_in_path=[list(pair) for pair in zip(path[:-1], path[1:]) if tuple(sorted(pair)) in virtual],
                target_node=int(nodes[tip]), previous_distance_m=float(nearest[tip]), status='unreviewed-coverage-proposal')
            additions.append(row)
            nearest = np.minimum(nearest, new_distance)
        def stats(values):
            return dict(maximum_m=float(values[eligible].max()), p95_m=float(np.quantile(values[eligible], .95)),
                        warning_nodes=int(np.count_nonzero(values[eligible] > warning)))
        records.append(dict(feature=name, unique_semantic_nodes=len(nodes), evaluated_free_nodes=int(eligible.sum()),
            protected_or_existing_root_band_nodes=int(np.count_nonzero(~eligible)), original_guides=len(existing), proposed_additions=len(additions),
            original=stats(baseline), proposed=stats(nearest), status='needs-semantic-and-path-review'))
        arrays[name+'_node_ids'], arrays[name+'_eligible'] = nodes, eligible
        arrays[name+'_original_distance_m'], arrays[name+'_proposed_distance_m'] = baseline, nearest
        proposed.extend(additions)
    out.mkdir(parents=True, exist_ok=False)
    (out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
    (out/'profile.json').write_bytes(profile_path.read_bytes())
    (out/'additional-guides.json').write_text(json.dumps(proposed, indent=2)+'\n', encoding='utf-8')
    np.savez_compressed(out/'coverage.npz', **arrays)
    result = dict(schema_version=1, status='coverage-proposals-only; not-simulated-or-skinned', cpu_only=True,
        mesh_hash=guide['mesh_hash'], source_sha256=guide['source_sha256'], original_guides=len(original), additions=len(proposed),
        features=records, tool_sha256=sha(Path(__file__)),
        coverage_file_sha256=sha(out/'coverage.npz'), additional_guides_sha256=sha(out/'additional-guides.json'),
        input_sha256={str(p):sha(p) for p in [guide_path, profile_path, selection_path, attachments_path, feature_path, motion_path, guides_path,
            selection_path.parent/selection['selection_file'], guide_path.parent/guide['weight_field_file'],surface_path]},
        limitations=['Nearest rest-segment point coverage is not strand identity, centerline, width, skin assignment, motion or contact acceptance.',
            'Existing semantic regions and proposed virtual attachments remain unapproved; no source topology or root decisions changed.',
            'Graph paths follow mesh surfaces and may cross unrelated fused branches; inspect them before use.',
            'Existing root-band exemption reproduced for comparable diagnostics, not enlarged; excluded nodes remain separately counted.',
            'Additional guides imply higher bone/solver cost and are not an approved runtime budget.',
            'No model/source/weights/animation/export change or new repair attempt.'])
    (out/'coverage-report.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({k:result[k] for k in ['status','original_guides','additions','features']}))


if __name__ == '__main__':
    main()
