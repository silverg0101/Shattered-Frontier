"""Locate peak held-startup motion in a frozen native cloth cache, read only.

Requires agreement with the producer's saved convergence measurements before
interpreting vertices. It does not tune, rebake, save or approve attachments.
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
profile, out = Path(a.profile).resolve(), Path(a.output).resolve()
cfg = json.loads(profile.read_text())
assert cfg['schema_version'] == 1 and not out.exists()
assert 2 <= cfg['held_frames'] <= 2000 and 1 <= cfg['top_count'] <= 100
for path, expected in cfg['input_sha256'].items():
    assert sha(path) == expected, path
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()))
scene, proxy = bpy.context.scene, bpy.data.objects[cfg['proxy_object']]
for action in bpy.data.actions:
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    for key in curve.keyframe_points:
                        key.co.x += cfg['held_frames']
                        key.handle_left.x += cfg['held_frames']
                        key.handle_right.x += cfg['held_frames']
for modifier in proxy.modifiers:
    if modifier.type in ['CLOTH', 'ARMATURE']:
        modifier.show_viewport = True
    elif modifier.type == 'MESH_CACHE':
        modifier.show_viewport = False
cloth = next(m for m in proxy.modifiers if m.type == 'CLOTH')
assert cloth.point_cache.is_baked
pin_group = proxy.vertex_groups[cloth.settings.vertex_group_mass].index
pins = np.array([any(g.group == pin_group and g.weight > .99 for g in v.groups)
                 for v in proxy.data.vertices])
held = []
for frame in [cfg['held_frames'] - 1, cfg['held_frames']]:
    scene.frame_set(frame)
    actual = proxy.evaluated_get(bpy.context.evaluated_depsgraph_get())
    values = np.empty(len(proxy.data.vertices) * 3, np.float32)
    actual.data.vertices.foreach_get('co', values)
    held.append(values.reshape(-1, 3))
speed = np.linalg.norm(held[1] - held[0], axis=1) * scene.render.fps
rms, peak = float(np.sqrt(np.mean(speed[~pins] ** 2))), float(speed[~pins].max())
expected = json.loads(Path(cfg['producer_report']).read_text())['warmup_velocity']
assert abs(rms - expected['rms_speed_m_s']) <= 1e-5
assert abs(peak - expected['peak_speed_m_s']) <= 1e-5
obj = bpy.data.objects[cfg['object']]
indices = np.empty(len(obj.data.vertices), np.int32)
selected = np.empty(len(obj.data.vertices), np.bool_)
obj.data.attributes[cfg['index_attribute']].data.foreach_get('value', indices)
obj.data.attributes[cfg['selection_attribute']].data.foreach_get('value', selected)
original = np.flatnonzero(selected)
unique, first = np.unique(indices[original], return_index=True)
assert np.array_equal(unique, np.arange(len(proxy.data.vertices)))
representatives = original[first]
neighbors = [[] for _ in proxy.data.vertices]
parents = np.arange(len(proxy.data.vertices), dtype=np.int32)
geometric_parents = parents.copy()


def find(index, table=parents):
    while table[index] != index:
        table[index] = table[table[index]]
        index = int(table[index])
    return index


for edge in proxy.data.edges:
    i, j = edge.vertices
    neighbors[i].append((j, bool(edge.is_loose)))
    neighbors[j].append((i, bool(edge.is_loose)))
    if not edge.is_loose or cloth.settings.use_sewing_springs:
        parents[find(int(i))] = find(int(j))
    if not edge.is_loose:
        geometric_parents[find(int(i), geometric_parents)] = find(int(j), geometric_parents)
components = {}
for index in range(len(parents)):
    components.setdefault(find(index), []).append(index)
component_by_vertex = {}
component_rows = []
for number, ids in enumerate(sorted(components.values(), key=lambda ids: (-len(ids), ids[0]))):
    ids = np.array(ids, np.int32)
    for index in ids:
        component_by_vertex[int(index)] = number
    component_rows.append(dict(id=number, proxy_vertices=len(ids),
        pinned_vertices=int(pins[ids].sum()),
        source_vertex_ids=representatives[ids].tolist(),
        maximum_speed_m_s=float(speed[ids].max()),
        rms_speed_m_s=float(np.sqrt(np.mean(speed[ids] ** 2))),
        mean_velocity_m_s=((held[1][ids] - held[0][ids]) * scene.render.fps).mean(axis=0).tolist()))
geometric_components = {}
for index in range(len(parents)):
    geometric_components.setdefault(find(index, geometric_parents), []).append(index)
geometric_rows = []
for root_id, ids in sorted(geometric_components.items()):
    ids = np.array(ids, np.int32)
    loose_links = []
    for index in ids:
        for other, loose in neighbors[int(index)]:
            if loose:
                loose_links.append(dict(source_vertex=int(representatives[index]),
                    other_source_vertex=int(representatives[other]),
                    other_geometric_component=find(other, geometric_parents)))
    geometric_rows.append(dict(id=root_id, proxy_vertices=len(ids),
        pinned_vertices=int(pins[ids].sum()), source_vertex_ids=representatives[ids].tolist(),
        sewing_links=loose_links, maximum_speed_m_s=float(speed[ids].max()),
        mean_velocity_m_s=((held[1][ids] - held[0][ids]) * scene.render.fps).mean(axis=0).tolist()))
ranked = np.flatnonzero(~pins)[np.argsort(speed[~pins])[::-1][:cfg['top_count']]]
rows = [dict(proxy_vertex=int(i), source_vertex=int(representatives[i]),
    spring_component=component_by_vertex[int(i)],
    geometric_component=find(int(i), geometric_parents),
    speed_m_s=float(speed[i]), position_proxy_m=held[1][i].tolist(),
    neighbors=[dict(proxy_vertex=int(j), pinned=bool(pins[j]), sewing_edge=loose)
               for j, loose in neighbors[i]]) for i in ranked]
out.mkdir(parents=True)
np.savez_compressed(out / 'held-proxy-points.npz', points=np.array(held, np.float32),
    frames=np.array([cfg['held_frames'] - 1, cfg['held_frames']], np.int32),
    source_vertex_ids=representatives,
    rest_points=np.array([vertex.co[:] for vertex in proxy.data.vertices], np.float32),
    pinned_mask=pins)
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
(out / 'profile.json').write_bytes(profile.read_bytes())
report = dict(schema_version=1, status='source-bound-warmup-diagnostic-only', cpu_only=True,
    input_sha256=cfg['input_sha256'], tool_sha256=sha(__file__),
    held_points_sha256=sha(out / 'held-proxy-points.npz'),
    rms_speed_m_s=rms, peak_speed_m_s=peak, top_vertices=rows,
    sewing_enabled=bool(cloth.settings.use_sewing_springs),
    spring_components=component_rows,
    geometric_components=geometric_rows,
    unanchored_components=[row['id'] for row in component_rows if row['pinned_vertices'] == 0],
    limitations=['Agreement with cached producer metrics only; no rebake or acceptance.',
        'Neighborhood and proposed sewing-edge data do not prove semantic strand ownership.',
        'Connectivity includes mesh edges and enabled loose sewing edges; graph reachability alone does not prove solver constraints are effective.'])
(out / 'warmup-report.json').write_text(json.dumps(report, indent=2))
for path, expected_sha in cfg['input_sha256'].items():
    assert sha(path) == expected_sha, path
print(json.dumps({'rms_speed_m_s': rms, 'peak_speed_m_s': peak,
                  'peak_source_vertex': rows[0]['source_vertex']}), flush=True)
