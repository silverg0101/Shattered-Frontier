"""Export finite posed component inventories from a checked indexed point cache.

Uses the complete evaluated source world positions captured by the offline
Alembic exchange check. No Blender execution, source editing or reclassification.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(8 * 1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile', required=True)
p.add_argument('--output', required=True)
a = p.parse_args()
profile, out = Path(a.profile).resolve(), Path(a.output).resolve()
cfg = json.loads(profile.read_text(encoding='utf-8'))
assert cfg['schema_version'] == 1 and not out.exists()
assert isinstance(cfg['frames'], list) and 1 <= len(cfg['frames']) <= 64
assert len(set(cfg['frames'])) == len(cfg['frames'])
for path, expected in cfg['input_sha256'].items():
    assert sha(path) == expected, path
for key in ['source', 'inspection', 'points', 'producer_report']:
    assert cfg[key] in cfg['input_sha256']
producer = json.loads(Path(cfg['producer_report']).read_text())
assert producer['status'] == 'passed-indexed-animated-cache-reimport-only'
assert producer['input_sha256'][cfg['source']] == cfg['input_sha256'][cfg['source']]
inventory_path = Path(cfg['inspection'])
inventory = json.loads(inventory_path.read_text())
entry = next(row for row in inventory['objects'] if row['object'] == cfg['object'])
selection_file = inventory_path.parent / entry['selection_file']
assert str(selection_file) in cfg['input_sha256']
with np.load(selection_file, allow_pickle=False) as archive:
    data = {key: archive[key] for key in archive.files}
points = np.load(cfg['points'], mmap_mode='r', allow_pickle=False)
assert points.dtype == np.float32 and points.shape == (producer['frames'], producer['vertices'], 3)
assert points.shape[1] == len(data['vertices'])
assert all(type(frame) is int and 1 <= frame <= len(points) for frame in cfg['frames'])
assert data['triangles'].max() < points.shape[1]
out.mkdir(parents=True)
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
(out / 'profile.json').write_bytes(profile.read_bytes())
records = []
for frame in cfg['frames']:
    xyz = points[frame - 1]
    assert np.isfinite(xyz).all()
    folder = out / f'Frame-{frame:04}'
    folder.mkdir()
    revised = json.loads(json.dumps(entry))
    for row in revised['components']:
        selected = xyz[data['component_per_vertex'] == row['component_index']]
        assert len(selected) > 0
        row.update(bounds_m=[selected.min(0).tolist(), selected.max(0).tolist()],
                   centroid_m=selected.mean(0).tolist(), dimensions_m=np.ptp(selected, axis=0).tolist())
        row['rest_sampled_main_surface_gap_m'] = row.pop('sampled_main_surface_gap_m', None)
        row['distance_note'] = 'Historical rest gap only; no posed gap recomputed.'
    np.savez_compressed(folder / revised['selection_file'],
        **{key: xyz if key == 'vertices' else value for key, value in data.items()})
    report = dict(schema_version=1, status='evaluated-cached-pose-inspection-only',
        source=cfg['source'], source_sha256=cfg['input_sha256'][cfg['source']],
        frame=frame, pose='Evaluated source WORLD positions in metres; IDs identify original rest components.',
        rest_inspection_sha256=cfg['input_sha256'][cfg['inspection']],
        input_sha256=cfg['input_sha256'], tool_sha256=sha(__file__),
        objects=[revised], limitations=['Finite saved poses only; no visibility, semantics or deletion approval.',
            'Exact cached indexed source positions; rest topology retained, source geometry is not modified.'])
    (folder / 'component-report.json').write_text(json.dumps(report, indent=2))
    records.append(dict(frame=frame, inspection=str(folder / 'component-report.json')))
(out / 'frames.json').write_text(json.dumps(records, indent=2))
for path, expected in cfg['input_sha256'].items():
    assert sha(path) == expected, path
print(json.dumps({'status': 'cached-component-inventories-exported', 'frames': cfg['frames']}))
