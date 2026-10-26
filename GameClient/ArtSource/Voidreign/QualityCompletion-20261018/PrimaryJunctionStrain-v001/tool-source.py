"""Measure indexed source edge/area distortion over a checked world-point clip.

Diagnostic only: no authored strain limits are inferred and no source changes.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(8 * 1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile', required=True)
p.add_argument('--output', required=True)
a = p.parse_args()
profile, out = Path(a.profile).resolve(), Path(a.output).resolve()
cfg = json.loads(profile.read_text(encoding='utf-8'))
assert cfg['schema_version'] == 1 and cfg['units'] == 'metres' and not out.exists()
for path, expected in cfg['input_sha256'].items():
    assert sha(path) == expected, path
assert all(cfg[key] in cfg['input_sha256'] for key in ['source', 'world_points', 'producer_report', 'rest_selection', 'feature_selection'])
producer = json.loads(Path(cfg['producer_report']).read_text())
assert producer['status'] == 'passed-indexed-animated-cache-reimport-only'
assert producer['input_sha256'][cfg['source']] == cfg['input_sha256'][cfg['source']]
with np.load(cfg['rest_selection'], allow_pickle=False) as archive:
    rest, faces = archive['vertices'].astype(np.float64), archive['triangles']
with np.load(cfg['feature_selection'], allow_pickle=False) as archive:
    labels = archive['labels']
points = np.load(cfg['world_points'], mmap_mode='r', allow_pickle=False)
assert points.shape == (producer['frames'], len(rest), 3) and points.dtype == np.float32
assert labels.shape == (len(rest),) and faces.ndim == 2 and faces.shape[1] == 3
assert faces.min() >= 0 and faces.max() < len(rest)
edges = np.unique(np.sort(np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]]), axis=1), axis=0)
rest_length = np.linalg.norm(rest[edges[:, 1]] - rest[edges[:, 0]], axis=1)
rest_area2 = np.linalg.norm(np.cross(rest[faces[:, 1]] - rest[faces[:, 0]], rest[faces[:, 2]] - rest[faces[:, 0]]), axis=1)
valid_edges, valid_faces = rest_length > 0, rest_area2 > 0
groups = [('all', np.ones(len(edges), bool), np.ones(len(faces), bool))]
for name, label in cfg['feature_labels'].items():
    assert type(label) is int
    groups.append((name, np.all(labels[edges] == label, axis=1), np.all(labels[faces] == label, axis=1)))
summary = {name: dict(indexed_edges=int(mask.sum()), triangles=int(face_mask.sum()),
    maximum_relative_edge_change=0., maximum_absolute_edge_change_m=0.,
    worst_frame_edge_p99_relative_change=0., worst_frame_area_p99_relative_change=0.,
    worst_frame_area_weighted_mean_relative_change=0.) for name, mask, face_mask in groups}
curve_rows = []
tested_frames = cfg.get('frames', list(range(1, len(points) + 1)))
assert isinstance(tested_frames, list) and tested_frames and len(set(tested_frames)) == len(tested_frames)
assert all(type(frame) is int and 1 <= frame <= len(points) for frame in tested_frames)
for frame in tested_frames:
    values = points[frame - 1]
    xyz = values.astype(np.float64)
    assert np.isfinite(xyz).all()
    length = np.linalg.norm(xyz[edges[:, 1]] - xyz[edges[:, 0]], axis=1)
    absolute = np.abs(length - rest_length)
    relative = np.zeros(len(edges))
    relative[valid_edges] = absolute[valid_edges] / rest_length[valid_edges]
    area2 = np.linalg.norm(np.cross(xyz[faces[:, 1]] - xyz[faces[:, 0]], xyz[faces[:, 2]] - xyz[faces[:, 0]]), axis=1)
    area_relative = np.zeros(len(faces))
    area_relative[valid_faces] = np.abs(area2[valid_faces] - rest_area2[valid_faces]) / rest_area2[valid_faces]
    per_frame = dict(frame=frame)
    for name, edge_mask, face_mask in groups:
        em, fm = edge_mask & valid_edges, face_mask & valid_faces
        assert em.any() and fm.any()
        row = summary[name]
        edge_ids = np.flatnonzero(em)
        index = int(edge_ids[np.argmax(relative[em])])
        maximum = float(relative[index])
        if maximum > row['maximum_relative_edge_change']:
            row.update(maximum_relative_edge_change=maximum, maximum_relative_change_frame=frame,
                maximum_relative_change_edge=edges[index].tolist(), worst_relative_rest_length_m=float(rest_length[index]))
        index = int(edge_ids[np.argmax(absolute[em])])
        if absolute[index] > row['maximum_absolute_edge_change_m']:
            row.update(maximum_absolute_edge_change_m=float(absolute[index]), maximum_absolute_change_frame=frame,
                maximum_absolute_change_edge=edges[index].tolist(), worst_absolute_rest_length_m=float(rest_length[index]))
        edge_p99 = float(np.quantile(relative[em], .99))
        area_p99 = float(np.quantile(area_relative[fm], .99))
        weighted = float(np.average(area_relative[fm], weights=rest_area2[fm]))
        row['worst_frame_edge_p99_relative_change'] = max(row['worst_frame_edge_p99_relative_change'], edge_p99)
        row['worst_frame_area_p99_relative_change'] = max(row['worst_frame_area_p99_relative_change'], area_p99)
        row['worst_frame_area_weighted_mean_relative_change'] = max(row['worst_frame_area_weighted_mean_relative_change'], weighted)
        per_frame[name] = dict(edge_p99_relative_change=edge_p99, area_p99_relative_change=area_p99,
                               area_weighted_mean_relative_change=weighted)
    curve_rows.append(per_frame)
out.mkdir(parents=True)
(out / 'profile.json').write_bytes(profile.read_bytes())
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
report = dict(schema_version=1, status='surface-strain-diagnostic-unrated', cpu_only=True,
    input_sha256=cfg['input_sha256'], tool_sha256=sha(__file__), profile_sha256=sha(profile),
    frames=len(tested_frames), source_clip_frames=len(points), tested_frames=tested_frames,
    representation='Unique indexed source edges; seam-split edges remain independently counted.',
    zero_rest_length_edges=int((~valid_edges).sum()), zero_rest_area_faces=int((~valid_faces).sum()), groups=summary,
    limitations=['No authored strain/style thresholds; this report cannot pass or fail material or model acceptance.',
        'Relative maxima can be dominated by tiny rest edges; absolute change, baseline lengths and quantiles are retained.',
        'Rotations and translations cancel in lengths/areas, but bending can alter triangle area in coarse meshes.',
        'Metadata/UV retention and contact passes do not imply acceptable deformed texture or surface shape.'])
(out / 'strain-report.json').write_text(json.dumps(report, indent=2))
(out / 'strain-curves.json').write_text(json.dumps(curve_rows, indent=2))
for path, expected in cfg['input_sha256'].items():
    assert sha(path) == expected, path
print(json.dumps({'status': report['status'], 'frames': len(tested_frames)}))
