"""Finite CPU analytic fixtures for the proposed split-contact velocity module.

This does not run or fix Voidreign. Expected values are analytic fixture
contracts, not new model acceptance tolerances.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from FeatureContactVelocity import ContactSettings, split_candidate, resolve_velocity, capsule_material_velocity
import FeatureContactVelocity as solver


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    profile_path, out = Path(args.profile), Path(args.output)
    profile = json.loads(profile_path.read_text(encoding='utf-8'))
    assert profile['schema_version'] == 1
    settings = ContactSettings(**profile['velocity_settings'])
    settings.validate()
    dt = profile['dt_seconds']
    assert type(dt) is float and 1e-6 <= dt <= .05
    assert profile['analytic_absolute_tolerance'] == 1e-9
    tolerance = profile['analytic_absolute_tolerance']
    rows = []

    def case(name, candidate, normals, wall, expected, correction=None, start=None, solved=None):
        original = np.array(candidate, dtype=float)
        if correction is not None:
            original = split_candidate(start, solved, correction, dt)
        resolved, report = resolve_velocity(original, normals, wall, settings)
        delta = float(np.max(np.abs(resolved-np.array(expected))))
        row = dict(name=name, candidate_m_s=original.tolist(), resolved_m_s=resolved.tolist(),
            expected_m_s=expected, maximum_absolute_error_m_s=delta,
            passed=bool(delta <= tolerance and report['status'] == 'passed-velocity-constraints-only'), **report)
        if correction is not None:
            row['baseline_verlet_inferred_m_s']=((np.array(solved)-start)/dt).tolist()
            row['summed_contact_correction_m']=correction
        rows.append(row)

    case('no-contact-preserves-motion', [1., 2., 3.], [], [], [1., 2., 3.])
    case('static-plane-rest', [0., 0., 0.], [[0., 0., 1.]], [[0., 0., 0.]], [0., 0., 0.])
    case('static-plane-gravity-approach', [0., 0., -1.], [[0., 0., 1.]], [[0., 0., 0.]], [0., 0., 0.])
    case('static-plane-frictionless-slide', [2., 3., -1.], [[0., 0., 1.]], [[0., 0., 0.]], [2., 3., 0.])
    case('separating-normal-kept', [2., 0., 1.], [[0., 0., 1.]], [[0., 0., 0.]], [2., 0., 1.])
    # Invalid overlap is repaired positionally; it must not silently become launch velocity.
    case('initial-penetration-no-launch', [0., 0., 0.], [[0., 0., 1.]], [[0., 0., 0.]], [0., 0., 0.],
        correction=[0., 0., .02], start=[0., 0., -.02], solved=[0., 0., 0.])
    case('capsule-flank-normal-slide', [-2., 3., 1.], [[1., 0., 0.]], [[0., 0., 0.]], [0., 3., 1.])
    case('capsule-endcap-normal-slide', [2., -3., 1.], [[0., 1., 0.]], [[0., 0., 0.]], [2., 0., 1.])
    case('moving-plane-relative-normal', [2., 0., -1.], [[0., 0., 1.]], [[0., 0., 3.]], [2., 0., 3.])
    case('two-static-contact-corner', [-2., 3., -1.], [[1., 0., 0.], [0., 0., 1.]], [[0., 0., 0.], [0., 0., 0.]], [0., 3., 0.])
    # Contradictory moving walls require v_x >=1 and v_x <=-1.
    _, report = resolve_velocity([0., 0., 0.], [[1., 0., 0.], [-1., 0., 0.]], [[1., 0., 0.], [-1., 0., 0.]], settings)
    rows.append(dict(name='contradictory-moving-walls-reported', passed=report['status']=='unresolved-velocity-constraints', **report))
    translation = capsule_material_velocity([0., 0., 0.], [0., 0., 1.], [dt*2, 0., 0.], [dt*2, 0., 1.], .4, dt)
    rows.append(dict(name='translating-capsule-fixed-material-parameter', velocity_m_s=translation.tolist(),
        passed=bool(np.max(np.abs(translation-[2., 0., 0.])) <= tolerance)))
    # Prescribed rotating endpoint motion; same u at both times (axis adapter only).
    rotation = capsule_material_velocity([0., 0., 0.], [0., 0., 1.], [0., 0., 0.], [dt, 0., 1.], .25, dt)
    rows.append(dict(name='rotating-axis-fixed-material-parameter', velocity_m_s=rotation.tolist(),
        passed=bool(np.max(np.abs(rotation-[.25, 0., 0.])) <= tolerance)))
    rejected = False
    try:
        resolve_velocity([0., 0., 0.], [[0., 0., 0.]], [[0., 0., 0.]], settings)
    except AssertionError:
        rejected = True
    rows.append(dict(name='undefined-axis-contact-normal-rejected', passed=rejected))
    result = dict(schema_version=1, status='passed-isolated-velocity-fixtures-only' if all(row['passed'] for row in rows) else 'needs-review',
        cpu_only=True, fixture_count=len(rows), results=rows,
        profile_sha256=hashlib.sha256(profile_path.read_bytes()).hexdigest(),
        tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        solver_sha256=hashlib.sha256(Path(solver.__file__).read_bytes()).hexdigest(),
        limitations=['Analytic velocity projection only, not an actual-mesh or complete strand simulation.',
            'No position constraint solve, active-set discovery, pinned-root feasibility, substep release, swept/finite-width/self collision.',
            'Capsule normals supplied analytically; degenerate normal rejected, not auto-repaired.',
            'Material velocity adapter covers axis endpoints, not radial material spin on thick rotating capsules.',
            'Removing depenetration contribution does not prevent internal-constraint energy or guarantee stable coupled motion.',
            'Existing model attempts, surface limits, saved mesh and animation unchanged.'])
    out.mkdir(parents=True, exist_ok=False)
    (out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
    (out/'FeatureContactVelocity.py').write_bytes(Path(solver.__file__).read_bytes())
    (out/'profile.json').write_bytes(profile_path.read_bytes())
    (out/'study-report.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'status':result['status'], 'fixtures':len(rows), 'failed':[row['name'] for row in rows if not row['passed']]}))


if __name__ == '__main__':
    main()
