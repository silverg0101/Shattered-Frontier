"""Measure moving rigid attachment frames from explicit source-domain boundaries.

Fresh CPU Blender capture only; no source/rig/weight changes. Kabsch residuals
report whether a rigid moving-root approximation fits the existing skin.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile', required=True)
p.add_argument('--output', required=True)
a = p.parse_args(sys.argv[sys.argv.index('--') + 1:])
profile_path, out = Path(a.profile).resolve(), Path(a.output).resolve()
cfg = json.loads(profile_path.read_text())
assert cfg['schema_version'] == 1 and not out.exists()
assert type(cfg['frame_start']) is int and type(cfg['frame_end']) is int
assert 1 <= cfg['frame_start'] < cfg['frame_end'] <= 10000
assert type(cfg['maximum_rigid_fit_residual_m']) is float and 0 < cfg['maximum_rigid_fit_residual_m'] <= .02
assert 1 <= len(cfg['domains']) <= 4
for f, expected in cfg['input_sha256'].items():
    assert sha(f) == expected, f
source, domain_path = Path(cfg['source']), Path(cfg['domain_archive'])
selection_archive = Path(cfg['selection_archive'])
with np.load(selection_archive, allow_pickle=False) as selected:
    canonical = selected['canonical_node']
domain_report = json.loads(Path(cfg['domain_report']).read_text())
assert domain_report['mesh_hash'] == cfg['mesh_hash']
root_band = cfg['root_boundary_band_m']
assert type(root_band) is float and .005 <= root_band <= .1
bpy.ops.wm.open_mainfile(filepath=str(source.resolve()))
scene, obj = bpy.context.scene, bpy.data.objects[cfg['object']]
assert obj.type == 'MESH'
obj.data.calc_loop_triangles()
local = np.array([v.co[:] for v in obj.data.vertices], np.float32)
faces = np.array([v.vertices[:] for v in obj.data.loop_triangles], np.int32)
assert hashlib.sha256(local.tobytes() + faces.tobytes()).hexdigest() == cfg['mesh_hash']
assert canonical.shape == (len(local),)
assert all(mod.type == 'ARMATURE' and not mod.use_deform_preserve_volume for mod in obj.modifiers)
arms = [mod.object for mod in obj.modifiers]
assert arms
for arm in arms:
    arm.data.pose_position = 'REST'
scene.frame_set(1)
bpy.context.view_layer.update()
assembly = bpy.data.objects[cfg['assembly_root']]
relative = np.array(assembly.matrix_world.inverted() @ obj.matrix_world)
rest = local.astype(np.float64) @ relative[:3, :3].T + relative[:3, 3]
domain_data = np.load(domain_path, allow_pickle=False)
root_ids, descriptions = [], []
for prefix in cfg['domains']:
    face_ids = domain_data[prefix + '_face_ids']
    assert face_ids.dtype.kind in 'iu' and len(face_ids) and face_ids.min() >= 0 and face_ids.max() < len(faces)
    selected = canonical[faces[face_ids]]
    # Boundary edges of the explicit selected surface, not whole-component IDs.
    edges = np.sort(np.concatenate([selected[:, [0, 1]], selected[:, [1, 2]], selected[:, [2, 0]]]), axis=1)
    unique, counts = np.unique(edges, axis=0, return_counts=True)
    boundary_nodes = np.unique(unique[counts == 1])
    _, representatives = np.unique(canonical, return_index=True)
    boundary = representatives[boundary_nodes]
    assert len(boundary) >= 3
    description = next(v for v in domain_report['domains'] if v['name'] == prefix)
    plane = description['cut_plane']
    origin, normal = np.array(plane['origin_m']), np.array(plane['normal'])
    boundary_before_band = len(boundary)
    boundary = boundary[np.abs((rest[boundary] - origin) @ normal) <= root_band]
    assert len(boundary) >= 3, 'No geometric boundary ring near the authored cut plane'
    # Seam duplicates must be treated geometrically in analysis. Keep original
    # source IDs with one representative at each exact rest position.
    _, representative = np.unique(rest[boundary].astype(np.float32), axis=0, return_index=True)
    boundary = boundary[representative]
    centered = rest[boundary] - rest[boundary].mean(axis=0)
    singular = np.linalg.svd(centered, compute_uv=False)
    assert singular[1] > 1e-8, 'Attachment points do not determine an orientation'
    root_ids.append(boundary)
    descriptions.append(dict(name=prefix, vertices=boundary.tolist(), rest_center_m=rest[boundary].mean(axis=0).tolist(),
        rest_singular_values_m=singular.tolist(), geometric_boundary_vertices=boundary_before_band,
        omitted_boundary_vertices=boundary_before_band-len(boundary), root_boundary_band_m=root_band,
        method='Canonical seam-aware triangle boundary restricted to the authored cut-plane band'))
for arm in arms:
    arm.data.pose_position = 'POSE'
transforms, maxima, rms_values = [], [], []
for frame in range(cfg['frame_start'], cfg['frame_end'] + 1):
    scene.frame_set(frame)
    evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    assert len(evaluated.data.vertices) == len(local)
    matrix = np.array(evaluated.matrix_world)
    frame_matrices, frame_max, frame_rms = [], [], []
    for ids in root_ids:
        x = rest[ids]
        y = np.array([evaluated.data.vertices[int(i)].co[:] for i in ids]) @ matrix[:3, :3].T + matrix[:3, 3]
        xc, yc = x.mean(axis=0), y.mean(axis=0)
        u, _, vt = np.linalg.svd((x - xc).T @ (y - yc))
        correction = np.eye(3)
        correction[2, 2] = np.linalg.det(vt.T @ u.T)
        rot = vt.T @ correction @ u.T
        translation = yc - rot @ xc
        error = np.linalg.norm(x @ rot.T + translation - y, axis=1)
        result = np.eye(4)
        result[:3, :3], result[:3, 3] = rot, translation
        frame_matrices.append(result)
        frame_max.append(float(error.max()))
        frame_rms.append(float(np.sqrt(np.mean(error * error))))
    transforms.append(frame_matrices)
    maxima.append(frame_max)
    rms_values.append(frame_rms)
maxima, rms_values = np.array(maxima), np.array(rms_values)
for f, expected in cfg['input_sha256'].items():
    assert sha(f) == expected, f
out.mkdir(parents=True)
np.savez_compressed(out / 'attachment-motion.npz', transforms=np.array(transforms), maximum_residual_m=maxima,
    rms_residual_m=rms_values, frames=np.arange(cfg['frame_start'], cfg['frame_end'] + 1))
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
(out / 'profile.json').write_bytes(profile_path.read_bytes())
report = dict(schema_version=1, status='passed-scoped-rigid-fit-only' if maxima.max() <= cfg['maximum_rigid_fit_residual_m'] else 'needs-review-nonrigid-attachment',
    cpu_only=True, input_sha256=cfg['input_sha256'], tool_sha256=sha(__file__),
    source_sha256=sha(source), mesh_hash=cfg['mesh_hash'], domains=descriptions,
    maximum_rigid_fit_residual_m=cfg['maximum_rigid_fit_residual_m'],
    measured_maximum_residual_m=maxima.max(axis=0).tolist(),
    worst_frames=(maxima.argmax(axis=0) + cfg['frame_start']).tolist(),
    motion_sha256=sha(out / 'attachment-motion.npz'),
    limitations=['Unreviewed domain boundaries may include seams, distal caps, holes or branches rather than an anatomical root ring.',
        'A rigid fit does not accept root ownership, local-guide shape, handoff weights, physical response or runtime playback.',
        'Fixed source rest geometry and source animation only; no source, bone, weight, geometry or collision changes.'])
(out / 'attachment-report.json').write_text(json.dumps(report, indent=2))
print(json.dumps({k: report[k] for k in ['status', 'measured_maximum_residual_m', 'worst_frames']}), flush=True)
