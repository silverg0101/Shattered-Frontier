"""CPU rest-component visibility samples; identification aid, never deletion approval."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

p = argparse.ArgumentParser(description=__doc__)
for key in ['inspection', 'object', 'profile', 'output']:
    p.add_argument('--'+key, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
inventory_path, profile_path, out = [Path(value).resolve() for value in [a.inspection, a.profile, a.output]]
inventory, profile = json.loads(inventory_path.read_text()), json.loads(profile_path.read_text())
assert profile['schema_version'] == 1
assert type(profile['samples_per_component']) is int and 16 <= profile['samples_per_component'] <= 1024
assert type(profile['hit_tolerance_m']) is float and 1e-5 <= profile['hit_tolerance_m'] <= .01
assert type(profile['low_visibility_fraction']) is float and 0 <= profile['low_visibility_fraction'] <= .1
assert hashlib.sha256(Path(inventory['source']).read_bytes()).hexdigest() == inventory['source_sha256']
entry = next(row for row in inventory['objects'] if row['object'] == a.object)
data_path = inventory_path.parent/entry['selection_file']
data = np.load(data_path)
points, faces, components = data['vertices'], data['triangles'], data['component_per_vertex']
distance = float(np.linalg.norm(np.ptp(points, axis=0))*3)
bvh = BVHTree.FromPolygons([Vector(point) for point in points], [tuple(int(v) for v in face) for face in faces], all_triangles=True)
directions = dict(posX=Vector((1, 0, 0)), negX=Vector((-1, 0, 0)),
                  front=Vector((0, -1, 0)), rear=Vector((0, 1, 0)),
                  top=Vector((0, 0, 1)), bottom=Vector((0, 0, -1)))
rows = []
for component in entry['components']:
    indices = np.flatnonzero(components == component['component_index'])
    sample = indices[np.unique(np.linspace(0, len(indices)-1, min(len(indices), profile['samples_per_component']), dtype=int))]
    fractions, visible_nodes = {}, set()
    for name, direction in directions.items():
        visible = 0
        for vertex in sample:
            point = Vector(points[vertex])
            hit, _, _, _ = bvh.ray_cast(point+direction*distance, -direction, distance*2)
            if hit is not None and (hit-point).length <= profile['hit_tolerance_m']:
                visible += 1
                visible_nodes.add(int(vertex))
        fractions[name] = visible/len(sample)
    peak = max(fractions.values())
    rows.append(dict(component_id=component['id'], component_index=component['component_index'],
                     sampled_vertices=len(sample), sampled_visible_vertices=len(visible_nodes),
                     visibility_by_direction=fractions, maximum_sample_visibility=peak,
                     candidate='low-rest-visibility-review' if peak <= profile['low_visibility_fraction'] else 'visible-part-review',
                     disposition='needs-review'))
out.mkdir(parents=True, exist_ok=False)
(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
(out/'profile.json').write_bytes(profile_path.read_bytes())
report = dict(schema_version=1, status='sampled-rest-visibility-only', cpu_only=True,
              source_sha256=inventory['source_sha256'], mesh_hash=entry['mesh_hash'], object=a.object,
              inspection_sha256=hashlib.sha256(inventory_path.read_bytes()).hexdigest(),
              selection_sha256=hashlib.sha256(data_path.read_bytes()).hexdigest(),
              profile_sha256=hashlib.sha256(profile_path.read_bytes()).hexdigest(),
              components=rows, low_visibility_ids=[row['component_id'] for row in rows if row['candidate']=='low-rest-visibility-review'],
              limitations=['Six orthographic directions and indexed-vertex samples, not area-complete visibility.',
                           'No visibility does not prove debris: internal anatomy, occluded ornaments or moving clumps may be intended.',
                           'Rest pose only; review other poses and actual shading before disposition.','No geometry changes or semantic approval.'])
(out/'visibility-report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(dict(components=len(rows), low_visibility=len(report['low_visibility_ids']))))
