"""Deterministic CPU secondary-motion bake for authored guide proposals.

Verlet/PBD styled guides, pinned root bands, capsule/ground contacts and settled
pre-roll. The body motion is a diagnostic rigid harness, never a gait skeleton.
"""
import argparse
from dataclasses import dataclass
import hashlib
import json
import math
from pathlib import Path
import numpy as np


@dataclass(frozen=True)
class Settings:
    fps: int = 30
    frame_count: int = 330
    substeps: int = 6
    constraint_iterations: int = 10
    pinned_nodes: int = 2
    warmup_min_seconds: float = 3.0
    warmup_max_seconds: float = 8.0
    settle_rms_speed_m_s: float = .02
    settle_peak_speed_m_s: float = .08
    gravity_m_s2: float = 9.81
    drag_per_second: float = 5.0
    style_spring_per_second_squared: float = 85.0
    bend_stiffness: float = .6
    collision_radius_m: float = .02
    ground_height_m: float = 0.0
    wind_acceleration_m_s2: float = .4
    walk_distance_m: float = .44
    run_distance_m: float = 1.30
    turn_forward_distance_m: float = .35
    turn_side_distance_m: float = .70
    turn_angle_degrees: float = -60.0
    walk_bob_m: float = .02
    run_bob_m: float = .04
    hand_sweep_distance_m: float = .48
    maximum_segment_relative_error: float = .08
    maximum_initial_step_m: float = .02


BOUNDS = dict(fps=(24, 60), frame_count=(330, 330), substeps=(4, 16),
    constraint_iterations=(6, 24), pinned_nodes=(1, 2), warmup_min_seconds=(1., 8.),
    warmup_max_seconds=(3., 15.), settle_rms_speed_m_s=(.001, .05),
    settle_peak_speed_m_s=(.01, .15), gravity_m_s2=(0., 20.), drag_per_second=(0., 15.),
    style_spring_per_second_squared=(0., 150.), bend_stiffness=(0., 1.),
    collision_radius_m=(.005, .06), ground_height_m=(-.1, .1), wind_acceleration_m_s2=(0., 5.),
    walk_distance_m=(.1, 1.), run_distance_m=(.2, 3.), turn_forward_distance_m=(0., 1.),
    turn_side_distance_m=(0., 2.), turn_angle_degrees=(-120., 120.),
    walk_bob_m=(0., .05), run_bob_m=(0., .08), hand_sweep_distance_m=(0., .6),
    maximum_segment_relative_error=(.01, .1), maximum_initial_step_m=(.001, .03))


def load_settings(raw):
    cfg = Settings(**raw['settings'])
    defaults = Settings()
    for key, (lo, hi) in BOUNDS.items():
        value = getattr(cfg, key)
        assert type(value) is type(getattr(defaults, key)) and lo <= value <= hi, (key, value)
    assert cfg.warmup_min_seconds <= cfg.warmup_max_seconds
    assert cfg.fps == 30, 'This phase fixture is versioned for30fps/330frames'
    for collider in raw['colliders']:
        assert collider['space'] in ['body', 'world', 'hand']
        assert .01 <= collider['radius_m'] <= 1.
        assert np.isfinite(np.array([collider['start_m'], collider['end_m']])).all()
        assert np.linalg.norm(np.array(collider['end_m'])-collider['start_m']) > 1e-5
    assert all(type(n) is int and 1 <= n <= 4 for n in raw.get('pinned_nodes_by_feature', {}).values())
    assert set(raw.get('required_contact_labels', [])) <= {c['label'] for c in raw['colliders']} | {'Ground'}
    envelopes = raw.get('surface_envelopes_m', {})
    assert set(envelopes) <= {c['label'] for c in raw['colliders']} | {'Ground'}
    assert all(type(value) is float and 0. <= value <= .15 for value in envelopes.values())
    return cfg


def ease(x):
    x = min(1., max(0., x))
    return x*x*x*(10+x*(-15+6*x))


def motion(t, cfg):
    """Continuous-position/velocity phase fixture; distances are author config."""
    if t <= 1:
        return np.zeros(3), 0., 'SETTLED'
    if t <= 3:
        d = t-1
        bob = cfg.walk_bob_m*math.sin(math.tau*1.4*d)*math.sin(math.pi*d/2)**2
        return np.array([0., -cfg.walk_distance_m*ease(d/2), bob]), 0., 'WALK RESPONSE'
    if t <= 5:
        d = t-3
        bob = cfg.run_bob_m*math.sin(math.tau*2.4*d)*math.sin(math.pi*d/2)**2
        return np.array([0., -cfg.walk_distance_m-cfg.run_distance_m*ease(d/2), bob]), 0., 'RUN RESPONSE'
    q = ease((t-5)/2)
    pos = np.array([cfg.turn_side_distance_m*q,
                   -cfg.walk_distance_m-cfg.run_distance_m-cfg.turn_forward_distance_m*q, 0.])
    angle = math.radians(cfg.turn_angle_degrees)*q
    return pos, angle, 'TURN RESPONSE' if t <= 7 else ('STOP / SETTLE' if t <= 9 else 'RIDER CONTACT')


def transform(t, cfg):
    pos, angle, label = motion(t, cfg)
    c, s = math.cos(angle), math.sin(angle)
    return pos, np.array([[c, -s, 0], [s, c, 0], [0, 0, 1.]]), label


def hand_offset(t, cfg):
    return -cfg.hand_sweep_distance_m*math.sin(math.pi*min(1., max(0., (t-9)/2)))


def world_colliders(t, cfg, raw):
    pos, rot, _ = transform(t, cfg)
    rows = []
    for collider in raw['colliders']:
        start, end = np.array(collider['start_m']), np.array(collider['end_m'])
        if collider['space'] == 'hand':
            start[0] += hand_offset(t, cfg)
            end[0] += hand_offset(t, cfg)
        if collider['space'] != 'world':
            start, end = rot@start+pos, rot@end+pos
        rows.append((collider['label'], start, end, collider['radius_m']))
    return rows


def simulate(guides, cfg, raw):
    count = len(guides)
    counts = np.array([len(g['points']) for g in guides])
    assert count and counts.min() >= 3
    nodes = int(counts.max())
    rest = np.zeros((count, nodes, 3), dtype=np.float64)
    for index, guide in enumerate(guides):
        rest[index, :counts[index]] = guide['points']
        rest[index, counts[index]:] = guide['points'][-1]
    valid = np.arange(nodes)[None] < counts[:, None]
    pinned_counts = np.array([raw.get('pinned_nodes_by_feature', {}).get(g['feature'], cfg.pinned_nodes) for g in guides])
    fixed = (np.arange(nodes)[None] < pinned_counts[:, None]) | ~valid
    free = valid & ~fixed
    lengths = np.linalg.norm(np.diff(rest, axis=1), axis=2)
    chords = np.linalg.norm(rest[:, 2:]-rest[:, :-2], axis=2)
    points, old = rest.copy(), rest.copy()
    dt = 1/(cfg.fps*cfg.substeps)
    contacts = {}

    def advance(t):
        nonlocal points, old
        substep_start = points.copy()
        contact_sum = np.zeros_like(points)
        pos, rot, _ = transform(t, cfg)
        target = rest @ rot.T + pos
        speed = (points-old)*math.exp(-cfg.drag_per_second*dt)
        old[:] = points
        spring = cfg.style_spring_per_second_squared*(target-points)
        wind = 0. if t <= 1 else cfg.wind_acceleration_m_s2*math.sin(t*2)*ease((t-1)/2)
        points[free] += (speed+(spring+np.array([wind, 0., -cfg.gravity_m_s2]))*dt*dt)[free]
        points[fixed] = target[fixed]
        prediction = points.copy()
        for iteration in range(cfg.constraint_iterations):
            for step, desired, stiffness in [(1, lengths, 1.), (2, chords, cfg.bend_stiffness)]:
                for index in range(nodes-step):
                    delta = points[:, index+step]-points[:, index]
                    distance = np.linalg.norm(delta, axis=1)
                    w0, w1 = free[:, index].astype(float), free[:, index+step].astype(float)
                    active = valid[:, index+step]
                    factor = stiffness*(1-desired[:, index]/np.maximum(distance, 1e-12))
                    correction = delta*(factor*active/np.maximum(w0+w1, 1))[:, None]
                    points[:, index] += w0[:, None]*correction
                    points[:, index+step] -= w1[:, None]*correction
            for label, start, end, radius in world_colliders(t, cfg, raw):
                vector = end-start
                parameter = np.clip(np.sum((points-start)*vector, axis=2)/(vector@vector), 0, 1)
                closest = start+parameter[:, :, None]*vector
                delta = points-closest
                distance = np.linalg.norm(delta, axis=2)
                envelope = raw.get('surface_envelopes_m', {}).get(label, cfg.collision_radius_m)
                penetration = radius+envelope-distance
                hit = (penetration > 0) & free
                normal = delta/np.maximum(distance[:, :, None], 1e-12)
                normal[distance < 1e-12] = [1, 0, 0]
                points[hit] += (normal*penetration[:, :, None])[hit]
                contact_sum[hit] += (normal*penetration[:, :, None])[hit]
                if iteration == 0:
                    contacts[label] = contacts.get(label, 0)+int(hit.sum())
            ground_envelope = raw.get('surface_envelopes_m', {}).get('Ground', cfg.collision_radius_m)
            hit = (points[:, :, 2] < cfg.ground_height_m+ground_envelope) & free
            contact_sum[:, :, 2][hit] += cfg.ground_height_m+ground_envelope-points[:, :, 2][hit]
            points[:, :, 2][hit] = cfg.ground_height_m+ground_envelope
            if iteration == 0:
                contacts['Ground'] = contacts.get('Ground', 0)+int(hit.sum())
            points[fixed] = target[fixed]
        assert np.isfinite(points).all()
        split_velocity(points, old, substep_start, prediction, contact_sum, t, dt, free, valid, cfg, raw)

    settled = False
    stable = 0
    min_warmup = math.ceil(cfg.warmup_min_seconds*cfg.fps)
    max_warmup = math.ceil(cfg.warmup_max_seconds*cfg.fps)
    for frame in range(max_warmup):
        for _ in range(cfg.substeps):
            advance(0.)
        speed = np.linalg.norm((points-old)/dt, axis=2)[free]
        rms, peak = float(np.sqrt(np.mean(speed**2))), float(speed.max())
        stable = stable+1 if rms < cfg.settle_rms_speed_m_s and peak < cfg.settle_peak_speed_m_s else 0
        if frame+1 >= min_warmup and stable >= cfg.fps//2:
            settled = True
            break
    warmup_frames = frame+1
    history, transforms, metrics = [], [], []
    for frame in range(cfg.frame_count):
        if frame:
            for substep in range(cfg.substeps):
                advance(((frame-1)+(substep+1)/cfg.substeps)/cfg.fps)
        t = frame/cfg.fps
        pos, rot, label = transform(t, cfg)
        lengths_now = np.linalg.norm(np.diff(points, axis=1), axis=2)
        relative = np.abs(lengths_now/np.maximum(lengths, 1e-12)-1)
        mask = valid[:, 1:]
        root_error = np.linalg.norm(points-rest@rot.T-pos, axis=2)[fixed & valid].max()
        metrics.append(dict(frame=frame+1, phase=label, root_error_m=float(root_error),
                             max_segment_relative_error=float(relative[mask].max())))
        history.append(points.astype(np.float32).copy())
        transforms.append(np.r_[pos, math.atan2(rot[1, 0], rot[0, 0])])
    history = np.array(history)
    initial_step = float(np.linalg.norm(history[1]-history[0], axis=2)[valid].max())
    report = dict(cpu_only=True, warmup_frames=warmup_frames, warmup_settled=settled,
        warmup_rms_speed_m_s=rms, warmup_peak_speed_m_s=peak, contacts=contacts,
        max_initial_step_m=initial_step,
        max_root_error_m=max(row['root_error_m'] for row in metrics),
        max_segment_relative_error=max(row['max_segment_relative_error'] for row in metrics), metrics=metrics)
    report['missing_required_contacts'] = [label for label in raw.get('required_contact_labels', []) if not contacts.get(label)]
    report['status'] = ('passed-particle-checks-only' if settled
        and initial_step <= cfg.maximum_initial_step_m
        and report['max_segment_relative_error'] <= cfg.maximum_segment_relative_error
        and report['max_root_error_m'] < 1e-6 and not report['missing_required_contacts'] else 'needs-review')
    report['limitations'] = ['Rigid body harness; legs do not walk.', 'Capsule/ground point contacts, not rendered-surface acceptance.',
        'No strand self-collision or arbitrary environment mesh collision.', 'Offline bake, not engine runtime simulation.']
    return history, np.array(transforms), rest, counts, report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ['guides', 'config', 'output']:
        p.add_argument('--'+key, required=True)
    a = p.parse_args()
    guide_path, config_path, out = map(Path, [a.guides, a.config, a.output])
    guides, raw = json.loads(guide_path.read_text()), json.loads(config_path.read_text())
    cfg = load_settings(raw)
    out.mkdir(parents=True, exist_ok=False)
    (out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
    (out/'motion-profile.json').write_bytes(config_path.read_bytes())
    (out/'guides.json').write_bytes(guide_path.read_bytes())
    history, transforms, rest, counts, report = simulate(guides, cfg, raw)
    np.savez_compressed(out/'trajectory.npz', points=history, root_transforms=transforms,
                        rest=rest, counts=counts)
    report.update(config_path=str(config_path), config_sha256=hashlib.sha256(config_path.read_bytes()).hexdigest(),
                  guides_path=str(guide_path), guides_sha256=hashlib.sha256(guide_path.read_bytes()).hexdigest(),
                  tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), settings=raw['settings'])
    (out/'simulation-report.json').write_text(json.dumps(report, indent=2))
    print(json.dumps({k: v for k, v in report.items() if k not in ['metrics', 'settings']}), flush=True)


if __name__ == '__main__':
    main()
