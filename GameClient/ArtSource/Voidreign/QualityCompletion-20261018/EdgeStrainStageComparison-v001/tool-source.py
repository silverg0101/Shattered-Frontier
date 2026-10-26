"""Compare exact source edges across frozen guide/cloth/corrective candidates."""
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


p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile', required=True)
p.add_argument('--output', required=True)
a = p.parse_args(sys.argv[sys.argv.index('--') + 1:])
profile, out = Path(a.profile).resolve(), Path(a.output).resolve()
cfg = json.loads(profile.read_text())
assert cfg['schema_version'] == 1 and not out.exists()
assert 1 <= len(cfg['sources']) <= 16 and 1 <= len(cfg['cases']) <= 32
for path, expected in cfg['input_sha256'].items():
    assert sha(path) == expected, path
with np.load(cfg['rest_selection'], allow_pickle=False) as archive:
    rest, triangles = archive['vertices'].astype(np.float64), archive['triangles']
with np.load(cfg['free_surface_review'], allow_pickle=False) as archive:
    free = archive['free_surface_mask']
with np.load(cfg['weight_field'], allow_pickle=False) as archive:
    labels, root_distance = archive['labels'], archive['root_distance_m']
assert free.shape == labels.shape == root_distance.shape == (len(rest),)
rows = []
for candidate in cfg['sources']:
    assert candidate['path'] in cfg['input_sha256']
    bpy.ops.wm.open_mainfile(filepath=str(Path(candidate['path']).resolve()))
    scene, obj = bpy.context.scene, bpy.data.objects[cfg['object']]
    obj.data.calc_loop_triangles()
    local = np.array([vertex.co[:] for vertex in obj.data.vertices], np.float32)
    faces = np.array([tuple(face.vertices) for face in obj.data.loop_triangles], np.int32)
    assert hashlib.sha256(local.tobytes() + faces.tobytes()).hexdigest() == cfg['mesh_hash']
    assert np.array_equal(faces, triangles)
    for case in cfg['cases']:
        edge, frame = case['edge'], case['frame']
        assert len(edge) == 2 and min(edge) >= 0 and max(edge) < len(rest)
        assert type(frame) is int and scene.frame_start <= frame <= scene.frame_end
        scene.frame_set(frame)
        evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
        evaluated.data.calc_loop_triangles()
        assert len(evaluated.data.vertices) == len(rest)
        evaluated_faces = np.array([tuple(face.vertices) for face in evaluated.data.loop_triangles], np.int32)
        assert np.array_equal(evaluated_faces, triangles)
        points = np.array([(evaluated.matrix_world @ evaluated.data.vertices[index].co)[:] for index in edge])
        length = float(np.linalg.norm(points[1] - points[0]))
        rest_length = float(np.linalg.norm(rest[edge[1]] - rest[edge[0]]))
        rows.append(dict(source=candidate['id'], case=case['id'], frame=frame, edge=edge,
            length_m=length, rest_length_m=rest_length, absolute_change_m=abs(length - rest_length),
            relative_change=abs(length - rest_length) / rest_length if rest_length else None,
            endpoints=[dict(source_vertex=index, original_free=bool(free[index]), label=int(labels[index]),
                root_distance_m=float(root_distance[index])) for index in edge]))
out.mkdir(parents=True)
(out / 'profile.json').write_bytes(profile.read_bytes())
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
report = dict(schema_version=1, status='source-bound-edge-stage-comparison-only', cpu_only=True,
    input_sha256=cfg['input_sha256'], tool_sha256=sha(__file__), profile_sha256=sha(profile), rows=rows,
    limitations=['Selected diagnostic edges/frames, not a new full-surface check or repair.',
        'Original free/root classification is recorded without changing masks or acceptance limits.'])
(out / 'edge-comparison.json').write_text(json.dumps(report, indent=2))
for path, expected in cfg['input_sha256'].items():
    assert sha(path) == expected, path
print(json.dumps({'cases': len(cfg['cases']), 'sources': len(cfg['sources'])}))
