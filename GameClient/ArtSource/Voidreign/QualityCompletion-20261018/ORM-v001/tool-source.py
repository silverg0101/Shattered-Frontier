"""Bake an ORM texture (R=ambient occlusion, G=roughness, B=metallic) for a skinned mount, CPU Cycles.

Run: blender -b --factory-startup --python tools/Bake-MountORM.py -- --profile <json> --output <new folder>
AO is baked in rest pose to the mesh's UV map. Roughness is classified from the base colour (HSV bands
declared in the profile); metallic is a constant. Writes ORM PNG, a lit preview and a report.
"""
import argparse, colorsys, hashlib, json, sys
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

bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()))
scene = bpy.context.scene; obj = bpy.data.objects[cfg['object']]
rig = next(o for o in scene.objects if o.type == 'ARMATURE'); rig.data.pose_position = 'REST'
res = cfg['resolution']

# Roughness from base colour, resampled to the bake resolution.
base = bpy.data.images.load(str(Path(cfg['base_color']).resolve())); base.scale(res, res)
px = np.array(base.pixels[:], dtype=np.float32).reshape(res, res, 4)[..., :3]
mx, mn = px.max(-1), px.min(-1); val = mx; sat = np.where(mx > 1e-6, (mx - mn) / np.maximum(mx, 1e-6), 0)
rough = np.full((res, res), cfg['roughness']['default'], np.float32)
for band in cfg['roughness']['bands']:  # later bands win
    m = np.ones((res, res), bool)
    for key, (lo, hi) in band['where'].items():
        ch = {'value': val, 'saturation': sat}[key]; m &= (ch >= lo) & (ch <= hi)
    rough[m] = band['roughness']
coverage = {b['name']: float(((rough == b['roughness'])).mean()) for b in cfg['roughness']['bands']}

# AO bake through a temporary image node on every material.
ao = bpy.data.images.new('AO bake', res, res, alpha=False, float_buffer=False)
for slot in obj.material_slots:
    nt = slot.material.node_tree; n = nt.nodes.new('ShaderNodeTexImage'); n.image = ao; nt.nodes.active = n
scene.render.engine = 'CYCLES'; scene.cycles.device = 'CPU'; scene.cycles.samples = cfg['ao_samples']
scene.render.bake.margin = cfg['bake_margin_px']
bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True); bpy.context.view_layer.objects.active = obj
bpy.ops.object.bake(type='AO', uv_layer=cfg['uv_layer'])
aop = np.array(ao.pixels[:], dtype=np.float32).reshape(res, res, 4)[..., 0]

orm = np.stack([aop, rough, np.full((res, res), cfg['metallic'], np.float32), np.ones((res, res), np.float32)], -1)
img = bpy.data.images.new('ORM', res, res, alpha=False); img.pixels[:] = orm.ravel()
img.filepath_raw = str(out / 'ORM-2K.png'); img.file_format = 'PNG'; img.colorspace_settings.name = 'Non-Color'; img.save()

# Lit preview with the ORM wired in (AO multiplied into base colour for preview only).
for slot in obj.material_slots:
    nt = slot.material.node_tree; bsdf = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
    t = nt.nodes.new('ShaderNodeTexImage'); t.image = img; sep = nt.nodes.new('ShaderNodeSeparateColor')
    nt.links.new(t.outputs[0], sep.inputs[0]); nt.links.new(sep.outputs[1], bsdf.inputs['Roughness']); nt.links.new(sep.outputs[2], bsdf.inputs['Metallic'])
rig.data.pose_position = 'POSE'
report = dict(schema_version=1, status='orm-baked-needs-visual-review', cpu_only=True, input_sha256=cfg['input_sha256'], tool_sha256=sha(__file__),
              profile_sha256=sha(profile), orm_sha256=sha(out / 'ORM-2K.png'), resolution=res,
              ao_mean=float(aop.mean()), ao_p05=float(np.percentile(aop, 5)), roughness_mean=float(rough.mean()), roughness_band_coverage=coverage,
              limitations=['Roughness is an albedo-derived classification, not authored material masks.', 'AO baked in rest pose; no normal map.'])
(out / 'orm-report.json').write_text(json.dumps(report, indent=1))
print('ORM', json.dumps({k: report[k] for k in ['ao_mean', 'ao_p05', 'roughness_mean', 'roughness_band_coverage']}), flush=True)
