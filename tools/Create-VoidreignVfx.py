"""Material-driven VFX kit for the Voidreign abilities (Editor Python, headless-safe).

    UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -nullrhi -unattended
        -script="<project>/tools/Create-VoidreignVfx.py --dest /Game/Frontier/Creatures/Voidreign/V1/VFX"

Creates M_VFX_Void (unlit additive, two-sided: radial falloff x panning noise x Color x Intensity x Progress)
and one material instance per effect, plus vfx-map.json mapping animation event markers to effect, mesh,
scale and duration. Gameplay drives Progress over the effect's duration on an engine plane/disc mesh.
Placeholder-grade: no particles/Niagara.
"""
import json, shlex
from pathlib import Path
import unreal

PROJECT = Path(__file__).resolve().parents[1]
toks = shlex.split(unreal.SystemLibrary.get_command_line(), posix=False)
i = next(i for i, t in enumerate(toks) if 'Create-VoidreignVfx.py' in t)
argv = [t.strip('"') for t in toks[i + 1:]]
dest = argv[argv.index('--dest') + 1]
if unreal.EditorAssetLibrary.does_directory_exist(dest): raise RuntimeError('Refusing to overwrite ' + dest)
unreal.EditorAssetLibrary.make_directory(dest)
tools, mel = unreal.AssetToolsHelpers.get_asset_tools(), unreal.MaterialEditingLibrary

mat = tools.create_asset('M_VFX_Void', dest, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property('two_sided', True)
E = lambda cls, x, y: mel.create_material_expression(mat, cls, x, y)
def param(cls, name, value, x, y):
    e = E(cls, x, y); e.set_editor_property('parameter_name', name); e.set_editor_property('default_value', value); return e
uv = E(unreal.MaterialExpressionTextureCoordinate, -1400, 0)
center = E(unreal.MaterialExpressionConstant2Vector, -1400, 150); center.set_editor_property('r', .5); center.set_editor_property('g', .5)
dist = E(unreal.MaterialExpressionDistance, -1200, 50); mel.connect_material_expressions(uv, '', dist, 'A'); mel.connect_material_expressions(center, '', dist, 'B')
two = E(unreal.MaterialExpressionMultiply, -1050, 50); two.set_editor_property('const_b', 2.0); mel.connect_material_expressions(dist, '', two, 'A')
inv = E(unreal.MaterialExpressionOneMinus, -900, 50); mel.connect_material_expressions(two, '', inv, '')
sat = E(unreal.MaterialExpressionSaturate, -780, 50); mel.connect_material_expressions(inv, '', sat, '')
falloff = param(unreal.MaterialExpressionScalarParameter, 'Falloff', 2.0, -780, 180)
pw = E(unreal.MaterialExpressionPower, -620, 80); mel.connect_material_expressions(sat, '', pw, 'Base'); mel.connect_material_expressions(falloff, '', pw, 'Exponent')
pan = E(unreal.MaterialExpressionPanner, -1200, 300); mel.connect_material_expressions(uv, '', pan, 'Coordinate')
pan.set_editor_property('speed_x', 0.15); pan.set_editor_property('speed_y', 0.3)
noise = E(unreal.MaterialExpressionTextureSample, -1000, 300)
tex = unreal.load_asset('/Engine/EngineMaterials/Good64x64TilingNoiseHighFreq')
if tex: noise.set_editor_property('texture', tex)
mel.connect_material_expressions(pan, '', noise, 'UVs')
color = param(unreal.MaterialExpressionVectorParameter, 'Color', unreal.LinearColor(0.35, 0.05, 0.9, 1), -620, 300)
inten = param(unreal.MaterialExpressionScalarParameter, 'Intensity', 10.0, -620, 450)
prog = param(unreal.MaterialExpressionScalarParameter, 'Progress', 1.0, -620, 560)
m1 = E(unreal.MaterialExpressionMultiply, -450, 80); mel.connect_material_expressions(pw, '', m1, 'A'); mel.connect_material_expressions(noise, 'R', m1, 'B')
m2 = E(unreal.MaterialExpressionMultiply, -320, 160); mel.connect_material_expressions(m1, '', m2, 'A'); mel.connect_material_expressions(color, '', m2, 'B')
m3 = E(unreal.MaterialExpressionMultiply, -200, 240); mel.connect_material_expressions(m2, '', m3, 'A'); mel.connect_material_expressions(inten, '', m3, 'B')
m4 = E(unreal.MaterialExpressionMultiply, -80, 320); mel.connect_material_expressions(m3, '', m4, 'A'); mel.connect_material_expressions(prog, '', m4, 'B')
mel.connect_material_property(m4, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(mat)

EFFECTS = {  # name: (color rgb, intensity, falloff, events, scale_m, duration_s)
    'RiftOpen': ((.35, .05, .9), 14, 1.5, ['telegraph_start'], 2.0, .9),
    'RiftTrail': ((.25, .02, .6), 8, 2.5, ['rift_charge_run'], 1.2, .4),
    'RiftAnchor': ((.3, .05, .8), 5, 2.0, ['anchor_window'], 1.6, 3.0),
    'RiftCollapse': ((.6, .2, 1.0), 25, 1.2, ['vfx_rift_collapse', 'vanish', 'appear'], 2.4, .6),
    'MomentumSpark': ((1.0, .35, .05), 18, 3.0, ['hit'], .6, .25),
    'GroundCrack': ((.8, .1, .3), 6, 1.0, ['vfx_ground_crack'], 1.4, .8),
    'Snort': ((.6, .6, .65), 2, 2.0, ['vfx_snort'], .8, .7),
}
made = {}
for name, (rgb, inten_v, fall, events, scale, dur) in EFFECTS.items():
    mi = tools.create_asset(f'MI_VFX_{name}', dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, mat)
    mel.set_material_instance_vector_parameter_value(mi, 'Color', unreal.LinearColor(*rgb, 1))
    mel.set_material_instance_scalar_parameter_value(mi, 'Intensity', inten_v)
    mel.set_material_instance_scalar_parameter_value(mi, 'Falloff', fall)
    made[name] = dict(material=mi.get_path_name(), events=events, mesh='/Engine/BasicShapes/Plane', scale_m=scale, duration_s=dur, drive='Progress 0->1->0')
unreal.EditorAssetLibrary.save_directory(dest, only_if_is_dirty=False, recursive=True)
out = PROJECT / 'GameClient' / 'ArtSource' / 'Voidreign' / 'VFX' / 'vfx-map.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(dict(schema_version=1, content=dest, master=mat.get_path_name(), noise_texture_found=tex is not None, effects=made,
                               limitations=['Material-on-plane placeholder effects; no Niagara particles.']), indent=1))
unreal.log('VFX_CREATED ' + json.dumps(dict(count=len(made), noise=tex is not None)))
