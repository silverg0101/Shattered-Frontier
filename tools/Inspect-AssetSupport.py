"""Read-only CPU support-region measurements from a source-hashed mesh inventory.

Regions are authored candidates, not automatic hoof/foot semantic detection.
Never uses the global minimum (which may be hair/debris) as a grounding decision.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np

p = argparse.ArgumentParser(description=__doc__)
for key in ['inspection', 'profile', 'output']:
    p.add_argument('--'+key, required=True)
a = p.parse_args()
inventory_path, profile_path, out = [Path(value).resolve() for value in [a.inspection, a.profile, a.output]]
inventory, profile = json.loads(inventory_path.read_text()), json.loads(profile_path.read_text())
assert profile['schema_version'] == 1
entry = next(row for row in inventory['objects'] if row['object'] == profile['object'])
assert entry['mesh_hash'] == profile['mesh_hash']
assert hashlib.sha256(Path(inventory['source']).read_bytes()).hexdigest() == inventory['source_sha256']
assert type(profile['ground_height_m']) is float and -.1 <= profile['ground_height_m'] <= .1
assert type(profile['contact_tolerance_m']) is float and .001 <= profile['contact_tolerance_m'] <= .03
data_path = inventory_path.parent/entry['selection_file']
data = np.load(data_path)
points, components = data['vertices'], data['component_per_vertex']
out.mkdir(parents=True, exist_ok=False)
(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
(out/'profile.json').write_bytes(profile_path.read_bytes())
rows, selections = [], {}
for region in profile['support_regions']:
    bounds = np.array(region['bounds_m'])
    assert bounds.shape == (3, 2) and np.all(bounds[:, 0] < bounds[:, 1])
    mask = np.all((points >= bounds[:, 0]) & (points <= bounds[:, 1]), axis=1)
    mask &= np.isin(components, region['permitted_components'])
    indices = np.flatnonzero(mask)
    assert len(indices) >= 10, 'Empty support candidate: '+region['name']
    selected = points[indices]
    lowest = selected[np.argmin(selected[:, 2])]
    height = float(lowest[2]-profile['ground_height_m'])
    contact = np.abs(selected[:, 2]-profile['ground_height_m']) <= profile['contact_tolerance_m']
    lower_band = selected[:, 2] <= selected[:, 2].min()+.01
    selections[region['name']] = indices
    rows.append(dict(name=region['name'], status='candidate-contact' if contact.any() else 'candidate-raised-support',
                     vertices=len(indices), minimum_height_m=height,
                     lower_fifth_percentile_height_m=float(np.quantile(selected[:, 2], .05)-profile['ground_height_m']),
                     closest_vertex=int(indices[np.argmin(selected[:, 2])]), closest_point_m=lowest.tolist(),
                     vertices_near_ground=int(contact.sum()),
                     lower_band_center_m=selected[lower_band].mean(0).tolist(),
                     bounds_m=[selected.min(0).tolist(), selected.max(0).tolist()]))
np.savez_compressed(out/'support-selections.npz', **selections)
report = dict(schema_version=1, status='needs-semantic-stance-review', source_sha256=inventory['source_sha256'],
              mesh_hash=entry['mesh_hash'], object=entry['object'], inspection_path=str(inventory_path),
              inspection_sha256=hashlib.sha256(inventory_path.read_bytes()).hexdigest(),
              selection_sha256=hashlib.sha256(data_path.read_bytes()).hexdigest(),
              profile_sha256=hashlib.sha256(profile_path.read_bytes()).hexdigest(), cpu_only=True,
              regions=rows, maximum_support_height_spread_m=max(r['minimum_height_m'] for r in rows)-min(r['minimum_height_m'] for r in rows),
              limitations=['Authored coordinate candidates need visual hoof/foot boundary approval.',
                           'A raised foot may be intentional pose; no automatic mesh translation or leg warp.',
                           'Static rest inspection only; no locomotion, ground collision or runtime acceptance.'])
(out/'support-report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(dict(status=report['status'], regions=[dict(name=r['name'], minimum_height_m=r['minimum_height_m']) for r in rows])))
