"""Separate rigid island motion from shape change in two checked held samples.

Descriptive best fits are not corrective transforms or proof of joint semantics.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def rigid_fit(before, after):
    center, target = before.mean(0), after.mean(0)
    u, singular, vt = np.linalg.svd((before - center).T @ (after - target))
    if np.linalg.det(vt.T @ u.T) < 0:
        vt[-1] *= -1
    rotation = vt.T @ u.T
    fitted = (before - center) @ rotation.T + target
    return fitted, rotation, singular


p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile', required=True)
p.add_argument('--output', required=True)
a = p.parse_args()
profile, out = Path(a.profile).resolve(), Path(a.output).resolve()
cfg = json.loads(profile.read_text(encoding='utf-8'))
assert cfg['schema_version'] == 1 and cfg['coordinate_space'] == 'canonical-proxy-local-metres'
assert type(cfg['fps']) is int and 1 <= cfg['fps'] <= 120 and not out.exists()
for path, expected in cfg['input_sha256'].items():
    assert sha(path) == expected, path
assert all(cfg[key] in cfg['input_sha256'] for key in ['held_points', 'held_report', 'source_components'])
with np.load(cfg['held_points'], allow_pickle=False) as archive:
    points, rest, ids, pins, frames = [archive[key] for key in ['points', 'rest_points', 'source_vertex_ids', 'pinned_mask', 'frames']]
assert points.shape == (2, len(ids), 3) and rest.shape == (len(ids), 3)
assert points.dtype == rest.dtype == np.float32 and pins.dtype == np.bool_
assert frames[1] - frames[0] == 1 and np.isfinite(points).all()
held_report = json.loads(Path(cfg['held_report']).read_text())
velocity = (points[1].astype(np.float64) - points[0]) * cfg['fps']
speed = np.linalg.norm(velocity, axis=1)
assert abs(float(speed[~pins].max()) - held_report['peak_speed_m_s']) <= 1e-5
with np.load(cfg['source_components'], allow_pickle=False) as archive:
    source_components = archive['component_per_vertex']
lookup = {int(source): index for index, source in enumerate(ids)}
assert len(lookup) == len(ids)
rows, visited = [], set()
for island in held_report['geometric_components']:
    selected = np.array([lookup[source] for source in island['source_vertex_ids']])
    assert not visited.intersection(selected.tolist())
    visited.update(selected.tolist())
    before, after = points[0, selected].astype(np.float64), points[1, selected].astype(np.float64)
    fitted, rotation, singular = rigid_fit(before, after)
    residual_speed = np.linalg.norm(after - fitted, axis=1) * cfg['fps']
    fitted_rest, _, _ = rigid_fit(rest[selected].astype(np.float64), after)
    rest_error = np.linalg.norm(after - fitted_rest, axis=1)
    squared_motion = float(np.sum((after - before) ** 2))
    squared_residual = float(np.sum((after - fitted) ** 2))
    rows.append(dict(geometric_island=island['id'], source_component_indices=sorted(set(source_components[ids[selected]].tolist())),
        source_vertex_ids=island['source_vertex_ids'], point_count=len(selected), pins=int(pins[selected].sum()),
        sewing_links=island['sewing_links'], maximum_speed_m_s=float(speed[selected].max()),
        peak_shape_residual_speed_m_s=float(residual_speed.max()),
        rms_shape_residual_speed_m_s=float(np.sqrt(np.mean(residual_speed ** 2))),
        rigid_explained_squared_motion_fraction=float(np.clip(1 - squared_residual / squared_motion, 0, 1)) if squared_motion else 1.,
        best_fit_rotation_speed_rad_s=float(np.arccos(np.clip((np.trace(rotation) - 1) / 2, -1, 1)) * cfg['fps']),
        centered_fit_singular_values_m2=singular.tolist(),
        held_to_rest_best_rigid_rms_m=float(np.sqrt(np.mean(rest_error ** 2))),
        held_to_rest_best_rigid_peak_m=float(rest_error.max())))
assert len(visited) == len(ids)
out.mkdir(parents=True)
(out / 'profile.json').write_bytes(profile.read_bytes())
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
report = dict(schema_version=1, status='held-island-rigid-shape-decomposition-only', cpu_only=True,
    input_sha256=cfg['input_sha256'], profile_sha256=sha(profile), tool_sha256=sha(__file__),
    frames=frames.tolist(), pinned_peak_speed_m_s=float(speed[pins].max()), islands=rows,
    limitations=['Two held frames only; not a modal-frequency, damping or full convergence identification.',
        'Best-fit rigid motion is descriptive; removing it is not a physically justified repair.',
        'Zero root velocity and pin support do not alone establish intended flexible behavior or material response.'])
(out / 'island-motion.json').write_text(json.dumps(report, indent=2))
for path, expected in cfg['input_sha256'].items():
    assert sha(path) == expected, path
print(json.dumps({'islands': len(rows), 'pinned_peak_speed_m_s': report['pinned_peak_speed_m_s']}))
