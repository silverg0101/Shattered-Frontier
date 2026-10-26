"""CPU offline geometry-cache exchange with indexed all-frame reimport checks.

Exports one evaluated mesh. This does not preserve a skeletal runtime rig or
PBR materials and does not authorize engine import or production promotion.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(8 * 1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def world_points(obj):
    evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    values = np.empty(len(evaluated.data.vertices) * 3, np.float32)
    evaluated.data.vertices.foreach_get('co', values)
    matrix = np.array(evaluated.matrix_world, np.float64)
    return (values.reshape(-1, 3) @ matrix[:3, :3].T + matrix[:3, 3]).astype(np.float32)


p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile', required=True)
p.add_argument('--output', required=True)
a = p.parse_args(sys.argv[sys.argv.index('--') + 1:])
profile, out = Path(a.profile).resolve(), Path(a.output).resolve()
cfg = json.loads(profile.read_text())
assert cfg['schema_version'] == 1 and not out.exists()
assert cfg['frame_start'] == 1 and 2 <= cfg['frame_end'] <= 2000
assert 1 <= cfg['fps'] <= 120 and 0 < cfg['maximum_error_m'] <= 1e-4
for path, expected in cfg['input_sha256'].items():
    assert sha(path) == expected, path
out.mkdir(parents=True)
(out / 'profile.json').write_bytes(profile.read_bytes())
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()))
scene, obj = bpy.context.scene, bpy.data.objects[cfg['object']]
assert obj.type == 'MESH' and scene.render.fps == cfg['fps']
scene.frame_set(1)
vertex_count = len(world_points(obj))
reference = np.lib.format.open_memmap(out / 'expected-world-points.npy', mode='w+',
    dtype=np.float32, shape=(cfg['frame_end'], vertex_count, 3))
for frame in range(1, cfg['frame_end'] + 1):
    scene.frame_set(frame)
    points = world_points(obj)
    assert points.shape == (vertex_count, 3) and np.isfinite(points).all()
    reference[frame - 1] = points
reference.flush()
bpy.ops.object.select_all(action='DESELECT')
obj.select_set(True)
bpy.context.view_layer.objects.active = obj
exchange = out / 'FeatureMotion.abc'
bpy.ops.wm.alembic_export(filepath=str(exchange), start=1, end=cfg['frame_end'],
    selected=True, flatten=True, uvs=True, normals=True, vcolors=True, face_sets=True,
    global_scale=1., as_background_job=False)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.fps = cfg['fps']
scene.frame_start, scene.frame_end = 1, cfg['frame_end']
bpy.ops.wm.alembic_import(filepath=str(exchange), as_background_job=False)
meshes = [item for item in scene.objects if item.type == 'MESH']
assert len(meshes) == 1
name = meshes[0].name
for cache in bpy.data.cache_files:
    assert Path(bpy.path.abspath(cache.filepath)).resolve() == exchange
    cache.filepath = '//' + exchange.name
reimport = out / 'FeatureMotion-Reimport.blend'
bpy.ops.wm.save_as_mainfile(filepath=str(reimport), relative_remap=False)
bpy.ops.wm.open_mainfile(filepath=str(reimport))
scene, imported = bpy.context.scene, bpy.data.objects[name]
errors = []
for frame in range(1, cfg['frame_end'] + 1):
    scene.frame_set(frame)
    actual = world_points(imported)
    assert actual.shape == reference[frame - 1].shape
    errors.append(float(np.linalg.norm(actual - reference[frame - 1], axis=1).max()))
maximum = max(errors)
report = dict(schema_version=1, status='passed-indexed-animated-cache-reimport-only'
    if maximum <= cfg['maximum_error_m'] else 'failed-indexed-animated-cache-reimport',
    cpu_only=True, frames=cfg['frame_end'], vertices=vertex_count,
    input_sha256=cfg['input_sha256'], tool_sha256=sha(__file__),
    exchange_sha256=sha(exchange), reimport_sha256=sha(reimport),
    maximum_error_m=maximum, limit_m=cfg['maximum_error_m'], per_frame_error_m=errors,
    limitations=['Offline geometry-cache exchange, not a skeletal runtime or engine acceptance.',
        'Indexed world positions only; UV/normals/material appearance require separate checks.',
        'PBR material networks/textures are retained by the original source, not this diagnostic reimport.'])
(out / 'exchange-report.json').write_text(json.dumps(report, indent=2))
for path, expected in cfg['input_sha256'].items():
    assert sha(path) == expected, path
print(json.dumps({'status': report['status'], 'maximum_error_m': maximum}), flush=True)
assert maximum <= cfg['maximum_error_m']
