"""GFX-01 WP4: reactive foliage. Creates /Game/Frontier/Materials/MPC_Interaction (vector PlayerPosition, scalar Radius) and
/Game/Frontier/Materials/MF_PlayerInteraction (WPO push away from the player), adds the push to the grass, fern, herb and shrub materials,
and fixes the shrub/sapling leaf look. Idempotent. WP7's C++ writes PlayerPosition each tick.
py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/NatureFamilies/foliage_interaction.py

Push (cm, world): d = horizontal distance from each vertex to the player; push = normalize(vertex - player) * saturate(1 - d / Radius)^2
* PushCm * saturate(local height / HeightCm), so bases stay planted. Clamped by Radius, so the cost stays local to the player.
"""
import unreal as u

mel, eal, tools = u.MaterialEditingLibrary, u.EditorAssetLibrary, u.AssetToolsHelpers.get_asset_tools()
DIR = '/Game/Frontier/Materials'
NATURE = '/Game/Frontier/Environment/Greenlands/Nature/Materials'


def node(m, cls, x, y, **kw):
    e = mel.create_material_expression(m, cls, x, y)
    for k, v in kw.items():
        e.set_editor_property(k, v)
    return e


def link(a, out, b, inp):
    assert mel.connect_material_expressions(a, out, b, inp), (a, out, b, inp)


def mpc():
    path = DIR + '/MPC_Interaction'
    if eal.does_asset_exist(path):
        return u.load_asset(path)
    c = tools.create_asset('MPC_Interaction', DIR, u.MaterialParameterCollection, u.MaterialParameterCollectionFactoryNew())
    v, s = u.CollectionVectorParameter(), u.CollectionScalarParameter()
    v.set_editor_property('parameter_name', 'PlayerPosition'); v.set_editor_property('default_value', u.LinearColor(0, 0, -1e7, 0))  # far below the world: no push until the player writes it
    s.set_editor_property('parameter_name', 'Radius'); s.set_editor_property('default_value', 120.0)
    c.set_editor_property('vector_parameters', [v]); c.set_editor_property('scalar_parameters', [s])
    assert eal.save_loaded_asset(c)
    return c


def function(collection):
    path = DIR + '/MF_PlayerInteraction'
    f = u.load_asset(path) if eal.does_asset_exist(path) else None
    if f:
        mel.delete_all_material_expressions_in_function(f)  # rebuilt in place on every run, so materials that already call it keep working
    return _build_function(collection, f)


def _build_function(collection, f=None):
    f = f or tools.create_asset('MF_PlayerInteraction', DIR, u.MaterialFunction, u.MaterialFunctionFactoryNew())
    mfl = u.MaterialEditingLibrary
    def n(cls, x, y, **kw):
        e = mfl.create_material_expression_in_function(f, cls, x, y)
        for k, v in kw.items():
            e.set_editor_property(k, v)
        return e
    def l(a, o, b, i):
        assert mfl.connect_material_expressions(a, o, b, i), (a, o, b, i)
    pos = n(u.MaterialExpressionCollectionParameter, -1400, 0, collection=collection, parameter_name='PlayerPosition')
    rad = n(u.MaterialExpressionCollectionParameter, -1400, 200, collection=collection, parameter_name='Radius')
    origin = n(u.MaterialExpressionWorldPosition, -1400, 400)  # per vertex, so a big shrub bends where the player stands in it, not by its distance from its origin
    pos3 = n(u.MaterialExpressionComponentMask, -1280, 0, r=True, g=True, b=True, a=False); l(pos, '', pos3, '')  # the collection vector is float4
    delta = n(u.MaterialExpressionSubtract, -1150, 100); l(origin, '', delta, 'A'); l(pos3, '', delta, 'B')
    flat = n(u.MaterialExpressionMultiply, -1000, 100); l(delta, '', flat, 'A'); l(n(u.MaterialExpressionConstant3Vector, -1150, 250, constant=u.LinearColor(1, 1, 0, 0)), '', flat, 'B')
    dist = n(u.MaterialExpressionLength if hasattr(u, 'MaterialExpressionLength') else u.MaterialExpressionDistance, -850, 100)
    l(flat, '', dist, '')
    ratio = n(u.MaterialExpressionDivide, -700, 100); l(dist, '', ratio, 'A'); l(rad, '', ratio, 'B')
    inv = n(u.MaterialExpressionOneMinus, -560, 100); l(ratio, '', inv, '')
    sat = n(u.MaterialExpressionSaturate, -430, 100); l(inv, '', sat, '')
    fall = n(u.MaterialExpressionMultiply, -300, 100); l(sat, '', fall, 'A'); l(sat, '', fall, 'B')
    push_cm = n(u.MaterialExpressionScalarParameter, -300, 250, parameter_name='PushCm', default_value=45.0)
    height_cm = n(u.MaterialExpressionScalarParameter, -300, 350, parameter_name='PushHeightCm', default_value=60.0)
    lz = n(u.MaterialExpressionComponentMask, -300, 450, r=False, g=False, b=True, a=False); l(n(u.MaterialExpressionLocalPosition, -450, 450), '', lz, '')
    hw = n(u.MaterialExpressionDivide, -150, 450); l(lz, '', hw, 'A'); l(height_cm, '', hw, 'B')
    hws = n(u.MaterialExpressionSaturate, -20, 450); l(hw, '', hws, '')
    dirn = n(u.MaterialExpressionNormalize, -850, 250); l(flat, '', dirn, '')
    amp = n(u.MaterialExpressionMultiply, 0, 150); l(fall, '', amp, 'A'); l(push_cm, '', amp, 'B')
    amp2 = n(u.MaterialExpressionMultiply, 130, 150); l(amp, '', amp2, 'A'); l(hws, '', amp2, 'B')
    out = n(u.MaterialExpressionMultiply, 260, 150); l(dirn, '', out, 'A'); l(amp2, '', out, 'B')
    fo = n(u.MaterialExpressionFunctionOutput, 420, 150, output_name='Offset'); l(out, '', fo, '')
    assert eal.save_loaded_asset(f)
    return f


def add_push(m, fn):
    """Adds MF_PlayerInteraction's offset to the material's WPO (creating the WPO if none). Skips a material that already has it."""
    for e in mel.get_material_expressions(m) if hasattr(mel, 'get_material_expressions') else []:
        if isinstance(e, u.MaterialExpressionMaterialFunctionCall) and e.get_editor_property('material_function') == fn:
            return False
    call = node(m, u.MaterialExpressionMaterialFunctionCall, -300, 4000, material_function=fn)
    existing = mel.get_material_property_input_node(m, u.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    if existing:
        add = node(m, u.MaterialExpressionAdd, -100, 4000)
        out_name = mel.get_material_property_input_node_output_name(m, u.MaterialProperty.MP_WORLD_POSITION_OFFSET)
        link(existing, out_name, add, 'A'); link(call, '', add, 'B')
        call = add
    mel.connect_material_property(call, '', u.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    m.set_editor_property('used_with_instanced_static_meshes', True)
    mel.recompile_material(m)
    return eal.save_loaded_asset(m)


def master_of(mat):
    while isinstance(mat, u.MaterialInstance):
        mat = mat.get_editor_property('parent')
    return mat


def masters_for(mesh_names):
    ar = u.AssetRegistryHelpers.get_asset_registry()
    flt = u.ARFilter(class_paths=[u.TopLevelAssetPath('/Script/Engine', 'StaticMesh')], package_paths=['/Game/Frontier'], recursive_paths=True)
    found = {str(a.asset_name): a for a in ar.get_assets(flt) if str(a.asset_name) in mesh_names}
    out = {}
    for name, a in found.items():
        for sm in a.get_asset().static_materials:
            mm = master_of(sm.material_interface)
            if mm and isinstance(mm, u.Material):
                out[mm.get_path_name()] = mm
    return out


if __name__ == '__main__':
    import json
    from pathlib import Path
    P = json.loads((Path(__file__).resolve().parents[1] / 'greenlands-look-profile.json').read_text(encoding='utf-8'))
    wanted = {m for l in P['layers'] if l['role'] in ('grass_clump', 'fern', 'herb_broadleaf', 'flower_small', 'shrub_broadleaf', 'sapling_broadleaf') for m in l['have']}
    fn = function(mpc())
    done = {}
    for path, m in masters_for(wanted).items():
        if m.get_name() in ('M_GN_Rock', 'M_GN_Wood'):
            continue  # rocks and trunks must not bend
        done[path] = add_push(m, fn)
    # shrub and sapling leaves: the shared leaf master's blend darkened the shrub (vertex colour into both base and tint); drop it and use the bright young-leaf tint
    for name, tint, sat in (('MI_ShrubBroadleaf_Leaf', (1.08, 1.18, 0.78), 0.8), ('MI_SaplingBroadleaf_Leaf', (1.35, 1.6, 0.7), 1.05)):
        mi = u.load_asset(f'{NATURE}/{name}')
        mel.set_material_instance_scalar_parameter_value(mi, 'UseBlend', 0.0)
        for p in ('TintA', 'TintB'):
            mel.set_material_instance_vector_parameter_value(mi, p, u.LinearColor(*tint, 1.0))
        for p in ('SatA', 'SatB'):
            mel.set_material_instance_scalar_parameter_value(mi, p, sat)
        eal.save_loaded_asset(mi)
    u.log('FOLIAGE_INTERACTION ' + json.dumps(done))
