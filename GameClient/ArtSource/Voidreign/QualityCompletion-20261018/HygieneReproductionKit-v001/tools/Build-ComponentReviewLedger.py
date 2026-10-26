"""Create a finite source-hashed component review ledger without auto-approval."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np

p = argparse.ArgumentParser(description=__doc__)
for key in ['inspection', 'selection', 'attachments', 'output']:
    p.add_argument('--'+key, required=True)
a = p.parse_args()
inventory_path, selection_path, attachment_path, out = [Path(value).resolve() for value in [a.inspection, a.selection, a.attachments, a.output]]
inventory, selection, attachments = [json.loads(path.read_text()) for path in [inventory_path, selection_path, attachment_path]]
entry = next(row for row in inventory['objects'] if row['object'] == selection['object'])
assert entry['mesh_hash'] == selection['mesh_hash'] == attachments['mesh_hash']
assert inventory['source_sha256'] == selection['source_sha256']
assert attachments['selection_sha256'] == hashlib.sha256(selection_path.read_bytes()).hexdigest()
data = np.load(inventory_path.parent/entry['selection_file'])
semantic = np.load(selection_path.parent/selection['selection_file'])
profile = json.loads(Path(selection['profile_path']).read_text())
feature_names = ['Unassigned']+[feature['name'] for feature in profile['features']]
rows = []
for component in entry['components']:
    mask = data['component_per_vertex'] == component['component_index']
    counts = np.bincount(semantic['labels'][mask], minlength=len(feature_names))
    relations = [dict(id=relation['id'], status=relation['status'], gap_m=relation['gap_m'])
                 for relation in attachments['proposals'] if component['id'] in relation['child_source_component_ids']]
    rows.append(dict(component_id=component['id'], component_index=component['component_index'],
                     status='needs-review', classification=None, disposition=None,
                     vertices=component['vertices'], triangles=component['triangles'], bounds_m=component['bounds_m'],
                     sampled_main_gap_m=component.get('sampled_main_surface_gap_m'),
                     semantic_vertices={name: int(count) for name, count in zip(feature_names, counts)},
                     proposed_child_bindings=relations,
                     next_action='Review purpose, intended attachment and rest/motion evidence; retain/rebuild/remove only after explicit disposition.'))
rows.sort(key=lambda row: (bool(row['proposed_child_bindings']), -row['triangles']))
out.mkdir(parents=True, exist_ok=False)
(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
report = dict(schema_version=1, status='finite-review-queue; no semantic auto-approval',
              source_sha256=inventory['source_sha256'], mesh_hash=entry['mesh_hash'],
              object=entry['object'], inspection_path=str(inventory_path), selection_path=str(selection_path),
              attachment_path=str(attachment_path),
              inputs_sha256={str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                             for path in [inventory_path, selection_path, attachment_path]},
              component_count=len(rows), next_component_ids=[row['component_id'] for row in rows if row['component_index'] != 0][:8],
              components=rows, limitations=['Counts/gaps/semantic masks suggest review priority, never defect or attachment approval.',
                                           'Source vertex IDs and component IDs become stale after topology changes.',
                                           'Proposed skinned bindings are not proof of physical or visual attachment.'])
(out/'component-review-ledger.json').write_text(json.dumps(report, indent=2))
print(json.dumps(dict(components=len(rows), next_component_ids=report['next_component_ids'])))
