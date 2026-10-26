"""Finite CPU analytic fixtures for the proposed split-contact velocity module.

This does not run or fix Voidreign. Expected values are analytic fixture
contracts, not new model acceptance tolerances.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from FeatureContactVelocity import ContactSettings, split_candidate, resolve_velocity, resolve_velocity_minimum_impulse, capsule_material_velocity
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
    # Length-preserving axis rotation; same u at both times, radial spin excluded.
    theta = .01
    endpoint = np.array([np.sin(theta), 0., np.cos(theta)])
    rotation = capsule_material_velocity([0., 0., 0.], [0., 0., 1.], [0., 0., 0.], endpoint, .25, dt)
    expected_rotation = (endpoint-[0., 0., 1.])*.25/dt
    rows.append(dict(name='rotating-axis-fixed-material-parameter', velocity_m_s=rotation.tolist(),
        endpoint_length_m=float(np.linalg.norm(endpoint)), expected_m_s=expected_rotation.tolist(),
        passed=bool(np.max(np.abs(rotation-expected_rotation)) <= tolerance and abs(np.linalg.norm(endpoint)-1.) <= tolerance)))
    normals = np.array([[1., 0., 0.], [1./np.sqrt(2), 1./np.sqrt(2), 0.]])
    forward, first = resolve_velocity([-1., -1., 0.], normals, np.zeros((2, 3)), settings)
    reverse, second = resolve_velocity([-1., -1., 0.], normals[::-1], np.zeros((2, 3)), settings)
    rows.append(dict(name='oblique-contact-order-dependence-exposed', forward_m_s=forward.tolist(), reverse_m_s=reverse.tolist(),
        passed=bool(first['status']==second['status']=='passed-velocity-constraints-only'
            and np.max(np.abs(forward-[.5, -.5, 0.])) <= tolerance and np.max(np.abs(reverse)) <= tolerance),
        meaning='Both feasible, different impulses; candidate is not a nearest-velocity solver.'))
    rejected = False
    try:
        resolve_velocity([0., 0., 0.], [[0., 0., 0.]], [[0., 0., 0.]], settings)
    except AssertionError:
        rejected = True
    rows.append(dict(name='undefined-axis-contact-normal-rejected', passed=rejected))
    # New algorithm keeps the old PGS regression evidence above, then checks the
    # distinct minimum-impulse design on order and nearly opposing constraints.
    oblique=np.array([[1.,0.,0.],[1./np.sqrt(2),1./np.sqrt(2),0.]])
    for name,velocity,normals,walls,expected in [
        ('minimum-oblique-forward',[-1.,-1.,0.],oblique,np.zeros((2,3)),[0.,0.,0.]),
        ('minimum-oblique-reversed',[-1.,-1.,0.],oblique[::-1],np.zeros((2,3)),[0.,0.,0.]),
        ('minimum-moving-oblique',[0.,0.,0.],oblique,[[1.,0.,0.],[2.,0.,0.]],[1.,1.,0.]),
        ('minimum-duplicate-normals',[0.,0.,0.],[[1.,0.,0.],[1.,0.,0.]],[[1.,0.,0.],[2.,0.,0.]],[2.,0.,0.]),
        ('minimum-three-contact-corner',[0.,0.,0.],np.eye(3),np.diag([1.,2.,3.]),[1.,2.,3.]),
        ('minimum-nearly-opposed',[0.,0.,0.],[[0.,0.,1.],[1e-5,0.,-np.sqrt(1.-1e-10)]],[[0.,0.,0.],[1.,0.,0.]],[1.,0.,0.])]:
        value,report=resolve_velocity_minimum_impulse(velocity,normals,walls,settings)
        rows.append(dict(name=name,resolved_m_s=value.tolist(),expected_m_s=expected,
            passed=bool(report['status']=='passed-velocity-constraints-only' and np.max(np.abs(value-expected))<=tolerance),**report))
    _,report=resolve_velocity_minimum_impulse([0.,0.,0.],[[1.,0.,0.],[-1.,0.,0.]],[[1.,0.,0.],[-1.,0.,0.]],settings)
    rows.append(dict(name='minimum-infeasible-reported',passed=report['status']=='unresolved-velocity-constraints',**report))
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
            'Sequential normal projections are contact-order dependent and do not guarantee minimum impulse; oblique fixture exposes this.',
            'Existing model attempts, surface limits, saved mesh and animation unchanged.'])
    out.mkdir(parents=True, exist_ok=False)
    (out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
    (out/'FeatureContactVelocity.py').write_bytes(Path(solver.__file__).read_bytes())
    (out/'profile.json').write_bytes(profile_path.read_bytes())
    (out/'study-report.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'status':result['status'], 'fixtures':len(rows), 'failed':[row['name'] for row in rows if not row['passed']]}))


if __name__ == '__main__':
    main()
