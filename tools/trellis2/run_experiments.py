"""Run the fixed Voidreign screening cases sequentially on the single Radeon GPU."""
import argparse
import datetime
import json
from pathlib import Path
import subprocess
import sys

p = argparse.ArgumentParser()
p.add_argument('--output', required=True)
a = p.parse_args()
root = Path(a.output)
root.mkdir(parents=True, exist_ok=True)
if (root / 'experiments.json').exists():
    raise RuntimeError('Choose a new experiment folder')
tools = Path(__file__).resolve().parent
spec = json.loads((tools / 'voidreign-experiments.json').read_text())
report = {'status': 'running', 'started_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'specification': spec, 'cases': []}
def save():
    temporary = root / 'experiments.tmp'
    temporary.write_text(json.dumps(report, indent=2))
    temporary.replace(root / 'experiments.json')
save()
for case in spec['cases']:
    entry = dict(case, status='running')
    report['cases'].append(entry)
    save()
    command = [sys.executable, '-u', str(tools / 'run_voidreign.py'), '--output', str(root / case['name']),
               '--pipeline', case['pipeline'], '--steps', str(case['steps']), '--decimation-target', str(case['decimation_target']),
               '--seed', str(spec['seed']), '--texture-size', str(spec['texture_size'])]
    with (root / (case['name'] + '.log')).open('w') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    entry.update(status='complete' if result.returncode == 0 else 'failed', returncode=result.returncode)
    save()
report['status'] = 'complete' if all(c['status'] == 'complete' for c in report['cases']) else 'completed_with_failures'
save()
