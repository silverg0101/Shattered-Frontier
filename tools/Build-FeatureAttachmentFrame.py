"""Experimental rigid micro-fragment follow using an authored parent triangle.

Preserves all source geometry, weights and unselected cached points. This is an
attachment representation study, not semantic approval or a new physics bake.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def frames(points, triangle, minimum_edge, minimum_sine):
    anchors = points[:, triangle]
    x, other = anchors[:, 1] - anchors[:, 0], anchors[:, 2] - anchors[:, 0]
    length, other_length = np.linalg.norm(x, axis=1), np.linalg.norm(other, axis=1)
    assert np.isfinite(anchors).all() and min(length.min(), other_length.min()) >= minimum_edge
    cross = np.cross(x, other)
    normal_length = np.linalg.norm(cross, axis=1)
    sine = normal_length / (length * other_length)
    assert sine.min() >= minimum_sine, 'Authored parent triangle became ill-conditioned'
    x, z = x / length[:, None], cross / normal_length[:, None]
    y = np.cross(z, x)
    basis = np.stack([x, y, z], axis=2)
    assert np.max(np.abs(basis.transpose(0, 2, 1) @ basis - np.eye(3))) < 1e-6
    assert np.max(np.abs(np.linalg.det(basis) - 1)) < 1e-6
    return anchors[:, 0], basis, float(sine.min())


p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile', required=True)
p.add_argument('--output', required=True)
a = p.parse_args()
profile, out = Path(a.profile).resolve(), Path(a.output).resolve()
cfg = json.loads(profile.read_text(encoding='utf-8'))
assert cfg['schema_version'] == 1 and cfg['scope'] == 'unapproved-parent-frame-attachment-study'
assert cfg['allow_proposed_attachment'] is True and not out.exists()
assert len(cfg['parent_triangle_source_ids']) == len(set(cfg['parent_triangle_source_ids'])) == 3
assert 1 <= len(cfg['child_source_ids']) <= 10000 and len(set(cfg['child_source_ids'])) == len(cfg['child_source_ids'])
assert not set(cfg['child_source_ids']) & set(cfg['parent_triangle_source_ids'])
assert 1e-5 <= cfg['minimum_parent_edge_m'] <= .1 and .01 <= cfg['minimum_parent_sine'] <= 1
assert type(cfg['fps']) is int and 1 <= cfg['fps'] <= 120
for path, expected in cfg['input_sha256'].items():
    assert sha(path) == expected, path
assert all(cfg[key] in cfg['input_sha256'] for key in ['output_points', 'held_points', 'held_report'])
with np.load(cfg['output_points'], allow_pickle=False) as archive:
    points, ids, output_frames = archive['points'], archive['source_vertex_ids'], archive['frames']
with np.load(cfg['held_points'], allow_pickle=False) as archive:
    held, held_ids, rest, pins = archive['points'], archive['source_vertex_ids'], archive['rest_points'], archive['pinned_mask']
assert np.array_equal(ids, held_ids) and len(set(ids.tolist())) == len(ids)
assert points.dtype == held.dtype == rest.dtype == np.float32
assert ids.ndim == 1 and np.issubdtype(ids.dtype, np.integer)
assert pins.dtype == np.bool_ and pins.shape == ids.shape
assert points.ndim == 3 and 2 <= len(points) <= 10000 and points.shape[2] == 3
assert np.array_equal(output_frames, np.arange(1, len(points) + 1))
assert points.ndim == 3 and points.shape[1:] == rest.shape == held.shape[1:]
assert held.shape[0] == 2 and np.isfinite(points).all() and np.isfinite(held).all()
lookup = {int(source_id): index for index, source_id in enumerate(ids)}
triangle = np.array([lookup[source_id] for source_id in cfg['parent_triangle_source_ids']])
child = np.array([lookup[source_id] for source_id in cfg['child_source_ids']])
assert not pins[child].any(), 'Do not override an existing pinned attachment'
origin, rest_basis, _ = frames(rest[None], triangle, cfg['minimum_parent_edge_m'], cfg['minimum_parent_sine'])
offsets = (rest[child] - origin[0]) @ rest_basis[0]


def follow(series):
    origin, basis, minimum_sine = frames(series, triangle, cfg['minimum_parent_edge_m'], cfg['minimum_parent_sine'])
    revised = series.copy()
    revised[:, child] = np.einsum('ci,fji->fcj', offsets, basis) + origin[:, None]
    return revised, minimum_sine


revised, minimum_output_sine = follow(points)
revised_held, minimum_held_sine = follow(held)
unchanged = np.ones(len(ids), bool)
unchanged[child] = False
assert np.array_equal(revised[:, unchanged], points[:, unchanged])
assert np.array_equal(revised_held[:, unchanged], held[:, unchanged])
root = lookup[cfg['child_root_source_id']]
assert root in child
rest_gap = float(np.linalg.norm(rest[root] - rest[triangle[0]]))
gap_error = float(np.abs(np.linalg.norm(revised[:, root] - revised[:, triangle[0]], axis=1) - rest_gap).max())
assert gap_error <= 1e-6
before_speed = np.linalg.norm(held[1] - held[0], axis=1) * cfg['fps']
after_speed = np.linalg.norm(revised_held[1] - revised_held[0], axis=1) * cfg['fps']
old = json.loads(Path(cfg['held_report']).read_text())
assert abs(float(before_speed[~pins].max()) - old['peak_speed_m_s']) <= 1e-5
assert abs(float(np.sqrt(np.mean(before_speed[~pins] ** 2))) - old['rms_speed_m_s']) <= 1e-5
out.mkdir(parents=True)
(out / 'profile.json').write_bytes(profile.read_bytes())
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
np.savez_compressed(out / 'attachment-frame-points.npz', points=revised, source_vertex_ids=ids, frames=output_frames)
report = dict(schema_version=1, status='attachment-frame-proposal-needs-review', cpu_only=True,
    binding_id=cfg['binding_id'], input_sha256=cfg['input_sha256'], tool_sha256=sha(__file__),
    profile_sha256=sha(profile),
    points_sha256=sha(out / 'attachment-frame-points.npz'), child_points=len(child),
    unchanged_point_count=int(unchanged.sum()), unselected_points_bit_exact=True,
    parent_triangle_source_ids=cfg['parent_triangle_source_ids'],
    minimum_parent_sine=min(minimum_output_sine, minimum_held_sine),
    original_rest_gap_m=rest_gap, maximum_gap_error_m=gap_error,
    before_child_peak_m_s=float(before_speed[child].max()), after_child_peak_m_s=float(after_speed[child].max()),
    before_global_free_peak_m_s=float(before_speed[~pins].max()), after_global_free_peak_m_s=float(after_speed[~pins].max()),
    after_global_free_rms_m_s=float(np.sqrt(np.mean(after_speed[~pins] ** 2))),
    limitations=['Authored rigid micro-fragment behavior is proposed, not approved semantics or natural flexible hair.',
        'Two native held frames before contact correction; no claim of full held convergence or new physics.',
        'All full-duration contact/motion/retention/style/export checks must be repeated on a new cache candidate.',
        'No source geometry, weights, root/free masks or acceptance limits changed.'])
(out / 'attachment-frame-report.json').write_text(json.dumps(report, indent=2))
for path, expected in cfg['input_sha256'].items():
    assert sha(path) == expected, path
print(json.dumps({key: report[key] for key in ['before_child_peak_m_s', 'after_child_peak_m_s', 'after_global_free_peak_m_s']}))
