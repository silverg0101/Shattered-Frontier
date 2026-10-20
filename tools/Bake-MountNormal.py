"""Bake a tangent-space normal map from a full-detail mount mesh onto its budgeted LOD0 (CPU Cycles).

Run: blender -b --factory-startup --python tools/Bake-MountNormal.py -- --profile <json> --output <new folder>
Both inputs are delivery .blend files in rest pose and identical world placement. Short cage/ray distances
keep stacked hair sheets from projecting onto each other. Writes the normal PNG, a lit before/after
comparison and a report.
"""
import argparse, hashlib, json, math, sys
from pathlib import Path
import bpy
import numpy as np


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for chunk in iter(lambda: f.read(8 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


p = argparse.ArgumentParser()
p.add_argument('--profile', required=True); p.add_argument('--output', required=True)
a = p.parse_args(sys.argv[sys.argv.index('--') + 1:])
profile, out = Path(a.profile).resolve(), Path(a.output).resolve()
cfg = json.loads(profile.read_text())
assert cfg['schema_version'] == 1 and not out.exists() and 512 <= cfg['resolution'] <= 4096
for f, h in cfg['input_sha256'].items():
    assert sha(f) == h, f
out.mkdir(parents=True)
(out / 'profile.json').write_bytes(profile.read_bytes()); (out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())

bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['low_blend']).resolve()))
scene = bpy.context.scene
low = bpy.data.objects[cfg['object']]
for o in scene.objects:
    if o.type == 'ARMATURE': o.data.pose_position = 'REST'
with bpy.data.libraries.load(str(Path(cfg['high_blend']).resolve()), link=False) as (src, dst):
    dst.objects = [cfg['object']]
high = dst.objects[0]; high.name = 'HIGH | full detail'; scene.collection.objects.link(high)
for m in list(high.modifiers): high.modifiers.remove(m)  # rest geometry, no armature
bpy.context.view_layer.update()  # appended objects get world matrices on update
assert (high.matrix_world.translation - low.matrix_world.translation).length < 1e-4, f'high/low placement differs: {high.matrix_world.translation} vs {low.matrix_world.translation}'

res = cfg['resolution']
img = bpy.data.images.new('Normal bake', res, res, alpha=False, float_buffer=False); img.colorspace_settings.name = 'Non-Color'
mat = low.material_slots[0].material; nt = mat.node_tree
tex = nt.nodes.new('ShaderNodeTexImage'); tex.image = img; nt.nodes.active = tex
scene.render.engine = 'CYCLES'; scene.cycles.device = 'CPU'; scene.cycles.samples = 1
bake = scene.render.bake
bake.use_selected_to_active = True; bake.cage_extrusion = cfg['cage_extrusion_m']; bake.max_ray_distance = cfg['max_ray_distance_m']
bake.margin = cfg['margin_px']; bake.normal_space = 'TANGENT'
bpy.ops.object.select_all(action='DESELECT'); high.select_set(True); low.select_set(True); bpy.context.view_layer.objects.active = low
bpy.ops.object.bake(type='NORMAL', uv_layer=cfg['uv_layer'])
img.filepath_raw = str(out / 'Normal-2K.png'); img.file_format = 'PNG'; img.save()
px = np.empty(res * res * 4, np.float32); img.pixels.foreach_get(px); px = px.reshape(-1, 4)[:, :3]
flat = np.abs(px - [0.5, 0.5, 1.0]).max(1) < 0.02

# Before/after: same lit view, normal map off then on.
high.hide_render = True
bsdf = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
nmap = nt.nodes.new('ShaderNodeNormalMap'); nmap.uv_map = cfg['uv_layer']
nt.links.new(tex.outputs['Color'], nmap.inputs['Color'])
for o in scene.objects:
    if o.type == 'ARMATURE': o.data.pose_position = 'POSE'
cam = bpy.data.objects.new('REVIEW', bpy.data.cameras.new('REVIEW')); scene.collection.objects.link(cam); scene.camera = cam
cam.location = cfg['camera_m']; cam.rotation_euler = [math.radians(v) for v in cfg['camera_rot_deg']]; cam.data.lens = cfg['camera_lens_mm']
light = bpy.data.objects.new('KEY', bpy.data.lights.new('KEY', 'SUN')); scene.collection.objects.link(light)
light.rotation_euler = [math.radians(v) for v in (50, 10, 40)]; light.data.energy = 4
scene.cycles.samples = cfg['review_samples']; scene.render.resolution_x, scene.render.resolution_y = cfg['review_resolution']
shots = []
for tag, on in [('before', False), ('after', True)]:
    if on: nt.links.new(nmap.outputs['Normal'], bsdf.inputs['Normal'])
    scene.render.filepath = str(out / f'review-{tag}.png'); bpy.ops.render.render(write_still=True); shots.append(scene.render.filepath)
report = dict(schema_version=1, status='normal-baked-needs-visual-review', cpu_only=True, input_sha256=cfg['input_sha256'], tool_sha256=sha(__file__),
              profile_sha256=sha(profile), normal_sha256=sha(out / 'Normal-2K.png'), resolution=res,
              flat_texel_fraction=float(flat.mean()), mean_rgb=px.mean(0).round(4).tolist(), review=[Path(s).name for s in shots],
              limitations=['Selected-to-active over thin stacked sheets can pick the wrong layer within the ray distance.',
                           'Flat texels include empty atlas space.'])
(out / 'normal-report.json').write_text(json.dumps(report, indent=1))
print('NORMAL', json.dumps({k: report[k] for k in ['flat_texel_fraction', 'mean_rgb']}), flush=True)
