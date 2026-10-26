"""CPU saved-source skin, seam, continuity, attachment-gap and surface checks.

Reports real evaluated mesh vertices against declared capsule proxies. It does
not replace triangle collision, semantic attachment review, gait or engine tests.
"""
import argparse
from dataclasses import dataclass
import hashlib
import importlib.util
import json
import sys
from pathlib import Path
import bpy
import numpy as np

p = argparse.ArgumentParser(description=__doc__)
for key in ['source', 'guide-report', 'config', 'profile', 'output']:
    p.add_argument('--'+key, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
source, guide_path, config_path, profile_path, out = map(lambda value: Path(value).resolve(),
    [a.source, a.guide_report, a.config, a.profile, a.output])
profile = json.loads(profile_path.read_text())
raw = json.loads(config_path.read_text())
guide_report = json.loads(guide_path.read_text())
assert hashlib.sha256(config_path.read_bytes()).hexdigest() == guide_report['motion_profile_sha256']
field = np.load(guide_path.parent/guide_report['weight_field_file'])
selection = json.loads(Path(guide_report['selection_path']).read_text())
feature_profile = json.loads(Path(guide_report['profile_path']).read_text())
attachments = json.loads(Path(guide_report['attachment_path']).read_text())
assert hashlib.sha256(Path(guide_report['attachment_path']).read_bytes()).hexdigest() == guide_report['attachment_sha256']


@dataclass(frozen=True)
class Limits:
    frame_count: int
    sample_frames: list
    protected_tolerance_m: float
    seam_tolerance_m: float
    maximum_initial_step_m: float
    maximum_surface_step_m: float
    attachment_gap_increase_m: float
    collision_margin_m: float
    maximum_penetration_m: float
    minimum_dynamic_weight: float
    root_exemption_extra_m: float


assert profile['schema_version'] == 1
limits = Limits(**profile['limits'])
assert type(limits.frame_count) is int and 2 <= limits.frame_count <= 10000
assert all(type(f) is int and 1 <= f <= limits.frame_count for f in limits.sample_frames)
for key in ['protected_tolerance_m', 'seam_tolerance_m', 'maximum_initial_step_m',
            'maximum_surface_step_m', 'attachment_gap_increase_m', 'collision_margin_m',
            'maximum_penetration_m', 'root_exemption_extra_m']:
    assert type(getattr(limits, key)) is float and 0 <= getattr(limits, key) <= .2
assert 0 < limits.minimum_dynamic_weight < 1
spec = importlib.util.spec_from_file_location('sf_feature_physics', Path(__file__).with_name('Simulate-FeatureGuides.py'))
physics = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = physics
spec.loader.exec_module(physics)
settings = physics.load_settings(raw)
out.mkdir(parents=True, exist_ok=False)
bpy.ops.wm.open_mainfile(filepath=str(source))
scene = bpy.context.scene
obj = bpy.data.objects[guide_report['object']]
root = bpy.data.objects[profile['assembly_root']]
local = np.array([v.co[:] for v in obj.data.vertices], dtype=np.float32)
obj.data.calc_loop_triangles()
triangles = np.array([tuple(t.vertices) for t in obj.data.loop_triangles], dtype=np.int32)
assert hashlib.sha256(local.tobytes()+triangles.tobytes()).hexdigest() == guide_report['mesh_hash']
groups = {g.index: g.name for g in obj.vertex_groups}
weights = np.array([sum(g.weight for g in v.groups if groups[g.group] != 'StaticBody') for v in obj.data.vertices])
protected = field['motion_protected'] | (field['labels'] == 0)
assert not np.any(weights[protected] > 1e-7)
assert max(len(v.groups) for v in obj.data.vertices) <= raw['skin']['maximum_bone_influences']
_, first, inverse = np.unique(local, axis=0, return_index=True, return_inverse=True)
representative = np.unique(field['canonical_node'], return_index=True)[1]
binding_pairs = np.array([[representative[r['parent_node']], representative[r['child_node']]]
                          for r in attachments['proposals']], dtype=np.int32)
root_exempt = np.zeros(len(local), dtype=bool)
for label, feature in enumerate(feature_profile['features'], 1):
    root_exempt |= (field['labels'] == label) & (field['root_distance_m'] <=
        feature['root_feather_distance_m']+limits.root_exemption_extra_m)
free = (weights >= limits.minimum_dynamic_weight) & ~protected & ~root_exempt
scene.frame_set(1)
assembly = np.array(root.matrix_world.inverted()@obj.matrix_world)
rest_body = local@assembly[:3, :3].T+assembly[:3, 3]
rest_gap = np.linalg.norm(rest_body[binding_pairs[:, 0]]-rest_body[binding_pairs[:, 1]], axis=1)
protected_error = seam_error = max_step = initial_step = gap_increase = 0.
max_step_frame = gap_frame = None
gap_id = None
previous = None
collision_rows = {c['label']: dict(maximum_penetration_m=0., frame=None, vertex=None) for c in raw['colliders']}
collision_rows['Ground'] = dict(maximum_penetration_m=0., frame=None, vertex=None)
stored_frames, stored_penetration = [], []
for frame in range(1, limits.frame_count+1):
    scene.frame_set(frame)
    evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    xyz = np.empty(len(local)*3, dtype=np.float32)
    evaluated.data.vertices.foreach_get('co', xyz)
    xyz = xyz.reshape(-1, 3)
    body = xyz@assembly[:3, :3].T+assembly[:3, 3]
    protected_error = max(protected_error, float(np.linalg.norm(body[protected]-rest_body[protected], axis=1).max()))
    seam_error = max(seam_error, float(np.linalg.norm(body-body[first][inverse], axis=1).max()))
    if previous is not None:
        step = float(np.linalg.norm(body-previous, axis=1).max())
        if frame == 2:
            initial_step = step
        if step > max_step:
            max_step, max_step_frame = step, frame
    previous = body
    gaps = np.linalg.norm(body[binding_pairs[:, 0]]-body[binding_pairs[:, 1]], axis=1)-rest_gap
    peak = int(np.argmax(gaps))
    if gaps[peak] > gap_increase:
        gap_increase, gap_frame, gap_id = float(gaps[peak]), frame, attachments['proposals'][peak]['id']
    if frame in limits.sample_frames:
        matrix = np.array(obj.matrix_world)
        world = xyz@matrix[:3, :3].T+matrix[:3, 3]
        penetration = np.zeros(len(world), dtype=np.float32)
        for label, start, end, radius in physics.world_colliders((frame-1)/settings.fps, settings, raw):
            vector = end-start
            u = np.clip(((world-start)@vector)/(vector@vector), 0, 1)
            closest = start+u[:, None]*vector
            overlap = np.maximum(0., radius+limits.collision_margin_m-np.linalg.norm(world-closest, axis=1))
            overlap[~free] = 0
            peak_vertex = int(np.argmax(overlap))
            if overlap[peak_vertex] > collision_rows[label]['maximum_penetration_m']:
                collision_rows[label] = dict(maximum_penetration_m=float(overlap[peak_vertex]),
                    frame=frame, vertex=peak_vertex, position_m=world[peak_vertex].tolist())
            penetration = np.maximum(penetration, overlap.astype(np.float32))
        overlap = np.maximum(0., settings.ground_height_m+limits.collision_margin_m-world[:, 2])
        overlap[~free] = 0
        peak_vertex = int(np.argmax(overlap))
        if overlap[peak_vertex] > collision_rows['Ground']['maximum_penetration_m']:
            collision_rows['Ground'] = dict(maximum_penetration_m=float(overlap[peak_vertex]), frame=frame,
                vertex=peak_vertex, position_m=world[peak_vertex].tolist())
        penetration = np.maximum(penetration, overlap.astype(np.float32))
        stored_frames.append(frame)
        stored_penetration.append(penetration)
    if frame % 60 == 0:
        print('Checked evaluated surfaces', frame, '/', limits.frame_count, flush=True)
measurements = [
    ('skin.protected_rigidity', protected_error, limits.protected_tolerance_m),
    ('skin.coincident_seams', seam_error, limits.seam_tolerance_m),
    ('motion.initial_surface_step', initial_step, limits.maximum_initial_step_m),
    ('motion.maximum_surface_step', max_step, limits.maximum_surface_step_m),
    ('attachments.proposed_gap_increase', gap_increase, limits.attachment_gap_increase_m),
    ('motion.surface_capsule_clearance', max(r['maximum_penetration_m'] for r in collision_rows.values()), limits.maximum_penetration_m)]
checks = [dict(id=name, status='passed' if value <= limit else 'failed', measured=value,
               limit=limit, units='m') for name, value, limit in measurements]
np.savez_compressed(out/'surface-review.npz', frames=np.array(stored_frames),
                    penetration_m=np.array(stored_penetration), free_surface_mask=free)
report = dict(status='passed-scoped-surface-checks' if all(r['status'] == 'passed' for r in checks) else 'needs-review',
    source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
    profile_sha256=hashlib.sha256(profile_path.read_bytes()).hexdigest(),
    config_sha256=hashlib.sha256(config_path.read_bytes()).hexdigest(),
    tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), cpu_only=True,
    checks=checks, frame_count=limits.frame_count, collision_frames=stored_frames,
    max_surface_step_frame=max_step_frame, max_gap_increase_frame=gap_frame, max_gap_binding=gap_id,
    protected_vertices=int(protected.sum()), tested_free_surface_vertices=int(free.sum()),
    root_exempt_vertices=int(root_exempt.sum()), collision_proxies=collision_rows,
    limitations=['Capsule proxies, not triangle-accurate or continuous swept collision.',
                 'Root/attachment band intentionally exempt; review masks and boundaries visually.',
                 'Proposed clump links still need semantic review even if gap metric passes.',
                 'Rigid horse harness, not gait or engine runtime acceptance.'])
(out/'surface-report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report), flush=True)
