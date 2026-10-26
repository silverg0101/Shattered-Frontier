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
if cfg.get('held_window'):
    assert cfg['held_window'] in cfg['input_sha256'] and cfg['motion_profile'] in cfg['input_sha256']
    with np.load(cfg['held_window'], allow_pickle=False) as archive:
        window, window_ids, window_pins, window_frames = [archive[key] for key in ['points', 'source_vertex_ids', 'pinned_mask', 'frames']]
        assert int(archive['fps']) == cfg['fps']
    assert np.array_equal(window_ids, ids) and np.array_equal(window_pins, pins)
    assert np.array_equal(window_frames, np.arange(1, len(window) + 1))
    assert np.isfinite(window).all() and np.max(np.abs(window[-2:] - points)) <= 1e-5
    policy = json.loads(Path(cfg['motion_profile']).read_text())['settings']
    speeds = np.linalg.norm(np.diff(window, axis=0), axis=2) * cfg['fps']
    rms_curve = np.sqrt(np.mean(speeds[:, ~pins] ** 2, axis=1))
    peak_curve = speeds[:, ~pins].max(axis=1)
    times = (window_frames[1:] - 1) / cfg['fps']
    permitted = (times >= policy['warmup_min_seconds']) & (times <= policy['warmup_max_seconds'])
    assert permitted.any()
    passing = permitted & (rms_curve <= policy['settle_rms_speed_m_s']) & (peak_curve <= policy['settle_peak_speed_m_s'])
    curve_rows = []
    for row in rows:
        selected = np.array([lookup[source] for source in row['source_vertex_ids']])
        curve_rows.append(speeds[:, selected].max(axis=1))
    np.savez_compressed(out / 'held-speed-curves.npz', frames=window_frames[1:], times_s=times,
        global_rms_m_s=rms_curve, global_peak_m_s=peak_curve,
        geometric_islands=np.array([row['geometric_island'] for row in rows]), island_peak_m_s=np.array(curve_rows))
    report['held_window'] = dict(frame_count=len(window), time_range_s=[float(times[0]), float(times[-1])],
        permitted_passing_frames=window_frames[1:][passing].tolist(),
        minimum_peak_in_permitted_window_m_s=float(peak_curve[permitted].min()),
        maximum_peak_in_permitted_window_m_s=float(peak_curve[permitted].max()),
        pinned_peak_m_s=float(speeds[:, pins].max()), policy=policy)
    if cfg.get('plot_source_component_indices'):
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        selected_components = cfg['plot_source_component_indices']
        assert isinstance(selected_components, list) and 1 <= len(selected_components) <= 8
        assert all(type(index) is int for index in selected_components)
        fig, axes = plt.subplots(2, 1, figsize=(10, 7), sharex=True)
        axes[0].plot(times, rms_curve * 1000, label='All original free points: RMS')
        axes[0].axhline(policy['settle_rms_speed_m_s'] * 1000, color='black', linestyle='--', label='Declared RMS limit')
        axes[0].set_ylabel('RMS speed (mm/s)')
        axes[1].plot(times, peak_curve * 1000, color='black', linewidth=2, label='All original free points: peak')
        for row, curve in zip(rows, curve_rows):
            if len(row['source_component_indices']) == 1 and row['source_component_indices'][0] in selected_components:
                axes[1].plot(times, curve * 1000, label=f"C{row['source_component_indices'][0]:04}")
        axes[1].axhline(policy['settle_peak_speed_m_s'] * 1000, color='black', linestyle='--', label='Declared peak limit')
        axes[1].set_ylabel('Peak speed (mm/s)')
        axes[1].set_xlabel('Held startup time (s), original source clock')
        for axis in axes:
            axis.axvspan(policy['warmup_min_seconds'], times[-1], color='grey', alpha=.1)
            axis.grid(alpha=.2)
            axis.legend(fontsize=8, loc='upper right')
        fig.suptitle('Frozen held cache: no new physics or response tuning')
        fig.tight_layout()
        fig.savefig(out / 'held-convergence.png', dpi=140)
        plt.close(fig)
(out / 'island-motion.json').write_text(json.dumps(report, indent=2))
for path, expected in cfg['input_sha256'].items():
    assert sha(path) == expected, path
print(json.dumps({'islands': len(rows), 'pinned_peak_speed_m_s': report['pinned_peak_speed_m_s']}))
