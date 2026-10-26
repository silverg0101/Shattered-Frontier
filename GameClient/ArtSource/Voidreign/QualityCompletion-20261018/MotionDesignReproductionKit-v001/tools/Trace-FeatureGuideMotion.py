"""Replay a frozen CPU guide solver with observation hooks; no repair or rebake.

The instrumented solver must reproduce every saved output-frame point exactly.
Only observations are inserted; original physics/settings stay unchanged.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import types
import numpy as np


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ['simulation', 'profile', 'output']:
        parser.add_argument('--'+key, required=True)
    args = parser.parse_args()
    folder, profile_path, out = map(Path, [args.simulation, args.profile, args.output])
    profile = json.loads(profile_path.read_text(encoding='utf-8'))
    report_path, code_path = folder/'simulation-report.json', folder/'tool-source.py'
    guide_path, config_path, trajectory_path = folder/'guides.json', folder/'motion-profile.json', folder/'trajectory.npz'
    report = json.loads(report_path.read_text(encoding='utf-8'))
    assert sha(code_path) == report['tool_sha256'] == profile['solver_sha256']
    assert sha(guide_path) == report['guides_sha256']
    assert sha(config_path) == report['config_sha256']
    assert profile['schema_version'] == 1
    start, stop, cap = profile['start_seconds'], profile['stop_seconds'], profile['maximum_records']
    assert type(start) is float and type(stop) is float and 0 <= start < stop <= 11.
    assert type(cap) is int and 1000 <= cap <= 250000
    guides = json.loads(guide_path.read_text(encoding='utf-8'))
    raw = json.loads(config_path.read_text(encoding='utf-8'))
    names = [g['name'] for g in guides]
    selected = profile['guides']
    assert 1 <= len(selected) <= 24 and len(set(selected)) == len(selected) and set(selected) <= set(names)
    indices = [names.index(name) for name in selected]
    code = code_path.read_text(encoding='utf-8')
    # Exact text adapters deliberately fail against any other solver revision.
    replacements = [
        ('        nonlocal points, old\n', "        nonlocal points, old\n        trace('start', t, -1, points)\n"),
        ('        for iteration in range(cfg.constraint_iterations):\n', "        trace('prediction', t, -1, points)\n        for iteration in range(cfg.constraint_iterations):\n"),
        ('            for label, start, end, radius in world_colliders(t, cfg, raw):\n', "            trace('structural', t, iteration, points)\n            for label, start, end, radius in world_colliders(t, cfg, raw):\n"),
        ('                points[hit] += (normal*penetration[:, :, None])[hit]\n', "                points[hit] += (normal*penetration[:, :, None])[hit]\n                trace('capsule:'+label, t, iteration, points)\n"),
        ('            points[fixed] = target[fixed]\n', "            trace('ground', t, iteration, points)\n            points[fixed] = target[fixed]\n            trace('pin', t, iteration, points)\n"),
        ('        assert np.isfinite(points).all()\n', "        trace('end', t, -1, points)\n        assert np.isfinite(points).all()\n")]
    for anchor, replacement in replacements:
        assert code.count(anchor) == 1, 'Frozen source adapter mismatch: '+anchor
        code = code.replace(anchor, replacement)
    records, times, iterations, event_ids = [], [], [], []
    event_names, stats, peaks = [], {}, []
    previous = None
    counts = np.array([len(guides[index]['points']) for index in indices])

    def trace(event, t, iteration, points):
        nonlocal previous
        if not start <= t <= stop:
            return
        current = points[indices].copy()
        if previous is None:
            delta = np.zeros(current.shape[:2])
        else:
            delta = np.linalg.norm(current-previous, axis=-1)
        previous = current
        delta[np.arange(current.shape[1])[None] >= counts[:, None]] = 0
        peak = float(delta.max())
        if event not in event_names:
            event_names.append(event)
        row = stats.setdefault(event, dict(observations=0, maximum_correction_m=0., sum_node_displacement_m=0.))
        row['observations'] += 1
        row['maximum_correction_m'] = max(row['maximum_correction_m'], peak)
        row['sum_node_displacement_m'] += float(delta.sum())
        if peak > 1e-9 or event in ['start', 'end']:
            assert len(records) < cap, 'Trace cap reached; use a smaller pinned time window'
            records.append(current.astype(np.float32))
            times.append(t)
            iterations.append(iteration)
            event_ids.append(event_names.index(event))
        if peak > .005:
            guide, node = map(int, np.unravel_index(np.argmax(delta), delta.shape))
            peaks.append(dict(time_seconds=t, event=event, iteration=iteration,
                correction_m=peak, guide=selected[guide], node=node))

    module = types.ModuleType('frozen_feature_solver_trace')
    module.__file__ = str(code_path.resolve())
    module.__dict__['trace'] = trace
    sys.modules[module.__name__] = module
    exec(compile(code, str(code_path), 'exec'), module.__dict__)
    cfg = module.load_settings(raw)
    history, transforms, rest, full_counts, fresh = module.simulate(guides, cfg, raw)
    with np.load(trajectory_path, allow_pickle=False) as data:
        equal = {key: bool(np.array_equal(value, data[key])) for key, value in
            [('points', history), ('root_transforms', transforms), ('rest', rest), ('counts', full_counts)]}
    assert all(equal.values()), 'Observation replay does not reproduce the inspected trajectory'
    out.mkdir(parents=True, exist_ok=False)
    (out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
    (out/'profile.json').write_bytes(profile_path.read_bytes())
    (out/'instrumented-source.py').write_text(code, encoding='utf-8')
    np.savez_compressed(out/'substep-trace.npz', points=np.array(records),
        times_seconds=np.array(times), iterations=np.array(iterations, dtype=np.int16),
        event_ids=np.array(event_ids, dtype=np.int16), counts=counts)
    result = dict(schema_version=1, status='exact-replay-diagnosis-only', cpu_only=True,
        input_sha256={str(path): sha(path) for path in [report_path, code_path, guide_path, config_path, trajectory_path, profile_path]},
        tool_sha256=sha(Path(__file__)), exact_replay=equal, guides=selected,
        start_seconds=start, stop_seconds=stop, records=len(records), event_names=event_names,
        event_statistics=stats, largest_events=sorted(peaks, key=lambda row: row['correction_m'], reverse=True)[:100],
        limitations=['No physics or settings change; not a new repair attempt.',
            'Stage deltas are observational displacement contributions, not independent forces, energy or a unique causal attribution.',
            'Structural combines length and bend constraints; prediction combines inertia, drag, spring and external acceleration.',
            'Current trajectory is exactly reproduced from frozen code/config/guides; missing original producer attestation remains historical.',
            'Selected guide/time window only; no surface, contact correctness, gait or runtime acceptance.'])
    (out/'trace-report.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({k: result[k] for k in ['status', 'exact_replay', 'records', 'event_statistics']}))


if __name__ == '__main__':
    main()
