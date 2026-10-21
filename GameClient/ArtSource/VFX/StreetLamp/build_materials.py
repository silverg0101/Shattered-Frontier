"""Author only /Game/Frontier/VFX/StreetLamp assets in the open full editor.

Called by Greenlands/lamp_fx.py; safe to rerun. No Niagara graph authoring is required.
"""
from pathlib import Path
import json
import unreal as u

HERE = Path(__file__).resolve().parent
CFG = HERE.parent.parent / 'Greenlands' / 'lamp-fx.json'
DEST = '/Game/Frontier/VFX/StreetLamp'
mel = u.MaterialEditingLibrary
eal = u.EditorAssetLibrary
tools = u.AssetToolsHelpers.get_asset_tools()


def _node(mat, cls, **props):
    n = mel.create_material_expression(mat, cls)
    for name, value in props.items():
        n.set_editor_property(name, value)
    return n


def _link(a, b, pin, output=''):
    assert mel.connect_material_expressions(a, output, b, pin), pin
    return b


def _output(a, prop, channel=''):
    assert mel.connect_material_property(a, channel, prop), prop


def _constant(mat, value):
    return _node(mat, u.MaterialExpressionConstant, r=float(value))


def _mul(mat, a, b):
    n = _node(mat, u.MaterialExpressionMultiply)
    _link(a, n, 'A'); _link(b, n, 'B')
    return n


def _add(mat, a, b):
    n = _node(mat, u.MaterialExpressionAdd)
    _link(a, n, 'A'); _link(b, n, 'B')
    return n


def _sin(mat, a):
    return _link(a, _node(mat, u.MaterialExpressionSine), '')


def _time(mat, speed, phase=0):
    wave = _mul(mat, _node(mat, u.MaterialExpressionTime), _constant(mat, speed))
    return _add(mat, wave, _constant(mat, phase)) if phase else wave


def _flicker(mat, cfg):
    # Two incommensurate waves avoid a regular metronome feel; both stay in 0..1.
    first = _sin(mat, _time(mat, cfg['flicker_speed']))
    second = _sin(mat, _time(mat, cfg['flicker_speed'] * 1.73, .19))
    blend = _add(mat, _mul(mat, first, _constant(mat, .7)), _mul(mat, second, _constant(mat, .3)))
    return _add(mat, _constant(mat, 1.0), _mul(mat, blend, _constant(mat, cfg['flicker_amount'])))


def _material(name, domain=None, blend=None):
    path = DEST + '/' + name
    mat = u.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(name, DEST, u.Material, u.MaterialFactoryNew())
    assert mat, path
    mel.delete_all_material_expressions(mat)
    if domain is not None:
        mat.set_editor_property('material_domain', domain)
    else:
        mat.set_editor_property('material_domain', u.MaterialDomain.MD_SURFACE)
        mat.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
        mat.set_editor_property('two_sided', True)
    if blend is not None:
        mat.set_editor_property('blend_mode', blend)
    return mat


def _sample(mat, texture, pan=0):
    tex = _node(mat, u.MaterialExpressionTextureSample, texture=u.load_asset(DEST + '/' + texture))
    assert tex.get_editor_property('texture'), texture
    if pan:
        uv = _node(mat, u.MaterialExpressionTextureCoordinate)
        panner = _node(mat, u.MaterialExpressionPanner, speed_x=.012, speed_y=pan)
        _link(uv, panner, 'Coordinate'); _link(panner, tex, 'UVs')
    return tex


def _alpha(mat, sample):
    scalar = _node(mat, u.MaterialExpressionMultiply, const_b=1.0)
    _link(sample, scalar, 'A', 'A')
    return scalar


def _save(mat):
    mel.recompile_material(mat)
    assert eal.save_loaded_asset(mat), mat.get_name()


def _texture(name):
    path = DEST + '/' + name
    task = u.AssetImportTask()
    task.filename = str(HERE / (name + '.png'))
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = True
    tools.import_asset_tasks([task])
    tex = u.load_asset(path)
    assert tex, path
    tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_DEFAULT)
    tex.set_editor_property('srgb', True)
    eal.save_loaded_asset(tex)


def build(cfg=None):
    if cfg is None:
        raw = json.loads(CFG.read_text(encoding='utf-8'))
        cfg = {k: v['value'] for k, v in raw.items() if not k.startswith('_')}
    for name in ('T_LampFlame', 'T_LampSmoke', 'T_LampGlow'):
        _texture(name)
    color = u.LinearColor(*cfg['flame_colour'], 1)

    flame = _material('M_LampFlame', blend=u.BlendMode.BLEND_ADDITIVE)
    image = _sample(flame, 'T_LampFlame')
    tint = _node(flame, u.MaterialExpressionConstant3Vector, constant=color)
    energy = _mul(flame, _mul(flame, tint, _alpha(flame, image)),
                  _mul(flame, _flicker(flame, cfg), _constant(flame, 5)))
    _output(energy, u.MaterialProperty.MP_EMISSIVE_COLOR); _save(flame)

    halo = _material('M_LampHalo', blend=u.BlendMode.BLEND_ADDITIVE)
    image = _sample(halo, 'T_LampGlow')
    tint = _node(halo, u.MaterialExpressionConstant3Vector, constant=color)
    _output(_mul(halo, _mul(halo, _alpha(halo, image), tint), _mul(halo, _flicker(halo, cfg), _constant(halo, 1.4))),
            u.MaterialProperty.MP_EMISSIVE_COLOR); _save(halo)

    smoke = _material('M_LampSmoke', blend=u.BlendMode.BLEND_TRANSLUCENT)
    image = _sample(smoke, 'T_LampSmoke', pan=-.018)
    _output(_node(smoke, u.MaterialExpressionConstant3Vector, constant=u.LinearColor(.31, .30, .27, 1)),
            u.MaterialProperty.MP_EMISSIVE_COLOR)
    _output(_mul(smoke, _alpha(smoke, image), _constant(smoke, cfg['smoke_opacity'])), u.MaterialProperty.MP_OPACITY)
    _save(smoke)

    ember = _material('M_LampEmber', blend=u.BlendMode.BLEND_ADDITIVE)
    image = _sample(ember, 'T_LampGlow')
    pulse = _mul(ember, _add(ember, _sin(ember, _time(ember, .11, .31)), _constant(ember, 1)),
                 _constant(ember, .5))
    rare = _node(ember, u.MaterialExpressionPower, const_exponent=16.0)
    _link(pulse, rare, 'Base')
    _output(_mul(ember, _mul(ember, _alpha(ember, image), rare), _node(ember, u.MaterialExpressionConstant3Vector,
                                  constant=u.LinearColor(2.5, .42, .05, 1))), u.MaterialProperty.MP_EMISSIVE_COLOR)
    _save(ember)

    firefly = _material('M_LampFirefly', blend=u.BlendMode.BLEND_ADDITIVE)
    image = _sample(firefly, 'T_LampGlow')
    rnd = _node(firefly, u.MaterialExpressionPerInstanceRandom)
    phase = _mul(firefly, rnd, _constant(firefly, 17.3))
    blink = _sin(firefly, _add(firefly, _time(firefly, .18), phase))
    blink = _mul(firefly, _add(firefly, blink, _constant(firefly, 1)), _constant(firefly, .5))
    gated = _node(firefly, u.MaterialExpressionPower, const_exponent=12.0)
    _link(blink, gated, 'Base')
    _output(_mul(firefly, _mul(firefly, _alpha(firefly, image), gated), _node(firefly, u.MaterialExpressionConstant3Vector,
             constant=u.LinearColor(.34, 1.0, .27, 1))), u.MaterialProperty.MP_EMISSIVE_COLOR)
    drift_x = _mul(firefly, _sin(firefly, _add(firefly, _time(firefly, .07), phase)), _constant(firefly, 35))
    drift_y = _mul(firefly, _sin(firefly, _add(firefly, _time(firefly, .093), phase)), _constant(firefly, 28))
    drift_z = _mul(firefly, _sin(firefly, _add(firefly, _time(firefly, .052), phase)), _constant(firefly, 16))
    xy = _node(firefly, u.MaterialExpressionAppendVector)
    _link(drift_x, xy, 'A'); _link(drift_y, xy, 'B')
    xyz = _node(firefly, u.MaterialExpressionAppendVector)
    _link(xy, xyz, 'A'); _link(drift_z, xyz, 'B')
    _output(xyz, u.MaterialProperty.MP_WORLD_POSITION_OFFSET); _save(firefly)

    light = _material('M_LampLightFunction', domain=u.MaterialDomain.MD_LIGHT_FUNCTION)
    _output(_flicker(light, cfg), u.MaterialProperty.MP_EMISSIVE_COLOR); _save(light)
    return {name: u.load_asset(DEST + '/' + name) for name in
            ('M_LampFlame', 'M_LampHalo', 'M_LampSmoke', 'M_LampEmber', 'M_LampFirefly', 'M_LampLightFunction')}
