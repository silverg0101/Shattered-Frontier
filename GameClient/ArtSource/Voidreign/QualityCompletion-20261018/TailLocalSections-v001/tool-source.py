"""Measure source-mesh plane contours near finite unreviewed feature paths.

CPU geometric evidence only. A nearby closed contour is not semantic ownership,
a complete hair lock, an approved interior guide, or a motion repair.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def contours(triangles, origin, normal, weld):
    signed = (triangles - origin) @ normal
    mask = (signed.min(axis=1) < -1e-10) & (signed.max(axis=1) > 1e-10)
    selected, sd = triangles[mask], signed[mask]
    coordinates, edges, lookup = [], set(), {}
    for tri, values in zip(selected, sd):
        cross = []
        for i, j in [(0, 1), (1, 2), (2, 0)]:
            if (values[i] < 0) != (values[j] < 0):
                point = tri[i] + values[i] / (values[i] - values[j]) * (tri[j] - tri[i])
                key = tuple(np.round(point / weld).astype(np.int64))
                if key not in lookup:
                    lookup[key] = len(coordinates)
                    coordinates.append(point)
                cross.append(lookup[key])
        if len(cross) == 2 and cross[0] != cross[1]:
            edges.add(tuple(sorted(cross)))
    if not coordinates:
        return []
    points = np.array(coordinates)
    adjacency = [set() for _ in points]
    for i, j in edges:
        adjacency[i].add(j)
        adjacency[j].add(i)
    seen, result = set(), []
    for seed in range(len(points)):
        if seed in seen:
            continue
        stack, component = [seed], []
        seen.add(seed)
        while stack:
            node = stack.pop()
            component.append(node)
            for nxt in adjacency[node] - seen:
                seen.add(nxt)
                stack.append(nxt)
        ids = np.array(component)
        closed = len(ids) >= 3 and all(len(adjacency[int(i)]) == 2 for i in ids)
        row = dict(nodes=len(ids), closed_degree_two=closed,
            nearest_path_distance_m=float(np.linalg.norm(points[ids] - origin, axis=1).min()),
            diameter_bound_m=float(np.linalg.norm(np.ptp(points[ids], axis=0))))
        if closed:
            ordered, previous, cursor = [int(ids[0])], -1, int(ids[0])
            for _ in range(len(ids)):
                nxt = next(v for v in adjacency[cursor] if v != previous)
                if nxt == ordered[0]:
                    break
                ordered.append(nxt)
                previous, cursor = cursor, nxt
            assert len(ordered) == len(ids)
            ring = points[ordered]
            basis = np.eye(3)[np.argmin(np.abs(normal))]
            u = np.cross(normal, basis)
            u /= np.linalg.norm(u)
            v = np.cross(normal, u)
            xy = np.column_stack([(ring - origin) @ u, (ring - origin) @ v])
            shifted = np.roll(xy, -1, axis=0)
            cross = xy[:, 0] * shifted[:, 1] - shifted[:, 0] * xy[:, 1]
            twice_area = cross.sum()
            if abs(twice_area) > 1e-10:
                centroid2 = ((xy + shifted) * cross[:, None]).sum(axis=0) / (3 * twice_area)
                centroid = origin + centroid2[0] * u + centroid2[1] * v
                row.update(area_m2=float(abs(twice_area) / 2), centroid_m=centroid.tolist(),
                    maximum_radius_from_centroid_m=float(np.linalg.norm(ring - centroid, axis=1).max()),
                    contour_points_m=ring.tolist())
        result.append(row)
    return sorted(result, key=lambda r: r['nearest_path_distance_m'])


p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile', required=True)
p.add_argument('--output', required=True)
a = p.parse_args()
profile_path, out = Path(a.profile), Path(a.output)
profile = json.loads(profile_path.read_text())
assert profile['schema_version'] == 1 and 1 <= len(profile['guides']) <= 4
weld = profile['analysis_endpoint_weld_m']
assert type(weld) is float and 1e-8 <= weld <= 1e-5
fractions = profile['path_fractions']
assert 2 <= len(fractions) <= 20 and all(type(f) is float and 0 < f < 1 for f in fractions)
for f, expected in profile['input_sha256'].items():
    assert sha(f) == expected, f
gp, ip, pp = map(Path, [profile['guide_report'], profile['inspection_report'], profile['failure_paths_report']])
guide, inspection, paths_report = [json.loads(f.read_text()) for f in [gp, ip, pp]]
entry = next(e for e in inspection['objects'] if e['object'] == guide['object'])
assert inspection['source_sha256'] == guide['source_sha256'] == paths_report['source_sha256']
assert entry['mesh_hash'] == guide['mesh_hash'] == paths_report['mesh_hash']
sel_path = Path(guide['selection_path'])
assert sha(sel_path) == guide['selection_sha256']
selection = json.loads(sel_path.read_text())
selection_file = sel_path.parent / selection['selection_file']
mesh_file = ip.parent / entry['selection_file']
path_file = pp.parent / 'additional-guides.json'
assert sha(path_file) == paths_report['additional_guides_sha256']
consumed = [profile_path, gp, ip, pp, sel_path, selection_file, mesh_file, path_file]
hashes = {str(f): sha(f) for f in consumed}
with np.load(mesh_file, allow_pickle=False) as d:
    local, faces = d['local_vertices'], d['triangles']
assert hashlib.sha256(local.astype(np.float32).tobytes() + faces.astype(np.int32).tobytes()).hexdigest() == guide['mesh_hash']
with np.load(selection_file, allow_pickle=False) as d:
    points, canonical, labels = d['unique_world_vertices'], d['canonical_node'], d['labels']
paths = {v['name']: v for v in json.loads(path_file.read_text())}
records = []
for name in profile['guides']:
    row = paths[name]
    label = int(labels[np.flatnonzero(canonical == row['target_node'])[0]])
    triangles = points[canonical[faces[np.all(labels[faces] == label, axis=1)]]].astype(np.float64)
    line = points[row['source_path_nodes']].astype(np.float64)
    arc = np.r_[0., np.cumsum(np.linalg.norm(np.diff(line, axis=0), axis=1))]
    sections = []
    for f in fractions:
        center = np.array([np.interp(f * arc[-1], arc, line[:, axis]) for axis in range(3)])
        # A local arc window reduces single-triangle tangent noise; a diagnostic,
        # not a universal anatomical tangent or automatic guide authoring rule.
        ends = np.array([[np.interp(np.clip(f + offset, 0, 1) * arc[-1], arc, line[:, axis]) for axis in range(3)] for offset in [-.025, .025]])
        normal = ends[1] - ends[0]
        normal /= np.linalg.norm(normal)
        rings = contours(triangles, center, normal, weld)
        nearest = rings[:3]
        sections.append(dict(path_fraction=f, origin_m=center.tolist(), normal=normal.tolist(),
            contour_count=len(rings), nearest_contours=nearest))
    records.append(dict(name=name, status='cross-section-diagnostics-only', sections=sections))
assert not out.exists()
out.mkdir(parents=True)
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
(out / 'profile.json').write_bytes(profile_path.read_bytes())
for f, expected in hashes.items():
    assert sha(f) == expected, f
report = dict(schema_version=1, status='unreviewed-source-cross-sections-only', cpu_only=True,
    source_sha256=guide['source_sha256'], mesh_hash=guide['mesh_hash'], input_sha256=hashes,
    tool_sha256=sha(__file__), analysis_endpoint_weld_m=weld, paths=records,
    limitations=['Plane cuts of all same-feature triangles; nearby unrelated locks or fused branches remain possible.',
        'Numerical endpoint welding is analysis only and does not weld or alter the source mesh.',
        'Nearest closed degree-two contour and area centroid are geometric candidates, not semantic ownership or guaranteed interior points of concave sections.',
        'Coplanar triangles and tangencies omitted; no arbitrary topology or nested-shell acceptance.'])
(out / 'section-report.json').write_text(json.dumps(report, indent=2))
print(json.dumps([dict(name=r['name'], nearest=[dict(fraction=s['path_fraction'], **{k:v for k,v in s['nearest_contours'][0].items() if k != 'contour_points_m'}) if s['nearest_contours'] else None for s in r['sections']]) for r in records]), flush=True)
