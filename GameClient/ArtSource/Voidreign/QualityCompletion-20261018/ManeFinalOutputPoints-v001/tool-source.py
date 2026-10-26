"""Capture final deformed feature output in its canonical proxy coordinates.

This bakes already measured geometry, not new physics. The exact source-index
mapping and coordinate round-trip are checked before writing a new point file.
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
assert 1 <= cfg['frame_start'] < cfg['frame_end'] <= 10000
for f, expected in cfg['input_sha256'].items():
    assert sha(f) == expected, f
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()))
scene, obj, proxy = bpy.context.scene, bpy.data.objects[cfg['object']], bpy.data.objects[cfg['proxy_object']]
indices = np.empty(len(obj.data.vertices), np.int32)
selected = np.empty(len(obj.data.vertices), np.bool_)
obj.data.attributes[cfg['index_attribute']].data.foreach_get('value', indices)
obj.data.attributes[cfg['selection_attribute']].data.foreach_get('value', selected)
original_ids = np.flatnonzero(selected)
unique_indices, first = np.unique(indices[original_ids], return_index=True)
assert np.array_equal(unique_indices, np.arange(len(proxy.data.vertices)))
representatives = original_ids[first]
points, source_points, max_roundtrip = [], [], 0.
for frame in range(cfg['frame_start'], cfg['frame_end'] + 1):
    scene.frame_set(frame)
    evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    assert len(evaluated.data.vertices) == len(obj.data.vertices)
    values = np.empty(len(obj.data.vertices) * 3, np.float32)
    evaluated.data.vertices.foreach_get('co', values)
    xyz = values.reshape(-1, 3)[representatives]
    relative = np.array(proxy.matrix_world.inverted() @ evaluated.matrix_world)
    converted = (xyz @ relative[:3, :3].T + relative[:3, 3]).astype(np.float32)
    inverse = np.linalg.inv(relative)
    reconstructed = converted @ inverse[:3, :3].T + inverse[:3, 3]
    world_linear = np.array(evaluated.matrix_world)[:3, :3]
    error = np.linalg.norm((reconstructed - xyz) @ world_linear.T, axis=1).max()
    max_roundtrip = max(max_roundtrip, float(error))
    points.append(converted)
    source_points.append(xyz)
assert max_roundtrip <= 1e-5
out.mkdir(parents=True)
np.savez_compressed(out / 'output-points.npz', points=np.array(points), source_points=np.array(source_points),
    source_vertex_ids=representatives, frames=np.arange(cfg['frame_start'], cfg['frame_end'] + 1))
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
(out / 'profile.json').write_bytes(profile_path.read_bytes())
for f, expected in cfg['input_sha256'].items():
    assert sha(f) == expected, f
report = dict(schema_version=1, status='captured-source-bound-feature-output', cpu_only=True,
    input_sha256=cfg['input_sha256'], tool_sha256=sha(__file__), positions_sha256=sha(out / 'output-points.npz'),
    proxy_vertices=len(representatives), frames=len(points), maximum_world_roundtrip_error_m=max_roundtrip,
    limitations=['Exact source-index output capture; not a new physical, semantic, runtime or exchange acceptance.'])
(out / 'capture-report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report), flush=True)
