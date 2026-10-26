"""Read-only CPU full-duration guide-motion check; never runs a new simulation.

World-space step uses the existing surface-step ceiling. Speed, acceleration
and body-relative motion are diagnostic measurements, not invented tolerances.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--simulation', required=True)
    parser.add_argument('--profile', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    folder, profile_path, out = map(Path, [args.simulation, args.profile, args.output])
    profile = json.loads(profile_path.read_text(encoding='utf-8'))
    assert profile['schema_version'] == 1
    ceiling = profile['maximum_world_step_m']
    assert type(ceiling) is float and .001 <= ceiling <= .08
    surface_path = Path(profile['surface_profile'])
    assert sha(surface_path) == profile['surface_profile_sha256']
    surface = json.loads(surface_path.read_text(encoding='utf-8'))
    assert ceiling == surface['limits']['maximum_surface_step_m'], 'Keep existing ceiling'
    report_path, trajectory_path = folder/'simulation-report.json', folder/'trajectory.npz'
    guide_path, motion_path = folder/'guides.json', folder/'motion-profile.json'
    old = json.loads(report_path.read_text(encoding='utf-8'))
    assert sha(guide_path) == old['guides_sha256']
    assert sha(motion_path) == old['config_sha256']
    motion = json.loads(motion_path.read_text(encoding='utf-8'))
    assert old['settings'] == motion['settings']
    fps = old['settings']['fps']
    assert type(fps) is int and 24 <= fps <= 60
    guides = json.loads(guide_path.read_text(encoding='utf-8'))
    with np.load(trajectory_path, allow_pickle=False) as data:
        points = data['points'].astype(np.float64)
        roots = data['root_transforms'].astype(np.float64)
        rest, counts = data['rest'], data['counts']
    frames, guide_count, node_count, xyz = points.shape
    assert frames == old['settings']['frame_count'] and xyz == 3 and frames >= 3
    assert guide_count == len(guides) and counts.shape == (guide_count,)
    assert roots.shape == (frames, 4) and rest.shape == points.shape[1:]
    assert np.isfinite(points).all() and np.isfinite(roots).all() and np.isfinite(rest).all()
    assert np.issubdtype(counts.dtype, np.integer) and np.all((counts >= 3) & (counts <= node_count))
    assert np.array_equal(counts, [len(g['points']) for g in guides])
    for index, guide in enumerate(guides):
        assert np.array_equal(rest[index, :counts[index]], guide['points'])
    valid = np.arange(node_count)[None, :] < counts[:, None]
    # Row-vector inverse of the simulator's rotation around world Z.
    shifted = points-roots[:, None, None, :3]
    c, s = np.cos(roots[:, 3])[:, None, None], np.sin(roots[:, 3])[:, None, None]
    local = shifted.copy()
    local[..., 0] = c*shifted[..., 0]+s*shifted[..., 1]
    local[..., 1] = -s*shifted[..., 0]+c*shifted[..., 1]
    world_delta = np.diff(points, axis=0)
    steps = np.linalg.norm(world_delta, axis=-1)
    relative_steps = np.linalg.norm(np.diff(local, axis=0), axis=-1)
    accelerations = np.linalg.norm(np.diff(world_delta, axis=0)*fps**2, axis=-1)
    steps[:, ~valid] = -1
    relative_steps[:, ~valid] = -1
    accelerations[:, ~valid] = -1
    metrics = old['metrics']
    assert len(metrics) == frames and [m['frame'] for m in metrics] == list(range(1, frames+1))

    def location(array, offset, units):
        frame, guide, node = map(int, np.unravel_index(np.argmax(array), array.shape))
        return dict(value=float(array[frame, guide, node]), units=units,
                    frame=frame+offset, guide=guides[guide]['name'], node=node,
                    phase=metrics[frame+offset-1]['phase'])

    violations = []
    per_frame = []
    for frame, guide, node in np.argwhere(steps > ceiling):
        frame, guide, node = int(frame), int(guide), int(node)
        violations.append(dict(frame=frame+2, guide=guides[guide]['name'], node=node,
            step_m=float(steps[frame, guide, node]), speed_m_s=float(steps[frame, guide, node]*fps),
            phase=metrics[frame+1]['phase']))
    for index in range(frames-1):
        per_frame.append(dict(frame=index+2, phase=metrics[index+1]['phase'],
            maximum_world_step_m=float(steps[index].max()),
            maximum_body_relative_step_m=float(relative_steps[index].max()),
            violating_nodes=int(np.count_nonzero(steps[index] > ceiling))))
    result = dict(schema_version=1, status='needs-review' if violations else 'passed-guide-step-only',
        cpu_only=True, simulation=str(folder.resolve()), fps=fps, frame_count=frames,
        guide_count=guide_count, maximum_world_step_m=ceiling,
        peak_world_step=location(steps, 2, 'm'),
        peak_world_speed=location(steps*fps, 2, 'm/s'),
        peak_world_acceleration=location(accelerations, 3, 'm/s2'),
        peak_body_relative_step=location(relative_steps, 2, 'm'),
        violation_count=len(violations), violating_frame_count=len({v['frame'] for v in violations}),
        input_sha256={str(p): sha(p) for p in [report_path, trajectory_path, guide_path, motion_path, profile_path, surface_path]},
        tool_sha256=sha(Path(__file__)), violations=violations, per_frame=per_frame,
        limitations=['No resimulation, modification or new repair attempt.',
            'World step ceiling inherited from actual-surface profile; passing is necessary but not sufficient for surface acceptance.',
            'Velocity is an interval average; acceleration is an output-frame finite difference, not substep dynamics.',
            'Body-relative speed and acceleration are diagnostics with no acceptance threshold.',
            'No contact, self-collision, skin, semantic, gait or runtime acceptance.'])
    out.mkdir(parents=True, exist_ok=False)
    (out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
    (out/'profile.json').write_bytes(profile_path.read_bytes())
    (out/'trajectory-check.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps({key: result[key] for key in ['status', 'peak_world_step', 'peak_world_acceleration', 'violation_count', 'violating_frame_count']}))


if __name__ == '__main__':
    main()
