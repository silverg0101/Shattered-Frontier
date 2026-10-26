"""Freeze measured canonical-feature positions into a verified MDD playback.

Does not rerun physics or change source mesh topology/weights. Every output
frame is reopened and compared with the frozen measured local positions.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import sys
import bpy
import numpy as np


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile', required=True)
p.add_argument('--output', required=True)
a = p.parse_args(sys.argv[sys.argv.index('--') + 1:])
profile_path, out = Path(a.profile).resolve(), Path(a.output).resolve()
cfg = json.loads(profile_path.read_text())
assert cfg['schema_version'] == 1 and not out.exists()
for f, expected in cfg['input_sha256'].items():
    assert sha(f) == expected, f
with np.load(cfg['positions_file'], allow_pickle=False) as d:
    points = d['points']
assert points.dtype == np.float32 and points.ndim == 3 and points.shape[2] == 3
assert np.isfinite(points).all() and 2 <= len(points) <= 10000
assert cfg['frame_start'] == 1 and cfg['frame_end'] == len(points)
out.mkdir(parents=True)
(out / 'profile.json').write_bytes(profile_path.read_bytes())
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
mdd = out / 'Feature-actual-motion.mdd'
with mdd.open('wb') as f:
    f.write(struct.pack('>2i', points.shape[0], points.shape[1]))
    f.write((np.arange(len(points)) / cfg['fps']).astype('>f4').tobytes())
    for frame in points:
        f.write(frame.astype('>f4').tobytes())
for cache in cfg.get('cache_directories', []):
    folder = Path(cache['path'])
    target = out / 'blendcache_Voidreign-ActualSurfacePilot'
    target.mkdir(exist_ok=True)
    for name, expected in cache['files'].items():
        assert Path(name).name == name and sha(folder / name) == expected
        assert not (target / name).exists()
        shutil.copy2(folder / name, target / name)
        assert sha(target / name) == expected
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()))
scene = bpy.context.scene
proxy = bpy.data.objects[cfg['proxy_object']]
for item in cfg.get('disable_output_modifiers', []):
    target = bpy.data.objects[item['object']].modifiers[item['modifier']]
    assert target.type == 'NODES'
    target.show_viewport = target.show_render = False
assert len(proxy.data.vertices) == points.shape[1]
for modifier in proxy.modifiers:
    modifier.show_viewport = modifier.show_render = False
modifier = proxy.modifiers.new('Verified measured surface motion | MDD', 'MESH_CACHE')
modifier.cache_format, modifier.deform_mode = 'MDD', 'OVERWRITE'
modifier.time_mode, modifier.play_mode = 'FRAME', 'SCENE'
modifier.frame_start = cfg['frame_start']
modifier.frame_scale = 1.
modifier.forward_axis, modifier.up_axis = 'POS_Y', 'POS_Z'
modifier.interpolation = 'LINEAR'
modifier.filepath = str(mdd)
scene.frame_start, scene.frame_end = cfg['frame_start'], cfg['frame_end']
scene.frame_set(1)
candidate = out / 'Voidreign-ActualSurfacePilot.blend'
bpy.ops.wm.save_as_mainfile(filepath=str(candidate))
bpy.ops.wm.open_mainfile(filepath=str(candidate))
scene, proxy = bpy.context.scene, bpy.data.objects[cfg['proxy_object']]
maximum = 0.
errors = []
for index, expected in enumerate(points):
    scene.frame_set(index + 1)
    actual = proxy.evaluated_get(bpy.context.evaluated_depsgraph_get())
    values = np.empty(points.shape[1] * 3, np.float32)
    actual.data.vertices.foreach_get('co', values)
    error = float(np.linalg.norm(values.reshape(-1, 3) - expected, axis=1).max())
    errors.append(error)
    maximum = max(maximum, error)
report = dict(schema_version=1, status='passed-feature-cache-reopen-only' if maximum <= cfg['maximum_reopen_error_m'] else 'failed-feature-cache-reopen',
    cpu_only=True, input_sha256=cfg['input_sha256'], tool_sha256=sha(__file__), profile_sha256=sha(profile_path),
    candidate_sha256=sha(candidate), motion_cache_sha256=sha(mdd), frames=len(points), vertices=points.shape[1],
    measured_maximum_error_m=maximum, maximum_reopen_error_m=cfg['maximum_reopen_error_m'], per_frame_error_m=errors,
    limitations=['Measured local-point playback only; original simulation validity, contacts, style and attachment checks remain separate.',
        'MDD is an offline Blender dependency, not a skeletal engine exchange. Original source mesh/UV/weights are retained.'])
(out / 'cache-report.json').write_text(json.dumps(report, indent=2))
for f, expected in cfg['input_sha256'].items():
    assert sha(f) == expected, f
print(json.dumps({k: report[k] for k in ['status', 'measured_maximum_error_m']}), flush=True)
assert maximum <= cfg['maximum_reopen_error_m']
