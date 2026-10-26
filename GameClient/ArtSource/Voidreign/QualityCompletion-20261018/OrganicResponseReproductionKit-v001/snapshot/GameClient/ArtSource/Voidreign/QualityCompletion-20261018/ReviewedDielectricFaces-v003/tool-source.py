"""Bind explicit reviewed face approvals to source topology and image evidence.

Produces selection data only. It does not infer classes, edit shaders, or
approve a model for import/runtime.
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile', required=True)
p.add_argument('--output', required=True)
a = p.parse_args()
profile, out = Path(a.profile).resolve(), Path(a.output).resolve()
cfg = json.loads(profile.read_text(encoding='utf-8-sig'))
assert cfg['schema_version'] == 1 and not out.exists()
assert cfg['approval_scope'] == 'offline-material-authoring-only'
assert cfg['response']['metallic'] == 0
assert cfg['response']['unit'] == 'unit interval'
assert 1 <= len(cfg['items']) <= 128
assert len({r['id'] for r in cfg['items']}) == len(cfg['items'])
for path, expected in cfg['input_sha256'].items():
    assert sha(path) == expected, path
for key in ['source', 'review', 'inventory', 'selection', 'rest_selection','source_retention']:
    assert cfg[key] in cfg['input_sha256']
review = json.loads(Path(cfg['review']).read_text(encoding='utf-8'))
inventory = json.loads(Path(cfg['inventory']).read_text(encoding='utf-8'))
review_items = {row['id']: row for row in review['items']}
inventory_items = {row['id']: row for row in inventory['items']}
assert cfg['input_sha256'][cfg['rest_selection']] in inventory['input_sha256'].values()
with np.load(cfg['rest_selection']) as ar:
    triangles = ar['triangles']
retention=json.loads(Path(cfg['source_retention']).read_text(encoding='utf-8'))
assert retention['status']=='passed-indexed-rest-retention-only'
assert retention['candidate_sha256']==cfg['input_sha256'][cfg['source']]
expected=retention['after']['faces']
assert list(triangles.shape)==expected['shape']
assert hashlib.sha256(triangles.astype(expected['dtype']).tobytes()).hexdigest()==expected['sha256']
groups = {'strand_dielectric': [], 'bone_dielectric': [], 'organic_dielectric': []}
rows = []
with np.load(cfg['selection']) as ar:
    mixed, whole = ar['mixed_faces'], ar['whole_faces']
    for item in cfg['items']:
        original = inventory_items[item['id']]
        prior = review_items[item['id']]
        assert item['material_class'] in groups and item['basis']
        faces = np.array(item['face_ids'], np.int32)
        assert faces.ndim == 1 and len(faces) and len(np.unique(faces)) == len(faces)
        assert np.isin(faces, ar[original['face_key']]).all()
        assert faces.min() >= 0 and faces.max() < len(triangles)
        assert prior['evidence_images']
        for image in prior['evidence_images']:
            assert sha(image['path']) == image['sha256'], image['path']
        groups[item['material_class']].extend(faces.tolist())
        rows.append(dict(id=item['id'], material_class=item['material_class'],
                         faces=len(faces), basis=item['basis']))
selected = np.sort(np.concatenate([np.array(v, np.int32) for v in groups.values()]))
assert len(selected) == len(np.unique(selected))
assert np.isin(selected, mixed).all() and not np.isin(selected, whole).any()
unresolved = np.setdiff1d(mixed, selected)
assert len(selected) + len(unresolved) == len(mixed)
out.mkdir(parents=True)
(out / 'profile.json').write_bytes(profile.read_bytes())
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
np.savez_compressed(out / 'approved-response-faces.npz', approved_faces=selected,
    unresolved_mixed_faces=unresolved,
    **{k + '_faces': np.array(sorted(v), np.int32) for k, v in groups.items()})
report = dict(schema_version=1, status='reviewed-authoring-selection-no-material-edit',
    approval_scope=cfg['approval_scope'], reviewer=cfg['reviewer'],
    profile_sha256=sha(profile), tool_sha256=sha(out / 'tool-source.py'),
    input_sha256=cfg['input_sha256'], response=cfg['response'], items=rows,
    approved_faces=len(selected), unresolved_mixed_faces=len(unresolved),
    class_faces={k: len(v) for k, v in groups.items()},
    limitations=['Explicit author judgement; evidence/hash/subset checks do not infer or prove material identity.',
                 'Organic dielectric is a shared metallic-zero response; finer tissue/hair/bone identity, roughness and motion remain separate.',
                 'Selection only; no shader, image, geometry, UV, rig, motion label or attempt-counter edit.',
                 'No roughness, normal, emission, full material coverage or engine acceptance.'])
(out / 'approval.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({k: report[k] for k in ['status', 'approved_faces', 'unresolved_mixed_faces', 'class_faces']}))
