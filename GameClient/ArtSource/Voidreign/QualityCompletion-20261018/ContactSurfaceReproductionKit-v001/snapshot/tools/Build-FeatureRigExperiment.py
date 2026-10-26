"""Bake one explicitly scoped diagnostic rig without promoting failed design gates.

The original builder remains unchanged. A hash-pinned experiment contract may
replace only its particle-status assertion after all other guards succeed.
This tool never accepts the result as a production rig.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--experiment', required=True)
args = p.parse_args(sys.argv[sys.argv.index('--') + 1:])
profile_path = Path(args.experiment).resolve()
contract = json.loads(profile_path.read_text())
assert contract['purpose'] == 'one-diagnostic-existing-mesh-evaluation'
assert contract['maximum_evaluations'] == 1
for path, expected in contract['input_sha256'].items():
    assert digest(path) == expected, f'Input changed: {path}'
paths = contract['paths']
out = Path(paths['output']).resolve()
assert not out.exists(), 'Immutable output already exists'
simulation = Path(paths['simulation'])
report = json.loads((simulation / 'simulation-report.json').read_text())
step = json.loads(Path(paths['trajectory_check']).read_text())
clearance = json.loads(Path(paths['clearance']).read_text())
baseline = json.loads(Path(paths['baseline_clearance']).read_text())
assert report['status'] == 'needs-review'
assert report['particle_status_before_design_gates'] == 'passed-particle-checks-only'
assert report['design_warning_gates'] == dict(full_duration_step=True,
    fixed_envelope_feasibility=False, contact_velocity_convergence=True,
    defined_contact_normals=True)
assert report['trajectory_sha256'] == digest(simulation / 'trajectory.npz')
assert step['trajectory_provenance'] == 'producer-hash-verified'
assert step['status'] == 'passed-guide-step-only' and step['violation_count'] == 0
assert step['peak_world_step']['value'] <= step['maximum_world_step_m']
assert clearance['trajectory_provenance'] == 'producer-hash-verified'
assert clearance['free_node_status'] == 'passed-free-guide-envelope-only'
assert clearance['maximum_fixed_penetration_m'] == baseline['maximum_fixed_penetration_m']
assert clearance['maximum_penetration_warning_m'] == baseline['maximum_penetration_warning_m']
assert clearance['maximum_fixed_penetration_m'] > clearance['maximum_penetration_warning_m']
builder_path = Path(paths['builder']).resolve()
original = builder_path.read_text()
anchor = "assert simulation_report['status'] == 'passed-particle-checks-only', 'Repair failed particle checks before rig baking'"
assert original.count(anchor) == 1
replacement = 'experiment_guard(simulation_report)'
modified = original.replace(anchor, replacement)
adapter_dir = Path(paths['adapter_output']).resolve()
adapter_dir.mkdir(parents=True, exist_ok=False)
modified_path = adapter_dir / 'adapted-builder.py'
modified_path.write_text(modified)
(adapter_dir / 'original-builder.py').write_bytes(builder_path.read_bytes())
(adapter_dir / 'experiment-profile.json').write_bytes(profile_path.read_bytes())
(adapter_dir / 'replay-wrapper.py').write_bytes(Path(__file__).read_bytes())


def experiment_guard(actual):
    assert actual == report, 'Simulation report changed after preflight'
    assert digest(simulation / 'trajectory.npz') == report['trajectory_sha256']


sys.argv = [str(modified_path), '--']
for name in ['source', 'guide-report', 'simulation', 'config', 'output']:
    sys.argv += ['--' + name, paths[name.replace('-', '_')]]
exec(compile(modified, str(modified_path), 'exec'),
     dict(__name__='__main__', __file__=str(modified_path), experiment_guard=experiment_guard))
for path, expected in contract['input_sha256'].items():
    assert digest(path) == expected, f'Input changed during bake: {path}'
result = dict(status='diagnostic-experiment-needs-review', cpu_only=True,
    purpose=contract['purpose'], experiment_profile_sha256=digest(profile_path),
    input_sha256=contract['input_sha256'],
    adapted_builder_sha256=digest(modified_path),
    unchanged_builder_sha256=digest(builder_path),
    original_simulation_status=report['status'],
    failed_fixed_envelope_gate_preserved=True,
    maximum_fixed_penetration_m=clearance['maximum_fixed_penetration_m'],
    open_gates=contract['open_gates'],
    outputs={f.name: digest(f) for f in out.iterdir() if f.is_file()})
(out / 'experiment-report.json').write_text(json.dumps(result, indent=2))
print(json.dumps({k: result[k] for k in ['status', 'purpose', 'open_gates']}), flush=True)
