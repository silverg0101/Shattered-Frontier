"""CPU evaluated-pose inventories preserving exact rest component correspondence."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np

p = argparse.ArgumentParser(description=__doc__)
for key in ['source', 'inspection', 'object', 'assembly-root', 'output']:
    p.add_argument('--'+key, required=True)
p.add_argument('--frame', type=int, action='append', required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
source, inventory_path, out = [Path(value).resolve() for value in [a.source, a.inspection, a.output]]
inventory = json.loads(inventory_path.read_text())
entry = next(row for row in inventory['objects'] if row['object'] == a.object)
data = np.load(inventory_path.parent/entry['selection_file'])
bpy.ops.wm.open_mainfile(filepath=str(source))
obj = bpy.data.objects[a.object]
scene = bpy.context.scene
obj.data.calc_loop_triangles()
rest = np.array([v.co[:] for v in obj.data.vertices], dtype=np.float32)
faces = np.array([tuple(t.vertices) for t in obj.data.loop_triangles], dtype=np.int32)
assert hashlib.sha256(rest.tobytes()+faces.tobytes()).hexdigest() == entry['mesh_hash']
assert np.array_equal(faces, data['triangles'])
assert all(scene.frame_start <= frame <= scene.frame_end for frame in a.frame)
root = bpy.data.objects[a.assembly_root]
out.mkdir(parents=True, exist_ok=False)
(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
records = []
for frame in a.frame:
    scene.frame_set(frame)
    evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    assert len(evaluated.data.vertices) == len(rest), 'Topology-changing modifiers need a reviewed correspondence adapter'
    values = np.empty(len(rest)*3, dtype=np.float32)
    evaluated.data.vertices.foreach_get('co', values)
    matrix = np.array(root.matrix_world.inverted()@obj.matrix_world)
    assert np.allclose(root.matrix_world.to_scale(), (1, 1, 1), atol=1e-6)
    points = ((values.reshape(-1, 3)@matrix[:3, :3].T+matrix[:3, 3])*scene.unit_settings.scale_length).astype(np.float32)
    folder = out/f'Frame-{frame:04}'
    folder.mkdir()
    np.savez_compressed(folder/entry['selection_file'], **{key: (points if key=='vertices' else data[key]) for key in data.files})
    revised = json.loads(json.dumps(entry))
    for component in revised['components']:
        selected = points[data['component_per_vertex'] == component['component_index']]
        component.update(bounds_m=[selected.min(0).tolist(), selected.max(0).tolist()],
                         centroid_m=selected.mean(0).tolist(), dimensions_m=np.ptp(selected, axis=0).tolist())
        component['rest_sampled_main_surface_gap_m'] = component.pop('sampled_main_surface_gap_m', None)
        component['distance_note'] = 'Rest gap is historical; no posed gap recomputation in this export.'
    report = dict(schema_version=1, status='evaluated-pose-inspection; rest topology correspondence preserved',
                  source=str(source), source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                  rest_inspection_sha256=hashlib.sha256(inventory_path.read_bytes()).hexdigest(),
                  frame=frame, scene_unit_scale_length=scene.unit_settings.scale_length,
                  pose='Evaluated POSE in rigid assembly frame, metre-scaled; mesh hash identifies rest topology.',
                  objects=[revised], limitations=['Same rest vertex/face correspondence required; no topology-changing modifiers.',
                                                 'Only specified pose; semantic/deletion decisions remain open.'])
    (folder/'component-report.json').write_text(json.dumps(report, indent=2))
    records.append(dict(frame=frame, inspection=str(folder/'component-report.json')))
(out/'frames.json').write_text(json.dumps(records, indent=2))
print(json.dumps(dict(frames=a.frame, preserved_component_correspondence=True)))
