"""Read-only CPU diagnosis of failed actual-mesh hair steps and contacts.

Records exact offending vertices, weights and bone lever arms before a reviewed
skinning/contact redesign. It never bakes another repair or changes limits.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np

p = argparse.ArgumentParser(description=__doc__)
for key in ['source', 'guide-report', 'surface-report', 'output']:
    p.add_argument('--'+key, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
source, guide_path, surface_path, out = [Path(value).resolve() for value in [a.source, a.guide_report, a.surface_report, a.output]]
surface, guide = json.loads(surface_path.read_text()), json.loads(guide_path.read_text())
assert hashlib.sha256(source.read_bytes()).hexdigest() == surface['source_sha256']
out.mkdir(parents=True, exist_ok=False)
(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
bpy.ops.wm.open_mainfile(filepath=str(source))
scene = bpy.context.scene
obj = bpy.data.objects[guide['object']]
arm = next(modifier.object for modifier in obj.modifiers if modifier.type == 'ARMATURE')
root = arm.parent
field = np.load(guide_path.parent/guide['weight_field_file'])
groups = {group.index: group.name for group in obj.vertex_groups}

def body_points(frame):
    scene.frame_set(frame)
    evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    points = np.empty(len(obj.data.vertices)*3, dtype=np.float32)
    evaluated.data.vertices.foreach_get('co', points)
    matrix = np.array(root.matrix_world.inverted()@obj.matrix_world)
    return points.reshape(-1, 3)@matrix[:3, :3].T+matrix[:3, 3]

def describe(vertex, frame, reason):
    scene.frame_set(1)
    point = np.array(root.matrix_world.inverted()@obj.matrix_world@obj.data.vertices[vertex].co)
    rows = []
    for weight in obj.data.vertices[vertex].groups:
        name = groups[weight.group]
        bone = arm.data.bones[name]
        start, end = np.array(bone.head_local), np.array(bone.tail_local)
        direction = end-start
        u = float(np.clip((point-start)@direction/(direction@direction), 0, 1))
        lever = float(np.linalg.norm(point-(start+u*direction)))
        scene.frame_set(max(1, frame-1))
        before = arm.pose.bones[name].matrix.to_quaternion()
        scene.frame_set(frame)
        rotation = before.rotation_difference(arm.pose.bones[name].matrix.to_quaternion()).angle
        rows.append(dict(bone=name, weight=weight.weight, rest_distance_to_segment_m=lever,
                         angular_step_degrees=float(np.degrees(min(rotation, 2*np.pi-rotation)))))
    return dict(reason=reason, vertex=int(vertex), frame=int(frame), rest_position_m=point.tolist(),
                semantic_label=int(field['labels'][vertex]), motion_protected=bool(field['motion_protected'][vertex]),
                root_distance_m=float(field['root_distance_m'][vertex]), influences=rows)

frame = surface['max_surface_step_frame']
before, after = body_points(frame-1), body_points(frame)
step = np.linalg.norm(after-before, axis=1)
largest = np.argsort(step)[-8:][::-1]
records = [dict(**describe(int(vertex), frame, 'maximum-surface-step'), actual_step_m=float(step[vertex])) for vertex in largest]
for label, collision in surface['collision_proxies'].items():
    if collision['vertex'] is not None:
        records.append(dict(**describe(collision['vertex'], collision['frame'], label+' penetration'),
                            penetration_m=collision['maximum_penetration_m']))
report = dict(status='diagnosis-only; failed checks remain unchanged', cpu_only=True,
              source_sha256=surface['source_sha256'], surface_report_sha256=hashlib.sha256(surface_path.read_bytes()).hexdigest(),
              records=records,
              next_design_review=['Review strand/clump topology when assigning guide influence; Euclidean proximity may cross separate locks.',
                                  'Use actual surface lever arms and contact envelopes; guide-center contacts do not bound cross sections.',
                                  'Review short-segment angular velocities and surface offsets before increasing solver resolution.',
                                  'Retain existing protected/seam/gap checks; no automatic fourth rig retry.'])
(out/'diagnosis.json').write_text(json.dumps(report, indent=2))
print(json.dumps(dict(records=len(records), worst_vertex=int(largest[0]), step_m=float(step[largest[0]]))))
