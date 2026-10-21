"""GFX-01 WP4: add clamped SimpleGrassWind to M_GN_Leaf's World Position Offset. Idempotent (skips when WindMaxCm exists).
py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/NatureFamilies/add_leaf_wind.py

VSM cost grows with WPO, so the offset is clamped to +-WindMaxCm (default 6 cm) and scaled by WindIntensity; pcg_dressing.py also
turns WPO off beyond WPO_DISABLE_CM on every cosmetic instance. Per-leaf weight is the mesh's local height (0 at the base, 1 at WindHeightCm).
"""
import unreal as u

PATH = '/Game/Frontier/Environment/Greenlands/Nature/Materials/M_GN_Leaf'
mel, eal = u.MaterialEditingLibrary, u.EditorAssetLibrary
m = u.load_asset(PATH)
assert m, PATH
WIND_FN = '/Engine/Functions/Engine_MaterialFunctions01/WorldPositionOffset/SimpleGrassWind'


def feed_additional_wpo():
    """SimpleGrassWind needs its AdditionalWPO input fed (else the whole material fails to compile and renders as the default material)."""
    for e in mel.get_material_expressions(m):
        if isinstance(e, u.MaterialExpressionMaterialFunctionCall) and e.get_editor_property('material_function') == u.load_asset(WIND_FN):
            zero = mel.create_material_expression(m, u.MaterialExpressionConstant3Vector, -800, 3500)
            assert mel.connect_material_expressions(zero, '', e, 'AdditionalWPO')


if 'WindMaxCm' in [str(n) for n in mel.get_scalar_parameter_names(m)]:
    feed_additional_wpo()
    mel.recompile_material(m)
    assert eal.save_loaded_asset(m)
    u.log('LEAF_WIND already present; AdditionalWPO fed')
else:
    def node(cls, x, y, **kw):
        e = mel.create_material_expression(m, cls, x, y)
        for k, v in kw.items():
            e.set_editor_property(k, v)
        return e

    def link(a, out, b, inp):
        assert mel.connect_material_expressions(a, out, b, inp), (a, out, b, inp)

    sc = lambda n, d, y: node(u.MaterialExpressionScalarParameter, -1400, y, parameter_name=n, default_value=d)
    intensity, max_cm, height = sc('WindIntensity', 0.6, 3000), sc('WindMaxCm', 6.0, 3100), sc('WindHeightCm', 400.0, 3200)
    # weight = saturate(local height / WindHeightCm): crowns move, trunks and bases stay put
    z = node(u.MaterialExpressionComponentMask, -1200, 3300, r=False, g=False, b=True, a=False)
    link(node(u.MaterialExpressionLocalPosition, -1400, 3300), '', z, '')
    div = node(u.MaterialExpressionDivide, -1000, 3300)
    link(z, '', div, 'A'); link(height, '', div, 'B')
    weight = node(u.MaterialExpressionSaturate, -850, 3300)
    link(div, '', weight, '')
    wind = node(u.MaterialExpressionMaterialFunctionCall, -650, 3200, material_function=u.load_asset('/Engine/Functions/Engine_MaterialFunctions01/WorldPositionOffset/SimpleGrassWind'))
    u.log('WIND_PINS ' + str([str(getattr(i, 'name', i)) for i in mel.get_inputs_for_material_expression(m, wind)]))
    link(intensity, '', wind, 'WindIntensity'); link(weight, '', wind, 'WindWeight')
    link(node(u.MaterialExpressionConstant3Vector, -800, 3500), '', wind, 'AdditionalWPO')
    neg = node(u.MaterialExpressionMultiply, -1200, 3100, const_b=-1.0)
    link(max_cm, '', neg, 'A')
    clamp = node(u.MaterialExpressionClamp, -450, 3200)  # the wind distance clamp: +-WindMaxCm per axis
    link(wind, '', clamp, '')
    link(neg, '', clamp, 'Min'); link(max_cm, '', clamp, 'Max')
    mel.connect_material_property(clamp, '', u.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    mel.recompile_material(m)
    assert eal.save_loaded_asset(m)
    u.log('LEAF_WIND added')
