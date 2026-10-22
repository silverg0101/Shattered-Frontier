"""Felling review r1 art fixes 6, 7 and 9, applied in place to /Game/Frontier/Gameplay/Harvest/MightyOakV1 (open editor).

6: SM_MightyOak_LowerTrunk <- ArtSource/HarvestOak/MightyOak-Felling-v1/SmoothLowerTrunk/SM_MightyOak_LowerTrunk.fbx
   (build it with that folder's tools/Build-SmoothLowerTrunk.py); keeps slots bark/cut wood and CPU access.
7: T_MightyOak_CutWood_BaseColor <- MightyOak-Felling-v1/Concept/endgrain/T_Oak_Endgrain_BaseColor.png (pale sapwood).
9: M_MightyOak_Foliage gets SubsurfaceColor (0.32, 0.46, 0.10) so shadow-side leaves are not black.
r2 fix 6: M_MightyOak_CutWood rebuilt in object space: end-grain rings on near-horizontal faces, torn fibres
   (Concept/splinter/T_Oak_Splinter_BaseColor.png -> T_MightyOak_Splinter_BaseColor) on splinter flanks.
Idempotent. py -3 tools/Run-InEditor.py tools/Update-HarvestOakFellingArt.py
"""
from pathlib import Path

import unreal as u

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'GameClient/ArtSource/HarvestOak/MightyOak-Felling-v1'
OAK = '/Game/Frontier/Gameplay/Harvest/MightyOakV1'
MEL = u.MaterialEditingLibrary


def reimport(source, folder, name, options=None):
    task = u.AssetImportTask()
    task.filename, task.destination_path, task.destination_name = str(source), folder, name
    task.automated, task.replace_existing, task.save = True, True, True
    if options:
        task.options = options
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset = u.load_asset(f'{folder}/{name}')
    assert asset, name
    return asset


# 6. Lower trunk: keep the existing material assignment and runtime CPU access.
lower_path = f'{OAK}/Meshes/SM_MightyOak_LowerTrunk'
lower = u.load_asset(lower_path)
slots = [s.get_editor_property('material_interface') for s in lower.get_editor_property('static_materials')]
ui = u.FbxImportUI()
ui.import_mesh, ui.import_materials, ui.import_textures, ui.import_as_skeletal = True, False, False, False
ui.static_mesh_import_data.combine_meshes = True
ui.static_mesh_import_data.auto_generate_collision = False
ui.static_mesh_import_data.import_uniform_scale = 1.0
lower = reimport(ART / 'SmoothLowerTrunk/SM_MightyOak_LowerTrunk.fbx', f'{OAK}/Meshes', 'SM_MightyOak_LowerTrunk', ui)
static = list(lower.get_editor_property('static_materials'))
assert len(static) == 2, len(static)
for slot, material in zip(static, slots):
    slot.set_editor_property('material_interface', material)
lower.set_editor_property('static_materials', static)
lower.set_editor_property('allow_cpu_access', True)
u.EditorAssetLibrary.save_loaded_asset(lower)

# 7. Cut-wood base colour.
tex = reimport(ART / 'Concept/endgrain/T_Oak_Endgrain_BaseColor.png', f'{OAK}/Textures', 'T_MightyOak_CutWood_BaseColor')
tex.set_editor_property('srgb', True)
u.EditorAssetLibrary.save_loaded_asset(tex)

# 9. Foliage subsurface colour.
foliage = u.load_asset(f'{OAK}/Materials/M_MightyOak_Foliage')
if not MEL.get_material_property_input_node(foliage, u.MaterialProperty.MP_SUBSURFACE_COLOR):
    sss = MEL.create_material_expression(foliage, u.MaterialExpressionConstant3Vector, -420, 300)
    sss.set_editor_property('constant', u.LinearColor(0.32, 0.46, 0.10, 1))
    MEL.connect_material_property(sss, '', u.MaterialProperty.MP_SUBSURFACE_COLOR)
foliage.set_editor_property('two_sided', True)
MEL.recompile_material(foliage)
u.EditorAssetLibrary.save_loaded_asset(foliage)
# r2 fix 6: cut wood in object space (the stump and the falling collar move, so world projection would swim):
# end-grain rings on faces within ~50 deg of the trunk axis, vertical torn fibres on steeper splinter flanks.
fibre = reimport(ART / 'Concept/splinter/T_Oak_Splinter_BaseColor.png', f'{OAK}/Textures', 'T_MightyOak_Splinter_BaseColor')
fibre.set_editor_property('srgb', True)
u.EditorAssetLibrary.save_loaded_asset(fibre)
wood = u.load_asset(f'{OAK}/Materials/M_MightyOak_CutWood')
MEL.delete_all_material_expressions(wood)
target = wood


def node(cls, x, y, **props):
    e = MEL.create_material_expression(target, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, a_out, b, b_in):
    assert MEL.connect_material_expressions(a, a_out, b, b_in), (a, a_out, b, b_in)


def object_space():
    p = node(u.MaterialExpressionTransformPosition, -1600, 0,
             transform_source_type=u.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
             transform_type=u.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    link(node(u.MaterialExpressionWorldPosition, -1800, 0), '', p, '')
    n = node(u.MaterialExpressionTransform, -1600, 300, transform_source_type=u.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,
             transform_type=u.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
    link(node(u.MaterialExpressionVertexNormalWS, -1800, 300), '', n, '')
    a = node(u.MaterialExpressionAbs, -1400, 300); link(n, '', a, '')
    return p, a


pos, absn = object_space()


def mask(src, x, y, r=False, g=False, b=False):
    m = node(u.MaterialExpressionComponentMask, x, y, r=r, g=g, b=b); link(src, '', m, ''); return m


def scaled_uv(src, offset, scale, x, y):
    s = node(u.MaterialExpressionSubtract, x, y); link(src, '', s, 'A')
    o = node(u.MaterialExpressionConstant2Vector, x - 150, y + 60, r=offset[0], g=offset[1]); link(o, '', s, 'B')
    d = node(u.MaterialExpressionDivide, x + 150, y, const_b=scale); link(s, '', d, 'A')
    return d


def sample(tex, uv, x, y):
    t = node(u.MaterialExpressionTextureSample, x, y, texture=tex); link(uv, '', t, 'UVs'); return t


rings = sample(u.load_asset(f'{OAK}/Textures/T_MightyOak_CutWood_BaseColor'), scaled_uv(mask(pos, -1400, -200, r=True, g=True), (-45, -62), 90.0, -1200, -200), -800, -200)
fib_x = sample(fibre, scaled_uv(mask(pos, -1400, 0, g=True, b=True), (0, 0), 40.0, -1200, 0), -800, 0)     # YZ plane, faces along X
fib_y = sample(fibre, scaled_uv(mask(pos, -1400, 120, r=True, b=True), (0, 0), 40.0, -1200, 120), -800, 120)  # XZ plane, faces along Y
side = node(u.MaterialExpressionLinearInterpolate, -500, 60); link(fib_y, 'RGB', side, 'A'); link(fib_x, 'RGB', side, 'B')
link(mask(absn, -1200, 300, r=True), '', side, 'Alpha')
# Rings where |n.z| > 0.8, fibres below 0.4, blended between.
nz = mask(absn, -1200, 420, b=True)
sub = node(u.MaterialExpressionSubtract, -1000, 420, const_b=0.4); link(nz, '', sub, 'A')
div = node(u.MaterialExpressionDivide, -850, 420, const_b=0.4); link(sub, '', div, 'A')
sat = node(u.MaterialExpressionSaturate, -700, 420); link(div, '', sat, '')
color = node(u.MaterialExpressionLinearInterpolate, -300, 0); link(side, '', color, 'A'); link(rings, 'RGB', color, 'B'); link(sat, '', color, 'Alpha')
MEL.connect_material_property(color, '', u.MaterialProperty.MP_BASE_COLOR)
MEL.connect_material_property(node(u.MaterialExpressionConstant, -300, 250, r=0.75), '', u.MaterialProperty.MP_ROUGHNESS)
MEL.recompile_material(wood)
u.EditorAssetLibrary.save_loaded_asset(wood)

# r4 fix 3: the upper structure's bark UVs are an atlas (not affine in angle: Build-SmoothLowerTrunk.py build.json uv_fit),
# so bark colour and roughness are object-space triplanar on both meshes and the 1 m join shows no texture break.
# The tangent-space normal map keeps the mesh UVs (detail only).
bark = u.load_asset(f'{OAK}/Materials/M_MightyOak_Bark')
MEL.delete_all_material_expressions(bark)
target = bark
pos, absn = object_space()
TILE = 136.0   # cm per bark tile: the source texel density (0.735 UV/m)


def triplanar(tex, y, channel):
    xy = sample(tex, scaled_uv(mask(pos, -1400, y, r=True, g=True), (0, 0), TILE, -1200, y), -800, y)
    yz = sample(tex, scaled_uv(mask(pos, -1400, y + 120, g=True, b=True), (0, 0), TILE, -1200, y + 120), -800, y + 120)
    xz = sample(tex, scaled_uv(mask(pos, -1400, y + 240, r=True, b=True), (0, 0), TILE, -1200, y + 240), -800, y + 240)
    # Normalised pow-4 weights keep one projection dominant, so 45 deg faces do not wash out the fissures.
    w4 = node(u.MaterialExpressionPower, -1200, y + 360, const_exponent=4.0); link(absn, '', w4, 'Base')
    wsum = node(u.MaterialExpressionDotProduct, -1050, y + 420); link(w4, '', wsum, 'A')
    link(node(u.MaterialExpressionConstant3Vector, -1200, y + 480, constant=u.LinearColor(1, 1, 1, 1)), '', wsum, 'B')
    w = node(u.MaterialExpressionDivide, -900, y + 360); link(w4, '', w, 'A'); link(wsum, '', w, 'B')
    total = None
    for tex_node, comp, dy in ((yz, dict(r=True), 0), (xz, dict(g=True), 60), (xy, dict(b=True), 120)):
        m = node(u.MaterialExpressionMultiply, -500, y + dy); link(tex_node, channel, m, 'A'); link(mask(w, -700, y + dy, **comp), '', m, 'B')
        if total is None:
            total = m
        else:
            s = node(u.MaterialExpressionAdd, -300, y + dy); link(total, '', s, 'A'); link(m, '', s, 'B'); total = s
    return total


MEL.connect_material_property(triplanar(u.load_asset(f'{OAK}/Textures/T_MightyOak_Bark_BaseColor'), -400, 'RGB'), '', u.MaterialProperty.MP_BASE_COLOR)
MEL.connect_material_property(triplanar(u.load_asset(f'{OAK}/Textures/T_MightyOak_Bark_Roughness'), 300, 'R'), '', u.MaterialProperty.MP_ROUGHNESS)
normal = node(u.MaterialExpressionTextureSample, -800, 900, texture=u.load_asset(f'{OAK}/Textures/T_MightyOak_Bark_Normal'),
              sampler_type=u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
MEL.connect_material_property(normal, 'RGB', u.MaterialProperty.MP_NORMAL)
MEL.recompile_material(bark)
u.EditorAssetLibrary.save_loaded_asset(bark)

b = lower.get_bounding_box()
print('FELLING_ART_UPDATED lower_tris', lower.get_num_triangles(0), 'bounds_cm', b.min, b.max)
