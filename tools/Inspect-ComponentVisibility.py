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
assert profile['schema_version'] == 2
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
for x in [-1, 1]:
    for y in [-1, 1]:
        for z in [-1, 1]:
            directions[f'diagonal_{x}_{y}_{z}'] = Vector((x, y, z)).normalized()
face_components = components[faces[:, 0]]
rows = []
component_indices = profile.get('component_indices')
if component_indices is not None:
    assert isinstance(component_indices, list) and component_indices
    assert all(type(index) is int for index in component_indices)
    assert len(set(component_indices)) == len(component_indices)
    assert set(component_indices) <= {row['component_index'] for row in entry['components']}
for component in entry['components']:
    if component_indices is not None and component['component_index'] not in component_indices:
        continue
    target_faces = np.flatnonzero(face_components == component['component_index'])
    target_triangles = faces[target_faces]
    corners = points[target_triangles]
    areas = np.linalg.norm(np.cross(corners[:, 1]-corners[:, 0], corners[:, 2]-corners[:, 0]), axis=1)/2
    assert areas.sum() > 0, 'Degenerate component needs separate review'
    count = profile['samples_per_component']
    indices = np.minimum(np.searchsorted(np.cumsum(areas)/areas.sum(), (np.arange(count)+.5)/count), len(areas)-1)
    chosen = corners[indices]
    # Area-stratified face sampling and deterministic interior barycentrics.
    u = np.sqrt((np.arange(count)*.6180339887498949+.25) % 1)
    v = (np.arange(count)*.4142135623730951+.5) % 1
    sample = (1-u[:, None])*chosen[:, 0]+(u*(1-v))[:, None]*chosen[:, 1]+(u*v)[:, None]*chosen[:, 2]
    unique, inverse = np.unique(target_triangles, return_inverse=True)
    isolated = BVHTree.FromPolygons([Vector(point) for point in points[unique]],
        [tuple(int(v) for v in face) for face in inverse.reshape(-1, 3)], all_triangles=True)
    fractions, visible_nodes, control_hits, near_cover = {}, set(), 0, 0
    covering_components = {}
    for name, direction in directions.items():
        visible = 0
        for index, location in enumerate(sample):
            point = Vector(location)
            origin = point+direction*distance
            control, _, _, _ = isolated.ray_cast(origin, -direction, distance*2)
            control_hits += int(control is not None)
            hit, _, face_index, _ = bvh.ray_cast(origin, -direction, distance*2)
            if hit is not None and face_components[face_index] == component['component_index']:
                visible += 1
                visible_nodes.add(index)
            elif hit is not None:
                covering = int(face_components[face_index])
                covering_components[covering] = covering_components.get(covering, 0)+1
                near_cover += int((hit-point).length <= profile['hit_tolerance_m'])
        fractions[name] = visible/len(sample)
    peak = max(fractions.values())
    rows.append(dict(component_id=component['id'], component_index=component['component_index'],
                     area_stratified_interior_samples=len(sample), visible_samples_any_direction=len(visible_nodes),
                     target_only_control_hits=control_hits, target_only_control_rays=len(sample)*len(directions),
                     nearby_covering_shell_hits=near_cover, first_covering_components=covering_components,
                     visibility_by_direction=fractions, maximum_sample_visibility=peak,
                     candidate=('unresolved-control-misses' if control_hits < len(sample)*len(directions) else
                                'low-rest-visibility-review' if peak <= profile['low_visibility_fraction'] else 'visible-part-review'),
                     disposition='needs-review'))
out.mkdir(parents=True, exist_ok=False)
(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
(out/'profile.json').write_bytes(profile_path.read_bytes())
pose_label = 'evaluated-pose' if 'frame' in inventory else 'rest'
report = dict(schema_version=2, status='sampled-'+pose_label+'-visibility-only', cpu_only=True,
              component_scope='explicit-subset' if component_indices is not None else 'all-inventory-components',
              tested_component_indices=[row['component_index'] for row in rows],
              frame=inventory.get('frame'), pose=inventory.get('pose', 'REST'),
              source_sha256=inventory['source_sha256'], mesh_hash=entry['mesh_hash'], object=a.object,
              inspection_sha256=hashlib.sha256(inventory_path.read_bytes()).hexdigest(),
              selection_sha256=hashlib.sha256(data_path.read_bytes()).hexdigest(),
              profile_sha256=hashlib.sha256(profile_path.read_bytes()).hexdigest(),
              components=rows, low_visibility_ids=[row['component_id'] for row in rows if row['candidate']=='low-rest-visibility-review'],
              limitations=['Fourteen directions with area-stratified triangle-interior samples; not complete visibility.',
                           'No visibility does not prove debris: internal anatomy, occluded ornaments or moving clumps may be intended.',
                           'Only the specified pose; review other poses and actual shading before disposition.',
                           'The low-rest-visibility-review candidate label is retained for schema compatibility; frame/pose fields define the actual tested pose.',
                           'No geometry changes or semantic approval.'])
(out/'visibility-report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(dict(components=len(rows), low_visibility=len(report['low_visibility_ids']))))
