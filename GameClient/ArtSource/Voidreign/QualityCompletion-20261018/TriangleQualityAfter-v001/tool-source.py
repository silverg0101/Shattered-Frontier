"""CPU source-hashed triangle/edge diagnostics; no topology repair or approval."""
import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import numpy as np


@dataclass(frozen=True)
class Thresholds:
    minimum_area_m2: float
    minimum_altitude_m: float
    maximum_aspect_ratio: float


p = argparse.ArgumentParser(description=__doc__)
for key in ['inspection', 'object', 'profile', 'output']:
    p.add_argument('--'+key, required=True)
p.add_argument('--visibility', help='Optional same-mesh sampled visibility report for review cross-reference')
a = p.parse_args()
inventory_path, profile_path, out = [Path(value).resolve() for value in [a.inspection, a.profile, a.output]]
inventory, profile = json.loads(inventory_path.read_text()), json.loads(profile_path.read_text())
assert profile['schema_version'] == 1
cfg = Thresholds(**profile['thresholds'])
assert type(cfg.minimum_area_m2) is float and 0 < cfg.minimum_area_m2 <= 1e-6
assert type(cfg.minimum_altitude_m) is float and 0 < cfg.minimum_altitude_m <= .001
assert type(cfg.maximum_aspect_ratio) is float and 10 <= cfg.maximum_aspect_ratio <= 1e6
entry = next(row for row in inventory['objects'] if row['object'] == a.object)
assert hashlib.sha256(Path(inventory['source']).read_bytes()).hexdigest() == inventory['source_sha256']
data_path = inventory_path.parent/entry['selection_file']
data = np.load(data_path)
points, faces = data['vertices'].astype(np.float64), data['triangles']
local = data['local_vertices'].astype(np.float32)
assert hashlib.sha256(local.tobytes()+faces.astype(np.int32).tobytes()).hexdigest() == entry['mesh_hash']
corners = points[faces]
lengths = np.linalg.norm(corners-np.roll(corners, -1, axis=1), axis=2)
area = np.linalg.norm(np.cross(corners[:, 1]-corners[:, 0], corners[:, 2]-corners[:, 0]), axis=1)/2
longest = lengths.max(axis=1)
altitude = 2*area/np.maximum(longest, 1e-30)
aspect = longest/np.maximum(altitude, 1e-30)
_, canonical = np.unique(local, axis=0, return_inverse=True)
canonical_faces = canonical[faces]
sorted_faces = np.sort(canonical_faces, axis=1)
_, duplicate_inverse, duplicate_count = np.unique(sorted_faces, axis=0, return_inverse=True, return_counts=True)
duplicate = duplicate_count[duplicate_inverse] > 1
edges = np.sort(np.concatenate([canonical_faces[:, [0, 1]], canonical_faces[:, [1, 2]], canonical_faces[:, [2, 0]]]), axis=1)
unique_edges, edge_first, edge_count = np.unique(edges, axis=0, return_index=True, return_counts=True)
edge_face = edge_first % len(faces)
face_component = data['component_per_vertex'][faces[:, 0]]
edge_component = face_component[edge_face]
flags = dict(zero_area=area == 0, small_area=area < cfg.minimum_area_m2,
             low_altitude=altitude < cfg.minimum_altitude_m, high_aspect=aspect > cfg.maximum_aspect_ratio,
             duplicate_position_faces=duplicate, collapsed_position_edge=np.any(np.diff(sorted_faces, axis=1)==0, axis=1))
visibility = {}
if a.visibility:
    v = json.loads(Path(a.visibility).read_text())
    assert v['mesh_hash'] == entry['mesh_hash']
    visibility = {row['component_index']: row for row in v['components']}
rows = []
for component in entry['components']:
    index = component['component_index']
    selected = face_component == index
    control = visibility.get(index, {})
    rows.append(dict(component_id=component['id'], component_index=index,
                     triangles=int(selected.sum()), flags={name: int(mask[selected].sum()) for name, mask in flags.items()},
                     minimum_area_m2=float(area[selected].min()), minimum_altitude_m=float(altitude[selected].min()),
                     maximum_aspect_ratio=float(aspect[selected].max()),
                     boundary_position_edges=int(((edge_component == index) & (edge_count == 1)).sum()),
                     nonmanifold_position_edges=int(((edge_component == index) & (edge_count > 2)).sum()),
                     visibility_candidate=control.get('candidate'),
                     visibility_control_misses=control.get('target_only_control_rays', 0)-control.get('target_only_control_hits', 0)))
out.mkdir(parents=True, exist_ok=False)
(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
(out/'profile.json').write_bytes(profile_path.read_bytes())
np.savez_compressed(out/'triangle-selections.npz', **{name: np.flatnonzero(mask).astype(np.int32) for name, mask in flags.items()},
                    boundary_position_edges=unique_edges[edge_count == 1],
                    nonmanifold_position_edges=unique_edges[edge_count > 2])
report = dict(schema_version=1, status='needs-review' if any(mask.any() for mask in flags.values()) else 'passed-scoped-triangle-checks',
              source_sha256=inventory['source_sha256'], mesh_hash=entry['mesh_hash'], object=a.object, cpu_only=True,
              inspection_sha256=hashlib.sha256(inventory_path.read_bytes()).hexdigest(),
              selection_sha256=hashlib.sha256(data_path.read_bytes()).hexdigest(),
              profile_sha256=hashlib.sha256(profile_path.read_bytes()).hexdigest(),
              triangles=len(faces), counts={name: int(mask.sum()) for name, mask in flags.items()},
              boundary_position_edges=int((edge_count == 1).sum()), nonmanifold_position_edges=int((edge_count > 2).sum()),
              components=rows, thresholds=profile['thresholds'],
              limitations=['Exact coincident positions only; no tolerance welding or topology changes.',
                           'Tiny/elongated or duplicated faces can be intentional; semantic review remains required.',
                           'Boundary/nonmanifold edges are diagnostics, not an automatic watertightness requirement for hair/tack.',
                           'No self-intersection, physical containment, UV overlap or deletion approval.'])
(out/'triangle-quality.json').write_text(json.dumps(report, indent=2))
print(json.dumps({key: report[key] for key in ['status', 'triangles', 'counts', 'boundary_position_edges', 'nonmanifold_position_edges']}))
