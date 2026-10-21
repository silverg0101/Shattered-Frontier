"""Silverbrook ground, road and river materials (full editor; run with tools/Run-InEditor.py).

py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/NatureFamilies/import_ground.py

Reuses the textures import_terrain.py imported; run that first. Writes /Game/Frontier/Environment/Greenlands/Nature/Materials:
  M_GN_Ground  -> MI_GN_Ground  forest floor (+ moss patches from a mask) blended to triplanar bedrock by vertex colour R (slope),
                                world-space macro noise on top; build_silverbrook.py paints the colour from the terrain slope.
  M_GN_Surface -> MI_GN_Road, MI_GN_Trail  flat, world-aligned road and trail strips with macro noise.
  M_GN_River   -> MI_GN_River   Single Layer Water, two panning normal layers.
Values below are material defaults; MI_* instances override them.
"""
import unreal as u
LIB = '/Game/Frontier/Environment/Greenlands/Nature'
ARR = '/Game/Frontier/Environment/Greenlands/Arrival/Textures'
tools = u.AssetToolsHelpers.get_asset_tools(); mel = u.MaterialEditingLibrary; eal = u.EditorAssetLibrary
S = u.MaterialSamplerType; WRAP = u.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS


def tex(path):
    t = u.load_asset(path); assert t, path; return t



def import_scan(asset, channels=('Diffuse', 'nor_gl', 'Rough')):
    """Import a Poly Haven scan from Sources/<asset>/ as T_GN_<asset>_<channel> (same texture settings as import_terrain.py)."""
    from pathlib import Path
    src = Path(__file__).resolve().parent / 'Sources' / asset
    for ch in channels:
        name = f'T_GN_{asset}_{ch}'; path = f'{LIB}/Textures/{name}'
        if not eal.does_asset_exist(path):
            t = u.AssetImportTask(); t.filename = str(src / f'{asset}_{ch}.jpg'); t.destination_path = LIB + '/Textures'; t.destination_name = name
            t.automated = t.save = t.replace_existing = True; tools.import_asset_tasks([t])
        a = u.load_asset(path); assert a, path
        if ch == 'nor_gl':
            a.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_NORMALMAP); a.set_editor_property('srgb', False); a.set_editor_property('flip_green_channel', True)
        elif ch == 'Rough':
            a.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_MASKS); a.set_editor_property('srgb', False)
        eal.save_loaded_asset(a)

def node(m, cls, x, y, **kw):
    e = mel.create_material_expression(m, cls, x, y)
    for k, v in kw.items(): e.set_editor_property(k, v)
    return e


def link(a, out, b, inp): assert mel.connect_material_expressions(a, out, b, inp), (a, out, b, inp)


def new(name, cls, factory):
    path = f'{LIB}/Materials/{name}'
    return u.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(name, LIB + '/Materials', cls, factory)


def master(name):
    m = new(name, u.Material, u.MaterialFactoryNew()); mel.delete_all_material_expressions(m); return m


class G:
    """Graph helpers bound to one material."""
    def __init__(s, m): s.m = m
    def sc(s, n, d, x, y): return node(s.m, u.MaterialExpressionScalarParameter, x, y, parameter_name=n, default_value=d)
    def vec(s, n, d, x, y): return node(s.m, u.MaterialExpressionVectorParameter, x, y, parameter_name=n, default_value=u.LinearColor(*d, 1))
    def op(s, cls, a, b, x, y, ao='', bo=''):
        n = node(s.m, cls, x, y); link(a, ao, n, 'A'); link(b, bo, n, 'B'); return n
    def mul(s, a, b, x, y, **o): return s.op(u.MaterialExpressionMultiply, a, b, x, y, **o)
    def lerp(s, a, b, w, x, y, ao='', bo=''):
        n = s.op(u.MaterialExpressionLinearInterpolate, a, b, x, y, ao, bo); link(w, '', n, 'Alpha'); return n
    def mask(s, src, r, g, b, x, y):
        c = node(s.m, u.MaterialExpressionComponentMask, x, y, r=r, g=g, b=b, a=False); link(src, '', c, ''); return c
    def uv(s, xy, tile, x, y):
        d = node(s.m, u.MaterialExpressionDivide, x, y); link(xy, '', d, 'A'); link(tile, '', d, 'B'); return d
    def smp(s, name, t, kind, uv, x, y):
        e = node(s.m, u.MaterialExpressionTextureSampleParameter2D, x, y, parameter_name=name, texture=t, sampler_type=kind, sampler_source=WRAP)
        link(uv, '', e, 'UVs'); return e
    def macro(s, xy, strength, x, y):
        """Brightness factor 1 +- strength from a 53 m world-space tiling of the forest-floor height mask."""
        t = s.smp('Macro', tex(f'{LIB}/Textures/T_GN_forest_ground_04_Displacement'), S.SAMPLERTYPE_MASKS,
                  s.uv(xy, s.sc('MacroTileCm', 5300, x - 400, y + 100), x - 200, y), x, y)
        n = s.mask(t, True, False, False, x + 150, y)
        k = s.sc('MacroAmount', strength, x + 150, y + 120); c = node(s.m, u.MaterialExpressionConstant, x + 150, y + 200, r=0.5)
        d = s.op(u.MaterialExpressionSubtract, n, c, x + 300, y); f = s.mul(d, k, x + 450, y)
        one = node(s.m, u.MaterialExpressionConstant, x + 450, y + 120, r=1.0)
        return n, s.op(u.MaterialExpressionAdd, one, f, x + 600, y)


def ground():
    m = master('M_GN_Ground'); g = G(m)
    xy = g.mask(node(m, u.MaterialExpressionWorldPosition, -2400, 0), True, True, False, -2250, 0)
    # triplanar weights from the vertex normal, |n|^4 normalised: z = top, x/y = side faces
    ab = node(m, u.MaterialExpressionAbs, -2250, 700); link(node(m, u.MaterialExpressionVertexNormalWS, -2400, 700), '', ab, '')
    pw = node(m, u.MaterialExpressionPower, -2100, 700, const_exponent=4.0); link(ab, '', pw, 'Base')
    c = [g.mask(pw, *a, -1950, 650 + i * 100) for i, a in enumerate(((1, 0, 0), (0, 1, 0), (0, 0, 1)))]
    tot = g.op(u.MaterialExpressionAdd, g.op(u.MaterialExpressionAdd, c[0], c[1], -1800, 700), c[2], -1650, 700)
    W = [g.op(u.MaterialExpressionDivide, e, tot, -1500, 700 + i * 100) for i, e in enumerate(c)]  # x, y, z
    wp = node(m, u.MaterialExpressionWorldPosition, -2400, 300)
    px, py, pz = (g.mask(wp, *a, -2250, 300 + i * 100) for i, a in enumerate(((1, 0, 0), (0, 1, 0), (0, 0, 1))))
    yz = node(m, u.MaterialExpressionAppendVector, -2100, 300); link(py, '', yz, 'A'); link(pz, '', yz, 'B')
    xz = node(m, u.MaterialExpressionAppendVector, -2100, 400); link(px, '', xz, 'A'); link(pz, '', xz, 'B')

    def layer(p, base, nor, rough, tile_cm, tint, y):
        t = g.sc('TileCm' + p, tile_cm, -1800, y + 150)
        uv = g.uv(xy, t, -1650, y)
        col = g.smp('Base' + p, tex(base), S.SAMPLERTYPE_COLOR, uv, -1100, y)
        n = g.smp('Normal' + p, tex(nor), S.SAMPLERTYPE_NORMAL, uv, -1100, y + 250)
        r = g.mask(g.smp('Rough' + p, tex(rough), S.SAMPLERTYPE_MASKS, uv, -1100, y + 500), True, False, False, -800, y + 500)
        return g.mul(col, g.vec('Tint' + p, tint, -900, y - 100), -500, y), n, r, t
    T = f'{LIB}/Textures/T_GN_'
    fa, na, ra, _ = layer('A', T + 'forest_ground_04_Diffuse', T + 'forest_ground_04_nor_gl', T + 'forest_ground_04_Rough', 200, (0.50, 0.66, 0.34), 0)
    fb, nb, rb, _ = layer('B', T + 'mossy_rock_Diffuse', T + 'mossy_rock_nor_gl', T + 'mossy_rock_Rough', 150, (0.34, 0.46, 0.20), 1200)
    mk = g.mask(g.smp('Mask', tex(T + 'forest_ground_04_Displacement'), S.SAMPLERTYPE_MASKS, g.uv(xy, g.sc('TileCmMask', 900, -1800, 2700), -1650, 2600), -1100, 2600), True, False, False, -800, 2600)
    wm = node(m, u.MaterialExpressionSaturate, 0, 2600)
    sub = g.op(u.MaterialExpressionSubtract, mk, g.sc('Low', 0.4, -600, 2700), -500, 2600)
    link(g.mul(sub, g.sc('InvRange', 32, -600, 2800), -300, 2600), '', wm, '')
    forest = g.lerp(fa, fb, wm, 200, 0); fn = g.lerp(na, nb, wm, 200, 250, 'RGB', 'RGB'); fr = g.lerp(ra, rb, wm, 200, 500)

    # bedrock: triplanar colour/roughness, top-projected normal
    def tri(name, path, kind, ymid):
        smp = []
        for i, a in enumerate((yz, xz, xy)):  # x-facing, y-facing, top
            smp.append(g.smp(name + 'XYZ'[i], tex(path), kind, g.uv(a, g.sc('TileCmC', 200, -1800, ymid), -1650, ymid + i * 60), -1100, ymid + i * 220))
        acc = None
        for s_, w_ in zip(smp, W):
            mu = g.mul(s_, w_, -900, ymid, ao='RGB' if kind == S.SAMPLERTYPE_COLOR else '')
            acc = mu if acc is None else g.op(u.MaterialExpressionAdd, acc, mu, -780, ymid)
        return acc
    bc = g.mul(tri('BaseC', T + 'marble_rock_01_Diffuse', S.SAMPLERTYPE_COLOR, 3000), g.vec('TintC', (0.5, 0.52, 0.56), -900, 2900), -500, 3000)
    bn = g.smp('NormalC', tex(T + 'marble_rock_01_nor_gl'), S.SAMPLERTYPE_NORMAL, g.uv(xy, g.sc('TileCmC', 200, -1800, 3900), -1650, 3900), -1100, 3900)
    br = g.mask(g.smp('RoughC', tex(T + 'marble_rock_01_Rough'), S.SAMPLERTYPE_MASKS, g.uv(xy, g.sc('TileCmC', 200, -1800, 4200), -1650, 4200), -1100, 4200), True, False, False, -800, 4200)

    # slope weight: vertex colour R (0 flat .. 1 steep) pushed around by the macro noise, then sharpened
    vr = g.mask(node(m, u.MaterialExpressionVertexColor, -600, 5000), True, False, False, -400, 5000)
    mnoise, bright = g.macro(xy, 0.3, -600, 5300)
    # patchy forest floor: large-scale noise swaps green moss cast for dry leaf-litter brown, so open ground is not one olive sheet
    dry = node(m, u.MaterialExpressionSaturate, 0, 5600); link(g.mul(g.op(u.MaterialExpressionSubtract, mnoise, node(m, u.MaterialExpressionConstant, -300, 5600, r=0.3), -150, 5600), g.sc('DryContrast', 3, -150, 5700), 0, 5700), '', dry, '')
    forest = g.mul(forest, g.lerp(g.vec('GreenCast', (0.8, 1.05, 0.6), 0, 5800), g.vec('DryCast', (1.35, 1.0, 0.75), 0, 5900), dry, 150, 5800), 300, 5600)
    jitter = g.mul(g.op(u.MaterialExpressionSubtract, mnoise, node(m, u.MaterialExpressionConstant, -300, 5400, r=0.5), -150, 5300), g.sc('SlopeNoise', 0.5, -150, 5400), 0, 5300)
    half = node(m, u.MaterialExpressionConstant, 0, 5000, r=0.5)
    sh = g.mul(g.op(u.MaterialExpressionAdd, g.op(u.MaterialExpressionSubtract, vr, half, 150, 5000), jitter, 300, 5000), g.sc('SlopeSharp', 4, 150, 5100), 450, 5000)
    s = node(m, u.MaterialExpressionSaturate, 600, 5000); link(g.op(u.MaterialExpressionAdd, sh, half, 600, 5100), '', s, '')
    col = g.mul(g.lerp(forest, bc, s, 900, 0), bright, 1100, 0)
    nrm = g.lerp(fn, bn, s, 900, 250, '', 'RGB'); rgh = g.lerp(fr, br, s, 900, 500)
    # roads and trails grown into the ground: vertex colour G (cobbled roads) / B (trails) is a distance field that is 0.5 at the road edge;
    # a fine noise moves the threshold so the edge is worn and ragged instead of a seam
    SB = '/Game/Frontier/Environment/Greenlands/Silverbrook/Textures/T_SB_cobblestone_pavement_'
    rc, rn, rr, _ = layer('R', SB + 'Diffuse', SB + 'nor_gl', SB + 'Rough', 240, (1, 1, 1), 6000)
    # trails: packed dark earth from the forest-floor scan with pale mossy stones pushed through it where a mask passes a high threshold
    import_scan('brown_mud_02'); import_scan('rocky_trail')  # dirt with roots and sticks; pale stones
    dc, dn, dr, _ = layer('T', T + 'brown_mud_02_Diffuse', T + 'brown_mud_02_nor_gl', T + 'brown_mud_02_Rough', 200, (0.85, 0.75, 0.65), 8000)
    sc_, sn, sr, _ = layer('S', T + 'rocky_trail_Diffuse', T + 'rocky_trail_nor_gl', T + 'rocky_trail_Rough', 150, (0.9, 0.88, 0.84), 9600)
    def clamp01(value, x, y):
        return g.op(u.MaterialExpressionMax,
                    g.op(u.MaterialExpressionMin, value, node(m, u.MaterialExpressionConstant, x - 150, y, r=1.0), x, y),
                    node(m, u.MaterialExpressionConstant, x - 150, y + 100, r=0.0), x + 150, y)
    sw = clamp01(g.mul(g.op(u.MaterialExpressionSubtract, mk, g.sc('StoneLow', 0.6, 300, 8000), 450, 8000),
                       g.sc('StoneSharp', 14, 300, 8100), 500, 8000), 600, 8000)
    tc, tn, tr = g.lerp(dc, sc_, sw, 800, 8000), g.lerp(dn, sn, sw, 800, 8250, '', 'RGB'), g.lerp(dr, sr, sw, 800, 8500)
    vcn = node(m, u.MaterialExpressionVertexColor, -600, 9000)
    edge = g.mask(g.smp('EdgeNoise', tex(T + 'forest_ground_04_Displacement'), S.SAMPLERTYPE_MASKS, g.uv(xy, g.sc('EdgeTileCm', 350, -1800, 9300), -1650, 9300), -1100, 9300), True, False, False, -800, 9300)
    wob = g.mul(g.op(u.MaterialExpressionSubtract, edge, half, -600, 9300), g.sc('EdgeAmp', 0.9, -600, 9400), -400, 9300)
    for ch, (c_, n_, r_) in ((('G', 0, 1, 0), (rc, rn, rr)), (('B', 0, 0, 1), (tc, tn, tr))):
        v = g.mask(vcn, *[bool(b) for b in ch[1:]], -400, 9000)
        w_ = node(m, u.MaterialExpressionSaturate, 400, 9000)
        edge_noise = wob if ch[0] == 'G' else g.mul(wob, g.sc('TrailEdgeNoise', 0.35, -200, 9200), 0, 9200)
        link(g.op(u.MaterialExpressionAdd, g.mul(g.op(u.MaterialExpressionAdd, g.op(u.MaterialExpressionSubtract, v, half, -200, 9000), edge_noise, 0, 9000), g.sc('EdgeSharp', 5, 0, 9100), 200, 9000), half, 300, 9000), '', w_, '')
        col = g.lerp(col, c_, w_, 1300, 0); nrm = g.lerp(nrm, n_, w_, 1300, 250); rgh = g.lerp(rgh, r_, w_, 1300, 500)
    # tilled fields: vertex alpha (0.5 at the field edge, same worn-edge blend); dark soil with crop rows, rows run along world X
    wa = clamp01(g.op(u.MaterialExpressionAdd, g.mul(g.op(u.MaterialExpressionAdd,
                 g.op(u.MaterialExpressionSubtract, vcn, half, -200, 10300, ao='A'), wob, 0, 10300),
                 g.sc('EdgeSharp', 5, 0, 10400), 200, 10300), half, 300, 10300), 400, 10300)
    rows = node(m, u.MaterialExpressionSine, 0, 10600, period=1.0)
    link(g.uv(py, g.sc('RowCm', 110, -300, 10600), -150, 10600), '', rows, '')
    stripe = clamp01(g.op(u.MaterialExpressionAdd, g.mul(rows, g.sc('RowContrast', 1.6, 150, 10700),
                    150, 10600), half, 300, 10700), 300, 10600)
    soil = g.mul(dc, g.vec('SoilTint', (0.55, 0.45, 0.36), 0, 10800), 400, 10800)
    crop = g.mul(forest, g.vec('CropTint', (0.7, 1.15, 0.55), 0, 10900), 400, 10900)
    col = g.lerp(col, g.lerp(soil, crop, stripe, 600, 10800), wa, 1500, 0)
    nrm = g.lerp(nrm, dn, wa, 1500, 250); rgh = g.lerp(rgh, dr, wa, 1500, 500)
    mel.connect_material_property(col, '', u.MaterialProperty.MP_BASE_COLOR); mel.connect_material_property(nrm, '', u.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(rgh, '', u.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m); eal.save_loaded_asset(m)
    mi = instance('MI_GN_Ground', m); eal.save_loaded_asset(mi); return mi


def surface():
    m = master('M_GN_Surface'); g = G(m)
    xy = g.mask(node(m, u.MaterialExpressionWorldPosition, -1400, 0), True, True, False, -1250, 0)
    uv = g.uv(xy, g.sc('TileCm', 200, -1250, 150), -1100, 0)
    col = g.smp('Base', tex(f'{ARR}/gravel_road_Diffuse'), S.SAMPLERTYPE_COLOR, uv, -800, 0)
    nor = g.smp('Normal', tex(f'{ARR}/gravel_road_nor_dx'), S.SAMPLERTYPE_NORMAL, uv, -800, 250)
    rgh = g.mask(g.smp('Rough', tex(f'{ARR}/grass_path_3_Rough'), S.SAMPLERTYPE_MASKS, uv, -800, 500), True, False, False, -500, 500)
    _, bright = g.macro(xy, 0.35, -800, 800)
    base = g.mul(g.mul(col, g.vec('Tint', (1, 1, 1), -500, -100), -300, 0), bright, -100, 0)
    mel.connect_material_property(base, '', u.MaterialProperty.MP_BASE_COLOR); mel.connect_material_property(nor, 'RGB', u.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(rgh, '', u.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m); eal.save_loaded_asset(m)
    for name, stem, tint, tile in (('MI_GN_Road', 'gravel_road', (0.9, 0.84, 0.72), 300), ('MI_GN_Trail', 'grass_path_3', (0.95, 0.9, 0.8), 250)):
        mi = instance(name, m)
        mel.set_material_instance_texture_parameter_value(mi, 'Base', tex(f'{ARR}/{stem}_Diffuse'))
        mel.set_material_instance_texture_parameter_value(mi, 'Normal', tex(f'{ARR}/{stem}_nor_dx'))
        mel.set_material_instance_scalar_parameter_value(mi, 'TileCm', tile)
        mel.set_material_instance_vector_parameter_value(mi, 'Tint', u.LinearColor(*tint, 1)); eal.save_loaded_asset(mi)


def river():
    m = master('M_GN_River'); g = G(m)
    m.set_editor_property('shading_model', u.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    xy = g.mask(node(m, u.MaterialExpressionWorldPosition, -1400, 0), True, True, False, -1250, 0)
    time = node(m, u.MaterialExpressionTime, -1250, 200)
    wn = tex('/Engine/Functions/Engine_MaterialFunctions02/ExampleContent/Textures/water_n')
    layers = []
    for i, (tile, vx, vy) in enumerate(((260, 0.020, 0.012), (170, -0.013, 0.026))):  # two scales drifting along the flow and across it
        t = g.sc(f'TileCm{i}', tile, -1250, 400 + i * 400); uv = g.uv(xy, t, -1100, 400 + i * 400)
        pan = node(m, u.MaterialExpressionPanner, -950, 400 + i * 400, speed_x=vx, speed_y=vy)
        link(uv, '', pan, 'Coordinate'); link(time, '', pan, 'Time')
        layers.append(g.smp(f'Normal{i}', wn, S.SAMPLERTYPE_NORMAL, pan, -700, 400 + i * 400))
    s = g.op(u.MaterialExpressionAdd, layers[0], layers[1], -450, 500, ao="RGB", bo="RGB")  # ponytail: plain sum of two unpacked normals, renormalised; fine for water ripples
    blend = node(m, u.MaterialExpressionNormalize, -300, 500); link(s, "", blend, "VectorInput")
    nrm = g.mul(blend, g.vec('NormalStrength', (1, 1, 1), -200, 700), -100, 500)
    scatter = g.vec('Scatter', (0.02, 0.07, 0.06), -400, 0)
    mel.connect_material_property(scatter, '', u.MaterialProperty.MP_BASE_COLOR); mel.connect_material_property(nrm, '', u.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(g.sc('Roughness', 0.08, -400, 150), '', u.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(g.sc('Specular', 0.5, -400, 220), '', u.MaterialProperty.MP_SPECULAR)
    for e in mel.get_material_expressions(m):  # delete_all_material_expressions leaves output nodes behind; an unlinked one fails the compile
        if isinstance(e, u.MaterialExpressionSingleLayerWaterMaterialOutput): mel.delete_material_expression(m, e)
    out = node(m, u.MaterialExpressionSingleLayerWaterMaterialOutput, 300, 0)
    link(g.vec('ScatteringCoefficients', (0.01, 0.04, 0.03), 0, 0), '', out, 'ScatteringCoefficients')
    link(g.vec('AbsorptionCoefficients', (0.25, 0.08, 0.06), 0, 100), '', out, 'AbsorptionCoefficients')
    mel.recompile_material(m); eal.save_loaded_asset(m)
    mi = instance('MI_GN_River', m); eal.save_loaded_asset(mi)


def instance(name, parent):
    mi = new(name, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew()); mel.set_material_instance_parent(mi, parent); return mi


ground(); surface(); river()
print('GROUND_IMPORT', [f'{LIB}/Materials/{n}' for n in ('MI_GN_Ground', 'MI_GN_Road', 'MI_GN_Trail', 'MI_GN_River')])
