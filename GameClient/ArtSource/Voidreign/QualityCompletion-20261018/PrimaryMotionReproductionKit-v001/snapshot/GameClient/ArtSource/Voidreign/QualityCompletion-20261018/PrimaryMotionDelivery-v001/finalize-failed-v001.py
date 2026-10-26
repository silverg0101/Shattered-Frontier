"""Finite Voidreign delivery replay. Requires completed full Tail producer; CPU only.

Run from repository root with py -3 <this file>. Immutable output folders;
failure stops the sequence and preserves evidence. Not a generic mount preset.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time
import numpy as np

ROOT = Path(__file__).resolve().parents[5]
assert ROOT == Path.cwd().resolve(), 'Run from repository root'
B = Path('GameClient/ArtSource/Voidreign/QualityCompletion-20261018')
D = B / 'PrimaryMotionDelivery-v001'
P = Path('docs/art-templates/model-profiles')
BLENDER = r'C:/Program Files/Blender Foundation/Blender 5.1/blender.exe'
commands = []

def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(8 * 1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()

def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))

def save(path, data):
    path = Path(path)
    assert not path.exists(), path
    path.write_text(json.dumps(data, indent=2), encoding='utf-8')

def run(tool, args, label, blender=True):
    prefix = [BLENDER, '--background', '--factory-startup', '--threads', '4',
              '--python-exit-code', '1', '--python', 'tools/' + tool, '--'] if blender else [sys.executable, 'tools/' + tool]
    command = prefix + list(map(str, args))
    commands.append(command)
    (D / 'commands.json').write_text(json.dumps(commands, indent=2))
    print('START ' + label, flush=True)
    with (D / (label + '.log')).open('w', encoding='utf-8') as stream:
        subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, check=True)
    print('DONE ' + label, flush=True)

tail = B / 'TailPrimaryOnlyFull-v001'
# Bounded wait for the already authorized producer; do not start a duplicate.
deadline = time.monotonic() + 1200
while not (tail / 'pilot-report.json').exists():
    assert time.monotonic() < deadline, 'Tail producer did not complete within20min'
    time.sleep(10)
t = load(tail / 'pilot-report.json')
assert t['warmup_velocity']['passed'] and t['fresh_cache_retime_max_error_m'] <= 1e-5
assert t['frame_end'] == 330 and t['deformation_input_mode'] == 'rigid-assembly-rest'
with np.load(tail / 'positive-warmup-reference.npz') as new, np.load(B / 'TailFinalOutputPoints-v001/output-points.npz') as old:
    assert np.array_equal(new['source_vertex_ids'], old['source_vertex_ids'])
    assert new['points'].shape[:2] == (330, 33233)

cfg = load(P / 'voidreign-primary-only-combined-pilot-cache-20261018.json')
cfg['permissions'] = 'Offline full330 primary-only actual-mesh review; no accepted-source promotion'
cfg['frame_end'] = 330
cfg['positions_file'] = str(tail / 'positive-warmup-reference.npz')
inputs = [cfg['source'], cfg['positions_file']] + [r['path'] for r in cfg['inherited_mesh_caches']]
cfg['input_sha256'] = {p: sha(p) for p in inputs}
cp = P / 'voidreign-primary-only-combined-full-cache-20261018.json'
save(cp, cfg)
combined = B / 'PrimaryOnlyCombinedFull-v001'
run('Build-FeatureMotionCache.py', ['--profile', cp, '--output', combined], 'combined-cache')
assert load(combined / 'cache-report.json')['status'] == 'passed-feature-cache-reopen-only'
source = combined / 'Voidreign-ActualSurfacePilot.blend'

check = B / 'PrimaryOnlyCombinedFullCheck-v001'
run('Check-FeatureMotionSurfaces.py', ['--source', source, '--guide-report', B / 'HygieneGuideProposal-v001/guide-report.json',
    '--config', P / 'voidreign-complete-hair-motion-v003-20261018.json',
    '--profile', P / 'voidreign-full-frame-surface-check-20261018.json', '--output', check], 'surface-check')
assert load(check / 'surface-report.json')['status'] == 'passed-scoped-surface-checks'
retention = B / 'PrimaryOnlyCombinedFullRetention-v001'
run('Check-AssetRigRetention.py', ['--source', B / 'DegenerateCleanup-v001/Voidreign-CompleteHair.blend',
    '--candidate', source, '--object', 'geometry_0', '--output', retention], 'rest-retention')
assert load(retention / 'validation.json')['status'] == 'passed-indexed-rest-retention-only'

ep = P / 'voidreign-primary-only-combined-full-alembic-20261018.json'
exchange = B / 'PrimaryOnlyCombinedFullAlembic-v001'
inputs = [str(source), str(combined / 'Feature-actual-motion.mdd'), str(combined / 'Mane-primary-motion.mdd')]
save(ep, dict(schema_version=1, source=str(source), object='geometry_0', frame_start=1, frame_end=330,
              fps=30, maximum_error_m=1e-5, input_sha256={p: sha(p) for p in inputs}))
run('Export-CheckedMotionAlembic.py', ['--profile', ep, '--output', exchange], 'animated-exchange')
assert load(exchange / 'exchange-report.json')['status'] == 'passed-indexed-animated-cache-reimport-only'
cfg = load(P / 'voidreign-source-surface-strain-20261018.json')
cfg.update(source=str(source), world_points=str(exchange / 'expected-world-points.npy'),
           producer_report=str(exchange / 'exchange-report.json'))
inputs = [cfg[k] for k in ['source', 'world_points', 'producer_report', 'rest_selection', 'feature_selection']]
cfg['input_sha256'] = {p: sha(p) for p in inputs}
sp = P / 'voidreign-primary-only-combined-full-strain-20261018.json'
save(sp, cfg)
run('Analyze-AssetSurfaceStrain.py', ['--profile', sp, '--output', B / 'PrimaryOnlyCombinedFullStrain-v001'], 'surface-strain', False)
for diagnostic, name in [(False, 'PrimaryOnlyCombinedFullPreview-v001'), (True, 'PrimaryOnlyCombinedFullDiagnostic-v001')]:
    args = ['--source', source, '--object', 'geometry_0', '--samples', '4', '--output', B / name]
    for frame in [1, 121, 211, 301]:
        args += ['--frame', str(frame)]
    if diagnostic:
        args.append('--diagnostic')
    run('Render-FeatureMotion.py', args, 'diagnostic-preview' if diagnostic else 'textured-preview')
save(D / 'completion.json', dict(status='offline-checks-complete-needs-visual-review', source=str(source),
     source_sha256=sha(source), cpu_only=True, commands='commands.json',
     limitations=['No gait, material, semantic, triangle/self/swept collision or runtime acceptance.']))
print('READY FOR VISIBLE BLENDER REVIEW', flush=True)
