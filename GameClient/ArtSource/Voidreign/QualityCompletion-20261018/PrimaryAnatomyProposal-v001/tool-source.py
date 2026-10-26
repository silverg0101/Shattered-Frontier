"""CPU rest-mesh atlas and authored landmark/region proposal, without rigging.

Selections are geometric proposals, never automatic anatomical approval.
Source/rest correspondence must already have independent retention evidence.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
from mathutils import Vector
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
cfg = json.loads(profile.read_text(encoding='utf-8'))
assert cfg['schema_version'] == 1 and cfg['units'] == 'metres'
assert not out.exists()
assert 512 <= cfg['render']['resolution'] <= 2048 and 1 <= cfg['render']['samples'] <= 64
for path, expected in cfg['input_sha256'].items():
    assert sha(path) == expected, path
for key in ['source', 'rest_selection', 'retention_report']:
    assert cfg[key] in cfg['input_sha256']
semantic = None
if cfg.get('semantic_selection'):
    assert cfg['semantic_selection'] in cfg['input_sha256']
    with np.load(cfg['semantic_selection'], allow_pickle=False) as archive:
        semantic = archive['labels']
retention = json.loads(Path(cfg['retention_report']).read_text())
assert retention['status'] == 'passed-indexed-rest-retention-only'
assert retention['candidate_sha256'] == cfg['input_sha256'][cfg['source']]
with np.load(cfg['rest_selection'], allow_pickle=False) as archive:
    points, faces = archive['vertices'], archive['triangles']
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()))
original = bpy.data.objects[cfg['object']]
assert len(original.data.vertices) == len(points)
raw_triangles = np.array([list(p.vertices) for p in original.data.polygons], dtype=np.int32)
assert np.array_equal(raw_triangles, faces), 'Exact indexed triangles required'
out.mkdir(parents=True)
(out / 'profile.json').write_bytes(profile.read_bytes())
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
mesh = bpy.data.meshes.new('Source-indexed rest diagnostic')
mesh.from_pydata(points.tolist(), [], faces.tolist())
mesh.update()
obj = bpy.data.objects.new('REFERENCE | original indexed rest surface', mesh)
bpy.context.collection.objects.link(obj)

def material(name, color):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1)
    mat.use_nodes = True
    shader = mat.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*color, 1)
    shader.inputs['Roughness'].default_value = .75
    return mat

mesh.materials.append(material('Rest reference gray', cfg['render']['surface_color']))
regions, records = {}, []
for index, region in enumerate(cfg.get('regions', []), 1):
    lower, upper = np.asarray(region['minimum_m']), np.asarray(region['maximum_m'])
    assert lower.shape == upper.shape == (3,) and np.all(lower < upper)
    mask = np.all((points >= lower) & (points <= upper), axis=1)
    if region.get('exclude_semantic_labels'):
        assert semantic is not None and len(semantic) == len(points)
        mask &= ~np.isin(semantic, region['exclude_semantic_labels'])
    regions[region['id']] = np.flatnonzero(mask).astype(np.int32)
    mesh.materials.append(material(region['id'], region['color']))
    # Majority membership is only a visual overlay. Export exact vertex mask.
    chosen = mask[faces].sum(axis=1) >= 2
    for polygon_index in np.flatnonzero(chosen):
        mesh.polygons[int(polygon_index)].material_index = index
    records.append(dict(id=region['id'], vertices=int(mask.sum()),
                        status='geometric-proposal-needs-anatomical-review'))
for landmark in cfg.get('landmarks', []):
    position = Vector(landmark['position_m'])
    assert all(-10 < v < 10 for v in position)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=12, ring_count=6,
        radius=cfg['render']['landmark_radius_m'], location=position)
    sphere = bpy.context.object
    sphere.name = 'PROPOSAL | ' + landmark['id']
    sphere.data.materials.append(material(landmark['id'], landmark['color']))
landmarks = {row['id']: row for row in cfg.get('landmarks', [])}
for chain in cfg.get('chains', []):
    for first, second in zip(chain['landmarks'], chain['landmarks'][1:]):
        start, end = Vector(landmarks[first]['position_m']), Vector(landmarks[second]['position_m'])
        delta = end - start
        assert delta.length > .001
        bpy.ops.mesh.primitive_cylinder_add(vertices=10, radius=cfg['render']['link_radius_m'],
            depth=delta.length, location=(start+end)/2)
        segment = bpy.context.object
        segment.name = 'PROPOSED LINK | ' + first + ' -> ' + second
        segment.rotation_euler = delta.to_track_quat('Z', 'Y').to_euler()
        segment.data.materials.append(material(segment.name, chain['color']))
np.savez_compressed(out / 'region-selections.npz', **regions)
scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.device = 'CPU'
scene.cycles.samples = cfg['render']['samples']
scene.cycles.use_denoising = False
scene.render.resolution_x = scene.render.resolution_y = cfg['render']['resolution']
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.world = bpy.data.worlds.new('Atlas world')
scene.world.use_nodes = True
scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.35, .35, .35, 1)
scene.world.node_tree.nodes['Background'].inputs[1].default_value = .8
bpy.ops.object.light_add(type='AREA', location=cfg['render']['light_position_m'])
light = bpy.context.object
light.data.energy = cfg['render']['light_watts']
light.data.shape = 'DISK'
light.data.size = cfg['render']['light_size_m']
light.rotation_euler = (Vector(cfg['render']['target_m'])-light.location).to_track_quat('-Z', 'Y').to_euler()
bpy.ops.object.camera_add()
camera = bpy.context.object
scene.camera = camera
camera.data.type = 'ORTHO'
camera.data.ortho_scale = cfg['render']['ortho_scale_m']
views = []
for view in cfg['views']:
    camera.location = view['position_m']
    target = view.get('target_m', cfg['render']['target_m'])
    camera.rotation_euler = (Vector(target)-camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera.data.ortho_scale = view.get('ortho_scale_m', cfg['render']['ortho_scale_m'])
    scene.render.filepath = str(out / (view['id'] + '.png'))
    bpy.ops.render.render(write_still=True)
    views.append(dict(id=view['id'], image=view['id']+'.png'))
    print('Rendered anatomy atlas: ' + view['id'], flush=True)
bpy.ops.wm.save_as_mainfile(filepath=str(out / 'Mount-AnatomyProposal.blend'))
report = dict(schema_version=1, status='anatomy-proposal-needs-review', cpu_only=True,
    input_sha256=cfg['input_sha256'], tool_sha256=sha(__file__), profile_sha256=sha(profile),
    vertices=len(points), triangles=len(faces), regions=records, views=views,
    coordinate_convention=cfg['coordinate_convention'], landmarks=cfg.get('landmarks', []),
    limitations=['Exact indexed rest diagnostic only; source rig/materials are not copied into this proposal.',
      'Box domains can include hanging tack or miss curved anatomy; not skin weights or semantic approval.',
      'Landmarks are authored proposals, not inferred joint centers or accepted stance.'])
(out / 'anatomy-report.json').write_text(json.dumps(report, indent=2))
for path, expected in cfg['input_sha256'].items():
    assert sha(path) == expected, path
