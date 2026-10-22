"""Author only /Game/Frontier/VFX/StreetLamp assets in the open full editor.

Called by Greenlands/lamp_fx.py; safe to rerun. NS_ChimneySmoke's emitter is authored by build_chimney_niagara.py.
"""
from pathlib import Path
import json
import math
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


def _sub(mat, a, b):
    n = _node(mat, u.MaterialExpressionSubtract)
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


def _night(mat, cfg):
    """M_SB_WindowGlow's sun gate (WP6h review 3c): 0 with the sun above SinBegins, 1 below SinFull, linear between, so lamp
    lights, lamp glass and windows switch together. lit = always (dev override) returns a constant 1."""
    if cfg['lit'] != 'time-of-day':
        return _constant(mat, 1.)
    w = json.loads((HERE.parent.parent / 'Silverbrook' / 'silverbrook-props.json').read_text(encoding='utf-8'))['windows']
    hi = _node(mat, u.MaterialExpressionScalarParameter, parameter_name='SinBegins', default_value=math.sin(math.radians(w['glow_begins_sun_elevation_deg']['value'])))
    lo = _node(mat, u.MaterialExpressionScalarParameter, parameter_name='SinFull', default_value=math.sin(math.radians(w['glow_full_sun_elevation_deg']['value'])))
    sun = _node(mat, u.MaterialExpressionSkyAtmosphereLightDirection, light_index=0)
    z = _node(mat, u.MaterialExpressionComponentMask, r=False, g=False, b=True, a=False); _link(sun, z, '')   # sin(sun elevation)
    q = _node(mat, u.MaterialExpressionDivide); _link(_sub(mat, hi, z), q, 'A'); _link(_sub(mat, hi, lo), q, 'B')
    return _link(q, _node(mat, u.MaterialExpressionSaturate), '')


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
    if name == 'T_ChimneyPuffs':   # WP6g review: luminance is density; clamped cells, effects LOD group
        tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_GRAYSCALE)
        tex.set_editor_property('srgb', False)
        tex.set_editor_property('address_x', u.TextureAddress.TA_CLAMP)
        tex.set_editor_property('address_y', u.TextureAddress.TA_CLAMP)
        tex.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_EFFECTS)
    else:
        tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_DEFAULT)
        tex.set_editor_property('srgb', True)
    eal.save_loaded_asset(tex)


def build_chimney_sprite():
    """Lit camera-facing puff sprite for NS_ChimneySmoke (WP6g review): opacity = atlas density x particle alpha x Opacity x distance fade."""
    m = _material('M_ChimneySmokeSprite', blend=u.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('shading_model', u.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property('translucency_lighting_mode', u.TranslucencyLightingMode.TLM_VOLUMETRIC_PER_VERTEX_NON_DIRECTIONAL)
    m.set_editor_property('used_with_niagara_sprites', True)
    m.set_editor_property('two_sided', False)
    puff = _node(m, u.MaterialExpressionParticleSubUV, texture=u.load_asset(DEST + '/T_ChimneyPuffs'))   # SubImageIndex picks the atlas cell
    pcol = _node(m, u.MaterialExpressionParticleColor)
    tint = _node(m, u.MaterialExpressionVectorParameter, parameter_name='Tint', default_value=u.LinearColor(.42, .42, .45, 1))
    _output(_mul(m, tint, pcol), u.MaterialProperty.MP_BASE_COLOR)                     # lit grey; ParticleColor.rgb stays 1
    opacity = _node(m, u.MaterialExpressionScalarParameter, parameter_name='Opacity', default_value=.55)
    depth = _node(m, u.MaterialExpressionPixelDepth)
    far = _node(m, u.MaterialExpressionSaturate)                                           # full to 80 m, gone at 120 m (= ET_ChimneySmoke Max Distance)
    _link(_mul(m, _add(m, _mul(m, depth, _constant(m, -1.)), _constant(m, 12000.)), _constant(m, 1 / 4000.)), far, '')
    density = _node(m, u.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False); _link(puff, density, '')
    dens_alpha = _node(m, u.MaterialExpressionMultiply)   # ParticleColor's default output is RGB only: take its A pin
    _link(density, dens_alpha, 'A'); _link(pcol, dens_alpha, 'B', 'A')
    _output(_mul(m, _mul(m, dens_alpha, opacity), far), u.MaterialProperty.MP_OPACITY)
    _save(m)
    return m


def build(cfg=None):
    if cfg is None:
        raw = json.loads(CFG.read_text(encoding='utf-8'))
        cfg = {k: v['value'] for k, v in raw.items() if not k.startswith('_')}
    for name in ('T_LampFlame', 'T_LampSmoke', 'T_LampGlow', 'T_ChimneyPuffs'):
        _texture(name)
    build_chimney_sprite()
    color = u.LinearColor(*cfg['flame_colour'], 1)

    # The imported window master is opaque, hiding every interior flame card.
    glass = _material('M_LampGlass', blend=u.BlendMode.BLEND_TRANSLUCENT)
    tint = _node(glass, u.MaterialExpressionConstant3Vector, constant=color)
    _output(_mul(glass, _mul(glass, tint, _constant(glass, cfg['glass_emission_strength'])), _night(glass, cfg)),
            u.MaterialProperty.MP_EMISSIVE_COLOR)
    _output(_constant(glass, .22), u.MaterialProperty.MP_OPACITY)
    _save(glass)

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
    # Lit, not emitted: smoke against a bright sky must read darker than the sky (WP6d review fix 1a).
    smoke.set_editor_property('shading_model', u.MaterialShadingModel.MSM_DEFAULT_LIT)
    smoke.set_editor_property('translucency_lighting_mode', u.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    # Lamp wisp only (chimneys use M_ChimneySmokeSprite / NS_ChimneySmoke). The chimney-card parameters below stay at their lamp defaults.
    image = _node(smoke, u.MaterialExpressionTextureSampleParameter2D, parameter_name='SmokeTex', texture=u.load_asset(DEST + '/T_LampSmoke'))
    uv = _node(smoke, u.MaterialExpressionTextureCoordinate)
    pan_v = _node(smoke, u.MaterialExpressionScalarParameter, parameter_name='PanV', default_value=-.018)
    speed = _node(smoke, u.MaterialExpressionAppendVector)
    _link(_constant(smoke, .012), speed, 'A'); _link(pan_v, speed, 'B')
    panner = _node(smoke, u.MaterialExpressionPanner)
    # Taper (WP6e review fix 1a): the card's V = 0 is at the pot (taken from the WP6e capture), so U is squeezed x1.6 at the
    # pot (V = 0) and x0.7 at the top (V = 1).
    vs = _node(smoke, u.MaterialExpressionComponentMask, r=False, g=True, b=False, a=False); _link(uv, vs, '')
    widen = _node(smoke, u.MaterialExpressionLinearInterpolate)
    _link(_constant(smoke, 1.6), widen, 'A'); _link(_constant(smoke, .7), widen, 'B'); _link(vs, widen, 'Alpha')
    thin = _node(smoke, u.MaterialExpressionLinearInterpolate)   # fix 1b: disperse, full at the pot, 45 % at the top
    _link(_constant(smoke, 1.), thin, 'A'); _link(_constant(smoke, .45), thin, 'B'); _link(vs, thin, 'Alpha')
    scale = _node(smoke, u.MaterialExpressionAppendVector); _link(widen, scale, 'A'); _link(_constant(smoke, 1), scale, 'B')
    centre = _node(smoke, u.MaterialExpressionConstant2Vector, r=.5, g=0.)
    tapered = _add(smoke, _mul(smoke, _sub(smoke, uv, centre), scale), centre)
    _link(tapered, panner, 'Coordinate'); _link(speed, panner, 'Speed'); _link(panner, image, 'UVs')
    tint = _node(smoke, u.MaterialExpressionVectorParameter, parameter_name='Tint', default_value=u.LinearColor(.31, .30, .27, 1))
    _output(tint, u.MaterialProperty.MP_BASE_COLOR)
    opacity = _node(smoke, u.MaterialExpressionScalarParameter, parameter_name='Opacity', default_value=cfg['smoke_opacity'])
    v = _node(smoke, u.MaterialExpressionComponentMask, r=False, g=True, b=False, a=False); _link(uv, v, '')
    foot = _node(smoke, u.MaterialExpressionSaturate); _link(_mul(smoke, v, _constant(smoke, 8.)), foot, '')   # 0 at the card's bottom edge, 1 from V = .125 (75 cm): hides the cut, keeps the pot end dense
    head = _node(smoke, u.MaterialExpressionSaturate); _link(_mul(smoke, _sub(smoke, _constant(smoke, 1.), v), _constant(smoke, 5.)), head, '')   # 1 to V = .8, 0 at the top edge
    ends = _mul(smoke, foot, head)   # WP6f review fix 1a: densest at the pot; `thin` does the dispersal
    fade = _node(smoke, u.MaterialExpressionLinearInterpolate)
    _link(_constant(smoke, 1), fade, 'A'); _link(ends, fade, 'B')
    _link(_node(smoke, u.MaterialExpressionScalarParameter, parameter_name='EdgeFade', default_value=0.), fade, 'Alpha')
    # ViewFade (chimney cards only): an edge-on card is invisible, 0 within 15 deg of edge-on, 1 beyond ~37 deg.
    cam = _node(smoke, u.MaterialExpressionCameraVectorWS); nrm = _node(smoke, u.MaterialExpressionVertexNormalWS)
    dot = _node(smoke, u.MaterialExpressionDotProduct); _link(cam, dot, 'A'); _link(nrm, dot, 'B')
    facing = _node(smoke, u.MaterialExpressionAbs); _link(dot, facing, '')
    view = _node(smoke, u.MaterialExpressionSaturate)
    _link(_mul(smoke, _add(smoke, facing, _constant(smoke, -.25)), _constant(smoke, 1.8)), view, '')
    vf = _node(smoke, u.MaterialExpressionLinearInterpolate); _link(_constant(smoke, 1), vf, 'A'); _link(view, vf, 'B')
    _link(_node(smoke, u.MaterialExpressionScalarParameter, parameter_name='ViewFade', default_value=0.), vf, 'Alpha')
    # Distance fade (fix 1f): full to 80 m, gone at 120 m, so back-row cards do not stack into a picket of streaks.
    depth = _node(smoke, u.MaterialExpressionPixelDepth)
    far = _node(smoke, u.MaterialExpressionSaturate)
    _link(_mul(smoke, _add(smoke, _mul(smoke, depth, _constant(smoke, -1.)), _constant(smoke, 12000.)), _constant(smoke, 1 / 4000.)), far, '')
    # Card side edges at zero on the card itself: the panner drifts U, so the texture's zero edge columns do not stay there.
    cu = _node(smoke, u.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False); _link(uv, cu, '')
    off = _node(smoke, u.MaterialExpressionAbs); _link(_add(smoke, cu, _constant(smoke, -.5)), off, '')
    sides = _node(smoke, u.MaterialExpressionSaturate)
    _link(_mul(smoke, _sub(smoke, _constant(smoke, .5), off), _constant(smoke, 10.)), sides, '')
    _output(_mul(smoke, _mul(smoke, _mul(smoke, _mul(smoke, _mul(smoke, _alpha(smoke, image), opacity), fade), vf), far), _mul(smoke, sides, thin)),
            u.MaterialProperty.MP_OPACITY)
    _save(smoke)

    firefly = _material('M_LampFirefly', blend=u.BlendMode.BLEND_ADDITIVE)
    image = _sample(firefly, 'T_LampGlow')
    rnd = _node(firefly, u.MaterialExpressionPerInstanceRandom)
    phase = _mul(firefly, rnd, _constant(firefly, 17.3))
    blink = _sin(firefly, _add(firefly, _time(firefly, .18), phase))
    blink = _mul(firefly, _add(firefly, blink, _constant(firefly, 1)), _constant(firefly, .5))
    gated = _node(firefly, u.MaterialExpressionPower, const_exponent=12.0)
    _link(blink, gated, 'Base')
    sun = _node(firefly, u.MaterialExpressionSkyAtmosphereLightDirection, light_index=0)
    height = _node(firefly, u.MaterialExpressionComponentMask, r=False, g=False, b=True, a=False)
    _link(sun, height, '')
    darkness = _node(firefly, u.MaterialExpressionSaturate)
    _link(_mul(firefly, _add(firefly, _constant(firefly, .07), _mul(firefly, height, _constant(firefly, -1))),
                       _constant(firefly, 8)), darkness, '')
    dot = _mul(firefly, _mul(firefly, _alpha(firefly, image), gated), darkness)
    _output(_mul(firefly, dot, _node(firefly, u.MaterialExpressionConstant3Vector,
             constant=u.LinearColor(.7, 1.0, .32, 1))), u.MaterialProperty.MP_EMISSIVE_COLOR)
    drift_x = _mul(firefly, _sin(firefly, _add(firefly, _time(firefly, .07), phase)), _constant(firefly, 20))
    drift_y = _mul(firefly, _sin(firefly, _add(firefly, _time(firefly, .093), phase)), _constant(firefly, 20))
    drift_z = _mul(firefly, _sin(firefly, _add(firefly, _time(firefly, .052), phase)), _constant(firefly, 10))
    xy = _node(firefly, u.MaterialExpressionAppendVector)
    _link(drift_x, xy, 'A'); _link(drift_y, xy, 'B')
    xyz = _node(firefly, u.MaterialExpressionAppendVector)
    _link(xy, xyz, 'A'); _link(drift_z, xyz, 'B')
    _output(xyz, u.MaterialProperty.MP_WORLD_POSITION_OFFSET); _save(firefly)

    light = _material('M_LampLightFunction', domain=u.MaterialDomain.MD_LIGHT_FUNCTION)
    _output(_mul(light, _flicker(light, cfg), _night(light, cfg)), u.MaterialProperty.MP_EMISSIVE_COLOR); _save(light)
    return {name: u.load_asset(DEST + '/' + name) for name in
            ('M_LampGlass', 'M_LampFlame', 'M_LampHalo', 'M_LampSmoke', 'M_LampFirefly', 'M_LampLightFunction')}
