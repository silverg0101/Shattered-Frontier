"""Reproduce one locked Voidreign diagnostic revision, serialized and CPU only.

This asset-specific adapter starts from reviewed rest/selection artifacts. It
does not infer semantic selections, approve attachments, or call a GPU backend.
"""
import argparse
import hashlib
import json
import platform
from pathlib import Path
import subprocess
import sys
from datetime import datetime, timezone


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lock', default='docs/art-templates/model-profiles/voidreign-quality-reproduction-20261018.json')
    parser.add_argument('--output', required=True, help='New attempt directory; existing paths are rejected')
    parser.add_argument('--render', action='store_true', help='Six CPU diagnostic and material stills')
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    lock_path = (repo/args.lock).resolve()
    lock = json.loads(lock_path.read_text())
    assert lock['schema_version'] == 1
    out = (repo/args.output).resolve()
    assert out.is_relative_to(repo/'GameClient/ArtSource/Voidreign') or out.is_relative_to(repo/'Artifacts/Art/Voidreign')
    assert not out.exists(), 'Use a new immutable attempt directory'
    for item in lock['files']:
        path = (repo/item['path']).resolve()
        assert path.is_relative_to(repo) and path.is_file(), item['path']
        assert digest(path) == item['sha256'], 'Pinned input changed: '+item['path']
    blender = Path(lock['blender_executable'])
    assert blender.is_file()
    version = subprocess.run([str(blender), '--version'], capture_output=True, text=True, check=True).stdout
    assert version.splitlines()[0] == lock['blender_version'], version.splitlines()[0]
    out.mkdir(parents=True)
    (out/'reproduction-lock.json').write_bytes(lock_path.read_bytes())
    snapshots = out/'Inputs'
    for item in lock['files']:
        if item['role'] in ['tool', 'profile']:
            target = snapshots/item['path']
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((repo/item['path']).read_bytes())
    report = dict(status='running', started_utc=datetime.now(timezone.utc).isoformat(),
                  scope='CPU diagnostic reproduction; no semantic or engine acceptance',
                  python=sys.version, platform=platform.platform(), blender=version.splitlines()[0],
                  lock_sha256=digest(lock_path), stages=[])

    def save():
        (out/'run-report.json').write_text(json.dumps(report, indent=2)+'\n')

    def run(name, tool, arguments, use_blender=False):
        prefix = ([str(blender), '--background', '--factory-startup', '--threads', '4',
                   '--python-exit-code', '1', '--python', str(repo/'tools'/tool), '--']
                  if use_blender else [sys.executable, str(repo/'tools'/tool)])
        command = prefix+[str(value) for value in arguments]
        row = dict(stage=name, command=command, log=name+'.log', status='running')
        report['stages'].append(row)
        save()
        print('Starting', name, flush=True)
        with (out/row['log']).open('w', encoding='utf-8') as log:
            result = subprocess.run(command, cwd=repo, stdout=log, stderr=subprocess.STDOUT)
        row.update(exit_code=result.returncode, status='completed' if result.returncode == 0 else 'failed')
        save()
        if result.returncode:
            raise RuntimeError(name+' failed; see '+str(out/row['log']))

    settings = lock['inputs']
    source, selection, attachments, motion, surface, exchange = [repo/settings[key] for key in
        ['source', 'selection', 'attachments', 'motion_profile', 'surface_profile', 'exchange_profile']]
    guide, simulation, rig, surfaces, imports = [out/name for name in ['Guides', 'Simulation', 'Rig', 'Surfaces', 'Imports']]
    try:
        run('guides', 'Build-FeatureGuides.py', ['--selection', selection, '--attachments', attachments,
            '--motion-profile', motion, '--allow-proposed-bindings', '--output', guide])
        run('simulation', 'Simulate-FeatureGuides.py', ['--guides', guide/'guides.json', '--config', motion, '--output', simulation])
        assert json.loads((simulation/'simulation-report.json').read_text())['status'] == 'passed-particle-checks-only', 'Particle checks failed; no rig bake'
        run('rig', 'Build-FeatureMotionRig.py', ['--source', source, '--guide-report', guide/'guide-report.json',
            '--simulation', simulation, '--config', motion, '--output', rig], True)
        model = rig/'Voidreign-CompleteHair.blend'
        run('surfaces', 'Check-FeatureMotionSurfaces.py', ['--source', model, '--guide-report', guide/'guide-report.json',
            '--config', motion, '--profile', surface, '--output', surfaces], True)
        run('import-contract', 'BlenderRigImport.py', ['capture', '--source', model,
            '--exchange', rig/'Voidreign-CompleteHair.fbx', '--armature', 'RIG | complete feature guides',
            '--output', imports/'rig-import-contract.json'], True)
        run('exchange', 'Validate-VoidreignRigImport.py', ['--folder', rig, '--output', imports,
            '--name', 'Voidreign-CompleteHair', '--profile', exchange], True)
        if args.render:
            for mode in ['Diagnostic', 'Material']:
                render_args = ['--source', model, '--object', settings['object'], '--output', out/mode]
                for frame in [1, 60, 120, 180, 240, 300]:
                    render_args += ['--frame', frame]
                if mode == 'Diagnostic':
                    render_args += ['--diagnostic']
                run('render-'+mode.lower(), 'Render-FeatureMotion.py', render_args, True)
        report['surface_status'] = json.loads((surfaces/'surface-report.json').read_text())['status']
        report['exchange_status'] = json.loads((imports/'validation.json').read_text())['status']
        report['status'] = 'diagnostic-checks-passed-review-required' if report['surface_status'] == 'passed-scoped-surface-checks' else 'needs-review'
    except Exception as error:
        report.update(status='needs-review', error=str(error))
        raise
    finally:
        report['finished_utc'] = datetime.now(timezone.utc).isoformat()
        report['outputs'] = [dict(path=str(path.relative_to(out)), sha256=digest(path))
                             for path in sorted(out.rglob('*')) if path.is_file() and path.name != 'run-report.json']
        save()
    print(json.dumps(dict(status=report['status'], report=str(out/'run-report.json'))))


if __name__ == '__main__':
    main()
