"""CPU saved-frame guide envelope/style diagnostics, separate from skin checks."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import numpy as np


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ['simulation', 'surface-profile', 'output']:
        parser.add_argument('--'+key, required=True)
    args = parser.parse_args()
    folder, surface_path, out = map(Path, [args.simulation, args.surface_profile, args.output])
    report_path, code_path = folder/'simulation-report.json', folder/'tool-source.py'
    guide_path, config_path, trajectory_path = folder/'guides.json', folder/'motion-profile.json', folder/'trajectory.npz'
    report = json.loads(report_path.read_text(encoding='utf-8'))
    assert sha(code_path) == report['tool_sha256']
    assert sha(guide_path) == report['guides_sha256'] and sha(config_path) == report['config_sha256']
    guides = json.loads(guide_path.read_text(encoding='utf-8'))
    raw = json.loads(config_path.read_text(encoding='utf-8'))
    limits = json.loads(surface_path.read_text(encoding='utf-8'))['limits']
    assert report['settings'] == raw['settings']
    spec = importlib.util.spec_from_file_location('saved_guide_clearance_solver', code_path)
    physics = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = physics
    spec.loader.exec_module(physics)
    cfg = physics.load_settings(raw)
    with np.load(trajectory_path, allow_pickle=False) as data:
        points, roots, rest, counts = data['points'].astype(float), data['root_transforms'], data['rest'], data['counts']
    assert points.shape[0] == cfg.frame_count and len(guides) == points.shape[1]
    assert np.isfinite(points).all() and np.isfinite(rest).all()
    assert np.array_equal(counts, [len(g['points']) for g in guides])
    valid = np.arange(points.shape[2])[None] < counts[:, None]
    pins = np.array([raw.get('pinned_nodes_by_feature', {}).get(g['feature'], cfg.pinned_nodes) for g in guides])
    fixed = valid & (np.arange(points.shape[2])[None] < pins[:, None])
    free = valid & ~fixed
    rows, phases = {}, []

    def sample(label, penetration, frame):
        row = rows.setdefault(label, dict(maximum_free_penetration_m=0., free_warnings=0,
            maximum_fixed_penetration_m=0., fixed_penetrating_samples=0))
        allowed = limits['maximum_penetration_m']
        free_values = np.where(free, np.maximum(0., penetration), -1.)
        fixed_values = np.where(fixed, np.maximum(0., penetration), -1.)
        peak = float(free_values.max())
        row['free_warnings'] += int(np.count_nonzero(free_values > allowed))
        if peak > row['maximum_free_penetration_m']:
            guide, node = map(int, np.unravel_index(np.argmax(free_values), free_values.shape))
            row.update(maximum_free_penetration_m=peak, peak_frame=frame+1, guide=guides[guide]['name'], node=node)
        fixed_peak = float(fixed_values.max())
        if fixed_peak > row['maximum_fixed_penetration_m']:
            guide, node = map(int, np.unravel_index(np.argmax(fixed_values), fixed_values.shape))
            row.update(maximum_fixed_penetration_m=fixed_peak, fixed_peak_frame=frame+1,
                       fixed_guide=guides[guide]['name'], fixed_node=node)
        row['fixed_penetrating_samples'] += int(np.count_nonzero(fixed_values > 0.))

    for frame, position in enumerate(points):
        t = frame/cfg.fps
        translation, rotation, phase = physics.transform(t, cfg)
        assert np.max(np.abs(roots[frame, :3]-translation)) < 1e-10
        offset = np.linalg.norm(position-(rest@rotation.T+translation), axis=-1)
        phases.append(dict(frame=frame+1, phase=phase,
            rms_style_deviation_m=float(np.sqrt(np.mean(offset[free]**2))), maximum_style_deviation_m=float(offset[free].max())))
        for label, start, end, radius in physics.world_colliders(t, cfg, raw):
            vector = end-start
            parameter = np.clip(np.sum((position-start)*vector, axis=-1)/(vector@vector), 0., 1.)
            closest = start+parameter[..., None]*vector
            envelope = raw.get('surface_envelopes_m', {}).get(label, cfg.collision_radius_m)
            sample(label, radius+envelope-np.linalg.norm(position-closest, axis=-1), frame)
        envelope = raw.get('surface_envelopes_m', {}).get('Ground', cfg.collision_radius_m)
        sample('Ground', cfg.ground_height_m+envelope-position[..., 2], frame)
    maximum = max(row['maximum_free_penetration_m'] for row in rows.values())
    result = dict(schema_version=1, status='guide-envelope-warnings' if maximum > limits['maximum_penetration_m'] else 'passed-saved-guide-envelope-only',
        cpu_only=True, maximum_free_penetration_m=maximum, maximum_penetration_warning_m=limits['maximum_penetration_m'],
        colliders=rows, per_frame_style=phases,
        phase_style={phase:dict(rms_style_deviation_m=float(np.sqrt(np.mean([r['rms_style_deviation_m']**2 for r in phases if r['phase']==phase]))),
            maximum_style_deviation_m=max(r['maximum_style_deviation_m'] for r in phases if r['phase']==phase)) for phase in dict.fromkeys(row['phase'] for row in phases)},
        input_sha256={str(p):sha(p) for p in [report_path, code_path, guide_path, config_path, trajectory_path, surface_path]},
        tool_sha256=sha(Path(__file__)),
        limitations=['Saved output-frame points/envelopes only; not swept contacts, strand segments or rendered skin clearance.',
            'Fixed-node penetration is reported separately and needs root/envelope feasibility review; not silently repaired or accepted.',
            'Style deviation is diagnostic, not a target to minimize blindly; recovery and intended motion require review.',
            'Uses the existing surface penetration ceiling numerically as a guide warning; cannot accept or reject surfaces from guide residual alone.'])
    out.mkdir(parents=True, exist_ok=False)
    (out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
    (out/'guide-clearance.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({key:result[key] for key in ['status', 'maximum_free_penetration_m', 'phase_style']}))


if __name__ == '__main__':
    main()
