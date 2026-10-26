"""Check source-bound UV correspondence in saved diagnostic atlas copies."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

import bpy
import numpy as np


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--atlas', action='append', required=True)
p.add_argument('--retention', required=True)
p.add_argument('--output', required=True)
a = p.parse_args(sys.argv[sys.argv.index('--') + 1:])
out = Path(a.output).resolve()
assert not out.exists() and 1 <= len(a.atlas) <= 4
retention = json.loads(Path(a.retention).read_text(encoding='utf-8'))
assert retention['status'] == 'passed-indexed-rest-retention-only'
rows = []
for folder in a.atlas:
    folder = Path(folder).resolve()
    report = json.loads((folder / 'anatomy-report.json').read_text(encoding='utf-8'))
    profile = json.loads((folder / 'profile.json').read_text(encoding='utf-8'))
    assert sha(folder / 'profile.json') == report['profile_sha256']
    assert sha(a.retention) == report['input_sha256'][profile['retention_report']]
    assert report['input_sha256'][profile['source']] == retention['candidate_sha256']
    snapshot = sha(folder / 'tool-source.py')
    provenance = 'matches-report'
    if snapshot != report['tool_sha256']:
        clarification = json.loads((folder / 'tool-provenance-clarification.json').read_text())
        assert clarification['execution_snapshot_sha256'] == snapshot
        assert clarification['reported_tool_sha256'] == report['tool_sha256']
        provenance = 'raw-mismatch-preserved-use-clarified-execution-snapshot'
    bpy.ops.wm.open_mainfile(filepath=str(folder / 'Mount-AnatomyProposal.blend'))
    base = bpy.data.objects['REFERENCE | original indexed rest surface'].data
    base_uv = {}
    for name, expected in retention['after']['uv'].items():
        layer = base.uv_layers[name]
        values = np.empty((len(layer.data), 2), np.float32)
        layer.data.foreach_get('uv', values.ravel())
        assert list(values.shape) == expected['shape']
        assert hashlib.sha256(values.tobytes()).hexdigest() == expected['sha256']
        base_uv[name] = values
    checked = 0
    archive = folder / 'isolated-source-faces.npz'
    if archive.exists():
        with np.load(archive) as selections:
            for key in selections.files:
                ids = selections[key]
                isolated = bpy.data.objects['DIAGNOSTIC ISOLATION | ' + key].data
                loops = np.array([list(base.polygons[int(i)].loop_indices) for i in ids]).ravel()
                for name, original_uv in base_uv.items():
                    values = np.empty((len(isolated.uv_layers[name].data), 2), np.float32)
                    isolated.uv_layers[name].data.foreach_get('uv', values.ravel())
                    assert np.array_equal(values, original_uv[loops]), key
                checked += 1
    rows.append(dict(atlas=str(folder), source_sha256=sha(folder / 'Mount-AnatomyProposal.blend'),
        execution_tool_sha256=snapshot, tool_provenance=provenance,
        source_uv_layers=list(base_uv), isolated_meshes_checked=checked))
out.mkdir(parents=True)
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
result = dict(status='passed-saved-atlas-source-UV-correspondence-only',
    retention_sha256=sha(a.retention), atlases=rows,
    limitations=['UV/source-loop correspondence only; no material identity, PBR, lighting or runtime acceptance.',
                 'A clarified execution hash does not rewrite or turn the original mismatched provenance field into a pass.'])
(out / 'verification.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result), flush=True)
